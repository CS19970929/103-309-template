#include "main.h"
#include "LowPowerSleep.h"
#include "rtc_sleep_afe_port.h"
#include "rtc_sleep_port.h"

UINT8 RtcSleep_PortIsOneSecondTick(void)
{
    return g_st_SysTimeFlag.bits.b1Sys1000msFlag;
}

UINT16 RtcSleep_PortGetIdleDelayTargetSeconds(void)
{
    return sys_time.time_enter_rtc;
}

UINT16 RtcSleep_PortGetCellMinMv(void)
{
    return g_stCellInfoReport.u16VCellMin;
}

UINT16 RtcSleep_PortGetCellMaxMv(void)
{
    return g_stCellInfoReport.u16VCellMax;
}

UINT16 RtcSleep_PortGetChargeCurrentMa(void)
{
    return g_stCellInfoReport.u16Ichg;
}

UINT16 RtcSleep_PortGetDischargeCurrentMa(void)
{
    return g_stCellInfoReport.u16IDischg;
}

UINT16 RtcSleep_PortGetLowVoltageSleepMv(void)
{
    return OtherElement.u16Sleep_Vlow;
}

UINT16 RtcSleep_PortGetLowVoltageSleepMinutes(void)
{
    return OtherElement.u16Sleep_TimeVlow;
}

UINT8 RtcSleep_PortIsMcuWakeActive(void)
{
    return (UINT8)(GPIO_ReadInputDataBit(GPIO_MCU_WK, PIN_MCU_WK) != Bit_RESET);
}

UINT8 RtcSleep_PortIsFactoryAgingActive(void)
{
    return FactoryAging_IsActive();
}

UINT8 RtcSleep_PortGetExternalCommCounter(void)
{
    return RTC_ExtComCnt;
}

UINT8 RtcSleep_PortIsAfeSleepBlocked(void)
{
    return RtcSleep_AfePortIsSleepBlocked();
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

UINT8 RtcSleep_PortIsEmergencyWakeVoltage(void)
{
    return (UINT8)(g_stCellInfoReport.u16VCellMin <= 2750U);
}

void RtcSleep_PortRequestSleepLog(void)
{
    LogRecord_RequestSleep();
}

void RtcSleep_PortCommitResetSleep(UINT8 sleep_mode)
{
    extern UINT32 su32_Interval_S_Tcnt;

    Can_PrepareSleep();
    LogRecord_RequestSleep();
    LogEvent_Record(1U, BMS_SLEEP, &su32_Interval_S_Tcnt);
    SleepDeal_Continue(sleep_mode);
}

void RtcSleep_PortOnDeepSleepRequest(void)
{
}

void RtcSleep_PortPrepareRtcStop(UINT32 rtc_cycle_count)
{
    (void)rtc_cycle_count;
    LowPowerSleep_SaveCoreState();

    Init_RTC();
    IOstatus_RTCMode();
    InitWakeUp_RTCMode();
}

void RtcSleep_PortEnterStop(void)
{
    Feed_IWatchDog;
    Sys_StopMode();
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

UINT8 RtcSleep_PortIsRtcWake(void)
{
    return is_rtc_wakekup ? 1U : 0U;
}

void RtcSleep_PortClearRtcWake(void)
{
    is_rtc_wakekup = false;
}

UINT32 RtcSleep_PortGetLastWakeupSeconds(void)
{
    return RTC_GetLastWakeupPeriodSeconds();
}

UINT32 RtcSleep_PortGetCanRtcPeriodSeconds(void)
{
    return Can_GetIdleRtcPeriodSeconds();
}

UINT8 RtcSleep_PortIsCanBusActive(void)
{
    return Can_IsBusActive();
}

void RtcSleep_PortRunCanRtcWakeService(UINT32 rtc_elapsed_seconds)
{
    Can_RtcWakeService(rtc_elapsed_seconds);
}

UINT8 RtcSleep_PortApplySocRtcRest(UINT32 rest_seconds)
{
    SOC_ApplyRtcRelaxationCompensation(rest_seconds,
                                       g_stCellInfoReport.u16VCellMin,
                                       g_stCellInfoReport.u16VCellMax);

    return SOC_Enhance_Element.u8_SOC;
}

void RtcSleep_PortAddRuntimeSeconds(UINT32 seconds)
{
    extern UINT32 su32_Interval_S_Tcnt;

    su32_Interval_S_Tcnt += seconds;
}

enum irqWakeup RtcSleep_PortGuessWakeupSource(void)
{
    if (GPIO_ReadInputDataBit(GPIO_CHG_IN, PIN_CHG_IN) == Bit_RESET)
    {
        return PA0_irq;
    }

    if (GPIO_ReadInputDataBit(GPIO_SW, PIN_SW) == Bit_RESET)
    {
        return soc_key;
    }

    return NO_IRQ;
}

void RtcSleep_PortOnWakeupSource(enum irqWakeup source)
{
    if (source == soc_key)
    {
        LedBar_RequestSocDisplay();
        APP_LedBar();
    }
}

void cpu_frequency_conf(void)
{
    SystemInit();
    SystemCoreClockUpdate();
    InitDelay();
}
