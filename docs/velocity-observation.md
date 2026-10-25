# Read-only controller velocity observations

This development ABI lets another mod copy observations already owned by HIGGS, rather than maintain a second OpenVR velocity history. It adds no sampling, input broker, pose/palm/grip service, actuator, contact query or hardware tracking guarantee. It changes neither official001 nor the scoped-restriction ABI.

## Discovery and ownership

Copy `include/higgsmotion.h` and resolve `HIGGS_QueryMotionInterface` from `higgs_vr.dll` after the provider loads. Pass exact `HIGGS_MOTION_ABI_VERSION` and table capacity. The independent x64 C table is 32 bytes, its snapshot 232 bytes, and each hand 96 bytes. Pack alignment is 8. Records contain fixed-width integers and float vectors, without C++ objects or engine pointers. The known v1 layouts/sizes are immutable; later layout changes require a new ABI version. `Snapshot.abiVersion` is output only; `structSize` is the sole input field.

The table provides `CopyVelocitySnapshot` and capabilities for velocity, producer age, paired continuity and five-entry history. Initialize snapshot `structSize` to buffer capacity. Success copies known bytes and sets actual size/version; caller extension bytes remain untouched. Unsupported version, null/undersized output and internal failure leave all output untouched. Supply valid caller-owned writable buffers and stop calls before DLL unload. There is no client registration, secret, lease or authentication claim.

The existing `ControllerTracking` mutex copies both physical hands, lifecycle metadata and producer copy time together. The ABI performs no engine access, dynamic allocation, additional OpenVR query or history mutation. The numeric owner and its single `Commit` remain the state authority. Native project/export wiring is included; the complete SKSE/Havok plugin has not been built or qualified.

## Availability, time and units

The pair is physical left then physical right, independent of dominant-hand settings. `hands[0].physicalHand` is LEFT(1), `hands[1].physicalHand` is RIGHT(2); these same values are the `availableHands` mask bits, not zero/one side indices. `availableHands` means last **observed** valid state. Callback silence does not clear availability; its previous observation grows older. Numeric and age validity are separate bits. A valid stationary zero differs from an unavailable zero field.

Per-hand `acceptedObservation != 0` means an accepted observation exists in the current HIGGS continuity scope, either valid or an observed failure. Timestamp zero does not mean absence when that ticket exists. Reset/no-sample hands have zero ticket/time/numeric/count fields and no validity/source bits. Status reports only the actual stored reason. Some shared HMD/early failures store generic Unavailable; this API invents no hardware confidence or more precise diagnosis. It does not prove raw pose validity, compositor success, current palm/grip or tracking currentness.

`continuityGeneration` is HIGGS's private continuity counter. `WORLD_OBSERVED` means its existing Update path observed a nonzero world identity, not a verified engine world or shared cross-mod ID. Loading and terminal exhaustion have explicit flags. `processAcceptedObservation` is a lifetime ordering watermark that can survive a reset, **not** a current-scope sample. Per-hand tickets clear on reset; sequence numbers are never reused. Do not compare these counters with other mods' counters.

`observedAtNs` is the producer's steady-clock callback observation time, not device sample time. Source flags identify PoseCallback and NoHardwareTimestamp. The copy clock is sampled under the same owner lock. With a ticket and a nonfuture observation, `AGE_KNOWN` marks the exact unsigned difference `copiedAtNs - observedAtNs`. Otherwise age is unknown, with that bit clear and `ageNs` zero. Future time never becomes a known zero-age sample. No external-clock subtraction or default timeout is provided.

Consumers must choose an explicit maximum age, require `AGE_KNOWN`, and compare `ageNs <= chosenMaximumAgeNs` alongside availability and needed numeric bits. Different features may choose different limits. Zero and `UINT64_MAX` are ordinary explicit limits, not timeout tokens. See `tests/motion_c_consumer.c` for a C example whose limit parameter is required. Numeric availability remains independent of known age.

Linear velocity and mean speed are m/s; angular velocity is rad/s. Vectors use Skyrim world axes at observation time: existing conversion maps OpenVR x/y/z to x/-z/y, then applies the observed HMD/world rotation. These are not Havok-unit values, local body coordinates or a current-transform service. No transform version is provided.

Mean linear velocity, mean speed and peak linear velocity copy existing cached calculations. History remains zero-padded after reset/reconnect and means always divide by five. Mean speed is the mean of magnitudes. Peak uses the maximum entry at either window edge; for an interior maximum it averages that entry and its two adjacent entries (three entries total). No numeric release calculation or player/object motion changes.

`historyObservationCount` is factual metadata: zero after failure/reset, one after first valid observation or role-device change, then saturated at five. It counts consecutive accepted valid callback observations, including valid zero velocity. It is not a count of distinct hardware samples, confidence or freshness; never infer it from numeric zeros. It does not change the zero-padded averaging policy.

## Offline verification and limits

```text
python tests/run_tests.py --motion-abi-only --build-dir <new external directory>
python tests/run_tests.py --build-dir <new external directory>
```

The focused command compiles the actual numeric owner/adapter with separate C and C++ consumers. It checks negotiation/sentinels/extensions, conversion/history, stationary versus unavailable state, coherent lifecycle/pair copies, history counts, load/world reset, exact uint64 counter/age boundaries, future/zero times and caller age policy. Counter boundaries use the actual pure transition and ABI projection, without production test setters. Full checks retain restrictions and extracted production motion callback/consumer fixtures as well.

An isolated DLL/shared module links the actual owner and ABI. A separately compiled C executable discovers the unmangled query symbol and uses its copied table. Its `MotionFixtureControl` export exists only in that test module to feed synthetic events, and is absent from the production project/export list. It is not a native plugin or a consumer mutation API. External `motion-abi-manifest.json` records logs, source hashes and module hash. Local execution uses Windows MSVC; alternate runner platform paths do not constitute separately executed platform qualification.

Native build compatibility, callback scheduling, engine layouts, controllers and game behavior remain unqualified. No game, device, asset or CommonLib implementation is required or exercised by these standalone checks.
