# Extended controller input and deferred recognition

`HIGGS_QueryInputInterface` adds a copied C table alongside the unchanged grip/trigger hand API. It observes a fixed OpenVR catalogue: System, Application Menu, Grip, DPad directions, A, proximity and axis buttons 32–36. Device-specific B/Y bindings normally appear through the controller's application-menu control; consumers use the user's actual control mapping rather than assuming a hardware label. System/proximity are observation-only, and no new private button types can be registered.

Each 144-byte controller record contains the complete original pressed/touched masks, packet number, five axes, actual OpenVR axis types, numeric validity, physical hand, device index, broker epoch/context, producer frame ID and steady-clock observation time. Unsupported/unknown axes do not become stationary-valid values. The latest pair is copied under one owner lock, but its left/right records retain independent sampling times. A 128-record history supports frame cursors. These are callback observation times, not hardware acquisition timestamps; silence retains history and callers must apply their own age bound.

After load or device replacement, an already-held auxiliary control is a baseline observation. It stays consumed until release rather than being invented as a new press. The existing grip/trigger interface retains its native compatibility semantics. Epoch changes invalidate input history and reservations.

## Observations and action authority

The native controller callback copies raw data before HIGGS's hand processing and uses the existing broker's serialization boundary. It then runs HIGGS and reapplies consumed auxiliary masks afterward, including the matching release frame. A selected press cannot reappear because an executor quickly completed/released its grant or a downstream stage synthesized its bit again.

Registered extended recognizers share the broker's callback leases, removal/drain protocol, executor routes, user priorities and single winner. Actionable events carry the actual broker input ID and retained hand snapshot. Legacy recognizers receive only the original effective grip/trigger inputs; the added auxiliary IDs do not leak into their ABI. Grip follows the currently configured press/touch interpretation, and that interpretation is explicit in the frame.

Raw press/release, touch/untouch, axis changes and held-duration observations are also available with `inputId=0` and without the actionable flag. Those observations do not authorize gameplay or candidate submission. Touch and axis data can drive an already authorized finite hand-use session, such as manipulating a lockpick; they do not silently consume an idle hand's input. An activation/contact-driven interaction obtains its hand-use grant through the separate hand-use API. Its B/Y or other actionable cancel input can submit a child candidate naming that same executor-owned grant. The child shares parent resources and expiry rather than acquiring a competing grant.

Callbacks never imply that engine calls are safe on the controller thread. The selected generic executor queues its domain work to its declared safe phase. `CopyRecognition` exposes proposed, waiting, matched, expired and cancelled recognition separately from the gameplay result. The assigned executor still reports its actual result and explicitly releases a finished grant, or ends its continuous hand-use session.

## Deferred hold and multi-tap

A candidate may request immediate execution, a bounded hold or 1–8 taps, with explicit duration/gap/deadline parameters. Hold recognition and tap timing live in HIGGS. The original Down participates in the normal priority round. If selected, HIGGS reserves the hand/input immediately and postpones that action's one executor notification until the gesture matches. A waiting recognition cannot authorize physical, inventory or domain mutation.

Multi-tap retains the same action and grant through its releases and subsequent taps. It does not dispatch a new competing action for every edge. The parent's deadline bounds inherited session grants. Changed grip interpretation or invalid observations, recognizer or executor removal, cancellation and expiry prevent deferred execution. Removal drains callback code; an already started routed action remains the assigned executor's responsibility.

The supported default policy is **discard after selection**. Timeout, failed recognition or cancellation keeps the affected press consumed through physical release. There is no replay capability and no speculative default followed by a second mod action. When no candidate wins and no existing use suppresses input, ordinary defaults remain available. Implementing a delayed default replay would require a separate verified engine contract and is not represented by this table.

For Simple Realistic Archery, a triple-tap on a configured catalogue control is one pending HIGGS recognition. For climbing/harvesting, a provider can reserve the initial grip while qualifying its shared action rather than privately disabling another mod's grip logic. Their actual adapters still supply domain eligibility and request the relevant HIGGS/PLANCK operations.

## Actual default-control coverage

The implemented masks cover the OpenVR controller state returned by this HIGGS callback and HIGGS's own relevant input paths. They do not establish control over every Skyrim engine path, SteamVR's dashboard or a legacy DLL that reads input outside this contract. Raw trigger masking suppresses that signal, including actions other than drawing a weapon.

[Stop Trigger Unsheathing For VR](https://github.com/adamhynek/stuf_vr/blob/master/src/main.cpp) instead edits four weapon-draw sites for Skyrim VR 1.4.15. That is a narrower engine operation. This input API does not label full trigger masking as draw-only suppression or advertise a dynamic unsheath capability.

## Verification

`python tests/run_input_tests.py --build-dir <external-directory>` compiles the actual input store and broker, checks the public C layout, hold/triple-tap/session-child protocols, cancellation/timeout and consumed-release behavior, executes the actual native controller callback against explicitly declared hand/device seams, and checks an independently loaded C DLL consumer. Full native compilation separately uses the compatible SKSE/Havok SDK. Offline checks do not qualify hardware bindings, game-default routing or gameplay quality.
