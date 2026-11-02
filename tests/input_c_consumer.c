#include "higgsinput.h"
#include <stddef.h>
_Static_assert(sizeof(HIGGS_ControllerFrame)==144,"C input frame layout");
_Static_assert(sizeof(HIGGS_InputInterface)==64,"C extended input table layout");
_Static_assert(HIGGS_CONTROL_MENU==1&&HIGGS_CONTROL_A==7&&HIGGS_CONTROL_TRIGGER==33,"fixed control IDs");
unsigned InputCLayout(void){return (unsigned)offsetof(HIGGS_ControllerFrame,axes);}
