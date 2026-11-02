// Declared engine seams for the ACTUAL native controller observation functions.
// Full native compilation separately checks SDK declarations and call signatures.
#include "hand_observations.h"
#include "hand_broker.h"
#include "controller_tracking.h"
#include <array>
#include <cmath>
#include <cstring>
#include <functional>
#include <iostream>
#include <stdexcept>
#include <string>
struct NiPoint3 {
    float x=0,y=0,z=0;
    NiPoint3 operator+(NiPoint3 b)const{return {x+b.x,y+b.y,z+b.z};}
    NiPoint3 operator*(float b)const{return {x*b,y*b,z*b};}
};
struct NiMatrix33 {
    float data[3][3]{{1,0,0},{0,1,0},{0,0,1}};
    NiMatrix33 Transpose()const{NiMatrix33 r;for(unsigned i=0;i<3;++i)for(unsigned j=0;j<3;++j)r.data[i][j]=data[j][i];return r;}
    NiMatrix33 operator*(const NiMatrix33& b)const{NiMatrix33 r;for(unsigned i=0;i<3;++i)for(unsigned j=0;j<3;++j){r.data[i][j]=0;for(unsigned k=0;k<3;++k)r.data[i][j]+=data[i][k]*b.data[k][j];}return r;}
    NiPoint3 operator*(NiPoint3 p)const{return {data[0][0]*p.x+data[0][1]*p.y+data[0][2]*p.z,data[1][0]*p.x+data[1][1]*p.y+data[1][2]*p.z,data[2][0]*p.x+data[2][1]*p.y+data[2][2]*p.z};}
};
struct NiTransform {
    NiMatrix33 rot;NiPoint3 pos;float scale=1;
    NiPoint3 operator*(NiPoint3 p)const{return pos+(rot*p)*scale;}
    NiTransform operator*(const NiTransform& b)const{NiTransform r;r.rot=rot*b.rot;r.pos=*this*b.pos;r.scale=scale*b.scale;return r;}
    void Invert(NiTransform& out)const{out.rot=rot.Transpose();out.scale=1/scale;out.pos=(out.rot*(pos*-1))*out.scale;}
};
class NiAVObject {public:NiTransform m_worldTransform;};
namespace vr_1_0_12 {
constexpr uint32_t k_unTrackedDeviceIndex_Hmd=0,k_unMaxTrackedDeviceCount=64;
enum ETrackingResult {TrackingResult_Running_OK=200,TrackingResult_Invalid=0};
enum ETrackedControllerRole {TrackedControllerRole_LeftHand=1,TrackedControllerRole_RightHand=2};
struct HmdMatrix34_t {float m[3][4]{{1,0,0,0},{0,1,0,0},{0,0,1,0}};};
struct HmdVector3_t {float v[3]{};};
struct TrackedDevicePose_t {HmdMatrix34_t mDeviceToAbsoluteTracking;HmdVector3_t vVelocity,vAngularVelocity;ETrackingResult eTrackingResult=TrackingResult_Running_OK;bool bPoseIsValid=true,bDeviceIsConnected=true;};
struct IVRSystem {
    std::array<bool,64> connected{};uint32_t left=1,right=2;
    IVRSystem(){connected.fill(true);}
    uint32_t GetTrackedDeviceIndexForControllerRole(ETrackedControllerRole role){return role==TrackedControllerRole_LeftHand?left:right;}
    bool IsTrackedDeviceConnected(uint32_t index){if(index>=64)throw std::runtime_error("invalid device queried");return connected[index];}
};
}
struct BSOpenVR {vr_1_0_12::IVRSystem* vrSystem=nullptr;};
BSOpenVR* openVrPointer=nullptr;BSOpenVR** g_openVR=&openVrPointer;
float physicsScale=1.f/70.f;float* g_havokWorldScale=&physicsScale;
std::function<void()> conversionHook;
NiTransform Converted(const vr_1_0_12::HmdMatrix34_t& m){
    const NiMatrix33 basis{{{1,0,0},{0,0,-1},{0,1,0}}};NiMatrix33 raw;
    for(unsigned i=0;i<3;++i)for(unsigned j=0;j<3;++j)raw.data[i][j]=m.m[i][j];
    NiTransform r;r.rot=basis*raw*basis.Transpose();r.pos=(basis*NiPoint3{m.m[0][3],m.m[1][3],m.m[2][3]})*70;return r;
}
void HmdMatrixToNiTransform(NiTransform& out,const vr_1_0_12::HmdMatrix34_t& m){out=Converted(m);if(conversionHook)conversionHook();}
