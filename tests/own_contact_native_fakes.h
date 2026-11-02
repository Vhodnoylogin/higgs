#pragma once
// Native source seams only; these are not SKSE/Havok layouts or game behavior.
#include "own_contact.h"
#include "hand_broker.h"
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <map>
#include <optional>
#include <vector>
using UInt32=unsigned long;using UInt16=unsigned short;
struct Ref {mutable int refs=0;};
extern int worldReads;
inline void(*afterRetainedAssign)()=nullptr;
inline int afterRetainedAssignDepth=0;
template<class T>struct NiPointer {
    T* p=nullptr;NiPointer()=default;NiPointer(T* v):p(v){if(p)++p->refs;}NiPointer(const NiPointer& v):NiPointer(v.p){}
    ~NiPointer(){if(p)--p->refs;}NiPointer& operator=(T* v){if(v)++v->refs;if(p)--p->refs;p=v;if(worldReads==afterRetainedAssignDepth&&afterRetainedAssign){auto call=afterRetainedAssign;afterRetainedAssign=nullptr;call();}return *this;}NiPointer& operator=(const NiPointer& v){return *this=v.p;}
    operator T*()const{return p;}T* operator->()const{return p;}T* val()const{return p;}explicit operator bool()const{return p!=nullptr;}
};
template<class T>bool operator==(const NiPointer<T>& a,const NiPointer<T>& b){return a.p==b.p;}
namespace RE{template<class T>using hkRefPtr=NiPointer<T>;}
enum{kFormType_Weapon=1,kFormType_Armor,kFormType_Ammo,kFormType_Book,kFormType_Potion,kFormType_Ingredient,kFormType_Misc,kFormType_Key,kFormType_SoulGem,kFormType_ScrollItem};
enum{kExtraData_ContainerChanges=1,kExtraData_UniqueID,kExtraData_Worn,kExtraData_WornLeft};
struct TESForm:Ref{virtual ~TESForm()=default;UInt32 formID=0;int formType=0;bool playable=true;bool IsPlayable(){return playable;}};
struct TESObjectWEAP:TESForm{struct GameData{enum{kType_HandToHandMelee=0};int type=1;}gameData;bool twoHanded=false;};
struct BGSBipedObjectForm{enum{kPart_Shield=512};UInt32 mask=0;UInt32 GetSlotMask(){return mask;}};
struct TESObjectARMO:TESForm{BGSBipedObjectForm bipedObject;};
struct BSExtraData{virtual ~BSExtraData()=default;};
struct ExtraUniqueID:BSExtraData{UInt32 ownerFormId=0;UInt16 uniqueId=0;};
struct BaseExtraList{UInt32 count=1;std::map<int,BSExtraData*> extras;bool HasType(int i){return extras.count(i)!=0;}BSExtraData* GetByType(int i){auto it=extras.find(i);return it==extras.end()?nullptr:it->second;}};
template<class T>struct List{std::vector<T*> values;struct Iterator{List* list;size_t i=0;bool End(){return i>=list->values.size();}Iterator& operator++(){++i;return *this;}T* Get(){return list->values[i];}};Iterator Begin(){return {this};}};
struct InventoryEntryData{TESForm* type=nullptr;List<BaseExtraList>* extendDataList=nullptr;};
struct ExtraContainerChanges:BSExtraData{struct Data{std::vector<InventoryEntryData*> entries;InventoryEntryData* FindItemEntry(TESForm* f){for(auto e:entries)if(e->type==f)return e;return nullptr;}};Data* data=nullptr;};
inline UInt32 BSExtraList_GetCount(BaseExtraList* list){return list->count;}
inline int engineReads=0,worldReads=0;
struct hkpWorld:Ref{};
struct hkpShape:Ref{uintptr_t user=0;uintptr_t getUserData()const{++engineReads;return user;}};
struct bhkShape:Ref{RE::hkRefPtr<hkpShape> shape;};
struct hkpRigidBody:Ref{
    struct Collidable{hkpShape* shape=nullptr;const hkpShape* getShape()const{++engineReads;return shape;}}collidable;
    hkpWorld* world=nullptr;uint32_t filter=56;
    hkpWorld* getWorld()const{++engineReads;return world;}uint32_t getCollisionFilterInfo()const{++engineReads;return filter;}const Collidable* getCollidable()const{return &collidable;}
};
struct bhkRigidBody:Ref{RE::hkRefPtr<hkpRigidBody> hkBody;};
struct bhkWorld:Ref{RE::hkRefPtr<hkpWorld> world;int worldLock=0;};
struct BSReadLocker{explicit BSReadLocker(int*){++worldReads;}~BSReadLocker(){--worldReads;}};
struct NiAVObject:Ref{virtual ~NiAVObject()=default;};
struct NiNode:NiAVObject{bhkRigidBody* collision=nullptr;};
struct TESObjectREFR:TESForm{TESForm* baseForm=nullptr;BaseExtraList extraData;NiNode* model=nullptr;bhkWorld* parentCell=nullptr;NiNode* GetNiNode(){return model;}};
struct Actor:TESObjectREFR{struct State{bool drawn=true;bool IsWeaponDrawn(){return drawn;}}actorState;TESForm* equipped[2]{};TESForm* GetEquippedObject(bool left){return equipped[left?0:1];}};
using PlayerCharacter=Actor;
#define DYNAMIC_CAST(obj,from,to) dynamic_cast<to*>(obj)
inline PlayerCharacter fakePlayer;inline PlayerCharacter* fakePlayerPointer=&fakePlayer;inline PlayerCharacter** g_thePlayer=&fakePlayerPointer;
inline bool fakeLeftHanded=false;inline bool* g_leftHandedMode=&fakeLeftHanded;
inline std::map<UInt32,TESObjectREFR*> references;
inline bool LookupREFRByHandle(UInt32& handle,NiPointer<TESObjectREFR>& out){auto it=references.find(handle);if(it==references.end())return false;out=it->second;return true;}
inline NiPointer<bhkWorld> GetHavokWorldFromCell(bhkWorld* cell){return cell;}
inline bool IsTwoHanded(TESObjectWEAP* form){return form->twoHanded;}
struct VRMeleeData{NiPointer<NiNode> collisionNode;};inline VRMeleeData melee[2];inline VRMeleeData* GetVRMeleeData(bool left){return &melee[left?0:1];}
inline NiPointer<bhkRigidBody> GetRigidBody(NiNode* node){return node?node->collision:nullptr;}
struct Hand{
    struct ShapeIdentifier{const void* worldObjectWrapper;const void* worldObject;const void* shapeWrapper;const void* shape;bool operator==(const ShapeIdentifier& b)const{return worldObjectWrapper==b.worldObjectWrapper&&worldObject==b.worldObject&&shapeWrapper==b.shapeWrapper&&shape==b.shape;}};
    struct Selected{UInt32 handle=0;NiPointer<bhkRigidBody> rigidBody;}selectedObject;
    NiPointer<bhkRigidBody> weaponBody;std::optional<ShapeIdentifier> clonedFromWeaponShape;
    uint64_t synergyContactHoldEpisode=1;bool held=false,twoHand=false;
    bool HasHeldObject(){return held;}bool IsTwoHanding(){return twoHand;}
};
inline Hand leftHand,rightHand;inline Hand* g_leftHand=&leftHand;inline Hand* g_rightHand=&rightHand;
namespace MenuChecker{inline bool stopped=false;inline bool isGameStopped(){return stopped;}}
enum EventResult{kEvent_Continue};template<class T>struct EventDispatcher;template<class T>struct BSTEventSink{virtual ~BSTEventSink()=default;virtual EventResult ReceiveEvent(T*,EventDispatcher<T>*)=0;};
template<class T>struct EventDispatcher{std::vector<BSTEventSink<T>*> sinks;void AddEventSink(BSTEventSink<T>* sink){sinks.push_back(sink);}void Send(T event){for(auto s:sinks)s->ReceiveEvent(&event,this);}};
struct TESContainerChangedEvent{UInt32 fromFormId=0,toFormId=0,itemFormId=0,count=0,toReference=0;};
struct TESUniqueIDChangeEvent{UInt32 oldOwnerFormId=0,newOwnerFormId=0,formId=0;UInt16 oldUniqueId=0,newUniqueId=0;};
struct HiggsOwnEquipEvent{NiPointer<TESObjectREFR> actor;uint32_t baseObject=0,originalReference=0;uint16_t uniqueID=0;bool equipped=false;uint8_t pad13=0;uint32_t pad14=0;};
struct EventDispatcherList{EventDispatcher<TESContainerChangedEvent> unk370;EventDispatcher<TESUniqueIDChangeEvent> uniqueIdChangeDispatcher;EventDispatcher<HiggsOwnEquipEvent> unk4D0;};
inline EventDispatcherList eventDispatchers;inline EventDispatcherList* GetEventDispatcherList(){return &eventDispatchers;}
#define CHECK(x) do{if(!(x)){std::fprintf(stderr,"FAIL %d %s\n",__LINE__,#x);std::abort();}}while(0)
