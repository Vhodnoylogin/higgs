# Internal controller-motion availability

This repair concerns observed tracking failures and preserves the official 001 header and implementation bytes. The separate additive [read-only velocity ABI](velocity-observation.md) copies its existing observations without adding sampling or a freshness policy. The native HIGGS plugin remains unbuilt/unqualified without the compatible Havok headers described in [native-build-prerequisites.md](native-build-prerequisites.md).

## Policy

Every initialized pose callback publishes one observation for each physical hand. Valid samples use the existing OpenVR-to-Skyrim axis mapping and world rotation; linear velocity remains metres/second and angular velocity radians/second. The five-sample average and peak-neighbor throw calculation remain unchanged during uninterrupted valid tracking.

An observed missing/disconnected/out-of-range/invalid controller, invalid velocity or failed shared HMD/world conversion immediately clears the affected history and marks it unavailable. A shared HMD failure affects both hands; failure of one controller preserves the other. There is no time grace. The first valid sample after a failure or role-device reassignment starts from zeroed history. This deliberately changes throws across even a one-observation dropout.

Unavailable is not stationary: it cannot trigger pull/loot direction tests, mouth consumption, shoulder deposit, slow-hand finger animation or controller-speed haptics. Existing possession is not implicitly released. A requested release still executes; unavailable controller linear/angular contributions become zero, while player velocity, object-local velocity and room-motion compensation remain unchanged. Tangential calculation skips zero angular axes and degenerate center-of-mass geometry.

The pose callback has no compositor-result status or hardware sample timestamp. `observedAtNs` is local steady-clock observation time only. No same-frame, consume-once or callback-silence freshness policy is introduced. If callbacks stop without reporting a failure, the last observation remains the last observed state. This repair must not be described as a freshness guarantee or qualified in-game dropout behavior.

## Ownership and lifecycle

`HiggsMotion::ControllerTracking` owns the physical-hand pair. `Next` and `Convert` are pure numerical operations; `Commit` is the single state assignment. A mutex protects transition publication and copied reads, including a coherent linear/angular release snapshot and paired haptic reads. No lock is held while reading engine objects, querying OpenVR, converting the HMD, or calling consumers.

Existing Update world observations feed private continuity state; there is no extra engine world lookup or public world authority. World loss/change, load start and new game clear both histories. Loading blocks observations until load end and a subsequent valid world observation. An internal generation captured before engine conversion prevents an old in-flight observation from restoring data after a lifecycle reset. Role-device changes reset only that hand. Internal generation exhaustion fails closed without wrapping. Initialization is guarded before engine access.

`BeginObservation` atomically captures that generation and reserves a monotonically increasing observation ticket before any engine work. Only issued tickets newer than the last accepted observation can publish. Thus an earlier conversion finishing late cannot replace a later accepted valid **or invalid** observation, including equal observation-clock timestamps. Lifecycle resets do not reuse ticket numbers. The final uint64 ticket may publish; the next reservation attempt enters terminal unavailable state, clears both histories and rejects all outstanding tickets. Neither reset nor a new world resurrects this exhausted owner. The additive velocity ABI exposes current-scope per-hand tickets and a separately named process watermark; neither is a time-based freshness promise.

Only finite inputs/results and proper finite rotation matrices are accepted. Rotation checks use a numerical tolerance of 0.001; this is not a time/freshness threshold. Input validation and both velocity components precede one publication, so invalid angular data cannot publish a new linear sample alongside an old angular sample.

## Reproducible standalone checks

```text
python tests/run_tests.py --motion-only --build-dir <new external directory>
python tests/run_tests.py --build-dir <new external directory>
```

Both compile actual `controller_tracking.cpp` with the existing MSVC/g++ standalone tooling. The full command also runs the restriction-service core, C consumer and DLL export probe. The official001 byte audit remains active.

`controller_tracking_tests.cpp` checks pure transitions, independent hands, invalid arithmetic/conversion, identity/lifecycle continuity, ordered/reversed/duplicate/equal-time tickets, no-wrap rejection and concurrent copied observations. `motion_fixture.py` extracts bounded current production callback, load-branch and consumer bodies into the external output directory. `controller_motion_fixture.cpp` executes them against narrow engine doubles, reproduces nested newer-valid/newer-invalid callbacks during an earlier conversion, compares 200 valid observations with the actual retained `GetMaxVelocity` function, and checks unavailable admission, release components and haptics. Update world-loss/change integration is structurally audited; the complete engine Update/state machine is not executed.

The fixture models engine types, mixed-type Windows min/max and HMD conversion; it does not qualify their layouts, relocation, hook scheduling or native game behavior. The extracted pre-existing peak/animation bodies retain their signed-index and double-to-float warnings, locally suppressed only around that fixture include; other MSVC fixture warnings remain errors. No game files are read or modified and all generated/compiled artifacts stay outside Git.
