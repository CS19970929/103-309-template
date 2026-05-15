#include "main.h"

UINT8 u8IICFaultcnt1 = 0;
UINT8 u8WakeCnt1 = 0;
UINT8 u8IICFaultcnt2 = 0;
UINT8 u8WakeCnt2 = 0;

#define MONITOR_AFE_FAIL_LIMIT ((UINT8)50)
#define MONITOR_AFE_RECOVER_TRIGGER ((UINT8)30)
#define MONITOR_AFE_WAKE_RETRY_LIMIT ((UINT8)20)
#define MONITOR_AFE_TASK_PERIOD_MS ((UINT16)200)
#define MONITOR_AFE_SLEEP_DELAY_SEC ((UINT16)(5U * 60U))
#define MONITOR_AFE_SLEEP_DELAY_TICKS ((UINT16)((MONITOR_AFE_SLEEP_DELAY_SEC * 1000U) / MONITOR_AFE_TASK_PERIOD_MS))
#define AFE_CURRENT_ADC_FULL_SCALE_MV ((UINT32)200U)
#define AFE_CURRENT_ADC_DENOMINATOR ((UINT32)21470U)
#define AFE_CURRENT_AUTO_ZERO_LIMIT_MA ((UINT32)1000U)
#define AFE_CURRENT_AUTO_ZERO_STABLE_RAW ((UINT16)8U)
#define AFE_CURRENT_AUTO_ZERO_CONFIRM_CNT ((UINT8)16U)
#define AFE_CURRENT_AUTO_ZERO_FILTER_DIV ((INT32)16L)
#define AFE_CURRENT_CALIB_MIN_MA ((UINT32)2000U)
#define AFE_CURRENT_OUTPUT_DEADBAND_MA ((UINT32)200U)
#define AFE_CURRENT_OUTPUT_DEADBAND_A10 ((UINT16)2U)
typedef struct _AFE_CURRENT_STARTUP_ZERO_PARAM
{
    UINT8 u8ConfirmCnt;
    UINT8 u8MaxCnt;
    UINT8 u8DiscardCnt;
    UINT16 u16SettleMs;
    UINT16 u16IntervalMs;
} AFE_CURRENT_STARTUP_ZERO_PARAM;

static const UINT8 s_u8AfeCurrentKbCalibEnable = 0U;
static const AFE_CURRENT_STARTUP_ZERO_PARAM s_stAfeCurrentColdStartupZeroParam = {4U, 32U, 6U, 800U, 25U};
static const AFE_CURRENT_STARTUP_ZERO_PARAM s_stAfeCurrentWarmStartupZeroParam = {4U, 16U, 2U, 120U, 20U};
static UINT8 s_u8AfeCurrentStartupColdBoot = 1U;

UINT16 g_u16CalibCoefK[KB_NUM];
INT16 g_i16CalibCoefB[KB_NUM];

UINT16 CopperLoss[CompensateNUM]; // uΩ
UINT16 CopperLoss_Num[CompensateNUM];

UINT32 g_u32CS_Res_AFE = 0;
UINT32 g_u32AfeCurrentSampleSeq = 0U;
AFE_CURRENT_OBSERVE g_stAfeCurrentObserve = {0};
INT32 g_i32AfeCurrentZeroOffsetRawQ4 = 0;
INT32 g_i32AfeCurrentLastRawSigned = 0;
UINT8 g_u8AfeCurrentZeroStableCnt = 0;
UINT8 g_u8AfeCurrentZeroReady = 0;
UINT8 g_u8AfeCurrentZeroState = (UINT8)AFE_CURRENT_ZERO_IDLE;

struct OTHER_ELEMENT OtherElement;

UINT32 u32_ChgCur_mA = 0;
UINT32 u32_DsgCur_mA = 0;

void charger_detect_and_keyLogi_200ms(void)
{
    static uint8_t state = 0;

    switch (state)
    {
    case 0:
        if (!GPIO_ReadInputDataBit(GPIO_CHG_IN, PIN_CHG_IN))
        {
            state = 1;
            open_chg_close_dsg();
        }
        else if (SleepDeal_IsBootFromSleepChargerWakeup() != 0U)
        {
            LedBar_SaveSleepSoc();
            entersleep(NORMAL_MODE);
        }
        else
        {
        }
        break;
    case 1:
        if (GPIO_ReadInputDataBit(GPIO_CHG_IN, PIN_CHG_IN))
        {
            state = 0;
            if (SleepDeal_IsBootFromSleepChargerWakeup() != 0U)
            {
                LedBar_SaveSleepSoc();
                entersleep(NORMAL_MODE);
            }
            else
            {
                open_dsg_close_chg();
            }
        }
        else
        {
        }
        break;
    default:
        state = 0;
        break;
    }

    App_DI1_Switch();
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

// UINT32 aaaaa1 = 0;
// UINT32 aaaaa2 = 0;

void DataLoad_CellVolt_Test(void)
{
    g_stCellInfoReport.u16VCell[23] = SH367309_Reg_Store.REG_BSTATUS1.all;
    g_stCellInfoReport.u16VCell[24] = SH367309_Reg_Store.REG_BSTATUS2.all;
    g_stCellInfoReport.u16VCell[25] = SH367309_Reg_Store.REG_BSTATUS3.all;

    g_stCellInfoReport.u16VCell[27] = aaaaaa1;
    g_stCellInfoReport.u16VCell[28] = aaaaaa2;
    g_stCellInfoReport.u16VCell[29] = aaaaaa3;
    g_stCellInfoReport.u16VCell[30] = aaaaaa4;
    g_stCellInfoReport.u16VCell[31] = aaa11;
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
        // if (g_tParam.CalibCoefK[VOLT_AFE1] != 1024 || g_tParam.CalibCoefB[VOLT_AFE1] != 0)
        // {
        // 	t_i32temp = ((t_i32temp * g_tParam.CalibCoefK[VOLT_AFE1]) >> 10) + g_tParam.CalibCoefB[VOLT_AFE1];
        // }
        t_i32temp = ((t_i32temp * SYSKDEFAULT) >> 10) + SYSBDEFAULT;
        t_i32temp = t_i32temp > 0 ? t_i32temp : 0;
        g_stCellInfoReport.u16VCell[i] = (UINT16)t_i32temp;
    }

    if (SeriesNum < 32)
    {
        for (i = SeriesNum; i < 31; ++i)
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
    INT32 i32VCellTotle;

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
    // u32VCellTotle = ((g_i32ADCResult[ADC_VBC]*g_u16CalibCoefK[VOLT_VBUS])>>10) + (UINT32)g_i16CalibCoefB[VOLT_VBUS]*1000;
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
	t_i32temp = g_i32ADCResult[ADC_TEMP_EV1] / 10 - 40;		//放大1000倍和B值对应的意思
	//t_i32temp =  - 40;
	t_i32temp = ((t_i32temp * g_u16CalibCoefK[MDL_TEMP_ENV1]) + g_i16CalibCoefB[MDL_TEMP_ENV1])>>10;
	g_stCellInfoReport.u16Temperature[ENV_TEMP1] = (UINT16)(t_i32temp*10 + 400);
	Monitor_TempBreak(&g_stCellInfoReport.u16Temperature[ENV_TEMP1]);
#endif

    // 环境温度2
    // 如果没有，这个默认就是0(ADC.c不会调用)
    t_i32temp = g_i32ADCResult[ADC_TEMP_EV2] / 10 - 40;
    t_i32temp = -40;
    t_i32temp = ((t_i32temp * g_u16CalibCoefK[MDL_TEMP_ENV2]) + g_i16CalibCoefB[MDL_TEMP_ENV2]) >> 10;
    g_stCellInfoReport.u16Temperature[ENV_TEMP2] = (UINT16)(t_i32temp * 10 + 400);

    // 环境温度3
    t_i32temp = g_i32ADCResult[ADC_TEMP_EV3] / 10 - 40;
    t_i32temp = -40;
    t_i32temp = ((t_i32temp * g_u16CalibCoefK[MDL_TEMP_ENV3]) + g_i16CalibCoefB[MDL_TEMP_ENV3]) >> 10;
    g_stCellInfoReport.u16Temperature[ENV_TEMP3] = (UINT16)(t_i32temp * 10 + 400);

#if 1
    // MOS温度为散热片温度
    // 取两者最大值
    // t_i32temp = (g_i32ADCResult[ADC_TEMP_MOS1] > g_i32ADCResult[ADC_TEMP_MOS2] ? g_i32ADCResult[ADC_TEMP_MOS1]:g_i32ADCResult[ADC_TEMP_MOS2]);
    t_i32temp = g_i32ADCResult[ADC_TEMP_MOS1];
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

static INT32 DataLoad_CurrentClampZeroOffset(INT32 offset_raw)
{
    INT32 limit_raw = (INT32)DataLoad_CurrentMilliAmpToRaw(AFE_CURRENT_AUTO_ZERO_LIMIT_MA);

    if (limit_raw <= 0)
    {
        return 0;
    }

    if (offset_raw > limit_raw)
    {
        return limit_raw;
    }

    if (offset_raw < -limit_raw)
    {
        return -limit_raw;
    }

    return offset_raw;
}

static void AfeCurrent_ObserveReset(void)
{
    memset((void *)&g_stAfeCurrentObserve, 0, sizeof(g_stAfeCurrentObserve));
    g_u8AfeCurrentZeroState = (UINT8)AFE_CURRENT_ZERO_IDLE;
    g_stAfeCurrentObserve.u8ZeroState = g_u8AfeCurrentZeroState;
    g_stAfeCurrentObserve.u8KbCalibEnable = s_u8AfeCurrentKbCalibEnable;
    g_stAfeCurrentObserve.u8StartupColdBoot = s_u8AfeCurrentStartupColdBoot;
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

static void DataLoad_CurrentSetZeroOffset(INT32 offset_raw, UINT8 zero_state)
{
    offset_raw = DataLoad_CurrentClampZeroOffset(offset_raw);

    g_i32AfeCurrentZeroOffsetRawQ4 = offset_raw * AFE_CURRENT_AUTO_ZERO_FILTER_DIV;
    g_i32AfeCurrentLastRawSigned = offset_raw;
    g_u8AfeCurrentZeroStableCnt = AFE_CURRENT_AUTO_ZERO_CONFIRM_CNT;
    g_u8AfeCurrentZeroReady = 1U;
    g_u8AfeCurrentZeroState = zero_state;

    g_stAfeCurrentObserve.i32ZeroOffsetRaw = offset_raw;
    g_stAfeCurrentObserve.i32CorrectedRaw = 0;
    g_stAfeCurrentObserve.u8ZeroReady = 1U;
    g_stAfeCurrentObserve.u8StableCnt = g_u8AfeCurrentZeroStableCnt;
    g_stAfeCurrentObserve.u8ZeroState = zero_state;
}

static void DataLoad_CurrentMarkZeroPending(UINT8 zero_state)
{
    g_i32AfeCurrentZeroOffsetRawQ4 = 0;
    g_i32AfeCurrentLastRawSigned = 0;
    g_u8AfeCurrentZeroStableCnt = 0U;
    g_u8AfeCurrentZeroReady = 0U;
    g_u8AfeCurrentZeroState = zero_state;

    g_stAfeCurrentObserve.i32ZeroOffsetRaw = 0;
    g_stAfeCurrentObserve.i32CorrectedRaw = g_stAfeCurrentObserve.i32RawSigned;
    g_stAfeCurrentObserve.u8ZeroReady = 0U;
    g_stAfeCurrentObserve.u8StableCnt = 0U;
    g_stAfeCurrentObserve.u8ZeroState = zero_state;
}
void AfeCurrent_SetStartupColdBoot(UINT8 cold_boot)
{
    s_u8AfeCurrentStartupColdBoot = (cold_boot != 0U) ? 1U : 0U;
    g_stAfeCurrentObserve.u8StartupColdBoot = s_u8AfeCurrentStartupColdBoot;
}

static const AFE_CURRENT_STARTUP_ZERO_PARAM *AfeCurrent_GetStartupZeroParam(void)
{
    if (s_u8AfeCurrentStartupColdBoot != 0U)
    {
        return &s_stAfeCurrentColdStartupZeroParam;
    }

    return &s_stAfeCurrentWarmStartupZeroParam;
}
void AfeCurrent_PrepareStartupZero(void)
{
    g_i32AfeCurrentZeroOffsetRawQ4 = 0;
    g_i32AfeCurrentLastRawSigned = 0;
    g_u8AfeCurrentZeroStableCnt = 0U;
    g_u8AfeCurrentZeroReady = 0U;

    AfeCurrent_ObserveReset();
    g_u8AfeCurrentZeroState = (UINT8)AFE_CURRENT_ZERO_STARTUP;
    g_stAfeCurrentObserve.u8ZeroState = g_u8AfeCurrentZeroState;
    close_ctlc();
}

UINT8 AfeCurrent_IsStartupZeroDone(void)
{
    if ((g_u8AfeCurrentZeroState == (UINT8)AFE_CURRENT_ZERO_READY) ||
        (g_u8AfeCurrentZeroState == (UINT8)AFE_CURRENT_ZERO_TIMEOUT) ||
        (g_u8AfeCurrentZeroState == (UINT8)AFE_CURRENT_ZERO_IIC_FAIL) ||
        (g_u8AfeCurrentZeroState == (UINT8)AFE_CURRENT_ZERO_RANGE_FAIL))
    {
        return 1U;
    }

    return 0U;
}

void AfeCurrent_StartupZeroCal(void)
{
    UINT8 i;
    UINT8 valid_cnt;
    UINT8 stable_cnt;
    UINT8 fail_cnt;
    UINT8 discard_cnt;
    UINT8 range_fail_cnt;
    UINT8 zero_done;
    UINT16 raw_code;
    UINT16 limit_raw;
    INT32 raw_signed;
    INT32 last_raw_signed;
    INT32 sum_raw_signed;
    UINT32 raw_abs;
    UINT32 delta_abs;
    const AFE_CURRENT_STARTUP_ZERO_PARAM *param;

    valid_cnt = 0U;
    stable_cnt = 0U;
    fail_cnt = 0U;
    discard_cnt = 0U;
    range_fail_cnt = 0U;
    zero_done = 0U;
    raw_code = 0U;
    raw_signed = 0;
    last_raw_signed = 0;
    sum_raw_signed = 0;
    param = AfeCurrent_GetStartupZeroParam();
    limit_raw = DataLoad_CurrentMilliAmpToRaw(AFE_CURRENT_AUTO_ZERO_LIMIT_MA);

    close_ctlc();
    g_u8AfeCurrentZeroState = (UINT8)AFE_CURRENT_ZERO_STARTUP;
    g_stAfeCurrentObserve.u8ZeroState = g_u8AfeCurrentZeroState;
    g_stAfeCurrentObserve.u16ZeroLimitRaw = limit_raw;
    g_stAfeCurrentObserve.u8StartupSampleCnt = 0U;
    g_stAfeCurrentObserve.u8StartupColdBoot = s_u8AfeCurrentStartupColdBoot;
    g_stAfeCurrentObserve.u8StartupDiscardCnt = 0U;
    g_stAfeCurrentObserve.u8StartupFailCnt = 0U;
    g_stAfeCurrentObserve.u8StartupRangeFailCnt = 0U;

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

            g_stAfeCurrentObserve.u16RawCode = raw_code;
            g_stAfeCurrentObserve.i32RawSigned = raw_signed;
            g_stAfeCurrentObserve.u32AbsRaw = raw_abs;
            g_stAfeCurrentObserve.u8StartupSampleCnt = (UINT8)(i + 1U);

            if (discard_cnt < param->u8DiscardCnt)
            {
                ++discard_cnt;
                g_stAfeCurrentObserve.u8StartupDiscardCnt = discard_cnt;
                last_raw_signed = raw_signed;
                valid_cnt = 0U;
                stable_cnt = 0U;
                sum_raw_signed = 0;
            }
            else if ((limit_raw > 0U) && (raw_abs <= (UINT32)limit_raw))
            {
                if (valid_cnt == 0U)
                {
                    sum_raw_signed = raw_signed;
                    stable_cnt = 1U;
                    valid_cnt = 1U;
                }
                else
                {
                    delta_abs = DataLoad_CurrentAbsI32(raw_signed - last_raw_signed);
                    if (delta_abs <= (UINT32)AFE_CURRENT_AUTO_ZERO_STABLE_RAW)
                    {
                        if (valid_cnt < 0xFFU)
                        {
                            ++valid_cnt;
                        }
                        if (stable_cnt < 0xFFU)
                        {
                            ++stable_cnt;
                        }
                        sum_raw_signed += raw_signed;
                    }
                    else
                    {
                        sum_raw_signed = raw_signed;
                        stable_cnt = 1U;
                        valid_cnt = 1U;
                    }
                }

                last_raw_signed = raw_signed;
                g_stAfeCurrentObserve.u8StableCnt = stable_cnt;

                if (stable_cnt >= param->u8ConfirmCnt)
                {
                    DataLoad_CurrentSetZeroOffset(sum_raw_signed / (INT32)valid_cnt, (UINT8)AFE_CURRENT_ZERO_READY);
                    zero_done = 1U;
                    break;
                }
            }
            else
            {
                if (range_fail_cnt < 0xFFU)
                {
                    ++range_fail_cnt;
                }
                g_stAfeCurrentObserve.u8StartupRangeFailCnt = range_fail_cnt;
                last_raw_signed = raw_signed;
                valid_cnt = 0U;
                stable_cnt = 0U;
                sum_raw_signed = 0;
                g_stAfeCurrentObserve.u8StableCnt = 0U;
            }
        }
        else
        {
            if (fail_cnt < 0xFFU)
            {
                ++fail_cnt;
            }
            g_stAfeCurrentObserve.u8StartupFailCnt = fail_cnt;
        }

        if ((i + 1U) < param->u8MaxCnt)
        {
            __delay_ms(param->u16IntervalMs);
        }
    }

    g_stAfeCurrentObserve.u8StartupDiscardCnt = discard_cnt;
    g_stAfeCurrentObserve.u8StartupFailCnt = fail_cnt;
    g_stAfeCurrentObserve.u8StartupRangeFailCnt = range_fail_cnt;

    if (zero_done == 0U)
    {
        if (valid_cnt > 0U)
        {
            DataLoad_CurrentSetZeroOffset(sum_raw_signed / (INT32)valid_cnt, (UINT8)AFE_CURRENT_ZERO_TIMEOUT);
        }
        else if (fail_cnt >= param->u8MaxCnt)
        {
            DataLoad_CurrentMarkZeroPending((UINT8)AFE_CURRENT_ZERO_IIC_FAIL);
        }
        else
        {
            DataLoad_CurrentMarkZeroPending((UINT8)AFE_CURRENT_ZERO_RANGE_FAIL);
        }
    }

    open_ctlc();
}
static INT32 DataLoad_CurrentApplyAutoZero(INT32 raw_signed)
{
    INT32 current_offset_raw;
    INT32 target_q4;
    INT32 corrected_raw;
    UINT16 limit_raw;
    UINT16 deadband_raw;
    UINT32 learn_abs;
    UINT32 delta_abs;
    UINT8 can_learn;

    current_offset_raw = g_i32AfeCurrentZeroOffsetRawQ4 / AFE_CURRENT_AUTO_ZERO_FILTER_DIV;
    limit_raw = DataLoad_CurrentMilliAmpToRaw(AFE_CURRENT_AUTO_ZERO_LIMIT_MA);
    deadband_raw = DataLoad_CurrentMilliAmpToRaw(AFE_CURRENT_OUTPUT_DEADBAND_MA);
    delta_abs = DataLoad_CurrentAbsI32(raw_signed - g_i32AfeCurrentLastRawSigned);
    can_learn = 0U;

    if (g_u8AfeCurrentZeroReady == 0U)
    {
        learn_abs = DataLoad_CurrentAbsI32(raw_signed);
        if ((limit_raw > 0U) && (learn_abs <= (UINT32)limit_raw))
        {
            can_learn = 1U;
        }
    }
    else
    {
        learn_abs = DataLoad_CurrentAbsI32(raw_signed - current_offset_raw);
        if ((deadband_raw > 0U) && (learn_abs <= (UINT32)deadband_raw))
        {
            can_learn = 1U;
        }
    }

    if ((can_learn != 0U) && (delta_abs <= (UINT32)AFE_CURRENT_AUTO_ZERO_STABLE_RAW))
    {
        if (g_u8AfeCurrentZeroStableCnt < AFE_CURRENT_AUTO_ZERO_CONFIRM_CNT)
        {
            ++g_u8AfeCurrentZeroStableCnt;
        }

        if (g_u8AfeCurrentZeroStableCnt >= AFE_CURRENT_AUTO_ZERO_CONFIRM_CNT)
        {
            target_q4 = DataLoad_CurrentClampZeroOffset(raw_signed) * AFE_CURRENT_AUTO_ZERO_FILTER_DIV;
            if (g_u8AfeCurrentZeroReady == 0U)
            {
                g_i32AfeCurrentZeroOffsetRawQ4 = target_q4;
                g_u8AfeCurrentZeroReady = 1U;
                g_u8AfeCurrentZeroState = (UINT8)AFE_CURRENT_ZERO_READY;
            }
            else
            {
                g_i32AfeCurrentZeroOffsetRawQ4 += (target_q4 - g_i32AfeCurrentZeroOffsetRawQ4) / AFE_CURRENT_AUTO_ZERO_FILTER_DIV;
            }
        }
    }
    else
    {
        g_u8AfeCurrentZeroStableCnt = 0U;
    }

    g_i32AfeCurrentLastRawSigned = raw_signed;
    current_offset_raw = g_i32AfeCurrentZeroOffsetRawQ4 / AFE_CURRENT_AUTO_ZERO_FILTER_DIV;
    corrected_raw = raw_signed - current_offset_raw;

    g_stAfeCurrentObserve.i32RawSigned = raw_signed;
    g_stAfeCurrentObserve.i32ZeroOffsetRaw = current_offset_raw;
    g_stAfeCurrentObserve.i32CorrectedRaw = corrected_raw;
    g_stAfeCurrentObserve.u16ZeroLimitRaw = limit_raw;
    g_stAfeCurrentObserve.u16ZeroDeadbandRaw = deadband_raw;
    g_stAfeCurrentObserve.u16ZeroDeltaRaw = (delta_abs > 0xFFFFU) ? 0xFFFFU : (UINT16)delta_abs;
    g_stAfeCurrentObserve.u8StableCnt = g_u8AfeCurrentZeroStableCnt;
    g_stAfeCurrentObserve.u8ZeroReady = g_u8AfeCurrentZeroReady;
    if ((g_u8AfeCurrentZeroReady != 0U) &&
        ((g_u8AfeCurrentZeroState == (UINT8)AFE_CURRENT_ZERO_IDLE) ||
         (g_u8AfeCurrentZeroState == (UINT8)AFE_CURRENT_ZERO_STARTUP)))
    {
        g_u8AfeCurrentZeroState = (UINT8)AFE_CURRENT_ZERO_READY;
    }
    g_stAfeCurrentObserve.u8ZeroState = g_u8AfeCurrentZeroState;

    return corrected_raw;
}

static UINT32 DataLoad_CurrentApplyCalib(UINT32 current_mA, UINT8 calib_index)
{
    UINT16 calib_k;
    INT16 calib_b;
    int64_t current_q10;

    if (s_u8AfeCurrentKbCalibEnable == 0U)
    {
        (void)calib_index;
        return current_mA;
    }

    if (current_mA <= AFE_CURRENT_CALIB_MIN_MA)
    {
        return current_mA;
    }

    calib_k = g_u16CalibCoefK[calib_index];
    calib_b = g_i16CalibCoefB[calib_index];
    if ((calib_k < SYSKMIN) || (calib_k > SYSKMAX))
    {
        calib_k = SYSKDEFAULT;
    }
    if ((calib_b < SYSBMIN) || (calib_b > SYSBMAX))
    {
        calib_b = SYSBDEFAULT;
    }

    current_q10 = ((int64_t)current_mA * (int64_t)calib_k) + ((int64_t)calib_b * 1000LL);
    if (current_q10 <= 0)
    {
        return 0U;
    }

    current_q10 >>= 10;
    if (current_q10 > 0xFFFFFFFFU)
    {
        return 0xFFFFFFFFU;
    }

    return (UINT32)current_q10;
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

void DataLoad_Current(void)
{
    INT32 raw_signed;
    INT32 corrected_raw;
    UINT32 current_mA;

    g_stAfeCurrentObserve.u8KbCalibEnable = s_u8AfeCurrentKbCalibEnable;
    g_stAfeCurrentObserve.u16RawCode = SH367309_Read_AFE1.u16Current;

    raw_signed = DataLoad_CurrentRawToSigned(SH367309_Read_AFE1.u16Current);
    corrected_raw = DataLoad_CurrentApplyAutoZero(raw_signed);
    current_mA = DataLoad_CurrentRawToMilliAmp(DataLoad_CurrentAbsI32(corrected_raw));

    u32_ChgCur_mA = 0;
    u32_DsgCur_mA = 0;
    g_stCellInfoReport.u16Ichg = 0;
    g_stCellInfoReport.u16IDischg = 0;

    if (corrected_raw > 0)
    {
        u32_ChgCur_mA = DataLoad_CurrentApplyCalib(current_mA, (UINT8)MDL_ICHG);
        g_stCellInfoReport.u16Ichg = DataLoad_CurrentMilliAmpToA10(u32_ChgCur_mA);
    }
    else if (corrected_raw < 0)
    {
        u32_DsgCur_mA = DataLoad_CurrentApplyCalib(current_mA, (UINT8)MDL_IDSG);
        g_stCellInfoReport.u16IDischg = DataLoad_CurrentMilliAmpToA10(u32_DsgCur_mA);
    }

#ifdef __VIRTURE_CURRENT__
    if (sys_time.isdebugenable == 1)
    {
        g_stCellInfoReport.u16Ichg = sys_time.CHG;
        g_stCellInfoReport.u16IDischg = sys_time.DSG;
    }
#endif

    g_stAfeCurrentObserve.u32AbsRaw = DataLoad_CurrentAbsI32(corrected_raw);
    g_stAfeCurrentObserve.u32Current_mA = current_mA;
    g_stAfeCurrentObserve.u32ChgCurrent_mA = u32_ChgCur_mA;
    g_stAfeCurrentObserve.u32DsgCurrent_mA = u32_DsgCur_mA;
    g_stAfeCurrentObserve.u16Ichg_A10 = g_stCellInfoReport.u16Ichg;
    g_stAfeCurrentObserve.u16IDsg_A10 = g_stCellInfoReport.u16IDischg;

    if (g_stCellInfoReport.u16Ichg != 0U)
    {
        g_stAfeCurrentObserve.u8Direction = (UINT8)AFE_CURRENT_DIR_CHG;
    }
    else if (g_stCellInfoReport.u16IDischg != 0U)
    {
        g_stAfeCurrentObserve.u8Direction = (UINT8)AFE_CURRENT_DIR_DSG;
    }
    else
    {
        g_stAfeCurrentObserve.u8Direction = (UINT8)AFE_CURRENT_DIR_ZERO;
    }
}

static void MonitorAFE_SetStatus(UINT8 num, UINT8 is_ok)
{
    switch (num)
    {
    case 0:
        SystemStatus.bits.b1Status_AFE1 = is_ok;
        break;
    case 1:
        SystemStatus.bits.b1Status_AFE2 = is_ok;
        break;
    default:
        break;
    }
}

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
    if ((fault_cnt == 0) || (wake_cnt == 0))
    {
        return;
    }

    if (result != 0)
    {
        if (*fault_cnt < 0xFFU)
        {
            ++(*fault_cnt);
        }

        if (*fault_cnt > MONITOR_AFE_FAIL_LIMIT)
        {
            Init_Registers(num);
            *fault_cnt = 0;
            MonitorAFE_ReportError(num);
        }

        if ((*fault_cnt == MONITOR_AFE_RECOVER_TRIGGER) && (*wake_cnt < MONITOR_AFE_WAKE_RETRY_LIMIT))
        {
            MonitorAFE_Recover(num);
            ++(*wake_cnt);
        }

        MonitorAFE_SetStatus(num, 0);
    }
    else
    {
        if (*fault_cnt > 0)
        {
            --(*fault_cnt);
        }

        if (*wake_cnt > 0)
        {
            --(*wake_cnt);
        }

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
            entersleep(NORMAL_MODE);
        }
    }
    else
    {
        *delay_tick = 0;
    }
}

void MonitorAFE(UINT8 num, UINT8 Result)
{
    static UINT16 su16_Sleep_DelayT1 = 0;
    static UINT16 su16_Sleep_DelayT2 = 0;
    static UINT16 su16_Sleep_DelayT3 = 0;

    switch (num)
    {
    case 0:
        MonitorAFE_UpdateChannel(num, Result, &u8IICFaultcnt1, &u8WakeCnt1);
        break;

    case 1:
        MonitorAFE_UpdateChannel(num, Result, &u8IICFaultcnt2, &u8WakeCnt2);
        break;
    default:
        break;
    }

    MonitorAFE_UpdateSleepDelay(System_ERROR_UserCallback(ERROR_STATUS_AFE1), &su16_Sleep_DelayT1);
    MonitorAFE_UpdateSleepDelay(System_ERROR_UserCallback(ERROR_STATUS_AFE2), &su16_Sleep_DelayT2);
    /* Sleep after persistent storage communication faults too. */
    MonitorAFE_UpdateSleepDelay((UINT8)(System_ERROR_UserCallback(ERROR_STATUS_EEPROM_COM) ||
                                        System_ERROR_UserCallback(ERROR_STATUS_EEPROM_STORE)),
                                &su16_Sleep_DelayT3);
}

void test_Autocurrent_cycle(void)
{
    static uint8_t step = 0;
#if 1
    static uint16_t CHG_current = 200;
    static uint16_t DSG_current = 400;
#else
    static uint16_t CHG_current = 200;
    static uint16_t DSG_current = 400;
#endif

    switch (step)
    {
    case 0:
        if (g_stCellInfoReport.SocElement.u16Soc < 99)
        {
            step = 1;
            g_stCellInfoReport.u16Ichg = CHG_current;
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
            g_stCellInfoReport.u16Ichg = 0;
            g_stCellInfoReport.u16IDischg = DSG_current;
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
void open_ctlc(void)
{
    MCUO_AFE_CTLC = 1;
    g_stAfeCurrentObserve.u8CtlcState = 1U;
}
void close_ctlc(void)
{
    MCUO_AFE_CTLC = 0;
    g_stAfeCurrentObserve.u8CtlcState = 0U;
    // todo 会不会存在冲突，逻辑完备？？？
    GPIO_WriteBit(GPIO_MCC_C, PIN_MCC_C, Bit_RESET);
}

// todo 总压、typec逻辑、电流
void new_todo_logi(void)
{
    static uint8_t mos_state = 0;

    charger_detect_and_keyLogi_200ms();

#if 1
    // // todo 什么电平唤醒？
    // if (GPIO_ReadInputDataBit(GPIO_MCU_WK, PIN_MCU_WK))
    // {
    // }
    // // todo 待确认 typec供电逻辑
    // GPIO_WriteBit(GPIO_DC_EN, PIN_DC_EN, Bit_SET);
    {
#ifdef DISP_VBAT_AND_TEMP_
        g_stCellInfoReport.u16VCell[29] = bat_temp_mv;
        g_stCellInfoReport.u16VCell[30] = mos_temp_mv;
        g_stCellInfoReport.u16VCell[31] = Vbat_mv;
#endif // ! FAC_TEST
       // g_stCellInfoReport.u16VCell[30] = g_u32Vbat_mV;
        UINT32 Vbat_mv = g_u32Vbat_mV;
#ifdef _UL_RENZHENG_ENABLE_
        static uint8_t state_fuse = 0;
        static uint32_t rong_fuse_afe_err_cnt = 0;
        static uint32_t rong_fuse = 0;
#endif

        switch (mos_state)
        {
        case 0:
            if (g_stCellInfoReport.u16Temperature[MOS_TEMP1] >= (95 + 40) * 10)
            {
                close_ctlc();
                FaultWarnRecord2(MosOTp_Third);
                mos_state = 1;
            }
            break;
        case 1:
            if (g_stCellInfoReport.u16Temperature[MOS_TEMP1] <= (75 + 40) * 10)
            {
                open_ctlc();
                mos_state = 0;
            }
            break;
        default:
            mos_state = 0;
            break;
        }

#ifdef _UL_RENZHENG_ENABLE_
        static bool err_afe = false;

        if (1 == System_ErrFlag.u8ErrFlag_Com_AFE1)
        {
            err_afe = 1;
            rong_fuse = 0;
            state_fuse = 0;

            close_ctlc();
            // todo mcc关了，when 开
            if (Vbat_mv >= 4280 * SeriesNum || g_stCellInfoReport.u16Temperature[1] >= (85 + 40) * 10)
            {
                if (++rong_fuse_afe_err_cnt >= 10)
                {
                    rong_fuse_afe_err_cnt = 0;
#ifdef _UL_RENZHENG_ENABLE_
                    GPIO_WriteBit(GPIO_RF_EN, PIN_RF_EN, Bit_SET);
#endif
                }
            }
        }
        else
        {
            static uint16_t delay_cnt = 0;
            if (err_afe && 0 == System_ErrFlag.u8ErrFlag_Com_AFE1)
            {
                err_afe = 0;
                open_ctlc();
            }

            switch (state_fuse)
            {
            case 0:
                if ((g_stCellInfoReport.u16Temperature[1] >= (80 + 40) * 10))
                {
                    state_fuse = 1;
                    close_ctlc();
                    FaultWarnRecord2(CellChgOTp_Third);
                    FaultWarnRecord2(CellDsgOTp_Third);
                }
                if ((g_stCellInfoReport.u16VCellMax >= 4270) && (g_stCellInfoReport.u16VCellMin >= 2000))
                {
                    ++delay_cnt;
                    if (delay_cnt >= 15)
                    {
                        delay_cnt = 0;
                        state_fuse = 1;
                        close_ctlc();
                        // 是否应该强制关掉放电？？？
                        FaultWarnRecord2(CellOvp_Third);
                        FaultWarnRecord2(BatOvp_Third);
                    }
                }
                else
                    delay_cnt = 0;
                break;
            case 1:
                if ((g_stCellInfoReport.u16Temperature[1] < (75 + 40) * 10) && (g_stCellInfoReport.u16VCellMax <= 4150))
                {
                    state_fuse = 0;
                    open_ctlc();
                }
                if (((g_stCellInfoReport.u16VCellMax >= 4280) || (Vbat_mv >= 4280 * SeriesNum) || g_stCellInfoReport.u16Temperature[1] >= (85 + 40) * 10) && (g_stCellInfoReport.u16Ichg))
                {
                    if (++rong_fuse >= (15))
                    {
                        rong_fuse = 0;
#ifdef _UL_RENZHENG_ENABLE_
                        GPIO_WriteBit(GPIO_RF_EN, PIN_RF_EN, Bit_SET);
#endif
                    }
                }
                else
                {
                    rong_fuse = 0;
                }
                break;
            default:
                state_fuse = 0;
                break;
            }
        }
#endif
    }

    // 74hc595 控制5pin 18 seg led ,待完善spi驱动、配置
#endif
}

void App_AFEGet(void)
{
    // if (0 == g_st_SysTimeFlag.bits.b1Sys200msFlag || 0 != Sci_IsAnyPortBusy())
    if (0U == SysTime_Take200msTaskPeriod())
        return;

    MCUO_DEBUG_LED1 = !MCUO_DEBUG_LED1;
    MonitorAFE(0, UpdateVoltageFromBqMaximo());

    DataLoad_CellVolt();
    DataLoad_CellVoltMaxMinFind();
    DataLoad_Temperature();
    DataLoad_TemperatureMaxMinFind();
    DataLoad_Current();
    if (++g_u32AfeCurrentSampleSeq == 0U)
    {
        ++g_u32AfeCurrentSampleSeq;
    }

    App_SH367309();
    // App_MOS_Relay_Ctrl();
    new_todo_logi();
    App_SOC();
}
