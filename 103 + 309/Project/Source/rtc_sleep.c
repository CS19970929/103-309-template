#include "main.h"
#include "BmsParameters.h"
#include "rtc_sleep_port.h"
#include "DataDeal.h"
#include "conf.h"
#include "Sci_Upper.h"
#include "RTC.h"
#include "LowPowerSleep.h"
#include "afe3520/Afe3520Config.h"

#ifdef TERNARYLI
#define LOW_POWER_FORCE_DEEP_SLEEP_MV ((uint16_t)2750U)
#elif (defined(LIFEPO))
#define LOW_POWER_FORCE_DEEP_SLEEP_MV ((uint16_t)2650U)
#endif
#define LOW_POWER_FORCE_DEEP_SLEEP_SECONDS ((uint16_t)(60))
#define LOW_POWER_DEEP_SLEEP_ICHG_LIMIT ((uint16_t)5U)

/* A host command survives automatic idle decisions and polling traffic. */
static uint8_t s_commandSleepPending;

void LowPower_RequestCommandSleep(void)
{
    s_commandSleepPending = 1U;
}

static uint8_t lp_process_command_sleep(void)
{
    if (!s_commandSleepPending) return 0U;
    /* Finish the command reply and any in-flight persistent transaction. */
    if (Sci_IsAnyPortBusy() || Can_IsBusy() || StorageFlash_IsBusy() ||
        u8FlashUpdateE2PROM || u8FlashUpdateFlag) return 1U;
    s_commandSleepPending = 0U;
    LowPowerSleep_SaveResetState();
    /* Explicit shutdown uses bounded AFE attempts, WDT-off and key-only STOP. */
    SleepDeal_EmergencySleep();
    return 1U;
}

volatile enum irqWakeup g_irq_t = NO_IRQ;
volatile struct LOW_POWER_RTC_STATUS g_stLowPowerRtcStatus = {
    NO_SLEEP,
    0U,
    0U,
    0U,
    0U,
    0U,
    0U,
    0U,
    0U,
    0U,
    0U,
    0U,
    0U};

uint32_t LP_GetBlockReason(void)
{
    uint32_t reason = 0U;
    uint8_t comm;
    if (!Afe3520_GetSnapshot()->valid || Afe3520_ConfigDirty() ||
        !Bms3520_GetProtectionStatus()->configValid ||
        (Bms3520_GetBlockMask() & (AFE3520_BLOCK_GLOBAL_AFE_COMM | AFE3520_BLOCK_GLOBAL_AFE_CONFIG | AFE3520_BLOCK_GLOBAL_SYSTEM)))
        reason |= LP_BLOCK_AFE;
#if AFE3520_CFG_WDT_ENABLE
    /* External WDT cannot be serviced during STOP: keep the scheduler alive. */
    reason |= LP_BLOCK_AFE_WDT;
#endif

    if (RtcSleep_PortGetChargeCurrentA10() >= 5U)
    {
        reason |= LP_BLOCK_CHARGE;
    }
    if (RtcSleep_PortGetDischargeCurrentA10() >= 5U)
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
    if (StorageFlash_IsBusy() || u8FlashUpdateE2PROM)
    {
        reason |= LP_BLOCK_FLASH_BUSY;
    }
    if (u8FlashUpdateFlag != 0U)
    {
        reason |= LP_BLOCK_UPGRADE;
    }
    if ((g_stCellInfoReport.unMdlFault_Third.all != 0U) ||
        (g_stCellInfoReport.unMdlFault_Second.all != 0U) ||
        (Afe3520_GetSnapshot()->flag1 & AFE3520_FLAG1_SC))
    {
        reason |= LP_BLOCK_FAULT;
    }

    comm = RtcSleep_PortGetExternalCommCounter();
    if (comm != g_stLowPowerRtcStatus.comm)
    {
        g_stLowPowerRtcStatus.comm = comm;
        reason |= LP_BLOCK_EXT_COMM;
    }

    return reason;
}

static void lp_refresh_status(void)
{
    g_stLowPowerRtcStatus.rtc = RTC_IsStopWakeup();
    g_stLowPowerRtcStatus.idleMax = sys_time.time_enter_rtc;
}

void low_power_log_and_commit_sleep(uint8_t sleep_mode)
{
    if ((sleep_mode != NORMAL_MODE) && (sleep_mode != DEEP_MODE))
    {
        LowPower_Request(NO_SLEEP);
        return;
    }

    if (LP_GetBlockReason() & (LP_BLOCK_AFE | LP_BLOCK_AFE_WDT | LP_BLOCK_FLASH_BUSY |
                              LP_BLOCK_UPGRADE | LP_BLOCK_KEY | LP_BLOCK_CHARGE | LP_BLOCK_DISCHARGE | LP_BLOCK_COMM))
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

    lp_refresh_status();
}

static uint8_t lp_select_deep_if_low_voltage(void)
{

    if ((RtcSleep_PortGetCellMinMv() > 0U) && (RtcSleep_PortGetCellMinMv() <= LOW_POWER_FORCE_DEEP_SLEEP_MV) &&
        (RtcSleep_PortGetChargeCurrentA10() <= LOW_POWER_DEEP_SLEEP_ICHG_LIMIT))
    {
        g_stLowPowerRtcStatus.idle = 0U;
        g_stLowPowerRtcStatus.block = 0U;
        if (++g_stLowPowerRtcStatus.force >= LOW_POWER_FORCE_DEEP_SLEEP_SECONDS)
        {
            LowPower_Request(DEEP_MODE);
        }
        return 1U;
    }

    if ((RtcSleep_PortGetCellMinMv() > 0U) && (RtcSleep_PortGetCellMinMv() <= RtcSleep_PortGetLowVoltageSleepMv()) &&
        (RtcSleep_PortGetChargeCurrentA10() <= LOW_POWER_DEEP_SLEEP_ICHG_LIMIT))
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

static uint16_t s_emergencyFaultSeconds;
static uint16_t s_emergencyLowSeconds;

static uint8_t lp_emergency_sleep_due(void)
{
    uint32_t mask = Bms3520_GetBlockMask();
    uint8_t invalid = !Afe3520_GetSnapshot()->valid || Afe3520_ConfigDirty() ||
                      !Bms3520_GetProtectionStatus()->configValid;
    uint8_t fault = invalid || (mask & (AFE3520_BLOCK_GLOBAL_AFE_COMM |
                       AFE3520_BLOCK_GLOBAL_AFE_CONFIG | AFE3520_BLOCK_GLOBAL_SHORT |
                       AFE3520_BLOCK_GLOBAL_WDT | AFE3520_BLOCK_GLOBAL_INTERNAL_TEMP));
    uint8_t low = !invalid && RtcSleep_PortGetCellMinMv() > 0U &&
                  RtcSleep_PortGetCellMinMv() <= LOW_POWER_FORCE_DEEP_SLEEP_MV &&
                  RtcSleep_PortGetChargeCurrentA10() <= LOW_POWER_DEEP_SLEEP_ICHG_LIMIT;
    if (fault) { if (s_emergencyFaultSeconds < AFE3520_CFG_EMERGENCY_FAULT_SECONDS) ++s_emergencyFaultSeconds; }
    else s_emergencyFaultSeconds = 0U;
    if (low) { if (s_emergencyLowSeconds < AFE3520_CFG_EMERGENCY_LOW_SECONDS) ++s_emergencyLowSeconds; }
    else s_emergencyLowSeconds = 0U;
    return s_emergencyFaultSeconds >= AFE3520_CFG_EMERGENCY_FAULT_SECONDS ||
           s_emergencyLowSeconds >= AFE3520_CFG_EMERGENCY_LOW_SECONDS;
}

static void lp_update_sleep_request(void)
{
    uint32_t block = LP_GetBlockReason();
    /* Low voltage can override idle/fault waiting, but never these constraints. */
    if (block & (LP_BLOCK_AFE | LP_BLOCK_AFE_WDT | LP_BLOCK_FLASH_BUSY | LP_BLOCK_UPGRADE |
                 LP_BLOCK_KEY | LP_BLOCK_CHARGE | LP_BLOCK_DISCHARGE | LP_BLOCK_COMM | LP_BLOCK_EXT_COMM))
    {
        g_stLowPowerRtcStatus.block = block;
        g_stLowPowerRtcStatus.idle = 0U;
        g_stLowPowerRtcStatus.force = 0U;
        g_stLowPowerRtcStatus.vlow = 0U;
        LowPower_Request(NO_SLEEP);
        return;
    }
    if (lp_select_deep_if_low_voltage() != 0U)
    {
        lp_refresh_status();
        return;
    }

    g_stLowPowerRtcStatus.block = block;
    if (g_stLowPowerRtcStatus.block != 0U)
    {
        g_stLowPowerRtcStatus.idle = 0U;
        LowPower_Request(NO_SLEEP);
        lp_refresh_status();
        return;
    }

    if (++g_stLowPowerRtcStatus.idle >= sys_time.time_enter_rtc)
    {
        g_stLowPowerRtcStatus.idle = 0U;
        LowPower_Request(HICCUP_MODE);
    }
    lp_refresh_status();
}

static bool rtc_sleep_has_wakeup_exception(void)
{
    enum irqWakeup source = NO_IRQ;

    if (RtcSleep_PortUpdateRtcData() == 0U)
    {
        g_irq_t = error_wake;
        return true;
    }

    if (g_stLowPowerRtcStatus.mode != HICCUP_MODE)
    {
        return false;
    }

    if (RtcSleep_PortHasCurrentWake(&source) != 0U)
    {
        g_irq_t = source;
        return true;
    }

    if (RtcSleep_PortHasAfeWake(&source) != 0U)
    {
        g_irq_t = source;
        return true;
    }

    return false;
}

static void rtc_sleep_prepare_rtc(void)
{
    g_irq_t = NO_IRQ;
    g_stLowPowerRtcStatus.cycles = 0U;
    g_stLowPowerRtcStatus.sleep = 0U;
    Init_RTC();
    IOstatus_RTCMode();
    if (g_stLowPowerRtcStatus.mode == HICCUP_MODE)
    {
        InitWakeUp_RTCMode();
    }
    else
    {
        InitWakeUp_Base();
    }

    LowPowerSleep_SaveCoreState();
    lp_refresh_status();
}

static bool rtc_sleep_run_hiccup_cycle(void)
{
    UINT32 rtc_start;
    UINT32 rtc_elapsed;

    {
        uint32_t primask = __get_PRIMASK();
        __disable_irq();
        if (g_irq_t == rtc_alarm_irq) g_irq_t = NO_IRQ;
        __set_PRIMASK(primask);
    }
    if (g_irq_t != NO_IRQ) return false;
    RTC_ClearStopWakeup();
    RTC_WKTimeConfig();
    rtc_start = RTC_GetCounter();
    sys_time.rtc_sec_cnt = rtc_start;

    RtcSleep_PortEnterStop();

    rtc_elapsed = RTC_GetCounter() - rtc_start;
    sys_time.rtc_sleep_cnt = rtc_elapsed;
    g_stLowPowerRtcStatus.sleep += rtc_elapsed;

    Afe3520_RestorePort();

    if ((RTC_IsStopWakeup() != 0U) && !rtc_sleep_has_wakeup_exception())
    {
        ++g_stLowPowerRtcStatus.cycles;
        g_stLowPowerRtcStatus.test_sample_voltage = g_stCellInfoReport.u16VCell[0];

        RtcSleep_PortApplySocRtcRest(g_stLowPowerRtcStatus.sleep);
        lp_refresh_status();

        if ((g_stCellInfoReport.u16VCellMin <= g_bmsParameters.u16VcellUvp.curValue) ||
            !SystemRuntime_IsDischargeMosOpen())
        {
            low_power_log_and_commit_sleep(DEEP_MODE);
        }

        return g_irq_t == rtc_alarm_irq || g_irq_t == NO_IRQ;
    }
    else if ((g_stLowPowerRtcStatus.mode == NORMAL_MODE) && (RTC_IsStopWakeup() == 0U))
    {
        extern UINT8 SleepDeal_IsWakeupValid(void);
        if (SleepDeal_IsWakeupValid())
        {
            return false;
        }

        return g_irq_t == rtc_alarm_irq || g_irq_t == NO_IRQ;
    }

    return false;
}

void rtc_sleep(void)
{
    uint8_t sleep_mode;

    if (lp_process_command_sleep()) return;

    if (RtcSleep_PortIsOneSecondTick() == 0U)
    {
        lp_refresh_status();
        return;
    }

    if (lp_emergency_sleep_due())
    {
        LowPower_Request(DEEP_MODE);
        SleepDeal_EmergencySleep();
        return;
    }
    lp_update_sleep_request();
    sleep_mode = g_stLowPowerRtcStatus.mode;

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
        rtc_sleep_prepare_rtc();

        while (rtc_sleep_run_hiccup_cycle())
        {
        }

        RtcSleep_PortDisableStopWakeup();
        RTC_ClearStopWakeup();
        LowPower_Request(NO_SLEEP);
        RtcSleep_PortRestoreAfterStop();

        g_stLowPowerRtcStatus.last = g_stLowPowerRtcStatus.sleep;
        RtcSleep_PortAddRuntimeSeconds(g_stLowPowerRtcStatus.sleep);
        break;

    default:
        LowPower_Request(NO_SLEEP);
        break;
    }
}
