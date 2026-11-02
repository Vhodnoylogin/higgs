int main()
{
    using namespace HiggsHands;
    hkpWorld raw;
    bhkWorld world;
    world.world = &raw;
    currentWorld = &world;
    fixturePlayer.parentCell = &world;
    NiNode root, itemModel;
    fixturePlayer.model = &root;
    NiAVObject controller, collision;
    fixturePlayer.unk3F0[0] = &controller;
    controller.m_flags = 0x20;
    hkpShape shape;
    hkpRigidBody body;
    body.world = &raw;
    body.collidable.shape = &shape;
    bhkRigidBody wrapper;
    wrapper.hkBody = &body;
    TESObjectREFR reference;
    reference.formID = 700;
    reference.model = &itemModel;
    forms[700] = &reference;
    bodyNodes[&body.collidable] = &collision;
    nodeBodies[&collision] = &wrapper;
    left.selectedObject.handle = 700;
    left.selectedObject.rigidBody = &wrapper;
    HIGGS_HandClientInfo info{};
    info.structSize = sizeof(info);
    std::snprintf(info.name, sizeof(info.name), "native properties");
    uint64_t owner = 0, other = 0;
    CHECK(!Service().Register(info, owner));
    std::snprintf(info.name, sizeof(info.name), "other properties");
    CHECK(!Service().Register(info, other));
    Service().ObserveWorld(1, false, Now());
    std::array<HIGGS_HandRecord, 2> hands{};
    for (auto &hand : hands)
        hand.observationMask =
            HIGGS_OBS_HELD_KNOWN | HIGGS_OBS_EQUIPMENT_KNOWN | HIGGS_OBS_TRACKING_VALID | HIGGS_OBS_PALM_VALID;
    hands[0].heldRefId = 700;
    Transactions().ObserveHeld(HIGGS_HAND_LEFT, 100, 700, 900, 1, Now());
    Service().Publish(hands, Now());
    MaintainNativePhysicalPolicies();
    auto requestFor = [&](uint32_t kind) {
        HIGGS_PhysicalPolicyRequest request{};
        request.structSize = sizeof(request);
        request.kind = kind;
        request.physicalHand = HIGGS_HAND_LEFT;
        request.durationMs = 1000;
        request.epoch = Service().Epoch();
        request.handGeneration = Service().HandGeneration(HIGGS_HAND_LEFT);
        return request;
    };
    auto request = requestFor(1);
    request.fields = HIGGS_PROPERTY_MASS;
    request.massMultiplier = 2;
    request.inertiaMultiplier = 1;
    CHECK(!Transactions().Held(owner, HIGGS_HAND_LEFT, request.item));
    uint64_t id = 0;
    CHECK(!PhysicalPolicies().Request(owner, request, id, Now()));
    MaintainNativePhysicalPolicies();
    CHECK(std::fabs(1 / body.motion.m_inertiaAndMassInv(3) - 20) < .001f);
    CHECK(body.motion.inertia.data[0][0] == 2 && body.motion.com(0) == 0);
    const auto writes = body.motion.massWrites;
    MaintainNativePhysicalPolicies();
    CHECK(body.motion.massWrites == writes);
    request.massMultiplier = 3;
    CHECK(!PhysicalPolicies().Update(owner, id, 1, request, Now()));
    MaintainNativePhysicalPolicies();
    CHECK(std::fabs(1 / body.motion.m_inertiaAndMassInv(3) - 30) < .001f);
    auto center = request;
    center.fields = HIGGS_PROPERTY_CENTER_OF_MASS;
    center.centerOfMassOffset[0] = .25f;
    center.massMultiplier = 1;
    uint64_t com = 0;
    CHECK(!PhysicalPolicies().Request(other, center, com, Now()));
    MaintainNativePhysicalPolicies();
    CHECK(body.motion.com(0) == .25f);
    CHECK(!PhysicalPolicies().End(owner, id, request.epoch));
    MaintainNativePhysicalPolicies();
    CHECK(std::fabs(1 / body.motion.m_inertiaAndMassInv(3) - 10) < .001f && body.motion.inertia.data[0][0] == 2 &&
          body.motion.com(0) == .25f);
    CHECK(!PhysicalPolicies().End(other, com, request.epoch));
    MaintainNativePhysicalPolicies();
    CHECK(body.motion.com(0) == 0);
    CHECK(!PhysicalPolicies().Request(owner, request, id, Now()));
    MaintainNativePhysicalPolicies();
    {
        BSWriteLocker lock(&world.worldLock);
        body.motion.setMass(77);
    }
    MaintainNativePhysicalPolicies();
    MaintainNativePhysicalPolicies();
    HIGGS_PhysicalPolicyState state{};
    CHECK(!PhysicalPolicies().Copy(owner, id, state));
    CHECK(state.stage == HIGGS_POLICY_BROKEN && (state.flags & HIGGS_POLICY_EXTERNAL_WRITE));
    CHECK(std::fabs(1 / body.motion.m_inertiaAndMassInv(3) - 77) < .001f);
    // A foreign inertia-only write must not strand our still-owned mass scale.
    CHECK(!PhysicalPolicies().Request(owner, request, id, Now()));
    MaintainNativePhysicalPolicies();
    {
        BSWriteLocker lock(&world.worldLock);
        auto tensor = body.motion.inertia;
        tensor.data[0][0] = tensor.data[1][1] = tensor.data[2][2] = 11;
        body.motion.setInertiaLocal(tensor);
    }
    MaintainNativePhysicalPolicies();
    MaintainNativePhysicalPolicies();
    CHECK(std::fabs(1 / body.motion.m_inertiaAndMassInv(3) - 77) < .001f && body.motion.inertia.data[0][0] == 11);
    CHECK(!PhysicalPolicies().Copy(owner, id, state));
    CHECK((state.flags & HIGGS_POLICY_RESTORED_MASS) && (state.flags & HIGGS_POLICY_EXTERNAL_WRITE));
    // Conversely preserve a foreign mass-only write while restoring owned inertia.
    request.fields = HIGGS_PROPERTY_INERTIA;
    request.massMultiplier = 1;
    request.inertiaMultiplier = 2;
    CHECK(!PhysicalPolicies().Request(owner, request, id, Now()));
    MaintainNativePhysicalPolicies();
    {
        BSWriteLocker lock(&world.worldLock);
        body.motion.m_inertiaAndMassInv(3) = 1.f / 55;
    }
    MaintainNativePhysicalPolicies();
    MaintainNativePhysicalPolicies();
    CHECK(std::fabs(1 / body.motion.m_inertiaAndMassInv(3) - 55) < .001f && body.motion.inertia.data[0][0] == 11);
    CHECK(!PhysicalPolicies().Copy(owner, id, state));
    CHECK((state.flags & HIGGS_POLICY_RESTORED_INERTIA) && (state.flags & HIGGS_POLICY_EXTERNAL_WRITE));
    // Ownership comparisons are exact readbacks, not an epsilon that could
    // silently overwrite a small foreign mass-only adjustment.
    CHECK(!PhysicalPolicies().Request(owner, request, id, Now()));
    MaintainNativePhysicalPolicies();
    float foreignInverse = 0;
    {
        BSWriteLocker lock(&world.worldLock);
        foreignInverse = 1.f / 55.00001f;
        body.motion.m_inertiaAndMassInv(3) = foreignInverse;
    }
    MaintainNativePhysicalPolicies();
    MaintainNativePhysicalPolicies();
    CHECK(body.motion.m_inertiaAndMassInv(3) == foreignInverse && body.motion.inertia.data[0][0] == 11);
    CHECK(!PhysicalPolicies().Copy(owner, id, state));
    CHECK(state.flags & HIGGS_POLICY_EXTERNAL_WRITE);
    request = requestFor(3);
    request.presentation = HIGGS_PRESENTATION_CULL_CONTROLLER;
    CHECK(!PhysicalPolicies().Request(owner, request, id, Now()));
    MaintainNativePhysicalPolicies();
    CHECK(controller.m_flags == 0x21);
    controller.m_flags |= 0x80;
    CHECK(!PhysicalPolicies().End(owner, id, request.epoch));
    MaintainNativePhysicalPolicies();
    CHECK(controller.m_flags == 0xa0);
    request = requestFor(1);
    request.fields = 7;
    request.massMultiplier = request.inertiaMultiplier = 2;
    CHECK(!Transactions().Held(owner, 1, request.item));
    CHECK(!PhysicalPolicies().Request(owner, request, id, Now()));
    MaintainNativePhysicalPolicies();
    const float base = state.effective.mass;
    (void)base;
    PreparePhysicalPolicyRelease(true);
    CHECK(!PhysicalPolicies().Copy(owner, id, state));
    CHECK(state.stage == HIGGS_POLICY_ENDED && (state.flags & HIGGS_POLICY_RESTORED));
    // The retained record is now the creation world's last simulated owner.
    // Clearing it during world-replacement cleanup must not free its live lock.
    CHECK(!PhysicalPolicies().Request(owner, request, id, Now()));
    MaintainNativePhysicalPolicies();
    CHECK(world.references == 1);
    world.simulateLastOwnerDestruction = true;
    currentWorld = nullptr;
    const auto destroyedBefore = fixtureWorldDestructions;
    Service().ObserveWorld(2, false, Now());
    MaintainNativePhysicalPolicies();
    CHECK(!world.alive && !world.references && !world.locked && !worldLocks);
    CHECK(fixtureWorldDestructions == destroyedBefore + 1 && !fixtureWorldDeathsWhileLocked);
    CHECK(!PhysicalPolicies().Copy(owner, id, state));
    CHECK(state.stage == HIGGS_POLICY_BROKEN && (state.flags & HIGGS_POLICY_RESTORED));
    std::puts("PASS actual policy cleanup retains last creation-world owner through unlock");
    std::puts("PASS actual native property source: coupled setter/readback, baseline no-drift update, independent COM, "
              "external-write preservation, controller cull restoration and before-drop cleanup (synthetic seams)");
}
