#include "main.h"

/* SH367309 cell register order is linear except 13S cell 9, which maps to VC10. */
static UINT8 DataLoad_CellVoltAfeIndex(UINT8 series_num, UINT8 cell_index)
{
    if (series_num < 5U)
    {
        return 0U;
    }
    if ((series_num == 13U) && (cell_index == 8U))
    {
        return 9U;
    }
    return cell_index;
}

#define MONITOR_AFE_FAIL_LIMIT ((UINT8)50)
#define MONITOR_AFE_RECOVER_TRIGGER ((UINT8)30)
#define MONITOR_AFE_RECOVER_RETRY_STEP ((UINT8)5U)
#define MONITOR_AFE_WAKE_RETRY_LIMIT ((UINT8)20)
#define MONITOR_AFE_TASK_PERIOD_MS ((UINT16)200)
#define MONITOR_AFE_SLEEP_DELAY_SEC ((UINT16)(5U * 60U))
#define MONITOR_AFE_SLEEP_DELAY_TICKS ((UINT16)((MONITOR_AFE_SLEEP_DELAY_SEC * 1000U) / MONITOR_AFE_TASK_PERIOD_MS))
#define AFE_CURRENT_RATIO_SCALE ((UINT32)20U)
#define AFE_CURRENT_RATIO_DENOMINATOR ((UINT32)2147U)
#define AFE_CURRENT_STARTUP_ZERO_LIMIT_MA ((UINT32)1000U)
#define AFE_CURRENT_STARTUP_ZERO_STABLE_RAW ((UINT16)8U)
#define AFE_CURRENT_OUTPUT_DEADBAND_MA ((UINT32)200U)

typedef struct _AFE_CURRENT_STARTUP_ZERO_PARAM
{
    UINT8 u8ConfirmCnt;
    UINT8 u8MaxCnt;
    UINT8 u8DiscardCnt;
    UINT16 u16SettleMs;
    UINT16 u16IntervalMs;
} AFE_CURRENT_STARTUP_ZERO_PARAM;

static const AFE_CURRENT_STARTUP_ZERO_PARAM s_stAfeCurrentColdStartupZeroParam = {4U, 32U, 6U, 800U, 25U};
static const AFE_CURRENT_STARTUP_ZERO_PARAM s_stAfeCurrentWarmStartupZeroParam = {4U, 16U, 2U, 120U, 20U};

typedef struct _AFE_CURRENT_RUNTIME
{
    INT32 zeroOffsetRaw;
} AFE_CURRENT_RUNTIME;

typedef struct _AFE_MONITOR_CH
{
    UINT8 faultCnt;
    UINT8 wakeCnt;
    UINT8 errorReported;
} AFE_MONITOR_CH;

typedef struct _AFE_MONITOR_RUNTIME
{
    AFE_MONITOR_CH ch[2];
    UINT16 sleepDelay[3];
} AFE_MONITOR_RUNTIME;

typedef struct _DATA_RUNTIME
{
    AFE_CURRENT_RUNTIME cur;
    AFE_MONITOR_RUNTIME mon;
    UINT32 afeSeq;
} DATA_RUNTIME;

static DATA_RUNTIME s_data = {0};

UINT16 g_u16CalibCoefK[KB_NUM];
INT16 g_i16CalibCoefB[KB_NUM];
UINT32 g_u32CS_Res_AFE;
struct OTHER_ELEMENT OtherElement;

void charger_detect_and_keyLogi_200ms(void)
{
}

void Init_Registers(UINT8 num)
{
    UINT8 j;
    switch (num)
    {
    case 0:
        for (j = 0; j < 21; j++)
        {
            *(&(Registers_AFE1.Temp1) + j) = 0;
        }
        break;
    case 1:
    default:
        break;
    }
}

void DataLoad_CellVolt(void)
{
    UINT8 i;

    for (i = 0; i < SeriesNum; ++i)
    {
        UINT8 afe_index = DataLoad_CellVoltAfeIndex(SeriesNum, i);
        g_stCellInfoReport.u16VCell[i] = SH367309_Read_AFE1.u16VCell[afe_index];
    }

#ifndef VCELL_DISP_TEST
    if (SeriesNum < 32U)
    {
        for (i = SeriesNum; i < 32U; ++i)
        {
            g_stCellInfoReport.u16VCell[i] = 61001U;
        }
    }
#endif
}

void DataLoad_CellVoltMaxMinFind(void)
{
    UINT8 i;
    UINT16 t_u16VcellTemp;
    UINT16 t_u16VcellMaxTemp;
    UINT16 t_u16VcellMinTemp;
    UINT8 t_u8VcellMaxPosition;
    UINT8 t_u8VcellMinPosition;
    UINT32 u32VCellTotle;

    t_u16VcellMaxTemp = 0;
    t_u16VcellMinTemp = 0x7FFF;
    t_u8VcellMaxPosition = 0;
    t_u8VcellMinPosition = 0;
    u32VCellTotle = 0;

    for (i = 0; i < SeriesNum; i++)
    {
        t_u16VcellTemp = g_stCellInfoReport.u16VCell[i];
        u32VCellTotle += g_stCellInfoReport.u16VCell[i];
        if (t_u16VcellMaxTemp < t_u16VcellTemp)
        {
            t_u16VcellMaxTemp = t_u16VcellTemp;
            t_u8VcellMaxPosition = i;
        }
        if (t_u16VcellMinTemp > t_u16VcellTemp)
        {
            t_u16VcellMinTemp = t_u16VcellTemp;
            t_u8VcellMinPosition = i;
        }
    }

    u32VCellTotle = ((u32VCellTotle * g_u16CalibCoefK[VOLT_VBUS]) >> 10) + (UINT32)g_i16CalibCoefB[VOLT_VBUS] * 1000;

    g_stCellInfoReport.u16VCellTotle = (UINT16)((u32VCellTotle * 1638 >> 14) & 0xFFFF);
    g_stCellInfoReport.u16VCellMax = t_u16VcellMaxTemp;
    g_stCellInfoReport.u16VCellMin = t_u16VcellMinTemp;
    g_stCellInfoReport.u16VCellDelta = t_u16VcellMaxTemp - t_u16VcellMinTemp;
    g_stCellInfoReport.u16VCellMaxPosition = t_u8VcellMaxPosition + 1;
    g_stCellInfoReport.u16VCellMinPosition = t_u8VcellMinPosition + 1;
}

void DataLoad_Temperature(void)
{
    UINT8 i;
    INT32 t_i32temp;
    UINT8 Select;

    Select = 2;
    for (i = 0; i < Select; i++)
    {
        t_i32temp = (INT32)SH367309_Read_AFE1.u16TempBat[i] / 10 - 40;
        t_i32temp = ((t_i32temp * g_u16CalibCoefK[MDL_TEMP1 + i]) + g_i16CalibCoefB[MDL_TEMP1 + i]) >> 10;
        g_stCellInfoReport.u16Temperature[i] = (UINT16)(t_i32temp * 10 + 400);
        Monitor_TempBreak(&g_stCellInfoReport.u16Temperature[i]);
    }

    g_stCellInfoReport.u16Temperature[2] = 0;

    t_i32temp = ADC_GetResult(ADC_TEMP_EV2) / 10 - 40;
    t_i32temp = -40;
    t_i32temp = ((t_i32temp * g_u16CalibCoefK[MDL_TEMP_ENV2]) + g_i16CalibCoefB[MDL_TEMP_ENV2]) >> 10;
    g_stCellInfoReport.u16Temperature[ENV_TEMP2] = (UINT16)(t_i32temp * 10 + 400);

    t_i32temp = ADC_GetResult(ADC_TEMP_EV3) / 10 - 40;
    t_i32temp = -40;
    t_i32temp = ((t_i32temp * g_u16CalibCoefK[MDL_TEMP_ENV3]) + g_i16CalibCoefB[MDL_TEMP_ENV3]) >> 10;
    g_stCellInfoReport.u16Temperature[ENV_TEMP3] = (UINT16)(t_i32temp * 10 + 400);

    t_i32temp = ADC_GetResult(ADC_TEMP_MOS1);
    t_i32temp = t_i32temp / 10 - 40;
    t_i32temp = ((t_i32temp * g_u16CalibCoefK[MDL_TEMP_MOS1]) + g_i16CalibCoefB[MDL_TEMP_MOS1]) >> 10;
    g_stCellInfoReport.u16Temperature[MOS_TEMP1] = (UINT16)(t_i32temp * 10 + 400);
    Monitor_TempBreak(&g_stCellInfoReport.u16Temperature[MOS_TEMP1]);
}

void DataLoad_TemperatureMaxMinFind(void)
{
    UINT8 i;
    UINT16 t_u16VcellTemp;
    UINT16 t_u16VcellMaxTemp;
    UINT16 t_u16VcellMinTemp;
    t_u16VcellMaxTemp = 0;
    t_u16VcellMinTemp = 0x7FFF;

    for (i = 0; i < 7; i++)
    {
        if (g_stCellInfoReport.u16Temperature[i] == 0)
        {
            continue;
        }
        t_u16VcellTemp = g_stCellInfoReport.u16Temperature[i];
        if (t_u16VcellMaxTemp < t_u16VcellTemp)
        {
            t_u16VcellMaxTemp = t_u16VcellTemp;
        }
        if (t_u16VcellMinTemp > t_u16VcellTemp)
        {
            t_u16VcellMinTemp = t_u16VcellTemp;
        }
    }

    g_stCellInfoReport.u16TempMax = t_u16VcellMaxTemp;
    g_stCellInfoReport.u16TempMin = t_u16VcellMinTemp;
}

static INT32 DataLoad_CurrentRawToSigned(UINT16 raw_code)
{
    if ((raw_code & 0x8000U) != 0U)
    {
        return (INT32)raw_code - 65536L;
    }

    return (INT32)raw_code;
}

static UINT32 DataLoad_CurrentAbsI32(INT32 value)
{
    if (value < 0)
    {
        return (UINT32)(-value);
    }

    return (UINT32)value;
}

/* Startup calibration only needs the fixed 1 A acceptance limit. Keeping this
 * specialized avoids 64-bit multiply/divide on Cortex-M3. */
static UINT16 DataLoad_CurrentStartupZeroLimitRaw(void)
{
    UINT32 denominator;
    UINT32 numerator;
    UINT32 raw_code;

    if (g_u32CS_Res_AFE == 0U)
    {
        return 0U;
    }

    /* Valid configuration bounds g_u32CS_Res_AFE to <= 10,000,000. */
    denominator = AFE_CURRENT_RATIO_SCALE * g_u32CS_Res_AFE;
    numerator = AFE_CURRENT_STARTUP_ZERO_LIMIT_MA * AFE_CURRENT_RATIO_DENOMINATOR +
                denominator / 2U;
    raw_code = numerator / denominator;

    return (raw_code > 0x7FFFU) ? 0x7FFFU : (UINT16)raw_code;
}

static UINT32 DataLoad_CurrentRawToMilliAmp(UINT32 raw_abs)
{
    UINT32 scaled_raw;
    UINT32 quotient;
    UINT32 remainder;
    UINT32 base;
    UINT32 fraction;

    if ((raw_abs == 0U) || (g_u32CS_Res_AFE == 0U))
    {
        return 0U;
    }

    /* corrected_raw comes from one signed 16-bit CADC sample and a calibrated
     * signed-16-bit-scale offset, so values above 0xFFFF are non-physical. */
    if (raw_abs > 0xFFFFU)
    {
        return 0xFFFFFFFFU;
    }

    scaled_raw = raw_abs * AFE_CURRENT_RATIO_SCALE;
    quotient = g_u32CS_Res_AFE / AFE_CURRENT_RATIO_DENOMINATOR;
    remainder = g_u32CS_Res_AFE % AFE_CURRENT_RATIO_DENOMINATOR;

    if ((quotient != 0U) && (scaled_raw > (0xFFFFFFFFU / quotient)))
    {
        return 0xFFFFFFFFU;
    }

    base = scaled_raw * quotient;
    /* scaled_raw <= 1,310,700 and remainder <= 2,146, so this product fits U32. */
    fraction = (scaled_raw * remainder +
                (AFE_CURRENT_RATIO_DENOMINATOR / 2U)) /
               AFE_CURRENT_RATIO_DENOMINATOR;

    if (base > (0xFFFFFFFFU - fraction))
    {
        return 0xFFFFFFFFU;
    }

    return base + fraction;
}

static UINT8 DataLoad_CurrentReadCadcRaw(UINT16 *raw_code)
{
    UINT16 raw_be;

    if (raw_code == 0)
    {
        return 0U;
    }

    raw_be = 0U;
    if (MTPRead(MTP_ADC2, 2, (UINT8 *)&raw_be))
    {
        *raw_code = U16_SwapEndian(raw_be);
        SH367309_Read_AFE1.u16Current = *raw_code;
        return 1U;
    }

    return 0U;
}

UINT32 AfeCurrent_GetSeq(void)
{
    return s_data.afeSeq;
}

static void AfeCurrent_NextSeq(void)
{
    if (++s_data.afeSeq == 0U)
    {
        ++s_data.afeSeq;
    }
}

void AfeCurrent_StartupZeroCal(void)
{
    UINT8 i;
    UINT8 sample_cnt;
    UINT8 discard_cnt;
    UINT16 raw_code;
    UINT16 limit_raw;
    INT32 raw_signed;
    INT32 last_raw_signed;
    INT32 sum_raw_signed;
    UINT32 raw_abs;
    UINT32 delta_abs;
    const AFE_CURRENT_STARTUP_ZERO_PARAM *param;

    sample_cnt = 0U;
    discard_cnt = 0U;
    raw_code = 0U;
    raw_signed = 0;
    last_raw_signed = 0;
    sum_raw_signed = 0;
    param = (SleepDeal_IsBootFromSleepStartup() != 0U) ? &s_stAfeCurrentWarmStartupZeroParam : &s_stAfeCurrentColdStartupZeroParam;
    limit_raw = DataLoad_CurrentStartupZeroLimitRaw();
    s_data.cur.zeroOffsetRaw = 0;

    if (param->u16SettleMs > 0U)
    {
        __delay_ms(param->u16SettleMs);
    }

    for (i = 0U; i < param->u8MaxCnt; ++i)
    {
        Feed_IWatchDog;

        if (DataLoad_CurrentReadCadcRaw(&raw_code) != 0U)
        {
            raw_signed = DataLoad_CurrentRawToSigned(raw_code);
            raw_abs = DataLoad_CurrentAbsI32(raw_signed);

            if (discard_cnt < param->u8DiscardCnt)
            {
                ++discard_cnt;
                last_raw_signed = raw_signed;
                sample_cnt = 0U;
                sum_raw_signed = 0;
            }
            else if ((limit_raw > 0U) && (raw_abs <= (UINT32)limit_raw))
            {
                if (sample_cnt == 0U)
                {
                    sum_raw_signed = raw_signed;
                    sample_cnt = 1U;
                }
                else
                {
                    delta_abs = DataLoad_CurrentAbsI32(raw_signed - last_raw_signed);
                    if (delta_abs <= (UINT32)AFE_CURRENT_STARTUP_ZERO_STABLE_RAW)
                    {
                        if (sample_cnt < 0xFFU)
                        {
                            ++sample_cnt;
                        }
                        sum_raw_signed += raw_signed;
                    }
                    else
                    {
                        sum_raw_signed = raw_signed;
                        sample_cnt = 1U;
                    }
                }

                last_raw_signed = raw_signed;

                if (sample_cnt >= param->u8ConfirmCnt)
                {
                    break;
                }
            }
            else
            {
                last_raw_signed = raw_signed;
                sample_cnt = 0U;
                sum_raw_signed = 0;
            }
        }

        if ((i + 1U) < param->u8MaxCnt)
        {
            __delay_ms(param->u16IntervalMs);
        }
    }

    if (sample_cnt > 0U)
    {
        s_data.cur.zeroOffsetRaw = sum_raw_signed / (INT32)sample_cnt;
    }
}

static UINT16 DataLoad_CurrentMilliAmpToA10(UINT32 current_mA)
{
    UINT32 current_a10;

    if (current_mA >= ((UINT32)0xFFFFU * 100U))
    {
        return 0xFFFFU;
    }

    current_a10 = (current_mA + 50U) / 100U;
    if (current_mA < AFE_CURRENT_OUTPUT_DEADBAND_MA)
    {
        return 0;
    }

    return (UINT16)current_a10;
}

void test_Autocurrent_cycle(void)
{
    static uint8_t step = 0;

    switch (step)
    {
    case 0:
        if (g_stCellInfoReport.SocElement.u16Soc < 99)
        {
            step = 1;
            sys_time.CHG = BMS_CAPCITY * 5;
            sys_time.DSG = 0;
            g_stCellInfoReport.u16Ichg = sys_time.CHG;
            g_stCellInfoReport.u16IDischg = 0;
        }
        else
        {
            step = 1;
        }
        break;
    case 1:
        if (g_stCellInfoReport.SocElement.u16Soc >= 99)
        {
            step = 2;
            sys_time.CHG = 0;
            sys_time.DSG = BMS_CAPCITY * 5;
            g_stCellInfoReport.u16Ichg = 0;
            g_stCellInfoReport.u16IDischg = sys_time.DSG;
        }
        break;
    case 2:
        if (g_stCellInfoReport.SocElement.u16Soc <= 1)
        {
            step = 0;
        }
        break;
    default:
        break;
    }
}

void DataLoad_soc_test(void)
{
    static uint8_t test_state = 0;

    switch (test_state)
    {
    case 0:
        if (sys_time.isdebugenable == 1)
        {
            g_stCellInfoReport.u16Ichg = sys_time.CHG;
            g_stCellInfoReport.u16IDischg = 0;
            test_state = 1;
        }
        else
        {
            g_stCellInfoReport.u16Ichg = 0;
            g_stCellInfoReport.u16IDischg = sys_time.DSG;
            test_state = 1;
        }
        break;
    case 1:
        g_stCellInfoReport.u16Ichg = 0;
        g_stCellInfoReport.u16IDischg = 0;
        test_state = 0;
        break;
    default:
        test_state = 0;
        break;
    }
}

void DataLoad_Current(void)
{
    INT32 raw_signed;
    INT32 corrected_raw;
    UINT32 current_mA;

    raw_signed = DataLoad_CurrentRawToSigned(SH367309_Read_AFE1.u16Current);
    corrected_raw = raw_signed - s_data.cur.zeroOffsetRaw;
    current_mA = DataLoad_CurrentRawToMilliAmp(DataLoad_CurrentAbsI32(corrected_raw));

    g_stCellInfoReport.u16Ichg = 0;
    g_stCellInfoReport.u16IDischg = 0;

    if (corrected_raw > 0)
    {
        g_stCellInfoReport.u16Ichg = DataLoad_CurrentMilliAmpToA10(current_mA);
    }
    else if (corrected_raw < 0)
    {
        g_stCellInfoReport.u16IDischg = DataLoad_CurrentMilliAmpToA10(current_mA);
    }

#ifdef __VIRTURE_CURRENT__
    if (sys_time.isdebugenable == 1)
    {
        g_stCellInfoReport.u16Ichg = sys_time.CHG;
        g_stCellInfoReport.u16IDischg = sys_time.DSG;
    }
#endif
}

static void MonitorAFE_SetStatus(UINT8 num, UINT8 is_ok)
{
    switch (num)
    {
    case 0:
        SystemRuntime_SetAfeStatus(0U, is_ok);
        break;
    case 1:
        SystemRuntime_SetAfeStatus(1U, is_ok);
        break;
    default:
        break;
    }
}

static AFE_MONITOR_CH *MonitorAFE_GetChannel(UINT8 num);

static void MonitorAFE_ReportError(UINT8 num)
{
    switch (num)
    {
    case 0:
        System_ERROR_UserCallback(ERROR_AFE1);
        break;
    case 1:
        System_ERROR_UserCallback(ERROR_AFE2);
        break;
    default:
        break;
    }
}

static void MonitorAFE_ClearError(UINT8 num)
{
    switch (num)
    {
    case 0:
        System_ERROR_UserCallback(ERROR_REMOVE_AFE1);
        break;
    case 1:
        System_ERROR_UserCallback(ERROR_REMOVE_AFE2);
        break;
    default:
        break;
    }
}

static void MonitorAFE_Recover(UINT8 num)
{
    switch (num)
    {
    case 0:
        InitAFE1();
        break;
    case 1:
        SH367309_Enable_AFE_Wdt_Cadc_Drivers();
        break;
    default:
        break;
    }
}

static void MonitorAFE_UpdateChannel(UINT8 num, UINT8 result, UINT8 *fault_cnt, UINT8 *wake_cnt)
{
    AFE_MONITOR_CH *channel;

    if ((fault_cnt == 0) || (wake_cnt == 0))
    {
        return;
    }

    channel = MonitorAFE_GetChannel(num);
    if (channel == 0)
    {
        return;
    }

    if (result != 0)
    {
        UINT8 recover_slot;

        if (*fault_cnt < 0xFFU)
        {
            ++(*fault_cnt);
        }

        if (*fault_cnt >= MONITOR_AFE_RECOVER_TRIGGER)
        {
            recover_slot = (UINT8)((*fault_cnt - MONITOR_AFE_RECOVER_TRIGGER) / MONITOR_AFE_RECOVER_RETRY_STEP);
            if ((recover_slot == *wake_cnt) && (*wake_cnt < MONITOR_AFE_WAKE_RETRY_LIMIT))
            {
                MonitorAFE_Recover(num);
                ++(*wake_cnt);
            }
        }

        if ((*fault_cnt >= MONITOR_AFE_FAIL_LIMIT) && (channel->errorReported == 0U))
        {
            Init_Registers(num);
            channel->errorReported = 1U;
            MonitorAFE_ReportError(num);
        }

        MonitorAFE_SetStatus(num, 0);
    }
    else
    {
        *fault_cnt = 0U;
        *wake_cnt = 0U;
        channel->errorReported = 0U;

        MonitorAFE_SetStatus(num, 1);
        MonitorAFE_ClearError(num);
    }
}

static void MonitorAFE_UpdateSleepDelay(UINT8 is_error, UINT16 *delay_tick)
{
    if (delay_tick == 0)
    {
        return;
    }

    if (is_error)
    {
        if (++(*delay_tick) >= MONITOR_AFE_SLEEP_DELAY_TICKS)
        {
            *delay_tick = 0;
            LowPower_Request(NORMAL_MODE);
        }
    }
    else
    {
        *delay_tick = 0;
    }
}

static AFE_MONITOR_CH *MonitorAFE_GetChannel(UINT8 num)
{
    if (num >= 2U)
    {
        return 0;
    }

    return &s_data.mon.ch[num];
}

void MonitorAFE(UINT8 num, UINT8 Result)
{
    AFE_MONITOR_CH *channel;

    channel = MonitorAFE_GetChannel(num);
    if (channel != 0)
    {
        MonitorAFE_UpdateChannel(num, Result, &channel->faultCnt, &channel->wakeCnt);
    }

    MonitorAFE_UpdateSleepDelay(System_ERROR_UserCallback(ERROR_STATUS_AFE1), &s_data.mon.sleepDelay[0]);
    MonitorAFE_UpdateSleepDelay(System_ERROR_UserCallback(ERROR_STATUS_AFE2), &s_data.mon.sleepDelay[1]);
    MonitorAFE_UpdateSleepDelay((UINT8)(System_ERROR_UserCallback(ERROR_STATUS_EEPROM_COM) ||
                                        System_ERROR_UserCallback(ERROR_STATUS_EEPROM_STORE)),
                                &s_data.mon.sleepDelay[2]);
}

void open_ctlc(void)
{
    MCUO_AFE_CTLC = 1;
}

void close_ctlc(void)
{
    MCUO_AFE_CTLC = 0;
}

void new_todo_logi(void)
{
    static uint16_t occ1_rec_cnt = 0;
    static uint16_t odc1_rec_cnt = 0;
    uint8_t Driver_Element_MOS_CHG = 1;
    uint8_t DRIVER_ELEMENT_MOS_DSG = 1;
    static uint32_t soc_low_cnt = 0;

    charger_detect_and_keyLogi_200ms();

#ifdef __SOC_5_PROTECT_
    if (g_stCellInfoReport.SocElement.u16Soc <= 5)
    {
        if (g_stCellInfoReport.u16Ichg >= 10)
        {
            DRIVER_ELEMENT_MOS_DSG = 1;
            soc_low_cnt = 0;
        }
        else
        {
            DRIVER_ELEMENT_MOS_DSG = 0;
            g_stCellInfoReport.unMdlFault_Third.bits.b1SocLow = 1;
            if (++soc_low_cnt >= (5 * 60 * 60))
            {
                soc_low_cnt = 0;
                LowPower_Request(DEEP_MODE);
            }
        }
    }
    else
    {
        g_stCellInfoReport.unMdlFault_Third.bits.b1SocLow = 0;
        soc_low_cnt = 0;
    }
#endif

    if (g_stCellInfoReport.u16Ichg >= AFE_Parameters_RS485_Struction.u16IchgOcp_First.curValue)
    {
        g_stCellInfoReport.unMdlFault_Second.bits.b1IchgOcp = 1;
        FaultWarnRecord2(IchgOcp_Second);
        occ1_rec_cnt = 0;
    }
    if (g_stCellInfoReport.unMdlFault_Second.bits.b1IchgOcp && g_stCellInfoReport.u16IDischg < 10)
    {
        Driver_Element_MOS_CHG = 0;
        if (++occ1_rec_cnt >= (5 * 30))
        {
            occ1_rec_cnt = 0;
            g_stCellInfoReport.unMdlFault_Second.bits.b1IchgOcp = 0;
        }
    }

    if (g_stCellInfoReport.u16IDischg >= AFE_Parameters_RS485_Struction.u16IdsgOcp_First.curValue)
    {
        g_stCellInfoReport.unMdlFault_Second.bits.b1IdischgOcp = 1;
        FaultWarnRecord2(IdischgOcp_Second);
        odc1_rec_cnt = 0;
    }
    if (g_stCellInfoReport.unMdlFault_Second.bits.b1IdischgOcp && g_stCellInfoReport.u16Ichg < 10)
    {
        DRIVER_ELEMENT_MOS_DSG = 0;
        if (odc1_rec_cnt++ >= (5 * 30))
        {
            odc1_rec_cnt = 0;
            g_stCellInfoReport.unMdlFault_Second.bits.b1IdischgOcp = 0;
        }
    }

    if (g_stCellInfoReport.unMdlFault_Third.bits.b1CellOvp ||
        g_stCellInfoReport.unMdlFault_Third.bits.b1IchgOcp ||
        g_stCellInfoReport.unMdlFault_Third.bits.b1CellChgOtp ||
        g_stCellInfoReport.unMdlFault_Third.bits.b1CellChgUtp)
    {
        Driver_Element_MOS_CHG = 0;
        if (g_stCellInfoReport.u16IDischg >= 10)
            Driver_Element_MOS_CHG = 1;
    }
    if (g_stCellInfoReport.unMdlFault_Third.bits.b1CellUvp ||
        g_stCellInfoReport.unMdlFault_Third.bits.b1IdischgOcp ||
        g_stCellInfoReport.unMdlFault_Third.bits.b1CellDischgOtp ||
        g_stCellInfoReport.unMdlFault_Third.bits.b1CellDischgUtp ||
        SH367309_Reg_Store.REG_BSTATUS1.bits.SC)
    {
        DRIVER_ELEMENT_MOS_DSG = 0;
        if (g_stCellInfoReport.u16Ichg >= 10)
            DRIVER_ELEMENT_MOS_DSG = 1;
    }

    if (s_system_status.bits.b1Status_MOS_CHG != Driver_Element_MOS_CHG)
    {
        sys_time.cnt_enter_chg_open++;
        SH367309_DriverMos_Ctrl(GPIO_CHG, Driver_Element_MOS_CHG);
    }
    if (s_system_status.bits.b1Status_MOS_DSG != DRIVER_ELEMENT_MOS_DSG)
    {
        sys_time.cnt_enter_dsg_open++;
        SH367309_DriverMos_Ctrl(GPIO_DSG, DRIVER_ELEMENT_MOS_DSG);
    }
}

void App_AFEGet(void)
{
    if (0U == SysTime_Take200msTaskPeriod())
        return;

    MCUO_DEBUG_LED1 = !MCUO_DEBUG_LED1;
    MonitorAFE(0, UpdateVoltageFromBqMaximo());

    DataLoad_CellVolt();
    DataLoad_CellVoltMaxMinFind();
    DataLoad_Temperature();
    DataLoad_TemperatureMaxMinFind();
    DataLoad_Current();

    AfeCurrent_NextSeq();

    App_SH367309();
    new_todo_logi();
    App_SOC();

#ifdef VCELL_DISP_TEST
    g_stCellInfoReport.u16VCell[31] = sys_time.rtc_sec_cnt;
#endif
}
