#pragma once
// Explicit SDK doubles. These execute actual native-adapter source; they do not
// establish Skyrim's event delivery, vtable layouts or game behavior.
#include "synergy_inventory_native.h"
#include "hand_broker.h"
#include "hand_transactions.h"
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <map>
#include <vector>
#include <cstdint>
using UInt32=unsigned long;using UInt16=unsigned short;using SInt32=long;
enum {kFormType_Weapon=1,kFormType_Armor,kFormType_Ammo,kFormType_Book,kFormType_Potion,kFormType_Ingredient,kFormType_Misc,kFormType_Key,kFormType_SoulGem,kFormType_ScrollItem};
enum {kExtraData_ContainerChanges=1,kExtraData_UniqueID,kExtraData_Worn,kExtraData_WornLeft,kExtraData_Enchantment,kExtraData_Health,kExtraData_Charge,kExtraData_Poison,kExtraData_Ownership,kExtraData_TextDisplayData,kExtraData_Soul};
struct TESForm {virtual ~TESForm()=default;enum{kFlagIsDeleted=32};UInt32 formID=0,flags=0;int formType=0;bool playable=true;bool IsPlayable(){return playable;}};
struct TESBoundObject:TESForm{};
struct TESObjectWEAP:TESBoundObject{bool twoHanded=false,bow=false;};
struct BGSBipedObjectForm{enum{kPart_Shield=512};UInt32 mask=0;UInt32 GetSlotMask(){return mask;}};
struct TESObjectARMO:TESBoundObject{BGSBipedObjectForm bipedObject;};
struct AlchemyItem:TESBoundObject{bool poison=false;bool IsPoison(){return poison;}};
struct TESContainer{virtual ~TESContainer()=default;std::map<TESForm*,UInt32> counts;UInt32 CountItem(TESForm* f){return counts[f];}};
struct ContainerBase:TESForm,TESContainer{};
struct BSExtraData{virtual ~BSExtraData()=default;};
struct ExtraUniqueID:BSExtraData{UInt32 ownerFormId=0;UInt16 uniqueId=0;};
struct BaseExtraList{UInt32 count=1;std::map<int,BSExtraData*> extras;BSExtraData* GetByType(int i){auto it=extras.find(i);return it==extras.end()?nullptr:it->second;}bool HasType(int i){return extras.count(i)!=0;}};
template<class T>struct List{std::vector<T*> values;struct Iterator{List* list;size_t i=0;bool End(){return i>=list->values.size();}Iterator& operator++(){++i;return *this;}T* Get(){return list->values[i];}};Iterator Begin(){return {this};}};
struct InventoryEntryData{TESForm* type=nullptr;List<BaseExtraList>* extendDataList=nullptr;SInt32 countDelta=0;};
struct ExtraContainerChanges:BSExtraData{struct Data{std::vector<InventoryEntryData*> entries;InventoryEntryData* FindItemEntry(TESForm* f){for(auto e:entries)if(e->type==f)return e;return nullptr;}};Data* data=nullptr;};
struct NiPoint3{float x=0,y=0,z=0;NiPoint3()=default;NiPoint3(float a,float b,float c):x(a),y(b),z(c){}};
struct TESObjectREFR:TESForm{TESForm* baseForm=nullptr;BaseExtraList extraData;NiPoint3 pos;};
struct Actor:TESObjectREFR{TESForm* equipped[2]{};TESForm* GetEquippedObject(bool left){return equipped[left?0:1];}};
using PlayerCharacter=Actor;
template<class T>struct NiPointer{T* p=nullptr;NiPointer()=default;NiPointer(T* v):p(v){};NiPointer& operator=(T* v){p=v;return *this;}operator T*()const{return p;}T* operator->()const{return p;}explicit operator bool()const{return p!=nullptr;}};
template<class T>bool operator==(NiPointer<T> a,NiPointer<T> b){return a.p==b.p;}
template<class T>bool operator!=(NiPointer<T> a,T* b){return a.p!=b;}
#define DYNAMIC_CAST(obj,from,to) dynamic_cast<to*>(obj)
#define CALL_MEMBER_FN(obj,method) ((obj)->method)
struct TESContainerChangedEvent{UInt32 fromFormId=0,toFormId=0,itemFormId=0,count=0,toReference=0;UInt16 unk14=0;};
struct TESUniqueIDChangeEvent{UInt32 oldOwnerFormId=0,newOwnerFormId=0,formId=0;UInt16 oldUniqueId=0,newUniqueId=0;};
enum EventResult{kEvent_Continue};
template<class T>struct EventDispatcher;
template<class T>struct BSTEventSink{virtual ~BSTEventSink()=default;virtual EventResult ReceiveEvent(T*,EventDispatcher<T>*)=0;};
template<class T>struct EventDispatcher{std::vector<BSTEventSink<T>*> sinks;void AddEventSink(BSTEventSink<T>* s){sinks.push_back(s);}void Send(T e){for(auto s:sinks)s->ReceiveEvent(&e,this);}};
struct EventDispatcherList{EventDispatcher<TESContainerChangedEvent> unk370;EventDispatcher<TESUniqueIDChangeEvent> uniqueIdChangeDispatcher;};
inline EventDispatcherList fakeDispatchers;inline EventDispatcherList* GetEventDispatcherList(){return &fakeDispatchers;}
inline PlayerCharacter fakePlayer;inline PlayerCharacter* fakePlayerPointer=&fakePlayer;inline PlayerCharacter** g_thePlayer=&fakePlayerPointer;
inline bool fakeLeftHanded=false;inline bool* g_leftHandedMode=&fakeLeftHanded;
inline UInt32 invalidHandle=0xffffffff;inline UInt32* g_invalidRefHandle=&invalidHandle;
inline std::map<UInt32,TESForm*> fakeForms;inline std::map<UInt32,TESObjectREFR*> fakeRefs;
inline TESForm* LookupFormByID(UInt32 id){auto it=fakeForms.find(id);return it==fakeForms.end()?nullptr:it->second;}
inline bool LookupREFRByHandle(UInt32& handle,NiPointer<TESObjectREFR>& out){auto it=fakeRefs.find(handle);if(it==fakeRefs.end())return false;out=it->second;return true;}
inline UInt32 BSExtraList_GetCount(BaseExtraList* list){return list->count;}
inline bool IsTwoHanded(TESObjectWEAP* w){return w->twoHanded;}
inline bool IsBow(TESObjectWEAP* w){return w->bow;}
struct BGSEquipSlot{bool left=false;};inline BGSEquipSlot leftSlot{true},rightSlot{false};inline BGSEquipSlot* GetLeftHandSlot(){return &leftSlot;}inline BGSEquipSlot* GetRightHandSlot(){return &rightSlot;}
inline bool fakeNoEffect=false,fakeNoEvents=false,fakeDelayEquip=false,fakeDropWrongUID=false,fakeOverflowAfterPickup=false,fakeResetDuringRemove=false;
inline int fakeDropCalls=0,fakePickUpCalls=0,fakeEquipCalls=0;
inline UInt32 fakeNextReference=10000;
inline void FakeEvent(TESContainerChangedEvent event){if(!fakeNoEvents)fakeDispatchers.unk370.Send(event);}
inline InventoryEntryData* FakeEntry(Actor* player,TESForm* form){auto changes=static_cast<ExtraContainerChanges*>(player->extraData.GetByType(kExtraData_ContainerChanges));auto entry=changes->data->FindItemEntry(form);if(!entry){entry=new InventoryEntryData;entry->type=form;entry->extendDataList=new List<BaseExtraList>;changes->data->entries.push_back(entry);}return entry;}
inline void FakeRemove(TESObjectREFR* ref,UInt32* handle,TESBoundObject* base,SInt32 count,UInt32,BaseExtraList* extra,TESObjectREFR*,const NiPoint3*,const NiPoint3*){
    ++fakeDropCalls;if(fakeNoEffect)return;auto actor=static_cast<Actor*>(ref);auto entry=FakeEntry(actor,base);entry->countDelta-=count;
    auto world=new TESObjectREFR;world->formID=fakeNextReference++;world->baseForm=base;world->extraData.count=static_cast<UInt32>(count);
    if(extra){world->extraData.extras=extra->extras;extra->count-=static_cast<UInt32>(count);if(!extra->count){auto& values=entry->extendDataList->values;values.erase(std::remove(values.begin(),values.end(),extra),values.end());}}
    if(fakeDropWrongUID){auto id=new ExtraUniqueID;id->ownerFormId=999;id->uniqueId=999;world->extraData.extras[kExtraData_UniqueID]=id;}
    fakeRefs[world->formID]=world;*handle=world->formID;FakeEvent({actor->formID,0,base->formID,static_cast<UInt32>(count),world->formID,0});
    if(fakeResetDuringRemove)HiggsHands::Service().ObserveWorld(0xabc666,false,HiggsHands::Now());
}
inline void FakePickup(Actor* actor,TESObjectREFR* world,SInt32 count,bool,bool){
    ++fakePickUpCalls;if(fakeNoEffect)return;auto entry=FakeEntry(actor,world->baseForm);entry->countDelta+=count;
    if(world->extraData.GetByType(kExtraData_UniqueID)||world->extraData.HasType(kExtraData_Health)||world->extraData.HasType(kExtraData_Enchantment)){auto list=new BaseExtraList;list->count=static_cast<UInt32>(count);list->extras=world->extraData.extras;entry->extendDataList->values.push_back(list);}
    world->flags|=TESForm::kFlagIsDeleted;FakeEvent({0,actor->formID,world->baseForm->formID,static_cast<UInt32>(count),world->formID,0});
    if(fakeOverflowAfterPickup)for(int i=0;i<130;++i)FakeEvent({0,actor->formID,999,1,0,0});
}
using Actor_RemoveItem=void(*)(TESObjectREFR*,UInt32*,TESBoundObject*,SInt32,UInt32,BaseExtraList*,TESObjectREFR*,const NiPoint3*,const NiPoint3*);
using Actor_PickUpObject=void(*)(Actor*,TESObjectREFR*,SInt32,bool,bool);
template<class T>T get_vfunc(void*,int index){if(index==0x56)return reinterpret_cast<T>(&FakeRemove);return reinterpret_cast<T>(&FakePickup);}
struct EquipManager{
    static EquipManager* GetSingleton(){static EquipManager manager;return &manager;}
    void EquipItem(Actor* actor,TESForm* base,BaseExtraList* list,SInt32 count,BGSEquipSlot* slot,bool,bool,bool,void*){
        ++fakeEquipCalls;if(fakeNoEffect||fakeDelayEquip)return;auto entry=FakeEntry(actor,base);
        if(base->formType==kFormType_Potion||base->formType==kFormType_Ingredient){entry->countDelta-=count;if(list){list->count-=static_cast<UInt32>(count);if(!list->count){auto& values=entry->extendDataList->values;values.erase(std::remove(values.begin(),values.end(),list),values.end());}}FakeEvent({actor->formID,0,base->formID,static_cast<UInt32>(count),0,0});return;}
        if(!list){list=new BaseExtraList;entry->extendDataList->values.push_back(list);}list->extras[slot&&slot->left?kExtraData_WornLeft:kExtraData_Worn]=nullptr;actor->equipped[slot&&slot->left?0:1]=base;
    }
    bool UnequipItem(Actor* actor,TESForm* base,BaseExtraList* list,SInt32,BGSEquipSlot* slot,bool,bool,bool,bool,void*){
        ++fakeEquipCalls;if(fakeNoEffect)return false;if(list)list->extras.erase(slot&&slot->left?kExtraData_WornLeft:kExtraData_Worn);if(actor->equipped[slot&&slot->left?0:1]==base)actor->equipped[slot&&slot->left?0:1]=nullptr;return false;
    }
};
namespace HiggsHands{inline std::map<uint64_t,TESObjectREFR*> retainedItems;inline bool CopyNativeItemReference(const HIGGS_ItemIdentity& id,NiPointer<TESObjectREFR>& out){auto it=retainedItems.find(id.instance);if(it==retainedItems.end()||!Transactions().IdentityCurrent(id))return false;out=it->second;return true;}}
#define CHECK(x) do{if(!(x)){std::fprintf(stderr,"FAIL %d %s\n",__LINE__,#x);std::abort();}}while(0)
