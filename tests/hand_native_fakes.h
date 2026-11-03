// Minimal declared native seams for compiling/executing the actual extracted
// adapter and input methods. These are NOT SKSE/Havok layout definitions and do
// not qualify game hooks, engine concurrency, collision response or hardware.
#pragma once
#include "hand_broker.h"
#include "hand_observations.h"
#include "hand_support.h"
#include "hand_transactions.h"
#include "hand_physical_policy.h"
#include "synergy_inventory_native.h"
#include "haptic_schedule.h"
#include "higgssynergy.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <mutex>
using UInt32 = uint32_t;
namespace HiggsHands {inline void DrainNativeInventory() { /* Separate inventory fixture executes its real adapter. */ }}
namespace HiggsHands {
// The own-item contact suite executes the real native snapshot/observation path.
inline void InvalidateNativeOwnContacts() {}
inline void PublishNativeOwnContacts() {}
}
struct hkpRigidBody;
namespace HiggsHands {
// Articulated-part application/contact behavior has its own actual-source fixture.
inline void MaintainNativeHandParts() {}
inline void MaintainNativePhysicalPolicies() {}
inline void RecordNativeHandPartContact(const hkpRigidBody*,uint64_t,uint64_t) {}
}
#define CHECK(x)                                                                                                       \
    do                                                                                                                 \
    {                                                                                                                  \
        if (!(x))                                                                                                      \
        {                                                                                                              \
            std::fprintf(stderr, "fixture FAIL %d: %s\n", __LINE__, #x);                                               \
            std::abort();                                                                                              \
        }                                                                                                              \
    } while (0)
struct NiPoint3
{
    float x = 0, y = 0, z = 0;
    NiPoint3() = default;
    NiPoint3(float a, float b, float c) : x(a), y(b), z(c) {};
    float operator()(int i) const
    {
        return i == 0 ? x : (i == 1 ? y : z);
    }
    NiPoint3 operator+(NiPoint3 b) const
    {
        return {x + b.x, y + b.y, z + b.z};
    }
    NiPoint3 operator-(NiPoint3 b) const
    {
        return {x - b.x, y - b.y, z - b.z};
    }
    NiPoint3 operator*(float v) const
    {
        return {x * v, y * v, z * v};
    }
    NiPoint3 operator/(float v) const
    {
        return {x / v, y / v, z / v};
    }
    NiPoint3 &operator+=(NiPoint3 v)
    {
        *this = *this + v;
        return *this;
    }
};
float VectorLength(NiPoint3 v)
{
    return std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z);
}
NiPoint3 VectorNormalized(NiPoint3 v)
{
    return v / VectorLength(v);
}
struct NiMatrix33
{
    float data[3][3]{{1, 0, 0}, {0, 1, 0}, {0, 0, 1}};
    NiPoint3 getColumn(int j) const
    {
        return {data[0][j], data[1][j], data[2][j]};
    };
    NiPoint3 operator*(NiPoint3 p) const
    {
        return {data[0][0] * p.x + data[0][1] * p.y + data[0][2] * p.z,
                data[1][0] * p.x + data[1][1] * p.y + data[1][2] * p.z,
                data[2][0] * p.x + data[2][1] * p.y + data[2][2] * p.z};
    }
    NiMatrix33 operator*(const NiMatrix33 &other) const
    {
        NiMatrix33 result{};
        for (size_t r = 0; r < 3; ++r)
            for (size_t c = 0; c < 3; ++c)
            {
                result.data[r][c] = 0;
                for (size_t k = 0; k < 3; ++k)
                    result.data[r][c] += data[r][k] * other.data[k][c];
            }
        return result;
    }
};
struct NiTransform
{
    NiPoint3 pos;
    NiMatrix33 rot;
    float scale = 1;
    NiPoint3 operator*(NiPoint3 p) const
    {
        return pos + NiPoint3(rot.data[0][0] * p.x + rot.data[0][1] * p.y + rot.data[0][2] * p.z,
                              rot.data[1][0] * p.x + rot.data[1][1] * p.y + rot.data[1][2] * p.z,
                              rot.data[2][0] * p.x + rot.data[2][1] * p.y + rot.data[2][2] * p.z) *
                         scale;
    }
};
template <class T> struct NiPointer
{
    T *p = nullptr;
    NiPointer() = default;
    NiPointer(T *v) : p(v)
    {
    }
    T *val() const
    {
        return p;
    }
    T *operator->() const
    {
        return p;
    }
    operator T *() const
    {
        return p;
    }
    NiPointer &operator=(T *v)
    {
        p = v;
        return *this;
    }
};
namespace RE
{
template <class T> using hkRefPtr = NiPointer<T>;
}
class NiAVObject
{
  public:
    NiTransform m_worldTransform;
    virtual ~NiAVObject() = default;
};
struct NiNode : NiAVObject
{
};
using hkVector4 = NiPoint3;
using hkRotation = NiMatrix33;
struct hkTransform
{
    hkVector4 m_translation;
    hkRotation m_rotation;
    const hkRotation &getRotation() const
    {
        return m_rotation;
    }
    const hkVector4 &getTranslation() const
    {
        return m_translation;
    }
};
struct hkQuaternion
{
    void setFromRotationSimd(const hkRotation &) {};
};
NiPoint3 HkVectorToNiPoint(hkVector4 v)
{
    return v;
}
hkVector4 NiPointToHkVector(NiPoint3 p)
{
    return p;
}
void NiMatrixToHkMatrix(const NiMatrix33 &a, hkRotation &b)
{
    b = a;
}
struct hkpMotion
{
    enum MotionType
    {
        MOTION_FIXED = 1,
        MOTION_DYNAMIC = 2,
        MOTION_KEYFRAMED = 3
    };
    MotionType m_type = MOTION_FIXED;
};
struct hkpShape
{
};
struct hkpRigidBody;
struct hkpCdBody
{
    const hkpCdBody *m_parent = nullptr;
};
struct hkpCollidable : hkpCdBody
{
    hkpShape *m_shape = nullptr;
    hkpRigidBody *owner = nullptr;
    const hkpShape *getShape() const
    {
        return m_shape;
    }
};
struct hkpWorld
{
};
struct hkpRigidBody
{
    hkpMotion m_motion;
    uint64_t m_uid = 1;
    uintptr_t m_userData = 0;
    uintptr_t getUserData() const
    {
        return m_userData;
    }
    hkpCollidable collider;
    hkTransform transform;
    hkpWorld *world = nullptr;
    hkpWorld *getWorld() const
    {
        return world;
    }
    hkVector4 getPosition() const
    {
        return transform.m_translation;
    }
    hkVector4 getLinearVelocity() const
    {
        return {};
    }
    hkVector4 getAngularVelocity() const
    {
        return {};
    }
    const hkpCollidable *getCollidable() const
    {
        return &collider;
    }
    hkpCollidable *getCollidable()
    {
        return &collider;
    }
    const hkTransform &getTransform() const
    {
        return transform;
    }
};
hkpRigidBody *hkpGetRigidBody(const hkpCollidable *c)
{
    return c->owner;
}
struct bhkRigidBody
{
    hkpRigidBody *hkBody = nullptr;
    hkpWorld *GetHavokWorld_1()
    {
        return hkBody->world;
    }
};
struct bhkWorld
{
    NiPointer<hkpWorld> world;
    std::recursive_mutex worldLock;
};
int readLocks = 0;
struct BSReadLocker
{
    std::recursive_mutex *m;
    BSReadLocker(std::recursive_mutex *v) : m(v)
    {
        m->lock();
        ++readLocks;
    }
    ~BSReadLocker()
    {
        --readLocks;
        m->unlock();
    }
};
using BSWriteLocker = BSReadLocker;
struct TESForm
{
    uint32_t formID = 0, formType = 0;
    virtual ~TESForm() = default;
};
struct BaseExtraList
{
    uint32_t count = 1;
    uint16_t testUniqueId = 0; // Opaque item metadata; native hold identity must not use it.
};
uint32_t BSExtraList_GetCount(BaseExtraList *data)
{
    return data->count;
}
struct TESObjectREFR : TESForm
{
    TESForm *baseForm = nullptr;
    BaseExtraList extraData;
    NiNode *node = nullptr;
    NiNode *GetNiNode()
    {
        return node;
    }
};
struct TESObjectWEAP : TESForm
{
    struct GameData
    {
        enum
        {
            kType_HandToHandMelee = 0
        };
        uint32_t type = 1;
    } gameData;
    bool bow = false, twoHanded = false;
};
struct Actor : TESObjectREFR
{
};
struct TESAmmo : TESForm
{
};
struct PlayerCharacter : TESObjectREFR
{
    struct ActorState
    {
        bool drawn = false;
        bool IsWeaponDrawn()
        {
            return drawn;
        }
    } actorState;
    void *parentCell = nullptr;
    TESForm *equipment[2]{};
    TESForm *GetEquippedObject(bool off)
    {
        return equipment[off ? 1 : 0];
    }
};
#define DYNAMIC_CAST(value, from, to) dynamic_cast<to *>(value)
PlayerCharacter fixturePlayer;
PlayerCharacter *playerPtr = &fixturePlayer;
PlayerCharacter **g_thePlayer = &playerPtr;
bool leftHanded = false;
bool *g_leftHandedMode = &leftHanded;
float scaleValue = 0.014f;
float *g_havokWorldScale = &scaleValue;
float inverseScaleValue = 1.f / scaleValue;
float *g_inverseHavokWorldScale = &inverseScaleValue;
float deltaValue = 0.01f;
float *g_deltaTime = &deltaValue;
double g_currentFrameTime = 1;
bhkWorld *currentWorld = nullptr;
NiPointer<bhkWorld> GetHavokWorldFromCell(void *)
{
    return currentWorld;
}
std::map<uint32_t, TESForm *> forms;
std::map<const hkpCollidable *, TESObjectREFR *> refs;
std::map<const hkpCollidable *, NiAVObject *> nodes;
TESForm *LookupFormByID(uint32_t id)
{
    auto it = forms.find(id);
    return it == forms.end() ? nullptr : it->second;
}
bool LookupREFRByHandle(uint32_t id, NiPointer<TESObjectREFR> &out)
{
    out = dynamic_cast<TESObjectREFR *>(LookupFormByID(id));
    return out.p != nullptr;
}
TESObjectREFR *GetRefFromCollidable(const hkpCollidable *c)
{
    return refs[c];
}
NiPointer<NiAVObject> GetNodeFromCollidable(const hkpCollidable *c)
{
    return nodes[c];
}
TESObjectWEAP *GetEquippedWeapon(PlayerCharacter *p, bool off)
{
    return dynamic_cast<TESObjectWEAP *>(p->GetEquippedObject(off));
}
bool IsBow(TESObjectWEAP *w)
{
    return w->bow;
}
bool IsTwoHanded(TESObjectWEAP *w)
{
    return w->twoHanded;
}
TESAmmo *ammo = nullptr;
using Actor_GetCurrentAmmo = TESAmmo *(*)(PlayerCharacter *);
TESAmmo *GetAmmo(PlayerCharacter *)
{
    return ammo;
}
template <class F> F get_vfunc(PlayerCharacter *, int)
{
    return GetAmmo;
}
int Actor_GetRace(PlayerCharacter *)
{
    return 0;
}
bool TESRace_IsBeast(int)
{
    return false;
}
struct hkpShapeRayCastCollectorOutput
{
    float m_hitFraction = 0.5f;
    hkVector4 m_normal{0, 0, 1};
};
struct hkpRayHitCollector
{
    float m_earlyOutHitFraction = 1;
    virtual ~hkpRayHitCollector() = default;
    virtual void addRayHit(const hkpCdBody &, const hkpShapeRayCastCollectorOutput &) = 0;
};
struct hkpWorldRayCastInput
{
    uint32_t m_filterInfo = 0;
    hkVector4 m_from, m_to;
};
hkpCollidable *rayHit = nullptr;
hkpShapeRayCastCollectorOutput rayOutput;
void hkpWorld_CastRay(hkpWorld *, hkpWorldRayCastInput *, hkpRayHitCollector *c)
{
    CHECK(readLocks > 0);
    if (rayHit)
        c->addRayHit(*rayHit, rayOutput);
}
namespace HiggsMotion
{
struct Pair
{
    struct Hand
    {
        bool available = true;
    } hands[2];
};
struct Owner
{
    Pair CopyObservation()
    {
        return {};
    }
} tracking;
Owner &Tracking()
{
    return tracking;
}
} // namespace HiggsMotion
namespace HiggsSynergy
{
HIGGS_SynergyHandState CopyHandState(bool)
{
    return {};
}
} // namespace HiggsSynergy
namespace MenuChecker
{
bool stopped = false;
bool isGameStopped()
{
    return stopped;
}
} // namespace MenuChecker
namespace Config
{
struct Options
{
    NiPoint3 mouthHmdOffset{1, 0, 0}, rightShoulderHmdOffset{0, 2, 0}, leftShoulderHmdOffset{0, -2, 0};
    float mouthRadius = 2, rightShoulderRadius = 3, leftShoulderRadius = 4;
    bool enableTrigger = true, enableGrip = true, allowGripPressWhileUsingTouchInput = false;
    double triggerPressedLeewayTime = 0.2, inputLeewayTime = 0.2, forceInputTime = 0.2;
    NiPoint3 handCollisionBoxOffset, handCollisionBoxOffsetBeast;
} options;
} // namespace Config
namespace vr_src
{
enum EVRButtonId
{
    k_EButton_SteamVR_Trigger = 33,
    k_EButton_Grip = 2
};
uint64_t ButtonMaskFromId(EVRButtonId b)
{
    return UINT64_C(1) << b;
}
struct VRControllerState001_t
{
    uint64_t ulButtonPressed = 0, ulButtonTouched = 0;
};
} // namespace vr_src
struct FakeHaptics
{
    int queued = 0;
    void QueueHapticEvent(float, float, float)
    {
        ++queued;
    }
};
struct Hand
{
    uint32_t nativeRegionInsideMask = 0;
    uint64_t nativeRegionEpoch = 0;
    NiPointer<NiAVObject> nativeRegionHmdSource;
    std::array<float,13> nativeRegionGeometrySignature{};
    uint64_t nativeRegionSourceGeneration = 0;
    void UpdateNativeRegionSelection(NiAVObject*, const NiPoint3&, const NiPoint3&);
    enum class State
    {
        Idle,
        SelectionLocked,
        SelectedTwoHand,
        Held,
        HeldBody,
        GrabExternal
    };
    enum class InputState
    {
        Idle,
        Leeway,
        Block,
        Force
    };
    bool isLeft = true, holding = false, secondary = false, constrained = false, disableConsumeStash = false,
         releaseRequested = false, externalGrabRequested = false, synergyGrabPending = false;
    bool triggerDown = false, gripDown = false, useTouchForGrip = false, delayGripInput = false, inputTrigger = false,
         inputGrip = false, gripPressWasBlockedWithGripTouch = false, grabRequested = false, wasObjectGrabbed = false;
    double grabRequestedTime = 0, forceInputTime = 0;
    State state = State::Idle;
    InputState inputState = InputState::Idle;
    NiPointer<bhkRigidBody> handBody, weaponBody;
    NiPointer<TESObjectREFR> externalGrabRequestedObject;
    void *grabConstraint = nullptr;
    uint32_t playerCollisionGroup = 1;
    struct Selected
    {
        uint32_t handle = 0;
        NiPointer<bhkRigidBody> rigidBody;
    } selectedObject;
    FakeHaptics haptics;
    NiAVObject *node = nullptr;
    NiTransform grabTransform, m_handTransformWithoutVrikOffset;
        struct {TESObjectWEAP* weapon=nullptr;} twoHandedState;
        double grabbedTime=0;
        uint64_t synergySecondaryEpisode=0;
    bhkRigidBody *backing = nullptr;
    NiPoint3 geometry;
    NiPoint3 palmPosHandspace;
    int rebuilds = 0;
    bool HasHeldObject() const
    {
        return holding;
    }
    bool IsTwoHanding() const
    {
        return secondary;
    }
    bool HasHeldConstrained() const
    {
        return holding && constrained;
    }
    bool CanGrabObject() const
    {
        return HiggsHands::MayAcquire(isLeft) && !holding;
    }
    bool CanHoldObject() const
    {
        return HiggsHands::MayAcquire(isLeft);
    }
    bool CanTwoHand() const
    {
        return HiggsHands::MaySupport(isLeft);
    }
    bool IsInGrabbableState() const
    {
        return !holding && !secondary;
    }
    bool ShouldRestrictTrigger() const
    {
        return false;
    }
    bool ShouldNotBlockTrigger() const
    {
        return false;
    }
    NiPointer<NiAVObject> GetFirstPersonHandNode()
    {
        return node;
    }
    NiPoint3 GetPalmPositionWS(const NiTransform &t)
    {
        return t.pos;
    }
    NiTransform GetGrabTransform()
    {
        return grabTransform;
    }
    void SetGrabTransform(const NiTransform &t)
    {
        grabTransform = t;
    }
    void RemoveHandCollision(bhkWorld *)
    {
        CHECK(readLocks > 0);
        handBody = nullptr;
    }
    void CreateHandCollision(bhkWorld *, const NiPoint3 *extents = nullptr)
    {
        CHECK(readLocks > 0);
        handBody = backing;
        geometry = extents ? *extents : NiPoint3(0.05f, 0.015f, 0.09f);
        ++rebuilds;
    }
    void ControllerStateUpdate(uint32_t, vr_src::VRControllerState001_t *);
    hkTransform ComputeHandCollisionTransform(bool, const NiTransform * = nullptr);
    hkTransform ComputeWeaponCollisionTransform(bhkRigidBody *b)
    {
        return b->hkBody->transform;
    }
    void MoveHandAndWeaponCollision(const NiPoint3 &);
};
Hand fixtureLeft, fixtureRight;
Hand *g_leftHand = &fixtureLeft;
Hand *g_rightHand = &fixtureRight;
struct VRMeleeData
{
    NiPointer<NiNode> collisionNode;
};
VRMeleeData melee;
VRMeleeData *GetVRMeleeData(bool)
{
    return &melee;
}
NiPointer<bhkRigidBody> GetRigidBody(NiNode *)
{
    return nullptr;
}
int keyframes = 0;
void ApplyHardKeyframeVelocityClamped(hkVector4 p, hkQuaternion, float, NiPointer<bhkRigidBody> b)
{
    b->hkBody->transform.m_translation = p;
    ++keyframes;
}

struct FixtureContactPoint
{
    hkVector4 position, normal{0, 0, 1};
    hkVector4 getPosition() const
    {
        return position;
    }
    hkVector4 getNormal() const
    {
        return normal;
    }
};
struct FixtureContactProperties
{
    uint8_t m_flags = 0;
    bool wasUsed() const
    {
        return true;
    }
    bool isPotential() const
    {
        return false;
    }
};
struct hkContactPointMaterial
{
    enum FlagEnum
    {
        CONTACT_IS_DISABLED = 1
    };
};
class hkpContactPointEvent
{
  public:
    FixtureContactPoint *m_contactPoint = nullptr;
    FixtureContactProperties *m_contactPointProperties = nullptr;
    hkpRigidBody *m_bodies[2]{};
};
