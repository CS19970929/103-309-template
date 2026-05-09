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
	SH367309_Reg_Store.REG_MTP_CONF.bits.SLEEP = 1;
	MTPWrite(MTP_CONF, 1, &SH367309_Reg_Store.REG_MTP_CONF.all);
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
	// ucMTP_CONF |= 0x04;						//开启看门狗，不开看门狗行不行
	// 结论，可以不开启。看门狗溢出，操作是
	// 1，关闭充放电MOS和预充MOS
	// 2，清除均衡
	// 两者对于目前使用情况意义不大，休眠带电不允许开，MCU控驱动没意义
	// 30x是需要开的，因为是自己的保护体系，这个309用的是他自己的体系，所以就算出问题
	// 看门狗不关，他自己的保护体系判断是否关MOS，风险也不大。
	SH367309_Reg_Store.REG_MTP_CONF.bits.CADCON = 1; // 开启CADC
	SH367309_Reg_Store.REG_MTP_CONF.bits.CHGMOS = 0; // 充电MOS由AFE硬件控制
	SH367309_Reg_Store.REG_MTP_CONF.bits.DSGMOS = 1; // 放电MOS由AFE硬件控制
	MTPWrite(MTP_CONF, 1, &SH367309_Reg_Store.REG_MTP_CONF.all);
}

// 1：修改失败。0：修改成功
UINT8 SH367309_SC_DelayT_Set(void)
{
	UINT8 result = 0;
	UINT8 u8temp_now = 0;
	UINT8 u8temp_need = 0;
	UINT8 u8temp_write = 0;

	u8temp_now = SH367309_Reg_Store.u8_MTP_SCV_SCT & 0x0F;
	u8temp_need = OtherElement.u16CBC_DelayT >> 6; // 除以64得出等级，其表格就是以64us为一个等级的
	if (u8temp_need > 15)
		u8temp_need = 15;

	if (u8temp_now != u8temp_need)
	{
		u8temp_write = (UINT8)((SH367309_Reg_Store.u8_MTP_SCV_SCT & 0xF0) | u8temp_need);

		if (MTPWriteROM(0x0E, 1, &u8temp_write))
		{
			SH367309_Reg_Store.u8_MTP_SCV_SCT = u8temp_write;
			OtherElement.u16CBC_DelayT = (UINT16)u8temp_need << 6;
		}
		else
		{
			// 写失败
			OtherElement.u16CBC_DelayT = (UINT16)u8temp_now << 6;
			result = 1;
		}
	}
	else
	{
		// 相同，则修改上传参数便可
		OtherElement.u16CBC_DelayT = (UINT16)u8temp_now << 6;
	}

	return result;
}

void SH367309_DriverMos_Ctrl(GPIO_Type Type, UINT8 OnOFF)
{
	switch (Type)
	{
	case GPIO_PreCHG:
		SH367309_Reg_Store.REG_MTP_CONF.bits.PCHMOS = OnOFF;
		break;

	case GPIO_CHG:
		SH367309_Reg_Store.REG_MTP_CONF.bits.CHGMOS = OnOFF;
		break;

	case GPIO_DSG:
		SH367309_Reg_Store.REG_MTP_CONF.bits.DSGMOS = OnOFF;
		break;

	default:
		break;
	}

	MTPWrite(MTP_CONF, 1, &SH367309_Reg_Store.REG_MTP_CONF.all);
}

void Fault_ChangeToMCU(void)
{
	static UINT8 su8_CellOvp_Flag = 0;
	static UINT8 su8_CellUvp_Flag = 0;
	static UINT8 su8_IdischgOcp1_Flag = 0;
	static UINT8 su8_IdischgOcp2_Flag = 0;
	static UINT8 su8_IchgOcp_Flag = 0;
	static UINT8 su8_CellChgUtp_Flag = 0;
	static UINT8 su8_CellChgOtp_Flag = 0;
	static UINT8 su8_CellDsgUtp_Flag = 0;
	static UINT8 su8_CellDsgOtp_Flag = 0;

	g_stCellInfoReport.unMdlFault_Third.bits.b1CellOvp = SH367309_Reg_Store.REG_BSTATUS1.bits.OV;
	g_stCellInfoReport.unMdlFault_Third.bits.b1CellUvp = SH367309_Reg_Store.REG_BSTATUS1.bits.UV;
	g_stCellInfoReport.unMdlFault_Third.bits.b1IdischgOcp = SH367309_Reg_Store.REG_BSTATUS1.bits.OCD1 || SH367309_Reg_Store.REG_BSTATUS1.bits.OCD2;
	g_stCellInfoReport.unMdlFault_Third.bits.b1IchgOcp = SH367309_Reg_Store.REG_BSTATUS1.bits.OCC;
	g_stCellInfoReport.unMdlFault_Third.bits.b1CellChgUtp = SH367309_Reg_Store.REG_BSTATUS2.bits.UTC;
	g_stCellInfoReport.unMdlFault_Third.bits.b1CellChgOtp = SH367309_Reg_Store.REG_BSTATUS2.bits.OTC;
	g_stCellInfoReport.unMdlFault_Third.bits.b1CellDischgUtp = SH367309_Reg_Store.REG_BSTATUS2.bits.UTD;
	g_stCellInfoReport.unMdlFault_Third.bits.b1CellDischgOtp = SH367309_Reg_Store.REG_BSTATUS2.bits.OTD;
	if (SH367309_Reg_Store.REG_BSTATUS1.bits.SC)
		System_ErrFlag.u8ErrFlag_CBC_DSG = 1;
	else
		System_ErrFlag.u8ErrFlag_CBC_DSG = 0;

	switch (su8_CellOvp_Flag)
	{
	case 0:
		if (SH367309_Reg_Store.REG_BSTATUS1.bits.OV)
		{
			FaultWarnRecord2(CellOvp_Third);
			su8_CellOvp_Flag = 1;
		}
		break;
	case 1:
		if (!SH367309_Reg_Store.REG_BSTATUS1.bits.OV)
		{
			su8_CellOvp_Flag = 0;
		}
		break;
	default:
		break;
	}
	switch (su8_CellUvp_Flag)
	{
	case 0:
		if (SH367309_Reg_Store.REG_BSTATUS1.bits.UV)
		{
			FaultWarnRecord2(CellUvp_Third);
			su8_CellUvp_Flag = 1;

			GPIO_WriteBit(GPIO_DC_EN, PIN_DC_EN, Bit_RESET);
			GPIO_WriteBit(GPIO_2727_EN, PIN_2737_EN, Bit_RESET);
		}
		break;
	case 1:
		if (!SH367309_Reg_Store.REG_BSTATUS1.bits.UV)
		{
			su8_CellUvp_Flag = 0;

			GPIO_WriteBit(GPIO_DC_EN, PIN_DC_EN, Bit_SET);
			GPIO_WriteBit(GPIO_2727_EN, PIN_2737_EN, Bit_SET);
		}
		break;
	default:
		break;
	}

#if 1
	switch (su8_IdischgOcp1_Flag)
	{
	case 0:
		if (SH367309_Reg_Store.REG_BSTATUS1.bits.OCD1 || SH367309_Reg_Store.REG_BSTATUS1.bits.OCD2)
		{
			// FaultWarnRecord2(IdischgOcp_Second);
			FaultWarnRecord2(IdischgOcp_Third);
			su8_IdischgOcp1_Flag = 1;
		}
		break;

	case 1:
		if ((!SH367309_Reg_Store.REG_BSTATUS1.bits.OCD1)&& (!SH367309_Reg_Store.REG_BSTATUS1.bits.OCD2))
		{
			su8_IdischgOcp1_Flag = 0;
		}
		break;

	default:
		break;
	}
#endif

	// switch (su8_IdischgOcp2_Flag)
	// {
	// case 0:
	// 	if (g_stCellInfoReport.unMdlFault_Third.bits.b1IdischgOcp)
	// 	{
	// 		FaultWarnRecord2(IdischgOcp_Third);
	// 		su8_IdischgOcp2_Flag = 1;
	// 	}
	// 	break;
	// case 1:
	// 	if (!g_stCellInfoReport.unMdlFault_Third.bits.b1IdischgOcp)
	// 	{
	// 		su8_IdischgOcp2_Flag = 0;
	// 	}
	// 	break;
	// default:
	// 	break;
	// }
#if 1
	switch (su8_IchgOcp_Flag)
	{
	case 0:
		if (SH367309_Reg_Store.REG_BSTATUS1.bits.OCC)
		{
			FaultWarnRecord2(IchgOcp_Third);
			su8_IchgOcp_Flag = 1;
		}
		break;

	case 1:
		if (!SH367309_Reg_Store.REG_BSTATUS1.bits.OCC)
		{
			su8_IchgOcp_Flag = 0;
		}
		break;

	default:
		break;
	}

#else
	// switch (su8_IchgOcp_Flag)
	// {
	// case 0:
	// 	if (g_stCellInfoReport.unMdlFault_Second.bits.b1IchgOcp)
	// 	{
	// 		FaultWarnRecord2(IchgOcp_Second);
	// 		su8_IchgOcp_Flag = 1;
	// 	}
	// 	break;

	// case 1:
	// 	if (!g_stCellInfoReport.unMdlFault_Second.bits.b1IchgOcp)
	// 	{
	// 		su8_IchgOcp_Flag = 0;
	// 	}
	// 	break;

	// default:
	// 	break;
	// }
#endif

	switch (su8_CellChgUtp_Flag)
	{
	case 0:
		if (SH367309_Reg_Store.REG_BSTATUS2.bits.UTC)
		{
			FaultWarnRecord2(CellChgUTp_Third);
			su8_CellChgUtp_Flag = 1;
		}
		break;

	case 1:
		if (!SH367309_Reg_Store.REG_BSTATUS2.bits.UTC)
		{
			su8_CellChgUtp_Flag = 0;
		}
		break;

	default:
		break;
	}

	switch (su8_CellChgOtp_Flag)
	{
	case 0:
		if (SH367309_Reg_Store.REG_BSTATUS2.bits.OTC)
		{
			FaultWarnRecord2(CellChgOTp_Third);
			su8_CellChgOtp_Flag = 1;
		}
		break;

	case 1:
		if (!SH367309_Reg_Store.REG_BSTATUS2.bits.OTC)
		{
			su8_CellChgOtp_Flag = 0;
		}
		break;

	default:
		break;
	}

	switch (su8_CellDsgUtp_Flag)
	{
	case 0:
		if (SH367309_Reg_Store.REG_BSTATUS2.bits.UTD)
		{
			FaultWarnRecord2(CellDsgUTp_Third);
			su8_CellDsgUtp_Flag = 1;
		}
		break;

	case 1:
		if (!SH367309_Reg_Store.REG_BSTATUS2.bits.UTD)
		{
			su8_CellDsgUtp_Flag = 0;
		}
		break;

	default:
		break;
	}

	switch (su8_CellDsgOtp_Flag)
	{
	case 0:
		if (SH367309_Reg_Store.REG_BSTATUS2.bits.OTD)
		{
			FaultWarnRecord2(CellDsgOTp_Third);
			su8_CellDsgOtp_Flag = 1;
		}
		break;

	case 1:
		if (!SH367309_Reg_Store.REG_BSTATUS2.bits.OTD)
		{
			su8_CellDsgOtp_Flag = 0;
		}
		break;

	default:
		break;
	}
}

void App_SH367309_Monitor(void)
{
	static UINT8 su8_SC_Flag = 0;
	static UINT8 su8_EEPR_WR_Flag = 0;

	// if(MTPRead(MTP_BSTATUS1, 3, &SH367309_Reg_Store.REG_BSTATUS1.all)) {
	if (MTPRead(MTP_BALANCEH, 5, &SH367309_Reg_Store.u8_MTP_BALANCEH))
	{
		// g_stCellInfoReport.u16BalanceFlag1 = SH367309_Reg_Store.u8_MTP_BALANCEL;
		// g_stCellInfoReport.u16BalanceFlag2 = SH367309_Reg_Store.u8_MTP_BALANCEH;
		// SystemStatus.bits.b1Status_MOS_PRE = SH367309_Reg_Store.REG_BSTATUS3.bits.PCHG_FET;
		SystemStatus.bits.b1Status_MOS_CHG = SH367309_Reg_Store.REG_BSTATUS3.bits.CHG_FET;
		SystemStatus.bits.b1Status_MOS_DSG = SH367309_Reg_Store.REG_BSTATUS3.bits.DSG_FET;

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

		// 观察类型，不能被置位，置位说明有问题，配置不对
		// SH367309_Reg_Store.REG_BSTATUS1.bits.PF;
		// SH367309_Reg_Store.REG_BSTATUS1.bits.WDT;
		// SH367309_Reg_Store.REG_BSTATUS3.bits.L0V;
		// SH367309_Reg_Store.REG_BSTATUS3.bits.EEPR_WR;
		switch (su8_EEPR_WR_Flag)
		{
		case 0:
			if (SH367309_Reg_Store.REG_BSTATUS3.bits.EEPR_WR)
			{
				System_ERROR_UserCallback(ERROR_EEPROM_STORE);
				su8_EEPR_WR_Flag = 1;
			}
			break;

		case 1:
			// 负载断开(DSGD管教电平低于VDSGD)，持续时间超过负载释放延时tD1，现在设置为2s
			if (!SH367309_Reg_Store.REG_BSTATUS3.bits.EEPR_WR)
			{
				su8_EEPR_WR_Flag = 0;
			}
			break;

		default:
			break;
		}

		if (SH367309_Reg_Store.REG_BSTATUS1.bits.PF)
		{
			System_ERROR_UserCallback(ERROR_SPI);
		}

		if (SH367309_Reg_Store.REG_BSTATUS1.bits.WDT)
		{
			System_ERROR_UserCallback(ERROR_UPPER);
		}

		if (SH367309_Reg_Store.REG_BSTATUS3.bits.L0V)
		{
			System_ERROR_UserCallback(ERROR_CLIENT);
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
