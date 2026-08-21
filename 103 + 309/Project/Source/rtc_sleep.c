#include "main.h"
#include "SH367309_DataDeal.h"
#include "rtc_sleep_port.h"
#include "DataDeal.h"
#include "conf.h"
#include "Sci_Upper.h"
#include "RTC.h"

#ifdef TERNARYLI
#define LOW_POWER_FORCE_DEEP_SLEEP_MV ((uint16_t)2750U)
#elif (defined(LIFEPO))
#define LOW_POWER_FORCE_DEEP_SLEEP_MV ((uint16_t)2650U)
#endif

#define LOW_POWER_FORCE_DEEP_SLEEP_SECONDS ((uint16_t)60U)
#define LOW_POWER_DEEP_SLEEP_ICHG_LIMIT ((uint16_t)5U)

enum irqWakeup g_irq_t = NO_IRQ;
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

typedef uint8_t (*BlockCheckFunc)(void);

static uint8_t CheckChargeMa(void) { return (RtcSleep_PortGetChargeCurrentMa() >= 5U) ? 1U : 0U; }
static uint8_t CheckDischargeMa(void) { return (RtcSleep_PortGetDischargeCurrentMa() >= 5U) ? 1U : 0U; }
static uint8_t CheckCommBusy(void) { return (Sci_IsAnyPortBusy() || Can_IsBusy()) ? 1U : 0U; }
static uint8_t CheckKeyActive(void) { return RtcSleep_PortIsMcuWakeActive(); }
static uint8_t CheckFlashBusy(void) { return (StorageFlash_IsBusy() || u8FlashUpdateE2PROM) ? 1U : 0U; }
static uint8_t CheckUpgrade(void) { return (u8FlashUpdateFlag != 0U) ? 1U : 0U; }
static uint8_t CheckFault(void) { return (g_stCellInfoReport.unMdlFault_Third.all != 0U || g_stCellInfoReport.unMdlFault_Second.all != 0U || SH367309_Reg_Store.REG_BSTATUS1.bits.SC) ? 1U : 0U; }

typedef struct
{
    uint32_t mask;
    BlockCheckFunc check;
} BlockReasonEntry;

static const BlockReasonEntry s_block_table[] = {
    {LP_BLOCK_CHARGE, CheckChargeMa},
    {LP_BLOCK_DISCHARGE, CheckDischargeMa},
    {LP_BLOCK_COMM, CheckCommBusy},
    {LP_BLOCK_KEY, CheckKeyActive},
    {LP_BLOCK_FLASH_BUSY, CheckFlashBusy},
    {LP_BLOCK_UPGRADE, CheckUpgrade},
    {LP_BLOCK_FAULT, CheckFault},
};

#define BLOCK_TABLE_SIZE (sizeof(s_block_table) / sizeof(s_block_table[0]))

uint32_t LP_GetBlockReason(void)
{
    uint32_t reason = 0U;
    uint32_t i;

    for (i = 0U; i < BLOCK_TABLE_SIZE; i++)
    {
        if (s_block_table[i].check() != 0U)
        {
            reason |= s_block_table[i].mask;
        }
    }

    {
        uint8_t comm = RtcSleep_PortGetExternalCommCounter();
        if (comm != g_stLowPowerRtcStatus.comm)
        {
            g_stLowPowerRtcStatus.comm = comm;
            reason |= LP_BLOCK_EXT_COMM;
        }
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
#ifdef _DI_SWITCH_SYS_ONOFF
    if ((MCUI_ENI_DI1 == 1) && (g_stCellInfoReport.u16Ichg < 5U))
    {
        LowPower_Request(NORMAL_MODE);
        return 1U;
    }
#endif

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
        lp_refresh_status();
        return;
    }

    g_stLowPowerRtcStatus.block = LP_GetBlockReason();
    if (g_stLowPowerRtcStatus.block != 0U)
    {
        g_stLowPowerRtcStatus.idle = 0U;
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

static UINT32 rtc_sleep_get_wakeup_seconds(void)
{
    if (g_stLowPowerRtcStatus.mode == NORMAL_MODE)
    {
        return (UINT32)PROJECT_CFG_RTC_NORMAL_WAKE_SECONDS;
    }

    return (UINT32)PROJECT_CFG_RTC_HICCUP_WAKE_SECONDS;
}

static bool rtc_sleep_has_wakeup_exception(void)
{
    enum irqWakeup source = NO_IRQ;

    if (RtcSleep_PortUpdateRtcData() == 0U)
    {
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
    g_stLowPowerRtcStatus.cycles = 0U;
    g_stLowPowerRtcStatus.sleep = 0U;
    LowPowerSleep_SaveCoreState();
    g_irq_t = NO_IRQ;
    MCUO_DEBUG_LED1 = 1;
    lp_refresh_status();
}

static bool rtc_sleep_prepare_stop_cycle(void)
{
    IOstatus_RTCMode();

    if (g_stLowPowerRtcStatus.mode == HICCUP_MODE)
    {
        InitWakeUp_RTCMode();
    }
    else
    {
        InitWakeUp_Base();
    }

    RTC_ClearStopWakeup();
    RTC_SetWakeupPeriodSeconds(rtc_sleep_get_wakeup_seconds());
    RTC_WKTimeConfig();

    if (RTC_GetLastWakeupPeriodSeconds() == 0U)
    {
        return false;
    }

    sys_time.rtc_sec_cnt = RTC_GetCounter();
    return true;
}

static bool rtc_sleep_run_hiccup_cycle(void)
{
    if (!rtc_sleep_prepare_stop_cycle())
    {
        return false;
    }

    RtcSleep_PortEnterStop();

    MCUO_DEBUG_LED1 = 0;
    initAFE1_IIC();

    if ((RTC_IsStopWakeup() != 0U) && !rtc_sleep_has_wakeup_exception())
    {
        ++g_stLowPowerRtcStatus.cycles;
        g_stLowPowerRtcStatus.sleep += RtcSleep_PortGetLastWakeupSeconds();
        g_stLowPowerRtcStatus.test_sample_voltage = g_stCellInfoReport.u16VCell[0];

        RtcSleep_PortApplySocRtcRest(g_stLowPowerRtcStatus.sleep);
        lp_refresh_status();

        if ((g_stCellInfoReport.u16VCellMin <= AFE_Parameters_RS485_Struction.u16VcellUvp.curValue) ||
            !SystemRuntime_IsDischargeMosOpen())
        {
            low_power_log_and_commit_sleep(DEEP_MODE);
        }

        MCUO_DEBUG_LED1 = 1;
        return true;
    }
    else if ((g_stLowPowerRtcStatus.mode == NORMAL_MODE) && (RTC_IsStopWakeup() == 0U))
    {
        extern UINT8 SleepDeal_IsWakeupValid(void);

        if (SleepDeal_IsWakeupValid())
        {
            return false;
        }

        MCUO_DEBUG_LED1 = 1;
        return true;
    }

    return false;
}

void rtc_sleep(void)
{
    uint8_t sleep_mode;

    if (RtcSleep_PortIsOneSecondTick() == 0U)
    {
        lp_refresh_status();
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

        if (sleep_mode == NORMAL_MODE)
        {
            MCUO_AFE_CTLC = 0;
        }

        while (rtc_sleep_run_hiccup_cycle())
        {
        }

        if (sleep_mode == NORMAL_MODE)
        {
            MCUO_AFE_CTLC = 1;
        }

        RtcSleep_PortDisableStopWakeup();
        LowPower_Request(NO_SLEEP);
        RtcSleep_PortRestoreAfterStop();
        RTC_ClearStopWakeup();
        lp_refresh_status();

        g_stLowPowerRtcStatus.last = g_stLowPowerRtcStatus.sleep;
        RtcSleep_PortAddRuntimeSeconds(g_stLowPowerRtcStatus.sleep);
        break;

    default:
        LowPower_Request(NO_SLEEP);
        break;
    }
}
