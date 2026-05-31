#include "main.h"
#include "low_power.h"
#include "bsp_clock.h"
#include "bsp_power.h"
#include "bsp_rtc.h"
#include "rtc_sleep_port.h"
#include "elog.h"

#undef LOG_TAG
#define LOG_TAG "low_power"

/* ── Deep-sleep thresholds ── */
#define LP_FORCE_DEEP_SLEEP_MV       ((uint16_t)2800U)
/* todo: 待测试确认 */
#define LP_FORCE_DEEP_SLEEP_SECONDS  ((uint16_t)60U)
#define LP_DEEP_SLEEP_ICHG_LIMIT     ((uint16_t)5U)

/* ── Unified runtime state ── */
typedef struct {
    LP_State  state;
    uint32_t  block_reason;
    uint32_t  last_sleep_seconds;
    uint8_t   sleep_mode;       /* LP_SleepMode */
    uint8_t   ready_to_sleep;
    uint8_t   rtc_wake;
    uint16_t  idle_delay_seconds;
    uint16_t  idle_delay_target_seconds;
    uint32_t  rtc_sleep_elapsed_seconds;
} LP_Runtime;

static LP_Runtime s_lp;

/* ── RTC HICCUP cycle state ── */
static uint32_t s_rtc_wake_cycles;
static uint8_t  s_rtc_soc;
static enum irqWakeup s_irq_source = NO_IRQ;

/* ── External communication counter (was RTC_ExtComCnt global) ── */
static uint8_t s_ext_comm_cnt;

/* ── Forward declarations ── */
static void lp_select_sleep_mode(void);
static bool lp_hiccup_cycle(void);
static void lp_commit_reset_sleep(void);

/* ═══════════════════════════════════════════════════════════════
   Block reason builder — single source of truth
   ═══════════════════════════════════════════════════════════════ */

static uint32_t LP_BuildBlockReason(void)
{
    uint32_t reason = 0U;
    uint32_t requested_period;

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

    requested_period = BSP_RTC_GetRequestedWakeupPeriodSeconds();
    if ((requested_period != 0U) &&
        (BSP_RTC_IsWakeupPeriodSafe(requested_period) == 0U))
    {
        reason |= LP_BLOCK_IWDG_UNSAFE;
    }

    if (RtcSleep_PortIsFactoryAgingActive() != 0U)
    {
        reason |= LP_BLOCK_FACTORY_AGING;
    }

    if (RtcSleep_PortIsAfeSleepBlocked() != 0U)
    {
        reason |= LP_BLOCK_AFE_BUSY;
    }

    return reason;
}

/* ═══════════════════════════════════════════════════════════════
   Public API
   ═══════════════════════════════════════════════════════════════ */

void LP_Init(void)
{
    s_lp.state                = LP_STATE_RUN;
    s_lp.block_reason         = 0U;
    s_lp.last_sleep_seconds   = 0U;
    s_lp.sleep_mode           = (uint8_t)LP_SLEEP_NONE;
    s_lp.ready_to_sleep       = 0U;
    s_lp.rtc_wake             = 0U;
    s_lp.idle_delay_seconds   = 0U;
    s_lp.rtc_sleep_elapsed_seconds = 0U;
    s_rtc_wake_cycles         = 0U;
    s_rtc_soc                 = 0U;
    s_irq_source              = NO_IRQ;
    s_ext_comm_cnt            = 0U;
}

void LP_Task(void)
{
    s_lp.state = LP_STATE_IDLE_CHECK;
    s_lp.block_reason = LP_BuildBlockReason();

    if (RtcSleep_PortIsOneSecondTick() == 0U)
    {
        s_lp.state = LP_STATE_RUN;
        return;
    }

    lp_select_sleep_mode();

    if (s_lp.ready_to_sleep == 0U)
    {
        if (s_lp.sleep_mode == (uint8_t)LP_SLEEP_HICCUP)
        {
            s_lp.ready_to_sleep = 1U;
        }
        else if ((s_lp.sleep_mode == (uint8_t)LP_SLEEP_NORMAL) ||
                 (s_lp.sleep_mode == (uint8_t)LP_SLEEP_DEEP))
        {
            /* Prepare reset-sleep: save state once */
            RtcSleep_PortRequestSleepLog();
            s_lp.ready_to_sleep = 1U;
        }
        else
        {
            s_lp.state = LP_STATE_RUN;
            return;
        }
    }

    if (s_lp.ready_to_sleep != 1U)
    {
        s_lp.state = LP_STATE_RUN;
        return;
    }

    /* HICCUP: re-check blocker before entering STOP loop */
    if ((s_lp.sleep_mode == (uint8_t)LP_SLEEP_HICCUP) &&
        (LP_BuildBlockReason() != 0U))
    {
        s_lp.sleep_mode = (uint8_t)LP_SLEEP_NONE;
        s_lp.ready_to_sleep = 0U;
        s_lp.state = LP_STATE_RUN;
        return;
    }

    switch ((LP_SleepMode)s_lp.sleep_mode)
    {
    case LP_SLEEP_NORMAL:
        log_w("normal sleep\n");
        lp_commit_reset_sleep();
        break;
    case LP_SLEEP_HICCUP:
        while (lp_hiccup_cycle())
        {
        }
        break;
    case LP_SLEEP_DEEP:
        log_w("deep sleep\n");
        lp_commit_reset_sleep();
        break;
    default:
        s_lp.sleep_mode = (uint8_t)LP_SLEEP_NONE;
        break;
    }

    s_lp.state = LP_STATE_RUN;
}

uint8_t LP_CanSleep(void)
{
    s_lp.block_reason = LP_BuildBlockReason();
    return (s_lp.block_reason == 0U) ? 1U : 0U;
}

uint32_t LP_GetBlockReason(void)
{
    s_lp.block_reason = LP_BuildBlockReason();
    return s_lp.block_reason;
}

void LP_RequestSleep(uint8_t mode)
{
    switch ((LP_SleepMode)mode)
    {
    case LP_SLEEP_HICCUP:
    case LP_SLEEP_NORMAL:
    case LP_SLEEP_DEEP:
    case LP_SLEEP_NONE:
        s_lp.sleep_mode = mode;
        s_lp.ready_to_sleep = 0U;
        break;
    default:
        break;
    }

    if (mode == (uint8_t)LP_SLEEP_DEEP)
    {
        RtcSleep_PortOnDeepSleepRequest();
    }
}

uint8_t LP_IsToSleepPending(void)
{
    return (s_lp.ready_to_sleep != 0U) ? 1U : 0U;
}

void LP_ClearToSleepFlag(void)
{
    s_lp.ready_to_sleep = 0U;
}

uint32_t LP_GetLastSleepSeconds(void)
{
    return s_lp.last_sleep_seconds;
}

void LP_RecordLastSleepSeconds(uint32_t seconds)
{
    s_lp.last_sleep_seconds = seconds;
}

void LP_NotifyExternalComm(void)
{
    s_ext_comm_cnt++;
}

/* ═══════════════════════════════════════════════════════════════
   Sleep mode selection (1-second tick)
   ═══════════════════════════════════════════════════════════════ */

static void lp_select_sleep_mode(void)
{
    static uint8_t  last_ext_comm_count;
    static uint32_t deep_sleep_delay_seconds;
    static uint16_t force_deep_delay_seconds;
    uint32_t block_reason;

    /* ── Force deep sleep: cell voltage critically low ── */
    if ((RtcSleep_PortGetCellMinMv() <= LP_FORCE_DEEP_SLEEP_MV) &&
        (RtcSleep_PortGetChargeCurrentMa() <= LP_DEEP_SLEEP_ICHG_LIMIT))
    {
        s_lp.idle_delay_seconds = 0U;
        if (++force_deep_delay_seconds >= LP_FORCE_DEEP_SLEEP_SECONDS)
        {
            LP_RequestSleep((uint8_t)LP_SLEEP_DEEP);
        }
        return;
    }

    /* ── Low-voltage deep sleep ── */
    if ((RtcSleep_PortGetCellMinMv() <= RtcSleep_PortGetLowVoltageSleepMv()) &&
        (RtcSleep_PortGetChargeCurrentMa() <= LP_DEEP_SLEEP_ICHG_LIMIT))
    {
        s_lp.idle_delay_seconds = 0U;
        if (++deep_sleep_delay_seconds >=
            (uint32_t)RtcSleep_PortGetLowVoltageSleepMinutes() * 60U)
        {
            LP_RequestSleep((uint8_t)LP_SLEEP_DEEP);
        }
        log_w("%d s enter deep sleep",
              (int)(60U * RtcSleep_PortGetLowVoltageSleepMinutes()
                    - deep_sleep_delay_seconds));
        return;
    }

    /* ── MCU_WK active → cancel sleep ── */
    if (RtcSleep_PortIsMcuWakeActive() != 0U)
    {
        deep_sleep_delay_seconds  = 0U;
        force_deep_delay_seconds  = 0U;
        s_lp.idle_delay_seconds   = 0U;
        if (s_lp.sleep_mode == (uint8_t)LP_SLEEP_HICCUP)
        {
            LP_RequestSleep((uint8_t)LP_SLEEP_NONE);
        }
        return;
    }

    /* ── Factory aging active → cancel sleep ── */
    if (RtcSleep_PortIsFactoryAgingActive() != 0U)
    {
        deep_sleep_delay_seconds  = 0U;
        force_deep_delay_seconds  = 0U;
        s_lp.idle_delay_seconds   = 0U;
        if (s_lp.sleep_mode == (uint8_t)LP_SLEEP_HICCUP)
        {
            LP_RequestSleep((uint8_t)LP_SLEEP_NONE);
        }
        return;
    }

    deep_sleep_delay_seconds = 0U;
    force_deep_delay_seconds = 0U;

    /* ── Build unified block reason ── */
    block_reason = LP_BuildBlockReason();

    /* ── External communication: delay if new comm happened ── */
    if ((block_reason == 0U) &&
        (last_ext_comm_count != s_ext_comm_cnt))
    {
        last_ext_comm_count = s_ext_comm_cnt;
        block_reason = LP_BLOCK_EXT_COMM;
    }

    if (block_reason != 0U)
    {
        s_lp.idle_delay_seconds = 0U;
        s_lp.block_reason = block_reason;
        return;
    }

    /* ── All clear → countdown to HICCUP ── */
    s_lp.idle_delay_target_seconds = RtcSleep_PortGetIdleDelayTargetSeconds();
    if (++s_lp.idle_delay_seconds >= s_lp.idle_delay_target_seconds)
    {
        s_lp.idle_delay_seconds = 0U;
        LP_RequestSleep((uint8_t)LP_SLEEP_HICCUP);
        log_w("enter rtc mode 1\n");
    }
}

/* ═══════════════════════════════════════════════════════════════
   HICCUP RTC cycle
   ═══════════════════════════════════════════════════════════════ */

static bool lp_is_exception(void)
{
    enum irqWakeup source = NO_IRQ;

    if (RtcSleep_PortUpdateRtcData() == 0U)
    {
        return true;
    }

    if (RtcSleep_PortHasCurrentWake(&source) != 0U)
    {
        s_irq_source = source;
        return true;
    }

    if (RtcSleep_PortHasAfeWake(&source) != 0U)
    {
        s_irq_source = source;
        return true;
    }

    return (RtcSleep_PortIsEmergencyWakeVoltage() != 0U);
}

static void lp_report_wakeup_source(void)
{
    switch (s_irq_source)
    {
    case uart1_irq:   log_e("uart1_irq");   break;
    case uart2_irq:   log_e("uart2_irq");   break;
    case uart3_irq:   log_e("uart3_irq");   break;
    case PA0_irq:     log_e("PA0_irq");     break;
    case bms_keyirq:  log_e("bms_keyirq");  break;
    case soc_key:     log_e("soc_key");     break;
    case CHG_IRQ:     log_e("CHG_IRQ");     break;
    case current_wake:log_e("current_wake"); break;
    case chg_dsg_close:log_e("chg_dsg_close"); break;
    case error_wake:  log_e("error_wake");  break;
    case cuv_wake:    log_e("cuv_wake");    break;
    case cov_wake:    log_e("cov_wake");    break;
    case rs485_irq:   log_e("rs485_irq");   break;
    default:          log_e("no def");      break;
    }

    s_irq_source = NO_IRQ;
}

static bool lp_hiccup_cycle(void)
{
    uint32_t rtc_elapsed_seconds = 0U;

    if (s_rtc_wake_cycles == 0U)
    {
        s_lp.rtc_sleep_elapsed_seconds = 0U;
    }

    RtcSleep_PortPrepareRtcStop(s_rtc_wake_cycles);
    RtcSleep_PortClearRtcWake();
    s_irq_source = NO_IRQ;

    log_w("[low_power] enter wake=%d cnt=%lu period=%lu can=%d",
          RtcSleep_PortIsRtcWake(),
          (unsigned long)s_rtc_wake_cycles,
          (unsigned long)RtcSleep_PortGetCanRtcPeriodSeconds(),
          RtcSleep_PortIsCanBusActive());

    RtcSleep_PortEnterStop();
    RtcSleep_PortDisableStopWakeup();

    if (RtcSleep_PortIsRtcWake() != 0U)
    {
        rtc_elapsed_seconds = RtcSleep_PortGetLastWakeupSeconds();
        ++s_rtc_wake_cycles;
        s_lp.rtc_sleep_elapsed_seconds += rtc_elapsed_seconds;
        log_w("[low_power] wake cnt=%lu",
              (unsigned long)s_rtc_wake_cycles);
    }

    RtcSleep_PortRestoreAfterStop();

    if ((RtcSleep_PortIsRtcWake() != 0U) && !lp_is_exception())
    {
        /* No exception — apply SOC compensation, CAN service, continue */
        if (s_rtc_wake_cycles > 0U)
        {
            s_rtc_soc = RtcSleep_PortApplySocRtcRest(
                s_lp.rtc_sleep_elapsed_seconds);
        }
        RtcSleep_PortRunCanRtcWakeService(rtc_elapsed_seconds);
        return true;
    }

    /* Exception — exit HICCUP loop */
    RtcSleep_PortClearRtcWake();
    s_lp.ready_to_sleep = 0U;
    log_w("[low_power] exit");

    LP_RequestSleep((uint8_t)LP_SLEEP_NONE);

    if (s_irq_source == NO_IRQ)
    {
        s_irq_source = RtcSleep_PortGuessWakeupSource();
    }
    RtcSleep_PortOnWakeupSource(s_irq_source);
    lp_report_wakeup_source();

    LP_RecordLastSleepSeconds(s_lp.rtc_sleep_elapsed_seconds);
    RtcSleep_PortAddRuntimeSeconds(s_lp.rtc_sleep_elapsed_seconds);
    s_rtc_wake_cycles = 0U;
    s_lp.rtc_sleep_elapsed_seconds = 0U;
    return false;
}

/* ═══════════════════════════════════════════════════════════════
   Reset-sleep commit
   ═══════════════════════════════════════════════════════════════ */

static void lp_commit_reset_sleep(void)
{
    if ((s_lp.sleep_mode != (uint8_t)LP_SLEEP_NORMAL) &&
        (s_lp.sleep_mode != (uint8_t)LP_SLEEP_DEEP))
    {
        LP_RequestSleep((uint8_t)LP_SLEEP_NONE);
        return;
    }

    RtcSleep_PortCommitResetSleep(s_lp.sleep_mode);
}