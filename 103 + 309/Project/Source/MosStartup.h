#ifndef MOS_STARTUP_H
#define MOS_STARTUP_H

#include <stdbool.h>
#include "stm32f10x.h"

UINT8 MosStartup_ApplyInitialState(void);

/* Keep legacy call sites readable while the module boundary is tightened. */

#endif
