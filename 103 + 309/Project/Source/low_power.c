#include "main.h"
#include "low_power.h"
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
    uint16_t  idle_delay_seconds;
    uint32_t  rtc_sleep_elapsed_seconds;
} LP_Runtime;

static LP_Runtime s_lp;

/* ── RTC HICCUP cycle state ── */
static uint32_t s_rtc_wake_cycles;
static uint8_t  s_rtc_soc;
static enum irqWakeup s_irq_source = NO_IRQ;

/* ── External communication counter ── */
static uint8_t s_ext_comm_cnt;

/* ── Forward declarations ── */
static void lp_select_sleep_mode(void);
static bool lp_hiccup_cycle(void);
static void lp_commit_reset_sleep(void);

/* ═══════════════════════════════════════════════════════════════
   AFE helpers (inlined from rtc_sleep_afe_sh367309)
   ═══════════════════════════════════════════════════════════════ */

static UINT8 lp_afe_is_sleep_blocked(void)
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

static UINT8 lp_afe_update_data(void)
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

static UINT8 lp_afe_has_current_wake(enum irqWakeup *source)
{
    UINT8 result;

    if (source != 0) { *source = NO_IRQ; }
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
        if (source != 0) { *source = current_wake; }
    }
    return result;
}

static UINT8 lp_afe_has_afe_wake(enum irqWakeup *source)
{
    if (source != 0) { *source = NO_IRQ; }
    if (MTPRead(MTP_BALANCEH, 5, &SH367309_Reg_Store.u8_MTP_BALANCEH))
    {
        SystemRuntime_SetMosStatus(SH367309_Reg_Store.REG_BSTATUS3.bits.CHG_FET,
                                   SH367309_Reg_Store.REG_BSTATUS3.bits.DSG_FET);
        Fault_ChangeToMCU();
        if (!SystemRuntime_IsDischargeMosOpen())
        {
            log_w("DSG close\n");
            if (source != 0) { *source = chg_dsg_close; }
            return 1U;
        }
        if (g_stCellInfoReport.unMdlFault_Third.all != 0U)
        {
            log_w("afe fault 0x%04x\n", g_stCellInfoReport.unMdlFault_Third.all);
            if (source != 0) { *source = error_wake; }
            return 1U;
        }
    }
    return 0U;
}

/* ═══════════════════════════════════════════════════════════════
   Block reason builder — single source of truth
   ═══════════════════════════════════════════════════════════════ */

static uint32_t LP_BuildBlockReason(void)
{
    uint32_t reason = 0U;

    if (g_stCellInfoReport.u16Ichg > 10U)
    {
        reason |= LP_BLOCK_CHARGE;
    }

    if (g_stCellInfoReport.u16IDischg > 10U)
    {
        reason |= LP_BLOCK_DISCHARGE;
    }

    if ((Sci_IsAnyPortBusy() != 0U) ||
        (Can_IsBusy() != 0U))
    {
        reason |= LP_BLOCK_COMM;
    }

    if ((GPIO_ReadInputDataBit(GPIO_MCU_WK, PIN_MCU_WK) != Bit_RESET))
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

    /* IWDG safe — always true with current prescaler, skip check */

    if (FactoryAging_IsActive() != 0U)
    {
        reason |= LP_BLOCK_FACTORY_AGING;
    }

    if (lp_afe_is_sleep_blocked() != 0U)
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

    if (g_st_SysTimeFlag.bits.b1Sys1000msFlag == 0U)
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
            LogRecord_RequestSleep();
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
#ifdef __FUNC__LED__
        set_LED_state(LED_BAR_NORMAL, 4);
#endif
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
    if ((g_stCellInfoReport.u16VCellMin <= LP_FORCE_DEEP_SLEEP_MV) &&
        (g_stCellInfoReport.u16Ichg <= LP_DEEP_SLEEP_ICHG_LIMIT))
    {
        s_lp.idle_delay_seconds = 0U;
        if (++force_deep_delay_seconds >= LP_FORCE_DEEP_SLEEP_SECONDS)
        {
            LP_RequestSleep((uint8_t)LP_SLEEP_DEEP);
        }
        return;
    }

    /* ── Low-voltage deep sleep ── */
    if ((g_stCellInfoReport.u16VCellMin <= OtherElement.u16Sleep_Vlow) &&
        (g_stCellInfoReport.u16Ichg <= LP_DEEP_SLEEP_ICHG_LIMIT))
    {
        s_lp.idle_delay_seconds = 0U;
        if (++deep_sleep_delay_seconds >=
            (uint32_t)OtherElement.u16Sleep_TimeVlow * 60U)
        {
            LP_RequestSleep((uint8_t)LP_SLEEP_DEEP);
        }
        log_w("%d s enter deep sleep",
              (int)(60U * OtherElement.u16Sleep_TimeVlow
                    - deep_sleep_delay_seconds));
        return;
    }

    /* ── MCU_WK active → cancel sleep ── */
    if ((GPIO_ReadInputDataBit(GPIO_MCU_WK, PIN_MCU_WK) != Bit_RESET))
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
    if (FactoryAging_IsActive() != 0U)
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
    if (++s_lp.idle_delay_seconds >= (UINT16)sys_time.time_enter_rtc)
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

    if (lp_afe_update_data() == 0U)
    {
        return true;
    }

    if (lp_afe_has_current_wake(&source) != 0U)
    {
        s_irq_source = source;
        return true;
    }

    if (lp_afe_has_afe_wake(&source) != 0U)
    {
        s_irq_source = source;
        return true;
    }

    return (UINT8)(g_stCellInfoReport.u16VCellMin <= 2750U);
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

    /* Prepare STOP mode */
    (void)s_rtc_wake_cycles;
    Can_PrepareSleep();
    SOC_SaveSnapshotBeforeSleep();
    FactoryAging_SaveProgressBeforeSleep();
    Init_RTC();
    IOstatus_RTCMode();
    InitWakeUp_RTCMode();
    is_rtc_wakekup = false;
    s_irq_source = NO_IRQ;

    log_w("[low_power] enter wake=%d cnt=%lu period=%lu can=%d",
          is_rtc_wakekup ? 1 : 0,
          (unsigned long)s_rtc_wake_cycles,
          (unsigned long)Can_GetIdleRtcPeriodSeconds(),
          Can_IsBusActive());

    Feed_IWatchDog;
    Sys_StopMode();
    Feed_IWatchDog;
    LowPower_DisableWakeupExti();
    RTC_DisableStopWakeup();

    if (is_rtc_wakekup)
    {
        rtc_elapsed_seconds = RTC_GetLastWakeupPeriodSeconds();
        ++s_rtc_wake_cycles;
        s_lp.rtc_sleep_elapsed_seconds += rtc_elapsed_seconds;
        log_w("[low_power] wake cnt=%lu",
              (unsigned long)s_rtc_wake_cycles);
    }

    InitRunAfterStopWakeup();

    if ((is_rtc_wakekup) && !lp_is_exception())
    {
        if (s_rtc_wake_cycles > 0U)
        {
            SOC_ApplyRtcRelaxationCompensation(
                s_lp.rtc_sleep_elapsed_seconds,
                g_stCellInfoReport.u16VCellMin,
                g_stCellInfoReport.u16VCellMax);
            s_rtc_soc = SOC_Enhance_Element.u8_SOC;
            log_w("rtc rest %lu s, vmin %u, soc %u",
                  (unsigned long)s_lp.rtc_sleep_elapsed_seconds,
                  (unsigned int)g_stCellInfoReport.u16VCellMin,
                  (unsigned int)SOC_Enhance_Element.u8_SOC);
        }
        Can_RtcWakeService(rtc_elapsed_seconds);
        return true;
    }

    /* Exception — exit HICCUP loop */
    is_rtc_wakekup = false;
    s_lp.ready_to_sleep = 0U;
    log_w("[low_power] exit");

    LP_RequestSleep((uint8_t)LP_SLEEP_NONE);

    if (s_irq_source == NO_IRQ)
    {
        if (GPIO_ReadInputDataBit(GPIO_CHG_IN, PIN_CHG_IN) == Bit_RESET)
            s_irq_source = PA0_irq;
        else if (GPIO_ReadInputDataBit(GPIO_SW, PIN_SW) == Bit_RESET)
            s_irq_source = soc_key;
    }
    if (s_irq_source == soc_key)
    {
        LedBar_RequestSocDisplay();
        APP_LedBar();
    }
    lp_report_wakeup_source();

    LP_RecordLastSleepSeconds(s_lp.rtc_sleep_elapsed_seconds);
    {
        extern UINT32 su32_Interval_S_Tcnt;
        su32_Interval_S_Tcnt += s_lp.rtc_sleep_elapsed_seconds;
        if (su32_Interval_S_Tcnt >= (3600U * 6U))
            log_e("rtc sleep update window reached\n");
        log_a("sleep time %d s", su32_Interval_S_Tcnt);
    }
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

    {
        extern UINT32 su32_Interval_S_Tcnt;
        Can_PrepareSleep();
        LogRecord_RequestSleep();
        LogEvent_Record(1U, BMS_SLEEP, &su32_Interval_S_Tcnt);
        SleepDeal_Continue(s_lp.sleep_mode);
    }
}