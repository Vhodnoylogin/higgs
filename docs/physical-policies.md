# Owned physical properties and hand presentation

Resolve `HIGGS_QueryPhysicalPolicyInterface`, version1 and full table size, using [the C header](../include/higgsphysicalpolicy.h). Native capability bits appear only when the real maintenance adapter is installed; portable builds report zero. Official001 and the existing operation table layout are preserved.

These are effect leases. Passive weight, grab preference and presentation never acquire input grants or consume controller presses. Each lease has an owner, epoch, deadline and revision; an optional supplied grant must remain owned. Property effects require an exact current native held-item identity and hand generation. Tracking/owner/epoch loss ends effects. CAS updates renew only live leases. `Pending`, `Active`, applied fields/revision, `Ending` and `Ended/Broken` distinguish admission, actual use and completed cleanup.

Conflicts return `CONFLICT`, with no hidden priority preemption. Mass and inertia are one coupled exclusive body resource; COM is separate. Native validation also detects aliases with different public identities. Grab preference and each presentation feature are exclusive per physical hand. Effective configured priority is copied for diagnostics; input arbitration retains its separate winner rules. Same-owner replacement requires CAS update or completed cleanup first.

## Physical properties

The native backend supports ordinary non-NPC objects currently held by HIGGS's constrained physics grab. It does not alter legacy keyframed holds, unheld/equipped objects, whole connected assemblies or PLANCK actor properties. The per-hand grab preference can request physics before a new grab; it does not convert an existing hold.

The adapter retains reference/model/collision node/wrapper/Havok body/shape/world. Under world write ownership it revalidates those identities and the same motion object/type before reads, writes or restoration. The baseline is captured once **after HIGGS's physics-grab setup**, including initial inertia fitting. `original` is that HIGGS grab baseline, not inferred NIF or pre-grab intrinsic data. Values use kg, body-local kg·m² and body-local meters. `bodyGeneration` identifies a retained lifetime, not an allocation UID.

Mass/inertia multipliers `[0.01,100]` always use that one baseline. COM offsets are additive local meters, each within ±2m. Each revision writes once; later checks never multiply current values again. The full tensor is retained; uniform inertia scaling preserves its orientation/proportions. Arbitrary tensor replacement is not offered. Resulting mass stays `[0.0001,1000000]`kg and tensor components finite with magnitude at most `1e12`.

The SDK dynamic classes override `setMass`, so independent mass/inertia semantics are not assumed. When changing mass, the adapter explicitly writes desired/preserved inertia afterward, preserves unrelated COM and reads back the result. An inertia-only write leaves a foreign mass value untouched. Ownership comparisons use exact last readbacks; target/restore validation allows SDK numerical roundoff. COM-only effects preserve other fields. Before HIGGS drop cleanup it restores matching owned fields, then HIGGS performs its existing pre-grab inertia restoration. End/expiry/reset use retained cleanup. Foreign values and stale/detached lifetimes are preserved and reported through `EXTERNAL_WRITE`. Cleanup restores each still-matching component, preserving a foreign counterpart through the coupled setter; per-component restored flags identify partial restoration. `RESTORED` means all requested fields were restored, even if an unrelated coupled component was preserved as foreign. Ending never drops possession.

## Grab preference and presentation

`FORCE_PHYSICS` enables the existing force preference for one hand; `IGNORE_GLOBAL_FORCE` ignores only `Config::forcePhysicsGrab`. Actor/constraint safety still forces physics. No global INI value is written. Ending resumes the current global preference, including a user reload during the lease.

Finger suppression gates HIGGS's `FingerAnimator::Update` before finger-node writes. Desired animation state can continue updating internally; the next permitted update samples current animation-owner transforms. Ending resumes normal HIGGS animation. This suppresses HIGGS writes; it is not another finger-pose driver or a promise that a mesh changes before normal animation updates.

Visibility changes only bit0 of the existing physical controller-node subtree's cull flags. It retains the node, preserves unrelated flags and restores the prior bit only while that same node still carries its own value. Replacement nodes/foreign edits are preserved. This does **not** hide VRIK's skinned hand meshes, alter virtual pose or hide an actor's body. Whole rendered-hand visibility remains a presentation-owner operation.

## Independent support grips

Existing `GRIP_TRANSFORM` gains negotiated capability64 for an active game-weapon secondary grip. It requires exact current secondary-support generation, actual weapon base identity and an already owned hand grant. The secondary `Hand` has its own desired transform, independent of main-hand-to-weapon state. Native validation also checks weapon identity/start time, and restores only the same lifetime while its last written value remains installed.

Cached desired state is native acceptance; physical confirmation follows the actual weapon-node actuator call. Queued work cannot confirm before installation. Ending/replacing support, weapon changes and foreign transform writes invalidate the contribution without overwriting replacements. Two HIGGS-held hands on one world object also retain independent transforms/restoration. No new arbitrary solver, driver or vtable entry is added.

## Evidence and wrapper candidates

Run `python tests/run_physical_policy_tests.py --build-dir <external-directory>` for production core, C layouts, isolated DLL discovery and extracted native-source execution on synthetic seams. `tests/run_hand_tests.py` adds secondary-grip admission/restoration cases and actuator-placement checks. Real SDK compilation is separate; fixtures do not qualify game collision response or animation.

Future technical wrappers can reuse existing motion virtual methods `setMass`, `get/setInertiaLocal`, `get/setCenterOfMassInLocal`, retained body/world checks, owned flag-bit restoration and per-hand grip actuation. No new absolute address is introduced. CommonLib migration remains separate from HIGGS ownership, conflicts, deadlines and policy.
