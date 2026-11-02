#include "higgshandapi.h"
#include "higgshandoperations.h"
#include "higgshandrouting.h"
#include <stddef.h>
_Static_assert(sizeof(HIGGS_HandRecord) == 120, "hand record x64 ABI");
_Static_assert(sizeof(HIGGS_HandSnapshot) == 280, "hand snapshot x64 ABI");
_Static_assert(offsetof(HIGGS_HandSnapshot, hands) == 40, "hand pair offset");
_Static_assert(sizeof(HIGGS_HandInterface) == 120, "hand table x64 ABI");
_Static_assert(sizeof(HIGGS_HandRoutingInterface) == 48, "routing table x64 ABI");
_Static_assert(sizeof(HIGGS_HandOperationsInterface) == 40, "operations table x64 ABI");
_Static_assert(sizeof(HIGGS_HandOperation) == 136, "operation request x64 ABI");
_Static_assert(sizeof(HIGGS_HandOperationState) == 224, "operation state x64 ABI");
_Static_assert(sizeof(HIGGS_HandInput) == 336, "input observation x64 ABI");
int hand_c_consumer(void)
{
    HIGGS_HandInterface api = {0};
    HIGGS_HandRoutingInterface routing = {0};
    HIGGS_HandOperationsInterface operations = {0};
    if (HIGGS_QueryHandInterface(1, sizeof(api), &api))
        return 1;
    if (HIGGS_QueryHandRoutingInterface(1, sizeof(routing), &routing))
        return 2;
    if (HIGGS_QueryHandOperationsInterface(1, sizeof(operations), &operations))
        return 4;
    return api.SubmitCandidate && routing.CancelRecognizedAction ? 0 : 3;
}
