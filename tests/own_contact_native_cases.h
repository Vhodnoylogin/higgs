extern "C" int own_contact_c_probe(void);
using namespace HiggsHands;
namespace {
uint64_t owner=0,other=0,worldSerial=100;
HIGGS_CosmeticContactInterface api{};HIGGS_OwnContactNativeInterface native{};
hkpWorld physicalWorld;bhkWorld world;
TESObjectWEAP sword;TESObjectARMO shield;
NiNode swordNode,shieldNode,heldModels[2];
bhkRigidBody weapons[2],originals[2],heldWrappers[2];hkpRigidBody weaponBodies[2],originalBodies[2],heldBodies[2];bhkShape shapes[6];hkpShape nativeShapes[6];
TESObjectREFR heldRefs[2];ExtraContainerChanges changes;ExtraContainerChanges::Data inventory;InventoryEntryData entries[2];List<BaseExtraList> extras[2];BaseExtraList worn[2];ExtraUniqueID ids[2];
std::array<HIGGS_HandRecord,2> hands{};
void Bodies(bhkRigidBody& wrapper,hkpRigidBody& body,unsigned i){wrapper.hkBody=&body;body.world=&physicalWorld;body.collidable.shape=&nativeShapes[i];nativeShapes[i].user=reinterpret_cast<uintptr_t>(&shapes[i]);shapes[i].shape=&nativeShapes[i];body.filter=56;}
void Publish(){Service().Publish(hands,Now());PublishNativeOwnContacts();}
void Setup(bool uid){
    InvalidateNativeOwnContacts();Service().ObserveWorld(++worldSerial,false,Now());world.world=&physicalWorld;fakePlayer.parentCell=&world;fakePlayer.actorState.drawn=true;fakePlayer.formID=0x14;MenuChecker::stopped=false;fakeLeftHanded=false;
    sword.formID=100;sword.formType=kFormType_Weapon;sword.gameData.type=1;sword.twoHanded=false;shield.formID=101;shield.formType=kFormType_Armor;shield.bipedObject.mask=BGSBipedObjectForm::kPart_Shield;
    fakePlayer.equipped[0]=&shield;fakePlayer.equipped[1]=&sword;changes.data=&inventory;inventory.entries.clear();fakePlayer.extraData.extras.clear();fakePlayer.extraData.extras[kExtraData_ContainerChanges]=&changes;
    leftHand.held=rightHand.held=false;leftHand.twoHand=rightHand.twoHand=false;
    for(unsigned i=0;i<2;++i){
        Bodies(weapons[i],weaponBodies[i],i);Bodies(originals[i],originalBodies[i],i+2);Bodies(heldWrappers[i],heldBodies[i],i+4);
        auto node=i==0?&shieldNode:&swordNode;node->collision=&originals[i];melee[i].collisionNode=node;
        auto& hand=i==0?leftHand:rightHand;hand.weaponBody=&weapons[i];hand.clonedFromWeaponShape=Hand::ShapeIdentifier{&originals[i],&originalBodies[i],&shapes[i+2],&nativeShapes[i+2]};hand.synergyContactHoldEpisode++;
        entries[i].type=i==0?static_cast<TESForm*>(&shield):static_cast<TESForm*>(&sword);entries[i].extendDataList=&extras[i];extras[i].values.clear();extras[i].values.push_back(&worn[i]);worn[i].extras.clear();worn[i].count=1;worn[i].extras[i==0?kExtraData_WornLeft:kExtraData_Worn]=nullptr;ids[i].ownerFormId=0x14;ids[i].uniqueId=static_cast<UInt16>(i+10);if(uid)worn[i].extras[kExtraData_UniqueID]=&ids[i];inventory.entries.push_back(&entries[i]);
        hands[i]={};hands[i].observationMask=HIGGS_OBS_HELD_KNOWN|HIGGS_OBS_EQUIPMENT_KNOWN|HIGGS_OBS_TRACKING_VALID|HIGGS_OBS_PALM_VALID;hands[i].equippedFormId=i==0?101:100;
        heldRefs[i].formID=700+i;heldRefs[i].baseForm=i==0?static_cast<TESForm*>(&shield):static_cast<TESForm*>(&sword);heldRefs[i].model=&heldModels[i];references[700+i]=&heldRefs[i];
    }
    Publish();CHECK(!HIGGS_QueryCosmeticContactInterface(1,sizeof(api),&api));CHECK(!HIGGS_QueryOwnContactNativeInterface(1,sizeof(native),&native));
}
HIGGS_ContactItemFrame Items(){HIGGS_ContactItemFrame f{};f.structSize=sizeof(f);CHECK(!api.CopyItems(owner,&f));return f;}
HIGGS_CosmeticContactPolicy Policy(uint32_t provenance=7){HIGGS_CosmeticContactPolicy p{};p.structSize=sizeof(p);p.epoch=Service().Epoch();p.feedbackKinds=3;p.leftSources=p.rightSources=3;p.leftClasses=p.rightClasses=7;p.minRelativeSpeed=0.1f;p.minClosingSpeed=0.05f;p.cooldownMs=50;p.leaseMs=60000;p.maxObservationAgeMs=100;p.provenanceMask=provenance;return p;}
HIGGS_CosmeticContactState Arm(HIGGS_CosmeticContactPolicy p){HIGGS_CosmeticContactState state{};state.structSize=sizeof(state);CHECK(!api.RequestPolicy(owner,&p,&state));return state;}
HIGGS_NativeOwnContact Contact(){HIGGS_NativeOwnContact c{};c.structSize=sizeof(c);c.producer=1;c.bodyA=reinterpret_cast<uintptr_t>(&weaponBodies[0]);c.bodyB=reinterpret_cast<uintptr_t>(&weaponBodies[1]);c.world=reinterpret_cast<uintptr_t>(&physicalWorld);c.bToANormal[0]=-1;c.pointVelocityA[0]=1;c.mainFrame=100;c.physicsTick=100;c.contactFlags=HIGGS_NATIVE_OWN_RESPONSE_DISABLED;return c;}
uint32_t Read(uint64_t lease,HIGGS_CosmeticContact* result,uint32_t capacity,uint64_t after=0){HIGGS_CosmeticContactQuery q{};q.structSize=sizeof(q);q.epoch=Service().Epoch();q.leaseId=lease;q.afterSequence=after;uint32_t count=0;for(unsigned i=0;i<capacity;++i)result[i].structSize=sizeof(*result);CHECK(!api.CopyContacts(owner,&q,result,capacity,&count));return count;}
void EquippedProvenance(){
    Setup(false);auto frame=Items();CHECK(frame.itemCount==2);CHECK(frame.items[0].provenance==HIGGS_CONTACT_PROVENANCE_EQUIPPED_COLLIDER);auto state=Arm(Policy(4));auto input=Contact();uint64_t sequence=0;const auto reads=engineReads;CHECK(!native.Observe(&input,&sequence));CHECK(sequence&&engineReads==reads&&worldReads==0);HIGGS_CosmeticContact values[8]{};CHECK(Read(state.leaseId,values,8)==1);CHECK(values[0].relativeSpeed==1&&values[0].closingSpeed==1);CHECK(values[0].flags&HIGGS_COSMETIC_NATIVE_RESPONSE_DISABLED);
    CHECK(native.Observe(&input,&sequence)==HIGGS_HAND_NOT_FOUND); // Same-frame/tick manifold duplicate.
    input.bodyA=1;input.bodyB=2;CHECK(native.Observe(&input,&sequence)==HIGGS_HAND_NOT_FOUND);CHECK(engineReads==reads);
    // A same-form, no-UID equip event ends the old contact episode synchronously.
    auto old=frame.items[0];HiggsOwnEquipEvent event{};event.actor=&fakePlayer;event.baseObject=101;event.equipped=true;eventDispatchers.unk4D0.Send(event);input=Contact();input.physicsTick++;CHECK(native.Observe(&input,&sequence)==HIGGS_HAND_NOT_READY);Publish();frame=Items();CHECK(frame.items[0].itemInstance!=old.itemInstance);CHECK(Read(state.leaseId,values,8)==0);
    Setup(true);frame=Items();CHECK(frame.items[0].provenance==HIGGS_CONTACT_PROVENANCE_EQUIPPED_UID);state=Arm(Policy(2));input=Contact();CHECK(!native.Observe(&input,&sequence));CHECK(Read(state.leaseId,values,8)==1);
    Setup(false);state=Arm(Policy(2));input=Contact();CHECK(native.Observe(&input,&sequence)==HIGGS_HAND_NOT_FOUND); // No silent provenance downgrade.
}
void MotionAndLifecycle(){
    Setup(false);auto policy=Policy();policy.minClosingSpeed=0;auto state=Arm(policy);auto input=Contact();input.pointVelocityA[0]=input.pointVelocityB[0]=100;uint64_t sequence=0;CHECK(native.Observe(&input,&sequence)==HIGGS_HAND_NOT_FOUND); // Shared fast motion is not an impact.
    input=Contact();input.bodyB=reinterpret_cast<uintptr_t>(&originalBodies[1]);CHECK(native.Observe(&input,&sequence)==HIGGS_HAND_NOT_FOUND);
    input=Contact();input.world=1;CHECK(native.Observe(&input,&sequence)==HIGGS_HAND_NOT_FOUND);
    input=Contact();input.bToANormal[0]=0;CHECK(native.Observe(&input,&sequence)==HIGGS_HAND_INVALID_ARGUMENT);
    input=Contact();CHECK(!native.Observe(&input,&sequence));auto bad=input;bad.physicsTick++;bad.mainFrame++;CHECK(native.Observe(&bad,&sequence)==HIGGS_HAND_NOT_FOUND); // Cooldown across ticks.
    HIGGS_CosmeticContact output[8]{};CHECK(Read(state.leaseId,output,8)==1);uint32_t required=77;output[0].structSize=0;HIGGS_CosmeticContactQuery q{sizeof(q),0,Service().Epoch(),0,state.leaseId,0,0};CHECK(api.CopyContacts(owner,&q,output,8,&required)==HIGGS_HAND_INVALID_ARGUMENT&&required==77);
    auto current=Items();++leftHand.synergyContactHoldEpisode;leftHand.held=true;leftHand.selectedObject.handle=700;leftHand.selectedObject.rigidBody=&heldWrappers[0];hands[0].heldRefId=700;Publish();auto frame=Items();CHECK(frame.itemCount==3);CHECK(frame.items[0].provenance==HIGGS_CONTACT_PROVENANCE_HELD_REFERENCE);
    InvalidateNativeOwnContacts();CHECK(native.Observe(&input,&sequence)==HIGGS_HAND_NOT_READY);Publish();CHECK(!api.EndPolicy(owner,state.leaseId,Service().Epoch()));CHECK(Read(state.leaseId,output,8)==0);
    state=Arm(Policy());Service().ObserveWorld(++worldSerial,false,Now());CHECK(api.CopyContacts(owner,&q,output,8,&required)==HIGGS_HAND_STALE);HIGGS_CosmeticContactState expired{};expired.structSize=sizeof(expired);CHECK(!api.CopyPolicy(owner,state.leaseId,&expired));CHECK(expired.stage==HIGGS_COSMETIC_ENDED);
    (void)current;
}
void PublicationFence(){
    for(int depth:{1,0}){
        Setup(false);auto state=Arm(Policy());auto input=Contact();auto captured=snapshot.load();
        afterRetainedAssignDepth=depth;afterRetainedAssign=[](){HiggsOwnEquipEvent event{};event.actor=&fakePlayer;event.baseObject=101;event.equipped=true;eventDispatchers.unk4D0.Send(event);};
        Publish();CHECK(!afterRetainedAssign);HIGGS_ContactItemFrame frame{};frame.structSize=sizeof(frame);CHECK(api.CopyItems(owner,&frame)==HIGGS_HAND_NOT_READY);
        uint64_t sequence=0;CHECK(native.Observe(&input,&sequence)==HIGGS_HAND_NOT_READY);
        CHECK(!OwnContacts().Publish(captured->frame,7,Now())); // A delayed stale commit cannot reopen readiness.
        CHECK(api.CopyItems(owner,&frame)==HIGGS_HAND_NOT_READY);
        Publish();CHECK(!api.CopyItems(owner,&frame));
        const auto& a=captured->sources[1].item;const auto& b=captured->sources[3].item;
        CHECK(OwnContacts().Observe(captured->frame,a,b,input,Now(),sequence)==HIGGS_HAND_STALE);
        HIGGS_CosmeticContact results[8]{};CHECK(Read(state.leaseId,results,8)==0);
    }
    afterRetainedAssignDepth=0;
}
void ReversalAndReplacement(){
    Setup(false);auto p=Policy();p.cooldownMs=0;auto state=Arm(p);auto input=Contact();uint64_t sequence=0;
    CHECK(!native.Observe(&input,&sequence));HIGGS_CosmeticContact values[8]{};CHECK(Read(state.leaseId,values,8)==1);auto first=values[0];
    std::swap(input.bodyA,input.bodyB);for(unsigned i=0;i<3;++i){input.bToANormal[i]=-input.bToANormal[i];std::swap(input.pointVelocityA[i],input.pointVelocityB[i]);}
    CHECK(native.Observe(&input,&sequence)==HIGGS_HAND_NOT_FOUND); // Reversed manifold is the same pair/tick.
    ++input.physicsTick;++input.mainFrame;CHECK(!native.Observe(&input,&sequence));CHECK(Read(state.leaseId,values,8)==2);
    CHECK(values[1].left.itemInstance==first.left.itemInstance&&values[1].right.itemInstance==first.right.itemInstance&&values[1].relativeSpeed==first.relativeSpeed&&values[1].closingSpeed==first.closingSpeed);
    CHECK(values[1].rightToLeftNormal[0]==first.rightToLeftNormal[0]&&values[1].leftPointVelocity[0]==first.leftPointVelocity[0]);
    // A fresh exact body may keep the item episode, but must change body generation
    // and retire all receipts for the old body even when base/model are unchanged.
    const auto old=Items().items[0];hkpRigidBody replacement;bhkRigidBody wrapper;Bodies(wrapper,replacement,0);leftHand.weaponBody=&wrapper;InvalidateNativeOwnContacts();Publish();auto now=Items().items[0];
    CHECK(now.itemInstance==old.itemInstance&&now.bodyGeneration!=old.bodyGeneration);CHECK(Read(state.leaseId,values,8)==0);
    input=Contact();CHECK(native.Observe(&input,&sequence)==HIGGS_HAND_NOT_FOUND);input.bodyA=reinterpret_cast<uintptr_t>(&replacement);CHECK(!native.Observe(&input,&sequence));
    leftHand.weaponBody=&weapons[0];InvalidateNativeOwnContacts();Publish();
    leftHand.clonedFromWeaponShape.reset();InvalidateNativeOwnContacts();Publish();auto missing=Items();CHECK(missing.sources[1]==HIGGS_CONTACT_SOURCE_NOT_READY&&missing.itemCount==1);input=Contact();CHECK(native.Observe(&input,&sequence)==HIGGS_HAND_NOT_FOUND);
}
void RingAndOwnership(){
    Setup(false);auto p=Policy();p.cooldownMs=0;p.minClosingSpeed=0;p.minRelativeSpeed=0;auto state=Arm(p);auto input=Contact();uint64_t sequence=0;for(unsigned i=0;i<520;++i){input.mainFrame=1000+i;input.physicsTick=1000+i;CHECK(!native.Observe(&input,&sequence));}
    HIGGS_CosmeticContact values[512]{};CHECK(Read(state.leaseId,values,512)==512);CHECK(values[0].sequence+511==values[511].sequence);CHECK(Read(state.leaseId,values,512,values[511].sequence)==0);
    HIGGS_CosmeticContactState out{};out.structSize=sizeof(out);CHECK(api.CopyPolicy(other,state.leaseId,&out)==HIGGS_HAND_WRONG_OWNER);auto update=p;update.leaseId=state.leaseId;update.expectedRevision=state.revision+1;CHECK(api.RequestPolicy(owner,&update,&out)==HIGGS_HAND_CONFLICT);update.expectedRevision=state.revision;CHECK(!api.RequestPolicy(owner,&update,&out));CHECK(out.revision==state.revision+1);CHECK(Read(state.leaseId,values,512)==0);
    input.physicsTick++;input.mainFrame++;CHECK(!native.Observe(&input,&sequence));CHECK(!Service().Unregister(owner));HIGGS_CosmeticContactQuery q{sizeof(q),0,Service().Epoch(),0,0,0,0};uint32_t required=99;CHECK(api.CopyContacts(owner,&q,values,512,&required)==HIGGS_HAND_INVALID_CLIENT&&required==99);
}
}
int main(){CHECK(!own_contact_c_probe());CHECK(!HIGGS_QueryCosmeticContactInterface(1,sizeof(api),&api));CHECK(api.capabilities==0);Service().MarkMainThread();HIGGS_HandClientInfo info{};info.structSize=sizeof(info);std::snprintf(info.name,sizeof(info.name),"cosmetic-test");CHECK(!Service().Register(info,owner));std::snprintf(info.name,sizeof(info.name),"other");CHECK(!Service().Register(info,other));EquippedProvenance();MotionAndLifecycle();PublicationFence();ReversalAndReplacement();RingAndOwnership();std::puts("PASS actual own-contact native/C/core provenance, paired motion, lease/dedup/ring and lifecycle");}
