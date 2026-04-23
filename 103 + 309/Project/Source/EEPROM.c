#include "main.h"

UINT32 u32E2P_Pro_VolCur_WriteFlag = 0;
UINT32 u32E2P_Pro_Temp_WriteFlag = 0;
UINT32 u32E2P_Pro_Other_WriteFlag = 0;
UINT32 u32E2P_RTC_Element_WriteFlag = 0;
UINT32 u32E2P_OtherElement1_WriteFlag = 0;
UINT32 u32E2P_HeatCool_WriteFlag = 0;

UINT8 u8E2P_SocTable_WriteFlag = 0;
UINT8 u8E2P_CopperLoss_WriteFlag = 0;
UINT8 u8E2P_KB_WriteFlag = 0;
UINT8 u8E2P_KB_WritePos = 0;

uint16_t curr_offset = 0;
UINT16 OffsetValue_CHG = 0;
UINT16 OffsetValue_DSG = 0;

extern const UINT16 SOC_Table_Default[SOC_TABLE_SIZE];

static void EEPROM_ClearLegacyWriteFlags(void)
{
	u32E2P_Pro_VolCur_WriteFlag = 0;
	u32E2P_Pro_Temp_WriteFlag = 0;
	u32E2P_Pro_Other_WriteFlag = 0;
	u32E2P_RTC_Element_WriteFlag = 0;
	u32E2P_OtherElement1_WriteFlag = 0;
	u32E2P_HeatCool_WriteFlag = 0;
	u8E2P_SocTable_WriteFlag = 0;
	u8E2P_CopperLoss_WriteFlag = 0;
	u8E2P_KB_WriteFlag = 0;
	u8E2P_KB_WritePos = 0;
	gu8_Reset_EventRecord = 0;
}

static void EEPROM_LoadDefaultProtect(void)
{
	UINT8 i;
	const struct PRT_E2ROM_PARAS protect_default = E2P_PROTECT_DEFAULT_PRT;

	for (i = 0; i < E2P_PARA_NUM_PROTECT; ++i)
	{
		*(&PRT_E2ROMParas.u16VcellOvp_First + i) = *(&protect_default.u16VcellOvp_First + i);
	}
}

static void EEPROM_LoadDefaultCalib(void)
{
	UINT8 i;

	for (i = 0; i < KB_NUM; ++i)
	{
		g_u16CalibCoefK[i] = SYSKDEFAULT;
		g_i16CalibCoefB[i] = SYSBDEFAULT;
	}
}

static void EEPROM_LoadDefaultOtherElement(void)
{
	UINT8 i;
	const struct OTHER_ELEMENT other_default = OtherElement_default;

	for (i = 0; i < E2P_PARA_NUM_OTHER_ELEMENT1; ++i)
	{
		*(&OtherElement.u16Balance_OpenVoltage + i) = *(&other_default.u16Balance_OpenVoltage + i);
	}

	g_u32CS_Res_AFE = ((UINT32)OtherElement.u16Sys_CS_Res_Num * 1000) / OtherElement.u16Sys_CS_Res;
}

static void EEPROM_LoadDefaultHeatCool(void)
{
	UINT8 i;
	const struct HEAT_COOL_ELEMENT heatcool_default = HeatCoolElement_Default;

	for (i = 0; i < E2P_PARA_NUM_HEAT_COOL; ++i)
	{
		*(&Heat_Cool_Element.u16Heat_OpenTemp + i) = *(&heatcool_default.u16Heat_OpenTemp + i);
	}
}

static void EEPROM_LoadDefaultSocTable(void)
{
	UINT8 i;

	for (i = 0; i < SOC_TABLE_SIZE; ++i)
	{
		SOC_Table_Set[i] = SOC_Table_Default[i];
	}
}

static void EEPROM_LoadDefaultCopperLoss(void)
{
	UINT8 i;

	for (i = 0; i < CompensateNUM; ++i)
	{
		CopperLoss[i] = CopperLoss_Default;
		CopperLoss_Num[i] = CopperLossNum_Default;
	}
}

static void EEPROM_LoadDefaultRuntimeData(void)
{
	EEPROM_LoadDefaultProtect();
	EEPROM_LoadDefaultCalib();
	EEPROM_LoadDefaultOtherElement();
	EEPROM_LoadDefaultHeatCool();
	EEPROM_LoadDefaultSocTable();
	EEPROM_LoadDefaultCopperLoss();

	curr_offset = 0;
	OffsetValue_CHG = 0;
	OffsetValue_DSG = 0;

	System_ERROR_UserCallback(ERROR_REMOVE_EEPROM_COM);
	System_ERROR_UserCallback(ERROR_REMOVE_EEPROM_STORE);
}

UINT8 ReadEEPROM_Byte(UINT16 addr)
{
	(void)addr;
	return 0xFF;
}

UINT8 WriteEEPROM_Byte(UINT16 addr, UINT8 val)
{
	(void)addr;
	(void)val;
	return 0;
}

UINT16 ReadEEPROM_Word_NoZone(UINT16 addr)
{
	(void)addr;
	return 0xFFFF;
}

UINT8 WriteEEPROM_Word_NoZone(UINT16 addr, UINT16 data)
{
	(void)addr;
	(void)data;
	return 0;
}

void InitE2PROM_i2c(void)
{
}

void InitE2PROM(void)
{
	EEPROM_LoadDefaultRuntimeData();
	ReadEEPROM_AFE_Parameters();
	ReadEEPROM_EventRecord_Parameters();
	EEPROM_ClearLegacyWriteFlags();
}

void App_E2promDeal(void)
{
	EEPROM_ClearLegacyWriteFlags();
}

void EEPROM_test(void)
{
}
