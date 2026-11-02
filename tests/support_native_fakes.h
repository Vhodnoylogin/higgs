// Synthetic lifetime/call-order seams only. Actual SDK compilation is separate.
#pragma once
#include "hand_native_fakes.h"
#include "interop/planck_physical_interop.h"
#include <memory>
NiTransform hkTransformToNiTransform(const hkTransform &transform)
{
    NiTransform result{};
    result.pos = transform.m_translation * *g_inverseHavokWorldScale;
    result.rot = transform.m_rotation;
    return result;
}
hkTransform NiTransformTohkTransform(const NiTransform &transform, bool scale = true)
{
    hkTransform result{};
    result.m_translation = transform.pos * (scale ? *g_havokWorldScale : 1.f);
    result.m_rotation = transform.rot;
    return result;
}
std::map<NiNode *, bhkRigidBody *> rootBodies;
NiPointer<bhkRigidBody> GetFirstRigidBody(NiNode *node)
{
    CHECK(readLocks > 0);
    return rootBodies[node];
}
struct hkpPositionConstraintMotor
{
    float maximum = 0;
    void setMaxForce(float value)
    {
        CHECK(readLocks > 0);
        maximum = value;
    }
};
struct hkpConstraintData
{
    virtual ~hkpConstraintData() = default;
};
struct GrabConstraintData : hkpConstraintData
{
    hkpPositionConstraintMotor angular, linear;
    struct Atoms
    {
        struct Ragdoll
        {
            hkpPositionConstraintMotor *m_motors[3]{};
        } m_ragdollMotors;
        struct Linear
        {
            hkpPositionConstraintMotor *m_motor = nullptr;
        } m_linearMotor0, m_linearMotor1, m_linearMotor2;
    } m_atoms;
    GrabConstraintData()
    {
        for (auto &motor : m_atoms.m_ragdollMotors.m_motors)
            motor = &angular;
        m_atoms.m_linearMotor0.m_motor = m_atoms.m_linearMotor1.m_motor = m_atoms.m_linearMotor2.m_motor = &linear;
    }
    void setInBodySpace(const hkTransform &, const hkTransform &)
    {
        CHECK(readLocks > 0);
    }
    void setTargetRelativeOrientationOfBodies(const hkRotation &)
    {
        CHECK(readLocks > 0);
    }
};
struct hkpConstraintInstance
{
    bool attached = false;
    GrabConstraintData data;
    void *getOwner()
    {
        return attached ? this : nullptr;
    }
    const hkpConstraintData *getData()
    {
        return &data;
    }
};
struct bhkGroupConstraint
{
    hkpConstraintInstance owned;
    hkpConstraintInstance *constraint = &owned;
};
unsigned constraintsAdded = 0, constraintsRemoved = 0, arrayAdds = 0, arrayRemoves = 0;
std::vector<std::unique_ptr<bhkGroupConstraint>> nativeConstraints;
bhkGroupConstraint *CreateGrabConstraint(hkpRigidBody *, hkpRigidBody *, NiTransform &, NiTransform &)
{
    CHECK(readLocks > 0);
    nativeConstraints.push_back(std::make_unique<bhkGroupConstraint>());
    return nativeConstraints.back().get();
}
void bhkRigidBody_AddConstraintToArray(bhkRigidBody *, bhkGroupConstraint *)
{
    CHECK(readLocks > 0);
    ++arrayAdds;
}
void bhkRigidBody_RemoveConstraintFromArray(bhkRigidBody *, bhkGroupConstraint *)
{
    CHECK(readLocks > 0);
    ++arrayRemoves;
}
void bhkWorld_AddConstraint(bhkWorld *, hkpConstraintInstance *constraint)
{
    CHECK(readLocks > 0);
    CHECK(!constraint->attached);
    constraint->attached = true;
    ++constraintsAdded;
}
void bhkWorld_RemoveConstraint(bhkWorld *, hkpConstraintInstance *constraint)
{
    CHECK(readLocks > 0);
    CHECK(constraint->attached);
    constraint->attached = false;
    ++constraintsRemoved;
}
struct SKSEMessagingInterface
{
    void Dispatch(uint32_t, uint32_t, void *, uint32_t, const char *)
    {
    }
};
