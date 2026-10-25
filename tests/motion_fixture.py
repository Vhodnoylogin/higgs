"""Extract bounded production callback/consumer bodies for a Havok-free fixture.

The generated file stays in the external build directory. Engine conversion/types
are test doubles; their Skyrim implementation/layout is not being qualified here.
"""
from pathlib import Path


def block(source, marker):
    start = source.index(marker)
    opening = source.index("{", start)
    depth = 1
    end = opening + 1
    while depth:
        depth += (source[end] == "{") - (source[end] == "}")
        end += 1
    return source[start:end]


def generate(root, output):
    main = (root / "src/main.cpp").read_text()
    hand = (root / "src/hand.cpp").read_text()
    physics = (root / "src/physics.cpp").read_text()
    header = (root / "include/hand.h").read_text()
    parts = [block(header, "inline NiPoint3 TrackedVelocityPoint")]
    for marker in ("HiggsMotion::Rotation TrackingRotation", "void ObserveControllerWorld(",
                   "void ControllerTrackingLifecycle(", "void FillControllerVelocities(", "bool WaitPosesCB("):
        parts.append(block(main, marker))
    for marker in ("NiPoint3 GetMaxVelocity(", "NiPoint3 Hand::GetHandVelocity(",
                   "bool Hand::IsObjectDepositable(", "bool Hand::IsObjectConsumable("):
        parts.append(block(hand, marker))
    pull = block(hand, "if (!isSelectedNear && tracking.available)")
    parts.append("void Hand::ProbePull() { const auto tracking = HiggsMotion::Tracking().Copy(isLeft);\n" + pull + "\n}")
    loot_start = hand.index("const auto tracking = HiggsMotion::Tracking().Copy(isLeft);", hand.index("if (state == State::LootOtherHand)"))
    loot = block(hand[loot_start:], "if (tracking.available)")
    parts.append("void Hand::ProbeLoot() { Hand& other=*this; NiPoint3 palmPos,palmVector; int worldValue=0; int* world=&worldValue; const auto tracking = HiggsMotion::Tracking().Copy(isLeft);\n" + loot + "\n}")
    animation = block(hand, "if (const auto tracking = HiggsMotion::Tracking().Copy(isLeft); tracking.available &&")
    parts.append("void Hand::ProbeAnimation() {\n" + animation + "\n}")
    release_start = hand.index("// One coherent copy for controller linear/angular contribution.")
    release_end = hand.index("if (state == State::HeldBody)", release_start)
    parts.append("ReleaseResult Hand::ProbeRelease() {\n" + hand[release_start:release_end] +
                 "return { totalVelocity, angularVelocity, tangentialVelocity, collideWithHandWhenLettingGo };\n}")
    body_start = hand.index("NiPoint3 velocityObjectComponent = GetMaxVelocity(selectedObject.localLinearVelocities);", release_end)
    body_end = hand.index("bhkRigidBody_setActivated", body_start)
    parts.append("NiPoint3 Hand::ProbeBodyRelease() {\n" + hand[body_start:body_end] + "return velocityObjectComponent;\n}")
    parts.append(block(physics, "void TriggerCollisionHapticsUsingHandSpeed("))
    # Execute actual load-branch statements, without unrelated plugin initialization.
    for name in ("PreLoadGame", "PostLoadGame", "NewGame"):
        branch = block(main, f"else if (msg->type == SKSEMessagingInterface::kMessage_{name})")
        parts.append(f"void Probe{name}() " + branch[branch.index("{"):])
    update = block(main, "void Update()")
    if update.count("ObserveControllerWorld(0);") != 3 or update.count("ObserveControllerWorld(reinterpret_cast<std::uintptr_t>(world.operator->()));") != 1:
        raise RuntimeError("World-loss/change integration missing")
    if "controllerData" in hand or "controllerData" in physics or "ControllerTrackingData" in header:
        raise RuntimeError("A consumer still reaches mutable controller history")
    if hand.count("tracking.available && tracking.averageSpeed < Config::options.selectedCloseFingerAnimMaxHandSpeed") != 2:
        raise RuntimeError("Both animation consumers require explicit availability")
    (output / "motion_source.inc").write_text("\n\n".join(parts) + "\n", encoding="utf-8")
