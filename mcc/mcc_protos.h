#ifndef MCC_PROTOS_H
#define MCC_PROTOS_H

#include <exec/types.h>
#include "mcc.h"

#ifdef __cplusplus
extern "C" {
#endif

ULONG MCC_Query(register __d0 LONG which, register __a6 struct WorldmapBase *base);

#ifdef __cplusplus
};
#endif

#endif
