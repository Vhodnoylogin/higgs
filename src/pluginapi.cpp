#include <vector>

#include "pluginapi.h"
#include "version.h"
#include "papyrusapi.h"
#include "config.h"
#include "hand.h"

using namespace HiggsPluginAPI;

namespace {
    template <class Callback>
    void AppendCallback(std::atomic<std::shared_ptr<const std::vector<Callback>>> &callbacks, Callback callback) {
        auto next = std::make_shared<std::vector<Callback>>(*callbacks.load(std::memory_order_acquire));
        next->push_back(callback);
        callbacks.store(std::shared_ptr<const std::vector<Callback>>(std::move(next)), std::memory_order_release);
    }
}

// A message used to fetch HIGGS's interface
struct HiggsMessage {
    enum { kMessage_GetInterface = 0xF9279A57 }; // Randomly generated
    void * (*getApiFunction)(unsigned int revisionNumber) = nullptr;
};

// Interface classes are stored statically
HiggsInterface001 g_interface001;

// Constructs and returns an API of the revision number requested
void * GetApi(unsigned int revisionNumber) {
    switch (revisionNumber) {
    case 1:	_MESSAGE("Interface revision 1 requested"); return &g_interface001;
    }
    return nullptr;
}

// Handles skse mod messages requesting to fetch API functions from VRIK
void HiggsPluginAPI::ModMessageHandler(SKSEMessagingInterface::Message * message) {
    if (message->type == HiggsMessage::kMessage_GetInterface) {
        HiggsMessage * higgsMessage = (HiggsMessage*)message->data;
        higgsMessage->getApiFunction = GetApi;
        _MESSAGE("Provided HIGGS plugin interface to \"%s\"", message->sender);
    }
}

// HIGGS build numbers are made up as follows: V01.00.05.00
constexpr int higgsBuildNumber = FPVR_VERSION_MAJOR * 1000000 + FPVR_VERSION_MINOR * 10000 + FPVR_VERSION_PATCH * 100 + FPVR_VERSION_BETA;

// Fetches the HIGGS version number
unsigned int HiggsInterface001::GetBuildNumber() {
    return higgsBuildNumber;
}

bool HiggsInterface001::Deprecated1(const std::string_view &name, double &out) {
    return Config::GetSettingDouble(name, out);
}

bool HiggsInterface001::Deprecated2(const std::string &name, double val) {
    return Config::SetSettingDouble(name, val);
}

bool HiggsInterface001::GetSettingDouble(const char *name, double& out) {
    return Config::GetSettingDouble(name, out);
}

bool HiggsInterface001::SetSettingDouble(const char *name, double val) {
    return Config::SetSettingDouble(name, val);
}

void HiggsInterface001::AddPulledCallback(PulledCallback callback) {
    if (!callback) return;
    std::scoped_lock lock(addCallbackLock);
    AppendCallback(pulledCallbacks, callback);
}

void HiggsInterface001::AddGrabbedCallback(GrabbedCallback callback) {
    if (!callback) return;
    std::scoped_lock lock(addCallbackLock);
    AppendCallback(grabbedCallbacks, callback);
}

void HiggsInterface001::AddDroppedCallback(DroppedCallback callback) {
    if (!callback) return;
    std::scoped_lock lock(addCallbackLock);
    AppendCallback(droppedCallbacks, callback);
}

void HiggsInterface001::AddStashedCallback(StashedCallback callback) {
    if (!callback) return;
    std::scoped_lock lock(addCallbackLock);
    AppendCallback(stashedCallbacks, callback);
}

void HiggsInterface001::AddConsumedCallback(ConsumedCallback callback) {
    if (!callback) return;
    std::scoped_lock lock(addCallbackLock);
    AppendCallback(consumedCallbacks, callback);
}

void HiggsInterface001::AddCollisionCallback(CollisionCallback callback) {
    if (!callback) return;
    std::scoped_lock lock(addCallbackLock);
    AppendCallback(collisionCallbacks, callback);
}

void HiggsInterface001::AddStartTwoHandingCallback(StartTwoHandingCallback callback) {
    if (!callback) return;
    std::scoped_lock lock(addCallbackLock);
    AppendCallback(startTwoHandingCallbacks, callback);
}

void HiggsInterface001::AddStopTwoHandingCallback(StopTwoHandingCallback callback) {
    if (!callback) return;
    std::scoped_lock lock(addCallbackLock);
    AppendCallback(stopTwoHandingCallbacks, callback);
}

void HiggsInterface001::AddCollisionFilterComparisonCallback(CollisionFilterComparisonCallback callback) {
    if (!callback) return;
    std::scoped_lock lock(addCallbackLock);
    AppendCallback(collisionFilterComparisonCallbacks, callback);
}

void HiggsInterface001::AddPrePhysicsStepCallback(PrePhysicsStepCallback callback) {
    if (!callback) return;
    std::scoped_lock lock(addCallbackLock);
    AppendCallback(prePhysicsStepCallbacks, callback);
}

void HiggsInterface001::AddPreVrikPreHiggsCallback(NoArgCallback callback) {
    if (!callback) return;
    std::scoped_lock lock(addCallbackLock);
    AppendCallback(preVrikPreHiggsCallbacks, callback);
}

void HiggsInterface001::AddPreVrikPostHiggsCallback(NoArgCallback callback) {
    if (!callback) return;
    std::scoped_lock lock(addCallbackLock);
    AppendCallback(preVrikPostHiggsCallbacks, callback);
}

void HiggsInterface001::AddPostVrikPreHiggsCallback(NoArgCallback callback) {
    if (!callback) return;
    std::scoped_lock lock(addCallbackLock);
    AppendCallback(postVrikPreHiggsCallbacks, callback);
}

void HiggsInterface001::AddPostVrikPostHiggsCallback(NoArgCallback callback) {
    if (!callback) return;
    std::scoped_lock lock(addCallbackLock);
    AppendCallback(postVrikPostHiggsCallbacks, callback);
}


void HiggsPluginAPI::TriggerPulledCallbacks(bool isLeft, TESObjectREFR *pulledRefr) {
    auto callbacks = g_interface001.pulledCallbacks.load(std::memory_order_acquire);
    for (auto callback : *callbacks) {
        callback(isLeft, pulledRefr);
    }

    PapyrusAPI::OnPullEvent(pulledRefr, isLeft);
}

void HiggsPluginAPI::TriggerGrabbedCallbacks(bool isLeft, TESObjectREFR *grabbedRefr) {
    auto callbacks = g_interface001.grabbedCallbacks.load(std::memory_order_acquire);
    for (auto callback : *callbacks) {
        callback(isLeft, grabbedRefr);
    }

    PapyrusAPI::OnGrabEvent(grabbedRefr, isLeft);
}

void HiggsPluginAPI::TriggerDroppedCallbacks(bool isLeft, TESObjectREFR *droppedRefr) {
    auto callbacks = g_interface001.droppedCallbacks.load(std::memory_order_acquire);
    for (auto callback : *callbacks) {
        callback(isLeft, droppedRefr);
    }

    PapyrusAPI::OnDropEvent(droppedRefr, isLeft);
}

void HiggsPluginAPI::TriggerStashedCallbacks(bool isLeft, TESForm *stashedForm) {
    auto callbacks = g_interface001.stashedCallbacks.load(std::memory_order_acquire);
    for (auto callback : *callbacks) {
        callback(isLeft, stashedForm);
    }

    PapyrusAPI::OnStashEvent(stashedForm, isLeft);
}

void HiggsPluginAPI::TriggerConsumedCallbacks(bool isLeft, TESForm *consumedForm) {
    auto callbacks = g_interface001.consumedCallbacks.load(std::memory_order_acquire);
    for (auto callback : *callbacks) {
        callback(isLeft, consumedForm);
    }

    PapyrusAPI::OnConsumeEvent(consumedForm, isLeft);
}

void HiggsPluginAPI::TriggerCollisionCallbacks(bool isLeft, float mass, float separatingVelocity) {
    auto callbacks = g_interface001.collisionCallbacks.load(std::memory_order_acquire);
    for (auto callback : *callbacks) {
        callback(isLeft, mass, separatingVelocity);
    }
}

void HiggsPluginAPI::TriggerStartTwoHandingCallbacks() {
    auto callbacks = g_interface001.startTwoHandingCallbacks.load(std::memory_order_acquire);
    for (auto callback : *callbacks) {
        callback();
    }

    PapyrusAPI::OnStartTwoHandingEvent();
}

void HiggsPluginAPI::TriggerStopTwoHandingCallbacks() {
    auto callbacks = g_interface001.stopTwoHandingCallbacks.load(std::memory_order_acquire);
    for (auto callback : *callbacks) {
        callback();
    }

    PapyrusAPI::OnStopTwoHandingEvent();
}

CollisionFilterComparisonResult HiggsPluginAPI::TriggerCollisionFilterComparisonCallbacks(void *filter, UInt32 filterInfoA, UInt32 filterInfoB) {
    auto callbacks = g_interface001.collisionFilterComparisonCallbacks.load(std::memory_order_acquire);
    for (auto callback : *callbacks) {
        CollisionFilterComparisonResult result = callback(filter, filterInfoA, filterInfoB);
        if (result == CollisionFilterComparisonResult::Collide || result == CollisionFilterComparisonResult::Ignore) {
            return result;
        }
        // result is CollisionFilterComparisonResult::Continue, i.e. don't do anything
    }

    return CollisionFilterComparisonResult::Continue;
}

void HiggsPluginAPI::TriggerPrePhysicsStepCallbacks(void *world) {
    auto callbacks = g_interface001.prePhysicsStepCallbacks.load(std::memory_order_acquire);
    for (auto callback : *callbacks) {
        callback(world);
    }
}

void HiggsPluginAPI::TriggerPreVrikPreHiggsCallbacks() {
    auto callbacks = g_interface001.preVrikPreHiggsCallbacks.load(std::memory_order_acquire);
    for (auto callback : *callbacks) {
        callback();
    }
}

void HiggsPluginAPI::TriggerPreVrikPostHiggsCallbacks() {
    auto callbacks = g_interface001.preVrikPostHiggsCallbacks.load(std::memory_order_acquire);
    for (auto callback : *callbacks) {
        callback();
    }
}

void HiggsPluginAPI::TriggerPostVrikPreHiggsCallbacks() {
    auto callbacks = g_interface001.postVrikPreHiggsCallbacks.load(std::memory_order_acquire);
    for (auto callback : *callbacks) {
        callback();
    }
}

void HiggsPluginAPI::TriggerPostVrikPostHiggsCallbacks() {
    auto callbacks = g_interface001.postVrikPostHiggsCallbacks.load(std::memory_order_acquire);
    for (auto callback : *callbacks) {
        callback();
    }
}

void HiggsInterface001::GrabObject(TESObjectREFR *object, bool isLeft)
{
    Hand &hand = isLeft ? *g_leftHand : *g_rightHand;

    hand.externalGrabRequestedObject = object;
    hand.externalGrabRequested = true;
}

TESObjectREFR * HiggsInterface001::GetGrabbedObject(bool isLeft)
{
    Hand &hand = isLeft ? *g_leftHand : *g_rightHand;

    if (hand.HasHeldObject()) {
        UInt32 handle = hand.selectedObject.handle;
        // To be somewhat thread-safe, check again if we're in held after getting the handle
        if (hand.HasHeldObject()) {
            NiPointer<TESObjectREFR> grabbedObj;
            if (LookupREFRByHandle(handle, grabbedObj)) {
                return grabbedObj;
            }
        }
    }

    return nullptr;
}

BSFixedString HiggsInterface001::GetGrabbedNodeName(bool isLeft)
{
    Hand &hand = isLeft ? *g_leftHand : *g_rightHand;
    if (hand.HasHeldObject() && hand.selectedObject.grabbedNodeName) {
        return hand.selectedObject.grabbedNodeName;
    }
    return "";
}

bool HiggsInterface001::IsHandInGrabbableState(bool isLeft)
{
    Hand &hand = isLeft ? *g_leftHand : *g_rightHand;
    return hand.IsInGrabbableState();
}

void HiggsInterface001::DisableHand(bool isLeft)
{
    if (isLeft) {
        isLeftHandDisabled = true;
    }
    else {
        isRightHandDisabled = true;
    }
}

void HiggsInterface001::EnableHand(bool isLeft)
{
    if (isLeft) {
        isLeftHandDisabled = false;
    }
    else {
        isRightHandDisabled = false;
    }
}

bool HiggsInterface001::IsDisabled(bool isLeft)
{
    if (isLeft) {
        return isLeftHandDisabled;
    }
    else {
        return isRightHandDisabled;
    }
}

void HiggsInterface001::DisableWeaponCollision(bool isLeft)
{
    if (isLeft) {
        isLeftWeaponCollisionDisabled = true;
    }
    else {
        isRightWeaponCollisionDisabled = true;
    }
}

void HiggsInterface001::EnableWeaponCollision(bool isLeft)
{
    if (isLeft) {
        isLeftWeaponCollisionDisabled = false;
    }
    else {
        isRightWeaponCollisionDisabled = false;
    }
}

bool HiggsInterface001::IsWeaponCollisionDisabled(bool isLeft)
{
    if (isLeft) {
        return isLeftWeaponCollisionDisabled;
    }
    else {
        return isRightWeaponCollisionDisabled;
    }
}

void HiggsInterface001::ForceWeaponCollisionEnabled(bool isLeft)
{
    forceEnableWeaponCollision[isLeft] = true;
}

bool HiggsInterface001::IsTwoHanding()
{
    return g_rightHand->IsTwoHanding() || g_leftHand->IsTwoHanding();
}

bool HiggsInterface001::CanGrabObject(bool isLeft)
{
    Hand &hand = isLeft ? *g_leftHand : *g_rightHand;
    return hand.CanGrabObject();
}

NiObject * HiggsInterface001::GetHandRigidBody(bool isLeft)
{
    Hand &hand = isLeft ? *g_leftHand : *g_rightHand;
    return hand.handBody;
}

NiObject * HiggsInterface001::GetWeaponRigidBody(bool isLeft)
{
    Hand &hand = isLeft ? *g_leftHand : *g_rightHand;
    return hand.weaponBody;
}

NiObject * HiggsInterface001::GetGrabbedRigidBody(bool isLeft)
{
    Hand &hand = isLeft ? *g_leftHand : *g_rightHand;
    if (hand.HasHeldObject()) {
        return hand.selectedObject.rigidBody;
    }
    return nullptr;
}

UInt64 HiggsInterface001::GetHiggsLayerBitfield()
{
    return higgsLayerBitfield;
}

void HiggsInterface001::SetHiggsLayerBitfield(UInt64 bitfield)
{
    higgsLayerBitfield = bitfield;
}

NiTransform HiggsInterface001::GetGrabTransform(bool isLeft)
{
    Hand &hand = isLeft ? *g_leftHand : *g_rightHand;
    return hand.GetGrabTransform();
}

void HiggsInterface001::SetGrabTransform(bool isLeft, const NiTransform &transform)
{
    Hand &hand = isLeft ? *g_leftHand : *g_rightHand;
    hand.SetGrabTransform(transform);
}

bool HiggsInterface001::IsHoldingObject(bool isLeft)
{
    Hand &hand = isLeft ? *g_leftHand : *g_rightHand;
    return hand.HasHeldObject();
}

void HiggsInterface001::GetFingerValues(bool isLeft, float values[5])
{
    Hand &hand = isLeft ? *g_leftHand : *g_rightHand;
    for (int i = 0; i < 5; i++) {
        values[i] = hand.grabbedFingerValues[i];
    }
}
