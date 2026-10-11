#include "higgssynergy.h"
#include <windows.h>
#include <stdio.h>
#include <string.h>

/* A separately compiled C client must discover the unmangled public symbol and
   call its copied entry points without statically linking the provider. */
int main(int argc, char** argv) {
    HMODULE module;
    FARPROC symbol;
    HIGGS_QuerySynergyInterfaceFn query = NULL;
    HIGGS_SynergyInterface table;
    HIGGS_SynergyClientInfo info = { sizeof(info), 0, "dynamic C consumer" };
    HIGGS_SynergyClient client = 0;
    HIGGS_SynergyEffectiveState state = {0};
    HIGGS_SynergyLeaseRequest request = {0};
    HIGGS_SynergyRenewRequest renewal = {0};
    HIGGS_SynergyLeaseInfo lease = {0};
    unsigned char untouched[sizeof(table)];
    int result = 0;
    if (argc != 2) return 1;
    module = LoadLibraryA(argv[1]);
    if (!module) return 2;
    symbol = GetProcAddress(module, "HIGGS_QuerySynergyInterface");
    _Static_assert(sizeof(query) == sizeof(symbol), "Windows function pointer width");
    memcpy(&query, &symbol, sizeof(query));
    if (!query) { result = 3; goto done; }
    memset(&table, 0xa5, sizeof(table));
    memcpy(untouched, &table, sizeof(table));
    if (query(0, sizeof(table), &table) != HIGGS_SYNERGY_UNSUPPORTED_VERSION ||
        memcmp(untouched, &table, sizeof(table))) { result = 4; goto done; }
    if (query(HIGGS_SYNERGY_ABI_VERSION, sizeof(table), &table) != HIGGS_SYNERGY_OK ||
        table.abiVersion != HIGGS_SYNERGY_ABI_VERSION || table.structSize != sizeof(table) ||
        !(table.capabilities & HIGGS_SYNERGY_CAP_SCOPED_RESTRICTIONS)) { result = 5; goto done; }
    if (table.RegisterClient(&info, &client) != HIGGS_SYNERGY_OK || !client) { result = 6; goto done; }
    state.structSize = sizeof(state);
    if (table.CopyEffectiveState(client, &state) != HIGGS_SYNERGY_OK || state.worldReady ||
        state.hands[0].physicalHand != HIGGS_SYNERGY_HAND_LEFT ||
        state.hands[1].physicalHand != HIGGS_SYNERGY_HAND_RIGHT) { result = 7; goto unregister; }
    request.structSize = sizeof(request);
    request.physicalHands = HIGGS_SYNERGY_HAND_BOTH;
    request.flags = HIGGS_SYNERGY_SUPPRESS_HAND_INTERACTIONS;
    request.durationMs = 60000;
    request.expectedWorldGeneration = state.worldGeneration;
    lease.structSize = sizeof(lease);
    if (table.AcquireLease(client, &request, &lease) != HIGGS_SYNERGY_NOT_READY || lease.lease) {
        result = 8; goto unregister;
    }
    renewal.structSize = sizeof(renewal);
    renewal.durationMs = 60000;
    renewal.expectedWorldGeneration = state.worldGeneration;
    renewal.lease = 1;
    if (table.RenewLease(client, &renewal, &lease) != HIGGS_SYNERGY_NOT_READY ||
        table.ReleaseLease(client, 1) != HIGGS_SYNERGY_INVALID_LEASE) { result = 9; goto unregister; }
unregister:
    if (table.UnregisterClient(client) != HIGGS_SYNERGY_OK && !result) result = 10;
    if (table.CopyEffectiveState(client, &state) != HIGGS_SYNERGY_INVALID_CLIENT && !result) result = 11;
done:
    FreeLibrary(module);
    if (result) fprintf(stderr, "FAIL dynamic C DLL probe: %d\n", result);
    else puts("PASS dynamic C DLL discovery and copied entry points");
    return result;
}
