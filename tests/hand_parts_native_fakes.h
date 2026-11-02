#pragma once
#include "hand_parts.h"
#include "hand_parts_native.h"
#include "higgshandpartsnative.h"
#include "hand_broker.h"
#include "hand_observations.h"
#include <array>
#include <atomic>
#include <cmath>
#include <cstring>
#include <iostream>
#include <map>
#include <memory>
#include <mutex>
#include <new>
#include <stdexcept>
#include <string>
#include <thread>
#include <unordered_set>
#include <vector>
struct FakeRefCounted {int m_uiRefCount=0;virtual ~FakeRefCounted()=default;};
template<class T> struct NiPointer {T* p=nullptr;NiPointer()=default;NiPointer(std::nullptr_t){}NiPointer(T* v):p(v){if(p)++p->m_uiRefCount;}NiPointer(const NiPointer& v):NiPointer(v.p){}NiPointer(NiPointer&& v)noexcept:p(v.p){v.p=nullptr;}~NiPointer(){if(p&&!--p->m_uiRefCount)delete p;}NiPointer& operator=(NiPointer v)noexcept{std::swap(p,v.p);return *this;}T* operator->()const{return p;}operator T*()const{return p;}T* val()const{return p;}};
template<class T>struct FakeHkRef {std::shared_ptr<T> p;FakeHkRef()=default;FakeHkRef(T* v):p(v){}FakeHkRef& operator=(T* v){p.reset(v);return *this;}T* operator->()const{return p.get();}operator T*()const{return p.get();}T* val()const{return p.get();}};
struct NiPoint3{float x=0,y=0,z=0;};
struct NiMatrix33 {float data[3][3]{{1,0,0},{0,1,0},{0,0,1}};};
struct NiTransform {NiPoint3 pos;NiMatrix33 rot;float scale=1;};
struct BSFixedString {const char* data=nullptr;};
class NiAVObject:public FakeRefCounted {public:NiTransform m_worldTransform;std::map<std::string,NiPointer<NiAVObject>> children;NiAVObject* GetObjectByName(const char* const* name){if(!name||!*name)return nullptr;auto i=children.find(*name);return i==children.end()?nullptr:i->second.val();}};
struct hkVector4{float v[4]{};void set(float x,float y,float z,float w){v[0]=x;v[1]=y;v[2]=z;v[3]=w;}float operator()(unsigned i)const{return v[i];}};
struct hkRotation{float v[3][3]{{1,0,0},{0,1,0},{0,0,1}};hkVector4 getColumn(unsigned i)const{hkVector4 out;out.set(v[0][i],v[1][i],v[2][i],0);return out;}};
struct hkQuaternion{hkRotation rotation;void setFromRotationSimd(const hkRotation& r){rotation=r;}};
struct hkTransform {hkVector4 p;hkRotation r;const hkVector4& getTranslation()const{return p;}const hkRotation& getRotation()const{return r;}};
inline void NiMatrixToHkMatrix(const NiMatrix33& in,hkRotation& out){std::memcpy(out.v,in.data,sizeof(out.v));}
struct FakeWorldLock{unsigned readers=0,writers=0;};
struct BSReadLocker{FakeWorldLock* lock;explicit BSReadLocker(FakeWorldLock* l):lock(l){++l->readers;}~BSReadLocker(){--lock->readers;}};
struct BSWriteLocker{FakeWorldLock* lock;explicit BSWriteLocker(FakeWorldLock* l):lock(l){if(l->writers||l->readers)throw std::runtime_error("recursive/inverted native world lock");++l->writers;}~BSWriteLocker(){--lock->writers;}};
struct hkpWorld;
struct hkpBoxShape{float m_radius=1;hkVector4 half;uintptr_t m_userData=0,wrapperKey=0;uintptr_t getUserData()const{return m_userData;}void setUserData(uintptr_t value){m_userData=value;}};
struct FakeCollidable{struct {uint32_t m_collisionFilterInfo=0;}m_broadPhaseHandle;hkpBoxShape* m_shape=nullptr;const hkpBoxShape* getShape()const{return m_shape;}};
class hkpRigidBody {public:uint32_t m_uid=0;uintptr_t m_userData=0;hkpWorld* world=nullptr;bool addQueued=false,removeQueued=false;hkTransform transform;FakeHkRef<hkpBoxShape> shape;FakeCollidable m_collidable;uint32_t getCollisionFilterInfo()const{return m_collidable.m_broadPhaseHandle.m_collisionFilterInfo;}hkpWorld* getWorld()const{return world;}const hkTransform& getTransform()const{return transform;}const hkVector4& getLinearVelocity()const{static const hkVector4 zero;return zero;}const FakeCollidable* getCollidable()const{return &m_collidable;}};
struct hkpWorld{std::vector<hkpRigidBody*> bodies,pendingAdds,pendingRemoves;int critical=0;int areCriticalOperationsLocked()const{return critical;}};
inline unsigned destroyedLockedWorlds=0;
struct bhkWorld:FakeRefCounted{FakeHkRef<hkpWorld> world;FakeWorldLock worldLock;~bhkWorld(){if(worldLock.writers||worldLock.readers)++destroyedLockedWorlds;}};
inline std::unordered_set<const void*> liveShapeWrappers;
inline unsigned danglingShapeBackPointers=0,danglingBodyBackPointers=0,shapeDestroyedBeforeBody=0;
struct bhkBoxShape:FakeRefCounted{FakeHkRef<hkpBoxShape> shape;uint32_t materialId=0;~bhkBoxShape(){if(shape&&shape.p.use_count()>1&&shape->m_userData==reinterpret_cast<uintptr_t>(this))++danglingShapeBackPointers;liveShapeWrappers.erase(this);}};
inline unsigned freedWhileNativeOwned=0;
inline std::unordered_set<const void*> liveNativeWrappers;
struct bhkRigidBody:FakeRefCounted{FakeHkRef<hkpRigidBody> hkBody;bool pluginBody=false;~bhkRigidBody(){if(pluginBody&&hkBody){if(hkBody->world||hkBody->addQueued)++freedWhileNativeOwned;if(hkBody.p.use_count()>1&&hkBody->m_userData==reinterpret_cast<uintptr_t>(this))++danglingBodyBackPointers;if(hkBody->shape&&!liveShapeWrappers.contains(reinterpret_cast<void*>(hkBody->shape->wrapperKey)))++shapeDestroyedBeforeBody;}liveNativeWrappers.erase(this);}};
struct hkpMotion{enum {MOTION_KEYFRAMED=1};};
struct hkpRigidBodyCinfo{enum {SOLVER_DEACTIVATION_OFF=0};uint32_t m_collisionFilterInfo=0;hkpBoxShape* m_shape=nullptr;unsigned m_motionType=0,m_solverDeactivation=1,m_qualityType=0;bool m_enableDeactivation=true;float m_maxAngularVelocity=0;hkVector4 m_position;hkQuaternion m_rotation;};
struct bhkRigidBodyCinfo{uint32_t collisionFilterInfo=0;FakeHkRef<hkpBoxShape> shape;hkpRigidBodyCinfo hkCinfo;};
enum {HK_COLLIDABLE_QUALITY_KEYFRAMED=2,HK_ENTITY_ACTIVATION_DO_ACTIVATE=1,HK_UPDATE_FILTER_ON_ENTITY_FULL_CHECK=1,HK_UPDATE_COLLECTION_FILTER_PROCESS_SHAPE_COLLECTIONS=1};
using hkBool=bool;
inline unsigned createdBodies=0,removedBodies=0,keyframes=0,filterUpdates=0;
inline unsigned addCalls=0,removeCalls=0;
inline bool deferNextAdd=false,deferNextRemove=false;
inline uint32_t nextUid=100;
inline bool failAllocation=false;
inline void* Heap_Allocate(size_t size){if(failAllocation)return nullptr;return ::operator new(size);}
inline void bhkBoxShape_ctor(bhkBoxShape* p,const hkVector4* h){new(p)bhkBoxShape;liveShapeWrappers.insert(p);p->shape=new hkpBoxShape;p->shape->half=*h;p->shape->m_userData=p->shape->wrapperKey=reinterpret_cast<uintptr_t>(p);}
inline void bhkRigidBodyCinfo_ctor(bhkRigidBodyCinfo* p){new(p)bhkRigidBodyCinfo;}
inline void bhkRigidBody_ctor(bhkRigidBody* p,bhkRigidBodyCinfo* info){new(p)bhkRigidBody;p->pluginBody=true;liveNativeWrappers.insert(p);p->hkBody=new hkpRigidBody;p->hkBody->m_userData=reinterpret_cast<uintptr_t>(p);p->hkBody->shape=info->shape;p->hkBody->m_collidable.m_shape=info->shape;p->hkBody->m_uid=nextUid++;p->hkBody->m_collidable.m_broadPhaseHandle.m_collisionFilterInfo=info->collisionFilterInfo;p->hkBody->transform.p=info->hkCinfo.m_position;p->hkBody->transform.r=info->hkCinfo.m_rotation.rotation;++createdBodies;}
inline void bhkRigidBody_setActivated(bhkRigidBody*,bool){}
inline void hkpWorld_AddEntity(hkpWorld* w,hkpRigidBody* b,int){++addCalls;if(w->critical||deferNextAdd){deferNextAdd=false;b->addQueued=true;w->pendingAdds.push_back(b);return;}b->world=w;w->bodies.push_back(b);}
inline void hkpWorld_RemoveEntity(hkpWorld* w,hkBool* removed,hkpRigidBody* b){++removeCalls;if(b->world!=w)throw std::runtime_error("removed from wrong world");if(w->critical||deferNextRemove){deferNextRemove=false;if(b->removeQueued)throw std::runtime_error("duplicate queued removal");b->removeQueued=true;w->pendingRemoves.push_back(b);*removed=false;return;}b->world=nullptr;std::erase(w->bodies,b);*removed=true;++removedBodies;}
inline void FlushNativeOperations(hkpWorld* w){if(w->critical)throw std::runtime_error("flush while critical");for(auto* b:w->pendingAdds){b->addQueued=false;b->world=w;w->bodies.push_back(b);}w->pendingAdds.clear();for(auto* b:w->pendingRemoves){b->removeQueued=false;b->world=nullptr;std::erase(w->bodies,b);++removedBodies;}w->pendingRemoves.clear();}
inline void hkpWorld_UpdateCollisionFilterOnEntity(hkpWorld*,hkpRigidBody*,int,int){++filterUpdates;}
inline void ApplyHardKeyframeVelocityClamped(const hkVector4&,const hkQuaternion&,float inv,bhkRigidBody*){if(!std::isfinite(inv)||inv<=0)throw std::runtime_error("bad native inverse dt");++keyframes;}
struct Hand {NiPointer<NiAVObject> node;NiPointer<bhkRigidBody> handBody,weaponBody;struct {NiPointer<bhkRigidBody> rigidBody;}selectedObject;BSFixedString fingerNodeNames[5][3];NiPointer<NiAVObject> GetFirstPersonHandNode(){return node;}bool HasHeldObject()const{return false;}};
struct FakeCell {NiPointer<bhkWorld> world;};
struct PlayerCharacter {FakeCell* parentCell=nullptr;};
inline PlayerCharacter fixturePlayer;inline PlayerCharacter* playerPointer=&fixturePlayer;inline PlayerCharacter** g_thePlayer=&playerPointer;
inline Hand left,right;inline Hand* g_leftHand=&left;inline Hand* g_rightHand=&right;
inline float worldScale=.0142875f,inverseWorldScale=1.f/worldScale;inline float* g_havokWorldScale=&worldScale;inline float* g_inverseHavokWorldScale=&inverseWorldScale;
inline bhkWorld* GetHavokWorldFromCell(FakeCell* c){return c?c->world.val():nullptr;}
inline std::vector<NiPointer<bhkRigidBody>> registered;inline bool throwRegister=false;
inline void RegisterPlayerSpaceBody(bhkRigidBody* b){if(throwRegister)throw std::runtime_error("synthetic registration failure");registered.emplace_back(b);}
namespace HiggsMotion {
struct CopiedTracking {struct Hand{bool available=true;uint64_t lastAcceptedObservation=1,observedAtNs=0;};std::array<Hand,2> hands;uint64_t generation=1;bool worldObserved=true,loading=false,exhausted=false;};
struct FakeTracking {CopiedTracking value;CopiedTracking CopyObservation(){auto copy=value;for(auto& h:copy.hands)if(!h.observedAtNs)h.observedAtNs=HiggsHands::Now();return copy;}};
inline FakeTracking& Tracking(){static FakeTracking t;return t;}
}
struct FakeForm{uint32_t formType=0;};
struct FakeReference{uint32_t formID=0;FakeForm* baseForm=nullptr;};
inline FakeReference* GetRefFromCollidable(const void*){return nullptr;}
struct FakePoint {hkVector4 position,normal;const hkVector4& getPosition()const{return position;}const hkVector4& getNormal()const{return normal;}};
struct hkContactPointMaterial{enum FlagEnum{CONTACT_IS_DISABLED=1};};
struct FakeContactProperties {unsigned m_flags=0;bool used=true,potential=false;bool wasUsed()const{return used;}bool isPotential()const{return potential;}};
class hkpContactPointEvent{public:FakePoint* m_contactPoint=nullptr;FakeContactProperties* m_contactPointProperties=nullptr;hkpRigidBody* m_bodies[2]{};};
