// Engine seams for the actual ControllerStateCB source. Broker/frame services are
// production code. Native Hand internals are checked separately by hand fixtures.
#include "hand_input_extended.h"
#include "hand_broker.h"
#include <array>
#include <cmath>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>
enum class EventResult { kEvent_Continue };
struct MenuOpenCloseEvent { struct Name { const char* data=nullptr; } menuName; bool opening=false; };
template<class T> struct EventDispatcher {};
namespace MenuChecker { class AllMenuEventHandler { public: EventResult ReceiveEvent(MenuOpenCloseEvent*,EventDispatcher<MenuOpenCloseEvent>*); }; }
namespace vr_src {
using TrackedDeviceIndex_t=uint32_t;
enum ETrackedControllerRole{TrackedControllerRole_LeftHand=1,TrackedControllerRole_RightHand=2};
enum ETrackedDeviceProperty{Prop_Axis0Type_Int32=3002};
enum ETrackedPropertyError{TrackedProp_Success=0,TrackedProp_Error=1};
struct VRControllerState001_t {uint32_t unPacketNum=0;uint64_t ulButtonPressed=0,ulButtonTouched=0;struct Axis{float x=0,y=0;}rAxis[5];};
struct System {
    uint32_t left=1,right=2;std::array<int,5> axisTypes{1,3,0,4,0};
    uint32_t GetTrackedDeviceIndexForControllerRole(ETrackedControllerRole role){return role==TrackedControllerRole_LeftHand?left:right;}
    int GetInt32TrackedDeviceProperty(uint32_t,ETrackedDeviceProperty property,ETrackedPropertyError* error){const auto index=uint32_t(property)-3002;if(index>=5)throw std::runtime_error("invalid axis");*error=index==3?TrackedProp_Error:TrackedProp_Success;return axisTypes[index];}
};
}
bool initComplete=true;struct OpenVr{vr_src::System* vrSystem=nullptr;};OpenVr* vrPointer=nullptr;OpenVr** g_openVR=&vrPointer;
struct PlayerCharacter{bool root=true;PlayerCharacter* GetNiNode(){return root?this:nullptr;}};PlayerCharacter* playerPointer=nullptr;PlayerCharacter** g_thePlayer=&playerPointer;
struct Hand{
    uint32_t physicalHand=1;bool useTouchForGrip=false,synthesize=false;unsigned updates=0;
    void ControllerStateUpdate(uint32_t,vr_src::VRControllerState001_t* state){++updates;HiggsHands::Service().Input(physicalHand,HIGGS_INPUT_TRIGGER,(state->ulButtonPressed&(UINT64_C(1)<<33))!=0,HiggsHands::Now());HiggsHands::Service().Input(physicalHand,HIGGS_INPUT_GRIP,((useTouchForGrip?state->ulButtonTouched:state->ulButtonPressed)&(UINT64_C(1)<<2))!=0,HiggsHands::Now());if(synthesize)state->ulButtonPressed|=UINT64_C(1)<<1;}
};
Hand leftHand{1},rightHand{2};Hand* g_leftHand=&leftHand;Hand* g_rightHand=&rightHand;
