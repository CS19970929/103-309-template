#ifndef BATTERY_RUNTIME_CLOCK_H
#define BATTERY_RUNTIME_CLOCK_H

#include "RTC.h"

void BatteryRuntimeClock_Init(void);
void BatteryRuntimeClock_Task1s(void);
void BatteryRuntimeClock_AddSleepSeconds(UINT32 seconds);
UINT8 BatteryRuntimeClock_MarkSleepEntry(void);
UINT8 BatteryRuntimeClock_SetRtcTime(const struct RTC_ELEMENT *time);
UINT8 BatteryRuntimeClock_GetRtcTime(struct RTC_ELEMENT *time);
UINT32 BatteryRuntimeClock_GetRuntimeSeconds(void);

#endif
