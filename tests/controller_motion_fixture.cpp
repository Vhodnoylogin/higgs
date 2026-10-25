// Actual production bodies are included below. These narrow engine doubles do
// not establish SKSE/Havok layout, conversion relocation or hook compatibility.
#include "controller_tracking.h"
#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <deque>
#include <functional>
#include <iostream>
#include <limits>
#include <numeric>
#include <stdexcept>
#include <string>
// Model Windows' mixed-type min/max macros used by these native source bodies.
template<class A,class B> auto min(A a,B b){return a<b?a:b;}
template<class A,class B> auto max(A a,B b){return a>b?a:b;}
using UInt32 = std::uint32_t;
struct NiPoint3 {
    float x=0, y=0, z=0;
    NiPoint3 operator+(NiPoint3 b) const { return {x+b.x,y+b.y,z+b.z}; }
    NiPoint3 operator-(NiPoint3 b) const { return {x-b.x,y-b.y,z-b.z}; }
    NiPoint3 operator*(float b) const { return {x*b,y*b,z*b}; }
    NiPoint3 operator/(float b) const { return {x/b,y/b,z/b}; }
    NiPoint3& operator+=(NiPoint3 b) { *this=*this+b; return *this; }
    NiPoint3& operator*=(float b) { *this=*this*b; return *this; }
};
float DotProduct(NiPoint3 a, NiPoint3 b) { return a.x*b.x+a.y*b.y+a.z*b.z; }
float VectorLength(NiPoint3 a) { return std::sqrt(DotProduct(a,a)); }
NiPoint3 VectorNormalized(NiPoint3 a) { return a/VectorLength(a); }
NiPoint3 CrossProduct(NiPoint3 a, NiPoint3 b) { return {a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x}; }
NiPoint3 ProjectVectorOntoPlane(NiPoint3 a, NiPoint3 normal) { return a-normal*DotProduct(a,normal); }
struct NiMatrix33 {
    float data[3][3]{{1,0,0},{0,1,0},{0,0,1}};
    NiMatrix33 Transpose() const { NiMatrix33 out; for(int i=0;i<3;++i) for(int j=0;j<3;++j) out.data[i][j]=data[j][i]; return out; }
    NiMatrix33 operator*(const NiMatrix33& b) const {
        NiMatrix33 out; for(int i=0;i<3;++i) for(int j=0;j<3;++j) {out.data[i][j]=0;for(int k=0;k<3;++k)out.data[i][j]+=data[i][k]*b.data[k][j];} return out;
    }
};
struct NiTransform { NiMatrix33 rot; NiPoint3 pos; NiPoint3 operator*(NiPoint3 a) const { return a+pos; } };
struct NiAVObject { NiTransform m_worldTransform; };
template<class T> struct NiPointer {
    T* pointer=nullptr;
    NiPointer()=default; NiPointer(T* p):pointer(p){}
    NiPointer& operator=(T* p){pointer=p;return *this;}
    operator T*() const {return pointer;}
    T* operator->() const {return pointer;}
};
struct PlayerCharacter {
    struct Node { enum { kNode_HmdNode }; };
    bool root=true; NiAVObject rootNode; NiAVObject* unk3F0[1]{};
    NiAVObject* GetNiNode() {return root?&rootNode:nullptr;}
};
namespace vr_src {
    using TrackedDeviceIndex_t=std::uint32_t;
    constexpr TrackedDeviceIndex_t k_unTrackedDeviceIndex_Hmd=0,k_unMaxTrackedDeviceCount=64;
    enum class ETrackedControllerRole { TrackedControllerRole_LeftHand, TrackedControllerRole_RightHand };
    enum class ETrackingResult { TrackingResult_Running_OK, Invalid };
    struct HmdMatrix34_t { float m[3][4]{{1,0,0,0},{0,1,0,0},{0,0,1,0}}; };
    struct Vector { float v[3]{}; };
    struct TrackedDevicePose_t { bool bDeviceIsConnected=true,bPoseIsValid=true; ETrackingResult eTrackingResult=ETrackingResult::TrackingResult_Running_OK; HmdMatrix34_t mDeviceToAbsoluteTracking; Vector vVelocity,vAngularVelocity; };
    struct IVRSystem {
        std::uint32_t left=1,right=2; std::array<bool,64> connected{};
        IVRSystem(){connected.fill(true);}
        TrackedDeviceIndex_t GetTrackedDeviceIndexForControllerRole(ETrackedControllerRole role){return role==ETrackedControllerRole::TrackedControllerRole_LeftHand?left:right;}
        bool IsTrackedDeviceConnected(std::uint32_t index){if(index>=connected.size())throw std::runtime_error("queried invalid role");return connected[index];}
    };
}
struct BSOpenVR {vr_src::IVRSystem* vrSystem=nullptr;};
std::atomic<bool> initComplete{false};
BSOpenVR* openVRPointer=nullptr; BSOpenVR** g_openVR=&openVRPointer;
PlayerCharacter* playerPointer=nullptr; PlayerCharacter** g_thePlayer=&playerPointer;
NiMatrix33 conversionRotation; std::function<void()> conversionHook;
void HmdMatrixToNiTransform(NiTransform& output,const vr_src::HmdMatrix34_t&){output.rot=conversionRotation;if(conversionHook)conversionHook();}
namespace HiggsSynergy { void BeginGameLoad(){} void EndGameLoad(){} void NewGame(){} }

enum { kFormType_Potion=1,kFormType_Ingredient=2,kFormType_Book=3 };
struct TESForm { virtual ~TESForm()=default; int formType=kFormType_Potion; bool IsPlayable(){return true;} };
struct TESObjectBOOK:TESForm { struct Data {enum{kType_CantBeTaken=1};int flags=0;}data; };
struct AlchemyItem:TESForm {bool IsPoison(){return false;} };
struct TESObjectREFR {TESForm* baseForm=nullptr;};
#define DYNAMIC_CAST(value,from,to) dynamic_cast<to*>(value)
namespace Config {
    struct Options {
        float shoulderVelocityThreshold=2,mouthVelocityThreshold=2,rightShoulderRadius=1,leftShoulderRadius=1,mouthRadius=1;
        NiPoint3 rightShoulderHmdOffset,leftShoulderHmdOffset,mouthHmdOffset;
        bool enableDrinkPoison=false,inheritTangentialVelocity=true;
        float throwVelocityThreshold=1,throwVelocityBoostFactor=1,angularVelocityMultiplier=0.6f,tangentialVelocityLimit=100;
        float selectedCloseFingerAnimMaxHandSpeed=1,fingerAnimateStartDoubleSpeedTime=1,fingerAnimateStartLinearSpeed=1,fingerAnimateStartAngularSpeed=1,selectedCloseFingerAnimValue=0.5f;
        float pullSpeedThreshold=1.2f,lootSpeedThreshold=1.2f,lootToGrabSpeedThreshold=0.5f,lootToGrabLeewayTime=0.1f;
        float selectionLockedBaseHapticStrength=0.1f,selectionLockedProportionalHapticStrength=0.5f;
        float pullHapticFadeTime=1;
    } options;
}
double g_currentFrameTime=10;
NiPoint3 g_prevDeltaVelocityWithVrikSmoothingOnly{3,4,5};
struct hkVector4 {NiPoint3 value;};
NiPoint3 HkVectorToNiPoint(hkVector4 value){return value.value;}
struct Body { NiPoint3 center{0,2,0}; hkVector4& getCenterOfMassInWorld(hkVector4& out){out.value=center;return out;} };
struct ReleaseResult {NiPoint3 total,angular,tangential;bool collide;};
struct Hand {
    bool isLeft=true,twoHand=false;float avgPlayerSpeedWorldspace=4;NiPoint3 avgPlayerVelocityWorldspace{2,3,4};
    float havokWorldScale=0.5f; NiPoint3 hkPalmPos,hkHandPos{3,0,0};NiAVObject* handNode=nullptr; float handSize=1;
    TESObjectREFR* selectedObj=nullptr;
    struct Selected {bool isActor=false;NiPoint3 point;Body* rigidBody=nullptr;std::deque<NiPoint3> localLinearVelocities{5,NiPoint3{2,0,0}};}selectedObject;
    struct Haptics {int pulses=0;void QueueHapticPulse(float){++pulses;}void QueueHapticEvent(float,float,float){++pulses;}}haptics;
    struct Animator {int calls=0;void SetFingerValues(float,float,float){++calls;}}fingerAnimator;
    double rolloverDisplayTime=0;bool isSelectedNear=false;int pulls=0,loots=0,grabs=0;float collisionSpeed=0;
    bool IsObjectPullable(){return true;}
    void TransitionPulled(){++pulls;}
    void TransitionPreGrab(TESObjectREFR*,bool){++loots;}
    template<class... Args> void TransitionHeld(Args&&...){++grabs;}
    bool IsTwoHanding(){return twoHand;}
    void TriggerCollisionHaptics(float,float speed){collisionSpeed=speed;}
    NiPoint3 GetHandVelocity();
    bool IsObjectConsumable(TESObjectREFR*,NiAVObject*,const NiPoint3&)const;
    bool IsObjectDepositable(TESObjectREFR*,NiAVObject*,const NiPoint3&)const;
    void ProbePull();void ProbeLoot();void ProbeAnimation();ReleaseResult ProbeRelease();NiPoint3 ProbeBodyRelease();
};
Hand leftHand,rightHand;Hand* g_leftHand=&leftHand;Hand* g_rightHand=&rightHand;
namespace HiggsPluginAPI {void TriggerCollisionCallbacks(bool,float,float){} }
#ifdef _MSC_VER
#pragma warning(push)
// Pre-existing native peak/animation bodies use signed deque indices and
// implicit double-to-float animation speeds. Keep other fixture warnings fatal.
#pragma warning(disable:4018 4244)
#endif
#include "motion_source.inc"
#ifdef _MSC_VER
#pragma warning(pop)
#endif

namespace {
    int checks=0;
    void Check(bool okay){++checks;if(!okay)throw std::runtime_error("motion fixture check "+std::to_string(checks));}
    bool Equal(NiPoint3 a,NiPoint3 b){return a.x==b.x&&a.y==b.y&&a.z==b.z;}
    bool Finite(NiPoint3 a){return std::isfinite(a.x)&&std::isfinite(a.y)&&std::isfinite(a.z);}
    vr_src::IVRSystem vr;BSOpenVR openVR;PlayerCharacter player;NiAVObject hmd;
    std::array<vr_src::TrackedDevicePose_t,3> poses;
    Body body;
    void Seed() {
        conversionHook={};conversionRotation={};vr={};openVR.vrSystem=&vr;openVRPointer=&openVR;g_openVR=&openVRPointer;
        hmd={};player={};player.unk3F0[0]=&hmd;playerPointer=&player;poses={};
        poses[1].vVelocity.v[0]=3;poses[2].vVelocity.v[0]=4;poses[1].vAngularVelocity.v[1]=1;
        initComplete=true;ProbeNewGame();ObserveControllerWorld(1);
        body={};leftHand={};rightHand={};rightHand.isLeft=false;leftHand.selectedObject.rigidBody=rightHand.selectedObject.rigidBody=&body;
        for(int i=0;i<5;++i)FillControllerVelocities(&hmd,poses.data(),3);
    }
    void CallbackFailures() {
        for(int failure=0;failure<16;++failure) {
            Seed();
            switch(failure) {
            case 0: FillControllerVelocities(nullptr,poses.data(),3);break;
            case 1: FillControllerVelocities(&hmd,nullptr,3);break;
            case 2: FillControllerVelocities(&hmd,poses.data(),0);break;
            case 3: g_openVR=nullptr;FillControllerVelocities(&hmd,poses.data(),3);break;
            case 4: openVRPointer=nullptr;FillControllerVelocities(&hmd,poses.data(),3);break;
            case 5: openVR.vrSystem=nullptr;FillControllerVelocities(&hmd,poses.data(),3);break;
            case 6: vr.connected[0]=false;FillControllerVelocities(&hmd,poses.data(),3);break;
            case 7: poses[0].bDeviceIsConnected=false;FillControllerVelocities(&hmd,poses.data(),3);break;
            case 8: poses[0].bPoseIsValid=false;FillControllerVelocities(&hmd,poses.data(),3);break;
            case 9: poses[0].eTrackingResult=vr_src::ETrackingResult::Invalid;FillControllerVelocities(&hmd,poses.data(),3);break;
            case 10: poses[0].mDeviceToAbsoluteTracking.m[0][0]=std::numeric_limits<float>::quiet_NaN();FillControllerVelocities(&hmd,poses.data(),3);break;
            case 11: conversionRotation.data[0][0]=std::numeric_limits<float>::infinity();FillControllerVelocities(&hmd,poses.data(),3);break;
            case 12: hmd.m_worldTransform.rot.data[2][2]=0;FillControllerVelocities(&hmd,poses.data(),3);break;
            case 13: playerPointer=nullptr;WaitPosesCB(nullptr,0,poses.data(),3);break;
            case 14: player.root=false;WaitPosesCB(nullptr,0,poses.data(),3);break;
            case 15: player.unk3F0[0]=nullptr;WaitPosesCB(nullptr,0,poses.data(),3);break;
            }
            const auto pair=HiggsMotion::Tracking().CopyAll();Check(!pair[0].available&&!pair[1].available);
            Check(Equal(TrackedVelocityPoint(pair[0].peakVelocity),{})&&Equal(TrackedVelocityPoint(pair[1].latestAngular),{}));
        }
        for(int failure=0;failure<10;++failure) {
            Seed();
            switch(failure){
            case 0:vr.left=UINT32_MAX;break;case 1:vr.left=0;break;case 2:vr.connected[1]=false;break;
            case 3:poses[1].bDeviceIsConnected=false;break;case 4:poses[1].bPoseIsValid=false;break;
            case 5:poses[1].vVelocity.v[0]=std::numeric_limits<float>::quiet_NaN();break;
            case 6:poses[1].vAngularVelocity.v[0]=std::numeric_limits<float>::infinity();break;
            case 7:vr.left=vr_src::k_unMaxTrackedDeviceCount;break;case 8:vr.left=3;break;
            case 9:poses[1].eTrackingResult=vr_src::ETrackingResult::Invalid;break;}
            FillControllerVelocities(&hmd,poses.data(),3);
            Check(!HiggsMotion::Tracking().Copy(true).available&&HiggsMotion::Tracking().Copy(false).available);
            Check(HiggsMotion::Tracking().Copy(false).averageSpeed==4);
            poses[1]={};poses[1].vVelocity.v[0]=2;vr.left=1;vr.connected[1]=true;
            FillControllerVelocities(&hmd,poses.data(),3);Check(HiggsMotion::Tracking().Copy(true).averageSpeed==0.4f);
        }
        Seed();poses[2].bPoseIsValid=false;FillControllerVelocities(&hmd,poses.data(),3);
        Check(HiggsMotion::Tracking().Copy(true).available&&!HiggsMotion::Tracking().Copy(false).available);
        Check(HiggsMotion::Tracking().Copy(true).averageSpeed==3);
        Seed();vr.left=vr.right;FillControllerVelocities(&hmd,poses.data(),3);
        Check(!HiggsMotion::Tracking().Copy(true).available&&!HiggsMotion::Tracking().Copy(false).available);
        Seed();conversionHook=[] {ObserveControllerWorld(2);};FillControllerVelocities(&hmd,poses.data(),3);
        Check(!HiggsMotion::Tracking().Copy(true).available); // no lock across engine conversion; old ticket rejected
        Seed();ProbePreLoadGame();FillControllerVelocities(&hmd,poses.data(),3);Check(!HiggsMotion::Tracking().Copy(true).available);
        ProbePostLoadGame();FillControllerVelocities(&hmd,poses.data(),3);Check(!HiggsMotion::Tracking().Copy(true).available);
        ObserveControllerWorld(1);FillControllerVelocities(&hmd,poses.data(),3);Check(HiggsMotion::Tracking().Copy(true).averageSpeed==0.6f);
        ObserveControllerWorld(0);Check(!HiggsMotion::Tracking().Copy(false).available);
        initComplete=false;g_openVR=nullptr;WaitPosesCB(nullptr,0,nullptr,0);Check(!HiggsMotion::Tracking().Copy(true).available);
    }
    void ReentrantObservationOrder() {
        Seed();bool entered=false;HiggsMotion::Snapshot inner;
        conversionHook=[&] {
            if(entered)return;entered=true;
            poses[1].bPoseIsValid=false;
            FillControllerVelocities(&hmd,poses.data(),3);
            inner=HiggsMotion::Tracking().Copy(true);
            poses[1].bPoseIsValid=true;
        };
        FillControllerVelocities(&hmd,poses.data(),3);
        auto result=HiggsMotion::Tracking().Copy(true);
        Check(entered&&!inner.available&&!result.available);
        Check(result.lastAcceptedObservation==inner.lastAcceptedObservation&&result.observedAtNs==inner.observedAtNs);
        Check(HiggsMotion::Tracking().Copy(false).available);
        Seed();entered=false;poses[1].bPoseIsValid=false;
        conversionHook=[&] {
            if(entered)return;entered=true;
            poses[1].bPoseIsValid=true;poses[1].vVelocity.v[0]=9;
            FillControllerVelocities(&hmd,poses.data(),3);
            inner=HiggsMotion::Tracking().Copy(true);
            poses[1].bPoseIsValid=false;
        };
        FillControllerVelocities(&hmd,poses.data(),3);
        result=HiggsMotion::Tracking().Copy(true);
        Check(entered&&inner.available&&result.available&&result.latestLinear.x==9);
        Check(result.lastAcceptedObservation==inner.lastAcceptedObservation&&result.observedAtNs==inner.observedAtNs);
    }
    void LegacyAndConsumers() {
        Seed();std::deque<NiPoint3> legacy(5);HiggsMotion::ControllerTracking owner;
        HiggsMotion::Event event;event.kind=HiggsMotion::EventKind::World;event.world=1;owner.Apply(event);
        event={};
        for(int i=0;i<200;++i) {
            const NiPoint3 value{static_cast<float>(i%13-6),static_cast<float>(i%7-3),static_cast<float>(i%5)};
            legacy.pop_back();legacy.push_front(value);const auto ticket=owner.BeginObservation();event.expectedGeneration=ticket.generation;event.observationSequence=ticket.sequence;event.hands[0]={HiggsMotion::Status::Valid,1,{value.x,value.y,value.z},{}};owner.Apply(event);
            const auto snapshot=owner.Copy(true);Check(Equal(TrackedVelocityPoint(snapshot.peakVelocity),GetMaxVelocity(legacy)));
            Check(Equal(TrackedVelocityPoint(snapshot.averageVelocity),std::accumulate(legacy.begin(),legacy.end(),NiPoint3())/5.f));
            float speed=0;for(auto v:legacy)speed+=VectorLength(v);Check(snapshot.averageSpeed==speed/5.f);
        }
        Seed();Check(leftHand.GetHandVelocity().x==3);TESForm form;TESObjectREFR reference{&form};
        poses[1].vVelocity={};poses[1].vAngularVelocity={};for(int i=0;i<5;++i)FillControllerVelocities(&hmd,poses.data(),3);
        Check(leftHand.IsObjectConsumable(&reference,&hmd,{})&&leftHand.IsObjectDepositable(&reference,&hmd,{}));
        leftHand.ProbeAnimation();Check(leftHand.fingerAnimator.calls==1);
        poses[1].bPoseIsValid=false;FillControllerVelocities(&hmd,poses.data(),3);
        Check(Equal(leftHand.GetHandVelocity(),{}));
        Check(!leftHand.IsObjectConsumable(&reference,&hmd,{})&&!leftHand.IsObjectDepositable(&reference,&hmd,{}));
        leftHand.ProbeAnimation();Check(leftHand.fingerAnimator.calls==1);
        leftHand.ProbePull();leftHand.ProbeLoot();Check(!leftHand.pulls&&!leftHand.loots&&!leftHand.grabs&&!leftHand.haptics.pulses);
        const auto release=leftHand.ProbeRelease();Check(Finite(release.total)&&Finite(release.angular)&&Finite(release.tangential));
        Check(Equal(release.total,leftHand.avgPlayerVelocityWorldspace*leftHand.havokWorldScale));
        Check(Equal(release.angular,{})&&Equal(release.tangential,{}));
        Check(Equal(leftHand.ProbeBodyRelease(),NiPoint3{5,4,5})); // object + room velocity preserved
        TriggerCollisionHapticsUsingHandSpeed(1,true);Check(leftHand.collisionSpeed==leftHand.avgPlayerSpeedWorldspace);
        leftHand.twoHand=true;TriggerCollisionHapticsUsingHandSpeed(1,true);Check(leftHand.collisionSpeed==8&&rightHand.collisionSpeed==8);
        Seed();leftHand.ProbePull();leftHand.ProbeLoot();Check(leftHand.pulls==1&&leftHand.loots==1);
        const auto spinning=leftHand.ProbeRelease();Check(Finite(spinning.total)&&spinning.angular.z==0.6f&&spinning.tangential.x<0);
        body.center={};const auto onAxis=leftHand.ProbeRelease();Check(Finite(onAxis.total)&&Equal(onAxis.tangential,{}));
    }
}
int main(){try{CallbackFailures();ReentrantObservationOrder();LegacyAndConsumers();std::cout<<"PASS source callback/consumer fixture "<<checks<<" checks\n";return 0;}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
