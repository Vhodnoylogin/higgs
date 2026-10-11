# HIGGS architecture for VRIK HIGGS and PLANCK synergy

The proposed synergy gives Skyrim VR mods a shared foundation with clear owners. HIGGS brokers grip/trigger input and owns physical hand/item interactions. VRIK interprets hand position, maps anchors onto the actual player body/equipment, handles spatial interactions and renders. PLANCK owns NPC physical animation, ragdoll lifetime and physical-hit processing. Consumers keep their gameplay, inventory-selection and equipment-proxy rules.

A separate slot service, **Body Slots** as a working name, owns slot creation, stable definitions/instances, local placement and access geometry, generic allowed-content policies and saved customization. Its main workflow is an offline visual editor on a reference body/skeleton/equipment model → editable project and versioned declarative package → slot runtime/API → VRIK binding, hand detection, interaction and presentation. Authoring/export requires neither Skyrim nor VRIK. Keeping the native 14 holsters is an optional compatibility path. The editor and new services are proposals, not implemented features.

The only maintained source of the full concept and illustrative pseudo-ABI is [the English concept in Body Pouches](https://github.com/Vhodnoylogin/body-pouches/blob/main/docs/trinity-synergy/concept-en.md), with [a Russian counterpart](https://github.com/Vhodnoylogin/body-pouches/blob/main/docs/trinity-synergy/concept-ru.md). This file records HIGGS-specific requirements, not another copy of the concept. Body Pouches intends to adopt the architecture; it is not an available SDK or an agreement with the framework authors.

## Required architectural changes in HIGGS

1. **Broker grip/trigger before actions execute.** Publish observations with physical hand and sequence ID. Let VRIK and consumer recognizers propose actions before defaults act. Resolve one consuming action using visible user priorities, including an explicit integration with user mod order. Hold/multi-tap reservations respect those priorities and have deadlines; existing committed hand ownership stays protected.

2. **Replace anonymous restrictions with owned leases.** Consumers request suppression for a named hand, feature and lifetime and release only their own effects. This replaces migrated uses of shared settings and `DisableHand`/`EnableHand`. Suppression and explicit item release remain separate operations.

3. **Provide correlated requests and confirmed results.** Grab, release, transfer and grip changes distinguish accepted, committed, completed, rejected and cancelled. Coordinate affected hands and item-instance ownership before mutation. Forward the assigned domain executor's receipt without taking over inventory or damage policy. Inventory commitment and physical grip are separate results; retries must not create duplicate items.

4. **Compose physical contributions through one final actuator per owned body.** Support grip, drive and constraint requests for weight, collision, penetration and climbing. Report effective acquisition, saturation, break and release. PLANCK validates NPC endpoints; HIGGS applies hand/weapon constraints. Player locomotion and fall rules remain with the climbing consumer.

5. **Publish coherent copied state.** Expose possession, grip geometry, motion, contacts and release observations with source, units, space, time, frame/physics IDs and body generations. Distinguish tracked controller, resolved palm and VRIK-rendered hand; identify the tracking-to-world transform and the sample VRIK should test against a zone.

6. **Respect the slot-service/runtime boundary.** Slot definitions and customization belong to Body Slots; current spatial offers, reservations and presentation readiness come from VRIK. HIGGS arbitrates input/hand resources and performs physical requests, while the content executor chooses and commits the item. Zone entry or a visible preview is not proof of possession. Correlate the slot configuration/content version and runtime generation used by an action; an unavailable or rebound slot invalidates uncommitted work without silently releasing an already delivered item. Generic slot filters must not become HIGGS inventory policy.

7. **Add a versioned lifecycle contract.** Keep interface 001 unchanged and negotiate new capabilities separately. Use copied records, request/executor identities and epochs; support callback removal with confirmed drainage. Apply physical changes at documented safe phases and invalidate pending resources on load, tracking loss or provider shutdown. The offline editor has no runtime dependency on HIGGS.

The current [001 interface](../include/higgsinterface001.h), [API implementation](../src/pluginapi.cpp) and [hooks](../src/hooks.cpp) are integration points, not proof that these services exist. Internal repairs still require their own source/build/runtime verification.

CommonLib is the second synergy level: reuse existing engine primitives and move address handling and repeated technical operations into verified library methods. Input arbitration and physical-possession decisions remain HIGGS services. Under that shared contract, VR Climbing and Immersive Harvesting can request hand actions without recreating common restrictions or patching each other's reservation mechanisms.
