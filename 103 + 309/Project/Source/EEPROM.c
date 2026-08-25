#include "main.h"
#include "UpgradeParamPolicy.h"
#include "SocEnhance.h"

#if FLASH_STORAGE_RW_PARAM_PROTECT_WORD_COUNT != E2P_PARA_NUM_PROTECT
#error "RW parameter protect word count mismatch"
#endif
#if FLASH_STORAGE_RW_PARAM_OTHER_WORD_COUNT != E2P_PARA_NUM_OTHER_ELEMENT1
#error "RW parameter other word count mismatch"
#endif
#if FLASH_STORAGE_RW_PARAM_RESERVED_WORD_COUNT != E2P_PARA_NUM_RESERVED_RW_PARAM
#error "RW parameter reserved word count mismatch"
#endif

static void EEPROM_UpdateOtherElementRuntime(void)
{
	SeriesNum = (UINT8)OtherElement.u16Sys_SeriesNum;
	if (OtherElement.u16Sys_CS_Res != 0)
	{
		g_u32CS_Res_AFE = ((UINT32)OtherElement.u16Sys_CS_Res_Num * 1000) / OtherElement.u16Sys_CS_Res;
	}
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
	EEPROM_UpdateOtherElementRuntime();
}

static void EEPROM_BuildRWParamData(STORAGE_FLASH_RW_PARAM_DATA *data)
{
	UINT16 i;
	for (i = 0; i < E2P_PARA_NUM_PROTECT; ++i)
		data->protect[i] = *(&PRT_E2ROMParas.u16VcellOvp_First + i);
	for (i = 0; i < E2P_PARA_NUM_OTHER_ELEMENT1; ++i)
		data->other[i] = *(&OtherElement.u16Balance_OpenVoltage + i);
	for (i = 0; i < E2P_PARA_NUM_RESERVED_RW_PARAM; ++i)
		data->reserved[i] = 0xFFFFU;
}

static UINT8 EEPROM_WordBlockInRange(const UINT16 *values, const UINT16 *min_values,
										 const UINT16 *max_values, UINT16 count)
{
	UINT16 i;
	for (i = 0; i < count; ++i)
	{
		if ((values[i] < min_values[i]) || (values[i] > max_values[i])) return 0;
	}
	return 1;
}

static UINT8 EEPROM_RWParamDataIsValid(const STORAGE_FLASH_RW_PARAM_DATA *data)
{
	const struct PRT_E2ROM_PARAS protect_min = E2P_PROTECT_MIN_PRT;
	const struct PRT_E2ROM_PARAS protect_max = E2P_PROTECT_MAX_PRT;
	const struct OTHER_ELEMENT other_min = OtherElement_min;
	const struct OTHER_ELEMENT other_max = OtherElement_max;
	if (!EEPROM_WordBlockInRange(data->protect, &protect_min.u16VcellOvp_First,
								 &protect_max.u16VcellOvp_First, E2P_PARA_NUM_PROTECT)) return 0;
	if (!EEPROM_WordBlockInRange(data->other, &other_min.u16Balance_OpenVoltage,
								 &other_max.u16Balance_OpenVoltage, E2P_PARA_NUM_OTHER_ELEMENT1)) return 0;
	return 1;
}

static void EEPROM_ApplyRWParamData(const STORAGE_FLASH_RW_PARAM_DATA *data)
{
	UINT16 i;
	for (i = 0; i < E2P_PARA_NUM_PROTECT; ++i)
		*(&PRT_E2ROMParas.u16VcellOvp_First + i) = data->protect[i];
	for (i = 0; i < E2P_PARA_NUM_OTHER_ELEMENT1; ++i)
		*(&OtherElement.u16Balance_OpenVoltage + i) = data->other[i];
	EEPROM_UpdateOtherElementRuntime();
}

UINT8 EEPROM_SaveRWParametersToFlash(void)
{
	STORAGE_FLASH_RW_PARAM_DATA data;
	EEPROM_BuildRWParamData(&data);
	if (!EEPROM_RWParamDataIsValid(&data))
	{
		System_ERROR_UserCallback(ERROR_EEPROM_STORE);
		return 0;
	}
	return StorageFlash_SaveRwParamData(&data);
}

static void EEPROM_LoadRWParametersFromFlash(void)
{
	STORAGE_FLASH_RW_PARAM_DATA data;
	if (StorageFlash_LoadRwParamData(&data) && EEPROM_RWParamDataIsValid(&data))
	{
		EEPROM_ApplyRWParamData(&data);
		return;
	}
	System_ERROR_UserCallback(ERROR_EEPROM_STORE);
	(void)EEPROM_SaveRWParametersToFlash();
}

static void EEPROM_LoadDefaultRuntimeData(void)
{
	EEPROM_LoadDefaultProtect();
	EEPROM_LoadDefaultCalib();
	EEPROM_LoadDefaultOtherElement();
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

#if UPGRADE_PARAM_POLICY_ENABLE && UPGRADE_PARAM_RESET_BALANCE_OPEN_VOLTAGE
static void EEPROM_LoadDefaultBalanceOpenVoltage(void)
{
	const struct OTHER_ELEMENT other_default = OtherElement_default;
	OtherElement.u16Balance_OpenVoltage = other_default.u16Balance_OpenVoltage;
}
#endif

UINT8 UpgradeParamPolicy_ApplyOnce(void)
{
#if (!UPGRADE_PARAM_POLICY_ENABLE) || (!UPGRADE_PARAM_POLICY_HAS_ACTION)
	return 1;
#else
	UINT8 result;
	UINT8 rw_param_dirty;
#if (!UPGRADE_PARAM_FORCE_REAPPLY)
	if (StorageFlash_GetConfigPolicyVersion() == UPGRADE_PARAM_POLICY_VERSION) return 1;
#endif
	result = 1;
	rw_param_dirty = 0;
#if UPGRADE_PARAM_RESET_AFE
	if (!EEPROM_ResetData_AFE_ParametersToDefault()) result = 0;
#endif
#if UPGRADE_PARAM_RESET_PROTECT
	EEPROM_LoadDefaultProtect();
	rw_param_dirty = 1;
#endif
#if UPGRADE_PARAM_RESET_BALANCE_OPEN_VOLTAGE
	EEPROM_LoadDefaultBalanceOpenVoltage();
	rw_param_dirty = 1;
#endif
#if UPGRADE_PARAM_RESET_SOC_CONFIG
	EEPROM_LoadDefaultSocConfig();
	rw_param_dirty = 1;
#endif
#if UPGRADE_PARAM_UPDATE_OTHER_ELEMENT
	EEPROM_LoadDefaultOtherElement();
	rw_param_dirty = 1;
#endif
	if (rw_param_dirty && !EEPROM_SaveRWParametersToFlash()) result = 0;
#if UPGRADE_PARAM_RESET_SOC_SNAPSHOT
	if (result && !SOC_ResetStoredSnapshotToDefault()) result = 0;
#endif
#if UPGRADE_PARAM_RESET_EVENT_RECORD
	if (result && !EEPROM_ResetData_EventRecord_ToDefault()) result = 0;
#endif
	if (!result) return 0;
	if (!StorageFlash_SetConfigPolicyVersion(UPGRADE_PARAM_POLICY_VERSION))
	{
		System_ERROR_UserCallback(ERROR_EEPROM_STORE);
		return 0;
	}
	return 1;
#endif
}

void InitE2PROM(void)
{
	EEPROM_LoadDefaultRuntimeData();
	EEPROM_LoadRWParametersFromFlash();
	ReadEEPROM_AFE_Parameters();
	ReadEEPROM_EventRecord_Parameters();
	UpgradeParamPolicy_ApplyOnce();
}
