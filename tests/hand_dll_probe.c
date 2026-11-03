#include "higgsgrantmutation.h"
#include "higgshandapi.h"
#include "higgshandoperations.h"
#include "higgshandrouting.h"
#include "higgshanduse.h"
#include "higgshandusetransition.h"
#include "higgshanduseproposals.h"
#include "higgshaptics.h"
#include "higgsinput.h"
#include "higgssupport.h"
#include "higgstransactions.h"
#include <stdio.h>
#ifdef _WIN32
#include <windows.h>
int main(int argc, char **argv)
{
    HMODULE module;
    HIGGS_QueryHandInterfaceFn query;
    HIGGS_QueryHandRoutingInterfaceFn route;
    HIGGS_QueryHandOperationsInterfaceFn operation;
    HIGGS_HandInterface api = {0};
    HIGGS_HandRoutingInterface routing = {0};
    HIGGS_HandOperationsInterface operations = {0};
    if (argc != 2)
        return 1;
    module = LoadLibraryA(argv[1]);
    if (!module)
        return 2;
    query = (HIGGS_QueryHandInterfaceFn)(void *)GetProcAddress(module, "HIGGS_QueryHandInterface");
    route = (HIGGS_QueryHandRoutingInterfaceFn)(void *)GetProcAddress(module, "HIGGS_QueryHandRoutingInterface");
    operation =
        (HIGGS_QueryHandOperationsInterfaceFn)(void *)GetProcAddress(module, "HIGGS_QueryHandOperationsInterface");
    if (!query || !route || !operation)
        return 3;
    if (query(1, sizeof(api), &api) || route(1, sizeof(routing), &routing) ||
        operation(1, sizeof(operations), &operations))
        return 4;
    if (!api.ReportResult || !routing.AuthorizeRecognizer)
        return 5;
#define CHECK_TABLE(Name, Type, Fn)                                                                                    \
    do                                                                                                                 \
    {                                                                                                                  \
        Type table = {0};                                                                                              \
        Fn get = (Fn)(void *)GetProcAddress(module, Name);                                                             \
        if (!get || get(1, sizeof(table), &table) || table.structSize != sizeof(table) || table.abiVersion != 1)       \
            return 6;                                                                                                  \
    } while (0)
    CHECK_TABLE("HIGGS_QueryTransactionsInterface", HIGGS_TransactionsInterface, HIGGS_QueryTransactionsInterfaceFn);
    CHECK_TABLE("HIGGS_QueryGrantMutationInterface", HIGGS_GrantMutationInterface, HIGGS_QueryGrantMutationInterfaceFn);
    CHECK_TABLE("HIGGS_QueryHandUseInterface", HIGGS_HandUseInterface, HIGGS_QueryHandUseInterfaceFn);
    CHECK_TABLE("HIGGS_QueryHandUseTransitionInterface", HIGGS_HandUseTransitionInterface, HIGGS_QueryHandUseTransitionInterfaceFn);
    CHECK_TABLE("HIGGS_QueryHandUseProposalsInterface", HIGGS_HandUseProposalsInterface, HIGGS_QueryHandUseProposalsInterfaceFn);
    CHECK_TABLE("HIGGS_QueryHapticsInterface", HIGGS_HapticsInterface, HIGGS_QueryHapticsInterfaceFn);
    CHECK_TABLE("HIGGS_QuerySupportInterface", HIGGS_SupportInterface, HIGGS_QuerySupportInterfaceFn);
    CHECK_TABLE("HIGGS_QueryInputInterface", HIGGS_InputInterface, HIGGS_QueryInputInterfaceFn);
#undef CHECK_TABLE
    FreeLibrary(module);
    puts("PASS isolated dynamic C hand/routing DLL discovery (not native plugin)");
    return 0;
}
#else
#include <dlfcn.h>
int main(int argc, char **argv)
{
    if (argc != 2)
        return 1;
    void *module = dlopen(argv[1], RTLD_NOW);
    if (!module)
        return 2;
    HIGGS_QueryHandInterfaceFn query = (HIGGS_QueryHandInterfaceFn)dlsym(module, "HIGGS_QueryHandInterface");
    HIGGS_HandInterface api = {0};
    if (!query || query(1, sizeof(api), &api))
        return 3;
    dlclose(module);
    return 0;
}
#endif
