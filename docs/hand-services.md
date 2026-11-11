# Proposed hand services

When a player's hand reaches a pouch and presses grip, a mod can offer a pouch action before the ordinary grab starts. HIGGS chooses one action for that input and reserves the required hands. The pouch decides which item to return and reports what actually happened. The same mechanism allows two mods observing the same grip to cooperate. Users can set their preferred order in the [HIGGS INI](../config/hand-broker.ini.example).

## Following one interaction

1. At SKSE PostPostLoad or later, resolve the optional `HIGGS_QueryHandInterface` export from the loaded HIGGS module. Request the declared version and table size, check the status, then register a named client. Read observations using their validity bits, epoch and age. A zero item ID alone cannot establish an empty hand. [Hand ABI](../include/higgshandapi.h)
2. Register an executor for the gameplay action. The executor may authorize another client's recognizer, such as Body Slots, to propose work on its behalf. A recognizer runs synchronously during input processing. It submits its candidate with the original input identity and returns. [Routing ABI](../include/higgshandrouting.h)
3. HIGGS selects an eligible candidate by effective priority, associates the grant with the executor and suppresses the native default for the consumed press and its release. The executor can queue work for the main HIGGS phase. There it requests one authorized invocation of its callback and rechecks the target before performing the action. An invocation acknowledges dispatch. The executor reports the actual game result separately. [Deferred dispatch ABI](../include/higgsauthorizeddispatch.h)
4. Remove registrations and unregister the client before unloading. When removal from a callback returns `PENDING`, finish draining from outside callbacks before freeing callback state.

For the existing mouth and shoulder interactions, an optional spatial detector can return an inside mask for the current HIGGS sample. HIGGS retains one consume/stash/drop execution path and standalone detection when the service is absent. Arbitrary region definitions and slot contents belong to the consumer of HIGGS. [Native regions ABI](../include/higgsnativeregions.h)

## Further services

Plugins discover each table individually. Continuous world or item interactions can request hand sessions, physical operations, support, transitions and release under the same authority. Transactions distinguish accepted requests from observed inventory and physical results. PLANCK can supply NPC endpoints, and PPB can supply anatomical geometry for the native owner to apply. [Hand sessions](../include/higgshanduse.h) В· [Physical operations](../include/higgshandoperations.h) В· [Transactions](../include/higgstransactions.h) В· [Hand parts](../include/higgshandparts.h)

The production broker tests run independently of Skyrim and the SDKs:

```sh
cmake -S tests -B ../higgs-service-tests
cmake --build ../higgs-service-tests --config Release
ctest --test-dir ../higgs-service-tests -C Release --output-on-failure
```

They exercise controller arbitration and priority, grant ownership, one-shot dispatch, callback drainage and native-region fallback with synthetic observations. The public headers specify the calling conventions and lifetimes. The supplied C probe exercises table discovery from a DLL. In an x64 MSVC developer shell, compile `tests/hand_dll_probe.c` with `cl /TC /std:c11 /Iinclude tests/hand_dll_probe.c`, then run `hand_dll_probe.exe <path-to-higgs_vr.dll>`. Running-game interaction, input timing and physical behavior still need testing.

The full proposal and the current implementation record are maintained in Body Pouches. [Concept](https://github.com/Vhodnoylogin/body-pouches/blob/main/docs/trinity-synergy/concept-en.md) В· [Implementation record](https://github.com/Vhodnoylogin/body-pouches/blob/main/docs/trinity-synergy/implementation-status.md)
