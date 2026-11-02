#include "hand_broker.h"
#include <cstring>
namespace HiggsHands
{
HIGGS_HandStatus Broker::HandUse(uint64_t owner, const HIGGS_HandUseRequest &r, HIGGS_HandUseState &out, uint64_t now)
{
    auto phase = NativePhase();
    std::lock_guard lock(mutex_);
    ExpireLocked(now);
    if (!ClientLocked(owner))
        return HIGGS_HAND_INVALID_CLIENT;
    if (r.structSize < sizeof(r) || r.kind < HIGGS_USE_HAND_GOALS || r.kind > HIGGS_USE_CONSUMPTION ||
        !r.physicalHands || (r.physicalHands & ~3u) || !r.durationMs || r.durationMs > 60000)
        return HIGGS_HAND_INVALID_ARGUMENT;
    if (r.epoch != snapshot_.epoch)
        return HIGGS_HAND_STALE;
    auto executor = RegistrationLocked(r.executor);
    if (!executor || !executor->active || executor->owner != owner || executor->kind != 2)
        return HIGGS_HAND_WRONG_OWNER;
    if (!EligibleLocked(r.physicalHands))
        return HIGGS_HAND_CONFLICT;
    auto action = NewActionLocked();
    if (!action)
        return HIGGS_HAND_CAPACITY;
    action->request.structSize = sizeof(action->request);
    action->request.requestId = nextToken_++;
    action->request.owner = owner;
    action->request.executor = r.executor;
    action->request.physicalHands = r.physicalHands;
    action->request.epoch = r.epoch;
    action->request.grant = nextToken_++;
    action->request.actionId = r.kind;
    action->request.contextId = r.contextId;
    action->request.contextGeneration = r.contextGeneration;
    action->selected = action->grantActive = true;
    action->sessionKind = r.kind;
    action->expires = now + static_cast<uint64_t>(r.durationMs) * 1000000;
    FinishLocked(*action, HIGGS_ACTION_ACCEPTED, 0);
    RefreshLocked();
    out = {};
    out.structSize = sizeof(out);
    out.kind = r.kind;
    out.active = 1;
    out.epoch = r.epoch;
    out.expiresAtNs = action->expires;
    out.authority = action->request;
    return HIGGS_HAND_OK;
}
HIGGS_HandStatus Broker::ReadHandUse(uint64_t owner, uint64_t id, uint64_t epoch, uint32_t renew,
                                     HIGGS_HandUseState &out, uint64_t now)
{
    auto phase = NativePhase();
    std::lock_guard lock(mutex_);
    ExpireLocked(now);
    if (!ClientLocked(owner))
        return HIGGS_HAND_INVALID_CLIENT;
    auto a = ActionLocked(id);
    if (!a || !a->sessionKind)
        return HIGGS_HAND_NOT_FOUND;
    if (a->request.owner != owner)
        return HIGGS_HAND_WRONG_OWNER;
    if (epoch && epoch != snapshot_.epoch)
        return HIGGS_HAND_STALE;
    if (renew)
    {
        if (renew > 60000)
            return HIGGS_HAND_INVALID_ARGUMENT;
        if (a->request.epoch != snapshot_.epoch || !a->grantActive)
            return HIGGS_HAND_STALE;
        auto executor = RegistrationLocked(a->request.executor);
        if (!executor || !executor->active)
            return HIGGS_HAND_NOT_FOUND;
        for (const auto &h : snapshot_.hands)
            if (h.physicalHand & a->request.physicalHands)
                if (h.grant != a->request.grant || h.owner != owner || !(h.observationMask & HIGGS_OBS_TRACKING_VALID))
                    return HIGGS_HAND_STALE;
        a->expires = now + static_cast<uint64_t>(renew) * 1000000;
    }
    out = {};
    out.structSize = sizeof(out);
    out.kind = a->sessionKind;
    out.active = a->grantActive ? 1u : 0u;
    out.epoch = a->request.epoch;
    out.expiresAtNs = a->expires;
    out.authority = a->request;
    return HIGGS_HAND_OK;
}
HIGGS_HandStatus Broker::EndHandUse(uint64_t owner, uint64_t id, uint64_t epoch)
{
    auto phase = NativePhase();
    uint64_t grant = 0;
    {
        std::lock_guard lock(mutex_);
        if (!ClientLocked(owner))
            return HIGGS_HAND_INVALID_CLIENT;
        auto a = ActionLocked(id);
        if (!a || !a->sessionKind)
            return HIGGS_HAND_NOT_FOUND;
        if (a->request.owner != owner)
            return HIGGS_HAND_WRONG_OWNER;
        if (epoch != a->request.epoch || epoch != snapshot_.epoch)
            return HIGGS_HAND_STALE;
        grant = a->request.grant;
    }
    return Release(owner, grant);
}
void Broker::DrainHandUses()
{
    auto phase = NativePhase();
    std::array<HIGGS_ActionRequest, 256> work{};
    size_t count = 0;
    {
        std::lock_guard lock(mutex_);
        ExpireLocked(Now());
        for (auto &a : actions_)
            if (a.sessionKind && a.grantActive && !a.sessionNotified)
            {
                a.sessionNotified = true;
                work[count++] = a.request;
            }
    }
    for (size_t i = 0; i < count; ++i)
    {
        std::shared_ptr<Registration> executor;
        {
            std::lock_guard lock(mutex_);
            auto a = ActionLocked(work[i].requestId);
            if (!a || !a->grantActive || a->result.stage >= HIGGS_ACTION_COMPLETED)
                continue;
            executor = RegistrationLocked(a->request.executor);
        }
        InvokeSessionExecutor(executor, work[i]);
    }
}
} // namespace HiggsHands
namespace
{
template <class F> HIGGS_HandStatus Guard(F f) noexcept
{
    try
    {
        return f();
    }
    catch (...)
    {
        return HIGGS_HAND_INTERNAL_ERROR;
    }
}
HIGGS_HandStatus Output(HIGGS_HandUseState *out)
{
    if (!out)
        return HIGGS_HAND_INVALID_ARGUMENT;
    if (out->structSize < sizeof(*out))
    {
        out->structSize = sizeof(*out);
        return HIGGS_HAND_BUFFER_TOO_SMALL;
    }
    return HIGGS_HAND_OK;
}
HIGGS_HandStatus HIGGS_HAND_CALL Request(uint64_t owner, const HIGGS_HandUseRequest *r, HIGGS_HandUseState *out)
{
    if (!r || r->structSize < sizeof(*r))
        return HIGGS_HAND_INVALID_ARGUMENT;
    auto s = Output(out);
    if (s)
        return s;
    return Guard([&] {
        HIGGS_HandUseState value{};
        auto status = HiggsHands::Service().HandUse(owner, *r, value, HiggsHands::Now());
        if (!status)
            std::memcpy(out, &value, sizeof(value));
        return status;
    });
}
HIGGS_HandStatus HIGGS_HAND_CALL Renew(uint64_t owner, uint64_t id, uint64_t epoch, uint32_t duration,
                                       HIGGS_HandUseState *out)
{
    auto s = Output(out);
    if (s)
        return s;
    if (!duration)
        return HIGGS_HAND_INVALID_ARGUMENT;
    return Guard([&] {
        HIGGS_HandUseState value{};
        auto status = HiggsHands::Service().ReadHandUse(owner, id, epoch, duration, value, HiggsHands::Now());
        if (!status)
            std::memcpy(out, &value, sizeof(value));
        return status;
    });
}
HIGGS_HandStatus HIGGS_HAND_CALL Copy(uint64_t owner, uint64_t id, HIGGS_HandUseState *out)
{
    auto s = Output(out);
    if (s)
        return s;
    return Guard([&] {
        HIGGS_HandUseState value{};
        auto status = HiggsHands::Service().ReadHandUse(owner, id, 0, 0, value, HiggsHands::Now());
        if (!status)
            std::memcpy(out, &value, sizeof(value));
        return status;
    });
}
HIGGS_HandStatus HIGGS_HAND_CALL End(uint64_t owner, uint64_t id, uint64_t epoch)
{
    return Guard([&] { return HiggsHands::Service().EndHandUse(owner, id, epoch); });
}
} // namespace
extern "C" HIGGS_HandStatus HIGGS_HAND_CALL HIGGS_QueryHandUseInterface(uint32_t version, uint32_t bytes,
                                                                        HIGGS_HandUseInterface *out)
{
    if (version != 1)
        return HIGGS_HAND_UNSUPPORTED_VERSION;
    if (!out)
        return HIGGS_HAND_INVALID_ARGUMENT;
    if (bytes < sizeof(*out))
        return HIGGS_HAND_BUFFER_TOO_SMALL;
    const HIGGS_HandUseInterface table{sizeof(table), 1, 1, Request, Renew, Copy, End};
    std::memcpy(out, &table, sizeof(table));
    return HIGGS_HAND_OK;
}
