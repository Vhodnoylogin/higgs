# Scoped hand and weapon collision restrictions

This development implementation supplies the first ownership primitive from the
[HIGGS cooperation concept](https://github.com/Vhodnoylogin/body-pouches/blob/main/docs/trinity-synergy/concept-en.md):
a consumer removes only its own temporary restrictions. For example, an old
consumer calling `EnableHand(true)` cannot clear a scoped restriction that a
climbing or harvesting consumer still owns.

The service controls new HIGGS hand interactions and weapon collision policy.
Overlapping restrictions are permitted and combined. A successful lease is not
exclusive ownership of a committed action, possession, grip input or a surface.
There is no input broker, action priority, release/transfer operation, contact
query, locomotion, or gameplay executor in this interface.

## Compatibility and discovery

`include/higgssynergy.h` is an independent C-compatible header. The official
`include/higgsinterface001.h`, its companion getter source and its vtable are
unchanged. Existing consumers continue using the 001 interface. No PLANCK
consumer changes are required.

Resolve `HIGGS_QuerySynergyInterface` from `higgs_vr.dll` with `GetProcAddress`
after SKSE `PostPostLoad`. Missing DLL/export means the optional service is
unavailable; never assume the original author's binaries contain it. Request
exact `HIGGS_SYNERGY_ABI_VERSION` (currently **1** for this separate service) and
pass a caller-owned table buffer. An unsupported version, null output, or short
table buffer returns a defined status without writing the table. Success copies
only the known table bytes, reporting its version, size, capability and limits.
Retain the copied function table only while the HIGGS module remains loaded.

Records contain fixed-width integers, copied names and opaque runtime handles.
There are no engine pointers, callbacks, STL objects, vtables or retained caller
buffers in the new ABI. Function pointers in the copied table are provider entry
points, not consumer callbacks. All calls use `__cdecl` on Windows; this service
targets the game's x64 process. Record packing is explicitly 8 bytes.

Initialize every input/output record's `structSize`. Inputs must contain at
least the known v1 bytes; unknown flags and nonzero reserved input fields are
rejected. Extra caller buffer bytes are untouched. A short output record must
still provide an aligned, writable 4-byte size field: `BUFFER_TOO_SMALL` writes
the required record size there and no other bytes. All pointers must remain
valid for their synchronous call. Outputs from failed calls are not usable.

## Client and lease lifetime

Register a nonempty NUL-terminated name (at most 63 bytes) to obtain a runtime
client handle. Names are copied descriptions; the returned handle is the
authority. Re-registering an identical name creates a distinct client. Handles
are opaque, never recycled during the provider's process lifetime, and must not
be persisted in save files or shared with other clients. This is a cooperation
contract between plugins in the same process, not a security boundary.

There are at most **64** clients and **256** live lease records. One lease
atomically covers physical left, physical right, or both, using one record.
Physical left corresponds to HIGGS's `isLeft == true`; neither mask depends on
the game's dominant-hand setting. A failed request grants no subset of its
footprint. Concurrent requests serialize against acquisition, renewal, release,
expiry, unregister and lifecycle reset.

Leases last **1 to 60,000 milliseconds**, measured by the provider's monotonic
wall clock. Pausing the game does not pause this deadline. Long use needs a
renewal heartbeat before expiry; for example, request 60 seconds and renew every
10 seconds. `RenewLease` requires the same exact owner and current world
generation, preserves the token, footprint and flags, and starts the requested
duration at the renewal's provider time. It cannot resurrect an expired lease.
The copied expiry and snapshot sample time use the same clock domain; no caller
clock or wall-clock calendar conversion is required.

`ReleaseLease(client, lease)` releases only that client's live lease. A foreign
owner returns `WRONG_OWNER`; an expired, released or revoked token returns
`INVALID_LEASE`. `UnregisterClient` atomically revokes all that client's leases
and makes its handle invalid. Clients must stop and join their own calling
threads before unregistering and unloading their module. The provider retains
no consumer code/context pointers and does not invoke consumers.

## Effective policy

| Request flag | Effect |
|---|---|
| `SUPPRESS_HAND_INTERACTIONS` | Prevent new selection, grabs, pulls and two-hand admission for the named physical hands |
| `SUPPRESS_WEAPON_COLLISION` | Disable HIGGS weapon collision for the named physical hands |
| `FORCE_WEAPON_COLLISION` | Bypass HIGGS's temporary post-hit collision disable window while the lease is live |

For each hand, effective interaction suppression is **legacy DisableHand OR any
live scoped hand suppression**. Old `EnableHand` clears only its legacy bit.
A scoped release clears only its own lease. Existing legacy consumers still
share a single legacy bit and can clear each other's legacy-only requests; the
new interface cannot assign ownership to calls that supplied no owner.

Weapon collision suppression is **legacy disable OR any live scoped collision
suppression**. Force is **legacy permanent force OR any live scoped force**,
effective only when collision is not suppressed. Suppression takes precedence,
regardless of registration or request order. Sheathed weapons and unavailable
weapon bodies retain HIGGS's original behavior. With no scoped leases, the old
collision behavior is unchanged, including permanent
`ForceWeaponCollisionEnabled`.

Scoped hand suppression affects admission. It does not drop a held object,
break an established two-hand relation, release another hand's item, or undo
earlier inventory/game-script side effects. Existing legacy `DisableHand` keeps
its existing held-object drop behavior. Pending selection/pull/loot or spawn
admission is rechecked before it creates a new grip. User release and equipment
changes keep their normal behavior. No automatic transfer/release is promised.

A pending nonphysics transfer from the other hand can already have scheduled the donor's release before the receiving hand is checked on a later update. Cancelling that receiver under a new restriction does not roll back the earlier transfer side effect. Consumers that need to keep both hands unchanged on rejection require the later correlated transfer/commit protocol; this admission service alone cannot provide that guarantee.

## Snapshots, generations and lifecycle

`CopyEffectiveState` copies both hands under the same lock. It returns readiness,
state generation, world generation, sample time, effective flags, legacy flags,
scoped flag union and live lease counts. Arrays always mean `[0] physical left`
and `[1] physical right`. Copying requires a live registered client and succeeds
when the world is unavailable, allowing consumers to inspect readiness.

Before acquisition/renewal, copy the current world generation and pass it as
`expectedWorldGeneration`. Old-world delayed requests return `STALE_WORLD` when
the new world is ready. Acquisition/renewal while unavailable returns
`NOT_READY`. State generation advances on mutations and lazy expiry. Refresh
snapshots when making decisions; copied state is an observation, not an engine
transaction or an exclusive reservation.

SKSE `PreLoadGame` immediately revokes scoped leases and closes lease acquisition and renewal.
`PostLoadGame` ends that gate; the next valid HIGGS world observation opens the
service in a new generation even if a native world address was reused. `NewGame`,
world replacement, and absence of the player/cell/world revoke scoped leases.
An old world seen during a pending load cannot reopen the service. Registered
clients and all legacy state survive these boundaries. Scoped leases are never
serialized; consumers must observe the new generation and reacquire as needed.

API calls mutate only the bounded service state. HIGGS samples it and applies
admission/collision policy at its normal game update points. The service mutex
is released before touching engine objects or dispatching existing callbacks.
Calls can overlap from plugin threads; each copied snapshot is coherent, while
engine policy takes effect at the next corresponding HIGGS update. There is no
claim that a successful call synchronously changed a Havok body.

## Minimal consumer sequence

1. Resolve/query the copied table and check its capability.
2. Register a client and copy effective state; wait for `worldReady`.
3. Acquire a lease with the current world generation, physical-hand mask, flags,
   and bounded duration. Check the returned status.
4. Renew the exact lease before expiry during long use; refresh generation after
   lifecycle changes. A failed renewal requires a new ready-state observation.
5. Release that lease when the feature ends. Stop/join callers and unregister the
   client before module unload.

## Integration and verification

Production sources are `src/synergy_core.cpp` and `src/synergy_runtime.cpp` and
are listed in the native Visual Studio project. The export is in `exports.def`.
The actual 001 setters/getters route through the shared service. `CanHoldObject`
and `CanTwoHand` gate admission; pending selection/spawn paths revalidate it.
The asynchronous `PrePullItem` spawn path also revalidates admission before changing ownership, damping or emitting a new pulled callback; an already committed pull remains a distinct state.
Held continuation uses the separate legacy-only policy. `UpdateWeaponCollision`
uses one copied policy sample to calculate the actual weapon-body filter.
`Update` and the SKSE listener provide lifecycle observations.

Run:

```text
python tests/run_tests.py --build-dir <external-output-directory>
```

The runner compiles the actual production service and export adapter with a
separately compiled C consumer. Nine suites exercise overlap/foreign release,
legacy isolation, held-item preservation, deterministic collision policy,
renewal/expiry, all-or-none capacity, stale handles, world/game reset, concurrent
owners and client revocation, plus ABI negotiation and caller buffer guards.
It also guards canonical official-001 bytes (allowing checkout newline
conversion), the native project/export, and native policy/lifecycle callsites.
MSVC x64 C11/C++20 checks pass with `/W4 /WX`.
On Windows the runner also builds the same provider sources as a standalone
DLL and uses a separately linked C program to load the exact export by name,
negotiate its table and call all six copied entry points. Its unobserved world
correctly refuses new leases. This DLL is a service probe, not a HIGGS plugin.

The whole native plugin remains **unbuilt and untested in game**. The existing
project's relative SKSE/common dependencies and original author's Havok include
path are absent in this checkout; an attempted Release x64 build with deployment
disabled stops on the missing `common/IPrefix.h` and SKSE project. Standalone
module and structural integration checks do not establish hook/runtime
qualification. No game files were installed or launched for these checks.
