# Controller input across menus

The separate `HIGGS_QueryInputContextInterface` export negotiates Context ABI v1 from [`higgsinputcontext.h`](../include/higgsinputcontext.h). The existing official001, Hand v1 and Input v1 tables and records retain their layouts. Context generation is independent of world epoch, hand generation, pose observation and the consumer's `contextGeneration`.

## Handoff

`CustomMenu` recognition follows [mikastamm's upstream PR#6](https://github.com/adamhynek/higgs/pull/6), whose patch adds that name to the two existing menu collections. The ownership, generation, drain and receipt handling below is additional fork work; neither the upstream proposal nor these offline checks establishes UI Extensions/game qualification or upstream approval.

The native menu-event handler updates the supported menu set and the broker under the same serialization fence used by controller callbacks and native mutation. The existing recognized UI menus include `CustomMenu`: it hosts modded UI and belongs to the same input exclusion as InventoryMenu and the other already-recognized interactive menus. This does not assert that those menus pause the engine. Unknown menu names and noninteractive overlays do not acquire gameplay input context. Duplicate menu events are idempotent; each supported nested open/close changes the context generation even when another UI menu remains open.

On a boundary, both hands' last observed held controls become drain sequences. A first or replaced-device sample also drains controls already held when they are first observed. Pressed and touched masks are independent; touch-based grip uses the touched sequence. Each drained control remains suppressed through its matching release sample. Other fresh UI controls pass through even when a gameplay client retains a hand grant. On return to gameplay, UI-held controls drain before a new gameplay Down can be recognized. No delayed default or synthetic replay is delivered.

Raw Input v1 observations continue in UI with no actionable input ID. Gameplay recognizers, candidate selection, deferred gesture completion and new gameplay admission are suspended. Recognition rounds also check the context generation after callbacks, including a reentrant open-and-close before the round ends. Legacy HIGGS leeway/Force history is retired at a boundary so it cannot manufacture a press, grab or release on return.

## Possession and receipts

Menu transitions preserve existing physical possession, committed support/constraints/effects and explicit hand-use sessions. Their real-time expiry and native lifetime checks continue. Consumers can end/release/cancel and renew an unchanged session. A UI context does not grant a new target or ownership.

Existing support targets and passive property/presentation policies continue native maintenance. Their Update paths reject changed targets or property values during UI; an exactly unchanged target/value set may renew or advance its revision. Comparisons use explicit fields and numeric values, not struct padding.

Undispatched, uncommitted offers, deferred recognitions and queued native starts are cancelled. A dispatched executor or entered engine mutator retains its receipt/recovery path; a menu event does not invent rollback. An uncommitted input action queued by an external executor cannot later enter the granted mutation fence using its old context generation, including after the UI closes. The executor must cancel/end it or obtain fresh contextual authority. Already committed inventory withdrawal whose queued grip is cancelled uses the existing exact-identity compensation path; compensation permits remain available in UI. An entered mutation with no known receipt remains uncertain and requires reconciliation.

## Consumer use

`CopyContext` returns the global kind, generation, active supported-menu count and current per-hand drain masks/frame IDs. `CopyFrameContext` returns the immutable generation and drain decision associated with an exact raw controller frame; the last128 samples are retained and an old world epoch is rejected. Use this association alongside Input v1's raw frame, rather than interpreting a VRIK pose sample as the menu clock. `Subscribe` reports copied before/after transitions outside the broker state mutex, serially on the transition thread. Remove and drain subscriptions through Hand v1 `RemoveRegistration`; client teardown revokes them too. A callback can remove itself safely.

These are global observed routing facts, not a guarantee that every consumer or every engine input path was masked. Coverage is the returned OpenVR controller button/touch state and HIGGS defaults. Continuous axes, hardware timestamps, unknown third-party menus, keyboard input and unrelated engine/legacy interception are outside this contract. No custom-menu implementation, SKSE callback timing, VR hardware or running-game behavior has been qualified by the offline checks.

## Evidence

An explicitly UI-authorized physical lockpicking interaction is not implemented by this increment: an ordinary gameplay session cannot bypass the UI admission gate. It needs a separately reviewed authority contract rather than a client-supplied menu exception.

`python tests/run_input_tests.py --build-dir <external-directory>` compiles the production broker/frame/context services, C consumers and a discoverable test DLL. Its native fixture executes the actual menu-event and controller-callback source through declared engine seams. It covers both-hand boundaries, initial/replaced held controls, nested and duplicate events, fresh UI input, consumed Up, deferred cancellation, reentrant context changes, stale queued mutation authority and removable callbacks.

`python tests/run_hand_tests.py --build-dir <external-directory>` additionally executes the actual `Hand::ControllerStateUpdate` source for Force/leeway retirement and typed transaction compensation after an inventory commitment. These fixtures do not substitute for a native plugin build or an in-game test. The full native build uses the documented external SDK route without installation or game startup.
