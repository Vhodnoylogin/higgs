#pragma once
// Deliberately synthetic engine seams for the extracted actual release branch.
// This checks source ordering/exclusivity, never Skyrim layouts or inventory.
#include "hand_broker.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <functional>
using UInt32=uint32_t;
using std::min;
using std::max;
struct NiPoint3 {};
struct NiAVObject {};
enum { kFormType_Book=1 };
struct TESForm { virtual ~TESForm()=default; uint32_t formType=0; };
struct TESObjectBOOK : TESForm { struct Data { enum {kType_Spell=1}; uint32_t flags=0; } data; };
struct TESObjectREFR { TESForm* baseForm=nullptr; uint32_t formID=42; int extraData=1; };
#define DYNAMIC_CAST(value,from,to) dynamic_cast<to*>(value)
static unsigned activations=0,equips=0,pickups=0,spellBooks=0,consumedCallbacks=0,stashedCallbacks=0,droppedCallbacks=0;
static std::function<void()> onConsumed,onStashed,onSemantic;
static void TESObjectREFR_Activate(TESObjectREFR*,void*,int,int,UInt32,bool) {++activations;}
static UInt32 BSExtraList_GetCount(int*) {return 1;}
namespace papyrusActor {inline void EquipItemEx(void*,TESForm*,int,bool,bool) {++equips;}}
static void ConsumeSpellBook(void*,TESObjectBOOK*) {++spellBooks;}
using Actor_PickUpObject=void(*)(void*,TESObjectREFR*,UInt32,bool,bool);
static void PickUp(void*,TESObjectREFR*,UInt32,bool,bool) {++pickups;}
template<class T> T get_vfunc(void*,int) {return PickUp;}
static void* GetNodeFromCollidable(void*) {return nullptr;}
static float NiAVObject_GetMass(void*,int) {return 1;}
static void PlayPhysicsSound(const NiPoint3&,bool) {}
namespace HiggsPluginAPI {
inline void TriggerConsumedCallbacks(bool,TESForm*) {++consumedCallbacks; if(onConsumed)onConsumed();}
inline void TriggerStashedCallbacks(bool,TESForm*) {++stashedCallbacks; if(onStashed)onStashed();}
inline void TriggerDroppedCallbacks(bool,TESObjectREFR*) {++droppedCallbacks;}
}
namespace Config {struct Options {
    float mouthDropHapticStrength=1,mouthDropHapticFadeTime=1,shoulderDropHapticStrength=1,shoulderDropHapticFadeTime=1;
    float grabBaseHapticStrength=1,grabProportionalHapticStrength=1,grabHapticMassExponent=1,grabHapticFadeTime=1;
    bool skipActivateBooks=false,useLoudSoundDrop=false;
} options;}
static double g_currentFrameTime=1;
static bool sharedSpatialReady=true;
struct Hand {
    enum class State {Held,HeldBody,Other} state=State::Held;
    bool isLeft=true,disableConsumeStash=false,disableDropEvents=false,mouthEligible=true,stashEligible=true;
    uint64_t nativeRegionEpoch=0; uint32_t nativeRegionInsideMask=0;
    double droppedTime=0;
    struct Selected {bool isBook=false; void* collidable=nullptr;} selectedObject;
    struct Haptics {unsigned count=0; void QueueHapticEvent(float,float,float) {++count;}} haptics;
    TESForm form; TESObjectREFR reference;
    Hand() {reference.baseForm=&form;}
    void UpdateNativeRegionSelection(NiAVObject*,const NiPoint3&,const NiPoint3&) {
        HIGGS_NativeRegionFrame frame{}; frame.structSize=sizeof(frame); frame.physicalHand=HIGGS_HAND_LEFT;
        frame.flags=HIGGS_REGION_SOURCE_VALID; frame.sourceGeneration=1; frame.hmdScale=1;
        frame.hmdRotation[0]=frame.hmdRotation[4]=frame.hmdRotation[8]=1;
        for(auto& sphere:frame.spheres) sphere.worldRadius=1;
        nativeRegionInsideMask=HiggsHands::Service().EvaluateNativeRegions(frame); nativeRegionEpoch=frame.epoch;
    }
    bool IsObjectConsumable(TESObjectREFR*,NiAVObject*,const NiPoint3&) const {return mouthEligible&&(nativeRegionInsideMask&HIGGS_REGION_MOUTH);}
    bool IsObjectDepositable(TESObjectREFR*,NiAVObject*,const NiPoint3&) const {return stashEligible&&(nativeRegionInsideMask&(HIGGS_REGION_RIGHT_SHOULDER|HIGGS_REGION_LEFT_SHOULDER));}
    void ProbeRegionRelease();
};
static unsigned checks=0;
#define CHECK(condition) do {++checks; if(!(condition)){std::fprintf(stderr,"FAIL %d %s\n",__LINE__,#condition);std::abort();}}while(0)
static HIGGS_HandStatus HIGGS_HAND_CALL Spatial(const HIGGS_NativeRegionFrame* frame,HIGGS_NativeRegionResult* result,void*) {
    result->structSize=sizeof(*result);result->epoch=frame->epoch;result->sampleId=frame->sampleId;
    result->insideMask=sharedSpatialReady?HIGGS_REGION_ALL:0;return HIGGS_HAND_OK;
}
static void HIGGS_HAND_CALL Semantic(const HIGGS_HandEvent* event,void*) {
    if((event->type==HIGGS_EVENT_CONSUMPTION_REQUESTED||event->type==HIGGS_EVENT_STASH_REQUESTED)&&onSemantic)onSemantic();
}
static uint64_t owner=0,detector=0,events=0;
static void Prepare() {
    auto& broker=HiggsHands::Service(); broker.ObserveWorld(0,false,HiggsHands::Now()); broker.ObserveWorld(1,false,HiggsHands::Now());
    std::array<HIGGS_HandRecord,2> hands{};
    for(auto& h:hands) h.observationMask=HIGGS_OBS_HELD_KNOWN|HIGGS_OBS_EQUIPMENT_KNOWN|HIGGS_OBS_PALM_VALID|HIGGS_OBS_TRACKING_VALID;
    hands[0].heldRefId=42;broker.Publish(hands,HiggsHands::Now());broker.MarkMainThread();
    activations=equips=pickups=spellBooks=consumedCallbacks=stashedCallbacks=droppedCallbacks=0;
    onConsumed={};onStashed={};onSemantic={};sharedSpatialReady=true;
}
static int RunReleaseCases() {
    auto& broker=HiggsHands::Service(); HIGGS_HandClientInfo info{};info.structSize=sizeof(info);
    std::snprintf(info.name,sizeof(info.name),"source-release-fixture");CHECK(!broker.Register(info,owner));
    CHECK(!broker.NativeRegionDetector(owner,Spatial,nullptr,detector));CHECK(!broker.Events(owner,Semantic,nullptr,events));
    Prepare();{Hand hand;hand.UpdateNativeRegionSelection(nullptr,{},{});hand.ProbeRegionRelease();
        CHECK(activations==1&&equips==1&&consumedCallbacks==1&&!stashedCallbacks&&!droppedCallbacks);}
    Prepare();{Hand hand;hand.mouthEligible=false;hand.UpdateNativeRegionSelection(nullptr,{},{});hand.ProbeRegionRelease();
        CHECK(activations==1&&!equips&&stashedCallbacks==1&&!consumedCallbacks&&!droppedCallbacks);}
    Prepare();{Hand hand;hand.disableConsumeStash=true;hand.UpdateNativeRegionSelection(nullptr,{},{});hand.ProbeRegionRelease();
        CHECK(!activations&&!equips&&!stashedCallbacks&&!consumedCallbacks&&droppedCallbacks==1);}
    Prepare();{Hand hand;hand.UpdateNativeRegionSelection(nullptr,{},{});sharedSpatialReady=false;hand.ProbeRegionRelease();
        CHECK(!activations&&!stashedCallbacks&&!consumedCallbacks&&droppedCallbacks==1);}
    Prepare();{Hand hand;hand.UpdateNativeRegionSelection(nullptr,{},{});onConsumed=[]{sharedSpatialReady=false;};hand.ProbeRegionRelease();
        CHECK(!activations&&!equips&&consumedCallbacks==1&&!stashedCallbacks&&droppedCallbacks==1);}
    Prepare();{Hand hand;hand.UpdateNativeRegionSelection(nullptr,{},{});onConsumed=[&]{broker.SetInputContext(1,HiggsHands::Now());broker.SetInputContext(0,HiggsHands::Now());};hand.ProbeRegionRelease();
        CHECK(!activations&&!equips&&consumedCallbacks==1&&!stashedCallbacks&&droppedCallbacks==1);}
    Prepare();{Hand hand;hand.mouthEligible=false;hand.UpdateNativeRegionSelection(nullptr,{},{});onStashed=[&]{broker.ObserveWorld(0,false,HiggsHands::Now());};hand.ProbeRegionRelease();
        CHECK(!activations&&!pickups&&stashedCallbacks==1&&!consumedCallbacks&&droppedCallbacks==1);}
    Prepare();{Hand hand;hand.UpdateNativeRegionSelection(nullptr,{},{});onSemantic=[&]{broker.SetInputContext(1,HiggsHands::Now());};hand.ProbeRegionRelease();
        CHECK(!activations&&!equips&&!stashedCallbacks&&droppedCallbacks==1);}
    CHECK(!broker.RemoveNativeRegionDetector(owner,detector));CHECK(!broker.Remove(owner,events));CHECK(!broker.Unregister(owner));
    std::printf("PASS extracted native region release branch: %u checks (synthetic engine seams)\n",checks);return 0;
}
