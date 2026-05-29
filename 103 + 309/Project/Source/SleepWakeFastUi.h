#ifndef SLEEP_WAKE_FAST_UI_H
#define SLEEP_WAKE_FAST_UI_H

#include "stm32f10x.h"

#define SLEEP_WAKE_FAST_NONE       ((UINT8)0)

UINT8 SleepWakeFastUi_ServiceAfterStop(UINT8 sleep_mode);
UINT8 SleepWakeFastUi_ServiceStartupPreview(void);

#endif
