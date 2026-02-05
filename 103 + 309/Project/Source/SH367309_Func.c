#include "main.h"

SH367309_REG_STORE SH367309_Reg_Store;

UINT8 gu8_DriverStartUpFlag = 0;

//-40到100的数值
UINT16 iSheldTemp_10K_NTC[141] = {20375, 19204, 18115, 17100, 16152, 15266, 14437, 13661, 12934, 12251,
								  11611, 11008, 10442, 9909, 9407, 8935, 8489, 8068, 7672, 7297,
								  6943, 6608, 6292, 5993, 5710, 5442, 5188, 4948, 4720, 4504,
								  4300, 4105, 3921, 3746, 3580, 3422, 3272, 3130, 2994, 2866,
								  2751, 2627, 2516, 2410, 2310, 2214, 2123, 2036, 1953, 1874,
								  1801, 1726, 1658, 1592, 1530, 1470, 1413, 1358, 1306, 1256,
								  1209, 1163, 1119, 1078, 1038, 1000, 963, 928, 894, 862,
								  831, 801, 773, 746, 719, 694, 670, 647, 625, 604,
								  583, 563, 544, 526, 509, 492, 476, 460, 445, 431,
								  416, 403, 390, 378, 366, 355, 343, 333, 322, 312,
								  303, 294, 285, 276, 268, 260, 252, 244, 237, 230,
								  224, 217, 211, 205, 199, 193, 188, 182, 177, 172,
								  167, 163, 158, 154, 150, 146, 142, 138, 134, 131,
								  127, 124, 120, 117, 114, 111, 108, 106, 103, 100,
								  98};

// 前26个寄存器默认参数
UINT8 ucMTPBuffer[26] = {
	BYTE_00H_SCONF1, BYTE_01H_SCONF2, BYTE_02H_OVT_LDRT_OVH, BYTE_03H_OVL, BYTE_04H_UVT_OVRH,
	BYTE_05H_OVRL, BYTE_06H_UV, BYTE_07H_UVR, BYTE_08H_BALV, BYTE_09H_PREV,
	BYTE_0AH_L0V, BYTE_0BH_PFV, BYTE_0CH_OCD1V_OCD1T, BYTE_0DH_OCD2V_OCD2T, BYTE_0EH_SCV_SCT,
	BYTE_0FH_OCCV_OCCT, BYTE_10H_MOST_OCRT_PFT, BYTE_11H_OTC, BYTE_12H_OTCR, BYTE_13H_UTC,
	BYTE_14H_UTCR, BYTE_15H_OTD, BYTE_16H_OTDR, BYTE_17H_UTD, BYTE_18H_UTDR,
	BYTE_19H_TR};

UINT16 aaaaaa1 = 0;
UINT16 aaaaaa2 = 0;
UINT16 aaaaaa3 = 0;
UINT16 aaaaaa4 = 0;
UINT8 aaa11 = 0;

/*******************************************************************************
Function:ResetAFE()
Description:  Reset SH367309 IC, Send Data:0xEA, 0xC0, CRC
Input:	 NULL
Output: NULL
Others:
*******************************************************************************/
void AFE_Reset(void)
{
	UINT8 WrBuf[2];

	WrBuf[0] = 0xC0;
	WrBuf[1] = 0xA5;

	/*
	if(!System_ErrFlag.u8ErrFlag_Com_AFE1) {
		if(!MTPWrite(AFE_ID, 0xEA, 1, WrBuf)) {              //0xEA, 0xC0?A CRC
			MTPWrite(AFE_ID, 0xEA, 1, WrBuf);
		}
		//MTPWrite(0xEA, 1, WrBuf);
	}
	*/

	if (!System_ERROR_UserCallback(ERROR_STATUS_AFE1))
	{
		if (!MTPWrite(0xEA, 1, WrBuf))
		{ // 0xEA, 0xC0?A CRC
			MTPWrite(0xEA, 1, WrBuf);
		}
	}
}

// 进入休眠模式
void AFE_Sleep(void)
{
	sh36735_write_reg_u8(AFE_SCONF1, 0xAA);
}

// 进入IDLE模式
// 1，有错误，不能进入
UINT8 AFE_IDLE_Old(void)
{
	UINT8 result = 0;

	if (MTPRead(MTP_BSTATUS1, 3, &SH367309_Reg_Store.REG_BSTATUS1.all))
	{
		if (SH367309_Reg_Store.REG_BSTATUS1.all || SH367309_Reg_Store.REG_BSTATUS2.all || SH367309_Reg_Store.REG_BSTATUS3.bits.L0V || SH367309_Reg_Store.REG_BSTATUS3.bits.PCHG_FET)
		{
			// 不能进入IDLE
			result = 1;
			System_ERROR_UserCallback(ERROR_AFE1);
		}
		else
		{
			SH367309_Reg_Store.REG_MTP_CONF.bits.IDLE = 1;
			MTPWrite(MTP_CONF, 1, &SH367309_Reg_Store.REG_MTP_CONF.all);
		}
	}
	else
	{
		result = 1;
		// System_ERROR_UserCallback(ERROR_AFE1);
	}

	return result;
}

void AFE_IDLE(void)
{
	SH367309_Reg_Store.REG_MTP_CONF.bits.IDLE = 1;
	MTPWrite(MTP_CONF, 1, &SH367309_Reg_Store.REG_MTP_CONF.all);
}

// 进入休眠模式
void AFE_SHIP(void)
{
	// MCUO_AFE_SHIP = 0;
}

// 1，出问题，同时上报系统AFE错误
// 0，没问题
UINT8 AFE_IsReady(void)
{
	UINT8 TempCnt = 0, result = 0;
	UINT8 TempVar;

	while (1)
	{
		Feed_IWatchDog;

		TempVar = 0;
		if (MTPRead(MTP_BFLAG2, 1, &TempVar))
		{ // 读取BLFG2，查看VADC是否转换完成
			if ((TempVar & 0x10) == 0x10)
			{
				break;
			}
		}

		Delay1ms(20);
		if (++TempCnt >= 50)
		{
			// System_ERROR_UserCallback(ERROR_AFE1);
			result = 1;
			break;
		}
	}
	return result;
}

UINT8 AFE_GetData(void)
{
	UINT8 result;
	result = MTPRead(MTP_TEMP1, sizeof(Registers_AFE1), (UINT8 *)&Registers_AFE1);
	return result;
}

// 1：状态查询失败。0：成功
UINT8 AFE_CheckStatus(void)
{
	UINT8 result = 0;

	if (!MTPRead(MTP_BSTATUS1, 3, &SH367309_Reg_Store.REG_BSTATUS1.all))
	{
		result = 1;
	}
	return result;
}

/*******************************************************************************
Function:EnableAFEWdtCadc()
Description:使能CHG&DSG&PCHG输出，且使能WDT和CADC模块
Input:
Output:
Others:
*******************************************************************************/
void SH367309_Enable_AFE_Wdt_Cadc_Drivers(void)
{
}

// #define isCOV g_stCellInfoReport.unMdlFault_Third.bits.b1CellOvp
// #define isCUV
// #define isOCC
// #define isODC
// #define isOTC
// #define isUTC
// #define isOTD
// #define isUTD

void SH_AFE_GetProtectStatus(void)
{
	g_stCellInfoReport.unMdlFault_Third.bits.b1CellOvp = Registers_AFE1.flag1.bits.ov_flg;
	g_stCellInfoReport.unMdlFault_Third.bits.b1CellUvp = Registers_AFE1.flag1.bits.uv_flg;
	g_stCellInfoReport.unMdlFault_Third.bits.b1IchgOcp = Registers_AFE1.flag1.bits.occ_flg;
	System_ErrFlag.u8ErrFlag_CBC_DSG = Registers_AFE1.flag1.bits.sc_flg;
	g_stCellInfoReport.unMdlFault_Third.bits.b1IdischgOcp = Registers_AFE1.flag1.bits.ocd1_flg | Registers_AFE1.flag1.bits.ocd2_flg;

	g_stCellInfoReport.unMdlFault_Third.bits.b1CellChgOtp = Registers_AFE1.flag2.bits.otc_flg;
	g_stCellInfoReport.unMdlFault_Third.bits.b1CellChgUtp = Registers_AFE1.flag2.bits.utc_flg;
	g_stCellInfoReport.unMdlFault_Third.bits.b1CellDischgOtp = Registers_AFE1.flag2.bits.otd_flg;
	g_stCellInfoReport.unMdlFault_Third.bits.b1CellDischgUtp = Registers_AFE1.flag2.bits.utd_flg;
}

bool SH_AFE_ClearProtectFlag(AFE_ProtectType AFE_Protect)
{
	bool Result = false;
	uint8_t Temp;
	uint8_t write_reg = Registers_AFE1.sonf2.all | 0x80; // 配置LTCLR=1
	sh36735_write_reg_u8(AFE_SCONF2, write_reg);

	if (AFE_Protect & 0xFF00)
	{
		Temp = (Registers_AFE1.flag2.all) & (uint8_t)(~(AFE_Protect | 0xFE)); // 将需要恢复的保护标志位清零
		Result = sh36735_write_reg_u8(AFE_FLAG2, Temp);
	}
	else
	{
		Temp = (Registers_AFE1.flag1.all) & (uint8_t)(~AFE_Protect);
		Result = sh36735_write_reg_u8(AFE_FLAG1, Temp);
	}

	return Result;
}
#if 0
void ProtectOV(void)
{
	if (!Info.bOV)
	{
		usOVRDelayCnt = 0;
	}
	else
	{
		if (Info.ssVCellMax < parameter.E2usOVRVol) // OV恢复电压判定
		{
			if (++usOVRDelayCnt >= OVR_DELAY_CNT)
			{
				if (SH_AFE_ClearProtectFlag(AFE_FLAG_OV)) // 清零AFE中的标志位
				{
					Info.bOV = 0;
					usOVRDelayCnt = 0;
				}
			}
		}
		else
		{
			usOVRDelayCnt = 0;
		}
	}
}
#endif

void SH367309_DriverMos_Ctrl(GPIO_Type Type, UINT8 OnOFF)
{
	switch (Type)
	{
	case GPIO_PreCHG:
		SH367309_Reg_Store.REG_MTP_CONF.bits.PCHMOS = OnOFF;
		break;
	case GPIO_CHG:
		Registers_AFE1.sonf2.bits.CHGMOS = OnOFF;
		if (OnOFF)
			GPIO_SetBits(GPIO_M_CCC, PIN_M_CCC);
		else
			GPIO_ResetBits(GPIO_M_CCC, PIN_M_CCC);
		break;
	case GPIO_DSG:
		Registers_AFE1.sonf2.bits.DSGMOS = OnOFF;
		break;
	default:
		break;
	}

	Registers_AFE1.sonf2.all |= 0x80;
	sh36735_write_reg_u8(AFE_SCONF2, Registers_AFE1.sonf2.all);
}

void Fault_ChangeToMCU(void)
{
}

void TemperatureCheck(void)
{
	// SH367309_Reg_Store.REG_BSTATUS2.bits.OTC;
	// SH367309_Reg_Store.REG_BSTATUS2.bits.UTC;
	// SH367309_Reg_Store.REG_BSTATUS2.bits.OTD;
	// SH367309_Reg_Store.REG_BSTATUS2.bits.UTD;

	if (SH367309_Reg_Store.REG_BSTATUS2.bits.OTC || SH367309_Reg_Store.REG_BSTATUS2.bits.UTC)
	{
		if (g_stCellInfoReport.u16Ichg == 0)
		{
			SH367309_Reg_Store.REG_BSTATUS2.bits.OTC = 0;
			SH367309_Reg_Store.REG_BSTATUS2.bits.UTC = 0;
		}
	}

	if (SH367309_Reg_Store.REG_BSTATUS2.bits.OTD || SH367309_Reg_Store.REG_BSTATUS2.bits.UTD)
	{
		if (g_stCellInfoReport.u16IDischg == 0)
		{
			SH367309_Reg_Store.REG_BSTATUS2.bits.OTD = 0;
			SH367309_Reg_Store.REG_BSTATUS2.bits.UTD = 0;
		}
	}
}

// mos控制汇总，历史保护记录加入体系
void App_SH367309_Monitor(void)
{
	static UINT8 su8_SC_Flag = 0;
	static UINT8 su8_EEPR_WR_Flag = 0;

	static UINT8 su8_CtrlMos_Flag = 0;
	// if (0 == g_st_SysTimeFlag.bits.b1Sys100msFlag)
	// { // 这个时基不能随便调，影响MOS动作，初始化电流校准
	// 	return;
	// }

	// if(MTPRead(MTP_BSTATUS1, 3, &SH367309_Reg_Store.REG_BSTATUS1.all)) {
	if (MTPRead(MTP_BALANCEH, 5, &SH367309_Reg_Store.u8_MTP_BALANCEH))
	{
		Fault_ChangeToMCU();

		switch (su8_SC_Flag)
		{
		case 0:
			if (SH367309_Reg_Store.REG_BSTATUS1.bits.SC)
			{
				System_ERROR_UserCallback(ERROR_CBC_DSG);
				su8_SC_Flag = 1;
			}
			break;

		case 1:
			// 负载断开(DSGD管教电平低于VDSGD)，持续时间超过负载释放延时tD1，现在设置为2s
			if (!SH367309_Reg_Store.REG_BSTATUS1.bits.SC)
			{
				su8_SC_Flag = 0;
			}
			break;

		default:
			break;
		}
	}
	else
	{
		// 读取失败，要做点什么事情？
		// 进入深度休眠咯？
		// Sleep_Mode.bits.b1ForceToSleep_L3 = 1;
	}
}

void App_SH367309(void)
{
	App_SH367309_Monitor();
	SH367309_UpdataAfeConfig();
}
