#include "main.h"
#include "FactoryAging.h"
#include "rtc_sleep_port.h"
#include "DataDeal.h"
#include "conf.h"
#include "Sci_Upper.h"
#include "RTC.h"
#include "IrqDebug.h"

#define LOW_POWER_FORCE_DEEP_SLEEP_MV ((uint16_t)2800U)
#define LOW_POWER_FORCE_DEEP_SLEEP_SECONDS ((uint16_t)(60 * 10))
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

uint32_t LP_GetBlockReason(void)
{
    uint32_t reason = 0U;
    uint8_t comm = RtcSleep_PortGetExternalCommCounter();

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

    if (comm != g_stLowPowerRtcStatus.comm)
    {
        g_stLowPowerRtcStatus.comm = comm;
        reason |= LP_BLOCK_EXT_COMM;
    }

    if ((FactoryAging_IsActive() != 0U))
    {
        reason |= LP_BLOCK_AGING;
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

    return reason;
}

static void lp_refresh_status(void)
{
    g_stLowPowerRtcStatus.rtc = RtcSleep_PortIsRtcWake();
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

static bool rtc_sleep_has_wakeup_exception(void)
{
    enum irqWakeup source = NO_IRQ;

    if (RtcSleep_PortUpdateRtcData() == 0U)
    {
        return true;
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

    return (RtcSleep_PortIsEmergencyWakeVoltage() != 0U);
}

static void rtc_sleep_prepare_rtc(void)
{
    IrqDebug_SetPhase((uint8_t)IRQDBG_PHASE_SLEEP_PREPARE);

    RtcSleep_PortPrepareRtcStop();
    RTC_ClearStopWakeup();
    g_irq_t = NO_IRQ;
    lp_refresh_status();
}

static bool rtc_sleep_run_hiccup_cycle(void)
{
    rtc_sleep_prepare_rtc();
    RTC_WKTimeConfig();

    RtcSleep_PortEnterStop();
    // RtcSleep_PortDisableStopWakeup();

    extern void test_rtc_led_display(void);
    test_rtc_led_display();
    initAFE1_IIC();

    if ((RtcSleep_PortIsRtcWake() != 0U) && !rtc_sleep_has_wakeup_exception())
    {
        ++g_stLowPowerRtcStatus.cycles;
        g_stLowPowerRtcStatus.sleep += RtcSleep_PortGetLastWakeupSeconds();
        g_stLowPowerRtcStatus.test_sample_voltage = g_stCellInfoReport.u16VCell[0];

        RtcSleep_PortApplySocRtcRest(g_stLowPowerRtcStatus.sleep);
        lp_refresh_status();

        MCUO_DEBUG_LED1 = 1;
        return true;
    }

    RTC_ClearStopWakeup();
    LowPower_Request(NO_SLEEP);
    // todo ◊–œ∏ ·¿Ìœ¬≈‰÷√
    RtcSleep_PortRestoreAfterStop();

    // if (g_irq_t == NO_IRQ)
    // {
    //     g_irq_t = RtcSleep_PortGuessWakeupSource();
    // }
    // RtcSleep_PortOnWakeupSource(g_irq_t);
    g_stLowPowerRtcStatus.last = g_stLowPowerRtcStatus.sleep;
    RtcSleep_PortAddRuntimeSeconds(g_stLowPowerRtcStatus.sleep);
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
    case NORMAL_MODE:
    case DEEP_MODE:
        low_power_log_and_commit_sleep(sleep_mode);
        break;
    case HICCUP_MODE:
        g_stLowPowerRtcStatus.cycles = 0U;
        g_stLowPowerRtcStatus.sleep = 0U;
        Init_RTC();
        IOstatus_RTCMode();
        InitWakeUp_RTCMode();

        while (rtc_sleep_run_hiccup_cycle())
        {
        }
        RtcSleep_PortDisableStopWakeup();
        break;
    default:
        LowPower_Request(NO_SLEEP);
        break;
    }
}
