#include "hand_broker.h"
#include "haptic_schedule.h"
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <thread>
#define CHECK(x)                                                                                                       \
    do                                                                                                                 \
    {                                                                                                                  \
        if (!(x))                                                                                                      \
        {                                                                                                              \
            std::fprintf(stderr, "FAIL %d %s\n", __LINE__, #x);                                                        \
            std::abort();                                                                                              \
        }                                                                                                              \
    } while (0)
using namespace HiggsHands;
struct Fixture
{
    Broker broker;
    HapticSchedule haptics{broker};
    uint64_t owner = 0, other = 0, executor = 0;
    int notifications = 0;
    HIGGS_ActionRequest notification{};
    uint64_t now = Now();
    std::array<HIGGS_HandRecord, 2> hands{};
    Fixture()
    {
        HIGGS_HandClientInfo c{};
        c.structSize = sizeof(c);
        std::snprintf(c.name, sizeof(c.name), "primary");
        c.userPriority = 10;
        CHECK(!broker.Register(c, owner));
        std::snprintf(c.name, sizeof(c.name), "other");
        c.userPriority = 1;
        CHECK(!broker.Register(c, other));
        CHECK(!broker.Executor(owner, Notify, this, executor));
        broker.ObserveWorld(1, false, now);
        for (auto &h : hands)
            h.observationMask =
                HIGGS_OBS_HELD_KNOWN | HIGGS_OBS_EQUIPMENT_KNOWN | HIGGS_OBS_PALM_VALID | HIGGS_OBS_TRACKING_VALID;
        broker.Publish(hands, ++now);
        broker.MarkMainThread();
    }
    static void HIGGS_HAND_CALL Notify(const HIGGS_ActionRequest *r, void *context)
    {
        auto &f = *static_cast<Fixture *>(context);
        ++f.notifications;
        f.notification = *r;
    }
    HIGGS_HandUseState Use(uint32_t duration = 1000)
    {
        HIGGS_HandUseRequest r{};
        r.structSize = sizeof(r);
        r.kind = HIGGS_USE_HAND_GOALS;
        r.physicalHands = HIGGS_HAND_BOTH;
        r.durationMs = duration;
        r.epoch = broker.Epoch();
        r.executor = executor;
        r.contextId = 900;
        r.contextGeneration = 2;
        HIGGS_HandUseState s{};
        CHECK(!broker.HandUse(owner, r, s, now));
        return s;
    }
    HIGGS_HapticRequest Pulse(uint64_t context = 0, float strength = .3f)
    {
        HIGGS_HapticRequest r{};
        r.structSize = sizeof(r);
        r.physicalHand = HIGGS_HAND_LEFT;
        r.epoch = broker.Epoch();
        r.contextKey = context;
        r.durationMs = 100;
        r.startStrength = r.endStrength = strength;
        return r;
    }
};
void SessionOwnership()
{
    auto f = std::make_unique<Fixture>();
    auto s = f->Use();
    CHECK(!f->notifications);
    f->broker.DrainHandUses();
    CHECK(f->notifications == 1 && f->notification.inputId == 0 && f->notification.physicalHands == HIGGS_HAND_BOTH);
    f->broker.DrainHandUses();
    CHECK(f->notifications == 1);
    HIGGS_HandSnapshot snapshot{};
    CHECK(!f->broker.Copy(f->owner, snapshot, f->now));
    CHECK(snapshot.hands[0].grant == snapshot.hands[1].grant && snapshot.hands[0].owner == f->owner);
    HIGGS_HandUseState renewed{};
    CHECK(f->broker.ReadHandUse(f->other, s.authority.requestId, s.epoch, 1000, renewed, f->now) ==
          HIGGS_HAND_WRONG_OWNER);
    CHECK(!f->broker.ReadHandUse(f->owner, s.authority.requestId, s.epoch, 1000, renewed, f->now + 100));
    CHECK(!f->broker.EndHandUse(f->owner, s.authority.requestId, s.epoch));
    CHECK(f->broker.ReadHandUse(f->owner, s.authority.requestId, s.epoch, 1000, renewed, f->now + 101) ==
          HIGGS_HAND_STALE);
    std::puts("PASS external multi-hand use without fake input, owned renewal and end");
}
struct Mutation
{
    Fixture *f;
    int value = 0;
};
HIGGS_HandStatus HIGGS_HAND_CALL Commit(const HIGGS_ActionRequest *r, const HIGGS_HandSnapshot *snapshot, void *context)
{
    auto &m = *static_cast<Mutation *>(context);
    CHECK(snapshot->epoch == r->epoch);
    CHECK(m.f->broker.Release(m.f->owner, r->grant) == HIGGS_HAND_PENDING);
    CHECK(m.f->broker.Remove(m.f->owner, r->executor) == HIGGS_HAND_PENDING);
    CHECK(m.f->broker.Unregister(m.f->owner) == HIGGS_HAND_PENDING);
    HIGGS_HandSnapshot copy{};
    CHECK(!m.f->broker.Copy(m.f->owner, copy, Now()));
    m.value = 1;
    return HIGGS_HAND_OK;
}
void MutationFence()
{
    auto f = std::make_unique<Fixture>();
    auto s = f->Use();
    HIGGS_GrantedMutationRequest r{};
    r.structSize = sizeof(r);
    r.commitStage = HIGGS_ACTION_COMPLETED;
    r.requestId = s.authority.requestId;
    r.executor = s.authority.executor;
    r.epoch = s.epoch;
    r.grant = s.authority.grant;
    HIGGS_ActionResult result{};
    Mutation m{f.get()};
    std::thread wrong(
        [&] { CHECK(f->broker.WithAuthorizedAction(f->owner, r, Commit, &m, result) == HIGGS_HAND_WRONG_PHASE); });
    wrong.join();
    CHECK(!m.value);
    CHECK(!f->broker.WithAuthorizedAction(f->owner, r, Commit, &m, result));
    CHECK(m.value == 1 && result.stage == HIGGS_ACTION_COMPLETED && !result.receiptFlags);
    CHECK(!f->broker.Release(f->owner, r.grant));
    HIGGS_ActionResult after{};
    CHECK(!f->broker.Result(f->owner, r.requestId, after));
    CHECK(after.stage == HIGGS_ACTION_COMPLETED);
    CHECK(f->broker.WithAuthorizedAction(f->owner, r, Commit, &m, result) == HIGGS_HAND_WRONG_PHASE);
    std::puts("PASS main mutation fence, reentrant revocation refusal and committed result before fence exit");
}
HIGGS_HandStatus HIGGS_HAND_CALL SlowCommit(const HIGGS_ActionRequest *, const HIGGS_HandSnapshot *, void *context)
{
    auto &m = *static_cast<Mutation *>(context);
    std::this_thread::sleep_for(std::chrono::milliseconds(35));
    m.value = 2;
    return HIGGS_HAND_OK;
}
HIGGS_HandStatus HIGGS_HAND_CALL LifecycleCommit(const HIGGS_ActionRequest *request, const HIGGS_HandSnapshot *,
                                                 void *context)
{
    auto &m = *static_cast<Mutation *>(context);
    m.f->broker.ObserveWorld(0, true, Now());
    CHECK(m.f->broker.CopyPublished().epoch == request->epoch);
    CHECK(!m.f->broker.MutationAdmissionReady());
    m.value = 3;
    return HIGGS_HAND_OK;
}
void FenceLifecycle()
{
    auto f = std::make_unique<Fixture>();
    auto use = f->Use();
    HIGGS_GrantedMutationRequest r{};
    r.structSize = sizeof(r);
    r.commitStage = HIGGS_ACTION_COMPLETED;
    r.requestId = use.authority.requestId;
    r.executor = use.authority.executor;
    r.epoch = use.epoch;
    r.grant = use.authority.grant;
    Mutation mutation{f.get()};
    HIGGS_ActionResult result{};
    CHECK(!f->broker.WithAuthorizedAction(f->owner, r, LifecycleCommit, &mutation, result));
    CHECK(mutation.value == 3 && result.stage == HIGGS_ACTION_COMPLETED && result.epoch == use.epoch);
    CHECK(f->broker.CopyPublished().epoch > use.epoch && !f->broker.CopyPublished().worldReady);
    CHECK(!f->broker.Result(f->owner, r.requestId, result));
    CHECK(result.stage == HIGGS_ACTION_COMPLETED);
    std::puts("PASS reentrant lifecycle waits for entered mutation and committed receipt");
}
void FenceExpiry()
{
    auto f = std::make_unique<Fixture>();
    auto s = f->Use(25);
    HIGGS_GrantedMutationRequest r{};
    r.structSize = sizeof(r);
    r.commitStage = HIGGS_ACTION_COMPLETED;
    r.requestId = s.authority.requestId;
    r.executor = s.authority.executor;
    r.epoch = s.epoch;
    r.grant = s.authority.grant;
    Mutation m{f.get()};
    HIGGS_ActionResult result{};
    CHECK(!f->broker.WithAuthorizedAction(f->owner, r, SlowCommit, &m, result));
    CHECK(m.value == 2 && result.stage == HIGGS_ACTION_COMPLETED);
    HIGGS_ActionResult actual{};
    CHECK(!f->broker.Result(f->owner, r.requestId, actual));
    CHECK(actual.stage == HIGGS_ACTION_COMPLETED);
    std::puts("PASS authority cannot expire inside an entered mutation callback");
}
void HapticOwnership()
{
    auto f = std::make_unique<Fixture>();
    auto r = f->Pulse();
    uint64_t low = 0, high = 0;
    CHECK(!f->haptics.Request(f->other, r, low, f->now));
    r.startStrength = r.endStrength = .2f;
    CHECK(!f->haptics.Request(f->owner, r, high, f->now));
    auto sample = f->haptics.Sample(HIGGS_HAND_LEFT, f->now + 1);
    CHECK(sample.strength == .2f);
    f->haptics.Submitted(sample, true);
    f->haptics.Submitted(sample, true);
    HIGGS_HapticState state{};
    CHECK(!f->haptics.Copy(f->owner, high, state, f->now + 1));
    CHECK(state.submittedSamples == 1);
    CHECK(!f->haptics.Copy(f->other, low, state, f->now + 1));
    CHECK(state.suppressedSamples == 1);
    CHECK(f->haptics.Cancel(f->other, high) == HIGGS_HAND_WRONG_OWNER);
    CHECK(!f->haptics.Cancel(f->owner, high));
    sample = f->haptics.Sample(HIGGS_HAND_LEFT, f->now + 2);
    CHECK(sample.strength == .3f);
    f->broker.ConfigurePriorities("primary=0,other=0,HIGGS=0");
    r = f->Pulse(1, .7f);
    CHECK(!f->haptics.Request(f->owner, r, high, f->now));
    f->haptics.Legacy(HIGGS_HAND_LEFT, .9f, .9f, .1f, f->now);
    sample = f->haptics.Sample(HIGGS_HAND_LEFT, f->now + 3);
    CHECK(sample.strength == .9f);
    r = f->Pulse(1, .4f);
    uint64_t replacement = 0;
    CHECK(!f->haptics.Request(f->owner, r, replacement, f->now));
    CHECK(!f->haptics.Copy(f->owner, high, state, f->now));
    CHECK(state.stage == HIGGS_HAPTIC_CANCELLED);
    CHECK(!f->broker.Unregister(f->owner));
    sample = f->haptics.Sample(HIGGS_HAND_LEFT, f->now + 4);
    CHECK(sample.strength == .9f);
    f->broker.ObserveWorld(0, false, f->now + 5);
    sample = f->haptics.Sample(HIGGS_HAND_LEFT, f->now + 6);
    CHECK(sample.strength == 0);
    std::puts(
        "PASS coordinated priority/commutative haptics, owned context replacement, duplicate submission and lifecycle");
}
int main()
{
    SessionOwnership();
    MutationFence();
    FenceExpiry();
    FenceLifecycle();
    HapticOwnership();
}
