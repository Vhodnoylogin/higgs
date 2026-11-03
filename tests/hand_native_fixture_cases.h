// Tests invoke the actual extracted native adapter/input/target methods through
// synthetic engine seams. This is source execution, never native ABI/game proof.
struct InputConsumer
{
    uint64_t client = 0, executor = 0, registration = 0, grant = 0;
    int calls = 0;
};
void HIGGS_HAND_CALL FixtureExecute(const HIGGS_ActionRequest *r, void *data)
{
    auto &c = *static_cast<InputConsumer *>(data);
    c.grant = r->grant;
    ++c.calls;
}
void HIGGS_HAND_CALL FixtureRecognize(const HIGGS_HandInput *i, void *data)
{
    if (i->edge != HIGGS_INPUT_DOWN)
        return;
    auto &c = *static_cast<InputConsumer *>(data);
    HIGGS_ActionCandidate p{};
    p.structSize = sizeof(p);
    p.physicalHands = i->physicalHand;
    p.inputId = i->inputId;
    p.epoch = i->epoch;
    p.handGeneration = i->handGeneration;
    p.executor = c.executor;
    p.grantDurationMs = 1000;
    uint64_t id = 0;
    auto status = HiggsHands::Service().Submit(c.client, p, id, HiggsHands::Now());
    CHECK(status == HIGGS_HAND_OK || status == HIGGS_HAND_CONFLICT);
}
uint64_t RegisterFixture(const char *name)
{
    HIGGS_HandClientInfo info{};
    info.structSize = sizeof(info);
    std::snprintf(info.name, sizeof(info.name), "%s", name);
    uint64_t client = 0;
    CHECK(!HiggsHands::Service().Register(info, client));
    return client;
}
HIGGS_HandSnapshot FixtureSnapshot(uint64_t client)
{
    HIGGS_HandSnapshot s{};
    CHECK(!HiggsHands::Service().Copy(client, s, HiggsHands::Now()));
    return s;
}
int main()
{
    using namespace HiggsHands;
    hkpWorld engineWorld;
    bhkWorld wrapper;
    wrapper.world = &engineWorld;
    currentWorld = &wrapper;
    fixturePlayer.parentCell = &wrapper;
    NiNode playerNode, leftNode, rightNode;
    fixturePlayer.node = &playerNode;
    fixtureLeft.node = &leftNode;
    fixtureRight.node = &rightNode;
    fixtureRight.isLeft = false;
    hkpShape shape;
    hkpRigidBody leftBody, rightBody;
    leftBody.collider.owner = &leftBody;
    rightBody.collider.owner = &rightBody;
    leftBody.world = rightBody.world = &engineWorld;
    bhkRigidBody leftWrapper, rightWrapper;
    leftWrapper.hkBody = &leftBody;
    rightWrapper.hkBody = &rightBody;
    fixtureLeft.handBody = fixtureLeft.backing = &leftWrapper;
    fixtureRight.handBody = fixtureRight.backing = &rightWrapper;
    Service().ObserveWorld(1, false, Now());
    PublishNative();
    InputConsumer consumer;
    consumer.client = RegisterFixture("mask-test");
    CHECK(!Service().Executor(consumer.client, FixtureExecute, &consumer, consumer.executor));
    CHECK(!Service().Recognizer(consumer.client, FixtureRecognize, &consumer, consumer.registration));
    const auto trigger = vr_src::ButtonMaskFromId(vr_src::k_EButton_SteamVR_Trigger),
               grip = vr_src::ButtonMaskFromId(vr_src::k_EButton_Grip);
    fixtureLeft.inputState = Hand::InputState::Force;
    fixtureLeft.inputTrigger = true;
    fixtureLeft.inputGrip = true;
    fixtureLeft.useTouchForGrip = true;
    fixtureLeft.gripPressWasBlockedWithGripTouch = true;
    fixtureLeft.forceInputTime = g_currentFrameTime;
    vr_src::VRControllerState001_t input;
    input.ulButtonPressed = trigger | grip;
    input.ulButtonTouched = grip;
    fixtureLeft.ControllerStateUpdate(0, &input);
    CHECK(consumer.calls == 1);
    CHECK(!input.ulButtonPressed && !input.ulButtonTouched);
    CHECK(!fixtureLeft.grabRequested && !fixtureLeft.releaseRequested);
    CHECK(!Service().Release(consumer.client, consumer.grant));
    input.ulButtonPressed = trigger | grip;
    input.ulButtonTouched = grip;
    fixtureLeft.ControllerStateUpdate(0, &input);
    CHECK(!input.ulButtonPressed && !input.ulButtonTouched);
    input = {};
    fixtureLeft.ControllerStateUpdate(0, &input);
    CHECK(!input.ulButtonPressed && !input.ulButtonTouched);
    CHECK(!Service().Remove(consumer.client, consumer.registration));
    // Actual Hand::ControllerStateUpdate must retire Force/leeway across UI,
    // preserve possession, and never synthesize a Down/Up when it returns.
    auto menuFrame = [&](uint64_t pressed, uint64_t touched) {
        HIGGS_ControllerFrame raw{}; raw.structSize=sizeof(raw); raw.physicalHand=1; raw.deviceIndex=1;
        raw.flags=HIGGS_INPUT_NO_HARDWARE_TIME|HIGGS_INPUT_GRIP_USES_TOUCH; raw.epoch=Service().Epoch();
        static uint64_t sample=Now()+1000000; raw.observedAtNs=++sample; raw.pressed=pressed; raw.touched=touched;
        BeginControllerFrame(raw);vr_src::VRControllerState001_t state{};state.ulButtonPressed=pressed;state.ulButtonTouched=touched;
        fixtureLeft.ControllerStateUpdate(0,&state);FinishControllerFrame(1,state.ulButtonPressed,state.ulButtonTouched);return state;
    };
    Service().SetInputContext(1,Now());MenuChecker::stopped=true;
    fixtureLeft.inputState=Hand::InputState::Force;fixtureLeft.inputTrigger=fixtureLeft.inputGrip=true;
    fixtureLeft.triggerDown=fixtureLeft.gripDown=true;fixtureLeft.grabRequested=true;
    auto uiInput=menuFrame(trigger|grip,grip);CHECK(!uiInput.ulButtonPressed&&!uiInput.ulButtonTouched);
    CHECK(fixtureLeft.inputState==Hand::InputState::Idle&&!fixtureLeft.grabRequested&&!fixtureLeft.releaseRequested);
    menuFrame(0,0);uiInput=menuFrame(trigger|grip,grip);CHECK(uiInput.ulButtonPressed==(trigger|grip)&&uiInput.ulButtonTouched==grip);
    Service().SetInputContext(0,Now());MenuChecker::stopped=false;
    uiInput=menuFrame(trigger|grip,grip);CHECK(!uiInput.ulButtonPressed&&!uiInput.ulButtonTouched&&!fixtureLeft.grabRequested&&!fixtureLeft.releaseRequested);
    uiInput=menuFrame(0,0);CHECK(!fixtureLeft.grabRequested&&!fixtureLeft.releaseRequested);
    uiInput=menuFrame(trigger,0);CHECK(fixtureLeft.grabRequested&&!fixtureLeft.releaseRequested);
    fixtureLeft.grabRequested=false;fixtureLeft.inputState=Hand::InputState::Idle;
    fixtureLeft.triggerDown=fixtureLeft.gripDown=false;menuFrame(0,0);fixtureLeft.releaseRequested=false;
    fixtureLeft.inputState=Hand::InputState::Force;fixtureLeft.inputTrigger=true;
    fixtureLeft.forceInputTime=g_currentFrameTime;
    Service().SetInputContext(1,Now());Service().ObserveWorld(2,false,Now());Service().SetInputContext(0,Now());
    // No controller callback occurred between open/reset/close. The first native
    // visit in the new lifetime still retires old Force state; the next neutral
    // sample must not replay it either.
    uiInput=menuFrame(0,0);CHECK(!uiInput.ulButtonPressed&&!uiInput.ulButtonTouched);
    uiInput=menuFrame(0,0);CHECK(!uiInput.ulButtonPressed&&!uiInput.ulButtonTouched);
    CHECK(fixtureLeft.inputState==Hand::InputState::Idle&&!fixtureLeft.inputTrigger&&!fixtureLeft.grabRequested&&!fixtureLeft.releaseRequested);
    // Drawn equipment, holstered selection and unknown arrow use come from real
    // PublishNative source, with fake game nodes rather than asserted game layouts.
    auto owner = RegisterFixture("native-ops");
    TESObjectWEAP sword;
    sword.formID = 30;
    fixturePlayer.equipment[0] = &sword;
    PublishNative();
    CHECK(FixtureSnapshot(owner).hands[1].occupancy == HIGGS_OCCUPANCY_EMPTY);
    fixturePlayer.actorState.drawn = true;
    PublishNative();
    CHECK(FixtureSnapshot(owner).hands[1].occupancy == HIGGS_OCCUPANCY_GAME_EQUIPPED);
    sword.bow = true;
    TESAmmo arrow;
    ammo = &arrow;
    PublishNative();
    CHECK(FixtureSnapshot(owner).hands[1].availability == HIGGS_AVAILABILITY_UNKNOWN);
    CHECK(!(FixtureSnapshot(owner).hands[1].observationMask & HIGGS_OBS_EQUIPMENT_KNOWN));
    fixturePlayer.actorState.drawn = false;
    ammo = nullptr;
    PublishNative();
    TESObjectREFR wall;
    NiNode wallNode;
    wall.formID = 900;
    wall.node = &wallNode;
    forms[900] = &wall;
    hkpRigidBody wallBody;
    bhkRigidBody wallWrapper;
    wallWrapper.hkBody = &wallBody;
    wallBody.m_userData = reinterpret_cast<uintptr_t>(&wallWrapper);
    wallBody.world = &engineWorld;
    wallBody.collider.owner = &wallBody;
    wallBody.collider.m_shape = &shape;
    refs[&wallBody.collider] = &wall;
    nodes[&wallBody.collider] = &wallNode;
    rayHit = &wallBody.collider;
    auto s = FixtureSnapshot(owner);
    HIGGS_HandOperation op{};
    op.structSize = sizeof(op);
    op.kind = HIGGS_OP_WORLD_SUPPORT;
    op.physicalHand = HIGGS_HAND_LEFT;
    op.itemRefId = 900;
    op.epoch = s.epoch;
    op.handGeneration = s.hands[0].generation;
    op.durationMs = 1000;
    op.position[0] = 10;
    op.maxDistance = 20;
    uint64_t id = 0;
    CHECK(!Service().Operation(owner, op, id, Now()));
    DrainNative();
    HIGGS_HandOperationState state{};
    CHECK(!Service().CopyOperation(owner, id, state, Now()));
    CHECK(state.supportGeneration && state.supportPosition[0] > 0);
    CHECK(!(state.flags & HIGGS_OP_APPLIED));
    fixtureLeft.MoveHandAndWeaponCollision(NiPoint3(100, 0, 0));
    CHECK(keyframes == 1);
    CHECK(std::fabs(leftBody.transform.m_translation.x - state.operation.position[0] * scaleValue) < 0.00001f);
    CHECK(!Service().CopyOperation(owner, id, state, Now()));
    CHECK(state.flags & HIGGS_OP_APPLIED);
    const auto supportGeneration=FixtureSnapshot(owner).hands[0].generation;
    Service().SetInputContext(1,Now());MenuChecker::stopped=true;PublishNative();
    CHECK(FixtureSnapshot(owner).hands[0].generation==supportGeneration);
    CHECK(FixtureSnapshot(owner).hands[0].availability==HIGGS_AVAILABILITY_UNAVAILABLE);
    CHECK(Service().OperationActive(id,Now())&&HasNativeTarget(true));
    fixtureLeft.MoveHandAndWeaponCollision(NiPoint3(100,0,0));CHECK(Service().OperationActive(id,Now()));
    Service().SetInputContext(0,Now());MenuChecker::stopped=false;PublishNative();
    CHECK(FixtureSnapshot(owner).hands[0].generation==supportGeneration&&Service().OperationActive(id,Now()));
    wallBody.m_motion.m_type = hkpMotion::MOTION_DYNAMIC;
    DrainNative();
    CHECK(!HasNativeTarget(true));
    CHECK(!Service().OperationActive(id, Now()));
    // Native non-static endpoints reject rather than fabricate support success.
    s = FixtureSnapshot(owner);
    op.handGeneration = s.hands[0].generation;
    CHECK(!Service().Operation(owner, op, id, Now()));
    DrainNative();
    HIGGS_ActionResult result{};
    CHECK(!Service().Result(owner, id, result));
    CHECK(result.stage == HIGGS_ACTION_REJECTED);
    op.kind = HIGGS_OP_HAND_GEOMETRY;
    op.geometryHalfExtents[0] = 0.1f;
    op.geometryHalfExtents[1] = 0.02f;
    op.geometryHalfExtents[2] = 0.2f;
    CHECK(!Service().Operation(owner, op, id, Now()));
    DrainNative();
    CHECK(fixtureLeft.geometry.x == 0.1f);
    CHECK(!Service().EndOperation(owner, id, op.epoch));
    DrainNative();
    CHECK(fixtureLeft.geometry.x == 0.05f);
    TESObjectREFR held;
    TESForm heldBase;
    heldBase.formID = 900;
    held.baseForm = &heldBase;
    NiNode heldNode;
    held.formID = 42;
    held.node = &heldNode;
    forms[42] = &held;
    fixtureLeft.holding = true;
    fixtureLeft.selectedObject.handle = 42;
    PublishNative();
    s = FixtureSnapshot(owner);
    op.kind = HIGGS_OP_GRIP_TRANSFORM;
    HIGGS_ItemIdentity heldIdentity{};
    CHECK(!Transactions().Held(owner,HIGGS_HAND_LEFT,heldIdentity));
    NiPointer<TESObjectREFR> retained;
    CHECK(CopyNativeItemReference(heldIdentity,retained)&&retained==&held);
    auto staleIdentity=heldIdentity;++staleIdentity.generation;
    CHECK(!CopyNativeItemReference(staleIdentity,retained));
    TESObjectREFR recycled;recycled.formID=42;recycled.baseForm=&heldBase;
    forms[42]=&recycled;
    CHECK(CopyNativeItemReference(heldIdentity,retained)&&retained==&held);
    forms[42]=&held;
    op.itemRefId = 42;
    op.handGeneration = s.hands[0].generation;
    op.scale = 1;
    op.rotation[0] = op.rotation[4] = op.rotation[8] = 1;
    op.position[0] = 1;
    CHECK(!Service().Operation(owner, op, id, Now()));
    DrainNative();
    CHECK(!Service().Result(owner, id, result));
    CHECK(result.stage == HIGGS_ACTION_COMMITTED);
    CHECK(!(result.receiptFlags & HIGGS_RECEIPT_PHYSICAL_CONFIRMED));
    // Force the critic's validation/replacement interleaving directly at the
    // actual native adapter seam; retiring the old record must retain baseline.
    ValidateOperations();
    CHECK(!Service().EndOperation(owner, id, op.epoch));
    op.position[0] = 2;
    CHECK(!Service().Operation(owner, op, id, Now()));
    ConfirmNativeGrip(true);
    CHECK(!Service().Result(owner, id, result));
    CHECK(result.stage == HIGGS_ACTION_ACCEPTED);
    Service().DrainOperations(DispatchOperation, nullptr, Now());
    CHECK(fixtureLeft.GetGrabTransform().pos.x == 2);
    ConfirmNativeGrip(true);
    CHECK(!Service().Result(owner, id, result));
    CHECK(result.receiptFlags & HIGGS_RECEIPT_PHYSICAL_CONFIRMED);
    CHECK(!Service().EndOperation(owner, id, op.epoch));
    DrainNative();
    CHECK(fixtureLeft.GetGrabTransform().pos.x == 0);
    // Exercise the actual nativeItems erase predicate, not an ID-counter model.
    // Reused engine form IDs and duplicated item UIDs are distinct native refs.
    fixtureLeft.holding=false;
    PublishNative();
    uint64_t previousInstance=0;
    for(int i=0;i<300;++i){
        auto reference=new TESObjectREFR;
        reference->formID=42;reference->baseForm=&heldBase;reference->extraData.testUniqueId=17;
        forms[42]=reference;fixtureLeft.holding=true;fixtureLeft.selectedObject.handle=42;
        PublishNative();
        HIGGS_ItemIdentity currentIdentity{};
        CHECK(!Transactions().Held(owner,HIGGS_HAND_LEFT,currentIdentity));
        CHECK(currentIdentity.instance&&currentIdentity.instance!=previousInstance);
        CHECK(nativeItems.size()==1);
        CHECK(!Transactions().RetireNativeItem(currentIdentity.instance));
        previousInstance=currentIdentity.instance;
        fixtureLeft.holding=false;PublishNative();
        CHECK(nativeItems.empty());
        CHECK(!Transactions().IdentityCurrent(currentIdentity));
        forms.erase(42);delete reference;
    }
    // A released hand is not permission to retire an in-flight transaction pin.
    forms[42]=&held;fixtureLeft.holding=true;fixtureLeft.selectedObject.handle=42;PublishNative();
    HIGGS_ItemIdentity pinned{};CHECK(!Transactions().Held(owner,HIGGS_HAND_LEFT,pinned));
    uint64_t transferExecutor=0;
    CHECK(!Service().TransferExecutor(owner,[](const HIGGS_TransferExecution*,void*){},nullptr,transferExecutor));
    HIGGS_TransferRequest transfer{};transfer.structSize=sizeof(transfer);transfer.kind=HIGGS_TRANSFER_HAND_TO_INVENTORY;transfer.sourceHand=HIGGS_HAND_LEFT;transfer.epoch=Service().Epoch();transfer.idempotencyKey=9001;transfer.original=pinned;transfer.executor=transferExecutor;transfer.quantity=pinned.quantity;transfer.timeoutMs=5000;
    uint64_t transferId=0;CHECK(!Transactions().Request(owner,transfer,transferId,Now()));
    fixtureLeft.holding=false;PublishNative();CHECK(nativeItems.size()==1);CHECK(!Transactions().RetireNativeItem(pinned.instance));
    CHECK(CopyNativeItemReference(pinned,retained)&&retained==&held);
    CHECK(!Transactions().Cancel(owner,transferId,Service().Epoch()));PublishNative();CHECK(nativeItems.empty());CHECK(!Transactions().IdentityCurrent(pinned));
    // Actual native source capture: offsets scale with HMD, world radii do not.
    NiAVObject regionHmd;
    regionHmd.m_worldTransform.pos = {10,20,30};
    regionHmd.m_worldTransform.scale = 2;
    fixtureLeft.UpdateNativeRegionSelection(&regionHmd,{10,24,30},{12,20,30});
    CHECK(fixtureLeft.nativeRegionInsideMask == 3);
    HIGGS_NativeRegionFrame regionFrame{};
    CHECK(!Service().CopyNativeRegionFrame(owner,HIGGS_HAND_LEFT,regionFrame));
    CHECK(regionFrame.spheres[0].worldCenter[0] == 12 && regionFrame.spheres[0].worldRadius == 2);
    CHECK(regionFrame.spheres[1].worldCenter[1] == 24 && regionFrame.spheres[1].worldRadius == 3);
    const auto sourceGeneration = regionFrame.sourceGeneration;
    regionHmd.m_worldTransform.pos.x += 1;
    fixtureLeft.UpdateNativeRegionSelection(&regionHmd,{11,24,30},{13,20,30});
    CHECK(!Service().CopyNativeRegionFrame(owner,HIGGS_HAND_LEFT,regionFrame));
    CHECK(regionFrame.sourceGeneration == sourceGeneration);
    NiAVObject replacementHmd;
    replacementHmd.m_worldTransform = regionHmd.m_worldTransform;
    fixtureLeft.UpdateNativeRegionSelection(&replacementHmd,{11,24,30},{13,20,30});
    CHECK(!Service().CopyNativeRegionFrame(owner,HIGGS_HAND_LEFT,regionFrame));
    CHECK(regionFrame.sourceGeneration == sourceGeneration + 1);
    Config::options.mouthRadius = 2.5f;
    fixtureLeft.UpdateNativeRegionSelection(&replacementHmd,{11,24,30},{13,20,30});
    CHECK(!Service().CopyNativeRegionFrame(owner,HIGGS_HAND_LEFT,regionFrame));
    CHECK(regionFrame.sourceGeneration == sourceGeneration + 2 && regionFrame.spheres[0].worldRadius == 2.5f);
    replacementHmd.m_worldTransform.scale = 1;
    fixtureLeft.UpdateNativeRegionSelection(&replacementHmd,{11,22,30},{12,20,30});
    CHECK(!Service().CopyNativeRegionFrame(owner,HIGGS_HAND_LEFT,regionFrame));
    CHECK(regionFrame.sourceGeneration == sourceGeneration + 3);
    fixtureLeft.UpdateNativeRegionSelection(nullptr,{},{});
    CHECK(!fixtureLeft.nativeRegionInsideMask && !fixtureLeft.nativeRegionEpoch);
    Config::options.mouthRadius = 2;
    SecondaryGripFixture(owner);
    std::puts("PASS actual native-source input masking, drawn/unknown equipment, static anchor validation, keyframe "
              "target, geometry restore and grip replacement/receipt seams (not engine/game qualification)");
}
