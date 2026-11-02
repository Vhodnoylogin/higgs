#include "higgsphysicalpolicy.h"
#include <windows.h>
int main(int argc, char **argv)
{
    if (argc != 2)
        return 1;
    HMODULE dll = LoadLibraryA(argv[1]);
    if (!dll)
        return 2;
    HIGGS_QueryPhysicalPolicyInterfaceFn query =
        (HIGGS_QueryPhysicalPolicyInterfaceFn)(void *)GetProcAddress(dll, "HIGGS_QueryPhysicalPolicyInterface");
    HIGGS_PhysicalPolicyInterface api = {0};
    if (!query || query(1, sizeof(api), &api) || api.structSize != sizeof(api) || !api.Request || api.capabilities)
        return 3;
    FreeLibrary(dll);
    return 0;
}
