namespace {
using namespace HiggsHands;
unsigned controllerChecks=0;
#define CHECK_CONTROLLER(x) do{++controllerChecks;if(!(x))throw std::runtime_error(std::string("line ")+std::to_string(__LINE__)+": " #x);}while(false)
bool Near(float a,float b){return std::abs(a-b)<.002f;}
struct CaptureResult {std::array<HIGGS_MotionSample,256> samples{};uint32_t count=0;};
HiggsMotion::ObservationTicket Ticket(){
    const auto ticket=HiggsMotion::Tracking().BeginObservation();HiggsMotion::Event event;event.expectedGeneration=ticket.generation;event.observationSequence=ticket.sequence;event.observedAtNs=1000+ticket.sequence;
    for(unsigned i=0;i<2;++i)event.hands[i]=HiggsMotion::Convert(i+1,{1,2,3},{.1f,.2f,.3f},{{{1,0,0},{0,1,0},{0,0,1}}});HiggsMotion::Tracking().Apply(event);return ticket;
}
CaptureResult Capture(NiAVObject* hmd,vr_1_0_12::TrackedDevicePose_t* poses,uint32_t count,uint64_t after=0){
    const auto ticket=Ticket();ObserveNativeControllers(hmd,poses,count,ticket.generation,ticket.sequence,1000+ticket.sequence);HIGGS_ObservationQuery query{sizeof(query),3,HIGGS_SAMPLE_CONTROLLER,0,Service().Epoch(),after,0};CaptureResult result;CHECK_CONTROLLER(Observations().CopyMotion(query,result.samples.data(),256,result.count)==HIGGS_HAND_OK);return result;
}
}
int main(){try{
    vr_1_0_12::IVRSystem system;BSOpenVR vr{&system};openVrPointer=&vr;
    HiggsMotion::Event world;world.kind=HiggsMotion::EventKind::World;world.world=123;HiggsMotion::Tracking().Apply(world);Service().ObserveWorld(123,false,Now());
    std::array<HIGGS_HandRecord,2> hands{};for(unsigned i=0;i<2;++i){hands[i].physicalHand=i+1;hands[i].observationMask=HIGGS_OBS_HELD_KNOWN|HIGGS_OBS_EQUIPMENT_KNOWN|HIGGS_OBS_PALM_VALID|HIGGS_OBS_TRACKING_VALID;hands[i].palmRotation[0]=hands[i].palmRotation[4]=hands[i].palmRotation[8]=1;}Service().Publish(hands,Now());
    std::array<vr_1_0_12::TrackedDevicePose_t,4> poses{};poses[0].mDeviceToAbsoluteTracking.m[1][3]=1.6f;poses[1].mDeviceToAbsoluteTracking.m[0][3]=.2f;poses[1].mDeviceToAbsoluteTracking.m[1][3]=1.3f;poses[1].mDeviceToAbsoluteTracking.m[2][3]=-.4f;poses[2].mDeviceToAbsoluteTracking.m[0][3]=-.2f;
    poses[1].vVelocity={{1,2,3}};poses[1].vAngularVelocity={{.1f,.2f,.3f}};
    NiTransform worldFromTracking;worldFromTracking.rot={{{0,-1,0},{1,0,0},{0,0,1}}};worldFromTracking.pos={100,200,300};worldFromTracking.scale=1.25f;NiAVObject hmd;hmd.m_worldTransform=worldFromTracking*Converted(poses[0].mDeviceToAbsoluteTracking);
    auto observed=Capture(&hmd,poses.data(),4);CHECK_CONTROLLER(observed.count==4);const auto& raw=observed.samples[0];const auto& resolved=observed.samples[1];CHECK_CONTROLLER(raw.space==HIGGS_SPACE_OPENVR_TRACKING&&raw.physicalHand==1&&raw.deviceIndex==1);CHECK_CONTROLLER(Near(raw.position[0],.2f)&&Near(raw.position[1],1.3f)&&Near(raw.position[2],-.4f));
    const auto expected=worldFromTracking*Converted(poses[1].mDeviceToAbsoluteTracking);CHECK_CONTROLLER(Near(resolved.position[0],expected.pos.x)&&Near(resolved.position[1],expected.pos.y)&&Near(resolved.position[2],expected.pos.z));CHECK_CONTROLLER(Near(resolved.linearVelocity[0],3.75f)&&Near(resolved.linearVelocity[1],1.25f)&&Near(resolved.linearVelocity[2],2.5f));CHECK_CONTROLLER(Near(resolved.angularVelocity[0],.3f)&&Near(resolved.angularVelocity[1],.1f)&&Near(resolved.angularVelocity[2],.2f));
    for(unsigned i=0;i<4;++i)CHECK_CONTROLLER(observed.samples[i].captureId==raw.captureId&&observed.samples[i].spaceGeneration==raw.spaceGeneration&&observed.samples[i].trackingGeneration==raw.trackingGeneration);
    HIGGS_ObservationQuery query{sizeof(query),3,0,HIGGS_SPACE_WORLD,Service().Epoch(),0,0};std::array<HIGGS_TrackingMappingSample,64> mappings{};uint32_t n=0;CHECK_CONTROLLER(Observations().CopyMappings(query,mappings.data(),64,n)==HIGGS_HAND_OK&&n==1&&Near(mappings[0].unitsPerMeter,87.5f));
    for(unsigned row=0;row<3;++row){double mapped=mappings[0].translation[row];for(unsigned col=0;col<3;++col)mapped+=mappings[0].rotation[row*3+col]*raw.position[col]*mappings[0].unitsPerMeter;CHECK_CONTROLLER(Near(float(mapped),resolved.position[row]));}
    uint64_t cursor=observed.samples[3].sequence;observed=Capture(nullptr,poses.data(),4,cursor);CHECK_CONTROLLER(observed.count==4&&observed.samples[0].status==HIGGS_SAMPLE_AVAILABLE&&observed.samples[1].status==HIGGS_SAMPLE_UNAVAILABLE&&!(observed.samples[1].flags&HIGGS_SAMPLE_POSE));
    cursor=observed.samples[3].sequence;poses[1].bPoseIsValid=false;observed=Capture(&hmd,poses.data(),4,cursor);CHECK_CONTROLLER(observed.count==4&&observed.samples[0].status==HIGGS_SAMPLE_INVALID&&observed.samples[1].status==HIGGS_SAMPLE_INVALID&&observed.samples[2].status==HIGGS_SAMPLE_AVAILABLE);poses[1].bPoseIsValid=true;
    cursor=observed.samples[3].sequence;system.left=UINT32_MAX;observed=Capture(&hmd,poses.data(),4,cursor);CHECK_CONTROLLER(observed.count==4&&observed.samples[0].status==HIGGS_SAMPLE_DISCONNECTED);system.left=1;
    cursor=observed.samples[3].sequence;system.right=1;observed=Capture(&hmd,poses.data(),4,cursor);CHECK_CONTROLLER(observed.count==4&&observed.samples[0].status==HIGGS_SAMPLE_UNAVAILABLE&&observed.samples[2].status==HIGGS_SAMPLE_UNAVAILABLE);system.right=2;
    cursor=observed.samples[3].sequence;poses[1].mDeviceToAbsoluteTracking.m[0][0]=-1;observed=Capture(&hmd,poses.data(),4,cursor);CHECK_CONTROLLER(observed.count==4&&observed.samples[0].status==HIGGS_SAMPLE_INVALID);poses[1].mDeviceToAbsoluteTracking.m[0][0]=1;
    cursor=observed.samples[3].sequence;poses[1].vVelocity.v[0]=std::numeric_limits<float>::quiet_NaN();observed=Capture(&hmd,poses.data(),4,cursor);CHECK_CONTROLLER(observed.count==4&&!(observed.samples[0].flags&HIGGS_SAMPLE_LINEAR)&&!(observed.samples[1].flags&HIGGS_SAMPLE_LINEAR)&&(observed.samples[1].flags&HIGGS_SAMPLE_ANGULAR));poses[1].vVelocity.v[0]=1;
    cursor=observed.samples[3].sequence;conversionHook=[](){throw std::runtime_error("converter seam failure");};observed=Capture(&hmd,poses.data(),4,cursor);CHECK_CONTROLLER(observed.count==0);conversionHook={};
    const auto old=Ticket();Ticket();ObserveNativeControllers(&hmd,poses.data(),4,old.generation,old.sequence,1000+old.sequence);query={sizeof(query),3,HIGGS_SAMPLE_CONTROLLER,0,Service().Epoch(),cursor,0};uint32_t count=999;CHECK_CONTROLLER(Observations().CopyMotion(query,observed.samples.data(),256,count)==HIGGS_HAND_OK&&!count);
    bool fired=false;conversionHook=[&](){if(!fired){fired=true;HiggsMotion::Event loading;loading.kind=HiggsMotion::EventKind::BeginLoad;HiggsMotion::Tracking().Apply(loading);}};observed=Capture(&hmd,poses.data(),4,cursor);CHECK_CONTROLLER(observed.count==0&&fired);conversionHook={};
    std::cout<<"PASS native controller observation source "<<controllerChecks<<" checks; explicit SDK conversion seam\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
