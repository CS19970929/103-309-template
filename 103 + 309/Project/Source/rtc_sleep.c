#include "main.h"

/* Existing DataDeal sampling functions used during RTC wake qualification. */
extern void DataLoad_CellVolt(void);
extern void DataLoad_CellVoltMaxMinFind(void);
extern void DataLoad_Temperature(void);
extern void DataLoad_TemperatureMaxMinFind(void);

#define SLEEP_FORCE_LOW_MV 2600U
#define SLEEP_FORCE_LOW_SECONDS 60U

/* One owner for requests, qualification counters and STOP execution. */
static enum _SLEEP_MODE s_sleep_mode = NO_SLEEP;
static UINT32 s_low_seconds;
static UINT16 s_force_seconds;
static UINT32 s_idle_seconds;
static UINT32 s_last_comm;
static UINT8 s_started;
volatile UINT32 RTC_ExtComCnt;
volatile enum irqWakeup g_irq_t = NO_IRQ;

void rtc_sleep_record_wakeup(enum irqWakeup source)
{
    if (g_irq_t == NO_IRQ)
        g_irq_t = source;
}

void entersleep(enum _SLEEP_MODE mode)
{
    if (mode == NO_SLEEP)
        s_sleep_mode = NO_SLEEP;
    else if (mode == DEEP_MODE ||
             (mode == NORMAL_MODE && s_sleep_mode != DEEP_MODE) ||
             (mode == HICCUP_MODE && s_sleep_mode == NO_SLEEP))
    {
        s_sleep_mode = mode;
#ifdef __FUNC__LED__
        if (mode == DEEP_MODE)
            set_LED_state(LED_BAR_NORMAL, 4);
#endif
    }
}

static bool isVol_cuv(void)
{
    uint8_t i;

    for (i = 0; i < OtherElement.u16Sys_SeriesNum; i++)
    {
        log_i("cell%d  %d\r", i, g_stCellInfoReport.u16VCell[i]);

        if (g_stCellInfoReport.u16VCell[i] < PRT_E2ROMParas.u16VcellUvp_Third)
        {
            log_i("i = %d, vol= %d\n", i, g_stCellInfoReport.u16VCell[i]);
            break;
        }
    }
    if (i == OtherElement.u16Sys_SeriesNum)
    {
        return false;
    }

    g_irq_t = cuv_wake;

    log_w("cuv fault\n");

    return true;
}

static bool isVol_cov(void)
{
    uint8_t i;

    for (i = 0; i < OtherElement.u16Sys_SeriesNum; i++)
    {
        if (g_stCellInfoReport.u16VCell[i] > PRT_E2ROMParas.u16VcellOvp_Third)
        {
            log_i("i = %d, vol= %d\n", i, g_stCellInfoReport.u16VCell[i]);
            break;
        }
    }

    if (i == OtherElement.u16Sys_SeriesNum)
    {
        return false;
    }

    g_irq_t = cov_wake;
    log_w("cov fault\n");

    return true;
}

static bool isHaveCurrent_sh3x(void)
{
#if (AFE_TYPE == sh36xx)

    bool isCURR = false;

    DataLoad_Current();

    log_i("ichg %d\n", g_stCellInfoReport.u16Ichg);
    log_i("dsg %d\n", g_stCellInfoReport.u16IDischg);

    if (g_stCellInfoReport.u16Ichg)
    {
        isCURR = true;
        g_irq_t = current_wake;
        log_w("afe current %d mA, ICHG %d", BmsCurrent_GetCurrent_mA(), g_stCellInfoReport.u16Ichg);
    }
    if (g_stCellInfoReport.u16IDischg)
    {
        isCURR = true;
        g_irq_t = current_wake;
        log_w("afe current %d mA, IDSG %d", BmsCurrent_GetCurrent_mA(), g_stCellInfoReport.u16IDischg);
    }

    return isCURR;
#endif
}

static UINT8 AFE_SleepMode_Judge(void)
{
    UINT8 result = 0;

    if (MTPRead(MTP_BSTATUS1, 3, &SH367309_Reg_Store.REG_BSTATUS1.all))
    {
        if (SH367309_Reg_Store.REG_BSTATUS1.all || SH367309_Reg_Store.REG_BSTATUS2.all || SH367309_Reg_Store.REG_BSTATUS3.bits.L0V || SH367309_Reg_Store.REG_BSTATUS3.bits.PCHG_FET)
        {
            log_e("error can not enter rtc");
            result = 1;
        }
        else
        {
            result = 0;
        }
    }
    else
    {
        log_a("err mtp comm");
        result = 2;
    }

    return result;
}

static bool rtc_monitor_sh367309(void)
{
#if AFE_TYPE == sh36xx

    bool result = false;
    if (MTPRead(MTP_BALANCEH, 5, &SH367309_Reg_Store.u8_MTP_BALANCEH))
    {
        CellBalance_UpdateReport(((UINT16)SH367309_Reg_Store.u8_MTP_BALANCEH << 8)
            | SH367309_Reg_Store.u8_MTP_BALANCEL);
        SystemStatus.bits.b1Status_MOS_CHG = SH367309_Reg_Store.REG_BSTATUS3.bits.CHG_FET;
        SystemStatus.bits.b1Status_MOS_DSG = SH367309_Reg_Store.REG_BSTATUS3.bits.DSG_FET;

        if (!SystemStatus.bits.b1Status_MOS_CHG)
        {
            result = true;
            log_w("CHG close\n");
            g_irq_t = chg_dsg_close;
        }
        if (!SystemStatus.bits.b1Status_MOS_DSG)
        {
            result = true;
            log_w("DSG close\n");
            g_irq_t = chg_dsg_close;
        }
    }
    else
    {
        g_irq_t = error_wake;
        return true;
    }
    if (SH367309_Reg_Store.REG_BSTATUS1.all || SH367309_Reg_Store.REG_BSTATUS2.all ||
        SH367309_Reg_Store.REG_BSTATUS3.bits.L0V || SH367309_Reg_Store.REG_BSTATUS3.bits.PCHG_FET)
    {
        g_irq_t = error_wake;
        return true;
    }
    return result;

#endif
}

static bool isErr_enterRTC(void)
{
    UINT8 balance_data[2];

    /* Read current AFE balance state before requesting IDLE/SLEEP. */
    if (!MTPRead(MTP_BALANCEH, 2, balance_data))
        return true;
    CellBalance_UpdateReport(((UINT16)balance_data[0] << 8) | balance_data[1]);

    if ((g_stCellInfoReport.u16Ichg > 10) || (g_stCellInfoReport.u16IDischg > 10))
    {
        log_e("CHG or DSG");
        return true;
    }

    else if (SystemStatus.bits.b1Status_Heat)
    {
        log_e("Heating");
        return true;
    }
    else if (g_stCellInfoReport.u16BalanceFlag1 != 0u)
    {
        log_e("Balancing");
        return true;
    }
#ifdef __same_door__
    else if (!SystemStatus.bits.b1Status_MOS_CHG || !SystemStatus.bits.b1Status_MOS_DSG)
    {
        log_e("mos close");
        return true;
    }
#else
    else if (GPIO_ReadInputDataBit(GPIOA, GPIO_Pin_0) == 1)
    {
        log_e("diff door and is CHGING");
        return true;
    }
#endif
    else
    {
        return false;
    }
}

static bool sleep_has_pending_work(void)
{
    if (u8FlashUpdateFlag || u8FlashUpdateE2PROM || gu8_Reset_EventRecord ||
        u32E2P_Pro_VolCur_WriteFlag || u32E2P_Pro_Temp_WriteFlag ||
        u32E2P_Pro_Other_WriteFlag || u32E2P_OtherElement1_WriteFlag ||
        u32E2P_HeatCool_WriteFlag || u32E2P_RTC_Element_WriteFlag ||
        u8E2P_SocTable_WriteFlag || u8E2P_CopperLoss_WriteFlag || u8E2P_KB_WriteFlag)
        return true;
    if (Sci_CommonUpperHasPendingWork())
        return true;
    return false;
}

static void sleep_update_request(void)
{
    UINT32 comm = RTC_ExtComCnt;

    if (s_sleep_mode == DEEP_MODE)
        return;
    if (g_stCellInfoReport.u16VCellMin <= SLEEP_FORCE_LOW_MV &&
        g_stCellInfoReport.u16Ichg == 0U)
    {
        s_idle_seconds = 0U;
        if (++s_force_seconds >= SLEEP_FORCE_LOW_SECONDS)
            entersleep(DEEP_MODE);
        return;
    }
    if (g_stCellInfoReport.u16VCellMin <= OtherElement.u16Sleep_Vlow &&
        g_stCellInfoReport.u16Ichg == 0U)
    {
        s_idle_seconds = 0U;
        if (++s_low_seconds >= (UINT32)OtherElement.u16Sleep_TimeVlow * 60U)
            entersleep(DEEP_MODE);
        return;
    }
    s_low_seconds = 0U;
    s_force_seconds = 0U;
    /* Low voltage may escalate NORMAL; idle cannot overwrite explicit requests. */
    if (s_sleep_mode != NO_SLEEP)
        return;
    if (!s_started)
    {
        s_started = 1U;
        s_last_comm = comm;
        return;
    }
    if (comm != s_last_comm || sleep_has_pending_work() || isErr_enterRTC() ||
        AFE_SleepMode_Judge() != 0U)
    {
        s_last_comm = comm;
        s_idle_seconds = 0U;
        return;
    }
    if (++s_idle_seconds >= sys_time.time_enter_rtc)
        entersleep(HICCUP_MODE);
}

static bool sleep_rtc_has_exception(void)
{
    if (UpdateVoltageFromBqMaximo())
    {
        g_irq_t = error_wake;
        return true;
    }
    DataLoad_CellVolt();
    DataLoad_CellVoltMaxMinFind();
    DataLoad_Temperature();
    DataLoad_TemperatureMaxMinFind();
    return isHaveCurrent_sh3x() || rtc_monitor_sh367309() ||
           isVol_cuv() || isVol_cov() ||
           g_stCellInfoReport.u16VCellMin <= SLEEP_FORCE_LOW_MV;
}

static void sleep_restore_run(UINT8 restore_peripherals)
{
    RTC_DisableStopAlarm();
    EXTI->IMR &= ~(EXTI_Line0 | EXTI_Line3 | EXTI_Line7 | EXTI_Line9 | EXTI_Line12);
    EXTI_ClearITPendingBit(EXTI_Line0 | EXTI_Line3 | EXTI_Line7 |
                           EXTI_Line9 | EXTI_Line12);
    NVIC_ClearPendingIRQ(EXTI0_IRQn);
    NVIC_ClearPendingIRQ(EXTI3_IRQn);
    NVIC_ClearPendingIRQ(EXTI9_5_IRQn);
    NVIC_ClearPendingIRQ(EXTI15_10_IRQn);
    if (restore_peripherals)
        InitRunAfterStop();
    s_idle_seconds = 0U;
    s_low_seconds = 0U;
    s_force_seconds = 0U;
    s_last_comm = RTC_ExtComCnt;
    entersleep(NO_SLEEP);
}

static bool sleep_has_active_wakeup(void)
{
    if (GPIO_ReadInputDataBit(GPIO_INT_WK_MCU, PIN_INT_WK_MCU))
    {
        rtc_sleep_record_wakeup(PA0_irq);
        return true;
    }
    if (GPIO_ReadInputDataBit(GPIO_INT_WK_CMNT, PIN_INT_WK_CMNT))
    {
        rtc_sleep_record_wakeup(rs485_irq);
        return true;
    }
    if (!GPIO_ReadInputDataBit(GPIO_KEY1, PIN_KEY1))
    {
        rtc_sleep_record_wakeup(bms_keyirq);
        return true;
    }
    return false;
}

static void sleep_run_rtc(void)
{
    UINT32 start_counter;
    UINT32 primask;
    UINT8 stop_io_prepared = 0U;

    g_irq_t = NO_IRQ;
    is_rtc_wakekup = false;
    if (!Init_RTC())
    {
        entersleep(NO_SLEEP);
        s_idle_seconds = 0U;
        return;
    }
    start_counter = RTC_GetCounter();
    do
    {
        /* Recheck before changing IO: work may have arrived after the idle tick. */
        if (g_irq_t != NO_IRQ || RTC_ExtComCnt != s_last_comm || sleep_has_pending_work() ||
            sleep_has_active_wakeup() || isErr_enterRTC() || AFE_SleepMode_Judge() != 0U)
            break;
        is_rtc_wakekup = false;
        /* Keep the last busy check, wake-source setup and WFI race-free. */
        primask = __get_PRIMASK();
        __disable_irq();
        if (!stop_io_prepared)
        {
            if (!Sci_CommonUpperPrepareStop())
            {
                __set_PRIMASK(primask);
                break;
            }
            /* Power down the communication rail before arming its detector.
               The controlled rail transition can pulse INT_WK_CMNT and must
               not be mistaken for external bus activity. */
            IOstatus_RTCMode();
            InitWakeUp_RTCMode();
            stop_io_prepared = 1U;
        }
        else
        {
            /* AFE/I2C pins were restored for the periodic sample. Wake EXTI is
               already armed, so do not clear a real event on later cycles. */
            IOstatus_RTCMode();
        }
        if (!RTC_WKTimeConfig())
        {
            __set_PRIMASK(primask);
            break;
        }
        if (RTC_ExtComCnt != s_last_comm || sleep_has_pending_work() ||
            sleep_has_active_wakeup() || g_irq_t != NO_IRQ)
        {
            __set_PRIMASK(primask);
            break;
        }
        Feed_IWatchDog;
        Sys_StopMode();
        /* PRIMASK is still set here. Latch the RTC flag synchronously so the
           sleep decision cannot run ahead of the pending RTCAlarm IRQ. */
        RTC_LatchAlarmWake();
        __set_PRIMASK(primask);
        __ISB();
        Feed_IWatchDog;
        initAFE1_IIC();
        if (g_irq_t != NO_IRQ)
            break;
        /* WFI may return for a pending interrupt that is not a configured wake
           source. Recheck all entry guards and retry STOP instead of returning
           to full RUN without a real wake reason. */
        if (!is_rtc_wakekup)
            continue;
        if (sleep_rtc_has_exception())
            break;
    } while (s_sleep_mode == HICCUP_MODE);

    /* Unsigned RTC subtraction handles counter wrap and partial external wakes. */
    su32_Interval_S_Tcnt += RTC_GetCounter() - start_counter;
    sleep_restore_run(stop_io_prepared);
}

void rtc_sleep(void)
{
    if (!TakePendingFlag(&gu8_1000msAccClock_Flag))
        return;
    sleep_update_request();
    if (s_sleep_mode == HICCUP_MODE)
        sleep_run_rtc();
    else if (s_sleep_mode == NORMAL_MODE || s_sleep_mode == DEEP_MODE)
    {
        /* Keep the request pending while a reply or parameter write completes. */
        if (!sleep_has_pending_work() && !SleepDeal_CommitResetSleep((UINT8)s_sleep_mode))
        {
            /* A failed transaction returns to RUN and must qualify a new request. */
            entersleep(NO_SLEEP);
            s_low_seconds = 0U;
            s_force_seconds = 0U;
            s_idle_seconds = 0U;
        }
    }
}
