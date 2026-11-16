#include "main.h"
#include "SH367309_DataDeal.h"
#include "rtc_sleep_port.h"
#include "DataDeal.h"
#include "conf.h"
#include "Sci_Upper.h"
#include "RTC.h"
#include "LowPowerSleep.h"

#ifdef TERNARYLI
#define LOW_POWER_FORCE_DEEP_SLEEP_MV ((uint16_t)2750U)
#elif (defined(LIFEPO))
#define LOW_POWER_FORCE_DEEP_SLEEP_MV ((uint16_t)2650U)
#endif
// #define LOW_POWER_FORCE_DEEP_SLEEP_SECONDS ((uint16_t)(60 * 10))
#define LOW_POWER_FORCE_DEEP_SLEEP_SECONDS ((uint16_t)(60))
#define LOW_POWER_DEEP_SLEEP_ICHG_LIMIT ((uint16_t)5U)

volatile enum irqWakeup g_irq_t = NO_IRQ;
volatile struct LOW_POWER_RTC_STATUS g_stLowPowerRtcStatus = {NO_SLEEP};

static uint8_t CheckChargeMa(void) { return (RtcSleep_PortGetChargeCurrentMa() >= 2U) ? 1U : 0U; }
static uint8_t CheckDischargeMa(void) { return (RtcSleep_PortGetDischargeCurrentMa() >= 2U) ? 1U : 0U; }
static uint8_t CheckKeyActive(void) { return RtcSleep_PortIsMcuWakeActive(); }

/* Explicit predicates are simpler than a function-pointer table here. */
uint32_t LP_GetBlockReason(void)
{
    uint32_t block=0U;
    uint8_t comm=RtcSleep_PortGetExternalCommCounter();

    if (CheckChargeMa()) block|=LP_BLOCK_CHARGE;
    if (CheckDischargeMa()) block|=LP_BLOCK_DISCHARGE;
    if (Sci_IsAnyPortBusy() || Can_IsBusy()) block|=LP_BLOCK_COMM;
    if (CheckKeyActive()) block|=LP_BLOCK_KEY;
    if (u8FlashUpdateFlag) block|=LP_BLOCK_UPGRADE;
    if (g_stCellInfoReport.unMdlFault_Third.all ||
        g_stCellInfoReport.unMdlFault_Second.all ||
        SH367309_Reg_Store.REG_BSTATUS1.bits.SC ||
        SystemError_IsActive(ERROR_STATUS_TEMP_BREAK))
        block|=LP_BLOCK_FAULT;
    if (!AFE_IsHealthy() || !AFE_IsDataFresh()) block|=LP_BLOCK_AFE;
    if (SystemError_IsActive(ERROR_STATUS_EEPROM_STORE) ||
        SystemError_IsActive(ERROR_STATUS_EEPROM_COM))
        block|=LP_BLOCK_STORAGE;
    if (comm != g_stLowPowerRtcStatus.comm)
    {
        g_stLowPowerRtcStatus.comm=comm;
        block|=LP_BLOCK_EXT_COMM;
    }
    return block;
}

void low_power_log_and_commit_sleep(uint8_t sleep_mode)
{
    if ((sleep_mode != NORMAL_MODE) && (sleep_mode != DEEP_MODE))
    {
        LowPower_Request(NO_SLEEP);
        return;
    }

    RtcSleep_PortCommitResetSleep(sleep_mode);
}

void LowPower_Request(enum _SLEEP_MODE mode)
{
    switch (mode)
    {
    case HICCUP_MODE:
    case NORMAL_MODE:
    case DEEP_MODE:
    case NO_SLEEP:
        g_stLowPowerRtcStatus.mode = (uint8_t)mode;
        break;
    default:
        break;
    }
}

static uint8_t lp_select_deep_if_low_voltage(void)
{
    /* Cached AFE data cannot start or advance low-voltage sleep timers. */
    if (!AFE_IsHealthy() || !AFE_IsDataFresh())
    {
        g_stLowPowerRtcStatus.force=0U;
        g_stLowPowerRtcStatus.vlow=0U;
        return 0U;
    }
    /* UV handling only suppresses its immediate event request. The existing
     * low-voltage timers must still run after a protective sleep wake. */
    if ((RtcSleep_PortGetCellMinMv() <= LOW_POWER_FORCE_DEEP_SLEEP_MV) &&
        (RtcSleep_PortGetChargeCurrentMa() <= LOW_POWER_DEEP_SLEEP_ICHG_LIMIT))
    {
        g_stLowPowerRtcStatus.idle = 0U;
        g_stLowPowerRtcStatus.block = 0U;
        if (++g_stLowPowerRtcStatus.force >= LOW_POWER_FORCE_DEEP_SLEEP_SECONDS)
        {
            LowPower_Request(DEEP_MODE);
        }
        return 1U;
    }

    if ((RtcSleep_PortGetCellMinMv() <= RtcSleep_PortGetLowVoltageSleepMv()) &&
        (RtcSleep_PortGetChargeCurrentMa() <= LOW_POWER_DEEP_SLEEP_ICHG_LIMIT))
    {
        g_stLowPowerRtcStatus.idle = 0U;
        g_stLowPowerRtcStatus.block = 0U;
        if (++g_stLowPowerRtcStatus.vlow >= (uint32_t)OtherElement.u16Sleep_TimeVlow * 60U)
        {
            LowPower_Request(DEEP_MODE);
        }
        return 1U;
    }

    g_stLowPowerRtcStatus.vlow = 0U;
    g_stLowPowerRtcStatus.force = 0U;
    return 0U;
}

static void lp_update_sleep_request(void)
{
    if (lp_select_deep_if_low_voltage() != 0U)
    {
        return;
    }
    /* The charger can stop before the confirmed full target reaches 100%.
     * Keep RUN for that short ramp, but never block an explicit DEEP or an
     * AFE failure/low-voltage sleep request. */
    if (SOC_HasPendingFull() && AFE_IsHealthy() && AFE_IsDataFresh())
    {
        g_stLowPowerRtcStatus.idle = 0U;
        return;
    }

    g_stLowPowerRtcStatus.block = LP_GetBlockReason();
    if (SleepDeal_NormalIdleExpired((UINT8)(
        (g_stLowPowerRtcStatus.block & (LP_BLOCK_CHARGE | LP_BLOCK_DISCHARGE |
         LP_BLOCK_COMM | LP_BLOCK_KEY | LP_BLOCK_EXT_COMM | LP_BLOCK_UPGRADE |
         LP_BLOCK_AFE | LP_BLOCK_STORAGE | LP_BLOCK_FAULT))!=0U))) {
        LowPower_Request(DEEP_MODE);
        return;
    }
    if (g_stLowPowerRtcStatus.block != 0U)
    {
        g_stLowPowerRtcStatus.idle = 0U;
        return;
    }

    if (++g_stLowPowerRtcStatus.idle >= sys_time.time_enter_rtc)
    {
        g_stLowPowerRtcStatus.idle = 0U;
        LowPower_Request(HICCUP_MODE);
    }
}

static bool rtc_sleep_has_wakeup_exception(void)
{
    enum irqWakeup source = NO_IRQ;

    /* A confirmed UV event has priority over key/UART/current wake. Read AFE
     * status even in NORMAL; a closed DSG MOS alone is not undervoltage. */
    if (RtcSleep_PortHasAfeWake(&source) != 0U) {
        g_irq_t=source;
        return true;
    }

    if (RtcSleep_PortUpdateRtcData() == 0U)
    {
        (void)SleepDeal_NormalIdleExpired(1U);
        return true;
    }

    if (g_stLowPowerRtcStatus.mode != HICCUP_MODE)
    {
        return false;
    }

    if (RtcSleep_PortHasCurrentWake(&source) != 0U)
    {
        /* A simultaneous current wake must retain the switch IRQ so the
         * HICCUP key wake can start its 3 s hold after RUN restoration. */
        if (g_irq_t != soc_key) g_irq_t = source;
        return true;
    }

    return false;
}

static bool rtc_sleep_prepare_rtc(void)
{
    if (!Init_RTC()) return false;
    g_stLowPowerRtcStatus.comm_wakeup=0U;
    SleepDeal_ResetKeyState(0U);
    g_stLowPowerRtcStatus.sleep = 0U;
    IOstatus_RTCMode();
    if (g_stLowPowerRtcStatus.mode == HICCUP_MODE)
    {
        InitWakeUp_RTCMode();
    }
    else
    {
        InitWakeUp_Base();
    }

    /* STOP time must not count as ongoing RUN failure monitoring. */
    SystemRuntime_PausePersistentFailureTimers();
    (void)LowPowerSleep_SaveCoreState();
    g_irq_t = NO_IRQ;
    MCUO_DEBUG_LED1 = 1;
    return true;
}

static bool rtc_sleep_run_hiccup_cycle(void)
{
    // todo !!!!
    RTC_ClearStopWakeup();
    if (!RTC_WKTimeConfig()) return false; /* Restore RUN, never enter blind STOP. */
    sys_time.rtc_sec_cnt = RTC_GetCounter();
    g_irq_t = NO_IRQ;

    RtcSleep_PortEnterStop();

    MCUO_DEBUG_LED1 = 0;
    initAFE1_IIC();

    if (rtc_sleep_has_wakeup_exception()) {
        if (g_stLowPowerRtcStatus.mode==DEEP_MODE)
            low_power_log_and_commit_sleep(DEEP_MODE);
        return false;
    }

    /* An edge authorizes RUN, not a protocol command. Keep a separate latch
     * because the shared IRQ source can be overwritten by a switch edge. */
    if (g_stLowPowerRtcStatus.comm_wakeup ||
        g_irq_t==uart1_irq || g_irq_t==uart2_irq || g_irq_t==rs485_irq)
        return false;

    if (g_irq_t==soc_key) {
        if (SleepDeal_IsWakeupValid()) return false;
        MCUO_DEBUG_LED1=1;
        return true;
    }

    if (RTC_IsStopWakeup() != 0U)
    {
        g_stLowPowerRtcStatus.sleep += RtcSleep_PortGetLastWakeupSeconds();

        if (g_stLowPowerRtcStatus.mode==HICCUP_MODE &&
            SleepDeal_NormalIdleExpired((UINT8)(CheckChargeMa() || CheckDischargeMa() || CheckKeyActive()))) {
            /* Commit before resetting: HICCUP has kept SOC only in RAM.
             * ADC and normal business remain paused throughout the handoff. */
            LowPower_Request(DEEP_MODE);
            RtcSleep_PortAddRuntimeSeconds(g_stLowPowerRtcStatus.sleep);
            low_power_log_and_commit_sleep(DEEP_MODE);
            return false;
        }

        MCUO_DEBUG_LED1 = 1;
        return true;
    }
    else if (g_stLowPowerRtcStatus.mode == NORMAL_MODE && (RTC_IsStopWakeup() == 0U))
    {
        if (SleepDeal_IsWakeupValid())
        {
            return false;
        }
        else
        {
            MCUO_DEBUG_LED1 = 1;
            return true;
        }
    }

    return false;
}

void rtc_sleep(void)
{
    uint8_t sleep_mode;

    if (SleepDeal_TakeKeySleepRequest() && g_stLowPowerRtcStatus.mode!=DEEP_MODE)
        LowPower_Request(NORMAL_MODE);

    if (g_stLowPowerRtcStatus.mode==NO_SLEEP && RtcSleep_PortIsOneSecondTick()!=0U)
        lp_update_sleep_request();
    sleep_mode = g_stLowPowerRtcStatus.mode;
    if (sleep_mode==NO_SLEEP)
    {
        return;
    }

    if ((sleep_mode != HICCUP_MODE) &&
        (sleep_mode != NORMAL_MODE) &&
        (sleep_mode != DEEP_MODE))
    {
        return;
    }

    switch (sleep_mode)
    {
    case DEEP_MODE:
        low_power_log_and_commit_sleep(sleep_mode);
        break;
    case NORMAL_MODE:
    case HICCUP_MODE:
        if (!rtc_sleep_prepare_rtc()) {
            LowPower_Request(NO_SLEEP);
            break;
        }
        if (sleep_mode == NORMAL_MODE)
            MCUO_AFE_CTLC = 0;

        while (rtc_sleep_run_hiccup_cycle())
        {
        }

        if (g_stLowPowerRtcStatus.mode==DEEP_MODE) return;

        sleep_mode=g_stLowPowerRtcStatus.mode;
        /* HICCUP retains CTL through STOP; waking must not pulse it low.
         * NORMAL stays closed from entry until normal AFE/MOS arbitration
         * authorizes release with fresh data. */

        RtcSleep_PortDisableStopWakeup();
        {
            RTC_ClearStopWakeup();
            LowPower_Request(NO_SLEEP);
            g_stLowPowerRtcStatus.comm_wakeup=0U;
            // todo 仔细梳理下配置
            RtcSleep_PortRestoreAfterStop();
            AFE_MonitorResumeAfterStop();
            SleepDeal_ResetKeyState((UINT8)(sleep_mode==HICCUP_MODE && g_irq_t==soc_key));
            RtcSleep_PortAddRuntimeSeconds(g_stLowPowerRtcStatus.sleep);
        }
        break;
    default:
        LowPower_Request(NO_SLEEP);
        break;
    }
}
