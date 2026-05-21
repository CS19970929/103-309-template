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

UINT32 u32_ChgCur_mA = 0;
UINT32 u32_DsgCur_mA = 0;

#define AFE_UPDATE_PERIOD_MS ((UINT16)200u)
#define AFE_UPDATE_STALE_FAIL_CNT ((UINT16)(5u * 1000u / AFE_UPDATE_PERIOD_MS))
#define AFE_UPDATE_FORCE_SLEEP_FAIL_CNT ((UINT16)(60u * 1000u / AFE_UPDATE_PERIOD_MS))
#define AFE_LOAD_REMOVE_CRLD_DISABLE ((UINT8)0u)
#define AFE_LOAD_REMOVE_CRLD_ENABLE ((UINT8)2u)
#define AFE_LOAD_REMOVE_PROTECT_MASK ((UINT16)(AFE_FLAG_OCD1 | AFE_FLAG_OCD2 | AFE_FLAG_SC))
#define AFE_LOAD_REMOVE_RLD_500UA_MASK ((UINT8)0x40u)
#define AFE_LOAD_REMOVE_DELAY_MS ((UINT32)(5u * 60u * 1000u))
#define AFE_LOAD_REMOVE_DELAY_CNT ((UINT16)(AFE_LOAD_REMOVE_DELAY_MS / AFE_UPDATE_PERIOD_MS))
#define AFE_CADC_CURRENT_DENOMINATOR ((UINT32)29127u)

UINT8 AFE3520_NormalizeSeriesNum(UINT16 series)
{
	if ((series >= AFE3520_CELL_SERIES_MIN) && (series <= AFE3520_CELL_SERIES_MAX))
	{
		return (UINT8)series;
	}

#if (SNum >= 4) && (SNum <= 20)
	return (UINT8)SNum;
#else
	return AFE3520_CELL_SERIES_MIN;
#endif
}

void AFE3520_SyncSeriesNum(UINT16 series)
{
	SeriesNum = AFE3520_NormalizeSeriesNum(series);
	OtherElement.u16Sys_SeriesNum = SeriesNum;
}

UINT32 AFE3520_UpdateSenseResScaleSafe(void)
{
	UINT16 cs_res = OtherElement.u16Sys_CS_Res;
	UINT16 cs_res_num = OtherElement.u16Sys_CS_Res_Num;

	if (cs_res == 0u)
	{
		cs_res = CS_Res;
		OtherElement.u16Sys_CS_Res = cs_res;
	}
	if (cs_res_num == 0u)
	{
		cs_res_num = CS_Res_Num;
		OtherElement.u16Sys_CS_Res_Num = cs_res_num;
	}

	g_u32CS_Res_AFE = ((UINT32)cs_res_num * 1000u) / cs_res;
	if (g_u32CS_Res_AFE == 0u)
	{
		OtherElement.u16Sys_CS_Res = CS_Res;
		OtherElement.u16Sys_CS_Res_Num = CS_Res_Num;
		g_u32CS_Res_AFE = ((UINT32)CS_Res_Num * 1000u) / CS_Res;
	}

	return g_u32CS_Res_AFE;
}

static UINT32 AFE3520_CadcRawToCurrentMa(UINT16 raw)
{
	return (UINT32)(((uint64_t)raw * 100u * (uint64_t)g_u32CS_Res_AFE) / AFE_CADC_CURRENT_DENOMINATOR);
}

// uint8_t Sh_GetCadcCurrent(uint32_t *current)
uint8_t Sh_GetCadcCurrent(void)
{
#define RSENSE (0.00025)
	uint8_t ret = 0;
	// uint8_t cadc[2];
	uint16_t tempvalue;

	// SH_iicReadRam(0x6e,2,cadc);
	// ShRamRegs.cadcdh = cadc[0];
	// ShRamRegs.cadcdl = cadc[1];
	//  tempvalue = (uint16_t)(ShRamRegs.cadcdh << 8) + ShRamRegs.cadcdl;
	tempvalue = (uint16_t)SH367309_Read_AFE1.u16Current;

	if ((tempvalue & 0x8000) == 0x8000)
	{
		tempvalue = 0x10000 - tempvalue;
		//*current = (uint16_t)((float)(tempvalue + CurrOffset) * 200 / 21470.0 / RSENSE);
		// *current = (uint32_t)((float)(tempvalue) * 200 / 21470.0 / RSENSE);
		g_stCellInfoReport.u16IDischg = (uint32_t)((float)(tempvalue) * 100 / 29127.0 / RSENSE);
		g_stCellInfoReport.u16Ichg = 0;
	}
	else
	{
		//*current = (uint16_t)((float)(tempvalue - CurrOffset) * 200 / 21470.0 / RSENSE);
		// *current = (uint32_t)((float)(tempvalue) * 200 / 21470.0 / RSENSE);
		g_stCellInfoReport.u16Ichg = (uint32_t)((float)(tempvalue) * 100 / 29127.0 / RSENSE);
		g_stCellInfoReport.u16IDischg = 0;
	}
	// 校准
	//  *current = *current * (float)CurrK/1000 + (float)CurrO/1000;

	// if ((ShRamRegs.cadcdh & 0x80) == 0x80)
	// {
	// 	ret = 1;
	// }
	return ret;
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

// 这里排列好就行，不需要电池位号映射表。>61000为不用
// 经过验算，AFE1校准一次，然后本身再校准一次叠加是可以的。不需要确定某一个KB值的做法。
// 假设先确定用AFE1还是本身的KB的话，会出现问题。如下：
// 假设需要整体校准，行，AFE1先行，然后发现某几串出问题，继续使用本身KB值，然后本身KB值需要同步前面AFE1的KB值一起算才行
// 如果又变成单独使用本身KB值校准，出现错误。
void DataLoad_CellVolt(void)
{
	UINT8 i;
	UINT8 cell_count;
	INT32 t_i32temp;

	cell_count = AFE3520_NormalizeSeriesNum(SeriesNum);
	for (i = 0; i < cell_count; ++i)
	{
		// t_i32temp = (UINT32)SH367309_Read_AFE1.u16VCell[SeriesSelect_AFE1[SeriesNum - 1][i]];
		t_i32temp = (UINT32)SH367309_Read_AFE1.u16VCell[i];
		if (g_u16CalibCoefK[VOLT_AFE1] != 1024 || g_i16CalibCoefB[VOLT_AFE1] != 0)
		{
			t_i32temp = ((t_i32temp * g_u16CalibCoefK[VOLT_AFE1]) >> 10) + g_i16CalibCoefB[VOLT_AFE1];
		}
		t_i32temp = ((t_i32temp * g_u16CalibCoefK[i]) >> 10) + g_i16CalibCoefB[i];
		t_i32temp = t_i32temp > 0 ? t_i32temp : 0;
		g_stCellInfoReport.u16VCell[i] = (UINT16)t_i32temp;
	}

	if (cell_count < 32)
	{
		for (i = cell_count; i < 32; ++i)
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
	UINT8 cell_count;
	UINT32 u32VCellTotle;

	t_u16VcellMaxTemp = 0;
	t_u16VcellMinTemp = 0x7FFF;
	t_u8VcellMaxPosition = 0;
	t_u8VcellMinPosition = 0;
	u32VCellTotle = 0;
	cell_count = AFE3520_NormalizeSeriesNum(SeriesNum);

	for (i = 0; i < cell_count; i++)
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
	// Select = 5;
	// 没纳入统计的，默认值就是0了
	for (i = 0; i < Select; i++)
	{
		t_i32temp = (INT32)SH367309_Read_AFE1.u16TempBat[i] / 10 - 40;
		t_i32temp = ((t_i32temp * g_u16CalibCoefK[MDL_TEMP1 + i]) + g_i16CalibCoefB[MDL_TEMP1 + i]) >> 10;
		g_stCellInfoReport.u16Temperature[i] = (UINT16)(t_i32temp * 10 + 400);
		Monitor_TempBreak(&g_stCellInfoReport.u16Temperature[i]);
	}
	// g_stCellInfoReport.u16Temperature[0] = (25 + 40) * 10;

#if 0
	//环境温度1
	t_i32temp = g_i32ADCResult[ADC_TEMP_EV1] / 10 - 40;		//放大1000倍和B值对应的意思
	//t_i32temp =  - 40;
	t_i32temp = ((t_i32temp * g_u16CalibCoefK[MDL_TEMP_ENV1]) + g_i16CalibCoefB[MDL_TEMP_ENV1])>>10;
	g_stCellInfoReport.u16Temperature[ENV_TEMP1] = (UINT16)(t_i32temp*10 + 400);
	Monitor_TempBreak(&g_stCellInfoReport.u16Temperature[ENV_TEMP1]);
#endif

#if 1
	// MOS温度为散热片温度
	// 取两者最大值
	// t_i32temp = (g_i32ADCResult[ADC_TEMP_MOS1] > g_i32ADCResult[ADC_TEMP_MOS2] ? g_i32ADCResult[ADC_TEMP_MOS1]:g_i32ADCResult[ADC_TEMP_MOS2]);
	t_i32temp = (INT32)SH367309_Read_AFE1.u16TempBat[3];
	t_i32temp = t_i32temp / 10 - 40;
	t_i32temp = ((t_i32temp * g_u16CalibCoefK[MDL_TEMP_MOS1]) + g_i16CalibCoefB[MDL_TEMP_MOS1]) >> 10;
	g_stCellInfoReport.u16Temperature[MOS_TEMP1] = (UINT16)(t_i32temp * 10 + 400);
	Monitor_TempBreak(&g_stCellInfoReport.u16Temperature[MOS_TEMP1]);

	// t_i32temp = (INT32)SH367309_Read_AFE1.u16TempBat[4];
	// t_i32temp = t_i32temp / 10 - 40;
	// t_i32temp = ((t_i32temp * g_u16CalibCoefK[MDL_TEMP_ENV1]) + g_i16CalibCoefB[MDL_TEMP_ENV1]) >> 10;
	// g_stCellInfoReport.u16Temperature[ENV_TEMP1] = (UINT16)(t_i32temp * 10 + 400);
	// Monitor_TempBreak(&g_stCellInfoReport.u16Temperature[ENV_TEMP1]);
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

extern uint16_t time_chg;
extern uint16_t time_dsg;
extern uint16_t time_real;

static UINT16 s_u16AfeUpdateFailCnt = 0;
static UINT8 s_u8AfeLastUpdateErr = AFE_UPDATE_OK;

static void AFE_ClearStaleRuntimeData(void)
{
	g_stCellInfoReport.u16Ichg = 0;
	g_stCellInfoReport.u16IDischg = 0;
	u32_ChgCur_mA = 0;
	u32_DsgCur_mA = 0;
	time_chg = 0xFFFF;
	time_dsg = 0xFFFF;
	time_real = 0xFFFF;
	g_stCellInfoReport.balance_status = 0;
	g_stCellInfoReport.u16BalanceFlag1 = 0;
	g_stCellInfoReport.u16BalanceFlag2 = 0;
}

static void AFE_UpdateFailDeal(UINT8 result)
{
	if (s_u16AfeUpdateFailCnt < AFE_UPDATE_FORCE_SLEEP_FAIL_CNT)
	{
		s_u16AfeUpdateFailCnt++;
	}

	if (s_u8AfeLastUpdateErr != result)
	{
		s_u8AfeLastUpdateErr = result;
	}

	sys_time.crc_err = true;
	if (s_u16AfeUpdateFailCnt >= AFE_UPDATE_STALE_FAIL_CNT)
	{
		AFE_ClearStaleRuntimeData();
	}

	if (s_u16AfeUpdateFailCnt >= AFE_UPDATE_FORCE_SLEEP_FAIL_CNT)
	{
		s_u16AfeUpdateFailCnt = AFE_UPDATE_FORCE_SLEEP_FAIL_CNT;
		entersleep(DEEP_MODE);
	}
}

static void AFE_UpdateOkDeal(void)
{
	s_u16AfeUpdateFailCnt = 0;
	s_u8AfeLastUpdateErr = AFE_UPDATE_OK;
}

void DataLoad_Current(void)
{
	// if ((SH367309_Read_AFE1.u16Current & 0x1000) == 0)
	if ((SH367309_Read_AFE1.u16Current & 0x8000) == 0)
	{
		// u32_ChgCur_mA = (UINT32)SH367309_Read_AFE1.u16Current * 1000 * g_u32CS_Res_AFE / gu32_CurCoefficient; // 默认使用200mV的计算方式
		u32_ChgCur_mA = AFE3520_CadcRawToCurrentMa(SH367309_Read_AFE1.u16Current);
		// t_i32temp = (UINT32)(0xFFFF - SH367309_Read_AFE1.u16Current + 1) * g_u32CS_Res_AFE / (21470) * 200; // mA

		log_i("******************************************\n");
		log_i("AFE value->%d\n", u32_ChgCur_mA);

		u32_DsgCur_mA = 0;
	}
	else
	{
		// u32_DsgCur_mA = (UINT32)(0xFFFF - (SH367309_Read_AFE1.u16Current | 0xE000) + 1) * 1000 * g_u32CS_Res_AFE / gu32_CurCoefficient; // mA
		// u32_DsgCur_mA = (UINT32)(0xFFFF - SH367309_Read_AFE1.u16Current + 1) * 200 * g_u32CS_Res_AFE / (21470); // mA
		u32_DsgCur_mA = AFE3520_CadcRawToCurrentMa((UINT16)(0xFFFF - SH367309_Read_AFE1.u16Current + 1)); // mA

		log_i("******************************************\n");
		log_i("AFE value->%d\n", u32_DsgCur_mA);

		u32_ChgCur_mA = 0;
	}

	// DataLoad_CurrentCali();

	if (u32_DsgCur_mA > 2000)
	{
		u32_DsgCur_mA = ((u32_DsgCur_mA * g_u16CalibCoefK[MDL_IDSG])) + (INT32)g_i16CalibCoefB[MDL_IDSG] * 1000; // B值是基于A为单位计算出来的
	}
	else
	{
		u32_DsgCur_mA = ((u32_DsgCur_mA * 1024));
	}

	if (u32_ChgCur_mA > 2000)
	{
		u32_ChgCur_mA = ((u32_ChgCur_mA * g_u16CalibCoefK[MDL_ICHG])) + (INT32)g_i16CalibCoefB[MDL_ICHG] * 1000;
	}
	else
	{
		u32_ChgCur_mA = ((u32_ChgCur_mA * 1024));
	}

	// 改为INT32
	u32_ChgCur_mA = u32_ChgCur_mA > 0 ? u32_ChgCur_mA : 0;
	u32_DsgCur_mA = u32_DsgCur_mA > 0 ? u32_DsgCur_mA : 0;

	g_stCellInfoReport.u16Ichg = (UINT16)((u32_ChgCur_mA >> 10) / 100);
	g_stCellInfoReport.u16IDischg = (UINT16)((u32_DsgCur_mA >> 10) / 100);

	if (g_stCellInfoReport.u16Ichg <= 2)
	{
		g_stCellInfoReport.u16Ichg = 0;
	}
	if (g_stCellInfoReport.u16IDischg <= 2)
	{
		g_stCellInfoReport.u16IDischg = 0;
	}

#ifdef __VIRTURE_CURRENT__
	if (sys_time.isdebugenable == 1)
	{
		g_stCellInfoReport.u16Ichg = sys_time.CHG;
		g_stCellInfoReport.u16IDischg = sys_time.DSG;
	}
#endif

#if 0
	if (g_stCellInfoReport.u16Ichg)
	{
		time_dsg = 0xffff;
		// todo 1、满充、满放容量校准 2、soh与学习满充容量
		//  time_chg = ((uint32_t)g_stCellInfoReport.SocElement.u16CapacityFactory - (uint32_t)g_stCellInfoReport.SocElement.u16CapacityNow) * 6 / g_stCellInfoReport.u16Ichg;
		time_chg = ((uint32_t)g_stCellInfoReport.SocElement.u16CapacityFull - (uint32_t)g_stCellInfoReport.SocElement.u16CapacityNow) * 6 / (g_stCellInfoReport.u16Ichg * CURRENT_K_CHG);
		time_real = ((uint32_t)g_stCellInfoReport.SocElement.u16CapacityFull - (uint32_t)g_stCellInfoReport.SocElement.u16CapacityNow) * 6 / (g_stCellInfoReport.u16Ichg);
	}
	else if (g_stCellInfoReport.u16IDischg)
	{
		float time;
		time_chg = 0xffff;
		time = (float)g_stCellInfoReport.SocElement.u16CapacityNow * 6 / (g_stCellInfoReport.u16IDischg * CURRENT_K_DSG);
		time_dsg = (uint16_t)time;
		time_real = (float)g_stCellInfoReport.SocElement.u16CapacityNow * 6 / (g_stCellInfoReport.u16IDischg);
	}
	else
	{
		if (g_stCellInfoReport.SocElement.u16Soc == 0)
		{
			time_chg = 0xffff;
			time_dsg = 0;
		}
		else if ((g_stCellInfoReport.SocElement.u16Soc == 100))
		{
			time_chg = 0;
			time_dsg = 0xffff;
		}
		else
		{
			time_chg = 0xffff;
			time_dsg = 0xffff;
		}
	}
#endif
}
void MonitorAFE(UINT8 num, UINT8 Result)
{
#if 0
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
#endif
}

// 030单片机的8M主频只能改为200ms，不然时基出问题。72M可以用50ms。
static AFE_ProtectType AFE_GetLoadRemoveProtectFlags(void)
{
	UINT16 clear_flags = 0;

	if (Registers_AFE1.flag1.bits.ocd1_flg)
	{
		clear_flags |= (UINT16)AFE_FLAG_OCD1;
	}
	if (Registers_AFE1.flag1.bits.ocd2_flg)
	{
		clear_flags |= (UINT16)AFE_FLAG_OCD2;
	}
	if (Registers_AFE1.flag1.bits.sc_flg)
	{
		clear_flags |= (UINT16)AFE_FLAG_SC;
	}

	return (AFE_ProtectType)clear_flags;
}

static UINT8 AFE_LoadRemoveSetCrld(UINT8 crld_en)
{
	if (crld_en == AFE_LOAD_REMOVE_CRLD_ENABLE)
	{
		Registers_AFE1.sonf7 &= (UINT8)(~AFE_LOAD_REMOVE_RLD_500UA_MASK);
		if (!sh36735_write_reg_u8(AFE_SCONF7, Registers_AFE1.sonf7))
		{
			System_ERROR_UserCallback(ERROR_SPI);
			return 0;
		}
	}

	Registers_AFE1.sonf3.bits.CRLD_EN = crld_en;
	if (!sh36735_write_reg_u8(AFE_SCONF3, Registers_AFE1.sonf3.all))
	{
		System_ERROR_UserCallback(ERROR_SPI);
		return 0;
	}
	if (!sh36735_read_regs(AFE_SCONF3, (uint8_t *)&Registers_AFE1.sonf3.all, 1))
	{
		System_ERROR_UserCallback(ERROR_SPI);
		return 0;
	}

	return (Registers_AFE1.sonf3.bits.CRLD_EN == crld_en) ? 1u : 0u;
}

static UINT8 AFE_LoadRemoveRefreshStatus(void)
{
	if (!sh36735_read_regs(AFE_BSTATUS2, (uint8_t *)&Registers_AFE1.bstatus2.all, 1))
	{
		System_ERROR_UserCallback(ERROR_SPI);
		return 0;
	}

	return 1;
}

static UINT8 AFE_IsLoadRemoved(void)
{
	if ((Registers_AFE1.bstatus2.bits.LOADOFF != 0u) && (Registers_AFE1.bstatus2.bits.LOADON == 0u))
	{
		return 1;
	}

	return 0;
}

static UINT8 AFE_ClearLoadRemoveProtectFlags(UINT16 clear_flags)
{
	UINT8 result = 1u;

	if ((clear_flags & (UINT16)AFE_FLAG_OCD1) != 0u)
	{
		result = (SH_AFE_ClearProtectFlag(AFE_FLAG_OCD1) && result) ? 1u : 0u;
	}
	if ((clear_flags & (UINT16)AFE_FLAG_OCD2) != 0u)
	{
		result = (SH_AFE_ClearProtectFlag(AFE_FLAG_OCD2) && result) ? 1u : 0u;
	}
	if ((clear_flags & (UINT16)AFE_FLAG_SC) != 0u)
	{
		result = (SH_AFE_ClearProtectFlag(AFE_FLAG_SC) && result) ? 1u : 0u;
	}
	SH_AFE_ClearProtectFlag(AFE_FLAG_OCD1);
	SH_AFE_ClearProtectFlag(AFE_FLAG_OCD2);
	SH_AFE_ClearProtectFlag(AFE_FLAG_SC);
	return result;
}

UINT8 func_LoadRemove(AFE_ProtectType clear_AFE_Protect_type)
{
	static UINT8 state = 0;
	static UINT16 load_removed_delay_cnt = 0;
	static AFE_ProtectType active_protect = (AFE_ProtectType)0;
	UINT16 clear_flags = ((UINT16)clear_AFE_Protect_type & AFE_LOAD_REMOVE_PROTECT_MASK);

	if (0u == clear_flags)
	{
		if ((state != 0u) || ((UINT16)active_protect != 0u))
		{
			(void)AFE_LoadRemoveSetCrld(AFE_LOAD_REMOVE_CRLD_DISABLE);
		}
		state = 0;
		load_removed_delay_cnt = 0;
		active_protect = (AFE_ProtectType)0;
		return 0;
	}

	if ((UINT16)active_protect != clear_flags)
	{
		state = 0;
		load_removed_delay_cnt = 0;
		active_protect = (AFE_ProtectType)clear_flags;
	}

	switch (state)
	{
	case 0:
		if (AFE_LoadRemoveSetCrld(AFE_LOAD_REMOVE_CRLD_ENABLE))
		{
			state = 1;
		}
		break;

	case 1:
		if (!AFE_LoadRemoveRefreshStatus())
		{
			load_removed_delay_cnt = 0;
			break;
		}

		if (AFE_IsLoadRemoved())
		{
			if (load_removed_delay_cnt < AFE_LOAD_REMOVE_DELAY_CNT)
			{
				load_removed_delay_cnt++;
			}
			if (load_removed_delay_cnt >= AFE_LOAD_REMOVE_DELAY_CNT)
			{
				if (AFE_ClearLoadRemoveProtectFlags(clear_flags))
				{
					(void)AFE_LoadRemoveSetCrld(AFE_LOAD_REMOVE_CRLD_DISABLE);
					state = 0;
					load_removed_delay_cnt = 0;
					active_protect = (AFE_ProtectType)0;
					return 1;
				}
			}
		}
		else
		{
			load_removed_delay_cnt = 0;
		}
		break;

	default:
		state = 0;
		load_removed_delay_cnt = 0;
		active_protect = (AFE_ProtectType)0;
		break;
	}

	return 0;
}
extern UINT8 gu8_200msAccClock_Flag2;
extern void test_read_afe_param(void);
void App_AFEGet(void)
{
	static uint8_t cov1_flag = 0;
	static uint8_t cuv1_flag = 0;
	static uint8_t otc1_flag = 0;
	static uint8_t utc1_flag = 0;
	static uint8_t otd1_flag = 0;
	static uint8_t utd1_flag = 0;
	static uint8_t occ1_flag = 0;
	static uint8_t ocd1_flag = 0;

	UINT8 afe_result;
	AFE_ProtectType load_remove_flags;

	if (0 == g_st_SysTimeFlag.bits.b1Sys200msFlag3 || 1 == gu8_TxEnable_SCI1 || 1 == gu8_TxEnable_SCI2 || 1 == gu8_TxEnable_SCI3)
	{
		return;
	}

	if (u32E2P_Pro_VolCur_WriteFlag != 0 || u32E2P_Pro_Temp_WriteFlag != 0 || u32E2P_Pro_Other_WriteFlag != 0 || u32E2P_OtherElement1_WriteFlag != 0 || u32E2P_RTC_Element_WriteFlag != 0 || u8E2P_SocTable_WriteFlag != 0 || u8E2P_CopperLoss_WriteFlag != 0 || u8E2P_KB_WriteFlag != 0)
	{
		return;
	}

	// MonitorAFE(0, UpdateVoltageFromBqMaximo());
	afe_result = UpdateVoltageFromBqMaximo();
	if (afe_result != AFE_UPDATE_OK)
	{
		AFE_UpdateFailDeal(afe_result);
		return;
	}
	AFE_UpdateOkDeal();

	DataLoad_CellVolt();
	DataLoad_CellVoltMaxMinFind();
	// Sh_GetCadcCurrent();
	DataLoad_Current();
	DataLoad_Temperature();
	DataLoad_TemperatureMaxMinFind();

	load_remove_flags = AFE_GetLoadRemoveProtectFlags();
	if (((UINT16)load_remove_flags & (UINT16)AFE_FLAG_SC) != 0u)
	{
		System_ErrFlag.u8ErrFlag_CBC_DSG = 1;
	}
	else
	{
		System_ErrFlag.u8ErrFlag_CBC_DSG = 0;
	}
	if (((UINT16)load_remove_flags & AFE_LOAD_REMOVE_PROTECT_MASK) == 0u)
	{
		(void)func_LoadRemove((AFE_ProtectType)0);
	}

	if (is_AFE_COV && g_stCellInfoReport.u16VCellMax < PRT_E2ROMParas.u16VcellOvp_Rcv)
		SH_AFE_ClearProtectFlag(AFE_FLAG_OV);
	else if (is_AFE_CUV && g_stCellInfoReport.u16VCellMin > PRT_E2ROMParas.u16VcellUvp_Rcv)
		SH_AFE_ClearProtectFlag(AFE_FLAG_UV);
	else if (is_AFE_OCC)
	{
		if (!is_charger_online())
			SH_AFE_ClearProtectFlag(AFE_FLAG_OCC);

		// todo c+电压采集
#if 0
		static uint8_t state = 0;
		switch (state)
		{
		case 0:
			// Registers_AFE1.sonf3.bits.CRLD_EN = 2;
			write = Registers_AFE1.sonf3.all | 0x08;
			sh36735_write_reg_u8(AFE_SCONF3, write);
			sh36735_read_regs(AFE_SCONF3, (uint8_t *)&Registers_AFE1.sonf3.all, 1);
			if (Registers_AFE1.sonf3.bits.CRLD_EN = 2)
				state = 1;
			break;
		case 1:
			if (Registers_AFE1.bstatus2.bits.LOADOFF)
			{
				SH_AFE_ClearProtectFlag(AFE_FLAG_SC);
				state = 0;

				Registers_AFE1.sonf3.bits.CRLD_EN = 0;
				sh36735_write_reg_u8(AFE_SCONF3, Registers_AFE1.sonf3.all);
				sh36735_read_regs(AFE_SCONF3, (uint8_t *)&Registers_AFE1.sonf3.all, 1);
				if (Registers_AFE1.sonf3.bits.CRLD_EN = 0)
					state = 0;
			}
			break;
		default:

			break;
		}
#endif
	}
	else if (is_AFE_ODC)
	{
		(void)func_LoadRemove(load_remove_flags);
	}
	else if (IS_AFE_SC)
	{
		(void)func_LoadRemove(load_remove_flags);
	}
	else if (is_AFE_OTC && g_stCellInfoReport.u16TempMax < PRT_E2ROMParas.u16TChgOTp_Rcv)
		SH_AFE_ClearProtectFlag(AFE_FLAG_OTC);
	else if (is_AFE_UTC && g_stCellInfoReport.u16TempMin > PRT_E2ROMParas.u16TchgUTp_Rcv)
		SH_AFE_ClearProtectFlag(AFE_FLAG_UTC);
	else if (is_AFE_OTD && g_stCellInfoReport.u16TempMax < PRT_E2ROMParas.u16TdischgOTp_Rcv)
		SH_AFE_ClearProtectFlag(AFE_FLAG_OTD);
	else if (is_AFE_UTD && g_stCellInfoReport.u16TempMin > PRT_E2ROMParas.u16TdischgUTp_Rcv)
		SH_AFE_ClearProtectFlag(AFE_FLAG_UTD);

	if (is_AFE_COV || is_AFE_CUV || is_AFE_OCC || is_AFE_ODC || is_AFE_OTC || is_AFE_UTC || is_AFE_OTD || is_AFE_UTD || IS_AFE_SC)
	{
		fault_report(&cov1_flag, is_AFE_COV, CellOvp_Third);
		fault_report(&cuv1_flag, is_AFE_CUV, CellUvp_Third);
		fault_report(&otc1_flag, is_AFE_OTC, CellChgOTp_Third);
		fault_report(&utc1_flag, is_AFE_UTC, CellChgUTp_Third);
		fault_report(&otd1_flag, is_AFE_OTD, CellDsgOTp_Third);
		fault_report(&utd1_flag, is_AFE_UTD, CellDsgUTp_Third);
		fault_report(&occ1_flag, is_AFE_OCC, IchgOcp_Third);
		fault_report(&ocd1_flag, is_AFE_ODC, IdischgOcp_Third);
	}

	SH_AFE_GetProtectStatus();

	App_MOS_Relay_Ctrl();
	// test_read_afe_param();
}
