#include "main.h"
#include "app_lowpower.h"
#include "rtc_sleep_port.h"
#include "RTC.h"

typedef struct
{
    LP_State_t state;
    uint32_t block_reason;
    uint32_t last_sleep_seconds;
    uint32_t requested_wakeup_seconds;
} LP_Runtime_t;

static LP_Runtime_t s_lp_runtime;

static uint32_t LP_BuildBlockReason(void)
{
    uint32_t reason = 0U;

    if (RtcSleep_PortGetChargeCurrentMa() > 10U)
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

    if ((StorageFlash_IsBusy() != 0U) || (u8FlashUpdateE2PROM != 0U))
    {
        reason |= LP_BLOCK_FLASH_BUSY;
    }

    if (u8FlashUpdateFlag != 0U)
    {
        reason |= LP_BLOCK_UPGRADE;
    }

    if (g_stCellInfoReport.unMdlFault_Third.all != 0U)
    {
        reason |= LP_BLOCK_FAULT;
    }

    if (LedBar_IsActiveForLowPower() != 0U)
    {
        reason |= LP_BLOCK_LED_ACTIVE;
    }

    if ((s_lp_runtime.requested_wakeup_seconds != 0U) &&
        (RTC_IsWakeupPeriodSafe(s_lp_runtime.requested_wakeup_seconds) == 0U))
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
    s_lp_runtime.requested_wakeup_seconds = 0U;
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
    s_lp_runtime.requested_wakeup_seconds = seconds;
    RTC_SetWakeupPeriodSeconds(seconds);
}

void LP_BeforeSleep(void)
{
    s_lp_runtime.state = LP_STATE_PREPARE_SLEEP;
    LP_UpdateBlockReason();
}

void LP_AfterWakeup(void)
{
    s_lp_runtime.state = LP_STATE_WAKEUP_RESTORE;
    cpu_frequency_conf();
    RtcSleep_PortRestoreAfterStop();
    s_lp_runtime.last_sleep_seconds = RTC_GetLastWakeupPeriodSeconds();
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

    RtcSleep_PortPrepareRtcStop(0U);
    s_lp_runtime.state = LP_STATE_STOP_SLEEP;
    RtcSleep_PortEnterStop();
    RtcSleep_PortDisableStopWakeup();
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
