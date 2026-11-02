#pragma once
#include "higgshandapi.h"
#include "higgshandoperations.h"
#include <array>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

namespace HiggsHands
{
using Clock = uint64_t;
// Actual production broker; no engine or SKSE dependency. All native mutation is
// routed through DrainPhysical at the native HIGGS main-update phase.
class Broker
{
  public:
    HIGGS_HandStatus Register(const HIGGS_HandClientInfo &, HIGGS_HandClient &);
    HIGGS_HandStatus Unregister(HIGGS_HandClient);
    HIGGS_HandStatus Copy(HIGGS_HandClient, HIGGS_HandSnapshot &, Clock);
    HIGGS_HandStatus Recognizer(HIGGS_HandClient, HIGGS_RecognizerCallback, void *, uint64_t &);
    HIGGS_HandStatus Executor(HIGGS_HandClient, HIGGS_ExecutorCallback, void *, uint64_t &);
    HIGGS_HandStatus Events(HIGGS_HandClient, HIGGS_HandEventCallback, void *, uint64_t &);
    HIGGS_HandStatus Remove(HIGGS_HandClient, uint64_t);
    HIGGS_HandStatus Submit(HIGGS_HandClient, const HIGGS_ActionCandidate &, uint64_t &, Clock);
    HIGGS_HandStatus Report(HIGGS_HandClient, const HIGGS_ActionResult &);
    HIGGS_HandStatus Result(HIGGS_HandClient, uint64_t, HIGGS_ActionResult &);
    HIGGS_HandStatus Decision(HIGGS_HandClient, uint64_t, HIGGS_InputDecision &);
    HIGGS_HandStatus Release(HIGGS_HandClient, uint64_t);
    HIGGS_HandStatus Physical(HIGGS_HandClient, const HIGGS_PhysicalRequest &, uint64_t &, Clock);
    void ObserveWorld(uintptr_t identity, bool loading, Clock);
    void Publish(const std::array<HIGGS_HandRecord, 2> &, Clock);
    // Returns true when this raw state belongs to a consumed press (including up).
    bool Input(uint32_t hand, uint32_t button, bool down, Clock);
    using PhysicalDispatch = bool (*)(const HIGGS_PhysicalRequest &, void *);
    void DrainPhysical(PhysicalDispatch, void *, Clock);
    void SemanticEvent(uint32_t type, uint32_t hand, uint32_t ref, Clock);
    bool HasGrant(uint32_t hand, Clock);
    bool MayAcquire(uint32_t hand, Clock);
    bool MaySupport(uint32_t hand, Clock);
    bool ConfigurePriorities(const std::string &settings);
    bool PhysicalPending(uint32_t hand, Clock);
    uint64_t Epoch() const;
    uint64_t HandGeneration(uint32_t hand) const;
    std::unique_lock<std::recursive_mutex> NativePhase();
    void BeginHandMutation(Clock);
    void EndHandMutation(uint32_t hand, uint32_t heldRef, bool heldKnown, bool secondary, Clock);
    HIGGS_HandStatus Authorize(uint64_t owner, uint64_t executor, uint64_t recognizer, uint32_t hands);
    HIGGS_HandStatus Revoke(uint64_t owner, uint64_t executor, uint64_t recognizer);
    HIGGS_HandStatus CanRoute(uint64_t recognizer, uint64_t executor, uint32_t hands);
    HIGGS_HandStatus CancelRecognized(uint64_t recognizer, uint64_t request, uint64_t epoch, uint32_t reason);
    HIGGS_HandStatus Operation(uint64_t, const HIGGS_HandOperation &, uint64_t &, Clock);
    HIGGS_HandStatus EndOperation(uint64_t, uint64_t, uint64_t);
    HIGGS_HandStatus CopyOperation(uint64_t, uint64_t, HIGGS_HandOperationState &, Clock);
    using OperationDispatch = bool (*)(uint64_t, HIGGS_HandOperationState &, void *);
    void DrainOperations(OperationDispatch, void *, Clock);
    bool OperationActive(uint64_t, Clock);
    bool CopyActiveOperation(uint32_t hand, uint32_t kind, HIGGS_HandOperationState &, Clock);
    void ConfirmOperation(uint64_t, Clock);
    void BreakOperation(uint64_t, uint32_t);

  private:
    struct Client
    {
        uint64_t id = 0;
        HIGGS_HandClientInfo info{};
        bool active = true;
    };
    struct Registration
    {
        uint64_t id = 0, owner = 0;
        uint32_t kind = 0, inFlight = 0;
        bool active = true;
        HIGGS_RecognizerCallback recognizer = nullptr;
        HIGGS_ExecutorCallback executor = nullptr;
        HIGGS_HandEventCallback events = nullptr;
        void *context = nullptr;
        std::vector<std::pair<uint64_t, uint32_t>> routes;
    };
    struct Action
    {
        HIGGS_ActionRequest request{};
        HIGGS_ActionResult result{};
        HIGGS_ActionCandidate candidate{};
        HIGGS_PhysicalRequest physical{};
        Clock expires = 0;
        bool grantActive = false, selected = false, dispatched = false;
        uint64_t recognizerOwner = 0;
        HIGGS_HandOperationState operation{};
        bool operationActive = false;
        bool nativeStarted = false;
    };
    HIGGS_HandStatus Add(HIGGS_HandClient, uint32_t, HIGGS_RecognizerCallback, HIGGS_ExecutorCallback,
                         HIGGS_HandEventCallback, void *, uint64_t &);
    Client *ClientLocked(uint64_t);
    std::shared_ptr<Registration> RegistrationLocked(uint64_t);
    Action *ActionLocked(uint64_t);
    Action *NewActionLocked();
    void ExpireLocked(Clock);
    bool EligibleLocked(uint32_t hands) const;
    bool RoutedLocked(uint64_t executor, uint64_t recognizer, uint32_t hands);
    int32_t PriorityLocked(const Client &, bool &overridden) const;
    void RefreshLocked();
    void FinishLocked(Action &, uint32_t stage, uint32_t reason);
    void QueueResultLocked(const Action &);
    void QueueEventLocked(uint32_t, uint32_t, uint32_t, Clock, const HIGGS_HandRecord *);
    void FlushEvents();
    bool Enter(const std::shared_ptr<Registration> &);
    void Leave(const std::shared_ptr<Registration> &);
    std::vector<std::shared_ptr<Registration>> List(uint32_t);
    mutable std::mutex mutex_;
    std::condition_variable drained_;
    std::recursive_mutex dispatch_;
    std::vector<Client> clients_;
    std::vector<std::pair<std::string, int32_t>> priorities_;
    std::vector<std::shared_ptr<Registration>> registrations_;
    std::array<Action, 256> actions_{};
    std::array<HIGGS_InputDecision, 256> decisions_{};
    std::vector<HIGGS_HandEvent> pendingEvents_;
    HIGGS_HandSnapshot snapshot_{};
    uintptr_t world_ = 0;
    bool loading_ = false;
    bool down_[2][2]{}, consumed_[2][2]{};
    uint64_t nextToken_ = 1, nextInput_ = 1, nextObservation_ = 1, nextEvent_ = 1;
};
Broker &Service();
Clock Now();
void ObserveWorld(uintptr_t, bool loading = false);
void BeginLoad();
void EndLoad();
void NewGame();
bool Input(uint32_t hand, uint32_t button, bool down);
bool HasGrant(bool isLeft);
bool MayAcquire(bool isLeft);
bool MaySupport(bool isLeft);
// Native adapter publishes after PostVrikUpdate and before public post callbacks.
void PublishNative();
void DrainNative();
void FinishNative();
bool CopyNativeTarget(bool isLeft, float position[3], float rotation[9], float &scale);
bool HasNativeTarget(bool isLeft);
void ConfirmNativeTarget(bool isLeft);
void ApplyDriveCap(bool isLeft, float &linear, float &angular);
bool CopyHandGeometry(bool isLeft, float halfExtents[3]);
void ConfirmNativeGrip(bool isLeft);
} // namespace HiggsHands
