#include "main.h"
#include "FactoryAging.h"
#include "LowPowerSleep.h"
#include "IrqDebug.h"
#include "rtc_sleep_afe_port.h"
#include "rtc_sleep_port.h"

UINT8 RtcSleep_PortIsOneSecondTick(void)
{
    return g_st_SysTimeFlag.bits.b1Sys1000msFlag;
}

UINT16 RtcSleep_PortGetCellMinMv(void)
{
    return g_stCellInfoReport.u16VCellMin;
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

UINT8 RtcSleep_PortIsMcuWakeActive(void)
{
    // return (UINT8)(GPIO_ReadInputDataBit(GPIO_MCU_WK, PIN_MCU_WK) != Bit_RESET);
    return false;
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
    IrqDebug_SetPhase((uint8_t)IRQDBG_PHASE_STOP_WAIT);
    Sys_StopMode();
    IrqDebug_SetPhase((uint8_t)IRQDBG_PHASE_STOP_WAKE_RAW);
    Feed_IWatchDog;
}

void RtcSleep_PortDisableStopWakeup(void)
{
    LowPower_DisableWakeupExti();
    RTC_DisableStopWakeup();
}

void RtcSleep_PortRestoreAfterStop(void)
{
    IrqDebug_SetPhase((uint8_t)IRQDBG_PHASE_STOP_RESTORE);
    InitRunAfterStopWakeup();
    IrqDebug_SetPhase((uint8_t)IRQDBG_PHASE_RUN);
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
    // FactoryAging_ApplySleepTime(seconds);
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

void cpu_frequency_conf(void)
{
    SystemInit();
    SystemCoreClockUpdate();
    InitDelay();
}
