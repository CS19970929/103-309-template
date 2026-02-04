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

// uint8_t Sh_GetCadcCurrent(uint32_t *current)
uint8_t Sh_GetCadcCurrent(void)
{
	#define RSENSE   (0.00025)
	uint8_t ret = 0;
	//uint8_t cadc[2];
	uint16_t tempvalue;

	//SH_iicReadRam(0x6e,2,cadc);
	//ShRamRegs.cadcdh = cadc[0];
	//ShRamRegs.cadcdl = cadc[1];
	// tempvalue = (uint16_t)(ShRamRegs.cadcdh << 8) + ShRamRegs.cadcdl;
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
	//校准
	// *current = *current * (float)CurrK/1000 + (float)CurrO/1000;

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

	// Select = 2;
	Select = 5;
	// 没纳入统计的，默认值就是0了
	for (i = 0; i < Select; i++)
	{
		t_i32temp = (INT32)SH367309_Read_AFE1.u16TempBat[i] / 10 - 40;
		t_i32temp = ((t_i32temp * g_u16CalibCoefK[MDL_TEMP1 + i]) + g_i16CalibCoefB[MDL_TEMP1 + i]) >> 10;
		g_stCellInfoReport.u16Temperature[i] = (UINT16)(t_i32temp * 10 + 400);
		Monitor_TempBreak(&g_stCellInfoReport.u16Temperature[i]);
	}

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

void DataLoad_CurrentCali(void)
{
	static UINT8 su8_StartUpFlag = 4;

	// todo 预留上位机校准接口，以防万一
	// if (sci_cali_falg)
	// 	DataLoad_CurrentCali_startup();

	if (OffsetValue_CHG)
	{
		su8_StartUpFlag = 4;
	}
	else
	{
		su8_StartUpFlag = 5;
	}

	switch (su8_StartUpFlag)
	{
	// 充电偏置
	case 4:
		if (u32_ChgCur_mA > OffsetValue_CHG)
		{
			u32_ChgCur_mA = u32_ChgCur_mA - OffsetValue_CHG;
		}
		else
		{
			// u32_ChgCur_mA = 0;	//不能先置0啊，不然错了
			u32_DsgCur_mA = u32_DsgCur_mA + OffsetValue_CHG - u32_ChgCur_mA;
			u32_ChgCur_mA = 0;
		}
		break;
	case 5:

		if (u32_DsgCur_mA > OffsetValue_DSG)
		{
			u32_DsgCur_mA = u32_DsgCur_mA - OffsetValue_DSG;
		}
		else
		{
			// u32_DsgCur_mA = 0;
			u32_ChgCur_mA = u32_ChgCur_mA + OffsetValue_DSG - u32_DsgCur_mA;
			u32_DsgCur_mA = 0;
		}
		break;
	default:
		break;
	}
}

extern uint16_t time_chg;
extern uint16_t time_dsg;
extern uint16_t time_real;
void DataLoad_Current(void)
{
	// if ((SH367309_Read_AFE1.u16Current & 0x1000) == 0)
	if ((SH367309_Read_AFE1.u16Current & 0x8000) == 0)
	{
		// u32_ChgCur_mA = (UINT32)SH367309_Read_AFE1.u16Current * 1000 * g_u32CS_Res_AFE / gu32_CurCoefficient; // 默认使用200mV的计算方式
		u32_ChgCur_mA = (UINT32)SH367309_Read_AFE1.u16Current * 100 * g_u32CS_Res_AFE / (29127);
		// t_i32temp = (UINT32)(0xFFFF - SH367309_Read_AFE1.u16Current + 1) * g_u32CS_Res_AFE / (21470) * 200; // mA

		log_i("******************************************\n");
		log_i("AFE value->%d\n", u32_ChgCur_mA);

		u32_DsgCur_mA = 0;
	}
	else
	{
		// u32_DsgCur_mA = (UINT32)(0xFFFF - (SH367309_Read_AFE1.u16Current | 0xE000) + 1) * 1000 * g_u32CS_Res_AFE / gu32_CurCoefficient; // mA
		// u32_DsgCur_mA = (UINT32)(0xFFFF - SH367309_Read_AFE1.u16Current + 1) * 200 * g_u32CS_Res_AFE / (21470); // mA
		u32_DsgCur_mA = (UINT32)(0xFFFF - SH367309_Read_AFE1.u16Current + 1) * g_u32CS_Res_AFE / (29127) * 100; // mA

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

	// MonitorAFE(0, UpdateVoltageFromBqMaximo());
	UpdateVoltageFromBqMaximo();

	DataLoad_CellVolt();
	DataLoad_CellVoltMaxMinFind();
	// Sh_GetCadcCurrent();
	DataLoad_Current();
	DataLoad_Temperature();
	DataLoad_TemperatureMaxMinFind();
#if 0

	App_SH367309();
	App_MOS_Relay_Ctrl();
#endif
}
