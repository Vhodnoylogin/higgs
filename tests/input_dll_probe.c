#include "higgsinput.h"
#include "higgsinputcontext.h"
#include <Windows.h>
#include <stdio.h>
#include <string.h>
static unsigned checks;
#define REQUIRE(x) do{++checks;if(!(x)){fprintf(stderr,"line%d: %s\n",__LINE__,#x);return 1;}}while(0)
static void HIGGS_HAND_CALL Observe(const HIGGS_InputEvent* event,void* context){(void)event;(void)context;}
static void HIGGS_HAND_CALL Context(const HIGGS_InputContextState* previous,const HIGGS_InputContextState* current,void* data){(void)previous;(void)current;(void)data;}
int main(int argc,char** argv){
    REQUIRE(argc==2);HMODULE module=LoadLibraryA(argv[1]);REQUIRE(module!=NULL);
    HIGGS_QueryInputInterfaceFn query=(HIGGS_QueryInputInterfaceFn)GetProcAddress(module,"HIGGS_QueryInputInterface");HIGGS_QueryHandInterfaceFn handQuery=(HIGGS_QueryHandInterfaceFn)GetProcAddress(module,"HIGGS_QueryHandInterface");REQUIRE(query&&handQuery);
    HIGGS_InputInterface input={0};REQUIRE(query(1,sizeof(input),&input)==HIGGS_HAND_OK&&input.structSize==sizeof(input)&&input.capabilities==31);HIGGS_InputInterface before=input;REQUIRE(query(2,sizeof(input),&input)==HIGGS_HAND_UNSUPPORTED_VERSION&&memcmp(&before,&input,sizeof(input))==0);REQUIRE(query(1,sizeof(input)-1,&input)==HIGGS_HAND_BUFFER_TOO_SMALL);
    HIGGS_HandInterface hands={0};REQUIRE(handQuery(1,sizeof(hands),&hands)==HIGGS_HAND_OK);HIGGS_HandClientInfo info={0};info.structSize=sizeof(info);memcpy(info.name,"C-input",8);HIGGS_HandClient client=0;REQUIRE(hands.RegisterClient(&info,&client)==HIGGS_HAND_OK);
    HIGGS_InputSnapshot snapshot={0};snapshot.structSize=sizeof(snapshot);REQUIRE(input.CopySnapshot(client,&snapshot)==HIGGS_HAND_NOT_READY&&snapshot.epoch==0&&snapshot.hands[0].frameId==0&&snapshot.hands[1].frameId==0);
    HIGGS_QueryInputContextInterfaceFn contextQuery=(HIGGS_QueryInputContextInterfaceFn)GetProcAddress(module,"HIGGS_QueryInputContextInterface");REQUIRE(contextQuery);HIGGS_InputContextInterface context={0};REQUIRE(contextQuery(1,sizeof(context),&context)==HIGGS_HAND_OK&&context.capabilities==7);HIGGS_InputContextState ui={0};ui.structSize=sizeof(ui);REQUIRE(context.CopyContext(client,&ui)==HIGGS_HAND_OK&&ui.kind==HIGGS_CONTEXT_GAMEPLAY&&ui.generation&&ui.epoch==0);HIGGS_HandRegistration contextToken=0;REQUIRE(context.Subscribe(client,Context,NULL,&contextToken)==HIGGS_HAND_OK);REQUIRE(hands.RemoveRegistration(client,contextToken)==HIGGS_HAND_OK);
    HIGGS_ControlInfo controls[64];memset(controls,0xcc,sizeof(controls));unsigned count=0;REQUIRE(input.CopyControlCatalog(controls,1,&count)==HIGGS_HAND_BUFFER_TOO_SMALL&&count==14&&controls[0].id==0xcccccccc);REQUIRE(input.CopyControlCatalog(controls,64,&count)==HIGGS_HAND_OK&&count==14);
    HIGGS_InputRecognizerSpec spec={0};spec.structSize=sizeof(spec);spec.physicalHands=3;spec.controlMask=1ULL<<7;spec.callback=Observe;HIGGS_HandRegistration subscription=0;REQUIRE(input.RegisterRecognizer(client,&spec,&subscription)==HIGGS_HAND_OK);REQUIRE(hands.RemoveRegistration(client,subscription)==HIGGS_HAND_OK);
    HIGGS_InputFrameQuery frames={sizeof(frames),3,1,0};HIGGS_ControllerFrame output[128];count=999;REQUIRE(input.CopyFrames(client,&frames,output,128,&count)==HIGGS_HAND_NOT_READY&&count==999);REQUIRE(hands.UnregisterClient(client)==HIGGS_HAND_OK);count=999;REQUIRE(input.CopyFrames(client,&frames,output,128,&count)==HIGGS_HAND_INVALID_CLIENT&&count==999);REQUIRE(FreeLibrary(module));printf("PASS input C DLL %u checks\n",checks);return 0;
}
