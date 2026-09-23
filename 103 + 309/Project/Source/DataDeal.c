#include "main.h"

UINT8 u8IICFaultcnt1 = 0;
UINT8 u8WakeCnt1 = 0;
UINT8 u8IICFaultcnt2 = 0;
UINT8 u8WakeCnt2 = 0;

UINT16 g_u16CalibCoefK[KB_NUM];
INT16 g_i16CalibCoefB[KB_NUM];

UINT16 CopperLoss[CompensateNUM]; // uΩ
UINT16 CopperLoss_Num[CompensateNUM];

UINT32 g_u32CS_Res_AFE = 0;

struct OTHER_ELEMENT OtherElement;

/* SH367309 V1.1: CADC is signed 16-bit at 4 Hz. */
#define SH309_CADC_NUMERATOR_REDUCED          20000u
#define SH309_CADC_DENOMINATOR_REDUCED         2147u
#define BOOT_CURRENT_ZERO_SAMPLE_INTERVAL_MS    300u
#define BOOT_CURRENT_CADC_DATA_LENGTH              2u
#define BOOT_CURRENT_ZERO_MAX_ABS_COUNTS           40
#define BOOT_CURRENT_ZERO_MAX_DELTA_COUNTS          6
#define BOOT_CURRENT_FET_STATUS_MASK              0x07u
#define CURRENT_FIXED_SCALE                          4u
#define CURRENT_REPORT_MA_PER_LSB                  100u
#define CURRENT_DEADBAND_CALIBRATED_MA               0u
#define CURRENT_DEADBAND_FALLBACK_MA               500u

/*
 * Canonical runtime current:
 *   positive mA = charge, negative mA = discharge.
 * Boot zero is stored in raw-count x4 so two samples retain 0.5-count
 * resolution without floating point.
 */
static INT32 g_i32BootCurrentZeroRawX4 = 0;
static UINT8 g_u8BootCurrentZeroStatus = BOOT_CURRENT_ZERO_NOT_RUN;
static INT32 g_i32Current_mA = 0;

static INT32 BmsCurrent_RawToSigned(UINT16 raw)
{
	return (INT32)(INT16)raw;
}

static UINT32 BmsCurrent_AbsI32(INT32 value)
{
	return (value < 0) ? (UINT32)(-value) : (UINT32)value;
}

static UINT32 SH309_CurrentRawX4To_mAX4(UINT32 raw_abs_x4)
{
	UINT32 res_mohm = (UINT32)OtherElement.u16Sys_CS_Res;
	UINT32 res_num = (UINT32)OtherElement.u16Sys_CS_Res_Num;
	UINT32 numerator_scale;
	UINT32 denominator;
	UINT32 quotient;
	UINT32 remainder;

	if (res_mohm == 0u)
		res_mohm = (UINT32)CS_Res;
	if (res_num == 0u)
		res_num = (UINT32)CS_Res_Num;

	if ((raw_abs_x4 == 0u) || (res_mohm == 0u) || (res_num == 0u))
		return 0u;

	numerator_scale = SH309_CADC_NUMERATOR_REDUCED * res_num;
	denominator = SH309_CADC_DENOMINATOR_REDUCED * res_mohm;
	quotient = raw_abs_x4 / denominator;
	remainder = raw_abs_x4 % denominator;

	return quotient * numerator_scale
		   + (remainder * numerator_scale + (denominator / 2u)) / denominator;
}

static UINT32 BmsCurrent_mAX4To_mA(UINT32 current_mA_x4)
{
	return (current_mA_x4 + (CURRENT_FIXED_SCALE / 2u)) / CURRENT_FIXED_SCALE;
}

static UINT32 BmsCurrent_Deadband_mA(void)
{
	return (g_u8BootCurrentZeroStatus == BOOT_CURRENT_ZERO_VALID)
			   ? CURRENT_DEADBAND_CALIBRATED_MA
			   : CURRENT_DEADBAND_FALLBACK_MA;
}

static UINT16 BmsCurrent_mAX4ToReport(UINT32 current_mA_x4, UINT16 report_divisor_mA)
{
	UINT32 divisor = CURRENT_FIXED_SCALE * (UINT32)report_divisor_mA;
	UINT32 report_value = (current_mA_x4 + (divisor / 2u)) / divisor;

	if (report_value > 0xFFFFu)
		return 0xFFFFu;

	return (UINT16)report_value;
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
		if (g_u16CalibCoefK[VOLT_AFE1] != 1024 || g_i16CalibCoefB[VOLT_AFE1] != 0)
		{
			t_i32temp = ((t_i32temp * g_u16CalibCoefK[VOLT_AFE1]) >> 10) + g_i16CalibCoefB[VOLT_AFE1];
		}
		t_i32temp = ((t_i32temp * g_u16CalibCoefK[i]) >> 10) + g_i16CalibCoefB[i];
		t_i32temp = t_i32temp > 0 ? t_i32temp : 0;
		g_stCellInfoReport.u16VCell[i] = (UINT16)t_i32temp;
	}

	if (SeriesNum < 32)
	{
		for (i = SeriesNum; i < 32; ++i)
		{
			g_stCellInfoReport.u16VCell[i] = 61001;
		}
	}

	if (g_stCellInfoReport.u16Ichg > 0)
	{
		for (i = 0; i < CompensateNUM; ++i)
		{
			if (CopperLoss_Num[i] == 0)
			{
				break;
			}
			t_i32temp = (UINT32)CopperLoss[i] * g_stCellInfoReport.u16Ichg;
			g_stCellInfoReport.u16VCell[CopperLoss_Num[i] - 1] -= (UINT16)(((t_i32temp >> 14) + (t_i32temp >> 15) + (t_i32temp >> 17)) & 0xFFFF);
		}
	}
	else if (g_stCellInfoReport.u16IDischg > 0)
	{
		for (i = 0; i < CompensateNUM; ++i)
		{
			if (CopperLoss_Num[i] == 0)
			{
				break;
			}
			t_i32temp = (UINT32)CopperLoss[i] * g_stCellInfoReport.u16IDischg;
			g_stCellInfoReport.u16VCell[CopperLoss_Num[i] - 1] += (UINT16)(((t_i32temp >> 14) + (t_i32temp >> 15) + (t_i32temp >> 17)) & 0xFFFF);
		}
	}

	// DataLoad_CellVolt_Test();
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
	// u32VCellTotle = ((g_i32ADCResult[ADC_VBC]*g_u16CalibCoefK[VOLT_VBUS])>>10) + (UINT32)g_i16CalibCoefB[VOLT_VBUS]*1000;
	// AFE读总压
	// u32VCellTotle = ((g_stBq769x0_Read_AFE1.u32VBat*g_u16CalibCoefK[VOLT_VBUS])>>10) + (UINT32)g_i16CalibCoefB[VOLT_VBUS]*1000;
	// 所有单节电池电压加起来
	u32VCellTotle = ((u32VCellTotle * g_u16CalibCoefK[VOLT_VBUS]) >> 10) + (UINT32)g_i16CalibCoefB[VOLT_VBUS] * 1000;

	g_stCellInfoReport.u16VCellTotle = (UINT16)((u32VCellTotle * 1638 >> 14) & 0xFFFF); // 除以10
	g_stCellInfoReport.u16VCellMax = t_u16VcellMaxTemp;									// max cell voltage
	g_stCellInfoReport.u16VCellMin = t_u16VcellMinTemp;									// min cell voltage
	g_stCellInfoReport.u16VCellDelta = t_u16VcellMaxTemp - t_u16VcellMinTemp;			// delta cell voltage
	g_stCellInfoReport.u16VCellMaxPosition = t_u8VcellMaxPosition + 1;					// max cell voltage
	g_stCellInfoReport.u16VCellMinPosition = t_u8VcellMinPosition + 1;					// min cell voltage
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
		{			  // 这段代码什么意思，断了就不判断吗？
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

static INT32 BmsCurrent_ApplyBootZeroX4(INT32 raw_signed)
{
	INT32 corrected_raw_x4 = raw_signed * (INT32)CURRENT_FIXED_SCALE;

	if (g_u8BootCurrentZeroStatus == BOOT_CURRENT_ZERO_VALID)
		corrected_raw_x4 -= g_i32BootCurrentZeroRawX4;

	return corrected_raw_x4;
}

static void BmsCurrent_BootZeroFail(UINT8 status)
{
	g_i32BootCurrentZeroRawX4 = 0;
	g_u8BootCurrentZeroStatus = status;
}

static void SH309_CurrentWaitFreshSample(void)
{
	UINT16 remain_ms = BOOT_CURRENT_ZERO_SAMPLE_INTERVAL_MS;

	while (remain_ms > 0u)
	{
		UINT16 slice_ms = (remain_ms > 100u) ? 100u : remain_ms;
		__delay_ms(slice_ms);
		remain_ms -= slice_ms;
	}
}

static UINT8 SH309_CurrentPrepareBootZero(void)
{
	/* Keep the external CTLC path disabled for the whole learning window. */
	MCUO_AFE_CTLC = 0;

	SH367309_Reg_Store.REG_MTP_CONF.bits.CADCON = 1u;
	SH367309_Reg_Store.REG_MTP_CONF.bits.CHGMOS = 0u;
	SH367309_Reg_Store.REG_MTP_CONF.bits.DSGMOS = 0u;
	SH367309_Reg_Store.REG_MTP_CONF.bits.PCHMOS = 0u;

	if (!MTPWrite(MTP_CONF, 1u, &SH367309_Reg_Store.REG_MTP_CONF.all))
	{
		log_i("[BOOT][CUR_ZERO] config write failed\n");
		BmsCurrent_BootZeroFail(BOOT_CURRENT_ZERO_CONFIG_WRITE_ERROR);
		return 0u;
	}

	return 1u;
}

static UINT8 SH309_CurrentCheckBootZeroSafe(void)
{
	MTP_REG_CONF confirmed_conf;
	UINT8 bstatus3 = 0u;

	confirmed_conf.all = 0u;
	if (!MTPRead(MTP_CONF, 1u, &confirmed_conf.all))
	{
		log_i("[BOOT][CUR_ZERO] MTP_CONF readback failed\n");
		BmsCurrent_BootZeroFail(BOOT_CURRENT_ZERO_CONFIG_READBACK_ERROR);
		return 0u;
	}

	if (!confirmed_conf.bits.CADCON
		|| confirmed_conf.bits.CHGMOS
		|| confirmed_conf.bits.DSGMOS
		|| confirmed_conf.bits.PCHMOS)
	{
		log_i("[BOOT][CUR_ZERO] FET control is not OFF\n");
		BmsCurrent_BootZeroFail(BOOT_CURRENT_ZERO_FET_ACTIVE);
		return 0u;
	}

	if (!MTPRead(MTP_BSTATUS3, 1u, &bstatus3))
	{
		log_i("[BOOT][CUR_ZERO] BSTATUS3 read failed\n");
		BmsCurrent_BootZeroFail(BOOT_CURRENT_ZERO_SAMPLE_READ_ERROR);
		return 0u;
	}

	if ((bstatus3 & BOOT_CURRENT_FET_STATUS_MASK) != 0u)
	{
		log_i("[BOOT][CUR_ZERO] actual FET is active\n");
		BmsCurrent_BootZeroFail(BOOT_CURRENT_ZERO_FET_ACTIVE);
		return 0u;
	}

	return 1u;
}

static UINT8 SH309_CurrentReadRaw(INT32 *raw_signed)
{
	UINT8 cadc_data[BOOT_CURRENT_CADC_DATA_LENGTH];
	UINT16 raw_current;

	if (raw_signed == 0)
	{
		BmsCurrent_BootZeroFail(BOOT_CURRENT_ZERO_SAMPLE_READ_ERROR);
		return 0u;
	}

	if (!MTPRead(MTP_ADC2, BOOT_CURRENT_CADC_DATA_LENGTH, cadc_data))
	{
		log_i("[BOOT][CUR_ZERO] CADCD read failed\n");
		BmsCurrent_BootZeroFail(BOOT_CURRENT_ZERO_SAMPLE_READ_ERROR);
		return 0u;
	}

	raw_current = ((UINT16)cadc_data[0] << 8) | (UINT16)cadc_data[1];
	*raw_signed = BmsCurrent_RawToSigned(raw_current);
	return 1u;
}

UINT8 BmsCurrent_BootZeroCalibrate(void)
{
	INT32 raw1;
	INT32 raw2;
	INT32 delta;

	if (g_u8BootCurrentZeroStatus != BOOT_CURRENT_ZERO_NOT_RUN)
		return (g_u8BootCurrentZeroStatus == BOOT_CURRENT_ZERO_VALID) ? 1u : 0u;

	g_i32BootCurrentZeroRawX4 = 0;

	if (!SH309_CurrentPrepareBootZero())
		return 0u;

	SH309_CurrentWaitFreshSample();
	if (!SH309_CurrentCheckBootZeroSafe()
		|| !SH309_CurrentReadRaw(&raw1)
		|| !SH309_CurrentCheckBootZeroSafe())
		return 0u;

	SH309_CurrentWaitFreshSample();
	if (!SH309_CurrentCheckBootZeroSafe()
		|| !SH309_CurrentReadRaw(&raw2)
		|| !SH309_CurrentCheckBootZeroSafe())
		return 0u;

	if ((BmsCurrent_AbsI32(raw1) > (UINT32)BOOT_CURRENT_ZERO_MAX_ABS_COUNTS)
		|| (BmsCurrent_AbsI32(raw2) > (UINT32)BOOT_CURRENT_ZERO_MAX_ABS_COUNTS))
	{
		log_i("[BOOT][CUR_ZERO] out of range raw1=%d raw2=%d\n", raw1, raw2);
		BmsCurrent_BootZeroFail(BOOT_CURRENT_ZERO_OUT_OF_RANGE);
		return 0u;
	}

	delta = raw2 - raw1;
	if (BmsCurrent_AbsI32(delta) > (UINT32)BOOT_CURRENT_ZERO_MAX_DELTA_COUNTS)
	{
		log_i("[BOOT][CUR_ZERO] unstable raw1=%d raw2=%d delta=%d\n", raw1, raw2, delta);
		BmsCurrent_BootZeroFail(BOOT_CURRENT_ZERO_UNSTABLE);
		return 0u;
	}

	g_i32BootCurrentZeroRawX4 = (raw1 + raw2) * 2;
	g_u8BootCurrentZeroStatus = BOOT_CURRENT_ZERO_VALID;
	log_i("[BOOT][CUR_ZERO] valid raw1=%d raw2=%d zero_x4=%d\n",
		  raw1, raw2, g_i32BootCurrentZeroRawX4);
	return 1u;
}

UINT8 BmsCurrent_IsBootZeroValid(void)
{
	return (g_u8BootCurrentZeroStatus == BOOT_CURRENT_ZERO_VALID) ? 1u : 0u;
}

UINT8 BmsCurrent_GetBootZeroStatus(void)
{
	return g_u8BootCurrentZeroStatus;
}

INT32 BmsCurrent_GetBootZeroRawX4(void)
{
	return BmsCurrent_IsBootZeroValid() ? g_i32BootCurrentZeroRawX4 : 0;
}

INT32 BmsCurrent_GetCurrent_mA(void)
{
	return g_i32Current_mA;
}

void BmsCurrent_Update(void)
{
	INT32 raw_signed = BmsCurrent_RawToSigned(U16_SwapEndian(Registers_AFE1.Cadc));
	INT32 corrected_raw_x4 = BmsCurrent_ApplyBootZeroX4(raw_signed);
	UINT32 current_mA_x4 = SH309_CurrentRawX4To_mAX4(BmsCurrent_AbsI32(corrected_raw_x4));
	UINT32 deadband_mA = BmsCurrent_Deadband_mA();
	UINT32 current_mA;

	if (current_mA_x4 < (deadband_mA * CURRENT_FIXED_SCALE))
		current_mA_x4 = 0u;

	current_mA = BmsCurrent_mAX4To_mA(current_mA_x4);
	if (current_mA_x4 == 0u)
		g_i32Current_mA = 0;
	else if (corrected_raw_x4 > 0)
		g_i32Current_mA = (INT32)current_mA;
	else
		g_i32Current_mA = -(INT32)current_mA;

	log_i("AFE raw=%d corrected_x4=%d zero_x4=%d current=%d mA deadband=%u mA\n",
		  raw_signed, corrected_raw_x4, BmsCurrent_GetBootZeroRawX4(),
		  g_i32Current_mA, deadband_mA);

	g_stCellInfoReport.u16Ichg = BmsCurrent_mAX4ToReport(
		(g_i32Current_mA > 0) ? current_mA_x4 : 0u,
		CURRENT_REPORT_MA_PER_LSB);
	g_stCellInfoReport.u16IDischg = BmsCurrent_mAX4ToReport(
		(g_i32Current_mA < 0) ? current_mA_x4 : 0u,
		CURRENT_REPORT_MA_PER_LSB);

#ifdef __VIRTURE_CURRENT__
	if (sys_time.isdebugenable == 1)
	{
		g_stCellInfoReport.u16Ichg = sys_time.CHG;
		g_stCellInfoReport.u16IDischg = sys_time.DSG;
	}
#endif
}

extern uint16_t time_chg;
extern uint16_t time_dsg;
extern uint16_t time_real;
void DataLoad_Current(void)
{
	static UINT32 s_u32EtaCurrent_mA = 0u;
	static INT8 s_i8EtaDirection = 0;
	INT32 signed_current_mA;
	UINT32 current_mA;
	INT8 direction;
	UINT32 capacity_x100;
	UINT32 minutes;

	BmsCurrent_Update();
	signed_current_mA = BmsCurrent_GetCurrent_mA();

	if (signed_current_mA >= 200)
	{
		direction = 1;
		current_mA = (UINT32)signed_current_mA;
	}
	else if (signed_current_mA <= -200)
	{
		direction = -1;
		current_mA = (UINT32)(-signed_current_mA);
	}
	else
	{
		direction = 0;
		current_mA = 0u;
	}

	/*
	 * ETA uses a direction-aware EMA (alpha=1/16 at the AFE sampling cadence).
	 * Direction changes restart the filter so charge history never contaminates
	 * discharge ETA, and vice versa.
	 */
	if (direction == 0)
	{
		s_u32EtaCurrent_mA = 0u;
		s_i8EtaDirection = 0;
	}
	else if ((direction != s_i8EtaDirection) || (s_u32EtaCurrent_mA == 0u))
	{
		s_u32EtaCurrent_mA = current_mA;
		s_i8EtaDirection = direction;
	}
	else
	{
		s_u32EtaCurrent_mA =
			(s_u32EtaCurrent_mA * 15u + current_mA + 8u) / 16u;
	}

	if (!SOC_Enhance_Element.u8_SOC_Valid)
	{
		time_chg = 0xFFFFu;
		time_dsg = 0xFFFFu;
		time_real = 0xFFFFu;
		return;
	}

	if ((direction > 0) && (s_u32EtaCurrent_mA > 0u))
	{
		capacity_x100 =
			((UINT32)g_stCellInfoReport.SocElement.u16CapacityFull >=
			 (UINT32)g_stCellInfoReport.SocElement.u16CapacityNow)
			? ((UINT32)g_stCellInfoReport.SocElement.u16CapacityFull -
			   (UINT32)g_stCellInfoReport.SocElement.u16CapacityNow)
			: 0u;
		minutes = capacity_x100 * 600u / s_u32EtaCurrent_mA;
		if (minutes > 0xFFFEu)
			minutes = 0xFFFEu;
		time_real = (UINT16)minutes;
		/* Preserve the previous 93% conservative ETA factor without float math. */
		time_chg = (UINT16)(minutes * 93u / 100u);
		time_dsg = 0xFFFFu;
	}
	else if ((direction < 0) && (s_u32EtaCurrent_mA > 0u))
	{
		capacity_x100 = (UINT32)g_stCellInfoReport.SocElement.u16CapacityNow;
		minutes = capacity_x100 * 600u / s_u32EtaCurrent_mA;
		if (minutes > 0xFFFEu)
			minutes = 0xFFFEu;
		time_real = (UINT16)minutes;
		time_dsg = (UINT16)(minutes * 93u / 100u);
		time_chg = 0xFFFFu;
	}
	else
	{
		time_real = 0xFFFFu;
		if (g_stCellInfoReport.SocElement.u16Soc == 0u)
		{
			time_chg = 0xFFFFu;
			time_dsg = 0u;
		}
		else if (g_stCellInfoReport.SocElement.u16Soc == 100u)
		{
			time_chg = 0u;
			time_dsg = 0xFFFFu;
		}
		else
		{
			time_chg = 0xFFFFu;
			time_dsg = 0xFFFFu;
		}
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
		if (Result != 0)
		{
			++u8IICFaultcnt1;
			if (u8IICFaultcnt1 > 50)
			{ // 20次1s
				Init_Registers(num);
				u8IICFaultcnt1 = 0;
				System_ERROR_UserCallback(ERROR_AFE1); // 这里调用便可
			}
			if (u8IICFaultcnt1 == 30 && u8WakeCnt1 <= 20)
			{
				InitAFE1();
				++u8WakeCnt1;
			}
			SystemStatus.bits.b1Status_AFE1 = 0;
		}
		else
		{
			if (u8IICFaultcnt1 > 0)
			{
				u8IICFaultcnt1--;
			}
			if (u8WakeCnt1 > 0)
			{
				u8WakeCnt1--;
			}
			SystemStatus.bits.b1Status_AFE1 = 1;
			System_ERROR_UserCallback(ERROR_REMOVE_AFE1);
		}
		break;

	case 1:
		if (Result != 0)
		{
			++u8IICFaultcnt2;
			if (u8IICFaultcnt2 > 50)
			{
				Init_Registers(num);
				u8IICFaultcnt2 = 0;
				System_ERROR_UserCallback(ERROR_AFE2); // 这里调用便可
			}
			if (u8IICFaultcnt2 == 30 && u8WakeCnt2 <= 20)
			{
				SH367309_Enable_AFE_Wdt_Cadc_Drivers();
				++u8WakeCnt2;
			}
			SystemStatus.bits.b1Status_AFE2 = 0;
		}
		else
		{
			if (u8IICFaultcnt2 > 0)
			{
				u8IICFaultcnt2--;
			}
			if (u8WakeCnt2 > 0)
			{
				u8WakeCnt2--;
			}
			SystemStatus.bits.b1Status_AFE2 = 1;
			// System_ERROR_UserCallback(ERROR_REMOVE_AFE2);
		}
		break;
	default:
		break;
	}

	if (System_ERROR_UserCallback(ERROR_STATUS_AFE1))
	{
		if (++su16_Sleep_DelayT1 >= 5 * 60)
		{ // 等待5min后进入休眠
			su16_Sleep_DelayT1 = 0;
			entersleep(NORMAL_MODE);
		}
	}
	else
	{
		su16_Sleep_DelayT1 = 0;
	}

	if (System_ERROR_UserCallback(ERROR_STATUS_AFE2))
	{
		if (++su16_Sleep_DelayT2 >= 5 * 60)
		{ // 等待5min后进入休眠
			su16_Sleep_DelayT2 = 0;
			entersleep(NORMAL_MODE);
		}
	}
	else
	{
		su16_Sleep_DelayT2 = 0;
	}

	// 暂时寄存这里
	if (System_ERROR_UserCallback(ERROR_STATUS_EEPROM_COM) || System_ERROR_UserCallback(ERROR_STATUS_EEPROM_STORE))
	{
		if (++su16_Sleep_DelayT3 >= 5 * 60)
		{ // 等待5min后进入休眠
			su16_Sleep_DelayT3 = 0;
			entersleep(NORMAL_MODE);
		}
	}
	else
	{
		su16_Sleep_DelayT3 = 0;
	}
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
// 030单片机的8M主频只能改为200ms，不然时基出问题。72M可以用50ms。

extern UINT8 gu8_200msAccClock_Flag2;
// void App_AFEGet(void)
// {
// 	if (0 == g_st_SysTimeFlag.bits.b1Sys200msFlag3 || 1 == gu8_TxEnable_SCI1 || 1 == gu8_TxEnable_SCI2 || 1 == gu8_TxEnable_SCI3)
// 	// if (0 == gu8_200msAccClock_Flag2 || 1 == gu8_TxEnable_SCI1 || 1 == gu8_TxEnable_SCI2 || 1 == gu8_TxEnable_SCI3)
// 	// if (0 == gu8_200msAccClock_Flag2 || 1 == gu8_TxEnable_SCI1 || 1 == gu8_TxEnable_SCI2)
// 	// if (0 == gu8_200msAccClock_Flag2)
// 	{
// 		return;
// 	}
// 	if (u32E2P_Pro_VolCur_WriteFlag != 0 || u32E2P_Pro_Temp_WriteFlag != 0 || u32E2P_Pro_Other_WriteFlag != 0 || u32E2P_OtherElement1_WriteFlag != 0 || u32E2P_RTC_Element_WriteFlag != 0 || u8E2P_SocTable_WriteFlag != 0 || u8E2P_CopperLoss_WriteFlag != 0 || u8E2P_KB_WriteFlag != 0)
// 	{
// 		return;
// 	}

// 	MonitorAFE(0, UpdateVoltageFromBqMaximo());

// 	DataLoad_CellVolt();
// 	DataLoad_CellVoltMaxMinFind();
// 	DataLoad_Temperature();
// 	DataLoad_TemperatureMaxMinFind();
// 	DataLoad_Current();
// 	// test_Autocurrent_cycle();
// 	App_SH367309();
// 	App_MOS_Relay_Ctrl();

// 	gu8_200msAccClock_Flag2 = 0;
// }

void App_AFEGet(void)
{
	if (0 == g_st_SysTimeFlag.bits.b1Sys200msFlag3 || 1 == gu8_TxEnable_SCI1 || 1 == gu8_TxEnable_SCI2 || 1 == gu8_TxEnable_SCI3)
	{
		return;
	}

	if (u32E2P_Pro_VolCur_WriteFlag != 0 || u32E2P_Pro_Temp_WriteFlag != 0 || u32E2P_Pro_Other_WriteFlag != 0 || u32E2P_OtherElement1_WriteFlag != 0 || u32E2P_RTC_Element_WriteFlag != 0 || u8E2P_SocTable_WriteFlag != 0 || u8E2P_CopperLoss_WriteFlag != 0 || u8E2P_KB_WriteFlag != 0)
	{
		return;
	}

	MonitorAFE(0, UpdateVoltageFromBqMaximo());

	DataLoad_CellVolt();
	// DataLoad_CellVolt_Test();
	DataLoad_CellVoltMaxMinFind();
	DataLoad_Temperature();
	DataLoad_TemperatureMaxMinFind();
	DataLoad_Current();

	App_SH367309();
	App_MOS_Relay_Ctrl();
}
