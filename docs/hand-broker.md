# Physical hand broker and operations

The `development` source adds three independent copied C tables: [hand/input](../include/higgshandapi.h), [executor routing](../include/higgshandrouting.h), and [finite physical operations](../include/higgshandoperations.h). Resolve their `HIGGS_QueryHand*Interface` exports from `higgs_vr.dll`, requesting exact version 1 and the complete table size. Official001 bytes/vtable, scoped-restrictions v1 and velocity v1 are preserved. Capabilities describe implemented source paths, not a released or game-qualified build.

HIGGS owns hand state, hand use, input, grants and physical possession. Body Slots owns all slot mechanics and contextual eligibility; VRIK owns body/virtual poses; PLANCK owns NPC physics; PPB supplies anatomy. The sole complete concept remains [the Body Pouches English concept](https://github.com/Vhodnoylogin/body-pouches/blob/main/docs/trinity-synergy/concept-en.md). This document describes delivered HIGGS source, not another concept or agreement with framework authors.

## Observation and input

`CopySnapshot` copies the physical left/right pair under one broker lock. Native publication runs after both `PostVrikUpdate()` calls and before official001 `PostVrikPostHiggs` callbacks. Each input retains that exact pair and its observation/pose IDs. The world-space palm is HIGGS's resolved game-node palm, not a VRIK virtual-pose observation. Position uses Skyrim units; rotations are row-major. `sampledAtNs` is producer steady-clock publication time, not hardware sample time or a freshness guarantee. Consumer adapters must choose an age bound and associate their own pose sample explicitly.

Occupancy has four closed values: Empty, GameEquipped, HiggsHeld, Both. It is established only when both `HELD_KNOWN` and `EQUIPMENT_KNOWN` are present; zero fields without these flags do not prove Empty. Equipment is gated by actual drawn state, physical handedness, bow remapping and unarmed/two-handed cases. Current ammo alone cannot prove a nocked arrow: a drawn bow with selected ammo leaves its dominant arrow hand's equipment observation unknown. Known holds/items remain visible when permission is unknown. Availability is Unknown, Ready, Reserved or Unavailable. Native held objects are Reserved with no external owner/grant; stash/release candidates may reserve their use while preserving the hold. Existing secondary weapon support blocks unrelated new candidates. Restrictions/menus/tracking and missing palm evidence gate eligibility.

Grip/trigger are observed before HIGGS defaults. Recognizers execute synchronously on the controller callback thread and submit copied candidates for that input only. They must not mutate inventory or call unsafe engine functions. Selection acquires all requested hand resources atomically and calls only the assigned executor. The controller output is masked before defaults and again after HIGGS Force/leeway synthesis; a consumed press owns its held samples and matching Up even if its grant ended earlier. Other buttons observed down under that hand's grant are also suppressed until their Up. Up remains observable but cannot create a second consuming action for an already consumed press. There is no delayed hold/multi-tap recognition reservation API yet.

## User priority and routing

Merge [the example section](../config/hand-broker.ini.example) into the existing `higgs_vr.ini`:

```ini
[HandBroker]
Priorities=BodySlots=20,ExampleClimbing=50,ExamplePouchProvider=30
```

Names are case-insensitive. Larger values win. The complete override map is parsed on the ordinary HIGGS configuration load/reload; invalid/duplicate/overflow/truncated entries preserve the previous complete map and log a warning. An empty value clears overrides. There are at most 64 entries and 8190 characters. Unspecified clients use their registered fallback. Equal priorities use provider client registration order, then candidate submission order. This is an explicit registration-order fallback, not an inferred MO2 or ESP order. The end user can express a desired mod order through the overrides. `CopyInputDecision` exposes winning owner, grant, request, effective priority and the applied priority/tie policy flags.

Routed candidates use the assigned executor provider's priority, not their spatial recognizer's priority. Submit all eligible contextual candidates in the consumer's own per-slot priority order; same-provider ties preserve that order. An executor must explicitly authorize another client through `AuthorizeRecognizer` with a hand mask. The assigned executor client owns the selected grant/request and alone may report results, request physical effects or release its grant. Both recognizer and executor may copy results. The recognizer may cancel its own uncommitted routed action when contextual validation fails; it cannot undo a commitment. Revocation does not silently drop a delivered item.

## Requests, receipts and native phase

Selection reports Accepted. An external executor queues work to its own safe engine phase, revalidates epoch/grant/context, and reports Committed/Completed/Rejected/Cancelled with the exact request/executor/input/epoch. Inventory commitment, physical confirmation and compensation are independent receipt flags. Duplicate identical reports are idempotent; committed stages cannot regress. Report and result polling expire deadlines themselves, even if native updates stall. HIGGS does not invent an inventory transaction, item instance, consumption result or compensation for a provider. Default mouth/stash notifications are explicitly *requested* events, not confirmed inventory completion.

Native grab/release and existing close-selected secondary weapon support are queued for HIGGS main update before both hand updates. A requested grab cannot replace an existing hold. Release suppresses automatic mouth/stash side effects for that native update. Accepted is queue admission; Committed is native acceptance; physical completion is reported only from a matching authoritative hold/support transition. Pending acquisitions revalidate their broker authority before continuing. A release with unknown possession evidence cannot report success. Native grab/release observations carry the copied pose/sample identity; no inventory-transfer success is inferred from them.

Each queued native task is revalidated immediately before entering its mutator. Grant/registration revocation and native dispatch serialize through one phase lock. A task cancelled by an earlier callback is skipped. A native mutator already entered is a distinct started boundary: reentrant release/end returns Pending and must be retried after that phase. Expiry does not revoke authority in the middle of the native call. Physical changes already committed are not rolled back by a cancellation receipt.

The finite operation table implements these source adapters:

| Operation | Existing actuator and actual boundary |
|---|---|
| Static world support | Raycast to the named fixed reference body, retain world/reference/body/shape/node identity and source generation, publish actual hit position/normal, then pin the existing keyframed hand collision target. Revalidate endpoint identity and fixed transform under the world read lock each main update. No player locomotion or VRIK skeletal target. |
| Physical hand target | Bounded, finite world transform for an empty physical hand's collision actuator. Ending/expiry removes the target. No IK solver claim. |
| Grip transform | Existing held-node-to-hand transform; restore only for the same hold generation at the next main phase. A regrab cannot receive an old transform. |
| Drive cap | Cap the existing constrained grab motors' linear/angular maxima; original HIGGS limits still apply. Receipt follows actual motor-policy use. |
| Box hand geometry | Bounded half extents in meters, using existing hand collision removal/creation under the world lock. Empty hand only; restore default geometry at the next safe main phase. No arbitrary shapes or NPC geometry. |
| Haptic | Bounded event through the existing HIGGS haptic queue. Receipt confirms queue acceptance, never measured hardware delivery. Already queued pulses cannot be recalled; expiry never extends their bounded duration. |

Targets exclude locomotion compensation so a fixed anchor stays fixed in world space. Accepted/native validation/actuator use are separate stages; target physical confirmation is queued only after the existing hand keyframe call, not merely after copying a target. A support/target/geometry operation conflicts with native acquisition. Operations require their own client's matching grant, or acquire a new bounded hand grant. Their effects cannot outlive an inherited grant. Ending one effect does not clear another owner's restrictions or release a held item.

General new Havok constraints, moving/dynamic/NPC support endpoints, contact/motion history unification, fall/movement policy, full haptic arbitration, arbitrary hand geometry, VRIK solver goals, delayed recognizers and automatic load-order import are not supplied. The existing scoped suppression service remains separately owned/negotiated; it does not implicitly release possession.

## Lifetime, callbacks and evidence

Copied reads, registration, requests and receipts are serialized and callable from any thread. External callback code/context must remain alive until confirmed drainage. Removing a registration or unregistering outside provider callbacks drains in-flight calls before OK. From a provider callback, removal deactivates immediately and returns Pending; retry outside callbacks before unloading. Recognizer/event additions join the next input/event dispatch. Epoch resets on world replacement/unavailability and load/new game; pending requests/grants/operations invalidate, registrations survive. IDs are runtime-only and never saved. At most 64 clients, 256 registrations and 256 retained request records exist; oldest terminal unowned requests may be evicted.

Run `python tests/run_tests.py --build-dir <external-directory>` for the complete suite, or `python tests/run_hand_tests.py --build-dir <external-directory>` for this service. Checks compile the actual production broker and ABI with MSVC `/W4 /WX`, a C consumer, isolated DLL discovery, adversarial expiry/cancellation/consumed-Up cases and extracted actual native input/adapter/keyframe methods. The latter use explicitly synthetic engine seams: they check source execution and call ordering, not game layouts, collision response, engine concurrency or hardware.

An actual native compilation probe using the installed SKSE VR 2.00.12 headers and forced `common/IPrefix.h` reaches `include/RE/havok.h:3` and fails on `Common/Base/hkBase.h`. [Native prerequisites](native-build-prerequisites.md) identify the compatible Havok dependency and separate project/link prerequisites. No complete native plugin, live installation or Skyrim run is delivered by these checks.
