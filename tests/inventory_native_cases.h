extern "C" int inventory_c_probe(void);
namespace {
using namespace HiggsHands;
uint64_t owner=0,other=0,executor=0,worldSerial=100,idempotency=100;
HIGGS_InventoryInterface inventoryApi{};
ContainerBase baseContainer;
AlchemyItem potion;
TESObjectWEAP sword;
std::array<HIGGS_HandRecord,2> observedHands{};
void Publish(){Service().Publish(observedHands,Now());}
void EmptyHands(){for(auto& h:observedHands){h={};h.observationMask=HIGGS_OBS_HELD_KNOWN|HIGGS_OBS_EQUIPMENT_KNOWN|HIGGS_OBS_PALM_VALID|HIGGS_OBS_TRACKING_VALID;}Publish();}
void Reset(){
    Service().ObserveWorld(++worldSerial,false,Now());EmptyHands();
    fakeNoEffect=fakeNoEvents=fakeDelayEquip=fakeDropWrongUID=fakeOverflowAfterPickup=fakeResetDuringRemove=false;fakeLeftHanded=false;
    fakePlayer.flags=0;fakePlayer.formID=0x14;fakePlayer.baseForm=&baseContainer;fakePlayer.equipped[0]=fakePlayer.equipped[1]=nullptr;
    baseContainer.counts.clear();fakePlayer.extraData.extras.clear();auto extra=new ExtraContainerChanges;extra->data=new ExtraContainerChanges::Data;fakePlayer.extraData.extras[kExtraData_ContainerChanges]=extra;
    potion.formID=100;potion.formType=kFormType_Potion;potion.poison=false;sword.formID=101;sword.formType=kFormType_Weapon;sword.twoHanded=sword.bow=false;
    fakeForms[100]=&potion;fakeForms[101]=&sword;FakeEntry(&fakePlayer,&potion)->countDelta=3;FakeEntry(&fakePlayer,&sword)->countDelta=2;
    DrainNativeInventory();CHECK(!HIGGS_QueryInventoryInterface(1,sizeof(inventoryApi),&inventoryApi));CHECK(inventoryApi.capabilities&HIGGS_INVENTORY_CAP_PLAYER_SELECTION);
}
HIGGS_InventorySelection Selection(uint32_t form=100,uint32_t ordinal=0,uint32_t quantity=1){
    HIGGS_InventorySelectionRequest request{sizeof(request),form,Service().Epoch(),ordinal,quantity,{0,0}};uint64_t ticket=0;
    CHECK(!inventoryApi.RequestSelection(owner,&request,&ticket));HIGGS_InventorySelection out{};out.structSize=sizeof(out);CHECK(!inventoryApi.CopySelection(owner,ticket,&out));CHECK(out.state==HIGGS_SELECTION_QUEUED);
    DrainNativeInventory();CHECK(!inventoryApi.CopySelection(owner,ticket,&out));CHECK(out.state==HIGGS_SELECTION_READY);CHECK(out.item.kind==HIGGS_ITEM_NATIVE_INVENTORY);CHECK(out.item.validFields&HIGGS_ITEM_NATIVE_OBSERVED);return out;
}
HIGGS_TransferRequest Transfer(HIGGS_ItemIdentity item,uint32_t kind){
    HIGGS_TransferRequest r{};r.structSize=sizeof(r);r.kind=kind;r.sourceHand=HIGGS_HAND_LEFT;r.destinationHand=HIGGS_HAND_LEFT;r.epoch=Service().Epoch();r.idempotencyKey=++idempotency;r.original=item;r.executor=executor;r.quantity=item.quantity;r.timeoutMs=5000;return r;
}
HIGGS_TransferState State(uint64_t id){HIGGS_TransferState s{};CHECK(!Transactions().Copy(owner,id,s));return s;}
HIGGS_PhysicalRequest physical{};
bool Dispatch(const HIGGS_PhysicalRequest& p,void*){physical=p;return true;}
void Pump(){
    DrainNativeInventory();Transactions().Drain(Now());physical={};Service().DrainPhysical(Dispatch,nullptr,Now());
    if(physical.kind){auto& h=observedHands[physical.physicalHand==HIGGS_HAND_LEFT?0:1];h.heldRefId=physical.kind==HIGGS_PHYSICAL_GRAB?physical.itemRefId:0;Publish();}
}
uint64_t Request(const HIGGS_TransferRequest& r){uint64_t id=0;CHECK(!Transactions().Request(owner,r,id,Now()));return id;}
HIGGS_ItemIdentity Held(TESObjectREFR* ref){
    auto instance=Transactions().ObserveHeld(HIGGS_HAND_LEFT,ref->formID+10000,ref->formID,ref->baseForm->formID,ref->extraData.count,Now());retainedItems[instance]=ref;observedHands[0].heldRefId=static_cast<uint32_t>(ref->formID);Publish();HIGGS_ItemIdentity item{};CHECK(!Transactions().Held(owner,HIGGS_HAND_LEFT,item));return item;
}
struct Permit:InventoryPermit{int begins=0;bool Begin()override{++begins;return true;}};
void CoreRoundTrip(){
    Reset();auto selected=Selection();CHECK(!inventoryApi.GetExecutor(owner,&executor));
    uint64_t e2=0;CHECK(!inventoryApi.GetExecutor(owner,&e2));CHECK(e2==executor);HIGGS_InventorySelection wrong{};wrong.structSize=sizeof(wrong);CHECK(inventoryApi.CopySelection(other,selected.ticket,&wrong)==HIGGS_HAND_WRONG_OWNER);
    auto r=Transfer(selected.item,HIGGS_TRANSFER_INVENTORY_TO_HAND);const auto calls=fakeDropCalls;auto id=Request(r);Transactions().Drain(Now());auto state=State(id);CHECK(state.stage==HIGGS_TX_WAIT_GRIP);CHECK(state.receiptFlags&HIGGS_TX_DOMAIN_ATTESTED);CHECK(state.actualRefId);CHECK(fakeDropCalls==calls+1);
    uint64_t same=0;CHECK(!Transactions().Request(owner,r,same,Now()));CHECK(same==id);CHECK(fakeDropCalls==calls+1);CHECK(inventoryApi.ReleaseSelection(owner,selected.ticket)==HIGGS_HAND_PENDING);
    for(int i=0;i<4;++i)Pump();CHECK(State(id).stage==HIGGS_TX_COMPLETED);CHECK(State(id).receiptFlags&HIGGS_TX_NATIVE_GRIP_CONFIRMED);
    CHECK(!inventoryApi.ReleaseSelection(owner,selected.ticket));Pump();CHECK(inventoryApi.CopySelection(owner,selected.ticket,&wrong)==HIGGS_HAND_NOT_FOUND);
    auto world=fakeRefs[state.actualRefId];auto held=Held(world);auto stash=Request(Transfer(held,HIGGS_TRANSFER_HAND_TO_INVENTORY));const auto pickups=fakePickUpCalls;for(int i=0;i<6;++i)Pump();auto stored=State(stash);CHECK(stored.stage==HIGGS_TX_COMPLETED);CHECK(stored.disposition==HIGGS_DISPOSITION_INVENTORY);CHECK(stored.receiptFlags&HIGGS_TX_NATIVE_RELEASE_CONFIRMED);CHECK(fakePickUpCalls==pickups+1);CHECK(world->flags&TESForm::kFlagIsDeleted);
    CHECK(FakeEntry(&fakePlayer,&potion)->countDelta==3);
}
void MissingPostfacts(){
    Reset();auto selected=Selection();auto id=Request(Transfer(selected.item,HIGGS_TRANSFER_INVENTORY_TO_HAND));fakeNoEvents=true;const auto calls=fakeDropCalls;Transactions().Drain(Now());CHECK(fakeDropCalls==calls+1);CHECK(State(id).stage==HIGGS_TX_EXECUTING);CHECK(!(State(id).receiptFlags&HIGGS_TX_DOMAIN_ATTESTED));Pump();CHECK(State(id).stage==HIGGS_TX_EXECUTING);CHECK(!Transactions().Cancel(owner,id,Service().Epoch()));CHECK(State(id).stage==HIGGS_TX_RECOVERY_REQUIRED);CHECK(fakeDropCalls==calls+1);fakeNoEvents=false;
}
void Compensation(){
    Reset();auto selected=Selection();auto id=Request(Transfer(selected.item,HIGGS_TRANSFER_INVENTORY_TO_HAND));Transactions().Drain(Now());CHECK(State(id).stage==HIGGS_TX_WAIT_GRIP);CHECK(!Transactions().Cancel(owner,id,Service().Epoch()));CHECK(State(id).stage==HIGGS_TX_WAIT_COMPENSATION);Transactions().Drain(Now());CHECK(State(id).stage==HIGGS_TX_COMPLETED);CHECK(State(id).receiptFlags&HIGGS_TX_COMPENSATION_CONFIRMED);CHECK(FakeEntry(&fakePlayer,&potion)->countDelta==3);
    Reset();selected=Selection();id=Request(Transfer(selected.item,HIGGS_TRANSFER_INVENTORY_TO_HAND));Transactions().Drain(Now());CHECK(!Transactions().Cancel(owner,id,Service().Epoch()));fakeOverflowAfterPickup=true;Transactions().Drain(Now());CHECK(State(id).stage==HIGGS_TX_RECOVERY_REQUIRED);CHECK(State(id).receiptFlags&HIGGS_TX_MUTATION_UNCERTAIN);fakeOverflowAfterPickup=false;
}
void DelayedFactsAndReset(){
    Reset();auto selected=Selection();auto id=Request(Transfer(selected.item,HIGGS_TRANSFER_INVENTORY_TO_HAND));fakeNoEvents=true;const auto calls=fakeDropCalls;Transactions().Drain(Now());CHECK(State(id).stage==HIGGS_TX_EXECUTING);fakeNoEvents=false;FakeEvent({0x14,0,100,1,fakeNextReference-1,0});Pump();CHECK(State(id).receiptFlags&HIGGS_TX_DOMAIN_ATTESTED);CHECK(fakeDropCalls==calls+1);for(int i=0;i<4;++i)Pump();CHECK(State(id).stage==HIGGS_TX_COMPLETED);
    Reset();selected=Selection();const auto oldEpoch=Service().Epoch();id=Request(Transfer(selected.item,HIGGS_TRANSFER_INVENTORY_TO_HAND));fakeResetDuringRemove=true;Transactions().Drain(Now());CHECK(Service().Epoch()!=oldEpoch);auto state=State(id);CHECK(state.stage==HIGGS_TX_RECOVERY_REQUIRED);CHECK(state.receiptFlags&HIGGS_TX_DOMAIN_ATTESTED);CHECK(state.receiptFlags&HIGGS_TX_MUTATION_UNCERTAIN);CHECK(state.actualRefId);fakeResetDuringRemove=false;
}
void EngineSelectionAndLock(){
    Reset();auto entry=FakeEntry(&fakePlayer,&sword);auto list=new BaseExtraList;auto uid=new ExtraUniqueID;uid->ownerFormId=0x14;uid->uniqueId=99;list->extras[kExtraData_UniqueID]=uid;entry->extendDataList->values.push_back(list);
    HIGGS_InventorySelectionRequest req{sizeof(req),101,Service().Epoch(),1,1,{0,0}};std::shared_ptr<InventorySelectionHandle> handle;uint32_t container=0;CHECK(!nativeInventory.Select(req,handle,container));
    // List storage changes without an identity change. Resolve follows the UID in
    // the fresh live entry, never the old BaseExtraList address.
    auto relocated=new BaseExtraList;relocated->extras=list->extras;entry->extendDataList->values[0]=relocated;delete list;
    CHECK(nativeInventory.Current(handle));HIGGS_TransferExecution x{};x.requestId=90000;x.phase=HIGGS_TX_EXECUTE;x.request.kind=HIGGS_TRANSFER_INVENTORY_TO_HAND;x.request.original.baseFormId=101;x.request.quantity=1;x.request.destinationHand=HIGGS_HAND_LEFT;Permit permit;auto result=nativeInventory.Execute(x,handle,permit);CHECK(!result.status&&result.preserved&&permit.begins==1);CHECK(Same(UID(&fakeRefs[result.refId]->extraData),{0x14,99}));nativeInventory.Forget(x.requestId);
    Reset();req.baseFormId=100;req.stackOrdinal=0;CHECK(!nativeInventory.Select(req,handle,container));fakeNoEffect=true;x.requestId++;x.request.original.baseFormId=100;Permit a,b;CHECK(nativeInventory.Execute(x,handle,a).status==HIGGS_HAND_PENDING);x.requestId++;CHECK(nativeInventory.Execute(x,handle,b).status==HIGGS_HAND_CONFLICT);CHECK(a.begins==1&&b.begins==0);fakeNoEffect=false;
}
void UIDRelocationAndAmbiguity(){
    // Independent review repro: two tickets for one actual UID instance must use
    // one private lock even when the engine moved its extra-list allocation.
    Reset();auto entry=FakeEntry(&fakePlayer,&sword);auto firstList=new BaseExtraList;auto uid=new ExtraUniqueID;uid->ownerFormId=0x14;uid->uniqueId=99;firstList->extras[kExtraData_UniqueID]=uid;entry->extendDataList->values.push_back(firstList);
    HIGGS_InventorySelectionRequest req{sizeof(req),101,Service().Epoch(),1,1,{0,0}};std::shared_ptr<InventorySelectionHandle> first,second;uint32_t container=0;CHECK(!nativeInventory.Select(req,first,container));
    auto relocated=new BaseExtraList;relocated->extras=firstList->extras;entry->extendDataList->values[0]=relocated;delete firstList;CHECK(!nativeInventory.Select(req,second,container));
    HIGGS_TransferExecution x{};x.requestId=910001;x.phase=HIGGS_TX_EXECUTE;x.request.kind=HIGGS_TRANSFER_INVENTORY_TO_HAND;x.request.original.baseFormId=101;x.request.quantity=1;x.request.destinationHand=HIGGS_HAND_LEFT;Permit one,two;fakeNoEffect=true;
    CHECK(nativeInventory.Execute(x,first,one).status==HIGGS_HAND_PENDING);++x.requestId;CHECK(nativeInventory.Execute(x,second,two).status==HIGGS_HAND_CONFLICT);CHECK(one.begins==1&&two.begins==0);
    nativeInventory.Reset();
    // A duplicated UID is ambiguous, not permission to choose one address or
    // silently merge two unrelated live inventory stacks.
    auto duplicate=new BaseExtraList;duplicate->extras=relocated->extras;entry->extendDataList->values.push_back(duplicate);
    CHECK(nativeInventory.Select(req,second,container)==HIGGS_HAND_CONFLICT);Permit ambiguous;++x.requestId;CHECK(nativeInventory.Execute(x,first,ambiguous).status==HIGGS_HAND_STALE);CHECK(ambiguous.begins==0);
    // Distinct valid UIDs remain independent native sources.
    auto different=new ExtraUniqueID;different->ownerFormId=0x14;different->uniqueId=100;duplicate->extras[kExtraData_UniqueID]=different;
    CHECK(!nativeInventory.Select(req,first,container));req.stackOrdinal=2;CHECK(!nativeInventory.Select(req,second,container));Permit a,b;++x.requestId;CHECK(nativeInventory.Execute(x,first,a).status==HIGGS_HAND_PENDING);++x.requestId;CHECK(nativeInventory.Execute(x,second,b).status==HIGGS_HAND_PENDING);CHECK(a.begins==1&&b.begins==1);fakeNoEffect=false;
}
void UnregisteredNativePins(){
    // Client liveness controls future authority. The transaction owner controls
    // already-issued mutation and recovery lifetimes independently of that client.
    const auto savedOwner=owner,savedExecutor=executor;
    HIGGS_HandClientInfo info{};info.structSize=sizeof(info);std::snprintf(info.name,sizeof(info.name),"retained-native-after-close");CHECK(!Service().Register(info,owner));
    Reset();CHECK(!inventoryApi.GetExecutor(owner,&executor));auto selected=Selection();auto id=Request(Transfer(selected.item,HIGGS_TRANSFER_INVENTORY_TO_HAND));fakeNoEvents=true;Transactions().Drain(Now());
    HIGGS_TransferExecution x{};x.requestId=id;CHECK(nativeInventory.Poll(x).status==HIGGS_HAND_PENDING);CHECK(Transactions().ItemInUse(selected.item.instance));CHECK(!Service().Unregister(owner));
    HIGGS_TransferState nativeState{};CHECK(Transactions().Copy(owner,id,nativeState)==HIGGS_HAND_INVALID_CLIENT);CHECK(Transactions().NativePinRequired(owner,id,Service().Epoch(),nativeState));
    DrainNativeInventory();CHECK(nativeInventory.Poll(x).status==HIGGS_HAND_PENDING);CHECK(Transactions().ItemInUse(selected.item.instance));
    Transactions().Drain(Now());CHECK(Transactions().NativePinRequired(owner,id,Service().Epoch(),nativeState));CHECK(nativeState.stage==HIGGS_TX_RECOVERY_REQUIRED);DrainNativeInventory();CHECK(nativeInventory.Poll(x).status==HIGGS_HAND_PENDING);
    CHECK(!Transactions().RetireInventory(owner,selected.ticket));
    Service().ObserveWorld(++worldSerial,false,Now());DrainNativeInventory();CHECK(nativeInventory.Poll(x).status==HIGGS_HAND_STALE);CHECK(!Transactions().NativePinRequired(owner,id,Service().Epoch(),nativeState));CHECK(!Transactions().ItemInUse(selected.item.instance));
    fakeNoEvents=false;owner=savedOwner;executor=savedExecutor;
}
void PendingUIDRemapLock(){
    // A fresh no-UID ticket must not evade a pending UID lock after the engine
    // relocates the list and explicitly reports that the persistent ID was lost.
    // A valid UID remap must retain that same lock as well.
    for(uint16_t replacement:{uint16_t{0},uint16_t{100}}){
        Reset();auto entry=FakeEntry(&fakePlayer,&sword);auto original=new BaseExtraList;auto uid=new ExtraUniqueID;uid->ownerFormId=0x14;uid->uniqueId=99;original->extras[kExtraData_UniqueID]=uid;entry->extendDataList->values.push_back(original);
        HIGGS_InventorySelectionRequest req{sizeof(req),101,Service().Epoch(),1,1,{0,0}};std::shared_ptr<InventorySelectionHandle> first,second;uint32_t container=0;CHECK(!nativeInventory.Select(req,first,container));
        HIGGS_TransferExecution x{};x.requestId=920001;x.phase=HIGGS_TX_EXECUTE;x.request.kind=HIGGS_TRANSFER_INVENTORY_TO_HAND;x.request.original.baseFormId=101;x.request.quantity=1;x.request.destinationHand=HIGGS_HAND_LEFT;Permit one,two;fakeNoEffect=true;
        CHECK(nativeInventory.Execute(x,first,one).status==HIGGS_HAND_PENDING);
        auto relocated=new BaseExtraList;relocated->extras=original->extras;
        if(replacement){auto remapped=new ExtraUniqueID;remapped->ownerFormId=0x14;remapped->uniqueId=replacement;relocated->extras[kExtraData_UniqueID]=remapped;}else relocated->extras.erase(kExtraData_UniqueID);
        entry->extendDataList->values[0]=relocated;delete original;fakeDispatchers.uniqueIdChangeDispatcher.Send({0x14,0x14,101,99,replacement});
        CHECK(!nativeInventory.Select(req,second,container));++x.requestId;CHECK(nativeInventory.Execute(x,second,two).status==HIGGS_HAND_CONFLICT);CHECK(one.begins==1&&two.begins==0);fakeNoEffect=false;
    }
}
void Consumption(){
    Reset();auto world=new TESObjectREFR;world->formID=7000;world->baseForm=&potion;world->extraData.count=1;fakeRefs[7000]=world;
    auto item=Held(world);auto id=Request(Transfer(item,HIGGS_TRANSFER_CONSUME_HELD));const auto equips=fakeEquipCalls;for(int i=0;i<6;++i)Pump();CHECK(State(id).stage==HIGGS_TX_COMPLETED);CHECK(State(id).disposition==HIGGS_DISPOSITION_CONSUMED);CHECK(fakeEquipCalls==equips+1);CHECK(FakeEntry(&fakePlayer,&potion)->countDelta==3);
    Reset();world=new TESObjectREFR;world->formID=7001;world->baseForm=&potion;world->extraData.extras[kExtraData_Enchantment]=nullptr;item=Held(world);id=Request(Transfer(item,HIGGS_TRANSFER_CONSUME_HELD));const auto pickups=fakePickUpCalls;for(int i=0;i<5;++i)Pump();CHECK(State(id).stage==HIGGS_TX_CANCELLED);CHECK(fakePickUpCalls==pickups);CHECK(!State(id).mutationPermit);
    Reset();world=new TESObjectREFR;world->formID=7002;world->baseForm=&potion;auto uid=new ExtraUniqueID;uid->ownerFormId=0x14;uid->uniqueId=123;world->extraData.extras[kExtraData_UniqueID]=uid;world->extraData.extras[kExtraData_Enchantment]=nullptr;item=Held(world);id=Request(Transfer(item,HIGGS_TRANSFER_CONSUME_HELD));for(int i=0;i<6;++i)Pump();CHECK(State(id).stage==HIGGS_TX_COMPLETED);CHECK(State(id).disposition==HIGGS_DISPOSITION_CONSUMED);
    Reset();world=new TESObjectREFR;world->formID=7003;world->baseForm=&potion;item=Held(world);auto reused=new TESObjectREFR;reused->formID=7003;reused->baseForm=&sword;fakeRefs[7003]=reused;id=Request(Transfer(item,HIGGS_TRANSFER_HAND_TO_INVENTORY));for(int i=0;i<6;++i)Pump();CHECK(State(id).stage==HIGGS_TX_COMPLETED);CHECK(world->flags&TESForm::kFlagIsDeleted);CHECK(!(reused->flags&TESForm::kFlagIsDeleted));
}
void EquipmentAndInvalidation(){
    Reset();auto selection=Selection(101);auto r=Transfer(selection.item,HIGGS_TRANSFER_EQUIP_GAME);r.destinationHand=HIGGS_HAND_RIGHT;r.expectedEquippedFormId=101;auto id=Request(r);Transactions().Drain(Now());CHECK(State(id).stage==HIGGS_TX_WAIT_EQUIPMENT);Transactions().Drain(Now());CHECK(State(id).stage==HIGGS_TX_WAIT_EQUIPMENT);
    observedHands[1].equippedFormId=101;Publish();Transactions().Drain(Now());CHECK(State(id).stage==HIGGS_TX_COMPLETED);CHECK(State(id).receiptFlags&HIGGS_TX_NATIVE_EQUIPMENT_CONFIRMED);
    auto worn=Selection(101,1);r=Transfer(worn.item,HIGGS_TRANSFER_UNEQUIP_GAME);r.destinationHand=HIGGS_HAND_RIGHT;r.expectedEquippedFormId=101;id=Request(r);Transactions().Drain(Now());CHECK(State(id).stage==HIGGS_TX_WAIT_EQUIPMENT);observedHands[1].equippedFormId=0;Publish();Transactions().Drain(Now());CHECK(State(id).stage==HIGGS_TX_COMPLETED);CHECK(State(id).disposition==HIGGS_DISPOSITION_INVENTORY);
    auto selected=Selection(100);FakeEvent({0,0x14,100,1,0,0});DrainNativeInventory();HIGGS_InventorySelection value{};value.structSize=sizeof(value);CHECK(!inventoryApi.CopySelection(owner,selected.ticket,&value));CHECK(value.state==HIGGS_SELECTION_INVALID);CHECK(value.reason==HIGGS_HAND_STALE);
    CHECK(!inventoryApi.ReleaseSelection(owner,selected.ticket));Pump();
    auto pending=Selection(100);Service().ObserveWorld(++worldSerial,false,Now());CHECK(inventoryApi.CopySelection(owner,pending.ticket,&value)==HIGGS_HAND_STALE);DrainNativeInventory();CHECK(inventoryApi.CopySelection(owner,pending.ticket,&value)==HIGGS_HAND_NOT_FOUND);
}
HIGGS_TransferExecution EquipmentExecution(uint32_t hand,uint32_t hands){
    HIGGS_TransferExecution x{};x.requestId=++idempotency;x.phase=HIGGS_TX_EXECUTE;
    x.request.kind=HIGGS_TRANSFER_EQUIP_GAME;x.request.original.baseFormId=101;x.request.quantity=1;
    x.request.destinationHand=hand;x.request.expectedEquippedFormId=101;x.current.physicalHands=hands;
    return x;
}
std::shared_ptr<InventorySelectionHandle> NativeWeaponSelection(uint32_t ordinal){
    HIGGS_InventorySelectionRequest req{sizeof(req),101,Service().Epoch(),ordinal,1,{0,0}};
    std::shared_ptr<InventorySelectionHandle> selected;uint32_t container=0;
    CHECK(!nativeInventory.Select(req,selected,container));return selected;
}
struct EquipmentChangePermit:Permit{
    BaseExtraList* list=nullptr;bool oppositeLeft=false;
    bool Begin()override{
        ++begins;if(list)list->extras[oppositeLeft?kExtraData_WornLeft:kExtraData_Worn]=nullptr;
        fakePlayer.equipped[oppositeLeft?0:1]=&sword;return true;
    }
};
void CrossHandEquipmentRefusal(){
    unsigned refusals=0;
    for(bool reversed:{false,true})for(uint32_t hand:{uint32_t(HIGGS_HAND_LEFT),uint32_t(HIGGS_HAND_RIGHT)})
        for(uint32_t hands:{hand,uint32_t(HIGGS_HAND_BOTH)}){
            Reset();fakeLeftHanded=reversed;const bool targetLeft=reversed!=(hand==HIGGS_HAND_LEFT);
            const bool oppositeLeft=!targetLeft;auto entry=FakeEntry(&fakePlayer,&sword);auto worn=new BaseExtraList;
            worn->extras[oppositeLeft?kExtraData_WornLeft:kExtraData_Worn]=nullptr;entry->extendDataList->values.push_back(worn);
            fakePlayer.equipped[oppositeLeft?0:1]=&sword;
            // Coherent drawn-equipment observation is empty while Skyrim keeps
            // the sheathed source equipped. Only fresh native state exposes it.
            CHECK(!Service().CopyPublished().hands[0].equippedFormId&&!Service().CopyPublished().hands[1].equippedFormId);
            auto selected=NativeWeaponSelection(1);auto x=EquipmentExecution(hand,hands);Permit permit;const auto calls=fakeEquipCalls;
            CHECK(nativeInventory.Execute(x,selected,permit).status==HIGGS_HAND_UNSUPPORTED);
            CHECK(!permit.begins&&fakeEquipCalls==calls&&Worn(worn,oppositeLeft));
            CHECK(fakePlayer.equipped[oppositeLeft?0:1]==&sword&&!fakePlayer.equipped[targetLeft?0:1]);++refusals;
            // Null selection stays ambiguous even if a second plain copy exists.
            selected=NativeWeaponSelection(0);x.requestId=++idempotency;Permit plain;
            CHECK(nativeInventory.Execute(x,selected,plain).status==HIGGS_HAND_UNSUPPORTED&&!plain.begins&&fakeEquipCalls==calls);++refusals;
        }
    // An engine equipment observation alone is also enough to reject ambiguous
    // plain selection, even before its worn-list metadata is available.
    Reset();fakePlayer.equipped[0]=&sword;auto selected=NativeWeaponSelection(0);auto x=EquipmentExecution(HIGGS_HAND_RIGHT,HIGGS_HAND_BOTH);Permit ambiguous;
    const auto calls=fakeEquipCalls;CHECK(nativeInventory.Execute(x,selected,ambiguous).status==HIGGS_HAND_UNSUPPORTED&&!ambiguous.begins&&fakeEquipCalls==calls);++refusals;
    // The public coordinator must not issue a permit or report an equip when a
    // sheathed source remains worn by the unreserved opposite hand.
    Reset();auto entry=FakeEntry(&fakePlayer,&sword);auto worn=new BaseExtraList;worn->extras[kExtraData_WornLeft]=nullptr;entry->extendDataList->values.push_back(worn);fakePlayer.equipped[0]=&sword;
    auto ticket=Selection(101,1);auto request=Transfer(ticket.item,HIGGS_TRANSFER_EQUIP_GAME);request.destinationHand=HIGGS_HAND_RIGHT;request.expectedEquippedFormId=101;
    const auto coordinatedCalls=fakeEquipCalls;auto id=Request(request);Transactions().Drain(Now());const auto state=State(id);
    CHECK(state.stage==HIGGS_TX_CANCELLED&&!state.mutationPermit&&!(state.receiptFlags&HIGGS_TX_NATIVE_EQUIPMENT_CONFIRMED));
    CHECK(fakeEquipCalls==coordinatedCalls&&fakePlayer.equipped[0]==&sword&&!fakePlayer.equipped[1]);
    // Preserve existing same-hand and distinct exact-copy behavior. The other
    // worn UID list does not turn a proven different list into a hand transfer.
    for(bool sameHand:{false,true}){
        Reset();entry=FakeEntry(&fakePlayer,&sword);auto exact=new BaseExtraList;auto uid=new ExtraUniqueID;uid->ownerFormId=0x14;uid->uniqueId=41;exact->extras[kExtraData_UniqueID]=uid;entry->extendDataList->values.push_back(exact);
        if(sameHand){exact->extras[kExtraData_Worn]=nullptr;fakePlayer.equipped[1]=&sword;}
        else{auto otherList=new BaseExtraList;auto otherId=new ExtraUniqueID;otherId->ownerFormId=0x14;otherId->uniqueId=42;otherList->extras[kExtraData_UniqueID]=otherId;otherList->extras[kExtraData_WornLeft]=nullptr;entry->extendDataList->values.push_back(otherList);fakePlayer.equipped[0]=&sword;}
        selected=NativeWeaponSelection(1);x=EquipmentExecution(HIGGS_HAND_RIGHT,HIGGS_HAND_RIGHT);Permit safe;const auto safeCalls=fakeEquipCalls;
        CHECK(!nativeInventory.Execute(x,selected,safe).status&&safe.begins==1&&fakeEquipCalls==safeCalls+1);CHECK(Worn(exact,false));nativeInventory.Forget(x.requestId);
    }
    // Pre-permit validation passes. Opposite wear appears while the permit is
    // issued; final Resolve still succeeds for the plain selection's same count.
    // The final guard must reject without calling EquipItem.
    for(bool reversed:{false,true}){
        Reset();fakeLeftHanded=reversed;entry=FakeEntry(&fakePlayer,&sword);auto otherList=new BaseExtraList;entry->extendDataList->values.push_back(otherList);
        selected=NativeWeaponSelection(0);x=EquipmentExecution(HIGGS_HAND_RIGHT,HIGGS_HAND_BOTH);EquipmentChangePermit late;late.list=otherList;late.oppositeLeft=!reversed;const auto lateCalls=fakeEquipCalls;
        CHECK(nativeInventory.Execute(x,selected,late).status==HIGGS_HAND_UNSUPPORTED&&late.begins==1&&fakeEquipCalls==lateCalls);++refusals;
        CHECK(Worn(otherList,late.oppositeLeft)&&fakePlayer.equipped[late.oppositeLeft?0:1]==&sword);
        // Changing the exact selected list instead invalidates its selection;
        // that fresh-resolution rejection likewise must make no engine call.
        Reset();fakeLeftHanded=reversed;entry=FakeEntry(&fakePlayer,&sword);auto exact=new BaseExtraList;entry->extendDataList->values.push_back(exact);
        selected=NativeWeaponSelection(1);x=EquipmentExecution(HIGGS_HAND_RIGHT,HIGGS_HAND_BOTH);EquipmentChangePermit stale;stale.list=exact;stale.oppositeLeft=!reversed;
        CHECK(nativeInventory.Execute(x,selected,stale).status==HIGGS_HAND_STALE&&stale.begins==1&&fakeEquipCalls==lateCalls);
    }
    std::printf("PASS cross-hand equipment refuses %u unsafe/ambiguous/late requests; safe same-hand/exact-copy equipment remains\n",refusals);
}
void ReuseAndMalformed(){
    Reset();for(int i=0;i<300;++i){auto selected=Selection();CHECK(!inventoryApi.ReleaseSelection(owner,selected.ticket));DrainNativeInventory();}
    HIGGS_InventorySelectionRequest r{sizeof(r),100,Service().Epoch(),0,1,{1,0}};uint64_t id=0;CHECK(inventoryApi.RequestSelection(owner,&r,&id)==HIGGS_HAND_INVALID_ARGUMENT);r.reserved[0]=0;r.quantity=0;CHECK(inventoryApi.RequestSelection(owner,&r,&id)==HIGGS_HAND_INVALID_ARGUMENT);r.quantity=1;r.epoch--;CHECK(inventoryApi.RequestSelection(owner,&r,&id)==HIGGS_HAND_STALE);
}
}
int main(){
    CHECK(!inventory_c_probe());CHECK(!HIGGS_QueryInventoryInterface(1,sizeof(inventoryApi),&inventoryApi));CHECK(inventoryApi.capabilities==0);
    auto& broker=HiggsHands::Service();broker.MarkMainThread();HIGGS_HandClientInfo info{};info.structSize=sizeof(info);std::snprintf(info.name,sizeof(info.name),"native-inventory-test");CHECK(!broker.Register(info,owner));std::snprintf(info.name,sizeof(info.name),"other");CHECK(!broker.Register(info,other));
    Reset();CHECK(!inventoryApi.GetExecutor(owner,&executor));CoreRoundTrip();MissingPostfacts();Compensation();DelayedFactsAndReset();EngineSelectionAndLock();UIDRelocationAndAmbiguity();PendingUIDRemapLock();Consumption();EquipmentAndInvalidation();CrossHandEquipmentRefusal();ReuseAndMalformed();UnregisteredNativePins();
    std::puts("PASS native selection, actual coordinator round trip, postfact gating, instance locks, consume, equipment, invalidation and retirement");return 0;
}
