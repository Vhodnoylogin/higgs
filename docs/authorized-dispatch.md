# Authorized deferred dispatch v1

`HIGGS_QueryAuthorizedDispatchInterface(1, sizeof(HIGGS_AuthorizedDispatchInterface), &table)` negotiates the independent copied C table in `include/higgsauthorizeddispatch.h`. Existing official001, hand, routing, input and grant-mutation tables retain their layouts and behavior. The production broker owns this service; consumers need no raw controller hook or separate input arbitration.

An ordinary executor callback can copy a selected `HIGGS_ActionRequest` into its own bounded queue. It must not perform unsafe engine work on the controller callback thread. At a HIGGS main-maintenance phase, its owner calls `Invoke(client, &request, callback, context)` using the selected request id, executor, epoch and grant. The table does not create an action, arbitrate again, acquire another grant, or invoke the registered selection callback a second time. The supplied callback is the separately deferred operation.

## Admission and ownership

The calling client must own the assigned executor and selected action. A routed recognizer does not gain the executor owner's authority. Existing routing revocation, client/registration removal, input-context transitions, deadline and load rules continue to invalidate uncommitted authority.

`Invoke` requires HIGGS's observed main-maintenance thread, a ready world without a pending reset or supported menu context, a live executor and an active selected nonterminal grant. A pending hold/multi-tap recognition cannot invoke. An uncommitted input action must still belong to the current input-context generation. The request epoch and grant must match the action and current snapshot. Every required physical hand must currently carry that owner's grant, be reserved with no unavailable reasons, have known held/equipment/palm/tracking observations, and have no secondary-support observation. Held-item occupancy alone is permitted. The copied snapshot is current broker evidence at entry; this API adds no hardware timestamp or callback-silence freshness guarantee.

The provider still checks its own domain evidence immediately before effects: region/binding revision, source identity/generation, pose/anchor age, item policy and other applicable preconditions. A HIGGS grant does not validate arbitrary caller-provided spatial data or domain policy.

## One claim and a synchronous fence

Under HIGGS's existing native-phase fence, the broker atomically marks the action's dispatch claim before calling the supplied callback. The callback receives copied `HIGGS_ActionRequest` and `HIGGS_HandSnapshot` records and runs outside the broker state mutex. `CopySnapshot` and ordinary child physical/transaction submission remain usable. Request storage is copied before callback entry; no callback or context pointer is retained after return.

There is one dispatch claim per request. Reentrant or later duplicate invocation returns `HIGGS_HAND_CONFLICT`, including after callback failure, exception or subsequent retirement of the grant. The claim is retained with the existing bounded action history; an evicted request returns `HIGGS_HAND_NOT_FOUND`. Invalid admission does not claim an action, so a caller may retry after a recoverable admission fault.

An entered callback prevents grant expiry, revocation, owner/assigned-executor removal and world reset from invalidating that action until return. Reentrant same-grant release/revocation/removal and `ReportResult` return `PENDING`; callers must retry the intended operation after the callback. External operations serialize on the same native-phase fence. Deferred world changes and ordinary deadlines are drained after callback exit, including exception exit.

## Invocation is not a gameplay receipt

`Invoke` returns the callback's status; a C++ exception is contained and returns `HIGGS_HAND_INTERNAL_ERROR`. Neither success nor failure changes the action result to committed, completed or rejected, creates receipt flags, or promises rollback. The dispatch claim is consumed even if the callback partially changed external state before failing. Never retry arbitrary gameplay effects because the callback failed. Existing lifecycle cancellation remains possible after return; cancellation alone does not prove that no external effect occurred.

For asynchronous native work, queue the existing `RequestPhysical` or transaction operation under the selected grant and retain its factual child result. Submission acceptance is not completion. Report factual parent results after `Invoke` returns; the parent is not automatically completed by the child or this table. Inventory mutation continues to use the transaction coordinator, item identities and permits.

For a synchronous actual domain commit, `WithAuthorizedAction` remains the separate commit fence that records the requested commit stage before releasing authority. Use the mechanism matching the effect; invoking an arbitrary callback does not substitute for a transaction or a factual commit receipt.

## ABI and verification

The v1 request is 40 bytes (`structSize`, zero `reserved`, request id, executor, epoch and grant). The x64 table is 16 bytes (`structSize`, `abiVersion`, `Invoke`). Request size must match exactly. Query rejects unsupported versions, null output and undersized tables without changing output. Callers provide valid, stable accessible memory and stop calls before DLL unload. C callbacks must not let exceptions cross the ABI.

The production authority fixture covers one-shot success/failure/exception, reentry, child physical submission, absence of invented receipts, factual reporting after return, thread/owner/scope/grant/resource rejection, input-context retirement, and entered-callback expiry/load drainage. The C consumer and isolated DLL probe check the copied layout and negotiated export. Run the existing offline fixture with `python tests/run_hand_tests.py --build-dir <external-directory>`. These checks do not establish native plugin, running-game or controller behavior.

The native mouth/stash spatial detector and its standalone fallback remain independently optional. This service adds no spatial detector or consume/stash/drop executor; the existing native and transaction paths retain their effect ownership.
