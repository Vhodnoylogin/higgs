#include "higgshandapi.h"
#include "higgshandoperations.h"
#include "higgshandrouting.h"
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
