#ifndef SLEEP_WAKE_FAST_UI_H
#define SLEEP_WAKE_FAST_UI_H

#include "stm32f10x.h"

#define SLEEP_WAKE_FAST_NONE       ((UINT8)0)
#define SLEEP_WAKE_FAST_TIMEOUT    ((UINT8)1)
#define SLEEP_WAKE_FAST_BOOT       ((UINT8)2)
#define SLEEP_WAKE_FAST_CHARGE     ((UINT8)3)

UINT8 SleepWakeFastUi_ServiceAfterStop(void);

#endif
