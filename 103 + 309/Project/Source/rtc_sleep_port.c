#include "main.h"
#include "FactoryAging.h"
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

// UINT8 RtcSleep_PortIsMcuWakeActive(void)
// {
//     if (GPIO_ReadInputDataBit(GPIO_SOC_KEY, PIN_SOC_KEY) == Bit_RESET)
//     {
//         return 1U;
//     }
//     if (GPIO_ReadInputDataBit(GPIO_MAIN_SW, PIN_MAIN_SW) == Bit_RESET)
//     {
//         return 1U;
//     }
//     return 0U;
// }

UINT8 RtcSleep_PortIsChargerInputActive(void)
{
    return (UINT8)(GPIO_ReadInputDataBit(GPIO_CHG_IN, PIN_CHG_IN) == Bit_RESET);
}

UINT8 RtcSleep_PortIsHeatActive(void)
{
    return SystemStatus.bits.b1Status_Heat ? 1U : 0U;
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
    LogRecord_Flag.bits.Log_Sleep = 1U;
}

void RtcSleep_PortClearLegacySleepRequest(void)
{
    Sleep_Mode.bits.b1TestSleep = 0U;
    Sleep_Mode.bits.b1NormalSleep_L1 = 0U;
    Sleep_Mode.bits.b1NormalSleep_L2 = 0U;
    Sleep_Mode.bits.b1NormalSleep_L3 = 0U;
    Sleep_Mode.bits.b1ForceToSleep_L1 = 0U;
    Sleep_Mode.bits.b1ForceToSleep_L2 = 0U;
    Sleep_Mode.bits.b1ForceToSleep_L3 = 0U;
    Sleep_Mode.bits.b1ForceToSleep_L1_Out = 0U;
    Sleep_Mode.bits.b1_ToSleepFlag = 0U;
}

void RtcSleep_PortSelectLegacyResetSleep(UINT8 sleep_mode)
{
    RtcSleep_PortClearLegacySleepRequest();
    Sleep_Mode.bits.b1_ToSleepFlag = 1U;

    if (sleep_mode == HICCUP_MODE)
    {
        Sleep_Mode.bits.b1ForceToSleep_L1 = 1U;
    }
    else if (sleep_mode == NORMAL_MODE)
    {
        Sleep_Mode.bits.b1ForceToSleep_L2 = 1U;
    }
    else if (sleep_mode == DEEP_MODE)
    {
        Sleep_Mode.bits.b1ForceToSleep_L3 = 1U;
    }
    else
    {
        Sleep_Mode.bits.b1_ToSleepFlag = 0U;
    }
}

void RtcSleep_PortCommitResetSleep(UINT8 sleep_mode)
{
    extern UINT32 su32_Interval_S_Tcnt;

    Can_PrepareSleep();
    LogRecord_Flag.bits.Log_Sleep = 1U;
    LogEvent_Record(LogRecord_Flag.bits.Log_Sleep, BMS_SLEEP, &su32_Interval_S_Tcnt);
    RtcSleep_PortSelectLegacyResetSleep(sleep_mode);
    SleepDeal_Continue();
}

void RtcSleep_PortOnDeepSleepRequest(void)
{
    LedBar_SaveSleepSoc();
    LedBar_SetSleep(1U);
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

    log_w("rtc rest %lu s, vmin %u, soc %u",
          (unsigned long)rest_seconds,
          (unsigned int)g_stCellInfoReport.u16VCellMin,
          (unsigned int)SOC_Enhance_Element.u8_SOC);

    return SOC_Enhance_Element.u8_SOC;
}

void RtcSleep_PortAddRuntimeSeconds(UINT32 seconds)
{
    extern UINT32 su32_Interval_S_Tcnt;

    su32_Interval_S_Tcnt += seconds;
    if (su32_Interval_S_Tcnt >= (3600U * 6U))
    {
        log_e("rtc sleep update window reached\n");
    }
    log_a("sleep time %d s", su32_Interval_S_Tcnt);
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
        LedBar_ShowSleepSocPreview();
    }
}

void cpu_frequency_conf(void)
{
    SystemInit();
    SystemCoreClockUpdate();
    InitDelay();
}
