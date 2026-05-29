#include "main.h"
#include "bsp_power.h"
#include "rtc_sleep_port.h"

void BSP_Power_PrepareStop(uint32_t rtc_cycle_count)
{
    RtcSleep_PortPrepareRtcStop(rtc_cycle_count);
}

void BSP_Power_EnterStop(void)
{
    RtcSleep_PortEnterStop();
}

void BSP_Power_DisableStopWakeup(void)
{
    RtcSleep_PortDisableStopWakeup();
}

void BSP_Power_RestoreAfterStop(void)
{
    RtcSleep_PortRestoreAfterStop();
}
