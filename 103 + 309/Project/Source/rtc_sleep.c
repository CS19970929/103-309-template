#include "rtc_sleep_port.h"
#include "elog.h"
#include "app_lowpower.h"

#undef LOG_TAG
#define LOG_TAG "rtc_sleep"

#define LOW_POWER_FORCE_DEEP_SLEEP_MV      ((uint16_t)2800U)
//todo  待测试确认，更新时间，同步更新文档
#define LOW_POWER_FORCE_DEEP_SLEEP_SECONDS ((uint16_t)60U)
#define LOW_POWER_DEEP_SLEEP_ICHG_LIMIT    ((uint16_t)5U)

static enum irqWakeup g_irq_t = NO_IRQ;
static volatile struct LOW_POWER_RTC_STATUS g_stLowPowerRtcStatus = {
    NO_SLEEP,
    0U,
    LOW_POWER_RTC_BLOCK_NONE,
    0U,
    0U,
    0U,
    0U
};

static uint16_t s_u16IdleDelaySeconds = 0U;
static uint32_t s_u32RtcSleepElapsedSeconds = 0U;
static uint32_t s_u32RtcWakeCycles = 0U;
static uint8_t s_u8RtcSoc = 0U;

static void report_wkup_sig(void);

static void low_power_refresh_rtc_status(void)
{
    g_stLowPowerRtcStatus.rtcWake = RtcSleep_PortIsRtcWake();
    g_stLowPowerRtcStatus.delaySeconds = s_u16IdleDelaySeconds;
    g_stLowPowerRtcStatus.delayTargetSeconds = RtcSleep_PortGetIdleDelayTargetSeconds();
    g_stLowPowerRtcStatus.elapsedSeconds = s_u32RtcSleepElapsedSeconds;
}

static void low_power_set_rtc_block_reason(uint8_t reason)
{
    g_stLowPowerRtcStatus.blockReason = reason;
}

static uint8_t low_power_is_idle_rtc_request(void)
{
    return (uint8_t)(g_stLowPowerRtcStatus.mode == HICCUP_MODE);
}

static void low_power_delay_rtc(uint8_t reason)
{
    s_u16IdleDelaySeconds = 0U;
    low_power_set_rtc_block_reason(reason);
    low_power_refresh_rtc_status();
}

static void low_power_cancel_rtc(uint8_t reason)
{
    low_power_delay_rtc(reason);
    if (low_power_is_idle_rtc_request() != 0U)
    {
        LowPower_Request(NO_SLEEP);
        low_power_refresh_rtc_status();
    }
}

static uint8_t low_power_is_reset_sleep_mode(enum _SLEEP_MODE mode)
{
    return (uint8_t)((mode == NORMAL_MODE) || (mode == DEEP_MODE));
}

static void low_power_prepare_reset_sleep(void)
{
    RtcSleep_PortRequestSleepLog();
    g_stLowPowerRtcStatus.readyToSleep = 1U;
    low_power_refresh_rtc_status();
}

static void low_power_log_and_commit_sleep(void)
{
    uint8_t sleep_mode = g_stLowPowerRtcStatus.mode;

    if (low_power_is_reset_sleep_mode((enum _SLEEP_MODE)sleep_mode) == 0U)
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
        g_stLowPowerRtcStatus.readyToSleep = 0U;
        break;
    default:
        break;
    }

    if (mode == DEEP_MODE)
    {
        RtcSleep_PortOnDeepSleepRequest();
    }

    low_power_refresh_rtc_status();
}

void LowPower_ClearToSleepFlag(void)
{
    g_stLowPowerRtcStatus.readyToSleep = 0U;
    low_power_refresh_rtc_status();
}

uint8_t LowPower_IsToSleepPending(void)
{
    return (uint8_t)(g_stLowPowerRtcStatus.readyToSleep != 0U);
}

void entersleep(enum _SLEEP_MODE mode)
{
    LowPower_Request(mode);
}

static uint8_t low_power_get_rtc_block_reason(void)
{
    if ((RtcSleep_PortGetChargeCurrentMa() > 10U) ||
        (RtcSleep_PortGetDischargeCurrentMa() > 10U))
    {
        log_e("CHG or DSG");
        return LOW_POWER_RTC_BLOCK_CURRENT;
    }

    if (RtcSleep_PortIsMcuWakeActive() != 0U)
    {
        return LOW_POWER_RTC_BLOCK_MCU_WAKE;
    }

    return LOW_POWER_RTC_BLOCK_NONE;
}

static void low_power_select_sleep_mode(void)
{
    static uint8_t last_ext_comm_count = 0U;
    static uint32_t deep_sleep_delay_seconds = 0U;
    static uint16_t force_deep_delay_seconds = 0U;
    uint8_t block_reason;

    if ((RtcSleep_PortGetCellMinMv() <= LOW_POWER_FORCE_DEEP_SLEEP_MV) &&
        (RtcSleep_PortGetChargeCurrentMa() <= LOW_POWER_DEEP_SLEEP_ICHG_LIMIT))
    {
        s_u16IdleDelaySeconds = 0U;
        low_power_set_rtc_block_reason(LOW_POWER_RTC_BLOCK_NONE);
        if (++force_deep_delay_seconds >= LOW_POWER_FORCE_DEEP_SLEEP_SECONDS)
        {
            entersleep(DEEP_MODE);
        }
        low_power_refresh_rtc_status();
        return;
    }

    if ((RtcSleep_PortGetCellMinMv() <= RtcSleep_PortGetLowVoltageSleepMv()) &&
        (RtcSleep_PortGetChargeCurrentMa() <= LOW_POWER_DEEP_SLEEP_ICHG_LIMIT))
    {
        s_u16IdleDelaySeconds = 0U;
        low_power_set_rtc_block_reason(LOW_POWER_RTC_BLOCK_NONE);
        if (++deep_sleep_delay_seconds >= (uint32_t)RtcSleep_PortGetLowVoltageSleepMinutes() * 60U)
        {
            entersleep(DEEP_MODE);
        }
        log_w("%d s enter deep sleep",
              (60U * RtcSleep_PortGetLowVoltageSleepMinutes() - deep_sleep_delay_seconds));
        low_power_refresh_rtc_status();
        return;
    }

    if (RtcSleep_PortIsMcuWakeActive() != 0U)
    {
        deep_sleep_delay_seconds = 0U;
        force_deep_delay_seconds = 0U;
        low_power_cancel_rtc(LOW_POWER_RTC_BLOCK_MCU_WAKE);
        return;
    }

    if (RtcSleep_PortIsFactoryAgingActive() != 0U)
    {
        deep_sleep_delay_seconds = 0U;
        force_deep_delay_seconds = 0U;
        low_power_cancel_rtc(LOW_POWER_RTC_BLOCK_FACTORY_AGING);
        return;
    }

    deep_sleep_delay_seconds = 0U;
    force_deep_delay_seconds = 0U;

    block_reason = low_power_get_rtc_block_reason();
    if ((block_reason == LOW_POWER_RTC_BLOCK_NONE) &&
        (last_ext_comm_count != RtcSleep_PortGetExternalCommCounter()))
    {
        last_ext_comm_count = RtcSleep_PortGetExternalCommCounter();
        block_reason = LOW_POWER_RTC_BLOCK_EXT_COMM;
    }
    if ((block_reason == LOW_POWER_RTC_BLOCK_NONE) &&
        (RtcSleep_PortIsAfeSleepBlocked() != 0U))
    {
        block_reason = LOW_POWER_RTC_BLOCK_AFE_NOT_IDLE;
    }
    if ((block_reason == LOW_POWER_RTC_BLOCK_NONE) &&
        (LP_CanSleep() == 0U))
    {
        block_reason = LOW_POWER_RTC_BLOCK_FRAMEWORK;
    }

    if (block_reason != LOW_POWER_RTC_BLOCK_NONE)
    {
        low_power_delay_rtc(block_reason);
        return;
    }

    low_power_set_rtc_block_reason(LOW_POWER_RTC_BLOCK_NONE);
    if (++s_u16IdleDelaySeconds >= RtcSleep_PortGetIdleDelayTargetSeconds())
    {
        s_u16IdleDelaySeconds = 0U;
        entersleep(HICCUP_MODE);
        log_w("enter rtc mode 1\n");
    }

    low_power_refresh_rtc_status();
}

void App_LowPowerProcess(void)
{
    rtc_sleep();
}

void sleep(void)
{
    App_LowPowerProcess();
}

static bool isException(void)
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
    if (s_u32RtcWakeCycles == 0U)
    {
        s_u32RtcSleepElapsedSeconds = 0U;
    }

    RtcSleep_PortPrepareRtcStop(s_u32RtcWakeCycles);
    RtcSleep_PortClearRtcWake();
    g_irq_t = NO_IRQ;
    low_power_refresh_rtc_status();
}

static void rtc_sleep_dump_state(const char *stage)
{
    log_w("[rtc_sleep] %s wake=%d cnt=%lu period=%lu can=%d rblk=%d",
          stage,
          RtcSleep_PortIsRtcWake(),
          (unsigned long)s_u32RtcWakeCycles,
          (unsigned long)RtcSleep_PortGetCanRtcPeriodSeconds(),
          RtcSleep_PortIsCanBusActive(),
          g_stLowPowerRtcStatus.blockReason);
}

static bool update_rtc_soc(uint32_t *sleep_cnt)
{
    uint32_t rest_seconds;

    if ((sleep_cnt == 0) || (*sleep_cnt == 0U))
    {
        return true;
    }

    rest_seconds = s_u32RtcSleepElapsedSeconds;
    s_u8RtcSoc = RtcSleep_PortApplySocRtcRest(rest_seconds);
    return true;
}

static bool rtc_sleep_run_hiccup_cycle(void)
{
    uint32_t rtc_elapsed_seconds = 0U;

    rtc_sleep_prepare_rtc();
    rtc_sleep_dump_state("enter");

    RtcSleep_PortEnterStop();
    RtcSleep_PortDisableStopWakeup();

    if (RtcSleep_PortIsRtcWake() != 0U)
    {
        rtc_elapsed_seconds = RtcSleep_PortGetLastWakeupSeconds();
        ++s_u32RtcWakeCycles;
        s_u32RtcSleepElapsedSeconds += rtc_elapsed_seconds;
        rtc_sleep_dump_state("wake");
    }

    RtcSleep_PortRestoreAfterStop();
    log_w("cnt %lu", (unsigned long)s_u32RtcWakeCycles);
    if ((RtcSleep_PortIsRtcWake() != 0U) && !isException())
    {
        update_rtc_soc(&s_u32RtcWakeCycles);
        RtcSleep_PortRunCanRtcWakeService(rtc_elapsed_seconds);
        low_power_refresh_rtc_status();
        return true;
    }

    RtcSleep_PortClearRtcWake();
    g_stLowPowerRtcStatus.readyToSleep = 0U;
    rtc_sleep_dump_state("exit");
    entersleep(NO_SLEEP);

    if (g_irq_t == NO_IRQ)
    {
        g_irq_t = RtcSleep_PortGuessWakeupSource();
    }
    report_wkup_sig();

    LP_RecordLastSleepSeconds(s_u32RtcSleepElapsedSeconds);
    RtcSleep_PortAddRuntimeSeconds(s_u32RtcSleepElapsedSeconds);
    s_u32RtcWakeCycles = 0U;
    s_u32RtcSleepElapsedSeconds = 0U;
    return false;
}

uint8_t get_rtc_soc(void)
{
    return s_u8RtcSoc;
}

void set_rtc_soc(uint8_t soc)
{
    s_u8RtcSoc = soc;
}

void set_irq_wksource(uint8_t irq)
{
    g_irq_t = (enum irqWakeup)irq;
}

static void report_wkup_sig(void)
{
    switch (g_irq_t)
    {
    case uart1_irq:
        log_e(enumToStr(uart1_irq));
        break;
    case uart2_irq:
        log_e(enumToStr(uart2_irq));
        break;
    case uart3_irq:
        log_e(enumToStr(uart3_irq));
        break;
    case PA0_irq:
        log_e(enumToStr(PA0_irq));
        break;
    case bms_keyirq:
        log_e(enumToStr(bms_keyirq));
        break;
    case soc_key:
        log_e(enumToStr(soc_key));
        break;
    case CHG_IRQ:
        log_e(enumToStr(CHG_IRQ));
        break;
    case current_wake:
        log_e(enumToStr(current_wake));
        break;
    case chg_dsg_close:
        log_e(enumToStr(chg_dsg_close));
        break;
    case error_wake:
        log_e(enumToStr(error_wake));
        break;
    case cuv_wake:
        log_e(enumToStr(cuv_wake));
        break;
    case cov_wake:
        log_e(enumToStr(cov_wake));
        break;
    case rs485_irq:
        log_e(enumToStr(rs485_irq));
        break;
    default:
        log_e("no def");
        break;
    }

    g_irq_t = NO_IRQ;
}

void rtc_sleep(void)
{
    if (RtcSleep_PortIsOneSecondTick() == 0U)
    {
        low_power_refresh_rtc_status();
        return;
    }

    low_power_select_sleep_mode();

    if (g_stLowPowerRtcStatus.readyToSleep == 0U)
    {
        if (g_stLowPowerRtcStatus.mode == HICCUP_MODE)
        {
            g_stLowPowerRtcStatus.readyToSleep = 1U;
            low_power_refresh_rtc_status();
        }
        else if ((g_stLowPowerRtcStatus.mode == NORMAL_MODE) ||
                 (g_stLowPowerRtcStatus.mode == DEEP_MODE))
        {
            low_power_prepare_reset_sleep();
        }
        else
        {
            return;
        }
    }

    if (g_stLowPowerRtcStatus.readyToSleep != 1U)
    {
        return;
    }

    if ((g_stLowPowerRtcStatus.mode == HICCUP_MODE) &&
        (LP_CanSleep() == 0U))
    {
        low_power_delay_rtc(LOW_POWER_RTC_BLOCK_FRAMEWORK);
        LowPower_Request(NO_SLEEP);
        return;
    }

    switch (g_stLowPowerRtcStatus.mode)
    {
    case NORMAL_MODE:
        log_w("normal sleep\n");
        low_power_log_and_commit_sleep();
        break;
    case HICCUP_MODE:
        while (rtc_sleep_run_hiccup_cycle())
        {
        }
        break;
    case DEEP_MODE:
        log_w("deep sleep\n");
        low_power_log_and_commit_sleep();
        break;
    default:
        LowPower_Request(NO_SLEEP);
        break;
    }
}
