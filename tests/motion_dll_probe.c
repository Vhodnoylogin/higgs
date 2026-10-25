#include "higgsmotion.h"
#include <stdio.h>
#include <string.h>
#ifdef _WIN32
#include <windows.h>
#else
#include <dlfcn.h>
#endif

typedef uint64_t (HIGGS_MOTION_CALL *FixtureControl)(uint32_t,uint64_t);
int main(int argc,char** argv) {
    HIGGS_QueryMotionInterfaceFn query=NULL;
    FixtureControl control=NULL;
    HIGGS_MotionInterface api;
    HIGGS_MotionSnapshot state;
    unsigned char sentinel[sizeof(api)];
    int checks=0,result=0;
#ifdef _WIN32
    HMODULE module;
    FARPROC querySymbol,controlSymbol;
#else
    void* module;
    void* querySymbol;
    void* controlSymbol;
#endif
#define CHECK(condition) do{++checks;if(!(condition)){result=checks;goto done;}}while(0)
#define READ() do{memset(&state,0,sizeof(state));state.structSize=sizeof(state);CHECK(api.CopyVelocitySnapshot(&state)==HIGGS_MOTION_OK);}while(0)
    if(argc!=2)return 1;
#ifdef _WIN32
    module=LoadLibraryA(argv[1]);if(!module)return 2;
    querySymbol=GetProcAddress(module,"HIGGS_QueryMotionInterface");controlSymbol=GetProcAddress(module,"MotionFixtureControl");
#else
    module=dlopen(argv[1],RTLD_NOW);if(!module)return 2;
    querySymbol=dlsym(module,"HIGGS_QueryMotionInterface");controlSymbol=dlsym(module,"MotionFixtureControl");
#endif
    _Static_assert(sizeof(query)==sizeof(querySymbol),"Function pointer width");
    _Static_assert(sizeof(control)==sizeof(controlSymbol),"Control pointer width");
    memcpy(&query,&querySymbol,sizeof(query));memcpy(&control,&controlSymbol,sizeof(control));
    CHECK(query&&control);
    memset(&api,0xa5,sizeof(api));memcpy(sentinel,&api,sizeof(api));
    CHECK(query(0,sizeof(api),&api)==HIGGS_MOTION_UNSUPPORTED_VERSION&&!memcmp(&api,sentinel,sizeof(api)));
    CHECK(query(UINT32_MAX,sizeof(api),&api)==HIGGS_MOTION_UNSUPPORTED_VERSION&&!memcmp(&api,sentinel,sizeof(api)));
    CHECK(query(1,sizeof(api)-1,&api)==HIGGS_MOTION_BUFFER_TOO_SMALL&&!memcmp(&api,sentinel,sizeof(api)));
    CHECK(query(1,sizeof(api),&api)==HIGGS_MOTION_OK&&api.capabilities==UINT64_C(15)&&api.historySize==5);
    CHECK(api.CopyVelocitySnapshot(NULL)==HIGGS_MOTION_INVALID_ARGUMENT);
    memset(&state,0xa5,sizeof(state));state.structSize=sizeof(state)-1;
    {HIGGS_MotionSnapshot old=state;CHECK(api.CopyVelocitySnapshot(&state)==HIGGS_MOTION_BUFFER_TOO_SMALL&&!memcmp(&state,&old,sizeof(state)));}
    READ();CHECK(!state.flags&&!state.availableHands&&state.continuityGeneration==1&&!state.processAcceptedObservation);
    CHECK(state.hands[0].physicalHand==1&&state.hands[1].physicalHand==2&&!state.hands[0].validFields&&!state.hands[0].acceptedObservation);
    CHECK(control(1,0)>1);CHECK(control(2,0)>1);READ();
    CHECK(state.availableHands==3&&state.flags==HIGGS_MOTION_WORLD_OBSERVED&&state.hands[0].acceptedObservation&&state.hands[0].observedAtNs==0);
    CHECK(state.hands[0].validFields==63&&state.hands[0].ageNs==state.copiedAtNs&&state.hands[1].meanSpeed==0&&state.hands[0].historyObservationCount==1);
    CHECK(state.hands[0].latestLinear.y==-3&&state.hands[0].latestAngular.y==-6&&state.hands[0].sourceFlags==3);
    control(3,UINT64_MAX);READ();
    CHECK(state.availableHands==2&&state.hands[0].status==HIGGS_MOTION_INVALID_POSE&&state.hands[0].validFields==0&&!state.hands[0].historyObservationCount&&state.hands[1].historyObservationCount==2);
    CHECK(state.hands[0].observedAtNs==UINT64_MAX&&state.hands[0].ageNs==0&&state.hands[0].acceptedObservation);
    CHECK(state.hands[1].validFields==31); /* Valid numeric zero with unknown future age. */
    {uint64_t generation=state.continuityGeneration,ticket=state.processAcceptedObservation;
        CHECK(control(4,0)>generation);READ();
        CHECK(state.flags==HIGGS_MOTION_LOADING&&!state.availableHands&&state.processAcceptedObservation==ticket);
        CHECK(!state.hands[0].acceptedObservation&&!state.hands[0].observedAtNs&&!state.hands[0].ageNs&&!state.hands[0].sourceFlags&&!state.hands[0].validFields);
    }
    control(2,0);READ();CHECK(!state.availableHands);
    control(5,0);READ();CHECK(!state.flags&&!state.availableHands);
    control(1,0);control(2,0);READ();CHECK(state.availableHands==3);
    control(7,0);READ();CHECK(!state.flags&&!state.availableHands&&!state.hands[1].acceptedObservation);
    control(6,0);READ();CHECK(!state.flags&&!state.availableHands);
done:
#ifdef _WIN32
    FreeLibrary(module);
#else
    dlclose(module);
#endif
    if(result)fprintf(stderr,"FAIL isolated velocity DLL C probe check %d\n",result);
    else printf("PASS isolated velocity DLL C discovery/lifecycle %d checks\n",checks);
    return result;
}
