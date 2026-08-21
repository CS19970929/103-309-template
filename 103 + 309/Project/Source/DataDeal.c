#include "main.h"

const unsigned char SeriesSelect_AFE1[16][16] = {
    {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},      // 1´®
    {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},      // 2´®
    {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},      // 3
    {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},      // 4
    {0, 1, 2, 3, 4, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},      // 5
    {0, 1, 2, 3, 4, 5, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},      // 6
    {0, 1, 2, 3, 4, 5, 6, 0, 0, 0, 0, 0, 0, 0, 0, 0},      // 7
    {0, 1, 2, 3, 4, 5, 6, 7, 0, 0, 0, 0, 0, 0, 0, 0},      // 8
    {0, 1, 2, 3, 4, 5, 6, 7, 8, 0, 0, 0, 0, 0, 0, 0},      // 9
    {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 0, 0, 0, 0, 0, 0},      // 10
    {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 0, 0, 0, 0, 0},     // 11
    {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 0, 0, 0, 0},    // 12
    {0, 1, 2, 3, 4, 5, 6, 7, 9, 9, 10, 11, 12, 0, 0, 0},   // 13
    {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 0, 0},  // 14
    {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 0}, // 15
    {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15} // 16
};

#define MONITOR_AFE_FAIL_LIMIT ((UINT8)50)
#define MONITOR_AFE_RECOVER_TRIGGER ((UINT8)30)
#define MONITOR_AFE_RECOVER_RETRY_STEP ((UINT8)5U)
#define MONITOR_AFE_WAKE_RETRY_LIMIT ((UINT8)20)
#define MONITOR_AFE_TASK_PERIOD_MS ((UINT16)200)
#define MONITOR_AFE_SLEEP_DELAY_SEC ((UINT16)(5U * 60U))
#define MONITOR_AFE_SLEEP_DELAY_TICKS ((UINT16)((MONITOR_AFE_SLEEP_DELAY_SEC * 1000U) / MONITOR_AFE_TASK_PERIOD_MS))
#define SW_OCP_RECOVER_TICKS ((UINT16)(((UINT32)PROJECT_CFG_SW_OCP_RECOVER_SECONDS * 1000U) / (UINT32)MONITOR_AFE_TASK_PERIOD_MS))
#define SOC_LOW_SLEEP_TICKS (((UINT32)PROJECT_CFG_SOC_LOW_DEEP_SLEEP_SECONDS * 1000U) / (UINT32)MONITOR_AFE_TASK_PERIOD_MS)
#define AFE_CURRENT_ADC_FULL_SCALE_MV ((UINT32)200U)
#define AFE_CURRENT_ADC_DENOMINATOR ((UINT32)21470U)
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
    /* Fixed after startup calibration; never relearned while running. */
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
        break;

    default:
        break;
    }
}

#if 0
void DataLoad_CellVolt(void)
{
    UINT8 i;
    UINT8 series_num = SeriesNum;
    INT32 t_i32temp;

    for (i = 0; i < series_num; ++i)
    {
        UINT8 afe_index = i;

        if (series_num < 5U)
        {
            afe_index = 0U;
        }
        else if ((series_num == 13U) && (i == 8U))
        {
            afe_index = 9U;
        }

        t_i32temp = (UINT32)SH367309_Read_AFE1.u16VCell[afe_index];
        g_stCellInfoReport.u16VCell[i] = (UINT16)t_i32temp;
    }

    if (series_num < 32)
    {
        for (i = series_num; i < 25; ++i)
        {
            g_stCellInfoReport.u16VCell[i] = 61001;
        }
    }
}
#endif

void DataLoad_CellVolt(void)
{
    UINT8 i;
    INT32 t_i32temp;

    for (i = 0; i < SeriesNum; ++i)
    {
        t_i32temp = (UINT32)SH367309_Read_AFE1.u16VCell[SeriesSelect_AFE1[SeriesNum - 1][i]];
        g_stCellInfoReport.u16VCell[i] = (UINT16)t_i32temp;
    }

#ifndef VCELL_DISP_TEST
    if (SeriesNum < 32)
    {
        for (i = SeriesNum; i < 32; ++i)
        {
            g_stCellInfoReport.u16VCell[i] = 61001;
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

#if 0
    t_i32temp = ADC_GetResult(ADC_TEMP_EV1) / 10 - 40;
    t_i32temp = ((t_i32temp * g_u16CalibCoefK[MDL_TEMP_ENV1]) + g_i16CalibCoefB[MDL_TEMP_ENV1]) >> 10;
    g_stCellInfoReport.u16Temperature[ENV_TEMP1] = (UINT16)(t_i32temp * 10 + 400);
    Monitor_TempBreak(&g_stCellInfoReport.u16Temperature[ENV_TEMP1]);
#endif

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

static UINT16 DataLoad_CurrentMilliAmpToRaw(UINT32 current_mA)
{
    uint64_t numerator;
    uint64_t denominator;
    uint64_t raw_code;

    if ((current_mA == 0U) || (g_u32CS_Res_AFE == 0U))
    {
        return 0;
    }

    denominator = (uint64_t)AFE_CURRENT_ADC_FULL_SCALE_MV * (uint64_t)g_u32CS_Res_AFE;
    numerator = ((uint64_t)current_mA * (uint64_t)AFE_CURRENT_ADC_DENOMINATOR) + (denominator / 2U);
    raw_code = numerator / denominator;

    if (raw_code > 0x7FFFU)
    {
        return 0x7FFFU;
    }

    return (UINT16)raw_code;
}

static UINT32 DataLoad_CurrentRawToMilliAmp(UINT32 raw_abs)
{
    uint64_t current_mA;

    if ((raw_abs == 0U) || (g_u32CS_Res_AFE == 0U))
    {
        return 0U;
    }

    current_mA = (uint64_t)raw_abs * (uint64_t)AFE_CURRENT_ADC_FULL_SCALE_MV * (uint64_t)g_u32CS_Res_AFE;
    current_mA = (current_mA + ((uint64_t)AFE_CURRENT_ADC_DENOMINATOR / 2U)) / (uint64_t)AFE_CURRENT_ADC_DENOMINATOR;

    if (current_mA > 0xFFFFFFFFU)
    {
        return 0xFFFFFFFFU;
    }

    return (UINT32)current_mA;
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
    limit_raw = DataLoad_CurrentMilliAmpToRaw(AFE_CURRENT_STARTUP_ZERO_LIMIT_MA);
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
        BmsEvent_Set(BMS_CORE_EVT_AFE_RECOVERED);
        break;
    case 1:
        SH367309_Enable_AFE_Wdt_Cadc_Drivers();
        BmsEvent_Set(BMS_CORE_EVT_AFE_RECOVERED);
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

static void Protection_ProcessSoftware200ms(void)
{
    static UINT16 charge_recover_ticks = 0U;
    static UINT16 discharge_recover_ticks = 0U;
#ifdef __SOC_5_PROTECT_
    static UINT32 soc_low_ticks = 0U;
#endif

    charger_detect_and_keyLogi_200ms();

#ifdef __SOC_5_PROTECT_
    if (g_stCellInfoReport.SocElement.u16Soc <= 5U)
    {
        if (g_stCellInfoReport.u16Ichg >= (UINT16)PROJECT_CFG_MOS_REVERSE_RELEASE_CURRENT_A10)
        {
            soc_low_ticks = 0U;
        }
        else
        {
            g_stCellInfoReport.unMdlFault_Third.bits.b1SocLow = 1U;
            if (++soc_low_ticks >= SOC_LOW_SLEEP_TICKS)
            {
                soc_low_ticks = 0U;
                LowPower_Request(DEEP_MODE);
            }
        }
    }
    else
    {
        g_stCellInfoReport.unMdlFault_Third.bits.b1SocLow = 0U;
        soc_low_ticks = 0U;
    }
#endif

    if (g_stCellInfoReport.u16Ichg >= AFE_Parameters_RS485_Struction.u16IchgOcp_First.curValue)
    {
        if (g_stCellInfoReport.unMdlFault_Second.bits.b1IchgOcp == 0U)
        {
            g_stCellInfoReport.unMdlFault_Second.bits.b1IchgOcp = 1U;
            FaultWarnRecord2(IchgOcp_Second);
        }
        charge_recover_ticks = 0U;
    }
    else if (g_stCellInfoReport.unMdlFault_Second.bits.b1IchgOcp != 0U)
    {
        if (g_stCellInfoReport.u16IDischg < (UINT16)PROJECT_CFG_MOS_REVERSE_RELEASE_CURRENT_A10)
        {
            if (++charge_recover_ticks >= SW_OCP_RECOVER_TICKS)
            {
                charge_recover_ticks = 0U;
                g_stCellInfoReport.unMdlFault_Second.bits.b1IchgOcp = 0U;
            }
        }
        else
        {
            charge_recover_ticks = 0U;
        }
    }
    else
    {
        charge_recover_ticks = 0U;
    }

    if (g_stCellInfoReport.u16IDischg >= AFE_Parameters_RS485_Struction.u16IdsgOcp_First.curValue)
    {
        if (g_stCellInfoReport.unMdlFault_Second.bits.b1IdischgOcp == 0U)
        {
            g_stCellInfoReport.unMdlFault_Second.bits.b1IdischgOcp = 1U;
            FaultWarnRecord2(IdischgOcp_Second);
        }
        discharge_recover_ticks = 0U;
    }
    else if (g_stCellInfoReport.unMdlFault_Second.bits.b1IdischgOcp != 0U)
    {
        if (g_stCellInfoReport.u16Ichg < (UINT16)PROJECT_CFG_MOS_REVERSE_RELEASE_CURRENT_A10)
        {
            if (++discharge_recover_ticks >= SW_OCP_RECOVER_TICKS)
            {
                discharge_recover_ticks = 0U;
                g_stCellInfoReport.unMdlFault_Second.bits.b1IdischgOcp = 0U;
            }
        }
        else
        {
            discharge_recover_ticks = 0U;
        }
    }
    else
    {
        discharge_recover_ticks = 0U;
    }
}

static void Protection_UpdateCoreBlocks(void)
{
    UINT32 sw_charge_block = 0U;
    UINT32 sw_discharge_block = 0U;
    UINT32 hw_charge_block = 0U;
    UINT32 hw_discharge_block = 0U;

    if (g_stCellInfoReport.unMdlFault_Second.bits.b1IchgOcp != 0U)
    {
        sw_charge_block |= BMS_MOS_BLOCK_CURRENT;
    }
    if (g_stCellInfoReport.unMdlFault_Second.bits.b1IdischgOcp != 0U)
    {
        sw_discharge_block |= BMS_MOS_BLOCK_CURRENT;
    }
    if (g_stCellInfoReport.unMdlFault_Third.bits.b1SocLow != 0U)
    {
        sw_discharge_block |= BMS_MOS_BLOCK_SOC_LOW;
    }

    if (SH367309_Reg_Store.REG_BSTATUS1.bits.OV != 0U)
    {
        hw_charge_block |= BMS_MOS_BLOCK_CELL_VOLTAGE;
    }
    if (SH367309_Reg_Store.REG_BSTATUS1.bits.OCC != 0U)
    {
        hw_charge_block |= BMS_MOS_BLOCK_CURRENT;
    }
    if ((SH367309_Reg_Store.REG_BSTATUS2.bits.OTC != 0U) ||
        (SH367309_Reg_Store.REG_BSTATUS2.bits.UTC != 0U))
    {
        hw_charge_block |= BMS_MOS_BLOCK_TEMPERATURE;
    }

    if (SH367309_Reg_Store.REG_BSTATUS1.bits.UV != 0U)
    {
        hw_discharge_block |= BMS_MOS_BLOCK_CELL_VOLTAGE;
    }
    if ((SH367309_Reg_Store.REG_BSTATUS1.bits.OCD1 != 0U) ||
        (SH367309_Reg_Store.REG_BSTATUS1.bits.OCD2 != 0U))
    {
        hw_discharge_block |= BMS_MOS_BLOCK_CURRENT;
    }
    if ((SH367309_Reg_Store.REG_BSTATUS2.bits.OTD != 0U) ||
        (SH367309_Reg_Store.REG_BSTATUS2.bits.UTD != 0U))
    {
        hw_discharge_block |= BMS_MOS_BLOCK_TEMPERATURE;
    }
    if (SH367309_Reg_Store.REG_BSTATUS1.bits.SC != 0U)
    {
        hw_discharge_block |= BMS_MOS_BLOCK_SHORT_CIRCUIT;
    }

    BmsCore_SetProtectionBlocks(sw_charge_block,
                                sw_discharge_block,
                                hw_charge_block,
                                hw_discharge_block);
}

static void MosPolicy_ApplyRequest(UINT8 charge_on, UINT8 discharge_on)
{
    MTP_REG_CONF requested;
    UINT8 old_charge;
    UINT8 old_discharge;

    requested = SH367309_Reg_Store.REG_MTP_CONF;
    old_charge = requested.bits.CHGMOS;
    old_discharge = requested.bits.DSGMOS;

    if ((old_charge == charge_on) && (old_discharge == discharge_on))
    {
        return;
    }

    requested.bits.CHGMOS = charge_on;
    requested.bits.DSGMOS = discharge_on;

    if (MTPWrite(MTP_CONF, 1, &requested.all))
    {
        SH367309_Reg_Store.REG_MTP_CONF = requested;
        if (old_charge != charge_on)
        {
            sys_time.cnt_enter_chg_open++;
        }
        if (old_discharge != discharge_on)
        {
            sys_time.cnt_enter_dsg_open++;
        }
    }
}

static void MosPolicy_Process200ms(void)
{
    struct BMS_CORE_STATE state;
    UINT8 charge_request;
    UINT8 discharge_request;

    BmsCore_GetState(&state);

    charge_request = (state.protection.swChargeBlock == 0U) ? 1U : 0U;
    discharge_request = (state.protection.swDischargeBlock == 0U) ? 1U : 0U;

    /* Reverse current may release software-only blocking. AFE hardware fault
       remains authoritative because hardware block and actual FET state are
       independent from these software request bits. */
    if ((state.protection.swChargeBlock != 0U) &&
        (state.measurement.dischargeCurrentA10 >= (UINT16)PROJECT_CFG_MOS_REVERSE_RELEASE_CURRENT_A10))
    {
        charge_request = 1U;
    }

    if ((state.protection.swDischargeBlock != 0U) &&
        (state.measurement.chargeCurrentA10 >= (UINT16)PROJECT_CFG_MOS_REVERSE_RELEASE_CURRENT_A10))
    {
        discharge_request = 1U;
    }

    (void)BmsCore_SetMosRequest(charge_request, discharge_request);
    MosPolicy_ApplyRequest(charge_request, discharge_request);
}

static void BmsCore_UpdateMeasurementFromReport(void)
{
    BmsCore_UpdateMeasurement(g_stCellInfoReport.u16VCellMin,
                              g_stCellInfoReport.u16VCellMax,
                              g_stCellInfoReport.u16Ichg,
                              g_stCellInfoReport.u16IDischg,
                              g_stCellInfoReport.SocElement.u16Soc);
}

void App_AFEGet(void)
{
    if (0U == SysTime_Take200msTaskPeriod())
    {
        return;
    }

    MCUO_DEBUG_LED1 = !MCUO_DEBUG_LED1;
    MonitorAFE(0, UpdateVoltageFromBqMaximo());

    DataLoad_CellVolt();
    DataLoad_CellVoltMaxMinFind();
    DataLoad_Temperature();
    DataLoad_TemperatureMaxMinFind();
    DataLoad_Current();

    AfeCurrent_NextSeq();

    App_SH367309();
    BmsCore_UpdateMeasurementFromReport();
    Protection_ProcessSoftware200ms();
    Protection_UpdateCoreBlocks();
    MosPolicy_Process200ms();

    App_SOC();
    BmsCore_UpdateMeasurementFromReport();

#ifdef VCELL_DISP_TEST
    g_stCellInfoReport.u16VCell[31] = sys_time.rtc_sec_cnt;
#endif
}
