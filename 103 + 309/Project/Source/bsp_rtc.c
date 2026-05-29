#include "main.h"
#include "bsp_rtc.h"

static uint32_t s_u32BspRtcRequestedWakeupSeconds = 0U;

void BSP_RTC_Init(void)
{
    Init_RTC();
}

void BSP_RTC_SetWakeupPeriodSeconds(uint32_t seconds)
{
    s_u32BspRtcRequestedWakeupSeconds = seconds;
    RTC_SetWakeupPeriodSeconds(seconds);
}

uint32_t BSP_RTC_GetRequestedWakeupPeriodSeconds(void)
{
    return s_u32BspRtcRequestedWakeupSeconds;
}

uint32_t BSP_RTC_GetWakeupPeriodSeconds(void)
{
    return RTC_GetWakeupPeriodSeconds();
}

uint32_t BSP_RTC_GetLastWakeupPeriodSeconds(void)
{
    return RTC_GetLastWakeupPeriodSeconds();
}

uint8_t BSP_RTC_IsWakeupPeriodSafe(uint32_t seconds)
{
    return RTC_IsWakeupPeriodSafe(seconds);
}

void BSP_RTC_ConfigWakeup(void)
{
    RTC_WKTimeConfig();
}

void BSP_RTC_DisableStopWakeup(void)
{
    RTC_DisableStopWakeup();
}
