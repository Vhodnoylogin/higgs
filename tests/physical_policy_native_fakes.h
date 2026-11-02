// Actual adapter compiled on synthetic seams, never game ABI/layout evidence.
#pragma once
#include "hand_broker.h"
#include "hand_physical_policy.h"
#include "hand_transactions.h"
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <map>
#include <mutex>
#define CHECK(x)                                                                                                       \
    do                                                                                                                 \
    {                                                                                                                  \
        if (!(x))                                                                                                      \
        {                                                                                                              \
            std::fprintf(stderr, "FAIL %d %s\n", __LINE__, #x);                                                        \
            std::abort();                                                                                              \
        }                                                                                                              \
    } while (0)
struct NiPoint3
{
    float x = 0, y = 0, z = 0;
};
struct NiMatrix33
{
    float data[3][3]{{2, 0, 0}, {0, 2, 0}, {0, 0, 2}};
    float operator()(int r, int c) const
    {
        return data[r][c];
    }
};
using hkMatrix3 = NiMatrix33;
struct hkVector4
{
    float value[4]{};
    hkVector4() = default;
    hkVector4(float x, float y, float z, float w = 0) : value{x, y, z, w}
    {
    }
    float &operator()(int i)
    {
        return value[i];
    }
    float operator()(int i) const
    {
        return value[i];
    }
};
hkVector4 NiPointToHkVector(NiPoint3 p)
{
    return {p.x, p.y, p.z};
}
void NiMatrixToHkMatrix(const NiMatrix33 &a, hkMatrix3 &b)
{
    b = a;
}
struct bhkWorld;
template <class T> void FixtureRetain(T *)
{
}
template <class T> void FixtureRelease(T *)
{
}
void FixtureRetain(bhkWorld *);
void FixtureRelease(bhkWorld *);
bool FixtureWorldAlive(std::recursive_mutex *);
void FixtureWorldLockEntered(std::recursive_mutex *);
void FixtureWorldLockLeft(std::recursive_mutex *);
template <class T> struct NiPointer
{
    T *p = nullptr;
    NiPointer() = default;
    NiPointer(T *pointer) : p(pointer)
    {
        FixtureRetain(p);
    }
    NiPointer(const NiPointer &other) : NiPointer(other.p)
    {
    }
    ~NiPointer()
    {
        FixtureRelease(p);
    }
    T *operator->() const
    {
        return p;
    }
    T *val() const
    {
        return p;
    }
    operator T *() const
    {
        return p;
    }
    NiPointer &operator=(NiPointer other)
    {
        std::swap(p, other.p);
        return *this;
    }
};
namespace RE
{
template <class T> using hkRefPtr = NiPointer<T>;
}
int worldLocks = 0;
struct BSWriteLocker
{
    std::recursive_mutex *mutex;
    BSWriteLocker(std::recursive_mutex *p) : mutex(p)
    {
        CHECK(FixtureWorldAlive(mutex));
        mutex->lock();
        FixtureWorldLockEntered(mutex);
        ++worldLocks;
    }
    ~BSWriteLocker()
    {
        CHECK(FixtureWorldAlive(mutex));
        --worldLocks;
        mutex->unlock();
        FixtureWorldLockLeft(mutex);
    }
};
struct hkpMotion
{
    uint32_t m_type = 1;
    hkVector4 m_inertiaAndMassInv{.5f, .5f, .5f, .1f};
    hkMatrix3 inertia;
    hkVector4 com;
    unsigned massWrites = 0, inertiaWrites = 0, comWrites = 0;
    void getInertiaLocal(hkMatrix3 &out)
    {
        CHECK(worldLocks);
        out = inertia;
    }
    const hkVector4 &getCenterOfMassLocal() const
    {
        CHECK(worldLocks);
        return com;
    }
    void setMass(float mass)
    {
        CHECK(worldLocks);
        float before = 1 / m_inertiaAndMassInv(3);
        m_inertiaAndMassInv(3) = 1 / mass;
        for (auto &row : inertia.data)
            for (float &value : row)
                value *= mass / before;
        com(0) += .5f;
        ++massWrites;
    }
    void setInertiaLocal(const hkMatrix3 &value)
    {
        CHECK(worldLocks);
        inertia = value;
        ++inertiaWrites;
    }
    void setCenterOfMassInLocal(const hkVector4 &value)
    {
        CHECK(worldLocks);
        com = value;
        ++comWrites;
    }
};
struct hkpShape
{
};
struct hkpWorld
{
};
struct hkpCollidable
{
    hkpShape *shape = nullptr;
    const hkpShape *getShape() const
    {
        return shape;
    }
};
struct hkpRigidBody
{
    hkpMotion motion;
    hkpCollidable collidable;
    hkpWorld *world = nullptr;
    hkpMotion *getMotion()
    {
        return &motion;
    }
    hkpWorld *getWorld()
    {
        return world;
    }
    hkpCollidable *getCollidable()
    {
        return &collidable;
    }
};
struct bhkRigidBody
{
    hkpRigidBody *hkBody = nullptr;
};
std::map<std::recursive_mutex *, bhkWorld *> fixtureWorldOwners;
unsigned fixtureWorldDestructions = 0, fixtureWorldDeathsWhileLocked = 0;
struct bhkWorld
{
    NiPointer<hkpWorld> world;
    std::recursive_mutex worldLock;
    unsigned references = 0, locked = 0;
    bool alive = true, simulateLastOwnerDestruction = false;
    bhkWorld()
    {
        fixtureWorldOwners[&worldLock] = this;
    }
    ~bhkWorld()
    {
        CHECK(!locked);
        fixtureWorldOwners.erase(&worldLock);
    }
};
void FixtureRetain(bhkWorld *world)
{
    if (world)
    {
        CHECK(world->alive);
        ++world->references;
    }
}
void FixtureRelease(bhkWorld *world)
{
    if (!world)
        return;
    CHECK(world->references);
    if (!--world->references && world->simulateLastOwnerDestruction)
    {
        world->alive = false;
        ++fixtureWorldDestructions;
        if (world->locked)
            ++fixtureWorldDeathsWhileLocked;
    }
}
bool FixtureWorldAlive(std::recursive_mutex *lock)
{
    auto it = fixtureWorldOwners.find(lock);
    return it == fixtureWorldOwners.end() || it->second->alive;
}
void FixtureWorldLockEntered(std::recursive_mutex *lock)
{
    auto it = fixtureWorldOwners.find(lock);
    if (it != fixtureWorldOwners.end())
        ++it->second->locked;
}
void FixtureWorldLockLeft(std::recursive_mutex *lock)
{
    auto it = fixtureWorldOwners.find(lock);
    if (it != fixtureWorldOwners.end())
    {
        CHECK(it->second->alive && it->second->locked);
        --it->second->locked;
    }
}
class NiAVObject
{
  public:
    uint32_t m_flags = 0;
    virtual ~NiAVObject() = default;
};
struct NiNode : NiAVObject
{
};
struct TESObjectREFR
{
    uint32_t formID = 0;
    NiNode *model = nullptr;
    NiNode *GetNiNode()
    {
        return model;
    }
};
struct PlayerCharacter : TESObjectREFR
{
    enum Node
    {
        kNode_LeftControllerNode,
        kNode_RightControllerNode
    };
    NiAVObject *unk3F0[2]{};
    void *parentCell = nullptr;
};
PlayerCharacter fixturePlayer;
PlayerCharacter *playerPointer = &fixturePlayer;
PlayerCharacter **g_thePlayer = &playerPointer;
bhkWorld *currentWorld = nullptr;
std::map<uint32_t, TESObjectREFR *> forms;
std::map<const hkpCollidable *, NiAVObject *> bodyNodes;
std::map<NiAVObject *, bhkRigidBody *> nodeBodies;
NiPointer<bhkWorld> GetHavokWorldFromCell(void *)
{
    return currentWorld;
}
TESObjectREFR *LookupFormByID(uint32_t id)
{
    return forms[id];
}
bool LookupREFRByHandle(uint32_t id, NiPointer<TESObjectREFR> &out)
{
    out = forms[id];
    return out.p != nullptr;
}
NiPointer<NiAVObject> GetNodeFromCollidable(const hkpCollidable *body)
{
    CHECK(worldLocks);
    return bodyNodes[body];
}
NiPointer<bhkRigidBody> GetRigidBody(NiAVObject *node)
{
    CHECK(worldLocks);
    return nodeBodies[node];
}
bool IsMoveableEntity(hkpRigidBody *body)
{
    CHECK(worldLocks);
    return body->motion.m_type == 1;
}
struct Hand
{
    bool constrained = true;
    struct
    {
        bool isActor = false;
        uint32_t handle = 0;
        NiPointer<bhkRigidBody> rigidBody;
    } selectedObject;
    bool HasHeldConstrained() const
    {
        return constrained;
    }
};
Hand left, right;
Hand *g_leftHand = &left;
Hand *g_rightHand = &right;
