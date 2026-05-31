#include "main.h"
#include "rtc_sleep_port.h"

static UINT8 rtc_afe_is_sleep_blocked(void)
{
    if (MTPRead(MTP_BSTATUS1, 3, &SH367309_Reg_Store.REG_BSTATUS1.all))
    {
        if (SH367309_Reg_Store.REG_BSTATUS1.all ||
            SH367309_Reg_Store.REG_BSTATUS2.all ||
            SH367309_Reg_Store.REG_BSTATUS3.bits.L0V ||
            SH367309_Reg_Store.REG_BSTATUS3.bits.PCHG_FET)
        {
            log_e("error can not enter rtc");
            return 1U;
        }
        return 0U;
    }

    log_a("err mtp comm");
    return 2U;
}

static UINT8 rtc_afe_update_data(void)
{
    if (UpdateVoltageFromBqMaximo())
    {
        log_e("IIC error!!!!!!!!!!!!!!!!!!!!!!!!!\n");
        return 0U;
    }
    DataLoad_CellVolt();
    DataLoad_CellVoltMaxMinFind();
    DataLoad_Temperature();
    DataLoad_TemperatureMaxMinFind();
    return 1U;
}

static UINT8 rtc_afe_has_current_wake(enum irqWakeup *source)
{
    UINT8 result;

    if (source != 0)
    {
        *source = NO_IRQ;
    }

    DataLoad_Current();

    log_i("ichg %d\n", g_stCellInfoReport.u16Ichg);
    log_i("dsg %d\n", g_stCellInfoReport.u16IDischg);
    result = (UINT8)((g_stCellInfoReport.u16Ichg != 0U) ||
                     (g_stCellInfoReport.u16IDischg != 0U));
    if (result != 0U)
    {
        log_w("afe current V %d, ICHG %d, IDSG %d",
              SH367309_Read_AFE1.u16Current,
              g_stCellInfoReport.u16Ichg,
              g_stCellInfoReport.u16IDischg);
        if (source != 0)
        {
            *source = current_wake;
        }
    }

    return result;
}

static UINT8 rtc_afe_has_afe_wake(enum irqWakeup *source)
{
    if (source != 0)
    {
        *source = NO_IRQ;
    }

    if (MTPRead(MTP_BALANCEH, 5, &SH367309_Reg_Store.u8_MTP_BALANCEH))
    {
        SystemRuntime_SetMosStatus(SH367309_Reg_Store.REG_BSTATUS3.bits.CHG_FET,
                                   SH367309_Reg_Store.REG_BSTATUS3.bits.DSG_FET);
        Fault_ChangeToMCU();

        if (!SystemRuntime_IsDischargeMosOpen())
        {
            log_w("DSG close\n");
            if (source != 0)
            {
                *source = chg_dsg_close;
            }
            return 1U;
        }

        if (g_stCellInfoReport.unMdlFault_Third.all != 0U)
        {
            log_w("afe fault 0x%04x\n", g_stCellInfoReport.unMdlFault_Third.all);
            if (source != 0)
            {
                *source = error_wake;
            }
            return 1U;
        }
    }
    return 0U;
}

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

UINT8 RtcSleep_PortIsAfeSleepBlocked(void)
{
    return rtc_afe_is_sleep_blocked();
}

UINT8 RtcSleep_PortUpdateRtcData(void)
{
    return rtc_afe_update_data();
}

UINT8 RtcSleep_PortHasCurrentWake(enum irqWakeup *source)
{
    return rtc_afe_has_current_wake(source);
}

UINT8 RtcSleep_PortHasAfeWake(enum irqWakeup *source)
{
    return rtc_afe_has_afe_wake(source);
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
#ifdef __FUNC__LED__
    set_LED_state(LED_BAR_NORMAL, 4);
#endif
}

void RtcSleep_PortPrepareRtcStop(UINT32 rtc_cycle_count)
{
    (void)rtc_cycle_count;
    Can_PrepareSleep();
    SOC_SaveSnapshotBeforeSleep();
    FactoryAging_SaveProgressBeforeSleep();

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
