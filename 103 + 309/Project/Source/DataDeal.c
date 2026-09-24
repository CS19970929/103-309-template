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
#define AFE_CURRENT_ADC_FULL_SCALE_MV ((UINT32)200U)
#define AFE_CURRENT_ADC_DENOMINATOR ((UINT32)21470U)
#define BOOT_CURRENT_ZERO_SAMPLE_INTERVAL_MS ((UINT16)300U)
#define BOOT_CURRENT_ZERO_MAX_ABS_COUNTS ((UINT32)40U)
#define BOOT_CURRENT_ZERO_MAX_DELTA_COUNTS ((UINT32)6U)
#define BOOT_CURRENT_FET_STATUS_MASK ((UINT8)0x07U)
#define CURRENT_FIXED_SCALE ((UINT32)4U)
#define CURRENT_REPORT_MA_PER_LSB ((UINT16)100U)
/* Preserve the product-visible 0.2 A deadband regardless of calibration result. */
#define CURRENT_DEADBAND_MA ((UINT16)200U)

typedef struct _AFE_CURRENT_RUNTIME
{
    /* Boot zero is stored in raw-count x4 units to retain half-count averaging. */
    INT32 zeroOffsetRawX4;
    INT32 correctedRawX4;
    /* Calibrated signed current before product deadband. Unit: mA. */
    INT32 measured_mA;
    /* Effective signed current after product deadband. Unit: mA. */
    INT32 current_mA;
    INT16 bootRaw1;
    INT16 bootRaw2;
    INT16 runtimeRaw;
    UINT16 deadband_mA;
    UINT8 zeroStatus;
    UINT8 lastMtpConf;
    UINT8 lastBstatus3;
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
    // CHG_OFF;
    // DSG_OFF;
}

// 这里排列好就行，不需要电池位号映射表。>61000为不用
// 经过验算，AFE1校准一次，然后本身再校准一次叠加是可以的。不需要确定某一个KB值的做法。
// 假设先确定用AFE1还是本身的KB的话，会出现问题。如下：
// 假设需要整体校准，行，AFE1先行，然后发现某几串出问题，继续使用本身KB值，然后本身KB值需要同步前面AFE1的KB值一起算才行
// 如果又变成单独使用本身KB值校准，出现错误。
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
#endif // !1
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

    // 单片机读总压
    // u32VCellTotle = ((ADC_GetResult(ADC_VBC)*g_u16CalibCoefK[VOLT_VBUS])>>10) + (UINT32)g_i16CalibCoefB[VOLT_VBUS]*1000;
    // AFE读总压
    // u32VCellTotle = ((g_stBq769x0_Read_AFE1.u32VBat*g_u16CalibCoefK[VOLT_VBUS])>>10) + (UINT32)g_i16CalibCoefB[VOLT_VBUS]*1000;
    // 所有单节电池电压加起来
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

#if 0
	//环境温度1
	t_i32temp = ADC_GetResult(ADC_TEMP_EV1) / 10 - 40;		//放大1000倍和B值对应的意思
	//t_i32temp =  - 40;
	t_i32temp = ((t_i32temp * g_u16CalibCoefK[MDL_TEMP_ENV1]) + g_i16CalibCoefB[MDL_TEMP_ENV1])>>10;
	g_stCellInfoReport.u16Temperature[ENV_TEMP1] = (UINT16)(t_i32temp*10 + 400);
	Monitor_TempBreak(&g_stCellInfoReport.u16Temperature[ENV_TEMP1]);
#endif

    // 环境温度2
    // 如果没有，这个默认就是0(ADC.c不会调用)
    t_i32temp = ADC_GetResult(ADC_TEMP_EV2) / 10 - 40;
    t_i32temp = -40;
    t_i32temp = ((t_i32temp * g_u16CalibCoefK[MDL_TEMP_ENV2]) + g_i16CalibCoefB[MDL_TEMP_ENV2]) >> 10;
    g_stCellInfoReport.u16Temperature[ENV_TEMP2] = (UINT16)(t_i32temp * 10 + 400);

    // 环境温度3
    t_i32temp = ADC_GetResult(ADC_TEMP_EV3) / 10 - 40;
    t_i32temp = -40;
    t_i32temp = ((t_i32temp * g_u16CalibCoefK[MDL_TEMP_ENV3]) + g_i16CalibCoefB[MDL_TEMP_ENV3]) >> 10;
    g_stCellInfoReport.u16Temperature[ENV_TEMP3] = (UINT16)(t_i32temp * 10 + 400);

#if 1
    // MOS温度为散热片温度
    // 取两者最大值
    // t_i32temp = (ADC_GetResult(ADC_TEMP_MOS1) > ADC_GetResult(ADC_TEMP_MOS2) ? ADC_GetResult(ADC_TEMP_MOS1):ADC_GetResult(ADC_TEMP_MOS2));
    t_i32temp = ADC_GetResult(ADC_TEMP_MOS1);
    t_i32temp = t_i32temp / 10 - 40;
    t_i32temp = ((t_i32temp * g_u16CalibCoefK[MDL_TEMP_MOS1]) + g_i16CalibCoefB[MDL_TEMP_MOS1]) >> 10;
    g_stCellInfoReport.u16Temperature[MOS_TEMP1] = (UINT16)(t_i32temp * 10 + 400);
    Monitor_TempBreak(&g_stCellInfoReport.u16Temperature[MOS_TEMP1]);
#endif
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
    s_data.cur.deadband_mA = CURRENT_DEADBAND_MA;
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

    s_data.cur.lastMtpConf = confirmed_conf.all;

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

    s_data.cur.lastBstatus3 = bstatus3;
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
    s_data.cur.correctedRawX4 = 0;
    s_data.cur.measured_mA = 0;
    s_data.cur.current_mA = 0;
    s_data.cur.bootRaw1 = 0;
    s_data.cur.bootRaw2 = 0;
    s_data.cur.runtimeRaw = 0;
    s_data.cur.deadband_mA = CURRENT_DEADBAND_MA;
    s_data.cur.lastMtpConf = 0U;
    s_data.cur.lastBstatus3 = 0U;

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
    s_data.cur.bootRaw1 = (INT16)raw1;

    AfeCurrent_WaitFreshSample();
    if ((!AfeCurrent_CheckBootZeroSafe()) ||
        (!DataLoad_CurrentReadCadcRaw(&raw_code)) ||
        (!AfeCurrent_CheckBootZeroSafe()))
    {
        return;
    }
    raw2 = DataLoad_CurrentRawToSigned(raw_code);
    s_data.cur.bootRaw2 = (INT16)raw2;

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
    s_data.cur.deadband_mA = CURRENT_DEADBAND_MA;
}

INT32 AfeCurrent_GetMeasuredCurrent_mA(void)
{
    return s_data.cur.measured_mA;
}

INT32 AfeCurrent_GetCurrent_mA(void)
{
    return s_data.cur.current_mA;
}

void AfeCurrent_GetDiagnostics(AFE_CURRENT_DIAG *diag)
{
    if (diag == 0)
    {
        return;
    }

    diag->version = AFE_CURRENT_DIAG_VERSION;
    diag->zeroStatus = s_data.cur.zeroStatus;
    diag->zeroValid = (s_data.cur.zeroStatus == (UINT8)AFE_CURRENT_ZERO_VALID) ? 1U : 0U;
    diag->bootRaw1 = s_data.cur.bootRaw1;
    diag->bootRaw2 = s_data.cur.bootRaw2;
    diag->zeroRawX4 = s_data.cur.zeroOffsetRawX4;
    diag->runtimeRaw = s_data.cur.runtimeRaw;
    diag->correctedRawX4 = s_data.cur.correctedRawX4;
    diag->current_mA = s_data.cur.current_mA;
    diag->deadband_mA = s_data.cur.deadband_mA;
    diag->mtpConf = s_data.cur.lastMtpConf;
    diag->bstatus3 = s_data.cur.lastBstatus3;
}


/*
 * Current K/B calibration follows the existing project Q10 convention:
 *
 *   calibrated_mA = (nominal_mA * K + B) / 1024
 *
 * K=1024 and B=0 are identity. B is a residual correction after boot-zero,
 * in Q10*mA. Charge/discharge use independent coefficients.
 */
static INT32 DataLoad_CurrentApplyCalibration(INT32 nominal_mA)
{
    UINT16 calib_index;
    UINT32 magnitude_mA;
    UINT16 k;
    INT16 b;
    int64_t scaled;
    INT32 calibrated_mA;

    if (nominal_mA == 0)
    {
        return 0;
    }

    calib_index = (nominal_mA > 0) ? (UINT16)MDL_ICHG : (UINT16)MDL_IDSG;
    magnitude_mA = DataLoad_CurrentAbsI32(nominal_mA);
    k = g_u16CalibCoefK[calib_index];
    b = g_i16CalibCoefB[calib_index];

    scaled = ((int64_t)magnitude_mA * (int64_t)k) + (int64_t)b;
    if (scaled <= 0)
    {
        return 0;
    }

    scaled = (scaled + ((int64_t)SYSKDEFAULT / 2)) / (int64_t)SYSKDEFAULT;
    if (scaled > (int64_t)0x7FFFFFFF)
    {
        scaled = (int64_t)0x7FFFFFFF;
    }

    calibrated_mA = (INT32)scaled;
    return (nominal_mA > 0) ? calibrated_mA : -calibrated_mA;
}

static UINT16 DataLoad_CurrentMilliAmpToA10(UINT32 current_mA)
{
    UINT32 report_value;

    report_value = (current_mA + ((UINT32)CURRENT_REPORT_MA_PER_LSB / 2U)) /
                   (UINT32)CURRENT_REPORT_MA_PER_LSB;
    if (report_value > 0xFFFFU)
    {
        return 0xFFFFU;
    }

    return (UINT16)report_value;
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
    {
        if (g_stCellInfoReport.SocElement.u16Soc >= 99)
        {
            step = 2;
            sys_time.CHG = 0;
            sys_time.DSG = BMS_CAPCITY * 5;
            g_stCellInfoReport.u16Ichg = 0;
            g_stCellInfoReport.u16IDischg = sys_time.DSG;
        }
        break;
    }
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
    INT32 corrected_raw_x4;
    INT32 nominal_mA;
    INT32 calibrated_mA;
    UINT32 nominal_mA_x4;
    UINT32 nominal_abs_mA;
    UINT32 effective_abs_mA;
    UINT16 deadband_mA;

    raw_signed = DataLoad_CurrentRawToSigned(SH367309_Read_AFE1.u16Current);
    corrected_raw_x4 = raw_signed * (INT32)CURRENT_FIXED_SCALE;
    if (s_data.cur.zeroStatus == (UINT8)AFE_CURRENT_ZERO_VALID)
    {
        corrected_raw_x4 -= s_data.cur.zeroOffsetRawX4;
    }

    nominal_mA_x4 = DataLoad_CurrentRawX4ToMilliAmpX4(DataLoad_CurrentAbsI32(corrected_raw_x4));
    nominal_abs_mA = DataLoad_CurrentMilliAmpX4ToMilliAmp(nominal_mA_x4);

    if ((nominal_mA_x4 == 0U) || (corrected_raw_x4 == 0))
    {
        nominal_mA = 0;
    }
    else if (corrected_raw_x4 > 0)
    {
        nominal_mA = (INT32)nominal_abs_mA;
    }
    else
    {
        nominal_mA = -(INT32)nominal_abs_mA;
    }

    calibrated_mA = DataLoad_CurrentApplyCalibration(nominal_mA);
    s_data.cur.measured_mA = calibrated_mA;

    deadband_mA = CURRENT_DEADBAND_MA;
    if (DataLoad_CurrentAbsI32(calibrated_mA) < (UINT32)deadband_mA)
    {
        s_data.cur.current_mA = 0;
    }
    else
    {
        s_data.cur.current_mA = calibrated_mA;
    }

    s_data.cur.runtimeRaw = (INT16)raw_signed;
    s_data.cur.correctedRawX4 = corrected_raw_x4;
    s_data.cur.deadband_mA = deadband_mA;

    /*
     * Keep the legacy public data model unchanged: charge/discharge are still
     * separate UINT16 values in A*10 (100 mA/LSB). Only the internal path is
     * upgraded to signed mA.
     */
    g_stCellInfoReport.u16Ichg = 0U;
    g_stCellInfoReport.u16IDischg = 0U;

    effective_abs_mA = DataLoad_CurrentAbsI32(s_data.cur.current_mA);
    if (s_data.cur.current_mA > 0)
    {
        g_stCellInfoReport.u16Ichg = DataLoad_CurrentMilliAmpToA10(effective_abs_mA);
    }
    else if (s_data.cur.current_mA < 0)
    {
        g_stCellInfoReport.u16IDischg = DataLoad_CurrentMilliAmpToA10(effective_abs_mA);
    }

#ifdef __VIRTURE_CURRENT__
    if (sys_time.isdebugenable == 1)
    {
        INT32 virtual_mA;

        g_stCellInfoReport.u16Ichg = sys_time.CHG;
        g_stCellInfoReport.u16IDischg = sys_time.DSG;

        virtual_mA = ((INT32)sys_time.CHG - (INT32)sys_time.DSG) * 100;
        s_data.cur.measured_mA = virtual_mA;
        s_data.cur.current_mA = virtual_mA;
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
    /* Sleep after persistent storage communication faults too. */
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
    // static uint8_t Driver_Element_MOS_CHG = s_system_status.bits.b1Status_MOS_CHG;
    // static uint8_t DRIVER_ELEMENT_MOS_DSG = s_system_status.bits.b1Status_MOS_DSG;
    uint8_t Driver_Element_MOS_CHG = 1;
    uint8_t DRIVER_ELEMENT_MOS_DSG = 1;
#ifdef __SOC_5_PROTECT_
    static uint32_t soc_low_cnt = 0;
#endif
    // if(g_stCellInfoReport.unMdlFault_Third.all != 0)

    // static bool first = true;
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
#endif // __SOC_5_PROTECT_
    // 增加软件充电过流
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

    // switch (occ1_ararm_state)
    // {
    // case 0:
    //     if (g_stCellInfoReport.unMdlFault_Second.bits.b1IchgOcp)
    //     {
    //         FaultWarnRecord2(IchgOcp_Second);
    //         Driver_Element_MOS_CHG = 0;
    //         occ1_ararm_state = 1;
    //     }
    //     break;

    // default:
    //     break;
    // }

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

    // 软件控制mos的逻辑
    if (s_system_status.bits.b1Status_MOS_CHG != Driver_Element_MOS_CHG)
    {
        // log_w();
        sys_time.cnt_enter_chg_open++;
        SH367309_DriverMos_Ctrl(GPIO_CHG, Driver_Element_MOS_CHG);
    }
    if (s_system_status.bits.b1Status_MOS_DSG != DRIVER_ELEMENT_MOS_DSG)
    {
        // log_w();
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
    // DataLoad_soc_test();
    // test_Autocurrent_cycle();


    App_SH367309();
    new_todo_logi();
    App_SOC();

#ifdef VCELL_DISP_TEST
    // {
    //     g_stCellInfoReport.u16VCell[27] = g_stLowPowerRtcStatus.test_sample_voltage;
    //     g_stCellInfoReport.u16VCell[28] = g_stLowPowerRtcStatus.last;
    //     g_stCellInfoReport.u16VCell[29] = g_stLowPowerRtcStatus.cycles;
    //     g_stCellInfoReport.u16VCell[30] = g_stLowPowerRtcStatus.sleep;
    // }
    g_stCellInfoReport.u16VCell[31] = sys_time.rtc_sec_cnt;
#endif
}
