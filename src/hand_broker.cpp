#include "hand_broker.h"
#include <algorithm>
#include <cctype>
#include <charconv>
#include <chrono>
#include <cmath>
#include <cstring>
#include <limits>

namespace HiggsHands
{
namespace
{
thread_local Broker *callbackBroker = nullptr;
thread_local uint64_t callbackOwner = 0, recognitionInput = 0;
thread_local uint32_t recognitionHand = 0;
thread_local uint64_t recognitionGeneration = 0;
thread_local uint32_t callbackDepth = 0;
thread_local Broker *flushingBroker = nullptr;
struct CallbackScope
{
    Broker *oldBroker = callbackBroker;
    uint64_t oldOwner = callbackOwner, oldInput = recognitionInput;
    CallbackScope(Broker *b, uint64_t owner, uint64_t input = 0)
    {
        callbackBroker = b;
        callbackOwner = owner;
        recognitionInput = input;
        ++callbackDepth;
    }
    ~CallbackScope()
    {
        --callbackDepth;
        callbackBroker = oldBroker;
        callbackOwner = oldOwner;
        recognitionInput = oldInput;
    }
};
bool Terminal(uint32_t stage)
{
    return stage >= HIGGS_ACTION_COMPLETED;
}
bool Hand(uint32_t hand)
{
    return hand == HIGGS_HAND_LEFT || hand == HIGGS_HAND_RIGHT;
}
std::string Name(std::string value)
{
    const auto first = value.find_first_not_of(" \t\r\n");
    if (first == std::string::npos)
        return {};
    value = value.substr(first, value.find_last_not_of(" \t\r\n") - first + 1);
    for (auto &ch : value)
        ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
    return value;
}
} // namespace
Clock Now()
{
    return static_cast<Clock>(
        std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now().time_since_epoch())
            .count());
}
Broker &Service()
{
    static Broker service;
    return service;
}
bool Broker::ConfigurePriorities(const std::string &settings)
{
    std::vector<std::pair<std::string, int32_t>> parsed;
    size_t start = 0;
    while (start < settings.size())
    {
        const auto end = settings.find(',', start);
        const auto entry = settings.substr(start, end == std::string::npos ? end : end - start);
        const auto equals = entry.find('=');
        if (equals == std::string::npos)
            return false;
        const auto name = Name(entry.substr(0, equals)), number = Name(entry.substr(equals + 1));
        if (name.empty() || name.size() > 63 || parsed.size() >= 64)
            return false;
        int32_t priority = 0;
        auto result = std::from_chars(number.data(), number.data() + number.size(), priority);
        if (result.ec != std::errc{} || result.ptr != number.data() + number.size())
            return false;
        for (const auto &prior : parsed)
            if (prior.first == name)
                return false;
        parsed.emplace_back(name, priority);
        if (end == std::string::npos)
            break;
        start = end + 1;
        if (start == settings.size())
            return false;
    }
    std::lock_guard lock(mutex_);
    priorities_ = std::move(parsed);
    return true;
}
int32_t Broker::PriorityLocked(const Client &client, bool &overridden) const
{
    const auto name = Name(client.info.name);
    for (const auto &priority : priorities_)
        if (priority.first == name)
        {
            overridden = true;
            return priority.second;
        }
    overridden = false;
    return client.info.userPriority;
}
Broker::Client *Broker::ClientLocked(uint64_t id)
{
    for (auto &c : clients_)
        if (c.id == id && c.active)
            return &c;
    return nullptr;
}
std::shared_ptr<Broker::Registration> Broker::RegistrationLocked(uint64_t id)
{
    for (auto &r : registrations_)
        if (r->id == id)
            return r;
    return {};
}
Broker::Action *Broker::ActionLocked(uint64_t id)
{
    for (auto &a : actions_)
        if (id && a.request.requestId == id)
            return &a;
    return nullptr;
}
Broker::Action *Broker::NewActionLocked()
{
    for (auto &a : actions_)
        if (!a.request.requestId)
        {
            a = {};
            return &a;
        }
    Action *oldest = nullptr;
    for (auto &a : actions_)
        if (!a.grantActive && !a.operationActive && !a.nativeStarted && Terminal(a.result.stage) &&
            (!oldest || a.request.requestId < oldest->request.requestId))
            oldest = &a;
    if (oldest)
        *oldest = {};
    return oldest;
}
HIGGS_HandStatus Broker::Register(const HIGGS_HandClientInfo &info, uint64_t &out)
{
    if (info.structSize < sizeof(info) || !info.name[0] || !std::memchr(info.name, 0, sizeof(info.name)))
        return HIGGS_HAND_INVALID_ARGUMENT;
    std::lock_guard lock(mutex_);
    size_t active = 0;
    for (auto &c : clients_)
        if (c.active)
            ++active;
    if (active >= 64)
        return HIGGS_HAND_CAPACITY;
    Client c{};
    c.id = nextToken_++;
    c.info = info;
    auto it = std::find_if(clients_.begin(), clients_.end(), [](const Client &v) { return !v.active; });
    if (it == clients_.end())
        clients_.push_back(c);
    else
        *it = c;
    out = c.id;
    return HIGGS_HAND_OK;
}
HIGGS_HandStatus Broker::Unregister(uint64_t owner)
{
    std::lock_guard serial(dispatch_);
    std::unique_lock lock(mutex_);
    for (const auto &a : actions_)
        if (a.request.owner == owner && a.nativeStarted)
            return HIGGS_HAND_PENDING;
    auto c = ClientLocked(owner);
    bool tombstone = false;
    for (auto &r : registrations_)
        if (r->owner == owner)
            tombstone = true;
    if (!c && !tombstone)
        return HIGGS_HAND_INVALID_CLIENT;
    if (c)
        c->active = false;
    for (auto &r : registrations_)
        if (r->owner == owner)
            r->active = false;
    for (auto &a : actions_)
        if (a.request.owner == owner)
        {
            a.grantActive = false;
            a.operationActive = false;
            if (!Terminal(a.result.stage))
                FinishLocked(a, HIGGS_ACTION_CANCELLED, HIGGS_HAND_INVALID_CLIENT);
        }
    RefreshLocked();
    if (callbackDepth)
        return HIGGS_HAND_PENDING;
    drained_.wait(lock, [&] {
        for (auto &r : registrations_)
            if (r->owner == owner && r->inFlight)
                return false;
        return true;
    });
    registrations_.erase(
        std::remove_if(registrations_.begin(), registrations_.end(), [&](const auto &r) { return r->owner == owner; }),
        registrations_.end());
    return HIGGS_HAND_OK;
}
HIGGS_HandStatus Broker::Add(uint64_t owner, uint32_t kind, HIGGS_RecognizerCallback rec, HIGGS_ExecutorCallback exec,
                             HIGGS_HandEventCallback ev, void *ctx, uint64_t &out)
{
    std::lock_guard lock(mutex_);
    if (!ClientLocked(owner))
        return HIGGS_HAND_INVALID_CLIENT;
    if (registrations_.size() >= 256)
        return HIGGS_HAND_CAPACITY;
    auto r = std::make_shared<Registration>();
    r->id = nextToken_++;
    r->owner = owner;
    r->kind = kind;
    r->recognizer = rec;
    r->executor = exec;
    r->events = ev;
    r->context = ctx;
    registrations_.push_back(r);
    out = r->id;
    return HIGGS_HAND_OK;
}
HIGGS_HandStatus Broker::Recognizer(uint64_t o, HIGGS_RecognizerCallback cb, void *c, uint64_t &out)
{
    return cb ? Add(o, 1, cb, nullptr, nullptr, c, out) : HIGGS_HAND_INVALID_ARGUMENT;
}
HIGGS_HandStatus Broker::Executor(uint64_t o, HIGGS_ExecutorCallback cb, void *c, uint64_t &out)
{
    return cb ? Add(o, 2, nullptr, cb, nullptr, c, out) : HIGGS_HAND_INVALID_ARGUMENT;
}
HIGGS_HandStatus Broker::Events(uint64_t o, HIGGS_HandEventCallback cb, void *c, uint64_t &out)
{
    return cb ? Add(o, 3, nullptr, nullptr, cb, c, out) : HIGGS_HAND_INVALID_ARGUMENT;
}
HIGGS_HandStatus Broker::Remove(uint64_t owner, uint64_t token)
{
    std::lock_guard serial(dispatch_);
    std::unique_lock lock(mutex_);
    auto r = RegistrationLocked(token);
    if (!r)
        return HIGGS_HAND_NOT_FOUND;
    if (r->owner != owner)
        return HIGGS_HAND_WRONG_OWNER;
    r->active = false;
    for (auto &a : actions_)
        if (a.request.executor == token && !Terminal(a.result.stage))
        {
            a.grantActive = false;
            FinishLocked(a, HIGGS_ACTION_CANCELLED, HIGGS_HAND_NOT_FOUND);
        }
    RefreshLocked();
    if (callbackDepth)
        return HIGGS_HAND_PENDING;
    drained_.wait(lock, [&] { return r->inFlight == 0; });
    registrations_.erase(std::remove(registrations_.begin(), registrations_.end(), r), registrations_.end());
    return HIGGS_HAND_OK;
}
std::vector<std::shared_ptr<Broker::Registration>> Broker::List(uint32_t kind)
{
    std::lock_guard lock(mutex_);
    std::vector<std::shared_ptr<Registration>> result;
    for (auto &r : registrations_)
        if (r->active && r->kind == kind)
            result.push_back(r);
    return result;
}
bool Broker::Enter(const std::shared_ptr<Registration> &r)
{
    std::lock_guard lock(mutex_);
    if (!r->active || !ClientLocked(r->owner))
        return false;
    ++r->inFlight;
    return true;
}
void Broker::Leave(const std::shared_ptr<Registration> &r)
{
    std::lock_guard lock(mutex_);
    --r->inFlight;
    drained_.notify_all();
}
void Broker::QueueResultLocked(const Action &a)
{
    HIGGS_HandEvent e{};
    e.structSize = sizeof(e);
    e.type = HIGGS_EVENT_ACTION_RESULT;
    e.eventId = nextEvent_++;
    e.epoch = a.request.epoch;
    e.observationId = snapshot_.observationId;
    e.result = a.result;
    pendingEvents_.push_back(e);
}
void Broker::FinishLocked(Action &a, uint32_t stage, uint32_t reason)
{
    a.result.structSize = sizeof(a.result);
    a.result.requestId = a.request.requestId;
    a.result.executor = a.request.executor;
    a.result.epoch = a.request.epoch;
    a.result.inputId = a.request.inputId;
    a.result.stage = stage;
    a.result.reason = reason;
    QueueResultLocked(a);
}
void Broker::QueueEventLocked(uint32_t type, uint32_t hand, uint32_t ref, Clock now, const HIGGS_HandRecord *state)
{
    HIGGS_HandEvent e{};
    e.structSize = sizeof(e);
    e.type = type;
    e.physicalHand = hand;
    e.itemRefId = ref;
    e.eventId = nextEvent_++;
    e.epoch = snapshot_.epoch;
    e.observationId = snapshot_.observationId;
    e.observedAtNs = now;
    if (state)
    {
        e.hand = *state;
        e.poseObservationId = state->poseObservationId;
    }
    pendingEvents_.push_back(e);
}
void Broker::FlushEvents()
{
    if (flushingBroker == this)
        return;
    std::lock_guard dispatch(dispatch_);
    struct Restore
    {
        Broker *previous;
        ~Restore()
        {
            flushingBroker = previous;
        }
    } restore{flushingBroker};
    flushingBroker = this;
    // Nested receipts append to the next batch. Registrations created by callbacks
    // never join the event currently in progress.
    for (size_t rounds = 0; rounds < 256; ++rounds)
    {
        std::vector<HIGGS_HandEvent> events;
        {
            std::lock_guard lock(mutex_);
            events.swap(pendingEvents_);
        }
        if (events.empty())
            break;
        for (auto &e : events)
        {
            auto list = List(3);
            for (auto &r : list)
                if (Enter(r))
                {
                    CallbackScope scope(this, r->owner);
                    try
                    {
                        r->events(&e, r->context);
                    }
                    catch (...)
                    {
                    }
                    Leave(r);
                }
        }
    }
}
void Broker::ExpireLocked(Clock now)
{
    for (auto &a : actions_)
        if (a.request.requestId && a.expires && now >= a.expires && !a.nativeStarted)
        {
            a.grantActive = false;
            a.operationActive = false;
            if (!Terminal(a.result.stage))
                FinishLocked(a, HIGGS_ACTION_CANCELLED, HIGGS_HAND_STALE);
        }
    RefreshLocked();
}
void Broker::RefreshLocked()
{
    for (auto &h : snapshot_.hands)
    {
        h.grant = 0;
        h.owner = 0;
        const bool known =
            (h.observationMask &
             (HIGGS_OBS_HELD_KNOWN | HIGGS_OBS_EQUIPMENT_KNOWN | HIGGS_OBS_PALM_VALID | HIGGS_OBS_TRACKING_VALID)) ==
            (HIGGS_OBS_HELD_KNOWN | HIGGS_OBS_EQUIPMENT_KNOWN | HIGGS_OBS_PALM_VALID | HIGGS_OBS_TRACKING_VALID);
        h.availability = !snapshot_.worldReady || !known
                             ? HIGGS_AVAILABILITY_UNKNOWN
                             : (h.unavailableReasons ? HIGGS_AVAILABILITY_UNAVAILABLE : HIGGS_AVAILABILITY_READY);
        if (h.availability == HIGGS_AVAILABILITY_READY &&
            (h.heldRefId || (h.observationMask & HIGGS_OBS_SECONDARY_SUPPORT)))
            h.availability = HIGGS_AVAILABILITY_RESERVED;
        for (auto &a : actions_)
            if (a.grantActive && (a.request.physicalHands & h.physicalHand))
            {
                h.grant = a.request.grant;
                h.owner = a.request.owner;
                if (h.availability == HIGGS_AVAILABILITY_READY || h.availability == HIGGS_AVAILABILITY_RESERVED)
                    h.availability = HIGGS_AVAILABILITY_RESERVED;
                break;
            }
    }
}
bool Broker::EligibleLocked(uint32_t hands) const
{
    if (!snapshot_.worldReady || !hands || (hands & ~3u))
        return false;
    for (const auto &h : snapshot_.hands)
        if (h.physicalHand & hands)
        {
            if (h.observationMask & HIGGS_OBS_SECONDARY_SUPPORT)
                return false;
            if (h.availability == HIGGS_AVAILABILITY_UNKNOWN || h.availability == HIGGS_AVAILABILITY_UNAVAILABLE ||
                h.grant)
                return false;
        }
    return true;
}
bool Broker::RoutedLocked(uint64_t executor, uint64_t recognizer, uint32_t hands)
{
    auto r = RegistrationLocked(executor);
    if (!r || !r->active || !ClientLocked(r->owner) || !ClientLocked(recognizer))
        return false;
    if (r->owner == recognizer)
        return true;
    for (const auto &route : r->routes)
        if (route.first == recognizer && (hands & ~route.second) == 0)
            return true;
    return false;
}
HIGGS_HandStatus Broker::Authorize(uint64_t owner, uint64_t executor, uint64_t recognizer, uint32_t hands)
{
    std::lock_guard lock(mutex_);
    if (!ClientLocked(owner) || !ClientLocked(recognizer))
        return HIGGS_HAND_INVALID_CLIENT;
    auto r = RegistrationLocked(executor);
    if (!r || !r->active || r->kind != 2)
        return HIGGS_HAND_NOT_FOUND;
    if (r->owner != owner)
        return HIGGS_HAND_WRONG_OWNER;
    if (!hands || (hands & ~3u))
        return HIGGS_HAND_INVALID_ARGUMENT;
    for (auto &route : r->routes)
        if (route.first == recognizer)
        {
            route.second = hands;
            return HIGGS_HAND_OK;
        }
    if (r->routes.size() >= 64)
        return HIGGS_HAND_CAPACITY;
    r->routes.emplace_back(recognizer, hands);
    return HIGGS_HAND_OK;
}
HIGGS_HandStatus Broker::Revoke(uint64_t owner, uint64_t executor, uint64_t recognizer)
{
    std::lock_guard serial(dispatch_);
    std::lock_guard lock(mutex_);
    if (!ClientLocked(owner))
        return HIGGS_HAND_INVALID_CLIENT;
    auto r = RegistrationLocked(executor);
    if (!r)
        return HIGGS_HAND_NOT_FOUND;
    if (r->owner != owner)
        return HIGGS_HAND_WRONG_OWNER;
    r->routes.erase(std::remove_if(r->routes.begin(), r->routes.end(),
                                   [&](const auto &route) { return route.first == recognizer; }),
                    r->routes.end());
    for (auto &a : actions_)
        if (a.request.executor == executor && a.recognizerOwner == recognizer &&
            a.result.stage < HIGGS_ACTION_COMMITTED)
        {
            a.grantActive = false;
            FinishLocked(a, HIGGS_ACTION_CANCELLED, HIGGS_HAND_WRONG_OWNER);
        }
    RefreshLocked();
    return HIGGS_HAND_OK;
}
HIGGS_HandStatus Broker::CanRoute(uint64_t recognizer, uint64_t executor, uint32_t hands)
{
    std::lock_guard lock(mutex_);
    if (!ClientLocked(recognizer))
        return HIGGS_HAND_INVALID_CLIENT;
    if (!hands || (hands & ~3u))
        return HIGGS_HAND_INVALID_ARGUMENT;
    return RoutedLocked(executor, recognizer, hands) ? HIGGS_HAND_OK : HIGGS_HAND_WRONG_OWNER;
}
HIGGS_HandStatus Broker::CancelRecognized(uint64_t recognizer, uint64_t request, uint64_t epoch, uint32_t reason)
{
    std::lock_guard serial(dispatch_);
    {
        std::lock_guard lock(mutex_);
        if (!ClientLocked(recognizer))
            return HIGGS_HAND_INVALID_CLIENT;
        auto a = ActionLocked(request);
        if (!a)
            return HIGGS_HAND_NOT_FOUND;
        if (a->recognizerOwner != recognizer)
            return HIGGS_HAND_WRONG_OWNER;
        if (a->request.epoch != epoch || epoch != snapshot_.epoch)
            return HIGGS_HAND_STALE;
        if (a->result.stage >= HIGGS_ACTION_COMMITTED)
            return HIGGS_HAND_CONFLICT;
        a->grantActive = false;
        FinishLocked(*a, HIGGS_ACTION_CANCELLED, reason);
        RefreshLocked();
    }
    FlushEvents();
    return HIGGS_HAND_OK;
}
HIGGS_HandStatus Broker::Copy(uint64_t owner, HIGGS_HandSnapshot &out, Clock now)
{
    std::lock_guard lock(mutex_);
    if (!ClientLocked(owner))
        return HIGGS_HAND_INVALID_CLIENT;
    ExpireLocked(now);
    out = snapshot_;
    out.structSize = sizeof(out);
    return HIGGS_HAND_OK;
}
HIGGS_HandStatus Broker::Submit(uint64_t owner, const HIGGS_ActionCandidate &c, uint64_t &out, Clock now)
{
    std::lock_guard lock(mutex_);
    if (!ClientLocked(owner))
        return HIGGS_HAND_INVALID_CLIENT;
    if (callbackBroker != this || callbackOwner != owner || !recognitionInput || recognitionInput != c.inputId)
        return HIGGS_HAND_WRONG_PHASE;
    if (c.structSize < sizeof(c) || c.reserved || !c.grantDurationMs || c.grantDurationMs > 60000 || !c.physicalHands ||
        (c.physicalHands & ~3u))
        return HIGGS_HAND_INVALID_ARGUMENT;
    auto exec = RegistrationLocked(c.executor);
    if (!exec || !exec->active || exec->kind != 2)
        return HIGGS_HAND_NOT_FOUND;
    if (!RoutedLocked(c.executor, owner, c.physicalHands))
        return HIGGS_HAND_WRONG_OWNER;
    if (c.epoch != snapshot_.epoch)
        return HIGGS_HAND_STALE;
    const auto &decision = decisions_[c.inputId % decisions_.size()];
    if (decision.inputId != c.inputId)
        return HIGGS_HAND_STALE;
    // A consumed Down owns its matching Up even if the executor already ended
    // the hand grant. Up observers may inspect; they cannot consume it again.
    if (decision.consumed)
        return HIGGS_HAND_CONFLICT;
    // The generation belongs to the triggering physical hand; both-hand candidates
    // still validate both resources against the current coherent snapshot below.
    if (c.handGeneration != recognitionGeneration || !(c.physicalHands & recognitionHand))
        return HIGGS_HAND_STALE;
    ExpireLocked(now);
    if (!EligibleLocked(c.physicalHands))
        return HIGGS_HAND_CONFLICT;
    auto a = NewActionLocked();
    if (!a)
        return HIGGS_HAND_CAPACITY;
    a->candidate = c;
    a->recognizerOwner = owner;
    a->request = {};
    a->request.structSize = sizeof(a->request);
    a->request.requestId = nextToken_++;
    a->request.owner = exec->owner;
    a->request.executor = c.executor;
    a->request.epoch = c.epoch;
    a->request.inputId = c.inputId;
    a->request.actionId = c.actionId;
    a->request.contextId = c.contextId;
    a->request.contextGeneration = c.contextGeneration;
    a->request.physicalHands = c.physicalHands;
    a->expires = now + static_cast<Clock>(c.grantDurationMs) * 1000000;
    out = a->request.requestId;
    return HIGGS_HAND_OK;
}
bool Broker::Input(uint32_t hand, uint32_t button, bool down, Clock now)
{
    if (!Hand(hand) || (button != HIGGS_INPUT_GRIP && button != HIGGS_INPUT_TRIGGER))
        return false;
    std::lock_guard dispatch(dispatch_);
    const size_t hi = hand == HIGGS_HAND_LEFT ? 0 : 1, bi = button - 1;
    HIGGS_HandInput input{};
    bool wasConsumed = false;
    {
        std::lock_guard lock(mutex_);
        ExpireLocked(now);
        if (down_[hi][bi] == down)
        {
            if (down && snapshot_.hands[hi].grant)
                consumed_[hi][bi] = true;
            return consumed_[hi][bi] || snapshot_.hands[hi].grant != 0;
        }
        down_[hi][bi] = down;
        wasConsumed = consumed_[hi][bi] || (down && snapshot_.hands[hi].grant != 0);
        input.structSize = sizeof(input);
        input.physicalHand = hand;
        input.button = button;
        input.edge = down ? HIGGS_INPUT_DOWN : HIGGS_INPUT_UP;
        input.inputId = nextInput_++;
        input.epoch = snapshot_.epoch;
        input.handGeneration = snapshot_.hands[hi].generation;
        input.poseObservationId = snapshot_.hands[hi].poseObservationId;
        input.observedAtNs = now;
        input.snapshot = snapshot_;
        auto &d = decisions_[input.inputId % decisions_.size()];
        d = {};
        d.structSize = sizeof(d);
        d.inputId = input.inputId;
        d.epoch = input.epoch;
        d.consumed = wasConsumed ? 1u : 0u;
    }
    auto list = List(1);
    for (auto &r : list)
        if (Enter(r))
        {
            CallbackScope scope(this, r->owner, input.inputId);
            const auto oldHand = recognitionHand;
            const auto oldGeneration = recognitionGeneration;
            recognitionHand = hand;
            recognitionGeneration = input.handGeneration;
            try
            {
                r->recognizer(&input, r->context);
            }
            catch (...)
            {
            }
            recognitionHand = oldHand;
            recognitionGeneration = oldGeneration;
            Leave(r);
        }
    std::shared_ptr<Registration> executor;
    HIGGS_ActionRequest request{};
    bool consumed = wasConsumed;
    {
        std::lock_guard lock(mutex_);
        Action *winner = nullptr;
        int32_t priority = (std::numeric_limits<int32_t>::min)();
        bool winnerOverride = false;
        for (auto &a : actions_)
            if (a.request.inputId == input.inputId && !a.selected && !a.result.stage)
            {
                auto client = ClientLocked(a.request.owner);
                auto exec = RegistrationLocked(a.request.executor);
                bool overridden = false;
                const auto effective = client ? PriorityLocked(*client, overridden) : 0;
                if (client && exec && exec->active &&
                    RoutedLocked(a.request.executor, a.recognizerOwner, a.request.physicalHands) &&
                    input.epoch == snapshot_.epoch && snapshot_.hands[hi].generation == input.handGeneration &&
                    EligibleLocked(a.request.physicalHands) &&
                    (!winner || effective > priority ||
                     (effective == priority &&
                      (client->id < winner->request.owner ||
                       (client->id == winner->request.owner && a.request.requestId < winner->request.requestId)))))
                {
                    winner = &a;
                    priority = effective;
                    winnerOverride = overridden;
                }
            }
        for (auto &a : actions_)
            if (a.request.inputId == input.inputId && !a.selected && !a.result.stage && &a != winner)
                FinishLocked(a, HIGGS_ACTION_REJECTED, HIGGS_HAND_CONFLICT);
        if (winner)
        {
            winner->selected = true;
            winner->grantActive = true;
            winner->request.grant = nextToken_++;
            winner->request.poseObservationId = input.poseObservationId;
            FinishLocked(*winner, HIGGS_ACTION_ACCEPTED, 0);
            request = winner->request;
            executor = RegistrationLocked(request.executor);
            consumed = true;
            auto &d = decisions_[input.inputId % decisions_.size()];
            d.consumed = 1;
            d.owner = request.owner;
            d.grant = request.grant;
            d.requestId = request.requestId;
            d.effectivePriority = priority;
            d.reason = (winnerOverride ? HIGGS_DECISION_USER_PRIORITY : HIGGS_DECISION_CLIENT_PRIORITY) |
                       HIGGS_DECISION_REGISTRATION_ORDER | HIGGS_DECISION_CANDIDATE_ORDER;
        }
        consumed_[hi][bi] = down && consumed;
        RefreshLocked();
    }
    if (executor && Enter(executor))
    {
        CallbackScope scope(this, executor->owner);
        try
        {
            executor->executor(&request, executor->context);
        }
        catch (...)
        {
            std::lock_guard lock(mutex_);
            if (auto a = ActionLocked(request.requestId))
            {
                a->grantActive = false;
                FinishLocked(*a, HIGGS_ACTION_REJECTED, HIGGS_HAND_INTERNAL_ERROR);
                RefreshLocked();
            }
        }
        Leave(executor);
    }
    FlushEvents();
    return consumed;
}
HIGGS_HandStatus Broker::Report(uint64_t owner, const HIGGS_ActionResult &result)
{
    std::lock_guard serial(dispatch_);
    {
        std::lock_guard lock(mutex_);
        ExpireLocked(Now());
        if (!ClientLocked(owner))
            return HIGGS_HAND_INVALID_CLIENT;
        auto a = ActionLocked(result.requestId);
        if (!a)
            return HIGGS_HAND_NOT_FOUND;
        if (a->request.owner != owner || a->request.executor != result.executor)
            return HIGGS_HAND_WRONG_OWNER;
        if (result.structSize < sizeof(result) || result.stage < HIGGS_ACTION_COMMITTED ||
            result.stage > HIGGS_ACTION_CANCELLED || (result.receiptFlags & ~7u))
            return HIGGS_HAND_INVALID_ARGUMENT;
        if (a->request.epoch != result.epoch || result.epoch != snapshot_.epoch || a->request.inputId != result.inputId)
            return HIGGS_HAND_STALE;
        if (!a->selected || !result.executor)
            return HIGGS_HAND_WRONG_PHASE;
        if (std::memcmp(&a->result, &result, sizeof(result)) == 0)
            return HIGGS_HAND_OK;
        if (Terminal(a->result.stage))
            return HIGGS_HAND_CONFLICT;
        if (result.stage < a->result.stage || (a->result.receiptFlags & ~result.receiptFlags))
            return HIGGS_HAND_CONFLICT;
        if (a->result.stage >= HIGGS_ACTION_COMMITTED && result.stage == HIGGS_ACTION_REJECTED)
            return HIGGS_HAND_CONFLICT;
        a->result = result;
        QueueResultLocked(*a);
        if (result.stage == HIGGS_ACTION_REJECTED || result.stage == HIGGS_ACTION_CANCELLED)
            a->grantActive = false;
        RefreshLocked();
    }
    FlushEvents();
    return HIGGS_HAND_OK;
}
HIGGS_HandStatus Broker::Result(uint64_t owner, uint64_t id, HIGGS_ActionResult &out)
{
    std::lock_guard lock(mutex_);
    ExpireLocked(Now());
    if (!ClientLocked(owner))
        return HIGGS_HAND_INVALID_CLIENT;
    auto a = ActionLocked(id);
    if (!a)
        return HIGGS_HAND_NOT_FOUND;
    if (a->request.owner != owner && a->recognizerOwner != owner)
        return HIGGS_HAND_WRONG_OWNER;
    out = a->result;
    return HIGGS_HAND_OK;
}
HIGGS_HandStatus Broker::Decision(uint64_t owner, uint64_t id, HIGGS_InputDecision &out)
{
    std::lock_guard lock(mutex_);
    if (!ClientLocked(owner))
        return HIGGS_HAND_INVALID_CLIENT;
    const auto &d = decisions_[id % decisions_.size()];
    if (!id || d.inputId != id)
        return HIGGS_HAND_NOT_FOUND;
    out = d;
    return HIGGS_HAND_OK;
}
HIGGS_HandStatus Broker::Release(uint64_t owner, uint64_t grant)
{
    std::lock_guard serial(dispatch_);
    std::lock_guard lock(mutex_);
    if (!ClientLocked(owner))
        return HIGGS_HAND_INVALID_CLIENT;
    for (const auto &a : actions_)
        if (grant && a.request.grant == grant && a.request.owner == owner && a.nativeStarted)
            return HIGGS_HAND_PENDING; // Already entered the native mutator; retry after this phase.
    bool found = false;
    for (auto &a : actions_)
        if (grant && a.request.grant == grant)
        {
            if (a.request.owner != owner)
                return HIGGS_HAND_WRONG_OWNER;
            found = true;
            a.grantActive = false;
            a.operationActive = false;
            if (!Terminal(a.result.stage))
                FinishLocked(a, HIGGS_ACTION_CANCELLED, 0);
        }
    RefreshLocked();
    return found ? HIGGS_HAND_OK : HIGGS_HAND_NOT_FOUND;
}
void Broker::ObserveWorld(uintptr_t world, bool loading, Clock now)
{
    std::lock_guard dispatch(dispatch_);
    {
        std::lock_guard lock(mutex_);
        if (world_ == world && loading_ == loading)
            return;
        world_ = world;
        loading_ = loading;
        const auto epoch = snapshot_.epoch + 1, generation = snapshot_.generation + 1;
        snapshot_ = {};
        snapshot_.structSize = sizeof(snapshot_);
        snapshot_.epoch = epoch;
        snapshot_.generation = generation;
        snapshot_.worldReady = world && !loading;
        snapshot_.sampledAtNs = now;
        snapshot_.hands[0].physicalHand = HIGGS_HAND_LEFT;
        snapshot_.hands[1].physicalHand = HIGGS_HAND_RIGHT;
        for (auto &a : actions_)
            if (a.request.requestId)
            {
                a.grantActive = false;
                a.operationActive = false;
                if (!Terminal(a.result.stage))
                    FinishLocked(a, HIGGS_ACTION_CANCELLED, HIGGS_HAND_STALE);
            }
        for (auto &row : down_)
            for (auto &value : row)
                value = false;
        for (auto &row : consumed_)
            for (auto &value : row)
                value = false;
        QueueEventLocked(HIGGS_EVENT_RESET, 0, 0, now, nullptr);
        RefreshLocked();
    }
    FlushEvents();
}
uint64_t Broker::Epoch() const
{
    std::lock_guard lock(mutex_);
    return snapshot_.epoch;
}
uint64_t Broker::HandGeneration(uint32_t hand) const
{
    std::lock_guard lock(mutex_);
    return snapshot_.hands[hand == HIGGS_HAND_LEFT ? 0 : 1].generation;
}
std::unique_lock<std::recursive_mutex> Broker::NativePhase()
{
    return std::unique_lock<std::recursive_mutex>(dispatch_);
}
void Broker::BeginHandMutation(Clock now)
{
    std::lock_guard lock(mutex_);
    ExpireLocked(now);
    for (auto &a : actions_)
        if (a.physical.kind && a.dispatched && !Terminal(a.result.stage))
        {
            const auto &h = snapshot_.hands[a.physical.physicalHand == HIGGS_HAND_LEFT ? 0 : 1];
            if (h.grant == a.request.grant && h.owner == a.request.owner)
                a.nativeStarted = true;
        }
}
void Broker::EndHandMutation(uint32_t hand, uint32_t held, bool known, bool secondary, Clock now)
{
    std::lock_guard lock(mutex_);
    for (auto &a : actions_)
        if (a.physical.physicalHand == hand && a.nativeStarted)
        {
            a.nativeStarted = false;
            if (!Terminal(a.result.stage) &&
                ((a.physical.kind == HIGGS_PHYSICAL_GRAB && known && held == a.physical.itemRefId) ||
                 (a.physical.kind == HIGGS_PHYSICAL_RELEASE && known && !held && !secondary) ||
                 (a.physical.kind == HIGGS_PHYSICAL_SECONDARY_SUPPORT && secondary)))
            {
                a.result.receiptFlags |= HIGGS_RECEIPT_PHYSICAL_CONFIRMED;
                a.result.itemRefId = a.physical.itemRefId;
                a.result.quantity = 1;
                FinishLocked(a, HIGGS_ACTION_COMPLETED, 0);
                if (a.physical.kind == HIGGS_PHYSICAL_RELEASE)
                    a.grantActive = false;
            }
        }
    ExpireLocked(now);
}
void Broker::Publish(const std::array<HIGGS_HandRecord, 2> &records, Clock now)
{
    std::lock_guard dispatch(dispatch_);
    {
        std::lock_guard lock(mutex_);
        ExpireLocked(now);
        if (!snapshot_.worldReady)
            return;
        const auto old = snapshot_.hands[0];
        const auto oldRight = snapshot_.hands[1];
        const std::array<HIGGS_HandRecord, 2> previous{old, oldRight};
        snapshot_.structSize = sizeof(snapshot_);
        snapshot_.observationId = nextObservation_++;
        snapshot_.sampledAtNs = now;
        ++snapshot_.generation;
        for (size_t i = 0; i < 2; ++i)
        {
            auto h = records[i];
            h.physicalHand = i == 0 ? HIGGS_HAND_LEFT : HIGGS_HAND_RIGHT;
            const auto &p = previous[i];
            const bool changed = p.heldRefId != h.heldRefId || p.equippedFormId != h.equippedFormId ||
                                 p.observationMask != h.observationMask || p.unavailableReasons != h.unavailableReasons;
            h.generation = changed || !p.generation ? nextToken_++ : p.generation;
            h.occupancy = (h.heldRefId ? HIGGS_OCCUPANCY_HIGGS_HELD : 0u) |
                          (h.equippedFormId ? HIGGS_OCCUPANCY_GAME_EQUIPPED : 0u);
            if (h.observationMask & HIGGS_OBS_PALM_VALID)
            {
                h.poseObservationId = snapshot_.observationId;
                h.poseObservedAtNs = now;
            }
            snapshot_.hands[i] = h;
            if (p.heldRefId != h.heldRefId)
            {
                if (p.heldRefId)
                    QueueEventLocked(HIGGS_EVENT_RELEASED, h.physicalHand, p.heldRefId, now, &h);
                if (h.heldRefId)
                    QueueEventLocked(HIGGS_EVENT_GRABBED, h.physicalHand, h.heldRefId, now, &h);
            }
            if ((p.observationMask ^ h.observationMask) & HIGGS_OBS_SECONDARY_SUPPORT)
                QueueEventLocked((h.observationMask & HIGGS_OBS_SECONDARY_SUPPORT) ? HIGGS_EVENT_SECONDARY_STARTED
                                                                                   : HIGGS_EVENT_SECONDARY_ENDED,
                                 h.physicalHand, h.equippedFormId, now, &h);
            if (!(h.observationMask & HIGGS_OBS_TRACKING_VALID))
                for (auto &a : actions_)
                    if (a.grantActive && (a.request.physicalHands & h.physicalHand))
                    {
                        a.grantActive = false;
                        if (!Terminal(a.result.stage))
                            FinishLocked(a, HIGGS_ACTION_CANCELLED, HIGGS_HAND_NOT_READY);
                    }
        }
        for (auto &a : actions_)
            if (a.physical.kind && a.dispatched && !Terminal(a.result.stage))
            {
                const auto &h = snapshot_.hands[a.physical.physicalHand == HIGGS_HAND_LEFT ? 0 : 1];
                if ((a.physical.kind == HIGGS_PHYSICAL_GRAB && h.heldRefId == a.physical.itemRefId) ||
                    (a.physical.kind == HIGGS_PHYSICAL_SECONDARY_SUPPORT &&
                     (h.observationMask & HIGGS_OBS_SECONDARY_SUPPORT)) ||
                    (a.physical.kind == HIGGS_PHYSICAL_RELEASE && (h.observationMask & HIGGS_OBS_HELD_KNOWN) &&
                     !h.heldRefId && !(h.observationMask & HIGGS_OBS_SECONDARY_SUPPORT)))
                {
                    a.result.receiptFlags |= HIGGS_RECEIPT_PHYSICAL_CONFIRMED;
                    a.result.itemRefId = a.physical.itemRefId;
                    a.result.quantity = 1;
                    FinishLocked(a, HIGGS_ACTION_COMPLETED, 0);
                    if (a.physical.kind == HIGGS_PHYSICAL_RELEASE)
                        a.grantActive = false;
                }
            }
        RefreshLocked();
    }
    FlushEvents();
}
HIGGS_HandStatus Broker::Physical(uint64_t owner, const HIGGS_PhysicalRequest &p, uint64_t &out, Clock now)
{
    std::lock_guard lock(mutex_);
    if (!ClientLocked(owner))
        return HIGGS_HAND_INVALID_CLIENT;
    if (p.structSize < sizeof(p) || !Hand(p.physicalHand) || p.reserved || !p.timeoutMs || p.timeoutMs > 60000 ||
        (!p.itemRefId && p.kind == HIGGS_PHYSICAL_GRAB))
        return HIGGS_HAND_INVALID_ARGUMENT;
    if (p.kind != HIGGS_PHYSICAL_GRAB && p.kind != HIGGS_PHYSICAL_RELEASE && p.kind != HIGGS_PHYSICAL_SECONDARY_SUPPORT)
        return HIGGS_HAND_UNSUPPORTED;
    if (p.epoch != snapshot_.epoch)
        return HIGGS_HAND_STALE;
    ExpireLocked(now);
    const auto &h = snapshot_.hands[p.physicalHand == HIGGS_HAND_LEFT ? 0 : 1];
    if (h.generation != p.handGeneration)
        return HIGGS_HAND_STALE;
    if (h.availability == HIGGS_AVAILABILITY_UNKNOWN || h.availability == HIGGS_AVAILABILITY_UNAVAILABLE)
        return HIGGS_HAND_NOT_READY;
    if (h.grant && (h.grant != p.grant || h.owner != owner))
        return HIGGS_HAND_CONFLICT;
    if (p.grant && (!h.grant || h.owner != owner))
        return HIGGS_HAND_WRONG_OWNER;
    if ((p.kind == HIGGS_PHYSICAL_GRAB || p.kind == HIGGS_PHYSICAL_SECONDARY_SUPPORT) &&
        (h.heldRefId || (h.observationMask & HIGGS_OBS_SECONDARY_SUPPORT)))
        return HIGGS_HAND_CONFLICT;
    for (const auto &operation : actions_)
        if (operation.operationActive && operation.operation.operation.physicalHand == p.physicalHand &&
            (operation.operation.operation.kind == HIGGS_OP_WORLD_SUPPORT ||
             operation.operation.operation.kind == HIGGS_OP_PHYSICAL_HAND_TARGET ||
             operation.operation.operation.kind == HIGGS_OP_HAND_GEOMETRY))
            return HIGGS_HAND_CONFLICT;
    if (p.kind == HIGGS_PHYSICAL_RELEASE && (!h.heldRefId && !(h.observationMask & HIGGS_OBS_SECONDARY_SUPPORT)))
        return HIGGS_HAND_CONFLICT;
    if (p.kind == HIGGS_PHYSICAL_RELEASE && p.itemRefId && p.itemRefId != h.heldRefId)
        return HIGGS_HAND_STALE;
    auto a = NewActionLocked();
    if (!a)
        return HIGGS_HAND_CAPACITY;
    a->request.structSize = sizeof(a->request);
    a->request.requestId = nextToken_++;
    a->request.owner = owner;
    a->request.epoch = p.epoch;
    a->request.physicalHands = p.physicalHand;
    a->request.grant = p.grant ? p.grant : nextToken_++;
    a->physical = p;
    if (!a->physical.itemRefId)
        a->physical.itemRefId = h.heldRefId;
    a->selected = true;
    a->grantActive = !p.grant;
    a->expires = now + static_cast<Clock>(p.timeoutMs) * 1000000;
    FinishLocked(*a, HIGGS_ACTION_ACCEPTED, 0);
    out = a->request.requestId;
    RefreshLocked();
    return HIGGS_HAND_OK;
}
void Broker::DrainPhysical(PhysicalDispatch dispatch, void *context, Clock now)
{
    std::lock_guard serial(dispatch_);
    std::vector<std::pair<uint64_t, HIGGS_PhysicalRequest>> work;
    {
        std::lock_guard lock(mutex_);
        ExpireLocked(now);
        for (auto &a : actions_)
            if (a.physical.kind && !a.dispatched && !Terminal(a.result.stage))
            {
                const auto &h = snapshot_.hands[a.physical.physicalHand == HIGGS_HAND_LEFT ? 0 : 1];
                if (a.request.epoch != snapshot_.epoch || h.generation != a.physical.handGeneration ||
                    h.grant != a.request.grant || h.owner != a.request.owner ||
                    h.availability == HIGGS_AVAILABILITY_UNKNOWN || h.availability == HIGGS_AVAILABILITY_UNAVAILABLE)
                {
                    a.grantActive = false;
                    FinishLocked(a, HIGGS_ACTION_CANCELLED, HIGGS_HAND_STALE);
                }
                else
                {
                    a.dispatched = true;
                    work.emplace_back(a.request.requestId, a.physical);
                }
            }
    }
    for (auto &task : work)
    {
        {
            std::lock_guard lock(mutex_);
            auto a = ActionLocked(task.first);
            if (!a || Terminal(a->result.stage))
                continue;
            const auto &h = snapshot_.hands[a->physical.physicalHand == HIGGS_HAND_LEFT ? 0 : 1];
            if (a->request.epoch != snapshot_.epoch || h.generation != a->physical.handGeneration ||
                h.grant != a->request.grant || h.owner != a->request.owner)
            {
                a->grantActive = false;
                FinishLocked(*a, HIGGS_ACTION_CANCELLED, HIGGS_HAND_STALE);
                continue;
            }
            a->nativeStarted = true;
        }
        bool accepted = false;
        try
        {
            accepted = dispatch && dispatch(task.second, context);
        }
        catch (...)
        {
        }
        std::lock_guard lock(mutex_);
        if (auto a = ActionLocked(task.first))
        {
            a->nativeStarted = false;
            if (!Terminal(a->result.stage))
            {
                if (accepted)
                    FinishLocked(*a, HIGGS_ACTION_COMMITTED, 0);
                else
                {
                    a->grantActive = false;
                    FinishLocked(*a, HIGGS_ACTION_REJECTED, HIGGS_HAND_NOT_READY);
                }
            }
        }
    }
    FlushEvents();
}
void Broker::SemanticEvent(uint32_t type, uint32_t hand, uint32_t ref, Clock now)
{
    std::lock_guard dispatch(dispatch_);
    {
        std::lock_guard lock(mutex_);
        if (Hand(hand))
            QueueEventLocked(type, hand, ref, now, &snapshot_.hands[hand == HIGGS_HAND_LEFT ? 0 : 1]);
    }
    FlushEvents();
}
bool Broker::HasGrant(uint32_t hand, Clock now)
{
    std::lock_guard lock(mutex_);
    ExpireLocked(now);
    return snapshot_.hands[hand == HIGGS_HAND_LEFT ? 0 : 1].grant != 0;
}
bool Broker::MayAcquire(uint32_t hand, Clock now)
{
    std::lock_guard lock(mutex_);
    ExpireLocked(now);
    const auto grant = snapshot_.hands[hand == HIGGS_HAND_LEFT ? 0 : 1].grant;
    if (!grant)
        return true;
    for (const auto &a : actions_)
        if (a.request.grant == grant && a.physical.kind == HIGGS_PHYSICAL_GRAB && a.dispatched &&
            !Terminal(a.result.stage))
            return true;
    return false;
}
bool Broker::PhysicalPending(uint32_t hand, Clock now)
{
    std::lock_guard lock(mutex_);
    ExpireLocked(now);
    const auto &h = snapshot_.hands[hand == HIGGS_HAND_LEFT ? 0 : 1];
    for (const auto &a : actions_)
        if (a.physical.physicalHand == hand &&
            (a.physical.kind == HIGGS_PHYSICAL_GRAB || a.physical.kind == HIGGS_PHYSICAL_SECONDARY_SUPPORT) &&
            a.dispatched && !Terminal(a.result.stage) && h.grant == a.request.grant && h.owner == a.request.owner)
            return true;
    return false;
}
bool Broker::MaySupport(uint32_t hand, Clock now)
{
    std::lock_guard lock(mutex_);
    ExpireLocked(now);
    const auto grant = snapshot_.hands[hand == HIGGS_HAND_LEFT ? 0 : 1].grant;
    if (!grant)
        return true;
    for (const auto &a : actions_)
        if (a.request.grant == grant && a.physical.kind == HIGGS_PHYSICAL_SECONDARY_SUPPORT && a.dispatched &&
            !Terminal(a.result.stage))
            return true;
    return false;
}
HIGGS_HandStatus Broker::Operation(uint64_t owner, const HIGGS_HandOperation &p, uint64_t &out, Clock now)
{
    std::lock_guard lock(mutex_);
    if (!ClientLocked(owner))
        return HIGGS_HAND_INVALID_CLIENT;
    if (p.structSize < sizeof(p) || !Hand(p.physicalHand) || p.reserved || p.reserved2 || !p.durationMs ||
        p.durationMs > 60000)
        return HIGGS_HAND_INVALID_ARGUMENT;
    if (p.kind < HIGGS_OP_WORLD_SUPPORT || p.kind > HIGGS_OP_HAND_GEOMETRY)
        return HIGGS_HAND_UNSUPPORTED;
    for (float v : p.position)
        if (!std::isfinite(v))
            return HIGGS_HAND_INVALID_ARGUMENT;
    for (float v : p.rotation)
        if (!std::isfinite(v))
            return HIGGS_HAND_INVALID_ARGUMENT;
    if (p.kind == HIGGS_OP_WORLD_SUPPORT || p.kind == HIGGS_OP_PHYSICAL_HAND_TARGET)
    {
        if (!std::isfinite(p.maxDistance) || p.maxDistance < 1 || p.maxDistance > 200)
            return HIGGS_HAND_INVALID_ARGUMENT;
    }
    if (p.kind == HIGGS_OP_WORLD_SUPPORT && !p.itemRefId)
        return HIGGS_HAND_INVALID_ARGUMENT;
    if (p.kind == HIGGS_OP_PHYSICAL_HAND_TARGET || p.kind == HIGGS_OP_GRIP_TRANSFORM)
    {
        if (!std::isfinite(p.scale) || p.scale <= 0 || p.scale > 10)
            return HIGGS_HAND_INVALID_ARGUMENT;
        for (size_t row = 0; row < 3; ++row)
            for (size_t col = 0; col < 3; ++col)
            {
                float dot = 0;
                for (size_t j = 0; j < 3; ++j)
                    dot += p.rotation[row * 3 + j] * p.rotation[col * 3 + j];
                if (std::fabs(dot - (row == col ? 1.f : 0.f)) > 0.001f)
                    return HIGGS_HAND_INVALID_ARGUMENT;
            }
        const auto *r = p.rotation;
        const auto determinant = r[0] * (r[4] * r[8] - r[5] * r[7]) - r[1] * (r[3] * r[8] - r[5] * r[6]) +
                                 r[2] * (r[3] * r[7] - r[4] * r[6]);
        if (determinant < 0.999f)
            return HIGGS_HAND_INVALID_ARGUMENT;
    }
    if (p.kind == HIGGS_OP_DRIVE_CAP && (!std::isfinite(p.linearForceCap) || !std::isfinite(p.angularForceCap) ||
                                         p.linearForceCap < 0 || p.angularForceCap < 0))
        return HIGGS_HAND_INVALID_ARGUMENT;
    if (p.kind == HIGGS_OP_HAPTIC &&
        (!std::isfinite(p.hapticStrength) || p.hapticStrength < 0 || p.hapticStrength > 1 || p.durationMs > 1000))
        return HIGGS_HAND_INVALID_ARGUMENT;
    if (p.reserved3)
        return HIGGS_HAND_INVALID_ARGUMENT;
    if (p.kind == HIGGS_OP_HAND_GEOMETRY)
        for (float value : p.geometryHalfExtents)
            if (!std::isfinite(value) || value < 0.005f || value > 0.5f)
                return HIGGS_HAND_INVALID_ARGUMENT;
    ExpireLocked(now);
    if (p.epoch != snapshot_.epoch)
        return HIGGS_HAND_STALE;
    const auto &h = snapshot_.hands[p.physicalHand == HIGGS_HAND_LEFT ? 0 : 1];
    if (p.handGeneration != h.generation)
        return HIGGS_HAND_STALE;
    if (h.availability == HIGGS_AVAILABILITY_UNKNOWN || h.availability == HIGGS_AVAILABILITY_UNAVAILABLE)
        return HIGGS_HAND_NOT_READY;
    if (h.grant && (h.grant != p.grant || h.owner != owner))
        return HIGGS_HAND_CONFLICT;
    if (p.grant && (!h.grant || h.owner != owner))
        return HIGGS_HAND_WRONG_OWNER;
    if ((p.kind == HIGGS_OP_WORLD_SUPPORT || p.kind == HIGGS_OP_PHYSICAL_HAND_TARGET ||
         p.kind == HIGGS_OP_HAND_GEOMETRY) &&
        (h.heldRefId || h.equippedFormId || (h.observationMask & HIGGS_OBS_SECONDARY_SUPPORT)))
        return HIGGS_HAND_CONFLICT;
    for (const auto &pending : actions_)
        if (pending.physical.kind && pending.physical.physicalHand == p.physicalHand && !Terminal(pending.result.stage))
            return HIGGS_HAND_CONFLICT;
    if ((p.kind == HIGGS_OP_GRIP_TRANSFORM || p.kind == HIGGS_OP_DRIVE_CAP) &&
        (!h.heldRefId || h.heldRefId != p.itemRefId))
        return HIGGS_HAND_CONFLICT;
    for (const auto &a : actions_)
        if (a.operationActive && a.operation.operation.physicalHand == p.physicalHand)
        {
            const auto kind = a.operation.operation.kind;
            if (kind == p.kind || ((kind == HIGGS_OP_WORLD_SUPPORT || kind == HIGGS_OP_PHYSICAL_HAND_TARGET) &&
                                   (p.kind == HIGGS_OP_WORLD_SUPPORT || p.kind == HIGGS_OP_PHYSICAL_HAND_TARGET)))
                return HIGGS_HAND_CONFLICT;
        }
    auto a = NewActionLocked();
    if (!a)
        return HIGGS_HAND_CAPACITY;
    a->request.structSize = sizeof(a->request);
    a->request.requestId = nextToken_++;
    a->request.owner = owner;
    a->request.epoch = p.epoch;
    a->request.physicalHands = p.physicalHand;
    a->request.grant = p.grant ? p.grant : nextToken_++;
    a->selected = true;
    a->grantActive = !p.grant;
    a->operationActive = true;
    a->expires = now + static_cast<Clock>(p.durationMs) * 1000000;
    a->operation.structSize = sizeof(a->operation);
    a->operation.requestId = a->request.requestId;
    a->operation.owner = owner;
    a->operation.grant = a->request.grant;
    a->operation.epoch = p.epoch;
    a->operation.operation = p;
    FinishLocked(*a, HIGGS_ACTION_ACCEPTED, 0);
    RefreshLocked();
    out = a->request.requestId;
    return HIGGS_HAND_OK;
}
bool Broker::OperationActive(uint64_t id, Clock now)
{
    std::lock_guard lock(mutex_);
    ExpireLocked(now);
    auto a = ActionLocked(id);
    if (!a || !a->operationActive || a->request.epoch != snapshot_.epoch)
        return false;
    const auto &h = snapshot_.hands[a->operation.operation.physicalHand == HIGGS_HAND_LEFT ? 0 : 1];
    const bool active = h.grant == a->request.grant && h.owner == a->request.owner &&
                        h.generation == a->operation.operation.handGeneration &&
                        (h.observationMask & HIGGS_OBS_TRACKING_VALID);
    if (!active)
    {
        a->operationActive = false;
        a->grantActive = false;
        if (!Terminal(a->result.stage))
            FinishLocked(*a, HIGGS_ACTION_CANCELLED, HIGGS_HAND_STALE);
        RefreshLocked();
    }
    return active;
}
HIGGS_HandStatus Broker::EndOperation(uint64_t owner, uint64_t id, uint64_t epoch)
{
    std::lock_guard serial(dispatch_);
    std::lock_guard lock(mutex_);
    if (!ClientLocked(owner))
        return HIGGS_HAND_INVALID_CLIENT;
    auto a = ActionLocked(id);
    if (!a || !a->operation.operation.kind)
        return HIGGS_HAND_NOT_FOUND;
    if (a->request.owner != owner)
        return HIGGS_HAND_WRONG_OWNER;
    if (a->request.epoch != epoch || epoch != snapshot_.epoch)
        return HIGGS_HAND_STALE;
    if (a->nativeStarted)
        return HIGGS_HAND_PENDING;
    a->operationActive = false;
    a->grantActive = false;
    if (!Terminal(a->result.stage))
        FinishLocked(*a, HIGGS_ACTION_CANCELLED, 0);
    RefreshLocked();
    return HIGGS_HAND_OK;
}
HIGGS_HandStatus Broker::CopyOperation(uint64_t owner, uint64_t id, HIGGS_HandOperationState &out, Clock now)
{
    const bool active = OperationActive(id, now);
    std::lock_guard lock(mutex_);
    if (!ClientLocked(owner))
        return HIGGS_HAND_INVALID_CLIENT;
    auto a = ActionLocked(id);
    if (!a || !a->operation.operation.kind)
        return HIGGS_HAND_NOT_FOUND;
    if (a->request.owner != owner)
        return HIGGS_HAND_WRONG_OWNER;
    out = a->operation;
    out.flags = (active ? HIGGS_OP_ACTIVE : 0u) | ((a->operation.flags & HIGGS_OP_APPLIED) ? HIGGS_OP_APPLIED : 0u);
    return HIGGS_HAND_OK;
}
bool Broker::CopyActiveOperation(uint32_t hand, uint32_t kind, HIGGS_HandOperationState &out, Clock now)
{
    uint64_t id = 0;
    {
        std::lock_guard lock(mutex_);
        for (const auto &a : actions_)
            if (a.operationActive && a.dispatched &&
                (a.result.stage == HIGGS_ACTION_COMMITTED || a.result.stage == HIGGS_ACTION_COMPLETED) &&
                a.operation.operation.physicalHand == hand && a.operation.operation.kind == kind)
            {
                id = a.request.requestId;
                break;
            }
    }
    if (!id || !OperationActive(id, now))
        return false;
    std::lock_guard lock(mutex_);
    if (auto a = ActionLocked(id))
    {
        out = a->operation;
        return true;
    }
    return false;
}
void Broker::ConfirmOperation(uint64_t id, Clock now)
{
    std::lock_guard lock(mutex_);
    auto a = ActionLocked(id);
    if (!a || !a->operationActive || !a->dispatched || a->result.stage != HIGGS_ACTION_COMMITTED)
        return;
    a->operation.flags |= HIGGS_OP_APPLIED;
    a->operation.observedAtNs = now;
    if (a->operation.operation.kind != HIGGS_OP_HAPTIC)
        a->result.receiptFlags |= HIGGS_RECEIPT_PHYSICAL_CONFIRMED;
    FinishLocked(*a, HIGGS_ACTION_COMPLETED, 0);
}
void Broker::BreakOperation(uint64_t id, uint32_t reason)
{
    std::lock_guard lock(mutex_);
    if (auto a = ActionLocked(id))
    {
        a->operationActive = false;
        a->grantActive = false;
        if (!Terminal(a->result.stage))
            FinishLocked(*a, HIGGS_ACTION_CANCELLED, reason);
        RefreshLocked();
    }
}
void Broker::DrainOperations(OperationDispatch dispatch, void *context, Clock now)
{
    std::lock_guard serial(dispatch_);
    std::vector<HIGGS_HandOperationState> work;
    {
        std::lock_guard lock(mutex_);
        ExpireLocked(now);
        for (auto &a : actions_)
            if (a.operationActive && !a.dispatched)
            {
                a.dispatched = true;
                work.push_back(a.operation);
            }
    }
    for (auto state : work)
    {
        if (!OperationActive(state.requestId, now))
            continue;
        {
            std::lock_guard lock(mutex_);
            auto a = ActionLocked(state.requestId);
            if (!a || !a->operationActive)
                continue;
            a->nativeStarted = true;
        }
        bool accepted = false;
        try
        {
            accepted = dispatch && dispatch(state.requestId, state, context);
        }
        catch (...)
        {
        }
        std::lock_guard lock(mutex_);
        if (auto a = ActionLocked(state.requestId))
        {
            a->nativeStarted = false;
            if (a->operationActive)
            {
                if (accepted)
                {
                    a->operation = state;
                    FinishLocked(*a, HIGGS_ACTION_COMMITTED, 0);
                }
                else
                {
                    a->operationActive = false;
                    a->grantActive = false;
                    FinishLocked(*a, HIGGS_ACTION_REJECTED, HIGGS_HAND_NOT_READY);
                    RefreshLocked();
                }
            }
        }
    }
    FlushEvents();
}
namespace
{
bool loading = false;
uintptr_t observedWorld = 0;
std::mutex lifecycle;
} // namespace
void ObserveWorld(uintptr_t world, bool)
{
    std::lock_guard lock(lifecycle);
    observedWorld = world;
    Service().ObserveWorld(world, loading, Now());
}
void BeginLoad()
{
    std::lock_guard lock(lifecycle);
    loading = true;
    Service().ObserveWorld(0, true, Now());
}
void EndLoad()
{
    std::lock_guard lock(lifecycle);
    loading = false;
    observedWorld = 0;
    Service().ObserveWorld(0, false, Now());
}
void NewGame()
{
    BeginLoad();
    EndLoad();
}
bool Input(uint32_t hand, uint32_t button, bool down)
{
    return Service().Input(hand, button, down, Now());
}
bool HasGrant(bool left)
{
    return Service().HasGrant(left ? HIGGS_HAND_LEFT : HIGGS_HAND_RIGHT, Now());
}
bool MayAcquire(bool left)
{
    return Service().MayAcquire(left ? HIGGS_HAND_LEFT : HIGGS_HAND_RIGHT, Now());
}
bool MaySupport(bool left)
{
    return Service().MaySupport(left ? HIGGS_HAND_LEFT : HIGGS_HAND_RIGHT, Now());
}
} // namespace HiggsHands
