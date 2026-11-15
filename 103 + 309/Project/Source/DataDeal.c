#include "main.h"
#include "Sci_Protocol.h"
#include "CurrentCalibration.h"

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

#define AFE_MONITOR_FAIL_LIMIT 3U
#define AFE_MONITOR_RECOVER_LIMIT 3U
#define AFE_MONITOR_MIN_SAMPLE_TICKS 15U
#define AFE_MONITOR_DEEP_TICKS 30000U
#define AFE_CONFIG_RETRY_TICKS 500U
#define AFE_CONFIG_MAX_ATTEMPTS 3U
#define AFE_CURRENT_ADC_FULL_SCALE_MV ((UINT32)200U)
#define AFE_CURRENT_ADC_DENOMINATOR ((UINT32)21470U)
#define BOOT_CURRENT_ZERO_SAMPLE_INTERVAL_MS ((UINT16)300U)
#define BOOT_CURRENT_ZERO_MAX_ABS_COUNTS ((UINT32)65U)
#define BOOT_CURRENT_ZERO_MAX_DELTA_COUNTS ((UINT32)15U)
#define BOOT_CURRENT_FET_STATUS_MASK ((UINT8)0x07U)
#define CURRENT_FIXED_SCALE ((UINT32)4U)
#define CURRENT_REPORT_MA_PER_LSB ((UINT16)100U)
/* Keep the configured product deadband in mA. */
#define CURRENT_DEADBAND_MA ((UINT16)0U)

typedef struct _AFE_CURRENT_RUNTIME
{
    /* Boot zero is stored in raw-count x4 units to retain half-count averaging. */
    INT32 zeroOffsetRawX4;
    /* Effective signed current after product deadband. Unit: mA. */
    INT32 current_mA;
    UINT8 zeroStatus;
} AFE_CURRENT_RUNTIME;

typedef struct _AFE_MONITOR_RUNTIME
{
    UINT32 fault_tick;
    UINT32 last_eval_tick;
    UINT32 last_data_tick;
    UINT8 fail_count;
    UINT8 ok_count;
    UINT8 evaluated;
    UINT8 data_valid;
    UINT8 healthy;
    UINT8 fault;
    UINT8 config_attempts;
    UINT8 config_attempted;
    UINT32 config_attempt_tick;
} AFE_MONITOR_RUNTIME;

typedef struct _DATA_RUNTIME
{
    AFE_CURRENT_RUNTIME cur;
    AFE_MONITOR_RUNTIME mon;
} DATA_RUNTIME;

static DATA_RUNTIME s_data = {0};

UINT16 g_u16CalibCoefK[KB_NUM];
INT16 g_i16CalibCoefB[KB_NUM];

UINT32 g_u32CS_Res_AFE;

struct OTHER_ELEMENT OtherElement;

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
    // CHG_OFF;
    // DSG_OFF;
}

// 这里排列好就行，不需要电池位号映射表。>61000为不用
// 经过验算，AFE1校准一次，然后本身再校准一次叠加是可以的。不需要确定某一个KB值的做法。
// 假设先确定用AFE1还是本身的KB的话，会出现问题。如下：
// 假设需要整体校准，行，AFE1先行，然后发现某几串出问题，继续使用本身KB值，然后本身KB值需要同步前面AFE1的KB值一起算才行
// 如果又变成单独使用本身KB值校准，出现错误。

void DataLoad_CellVolt(void)
{
    UINT8 i;
    INT32 t_i32temp;

    for (i = 0; i < SeriesNum; ++i)
    {
        t_i32temp = (UINT32)SH367309_Read_AFE1.u16VCell[SeriesSelect_AFE1[SeriesNum - 1][i]];
        g_stCellInfoReport.u16VCell[i] = (UINT16)t_i32temp;
    }

    if (SeriesNum < 32)
    {
        for (i = SeriesNum; i < 32; ++i)
        {
            g_stCellInfoReport.u16VCell[i] = 61001;
        }
    }
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

    g_stCellInfoReport.u16VCellTotle = (UINT16)((u32VCellTotle * 1638 >> 14) & 0xFFFF); // 除以10
    g_stCellInfoReport.u16VCellMax = t_u16VcellMaxTemp;                                 // max cell voltage
    g_stCellInfoReport.u16VCellMin = t_u16VcellMinTemp;                                 // min cell voltage
    g_stCellInfoReport.u16VCellDelta = t_u16VcellMaxTemp - t_u16VcellMinTemp;           // delta cell voltage
    g_stCellInfoReport.u16VCellMaxPosition = t_u8VcellMaxPosition + 1;                  // max cell voltage
    g_stCellInfoReport.u16VCellMinPosition = t_u8VcellMinPosition + 1;                  // min cell voltage
}

/*这个是数据溢出的问题，其次是>>这个的优先级和别的符号优先级的问题
  运算符优先级太混乱导致数据溢出的问题
   (UINT16)(t_i32temp/100) 和
    (UINT16)(t_i32temp)/100不一样
*/
void DataLoad_Temperature(void)
{
    UINT8 i;
    INT32 t_i32temp;
    UINT8 Select;

    Select = 2;
    // 没纳入统计的，默认值就是0了
    for (i = 0; i < Select; i++)
    {
        t_i32temp = (INT32)SH367309_Read_AFE1.u16TempBat[i] / 10 - 40;
        t_i32temp = ((t_i32temp * g_u16CalibCoefK[MDL_TEMP1 + i]) + g_i16CalibCoefB[MDL_TEMP1 + i]) >> 10;
        g_stCellInfoReport.u16Temperature[i] = (UINT16)(t_i32temp * 10 + 400);
        Monitor_TempBreak(&g_stCellInfoReport.u16Temperature[i]);
    }

    g_stCellInfoReport.u16Temperature[2] = 0;

    /* Unfitted environment channels retain the existing calibration/encoding. */
    t_i32temp = ((-40 * (INT32)g_u16CalibCoefK[MDL_TEMP_ENV2]) + g_i16CalibCoefB[MDL_TEMP_ENV2]) >> 10;
    g_stCellInfoReport.u16Temperature[ENV_TEMP2] = (UINT16)(t_i32temp * 10 + 400);
    t_i32temp = ((-40 * (INT32)g_u16CalibCoefK[MDL_TEMP_ENV3]) + g_i16CalibCoefB[MDL_TEMP_ENV3]) >> 10;
    g_stCellInfoReport.u16Temperature[ENV_TEMP3] = (UINT16)(t_i32temp * 10 + 400);
}

/* Raw NTC counts -> (degrees C + 40) * 10, shared by report and protection. */
void DataLoad_MosTemperature(void)
{
    static const UINT16 mos_ntc_table[] = {
    3771, 100, 3683, 150, 3580, 200, 3460, 250,
    3323, 300, 3169, 350, 3004, 400, 2820, 450,
    2633, 500, 2437, 550, 2241, 600, 2048, 650,
    1859, 700, 1679, 750, 1509, 800, 1351, 850,
    1204, 900, 1073, 950, 953, 1000, 845, 1050,
    749, 1100, 664, 1150, 588, 1200, 522, 1250,
    463, 1300, 411, 1350, 366, 1400, 326, 1450
    };
    UINT16 raw;
    INT32 temperature;
    UINT8 status = ADC_TakeSample(ADC_INPUT_MOS, &raw);

    if (status == ADC_SAMPLE_NONE)
    {
        /* No new sample is not an invalid sample: keep thermal debounce. */
        Fault_MosSensorWatchdog();
        return;
    }
    if (status != ADC_SAMPLE_READY)
        goto invalid;
    /* Outside the characterized NTC table is unavailable, not confirmed heat. */
    if (raw > mos_ntc_table[0] || raw < mos_ntc_table[sizeof(mos_ntc_table) / sizeof(mos_ntc_table[0]) - 2U])
    {
        goto invalid;
    }
    temperature = GetEndValue(mos_ntc_table, sizeof(mos_ntc_table) / sizeof(mos_ntc_table[0]), raw);
    temperature = temperature / 10 - 40;
    temperature = (temperature * g_u16CalibCoefK[MDL_TEMP_MOS1] + g_i16CalibCoefB[MDL_TEMP_MOS1]) >> 10;
    temperature = temperature * 10 + 400;
    if (temperature < 0 || temperature > 2000)
    {
        goto invalid;
    }
    g_stCellInfoReport.u16Temperature[MOS_TEMP1] = (UINT16)temperature;
    Fault_UpdateMosTemperature(1U, (UINT16)temperature);
    return;

invalid:
    Fault_UpdateMosTemperature(0U, 0U);
}

void DataLoad_TemperatureMaxMinFind(void)
{
    UINT8 i;
    UINT16 t_u16VcellTemp;
    UINT16 t_u16VcellMaxTemp;
    UINT16 t_u16VcellMinTemp;
    t_u16VcellMaxTemp = 0;
    t_u16VcellMinTemp = 0x7FFF;

    // 如果是两个环境温度，则改为8便可
    for (i = 0; i < 7; i++)
    { // 默认只有一个环境温度，纳入计算
        if (g_stCellInfoReport.u16Temperature[i] == 0)
        {             // 这段代码什么意思，断了就不判断吗？
            continue; // 有的，则必定会被赋值，要么-29摄氏度。
        } // 空的，则就是默认刚上电的值0
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

    g_stCellInfoReport.u16TempMax = t_u16VcellMaxTemp; // max temp
    g_stCellInfoReport.u16TempMin = t_u16VcellMinTemp; // min temp
}

static INT32 DataLoad_CurrentRawToSigned(UINT16 raw_code)
{
    return (INT32)(INT16)raw_code;
}

static UINT32 DataLoad_CurrentAbsI32(INT32 value)
{
    if (value < 0)
    {
        return (UINT32)(-(value + 1)) + 1U;
    }

    return (UINT32)value;
}

static UINT32 DataLoad_CurrentRawX4ToMilliAmpX4(UINT32 raw_abs_x4)
{
    uint64_t current_mA_x4;

    if ((raw_abs_x4 == 0U) || (g_u32CS_Res_AFE == 0U))
    {
        return 0U;
    }

    current_mA_x4 = (uint64_t)raw_abs_x4 *
                    (uint64_t)AFE_CURRENT_ADC_FULL_SCALE_MV *
                    (uint64_t)g_u32CS_Res_AFE;
    current_mA_x4 = (current_mA_x4 + ((uint64_t)AFE_CURRENT_ADC_DENOMINATOR / 2U)) /
                    (uint64_t)AFE_CURRENT_ADC_DENOMINATOR;

    if (current_mA_x4 > 0xFFFFFFFFULL)
    {
        return 0xFFFFFFFFU;
    }

    return (UINT32)current_mA_x4;
}

static UINT32 DataLoad_CurrentMilliAmpX4ToMilliAmp(UINT32 current_mA_x4)
{
    return (current_mA_x4 + (CURRENT_FIXED_SCALE / 2U)) / CURRENT_FIXED_SCALE;
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

static void AfeCurrent_BootZeroFail(UINT8 status)
{
    s_data.cur.zeroOffsetRawX4 = 0;
    s_data.cur.zeroStatus = status;
}

static void AfeCurrent_WaitFreshSample(void)
{
    /*
     * SH367309 CADC is 4 Hz. 300 ms guarantees that two reads cannot merely
     * be repeated accesses to the same conversion result.
     */
    __delay_ms(BOOT_CURRENT_ZERO_SAMPLE_INTERVAL_MS);
}

static UINT8 AfeCurrent_PrepareBootZero(void)
{
    SH367309_Reg_Store.REG_MTP_CONF.bits.CADCON = 1U;
    SH367309_Reg_Store.REG_MTP_CONF.bits.CHGMOS = 0U;
    SH367309_Reg_Store.REG_MTP_CONF.bits.DSGMOS = 0U;
    SH367309_Reg_Store.REG_MTP_CONF.bits.PCHMOS = 0U;

    if (!MTPWrite(MTP_CONF, 1U, &SH367309_Reg_Store.REG_MTP_CONF.all))
    {
        AfeCurrent_BootZeroFail((UINT8)AFE_CURRENT_ZERO_CONFIG_WRITE_ERROR);
        return 0U;
    }

    return 1U;
}

static UINT8 AfeCurrent_CheckBootZeroSafe(void)
{
    MTP_REG_CONF confirmed_conf;
    UINT8 bstatus3;

    confirmed_conf.all = 0U;
    bstatus3 = 0U;

    if (!MTPRead(MTP_CONF, 1U, &confirmed_conf.all))
    {
        AfeCurrent_BootZeroFail((UINT8)AFE_CURRENT_ZERO_CONFIG_READBACK_ERROR);
        return 0U;
    }

    if ((!confirmed_conf.bits.CADCON) ||
        confirmed_conf.bits.CHGMOS ||
        confirmed_conf.bits.DSGMOS ||
        confirmed_conf.bits.PCHMOS)
    {
        AfeCurrent_BootZeroFail((UINT8)AFE_CURRENT_ZERO_FET_ACTIVE);
        return 0U;
    }

    if (!MTPRead(MTP_BSTATUS3, 1U, &bstatus3))
    {
        AfeCurrent_BootZeroFail((UINT8)AFE_CURRENT_ZERO_SAMPLE_READ_ERROR);
        return 0U;
    }
    if ((bstatus3 & BOOT_CURRENT_FET_STATUS_MASK) != 0U)
    {
        AfeCurrent_BootZeroFail((UINT8)AFE_CURRENT_ZERO_FET_ACTIVE);
        return 0U;
    }

    return 1U;
}

void AfeCurrent_StartupZeroCal(void)
{
    UINT16 raw_code;
    INT32 raw1;
    INT32 raw2;
    INT32 delta;

    if (s_data.cur.zeroStatus != (UINT8)AFE_CURRENT_ZERO_NOT_RUN)
    {
        return;
    }

    s_data.cur.zeroOffsetRawX4 = 0;
    s_data.cur.current_mA = 0;

    if (!AfeCurrent_PrepareBootZero())
    {
        return;
    }

    AfeCurrent_WaitFreshSample();
    if ((!AfeCurrent_CheckBootZeroSafe()) ||
        (!DataLoad_CurrentReadCadcRaw(&raw_code)) ||
        (!AfeCurrent_CheckBootZeroSafe()))
    {
        return;
    }
    raw1 = DataLoad_CurrentRawToSigned(raw_code);

    AfeCurrent_WaitFreshSample();
    if ((!AfeCurrent_CheckBootZeroSafe()) ||
        (!DataLoad_CurrentReadCadcRaw(&raw_code)) ||
        (!AfeCurrent_CheckBootZeroSafe()))
    {
        return;
    }
    raw2 = DataLoad_CurrentRawToSigned(raw_code);

    if ((DataLoad_CurrentAbsI32(raw1) > BOOT_CURRENT_ZERO_MAX_ABS_COUNTS) ||
        (DataLoad_CurrentAbsI32(raw2) > BOOT_CURRENT_ZERO_MAX_ABS_COUNTS))
    {
        AfeCurrent_BootZeroFail((UINT8)AFE_CURRENT_ZERO_OUT_OF_RANGE);
        return;
    }

    delta = raw2 - raw1;
    if (DataLoad_CurrentAbsI32(delta) > BOOT_CURRENT_ZERO_MAX_DELTA_COUNTS)
    {
        AfeCurrent_BootZeroFail((UINT8)AFE_CURRENT_ZERO_UNSTABLE);
        return;
    }

    /* ((raw1 + raw2) / 2) * 4 == (raw1 + raw2) * 2. */
    s_data.cur.zeroOffsetRawX4 = (raw1 + raw2) * 2;
    s_data.cur.zeroStatus = (UINT8)AFE_CURRENT_ZERO_VALID;
}

INT32 AfeCurrent_GetCurrent_mA(void)
{
    return s_data.cur.current_mA;
}

/*
 * Boot-zero is applied before K/B. K is Q10 and B is signed Q10(A), matching
 * the historical BMS-upper protocol. Internal output remains signed mA.
 * Charge/discharge use separate coefficients and there is no 2 A split.
 */
static INT32 DataLoad_CurrentApplyCalibrationX4(INT32 nominal_mA_x4)
{
    UINT16 calib_index;

    if (nominal_mA_x4 == 0)
    {
        return 0;
    }

    calib_index = (nominal_mA_x4 > 0) ? (UINT16)MDL_ICHG : (UINT16)MDL_IDSG;
    return (INT32)CurrentCalibration_ApplySignedMilliAmpX4(
        (int32_t)nominal_mA_x4,
        (uint16_t)g_u16CalibCoefK[calib_index],
        (int16_t)g_i16CalibCoefB[calib_index]);
}

static UINT16 DataLoad_CurrentMilliAmpX4ToA10(UINT32 current_mA_x4)
{
    UINT32 divisor;
    UINT32 report_value;

    divisor = CURRENT_FIXED_SCALE * (UINT32)CURRENT_REPORT_MA_PER_LSB;
    report_value = (current_mA_x4 + (divisor / 2U)) / divisor;
    if (report_value > 0xFFFFU)
    {
        return 0xFFFFU;
    }

    return (UINT16)report_value;
}

void DataLoad_Current(void)
{
    UINT16 raw_code;
    INT32 raw_signed;
    INT32 corrected_raw_x4;
    INT32 nominal_mA_x4;
    INT32 calibrated_mA_x4;
    INT32 measured_mA;
    UINT32 calibrated_abs_mA_x4;
    UINT32 calibrated_abs_mA;

    UINT16 deadband_mA;

    raw_code = SH367309_Read_AFE1.u16Current;
    raw_signed = DataLoad_CurrentRawToSigned(raw_code);
    corrected_raw_x4 = raw_signed * (INT32)CURRENT_FIXED_SCALE;
    if (s_data.cur.zeroStatus == (UINT8)AFE_CURRENT_ZERO_VALID)
    {
        corrected_raw_x4 -= s_data.cur.zeroOffsetRawX4;
    }

    calibrated_abs_mA_x4 = DataLoad_CurrentRawX4ToMilliAmpX4(DataLoad_CurrentAbsI32(corrected_raw_x4));
    if ((calibrated_abs_mA_x4 == 0U) || (corrected_raw_x4 == 0))
    {
        nominal_mA_x4 = 0;
    }
    else if (corrected_raw_x4 > 0)
    {
        nominal_mA_x4 = (INT32)calibrated_abs_mA_x4;
    }
    else
    {
        nominal_mA_x4 = -(INT32)calibrated_abs_mA_x4;
    }

    calibrated_mA_x4 = DataLoad_CurrentApplyCalibrationX4(nominal_mA_x4);
    calibrated_abs_mA_x4 = DataLoad_CurrentAbsI32(calibrated_mA_x4);
    calibrated_abs_mA = DataLoad_CurrentMilliAmpX4ToMilliAmp(calibrated_abs_mA_x4);

    if (calibrated_mA_x4 > 0)
    {
        measured_mA = (INT32)calibrated_abs_mA;
    }
    else if (calibrated_mA_x4 < 0)
    {
        measured_mA = -(INT32)calibrated_abs_mA;
    }
    else
    {
        measured_mA = 0;
    }

    deadband_mA = CURRENT_DEADBAND_MA;
    if (calibrated_abs_mA_x4 < ((UINT32)deadband_mA * CURRENT_FIXED_SCALE))
    {
        s_data.cur.current_mA = 0;
    }
    else
    {
        s_data.cur.current_mA = measured_mA;
    }

    /*
     * Keep the legacy public data model unchanged: charge/discharge are still
     * separate UINT16 values in A*10 (100 mA/LSB). Only the internal path is
     * upgraded to signed mA.
     */
    g_stCellInfoReport.u16Ichg = 0U;
    g_stCellInfoReport.u16IDischg = 0U;

    if (s_data.cur.current_mA > 0)
    {
        g_stCellInfoReport.u16Ichg = DataLoad_CurrentMilliAmpX4ToA10(calibrated_abs_mA_x4);
    }
    else if (s_data.cur.current_mA < 0)
    {
        g_stCellInfoReport.u16IDischg = DataLoad_CurrentMilliAmpX4ToA10(calibrated_abs_mA_x4);
    }

#ifdef __VIRTURE_CURRENT__
    if (sys_time.isdebugenable == 1)
    {
        INT32 virtual_mA;

        g_stCellInfoReport.u16Ichg = sys_time.CHG;
        g_stCellInfoReport.u16IDischg = sys_time.DSG;

        virtual_mA = ((INT32)sys_time.CHG - (INT32)sys_time.DSG) * 100;
        s_data.cur.current_mA = virtual_mA;
    }
#endif
}

/* No AFE software reset or EEPROM programming from communication recovery. */
void AFE_MonitorInvalidate(void)
{
    s_data.mon.data_valid = 0U;
    s_data.mon.healthy = 0U;
    s_data.mon.ok_count = 0U;
    s_data.mon.evaluated = 0U;
    SystemRuntime_SetAfeStatus(0U, 0U);
    Fault_PauseMeasurements();
}

/* STOP resets the RUN tick. Keep confirmed faults, but do not include
   offline time in the five-minute communication fault timeout. */
void AFE_MonitorResumeAfterStop(void)
{
    AFE_MonitorInvalidate();
    s_data.mon.fail_count = 0U;
    if (s_data.mon.fault)
        s_data.mon.fault_tick = SysTime_Get10msTickCount();
}

UINT8 AFE_IsDataFresh(void)
{
    return (UINT8)(s_data.mon.data_valid &&
        (UINT32)(SysTime_Get10msTickCount() - s_data.mon.last_data_tick) <= 50U);
}

UINT8 AFE_IsHealthy(void)
{
    return (UINT8)(s_data.mon.healthy && !s_data.mon.fault &&
                   !AFE_PARAM_WRITE_Flag);
}

static void AFE_MonitorUpdate(UINT8 data_ok, UINT8 status_ok, UINT8 wire_ok)
{
    AFE_MONITOR_RUNTIME *mon = &s_data.mon;
    UINT32 now = SysTime_Get10msTickCount();
    UINT8 complete = (UINT8)(wire_ok && status_ok);

    mon->data_valid = data_ok;
    if (data_ok) mon->last_data_tick = now;

    /* Pending periods may be drained in one RUN tick: never count them
       as separate fault or recovery confirmations. */
    if (!mon->evaluated ||
        (UINT32)(now - mon->last_eval_tick) >= AFE_MONITOR_MIN_SAMPLE_TICKS)
    {
        mon->evaluated = 1U;
        mon->last_eval_tick = now;
        if (complete)
        {
            mon->fail_count = 0U;
            if (mon->ok_count < AFE_MONITOR_RECOVER_LIMIT)
                ++mon->ok_count;
            if (mon->ok_count >= AFE_MONITOR_RECOVER_LIMIT)
            {
                mon->fault = 0U;
                mon->healthy = 1U;
                SystemError_SetActive(ERROR_AFE1, 0U);
            }
        }
        else
        {
            mon->ok_count = 0U;
            mon->healthy = 0U;
            if (mon->fail_count < AFE_MONITOR_FAIL_LIMIT)
                ++mon->fail_count;
            if (mon->fail_count >= AFE_MONITOR_FAIL_LIMIT && !mon->fault)
            {
                mon->fault = 1U;
                mon->fault_tick = now;
                SystemError_SetActive(ERROR_AFE1, 1U);
                close_ctlc(); /* CTLC=3 is required and must be verified on board. */
            }
        }
    }
    SystemRuntime_SetAfeStatus(0U, (UINT8)(complete && mon->healthy));
    if (mon->fault && (UINT32)(now - mon->fault_tick) >= AFE_MONITOR_DEEP_TICKS)
        LowPower_Request(DEEP_MODE);
}

void AFE_RequestParameterApply(void)
{
    s_data.mon.config_attempts = 0U;
    s_data.mon.config_attempted = 0U;
    AFE_PARAM_WRITE_Flag = 1;
    close_ctlc();
    AFE_MonitorInvalidate();
}

static void AFE_ApplyPendingConfiguration(UINT8 data_ok, UINT8 status_ok)
{
    AFE_MONITOR_RUNTIME *mon = &s_data.mon;
    UINT32 now = SysTime_Get10msTickCount();

    if (!AFE_PARAM_WRITE_Flag)
    {
        mon->config_attempts = 0U;
        mon->config_attempted = 0U;
        return;
    }
    if (!data_ok || !status_ok ||
        mon->config_attempts >= AFE_CONFIG_MAX_ATTEMPTS)
        return;
    if (mon->config_attempted &&
        (UINT32)(now - mon->config_attempt_tick) < AFE_CONFIG_RETRY_TICKS)
        return;

    mon->config_attempted = 1U;
    mon->config_attempt_tick = now;
    ++mon->config_attempts;
    close_ctlc();
    if (SH367309_UpdataAfeConfig())
        AFE_MonitorInvalidate(); /* Require fresh frames after any configuration. */
}

void open_ctlc(void)
{
    MCUO_AFE_CTLC = 1;
}
void close_ctlc(void)
{
    MCUO_AFE_CTLC = 0;
}

void App_AFEGet(void)
{
    UINT8 data_ok, status_ok, mos_ok, sample_result;
    if (SysTime_Take200msTaskPeriod() == 0U)
        return;

    MCUO_DEBUG_LED1 = !MCUO_DEBUG_LED1;
    sample_result = UpdateVoltageFromBqMaximo();
    data_ok = (UINT8)(sample_result == 0U);
    Fault_UpdatePackTemperatureResult(sample_result);
    if (data_ok)
    {
        DataLoad_CellVolt();
        DataLoad_CellVoltMaxMinFind();
        DataLoad_Temperature();
        DataLoad_TemperatureMaxMinFind();
        DataLoad_Current();
    }
    Fault_UpdateCurrentProtection(data_ok);
    status_ok = App_SH367309();
    AFE_MonitorUpdate(data_ok, status_ok, (UINT8)(sample_result != 1U));

    App_SOC(data_ok, status_ok);
    if (status_ok)
        SleepDeal_UpdateOverdischarge(g_stCellInfoReport.unMdlFault_Third.bits.b1CellUvp);
    else
        (void)SleepDeal_NormalIdleExpired(1U);

    /* Normal permits are applied once. CTL releases only after a verified
       healthy cycle, successful register readback and no pending profile. */
    mos_ok = Fault_ApplyMosPermits(1U, 1U);
    if (data_ok && status_ok)
        Sci_CustomerUpdateCounters();
    if (mos_ok && AFE_IsHealthy() && data_ok && status_ok)
        open_ctlc();
    AFE_ApplyPendingConfiguration(data_ok, status_ok);
}
