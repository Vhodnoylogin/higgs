# HIGGS architecture for VRIK HIGGS and PLANCK synergy

The proposed synergy gives Skyrim VR mods a shared foundation with clear owners:
VRIK provides the player body, spatial hand interpretation and generic zones/slots;
HIGGS brokers controller input and owns physical hand/item interactions; PLANCK
owns NPC physical animation, ragdoll lifetime and its physical-hit processing.
Consumers retain their gameplay and request services from those owners.

The single source of the full concept, including illustrative pseudo-ABI, is
[the English concept in Body Pouches](https://github.com/Vhodnoylogin/body-pouches/blob/main/docs/trinity-synergy/concept-en.md).
[The Russian version](https://github.com/Vhodnoylogin/body-pouches/blob/main/docs/trinity-synergy/concept-ru.md)
is maintained in the same directory. Body Pouches intends to adopt this
architecture. The services below are proposed changes, not an available SDK or
an agreement with the framework authors.

## Required architectural changes in HIGGS

1. **Broker grip/trigger input before actions execute.** Publish observations with
   a physical hand and sequence ID. Let VRIK and consumer recognizers submit action
   candidates before HIGGS defaults act. Resolve one consuming action using visible
   user priorities, including an explicit integration with the user's mod order.
   Hold/multi-tap reservations must respect the same priorities and have deadlines.
   Existing committed hand ownership remains protected.

2. **Replace migrated callers' anonymous restrictions with owned leases.** A mod
   requests suppression for a named hand, feature and lifetime and releases only
   its own request. This addresses conflicting uses of the current
   `DisableHand`/`EnableHand` and shared settings. Suppression and an explicit item
   release must be separate operations.

3. **Provide requests with confirmed results.** Grab, release, transfer and grip
   changes need request IDs and distinct accepted, committed, completed, rejected
   and cancelled stages. Coordinate both hands and item-instance ownership before
   mutation. For consumer gameplay, report the assigned domain executor's result;
   forwarding it does not transfer inventory or damage authority to HIGGS.

4. **Compose physical contributions through one final actuator per owned body.**
   Offer supported grip, drive and constraint requests so weight, collision,
   penetration and climbing mods do not overwrite each other's transforms.
   Report effective acquisition, saturation, break and release. PLANCK validates
   NPC attachment endpoints; HIGGS applies hand/weapon constraints. Player
   locomotion and fall rules remain with the climbing consumer.

5. **Publish coherent copied state.** Expose possession, grip geometry, motion and
   contacts with source, units, coordinate space, time, frame/physics IDs and body
   generations. Distinguish tracked controllers, resolved palms and VRIK's rendered
   hands. Include the version of the tracking-to-world transform.

6. **Add a versioned lifecycle contract.** Keep interface 001 unchanged. Negotiate
   new capabilities separately, assign owners and epochs, and support callback
   removal with confirmed drainage. Apply physical changes at documented safe
   phases and invalidate pending resources on load or provider shutdown.

The current [001 interface](../include/higgsinterface001.h) and
[implementation](../src/pluginapi.cpp) expose the legacy callbacks, settings and
hand toggles; [the hooks](../src/hooks.cpp) expose update points around VRIK.
These are integration points, rather than proof that the proposed contract exists.
Reuse verified CommonLib engine primitives; the runtime ownership broker belongs
in HIGGS, since linking the library into several DLLs does not share state.

With this service, VR Climbing requests hand support and Immersive Harvesting
requests a harvesting action under one arbitration contract. They can stop
recreating shared HIGGS restrictions and patching each other for that conflict.
