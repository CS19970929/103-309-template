#include "main.h"
#include "LowPowerSleep.h"
#include "rtc_sleep_afe_port.h"
#include "rtc_sleep_port.h"

#define RTC_SLEEP_ACTIVE_CURRENT_A10 ((UINT16)5U)

UINT8 RtcSleep_PortIsOneSecondTick(void)
{
    return g_st_SysTimeFlag.bits.b1Sys1000msFlag;
}

UINT16 RtcSleep_PortGetCellMinMv(void)
{
    return g_stCellInfoReport.u16VCellMin;
}

UINT16 RtcSleep_PortGetChargeCurrentA10(void)
{
    return g_stCellInfoReport.u16Ichg;
}

UINT16 RtcSleep_PortGetDischargeCurrentA10(void)
{
    return g_stCellInfoReport.u16IDischg;
}

UINT16 RtcSleep_PortGetLowVoltageSleepMv(void)
{
    return OtherElement.u16Sleep_Vlow;
}

UINT32 RtcSleep_PortCollectBlockReason(volatile UINT8 *external_comm_snapshot)
{
    UINT32 reason = 0U;
    UINT8 comm;

    if (RtcSleep_PortGetChargeCurrentA10() >= RTC_SLEEP_ACTIVE_CURRENT_A10)
    {
        reason |= LP_BLOCK_CHARGE;
    }
    if (RtcSleep_PortGetDischargeCurrentA10() >= RTC_SLEEP_ACTIVE_CURRENT_A10)
    {
        reason |= LP_BLOCK_DISCHARGE;
    }
    if (Sci_IsAnyPortBusy() || Can_IsBusy())
    {
        reason |= LP_BLOCK_COMM;
    }
    if (RtcSleep_PortIsMcuWakeActive() != 0U)
    {
        reason |= LP_BLOCK_KEY;
    }
    if (StorageFlash_IsBusy() || (u8FlashUpdateE2PROM != 0U))
    {
        reason |= LP_BLOCK_FLASH_BUSY;
    }
    if (u8FlashUpdateFlag != 0U)
    {
        reason |= LP_BLOCK_UPGRADE;
    }
    if ((g_stCellInfoReport.unMdlFault_Third.all != 0U) ||
        (g_stCellInfoReport.unMdlFault_Second.all != 0U) ||
        (SH367309_Reg_Store.REG_BSTATUS1.bits.SC != 0U))
    {
        reason |= LP_BLOCK_FAULT;
    }

    comm = SleepDeal_GetExternalCommCounter();
    if ((external_comm_snapshot != 0) && (comm != *external_comm_snapshot))
    {
        *external_comm_snapshot = comm;
        reason |= LP_BLOCK_EXT_COMM;
    }

    return reason;
}

UINT8 RtcSleep_PortIsMcuWakeActive(void)
{
#if PROJECT_CFG_DI_SWITCH_LONGKEY_ONOFF_ENABLE
    return (UINT8)(MCUI_ENI_DI1 == 0);
#else
    return 0U;
#endif
}

UINT8 RtcSleep_PortGetExternalCommCounter(void)
{
    return SleepDeal_GetExternalCommCounter();
}

UINT8 RtcSleep_PortUpdateRtcData(void)
{
    return RtcSleep_AfePortUpdateRtcData();
}

UINT8 RtcSleep_PortHasCurrentWake(enum irqWakeup *source)
{
    return RtcSleep_AfePortHasCurrentWake(source);
}

UINT8 RtcSleep_PortHasAfeWake(enum irqWakeup *source)
{
    return RtcSleep_AfePortHasAfeWake(source);
}

void RtcSleep_PortCommitResetSleep(UINT8 sleep_mode)
{
    LogRecord_RequestSleep();
    SleepDeal_Continue(sleep_mode);
}

void RtcSleep_PortEnterStop(void)
{
    Feed_IWatchDog;
    Sys_StopMode();
    RTC_SyncAfterStop();
    Feed_IWatchDog;
}

void RtcSleep_PortDisableStopWakeup(void)
{
    LowPower_DisableWakeupExti();
    RTC_DisableStopWakeup();
}

void RtcSleep_PortRestoreAfterStop(void)
{
    InitRunAfterStopWakeup();
}

UINT32 RtcSleep_PortGetLastWakeupSeconds(void)
{
    return RTC_GetLastWakeupPeriodSeconds();
}

void RtcSleep_PortApplySocRtcRest(UINT32 rest_seconds)
{
    SOC_ApplyRtcRelaxationCompensation(rest_seconds,
                                       g_stCellInfoReport.u16VCellMin,
                                       g_stCellInfoReport.u16VCellMax);
}

void RtcSleep_PortAddRuntimeSeconds(UINT32 seconds)
{
    extern UINT32 su32_Interval_S_Tcnt;

    su32_Interval_S_Tcnt += seconds;
}

void cpu_frequency_conf(void)
{
    SystemInit();
    SystemCoreClockUpdate();
    InitDelay();
}
