#include "main.h"
#include "app_lowpower.h"
#include "bsp_clock.h"
#include "bsp_power.h"
#include "bsp_rtc.h"
#include "rtc_sleep_port.h"

typedef struct
{
    LP_State_t state;
    uint32_t block_reason;
    uint32_t last_sleep_seconds;
} LP_Runtime_t;

static LP_Runtime_t s_lp_runtime;

static uint32_t LP_BuildBlockReason(void)
{
    uint32_t reason = 0U;
    uint32_t requested_period;

    if ((RtcSleep_PortIsChargerInputActive() != 0U) ||
        (RtcSleep_PortGetChargeCurrentMa() > 10U))
    {
        reason |= LP_BLOCK_CHARGE;
    }

    if (RtcSleep_PortGetDischargeCurrentMa() > 10U)
    {
        reason |= LP_BLOCK_DISCHARGE;
    }

    if ((Sci_IsAnyPortBusy() != 0U) ||
        (Can_IsBusy() != 0U))
    {
        reason |= LP_BLOCK_COMM;
    }

    if (RtcSleep_PortIsMcuWakeActive() != 0U)
    {
        reason |= LP_BLOCK_KEY;
    }

    // if (RtcSleep_PortIsAfeSleepBlocked() != 0U)
    // {
    //     reason |= LP_BLOCK_AFE_BUSY;
    // }

    if (u8FlashUpdateE2PROM != 0U)
    {
        reason |= LP_BLOCK_FLASH_BUSY;
    }

    if (u8FlashUpdateFlag != 0U)
    {
        reason |= LP_BLOCK_UPGRADE;
    }

    if ((g_stCellInfoReport.unMdlFault_Third.all != 0U) ||
        (RtcSleep_PortIsHeatActive() != 0U))
    {
        reason |= LP_BLOCK_FAULT;
    }

    requested_period = BSP_RTC_GetRequestedWakeupPeriodSeconds();
    if ((requested_period != 0U) &&
        (BSP_RTC_IsWakeupPeriodSafe(requested_period) == 0U))
    {
        reason |= LP_BLOCK_IWDG_UNSAFE;
    }

    return reason;
}

static void LP_UpdateBlockReason(void)
{
    s_lp_runtime.block_reason = LP_BuildBlockReason();
}

void LP_Init(void)
{
    s_lp_runtime.state = LP_STATE_RUN;
    s_lp_runtime.block_reason = 0U;
    s_lp_runtime.last_sleep_seconds = 0U;
}

void LP_Task(void)
{
    s_lp_runtime.state = LP_STATE_IDLE_CHECK;
    LP_UpdateBlockReason();
    rtc_sleep();
    s_lp_runtime.state = LP_STATE_RUN;
}

uint8_t LP_CanSleep(void)
{
    LP_UpdateBlockReason();
    return (s_lp_runtime.block_reason == 0U) ? 1U : 0U;
}

uint32_t LP_GetBlockReason(void)
{
    LP_UpdateBlockReason();
    return s_lp_runtime.block_reason;
}

void LP_SetWakeupPeriod(uint32_t seconds)
{
    BSP_RTC_SetWakeupPeriodSeconds(seconds);
}

void LP_BeforeSleep(void)
{
    s_lp_runtime.state = LP_STATE_PREPARE_SLEEP;
    LP_UpdateBlockReason();
}

void LP_AfterWakeup(void)
{
    s_lp_runtime.state = LP_STATE_WAKEUP_RESTORE;
    BSP_Clock_RestoreAfterStop();
    BSP_Power_RestoreAfterStop();
    s_lp_runtime.last_sleep_seconds = BSP_RTC_GetLastWakeupPeriodSeconds();
    s_lp_runtime.state = LP_STATE_RUN;
}

void LP_EnterStop(uint32_t seconds)
{
    LP_SetWakeupPeriod(seconds);
    LP_BeforeSleep();
    if (s_lp_runtime.block_reason != 0U)
    {
        s_lp_runtime.state = LP_STATE_RUN;
        return;
    }

    BSP_Power_PrepareStop(0U);
    s_lp_runtime.state = LP_STATE_STOP_SLEEP;
    BSP_Power_EnterStop();
    BSP_Power_DisableStopWakeup();
    LP_AfterWakeup();
}

uint32_t LP_GetLastSleepSeconds(void)
{
    return s_lp_runtime.last_sleep_seconds;
}

LP_State_t LP_GetState(void)
{
    return s_lp_runtime.state;
}

void LP_RecordLastSleepSeconds(uint32_t seconds)
{
    s_lp_runtime.last_sleep_seconds = seconds;
}
