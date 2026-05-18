#include "main.h"
#include "LowPowerSleep.h"

#undef LOG_TAG
#define LOG_TAG "rtc_sleep"

#define LOW_POWER_FORCE_DEEP_SLEEP_MV      ((UINT16)2800U)
#define LOW_POWER_FORCE_DEEP_SLEEP_SECONDS ((UINT16)60U)
#define LOW_POWER_DEEP_SLEEP_ICHG_LIMIT    ((UINT16)5U)



enum irqWakeup g_irq_t = NO_IRQ;

void rtc_sleep(void);
void App_LowPowerProcess(void);
void test_dealError(void);
void Fault_ChangeToMCU(void);
void DataLoad_CellVolt(void);
void DataLoad_CellVoltMaxMinFind(void);
void DataLoad_Temperature(void);
void DataLoad_TemperatureMaxMinFind(void);
void DataLoad_Current(void);

static bool rtc_monitor(void);
static bool isException(void);
static bool updataData_rtc(void);
static void report_wkup_sig(void);
static UINT8 low_power_get_rtc_block_reason(void);
static bool low_power_is_mcu_wake_active(void);
static void low_power_delay_rtc(UINT8 reason);
static void low_power_cancel_rtc(UINT8 reason);
static UINT8 low_power_is_idle_rtc_request(void);
static void low_power_set_rtc_block_reason(UINT8 reason);
static void low_power_update_rtc_status(void);

static void before_wakeup(uint32_t *_sleep_cnt);
static void before_rtcsleep(void);
static uint32_t rtc_sleep_get_period_seconds(void);
static UINT8 low_power_is_reset_sleep_mode(enum _SLEEP_MODE mode);
static void low_power_prepare_reset_sleep(void);
static void low_power_log_and_commit_sleep(void);
static void low_power_guess_wakeup_source(void);
static void rtc_sleep_prepare_rtc(void);
static void rtc_sleep_disable_stop_wakeup(void);
static void rtc_sleep_dump_state(const char *stage);
static void rtc_sleep_restore_after_stop(void);
static void rtc_sleep_restore_for_run(void);
static bool rtc_sleep_run_hiccup_cycle(void);

#if (AFE_TYPE == sh36xx)
static bool isHaveCurrent_sh3x(void);
static bool rtc_monitor_sh367309(void);
static bool updataData_rtc_sh3x(void);
#elif (AFE_TYPE == bq76xx_afe)
static bool updataData_rtc_bq7x(void);
static bool isHaveCurrent_bq7x(void);
static bool rtc_monitor_bq7x(void);
#endif

static bool update_rtc_soc(uint32_t *_sleep_cnt);

typedef struct
{
    uint8_t soc_disp;
    uint8_t soc_real;
    uint8_t soc_min;
    uint8_t soc_max;
    uint8_t soc_mean;
} SOC_T;

SOC_T g_Psoc;

typedef struct
{
    enum _SLEEP_MODE mode;
    UINT8 armed;
} LOW_POWER_RUNTIME_STATE;

static LOW_POWER_RUNTIME_STATE s_stLowPowerRuntime = { NO_SLEEP, 0U };
static uint32_t s_u32RtcSleepElapsedSeconds = 0U;
bool is_wakeup = false;
volatile struct LOW_POWER_RTC_STATUS g_stLowPowerRtcStatus;

static bool low_power_is_mcu_wake_active(void)
{
    return GPIO_ReadInputDataBit(GPIO_MCU_WK, PIN_MCU_WK) != Bit_RESET;
}

static void low_power_set_rtc_block_reason(UINT8 reason)
{
    g_stLowPowerRtcStatus.u8BlockReason = reason;
}

static void low_power_delay_rtc(UINT8 reason)
{
    sys_time.enter_rtc_delay = 0;
    low_power_set_rtc_block_reason(reason);
    low_power_update_rtc_status();
}

static void low_power_cancel_rtc(UINT8 reason)
{
    low_power_delay_rtc(reason);
    if (low_power_is_idle_rtc_request() != 0U)
    {
        LowPower_Request(NO_SLEEP);
        low_power_update_rtc_status();
    }
}

static UINT8 low_power_is_idle_rtc_request(void)
{
    return (UINT8)(s_stLowPowerRuntime.mode == HICCUP_MODE);
}

static void low_power_update_rtc_status(void)
{
    g_stLowPowerRtcStatus.u8SleepModeSelect = (UINT8)s_stLowPowerRuntime.mode;
    g_stLowPowerRtcStatus.u8StateSleep = s_stLowPowerRuntime.armed;
    g_stLowPowerRtcStatus.u8RtcWakeFlag = is_rtc_wakekup ? 1U : 0U;
    g_stLowPowerRtcStatus.u16EnterRtcDelay = sys_time.enter_rtc_delay;
    g_stLowPowerRtcStatus.u16EnterRtcTarget = sys_time.time_enter_rtc;
    g_stLowPowerRtcStatus.u32RtcElapsedSeconds = s_u32RtcSleepElapsedSeconds;
}

void print_vcell(void)
{
    uint8_t i;
    for (i = 0; i < OtherElement.u16Sys_SeriesNum; i++)
    {
        log_i("cell%d  %d\n", i, g_stCellInfoReport.u16VCell[i]);
    }
    log_i("vcelltotle %d", g_stCellInfoReport.u16VCellTotle);
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
        // log_d("cell%d  %d\n", i, g_stCellInfoReport.u16VCell[i]);
        if (g_stCellInfoReport.u16VCell[i] > PRT_E2ROMParas.u16VcellOvp_Third)
        {
            log_i("i = %d, vol= %d\n", i, g_stCellInfoReport.u16VCell[i]);
            break;
        }
    }
    // for (j = 0; j < OtherElement.u16Sys_SeriesNum; j++)
    // {
    //     log_w("cell%d  %d\n", i, g_stCellInfoReport2.u16VCell[j]);

    //     if (g_stCellInfoReport2.u16VCell[j] > g_tParam.protect.u16VcellOvp_Third)
    //     {
    //         log_w("j = %d, vol= %d\n", j, g_stCellInfoReport2.u16VCell[j]);
    //         break;
    //     }
    // }
    // log_w("i = %d, snum= %d\n", i, OtherElement.u16Sys_SeriesNum);
    // if (i == OtherElement.u16Sys_SeriesNum && j == OtherElement.u16Sys_SeriesNum)
    if (i == OtherElement.u16Sys_SeriesNum)
    {
        return false;
    }

    g_irq_t = cov_wake;
    log_w("cov fault\n");

    return true;
}

// void cpu_frequency_conf(uint8_t Mhz)
void cpu_frequency_conf(void)
{
    SystemInit();
    SystemCoreClockUpdate();
    InitDelay();
}

bool isHaveCurrent(void)
{
    bool isCURR = false;

#if AFE_TYPE == bq76xx_afe
    isCURR = isHaveCurrent_bq7x();
#elif AFE_TYPE == sh36xx
    isCURR = isHaveCurrent_sh3x();
#else
#error "error!!!"
#endif

    return isCURR;
}

#if (AFE_TYPE == sh36xx)
static bool isHaveCurrent_sh3x(void)
{
    bool isCURR = false;
    uint16_t current = SH367309_Read_AFE1.u16Current;
    (void)current;

    DataLoad_Current();
    // DataLoad_Current_OK();

    log_i("ichg %d\n", g_stCellInfoReport.u16Ichg);
    log_i("dsg %d\n", g_stCellInfoReport.u16IDischg);
#if 0
	if(ModulusSub(current, su16_OffsetValue) < 1)
#endif

    if (g_stCellInfoReport.u16Ichg)
    {
        isCURR = true;
        g_irq_t = current_wake;
        log_w("afe current V %d, ICHG %d", current, g_stCellInfoReport.u16Ichg);
    }
    if (g_stCellInfoReport.u16IDischg)
    {
        isCURR = true;
        g_irq_t = current_wake;
        log_w("afe current V %d, IDSG %d", current, g_stCellInfoReport.u16IDischg);
    }

    return isCURR;
}
#endif

#if (AFE_TYPE == bq76xx_afe)
static bool isHaveCurrent_bq7x(void)
{
    bool isCURR = false;

    uint16_t current = g_stBq769x0_Read_AFE1.u16Current;
    (void)current;

    DataLoad_Current();

#if defined(_DEBUG_)
    if (g_stCellInfoReport.u16Ichg)
        log_w("afe current V %d, ICHG %d", current, g_stCellInfoReport.u16Ichg);
    if (g_stCellInfoReport.u16IDischg)
        log_w("afe current V %d, IDSG %d", current, g_stCellInfoReport.u16IDischg);

    if (g_stCellInfoReport.u16Ichg >= OtherElement.u16Sleep_VirCur_Chg)
    {
        isCURR = true;
        g_irq_t = current_wake;
    }
    if (g_stCellInfoReport.u16IDischg >= OtherElement.u16Sleep_VirCur_Dsg)
    {
        isCURR = true;
        g_irq_t = current_wake;
    }
#else
    if (g_stCellInfoReport.u16Ichg)
    {
        isCURR = true;
        g_irq_t = current_wake;
        log_w("afe current V %d, ICHG %d", current, g_stCellInfoReport.u16Ichg);
    }
    if (g_stCellInfoReport.u16IDischg)
    {
        isCURR = true;
        g_irq_t = current_wake;
        log_w("afe current V %d, IDSG %d", current, g_stCellInfoReport.u16IDischg);
    }
#endif

    return isCURR;
}
#endif

static UINT8 low_power_is_reset_sleep_mode(enum _SLEEP_MODE mode)
{
    return (UINT8)((mode == NORMAL_MODE) || (mode == DEEP_MODE));
}

static void low_power_prepare_reset_sleep(void)
{
    LogRecord_Flag.bits.Log_Sleep = 1;
    s_stLowPowerRuntime.armed = 1;
    low_power_update_rtc_status();
}

static void low_power_log_and_commit_sleep(void)
{
    extern UINT32 su32_Interval_S_Tcnt;
    UINT8 sleep_mode = (UINT8)s_stLowPowerRuntime.mode;

    if (low_power_is_reset_sleep_mode(s_stLowPowerRuntime.mode) == 0U)
    {
        LowPower_Request(NO_SLEEP);
        return;
    }

    Can_PrepareSleep();
    LogRecord_Flag.bits.Log_Sleep = 1;
    LogEvent_Record(LogRecord_Flag.bits.Log_Sleep, BMS_SLEEP, &su32_Interval_S_Tcnt);
    SleepDeal_Continue(sleep_mode);
}

void LowPower_Request(enum _SLEEP_MODE mode)
{
    switch (mode)
    {
    case HICCUP_MODE:
        s_stLowPowerRuntime.mode = HICCUP_MODE;
        s_stLowPowerRuntime.armed = 0;
        break;
    case NORMAL_MODE:
        s_stLowPowerRuntime.mode = NORMAL_MODE;
        s_stLowPowerRuntime.armed = 0;
        break;
    case DEEP_MODE:
        s_stLowPowerRuntime.mode = DEEP_MODE;
        s_stLowPowerRuntime.armed = 0;
#ifdef __FUNC__LED__
        set_LED_state(LED_BAR_NORMAL, 4);
#endif // DEBUG
        break;
    case NO_SLEEP:
        s_stLowPowerRuntime.mode = NO_SLEEP;
        s_stLowPowerRuntime.armed = 0;
        break;
    default:
        break;
    }

    low_power_update_rtc_status();
}

void LowPower_ClearToSleepFlag(void)
{
    s_stLowPowerRuntime.armed = 0;
    low_power_update_rtc_status();
}

UINT8 LowPower_IsToSleepPending(void)
{
    return (UINT8)(s_stLowPowerRuntime.armed != 0U);
}

void entersleep(enum _SLEEP_MODE mode)
{
    LowPower_Request(mode);
}

#if (AFE_TYPE == bq76xx_afe)

UINT8 AFE_SleepMode_Judge(void)
{
    UINT8 result = 0;

    if (g_stCellInfoReport.u16VCellMin <= OtherElement.u16Sleep_Vlow && !g_stCellInfoReport.u16Ichg)
    {
        result = 1;
        log_e("cuv sleep");
    }
    else if (System_ERROR_UserCallback(ERROR_STATUS_AFE1))
    {
        result = 1;
        log_e(enumToStr(ERROR_STATUS_AFE1));
    }
    else if (System_ERROR_UserCallback(ERROR_STATUS_AFE2))
    {
        result = 1;
        log_e(enumToStr(ERROR_STATUS_AFE2));
    }
    else if ((System_ERROR_UserCallback(ERROR_STATUS_CBC_DSG)))
    {
        result = 1;
        log_e(enumToStr(ERROR_STATUS_CBC_DSG));
    }
    // else if (g_stCellInfoReport.unMdlFault_Third.all)
    // {
    //     result = 2;
    //     log_e("in faulting");
    // }
    else
    {
        result = 0;
    }

    return result;
}

#elif (AFE_TYPE == sh36xx)

UINT8 AFE_SleepMode_Judge(void)
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

#endif

UINT16 gu8_WakeUp_Type = 0;
void BQ769x0_SleepMode_Ctrl(void)
{
    static UINT8 su8_SleepExtComCnt = 0;
    static uint32_t deepsleep_cnt = 0;
    static uint16_t deepsleep_cnt_1min = 0;
    UINT8 block_reason;

    if ((g_stCellInfoReport.u16VCellMin <= LOW_POWER_FORCE_DEEP_SLEEP_MV) &&
        (g_stCellInfoReport.u16Ichg <= LOW_POWER_DEEP_SLEEP_ICHG_LIMIT))
    {
        sys_time.enter_rtc_delay = 0;
        low_power_set_rtc_block_reason(LOW_POWER_RTC_BLOCK_NONE);
        ++deepsleep_cnt_1min;
        if (deepsleep_cnt_1min >= LOW_POWER_FORCE_DEEP_SLEEP_SECONDS)
        {
            entersleep(DEEP_MODE);
        }
        low_power_update_rtc_status();
        return;
    }

    if (g_stCellInfoReport.u16VCellMin <= OtherElement.u16Sleep_Vlow &&
        (g_stCellInfoReport.u16Ichg <= LOW_POWER_DEEP_SLEEP_ICHG_LIMIT))
    {
        sys_time.enter_rtc_delay = 0;
        low_power_set_rtc_block_reason(LOW_POWER_RTC_BLOCK_NONE);
        if (++deepsleep_cnt >= (uint32_t)OtherElement.u16Sleep_TimeVlow * 60U)
        {
            entersleep(DEEP_MODE);
        }
        log_w("%d s enter deep sleep", (60 * OtherElement.u16Sleep_TimeVlow - deepsleep_cnt));
        low_power_update_rtc_status();
        return;
    }

    if (low_power_is_mcu_wake_active())
    {
        deepsleep_cnt = 0;
        deepsleep_cnt_1min = 0;
        low_power_cancel_rtc(LOW_POWER_RTC_BLOCK_MCU_WAKE);
        return;
    }

    if (FactoryAging_IsActive() != 0U)
    {
        deepsleep_cnt = 0;
        deepsleep_cnt_1min = 0;
        low_power_cancel_rtc(LOW_POWER_RTC_BLOCK_FACTORY_AGING);
        return;
    }

    deepsleep_cnt = 0;
    deepsleep_cnt_1min = 0;

    block_reason = low_power_get_rtc_block_reason();
    if ((block_reason == LOW_POWER_RTC_BLOCK_NONE) && (su8_SleepExtComCnt != RTC_ExtComCnt))
    {
        su8_SleepExtComCnt = RTC_ExtComCnt;
        block_reason = LOW_POWER_RTC_BLOCK_EXT_COMM;
    }
    if ((block_reason == LOW_POWER_RTC_BLOCK_NONE) && (AFE_SleepMode_Judge() != 0U))
    {
        block_reason = LOW_POWER_RTC_BLOCK_AFE_NOT_IDLE;
    }

    if (block_reason != LOW_POWER_RTC_BLOCK_NONE)
    {
        low_power_delay_rtc(block_reason);
        return;
    }

    low_power_set_rtc_block_reason(LOW_POWER_RTC_BLOCK_NONE);
    if (++sys_time.enter_rtc_delay >= sys_time.time_enter_rtc)
    {
        sys_time.enter_rtc_delay = 0;
        entersleep(HICCUP_MODE);
        log_w("enter rtc mode 1\n");
    }

    low_power_update_rtc_status();
}

void App_LowPowerProcess(void)
{
    rtc_sleep();
}

void sleep(void)
{
    App_LowPowerProcess();
}

static bool rtc_monitor(void)
{
    bool result = false;

#if (AFE_TYPE == bq76xx_afe)
    result = rtc_monitor_bq7x();
#elif (AFE_TYPE == sh36xx)
    result = rtc_monitor_sh367309();
#else
#error "error!!!"
#endif

    return result;
}

#if (AFE_TYPE == bq76xx_afe)
static bool rtc_monitor_bq7x(void)
{
    bool result = false;

    I2CReadRegisterByteWithCRC(DEVICE_ADDR_AFE1, SYS_CTRL2, &(Registers_AFE1.SysCtrl2.SysCtrl2Byte));
    I2CReadRegisterByteWithCRC(DEVICE_ADDR_AFE1, SYS_STAT, &(Registers_AFE1.SysStatus.StatusByte));

    if (!Registers_AFE1.SysCtrl2.SysCtrl2Bit.CHG_ON || !Registers_AFE1.SysCtrl2.SysCtrl2Bit.DSG_ON)
    {
        g_irq_t = chg_dsg_close;
        log_e("chg dsg off\n");

        result = true;
    }

    return result;
}
#endif

#if (AFE_TYPE == sh36xx)
static bool rtc_monitor_sh367309(void)
{
    // if (!sys_time.power_on)
    // {
    //     // todo 冗余检测
    //     return false;
    // }

    bool result = false;
    if (MTPRead(MTP_BALANCEH, 5, &SH367309_Reg_Store.u8_MTP_BALANCEH))
    {
        // g_stCellInfoReport.u16BalanceFlag1 = SH367309_Reg_Store.u8_MTP_BALANCEL;
        // g_stCellInfoReport.u16BalanceFlag2 = SH367309_Reg_Store.u8_MTP_BALANCEH;
        // SystemStatus.bits.b1Status_MOS_PRE = SH367309_Reg_Store.REG_BSTATUS3.bits.PCHG_FET;
        SystemStatus.bits.b1Status_MOS_CHG = SH367309_Reg_Store.REG_BSTATUS3.bits.CHG_FET;
        SystemStatus.bits.b1Status_MOS_DSG = SH367309_Reg_Store.REG_BSTATUS3.bits.DSG_FET;

        Fault_ChangeToMCU();
#if 0
		if (SH367309_Reg_Store.REG_BSTATUS1.bits.OV)
		{
			result = true;
			log_w("过压\n");
		}
		if (SH367309_Reg_Store.REG_BSTATUS1.bits.UV)
		{
			result = true;
			log_w("低压\n");
		}
#endif
        // if (!SystemStatus.bits.b1Status_MOS_CHG)
        // {
        //     result = true;
        //     log_w("CHG close\n");
        //     g_irq_t = chg_dsg_close;
        // }
        if (!SystemStatus.bits.b1Status_MOS_DSG)
        {
            result = true;
            log_w("DSG close\n");
            g_irq_t = chg_dsg_close;
        }

        if (g_stCellInfoReport.unMdlFault_Third.all != 0U)
        {
            result = true;
            if (g_irq_t == NO_IRQ)
            {
                g_irq_t = error_wake;
            }
            log_w("afe fault 0x%04x\n", g_stCellInfoReport.unMdlFault_Third.all);
        }
    }
    return result;
}
#endif

static bool isException(void)
{
    // todo rtc起来读 AFE 保护状态2、OCV 逻辑，大电流延时 OCV
    if (!updataData_rtc())
    {
        return true;
    }

    // todo read AFE status and to deal logi
    // if (isHaveCurrent() || rtc_monitor() || isVol_cuv() || isVol_cov() || (g_stCellInfoReport.u16VCellMin <= 2750))
    if (isHaveCurrent() || rtc_monitor() || (g_stCellInfoReport.u16VCellMin <= 2750))
    {
        return true;
    }
    // TOTEST
    // else if (AFE_SleepMode_Judge() == 1)
    // {
    //     log_e("过放休眠");

    //     return true;
    // }

    return false;
}

static bool updataData_rtc(void)
{
#if (AFE_TYPE == bq76xx_afe)

    return updataData_rtc_bq7x();

#elif (AFE_TYPE == sh36xx)

    return updataData_rtc_sh3x();

#else

#error "error!!!"

#endif

    // return true;
}

// todo 需要考虑afe采样时序
#if (AFE_TYPE == bq76xx_afe)
static bool updataData_rtc_bq7x(void)
{
    // if (UpdateVoltageFromBqMaximo(DEVICE_ADDR_AFE1) && UpdateVoltageFromBqMaximo2(DEVICE_ADDR_AFE1))
    if (UpdateVoltageFromBqMaximo(DEVICE_ADDR_AFE1))
    {
        log_e("IIC error!!!!!!!!!!!!!!!!!!!!!!!!!\n");

        return false;
    }
    // if (UpdateVoltageFromBqMaximo2(DEVICE_ADDR_AFE1))
    // {
    // 	log_w("IIC2 error!!!!!!!!!!!!!!!!!!!!!!!!!\n");
    // return false;
    // }
    DataLoad_CellVolt();
    DataLoad_CellVoltMaxMinFind();

#ifdef __test__
    DataLoad_Temperature();
    DataLoad_TemperatureMaxMinFind();
    DataLoad_Current();
#endif

    return true;
}
#endif

#if (AFE_TYPE == sh36xx)
static bool updataData_rtc_sh3x(void)
{
    // if (UpdateVoltageFromBqMaximo(DEVICE_ADDR_AFE1) && UpdateVoltageFromBqMaximo2(DEVICE_ADDR_AFE1))
    if (UpdateVoltageFromBqMaximo())
    {
        log_e("IIC error!!!!!!!!!!!!!!!!!!!!!!!!!\n");

        return false;
    }
    // if (UpdateVoltageFromBqMaximo2(DEVICE_ADDR_AFE1))
    // {
    // 	log_w("IIC2 error!!!!!!!!!!!!!!!!!!!!!!!!!\n");
    // return false;
    // }
    DataLoad_CellVolt();
    DataLoad_CellVoltMaxMinFind();
    DataLoad_Temperature();
    DataLoad_TemperatureMaxMinFind();

    return true;
}
#endif

void get_soc(SOC_T *soc, uint16_t vcell_min, uint16_t vcell_max, uint16_t vcell_mean)
{
}

static void before_wakeup(uint32_t *_sleep_cnt)
{
    // g_stCellInfoReport.SocElement.u16Soc =  get_rtc_soc();
    // temp_soc = get_rtc_soc();

#if 0
#define SOC_ERROR 10
    if (ModulusSub(soc_befor_sleep, temp_soc) < SOC_ERROR)
    {
        set_soc_param(temp_soc, 11, 0);
        log_w("rtc soc ocv success %d old soc %d\n", temp_soc, soc_befor_sleep);
        su32_Interval_S_Tcnt += *_sleep_cnt * RTC_sleepTIME;
    }
    else
    {
        log_w("<error> rtc ocv soc error\n");
    }
#else
    su32_Interval_S_Tcnt += s_u32RtcSleepElapsedSeconds;

    if (su32_Interval_S_Tcnt >= (3600 * 6))
    {
        log_e("sleep_cnt %lu, rtc soc update window reached\n", (unsigned long)(*_sleep_cnt));
    }
    else
    {
        // log_e("sleep cnt %d < %d, no update soc", *_sleep_cnt, g_tParam.dev.time_ocv_soc_rtcing);
        // log_e("sleep cnt %d < %d, no update soc", *_sleep_cnt, g_tParam.dev.time_ocv_soc_rtcing);
    }
    log_a("sleep time %d s", su32_Interval_S_Tcnt);
#endif
}

static void before_rtcsleep(void)
{
    // soc_befor_sleep.soc_disp = g_stCellInfoReport.SocElement.u16Soc;
    // soc_befor_sleep.soc_real = g_stCellInfoReport.soc_real;

    // set_rtc_soc(soc_befor_sleep);

    // s_u16_OCVTime200msCnt_reset();
    // err_flag_reset();
    // SOC_OCV_Fix2_var_reset();
    // todo before sleep clear all cnt
}

static uint32_t rtc_sleep_get_period_seconds(void)
{
    return RTC_GetLastWakeupPeriodSeconds();
}

static void rtc_sleep_prepare_rtc(void)
{
    if (sys_time.rtc_sleep_cnt == 0U)
    {
        s_u32RtcSleepElapsedSeconds = 0U;
    }

    LowPowerSleep_SaveCoreState();
	before_rtcsleep();

    Init_RTC();
    IOstatus_RTCMode();
    InitWakeUp_RTCMode();

    is_rtc_wakekup = false;
    g_irq_t = NO_IRQ;
    low_power_update_rtc_status();
}

static void rtc_sleep_disable_stop_wakeup(void)
{
    LowPower_DisableWakeupExti();
    RTC_DisableStopWakeup();
}

static void rtc_sleep_restore_after_stop(void)
{
    if (is_rtc_wakekup)
    {
        InitRtcWakeupCheck();
    }
    else
    {
        InitRunAfterStopWakeup();
    }
}

static void rtc_sleep_restore_for_run(void)
{
    if (is_rtc_wakekup)
    {
        is_rtc_wakekup = false;
        InitRunAfterStopWakeup();
    }
}

static void rtc_sleep_dump_state(const char *stage)
{
    log_w("[rtc_sleep] %s wake=%d cnt=%d period=%lu can=%d rblk=%d",
          stage,
          is_rtc_wakekup,
          sys_time.rtc_sleep_cnt,
          (unsigned long)Can_GetIdleRtcPeriodSeconds(),
          Can_IsBusActive(),
          g_stLowPowerRtcStatus.u8BlockReason);
}

static void low_power_guess_wakeup_source(void)
{
    if ((g_irq_t != NO_IRQ) || is_rtc_wakekup)
    {
        return;
    }

    if (GPIO_ReadInputDataBit(GPIO_CHG_IN, PIN_CHG_IN) == Bit_RESET)
    {
        g_irq_t = PA0_irq;
        return;
    }

    if (GPIO_ReadInputDataBit(GPIO_SW, PIN_SW) == Bit_RESET)
    {
        g_irq_t = soc_key;
        return;
    }

// #if defined(RS485_WAKEUP_ENABLE)
//     if (GPIO_ReadInputDataBit(GPIO_INT_WK_CMNT, PIN_INT_WK_CMNT) != Bit_RESET)
//     {
//         g_irq_t = rs485_irq;
//         return;
//     }
// #endif

// #if defined(UART1_WAKEUP_ENABLE)
//     if (GPIO_ReadInputDataBit(GPIO_SCI1_RX, PIN_SCI1_RX) != Bit_RESET)
//     {
//         g_irq_t = uart1_irq;
//     }
// #endif
}

static bool rtc_sleep_run_hiccup_cycle(void)
{
    uint32_t rtc_elapsed_seconds = 0U;

    rtc_sleep_prepare_rtc();
    rtc_sleep_dump_state("enter");
    // USART_DeInit(USART1);
    // USART_DeInit(USART2);
    // USART_DeInit(USART3);
    // __delay_ms(100);

    Feed_IWatchDog;
    Sys_StopMode();
    Feed_IWatchDog;

    rtc_sleep_disable_stop_wakeup();

    if (is_rtc_wakekup)
    {
        rtc_elapsed_seconds = rtc_sleep_get_period_seconds();
        ++sys_time.rtc_sleep_cnt;
        s_u32RtcSleepElapsedSeconds += rtc_elapsed_seconds;
        rtc_sleep_dump_state("wake");
    }

    rtc_sleep_restore_after_stop();
    log_w("cnt %d", sys_time.rtc_sleep_cnt);
    if (is_rtc_wakekup && !isException())
    {
        update_rtc_soc(&sys_time.rtc_sleep_cnt);
        Can_RtcWakeService(rtc_elapsed_seconds);
        low_power_update_rtc_status();
        return true;
    }

    rtc_sleep_restore_for_run();
    s_stLowPowerRuntime.armed = 0;
    rtc_sleep_dump_state("exit");
    entersleep(NO_SLEEP);
    low_power_guess_wakeup_source();
    report_wkup_sig();
    before_wakeup(&sys_time.rtc_sleep_cnt);
    sys_time.rtc_sleep_cnt = 0;
    s_u32RtcSleepElapsedSeconds = 0U;
    return false;
}

static uint8_t soc_rtc;

static bool update_rtc_soc(uint32_t *_sleep_cnt)
{
    uint32_t rest_seconds;

    if ((_sleep_cnt == 0) || (*_sleep_cnt == 0U))
    {
        return true;
    }

    rest_seconds = s_u32RtcSleepElapsedSeconds;
    SOC_ApplyRtcRelaxationCompensation(rest_seconds,
                                       g_stCellInfoReport.u16VCellMin,
                                       g_stCellInfoReport.u16VCellMax);
    soc_rtc = SOC_Enhance_Element.u8_SOC;

    log_w("rtc rest %lu s, vmin %u, soc %u",
          (unsigned long)rest_seconds,
          (unsigned int)g_stCellInfoReport.u16VCellMin,
          (unsigned int)SOC_Enhance_Element.u8_SOC);

    return true;
}

uint8_t get_rtc_soc(void)
{
    return soc_rtc;
}

void set_rtc_soc(uint8_t _soc)
{
    soc_rtc = _soc;
}

// void set_irq_wksource(enum irqWakeup irq)
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
        // log_e("uart2_irq");
        log_e(enumToStr(uart2_irq));
        break;
    case uart3_irq:
        // log_e("uart3_irq");
        log_e(enumToStr(uart3_irq));
        break;
    case PA0_irq:
        // log_e("PA0_irq");
        log_e(enumToStr(PA0_irq));
        break;
    case bms_keyirq:
        // log_e("bms_keyirq");
        log_e(enumToStr(bms_keyirq));
        break;
    case soc_key:
        // log_e("soc_key_irq");
        log_e(enumToStr(soc_key));
        break;
    case CHG_IRQ:
        // log_e("CHG_IRQ");
        log_e(enumToStr(CHG_IRQ));
        break;
    case current_wake:
        // log_e("g_test %d, exception wakeup", g_irq_t);
        log_e(enumToStr(current_wake));
        break;
    case chg_dsg_close:
        // log_e("g_test %d, exception wakeup", g_irq_t);
        log_e(enumToStr(chg_dsg_close));
        break;
    case error_wake:
        // log_e("g_test %d, exception wakeup", g_irq_t);
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
    // case :
    //     log_e(enumToStr(error_wake));
    // case error_wake:
    //     log_e(enumToStr(error_wake));
    default:
        log_e("no def");
        break;
    }

    g_irq_t = NO_IRQ;
}

static UINT8 low_power_get_rtc_block_reason(void)
{
    if ((g_stCellInfoReport.u16Ichg > 10) || (g_stCellInfoReport.u16IDischg > 10))
    {
        log_e("CHG or DSG");
        return LOW_POWER_RTC_BLOCK_CURRENT;
    }

    if (SystemStatus.bits.b1Status_Heat)
    {
        log_e("Heating");
        return LOW_POWER_RTC_BLOCK_HEAT;
    }

#ifdef __same_door__
    if (!SystemStatus.bits.b1Status_MOS_CHG || !SystemStatus.bits.b1Status_MOS_DSG)
    {
        log_e("mos close");
        return LOW_POWER_RTC_BLOCK_MOS_OFF;
    }
#else
    // else if (GPIO_ReadInputDataBit(GPIOA, GPIO_Pin_0) == 1)
    // {
    //     log_e("diff door and is CHGING");
    //     return true;
    // }
#endif

    if (low_power_is_mcu_wake_active())
    {
        return LOW_POWER_RTC_BLOCK_MCU_WAKE;
    }

    return LOW_POWER_RTC_BLOCK_NONE;
}

void rtc_sleep(void)
{
    if (g_st_SysTimeFlag.bits.b1Sys1000msFlag == 0U)
    {
        low_power_update_rtc_status();
        return;
    }

    BQ769x0_SleepMode_Ctrl();

    if (s_stLowPowerRuntime.armed == 0U)
    {
        if (s_stLowPowerRuntime.mode == HICCUP_MODE)
        {
            s_stLowPowerRuntime.armed = 1;
            low_power_update_rtc_status();
        }
        else if ((s_stLowPowerRuntime.mode == NORMAL_MODE) || (s_stLowPowerRuntime.mode == DEEP_MODE))
        {
            low_power_prepare_reset_sleep();
        }
        else
        {
            return;
        }
    }

    if (s_stLowPowerRuntime.armed != 1U)
    {
        return;
    }

    switch (s_stLowPowerRuntime.mode)
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
