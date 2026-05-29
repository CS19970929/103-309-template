#ifndef BSP_RTC_H
#define BSP_RTC_H

#include <stdint.h>

void BSP_RTC_Init(void);
void BSP_RTC_SetWakeupPeriodSeconds(uint32_t seconds);
uint32_t BSP_RTC_GetRequestedWakeupPeriodSeconds(void);
uint32_t BSP_RTC_GetWakeupPeriodSeconds(void);
uint32_t BSP_RTC_GetLastWakeupPeriodSeconds(void);
uint8_t BSP_RTC_IsWakeupPeriodSafe(uint32_t seconds);
void BSP_RTC_ConfigWakeup(void);
void BSP_RTC_DisableStopWakeup(void);

#endif
