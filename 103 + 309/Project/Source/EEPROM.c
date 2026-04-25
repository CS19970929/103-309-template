#include "main.h"
#include "UpgradeParamPolicy.h"

uint16_t curr_offset = 0;
UINT16 OffsetValue_CHG = 0;
UINT16 OffsetValue_DSG = 0;

extern const UINT16 SOC_Table_Default[SOC_TABLE_SIZE];

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

#if UPGRADE_PARAM_POLICY_ENABLE && UPGRADE_PARAM_RESET_SOC_CONFIG
static void EEPROM_LoadDefaultSocConfig(void)
{
	const struct OTHER_ELEMENT other_default = OtherElement_default;

	OtherElement.u16Soc_TableSelect = other_default.u16Soc_TableSelect;
	OtherElement.u16Soc_Ah = other_default.u16Soc_Ah;
	OtherElement.u16Soc_Cycle_times = other_default.u16Soc_Cycle_times;
	OtherElement.u16Soc_V_100 = other_default.u16Soc_V_100;
	OtherElement.u16Soc_V_0 = other_default.u16Soc_V_0;
}
#endif

UINT8 UpgradeParamPolicy_ApplyOnce(void)
{
#if (!UPGRADE_PARAM_POLICY_ENABLE)
	return 1;
#else
	UINT8 result;

	if (!UPGRADE_PARAM_POLICY_HAS_ACTION)
	{
		return 1;
	}

#if (!UPGRADE_PARAM_FORCE_REAPPLY)
	if (FlashReadOneHalfWord(FLASH_ADDR_UPGRADE_PARAM_FLAG) == UPGRADE_PARAM_POLICY_VERSION)
	{
		return 1;
	}
#endif

	result = 1;

#if UPGRADE_PARAM_RESET_AFE
	if (!EEPROM_ResetData_AFE_ParametersToDefault())
	{
		result = 0;
	}
#endif

#if UPGRADE_PARAM_RESET_PROTECT
	EEPROM_LoadDefaultProtect();
#endif

#if UPGRADE_PARAM_RESET_SOC_TABLE
	EEPROM_LoadDefaultSocTable();
#endif

#if UPGRADE_PARAM_RESET_SOC_CONFIG
	EEPROM_LoadDefaultSocConfig();
#endif

	if (!result)
	{
		return 0;
	}

	if (FlashWriteOneHalfWord(FLASH_ADDR_UPGRADE_PARAM_FLAG, UPGRADE_PARAM_POLICY_VERSION) != FLASH_COMPLETE)
	{
		System_ERROR_UserCallback(ERROR_EEPROM_STORE);
		return 0;
	}

	return 1;
#endif
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
	UpgradeParamPolicy_ApplyOnce();
}

void App_E2promDeal(void)
{
}


