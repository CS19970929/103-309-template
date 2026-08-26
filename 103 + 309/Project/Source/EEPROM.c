#include "main.h"
#include "UpgradeParamPolicy.h"
#include "SocEnhance.h"

#if BMS_CONFIG_AFE_WORD_COUNT != AFE_PARAMETES_TOTAL_LENGTH
#error "BMS config AFE word count mismatch"
#endif
#if BMS_CONFIG_PROTECT_WORD_COUNT != E2P_PARA_NUM_PROTECT
#error "BMS config protect word count mismatch"
#endif
#if BMS_CONFIG_CALIB_WORD_COUNT != KB_NUM
#error "BMS config calibration word count mismatch"
#endif
#if BMS_CONFIG_OTHER_WORD_COUNT != E2P_PARA_NUM_OTHER_ELEMENT1
#error "BMS config other word count mismatch"
#endif
#if BMS_CONFIG_RESERVED_WORD_COUNT != E2P_PARA_NUM_RESERVED_RW_PARAM
#error "BMS config reserved word count mismatch"
#endif

static UINT16 s_u16ConfigPolicyVersion = FLASH_UPGRADE_PARAM_FLAG_RESET;

static void EEPROM_UpdateOtherElementRuntime(void)
{
	SeriesNum = (UINT8)OtherElement.u16Sys_SeriesNum;
	if (OtherElement.u16Sys_CS_Res != 0U)
	{
		g_u32CS_Res_AFE = ((UINT32)OtherElement.u16Sys_CS_Res_Num * 1000U) /
			OtherElement.u16Sys_CS_Res;
	}
}

static void EEPROM_LoadDefaultAfe(void)
{
	UINT16 i;
	AFE_Value_Typedef *param = &AFE_Parameters_RS485_Struction.u16VcellOvp;

	for (i = 0U; i < AFE_PARAMETES_TOTAL_LENGTH; ++i)
	{
		(param + i)->curValue = (param + i)->defaultValue;
	}
	AFE_PARAM_WRITE_Flag = 1;
}

static void EEPROM_LoadDefaultProtect(void)
{
	UINT16 i;
	const struct PRT_E2ROM_PARAS protect_default = E2P_PROTECT_DEFAULT_PRT;

	for (i = 0U; i < E2P_PARA_NUM_PROTECT; ++i)
	{
		*(&PRT_E2ROMParas.u16VcellOvp_First + i) =
			*(&protect_default.u16VcellOvp_First + i);
	}
}

static void EEPROM_LoadDefaultCalib(void)
{
	UINT16 i;

	for (i = 0U; i < KB_NUM; ++i)
	{
		g_u16CalibCoefK[i] = SYSKDEFAULT;
		g_i16CalibCoefB[i] = SYSBDEFAULT;
	}
}

static void EEPROM_LoadDefaultOtherElement(void)
{
	UINT16 i;
	const struct OTHER_ELEMENT other_default = OtherElement_default;

	for (i = 0U; i < E2P_PARA_NUM_OTHER_ELEMENT1; ++i)
	{
		*(&OtherElement.u16Balance_OpenVoltage + i) =
			*(&other_default.u16Balance_OpenVoltage + i);
	}
	EEPROM_UpdateOtherElementRuntime();
}

static void EEPROM_LoadDefaultRuntimeData(void)
{
	EEPROM_LoadDefaultAfe();
	EEPROM_LoadDefaultProtect();
	EEPROM_LoadDefaultCalib();
	EEPROM_LoadDefaultOtherElement();
	s_u16ConfigPolicyVersion = FLASH_UPGRADE_PARAM_FLAG_RESET;
	System_ERROR_UserCallback(ERROR_REMOVE_EEPROM_COM);
	System_ERROR_UserCallback(ERROR_REMOVE_EEPROM_STORE);
}

static UINT8 EEPROM_WordBlockInRange(const UINT16 *values,
										 const UINT16 *min_values,
										 const UINT16 *max_values,
										 UINT16 count)
{
	UINT16 i;

	for (i = 0U; i < count; ++i)
	{
		if ((values[i] < min_values[i]) || (values[i] > max_values[i]))
		{
			return 0U;
		}
	}
	return 1U;
}

static void EEPROM_BuildConfig(BMS_CONFIG *config)
{
	UINT16 i;
	AFE_Value_Typedef *afe = &AFE_Parameters_RS485_Struction.u16VcellOvp;

	memset(config, 0xFF, sizeof(*config));
	config->u16FormatVersion = FLASH_STORAGE_CONFIG_FORMAT_VERSION;
	config->u16AppliedPolicyVersion = s_u16ConfigPolicyVersion;

	for (i = 0U; i < BMS_CONFIG_AFE_WORD_COUNT; ++i)
	{
		config->afe[i] = (afe + i)->curValue;
	}
	for (i = 0U; i < BMS_CONFIG_PROTECT_WORD_COUNT; ++i)
	{
		config->protect[i] = *(&PRT_E2ROMParas.u16VcellOvp_First + i);
	}
	for (i = 0U; i < BMS_CONFIG_CALIB_WORD_COUNT; ++i)
	{
		config->calibK[i] = g_u16CalibCoefK[i];
		config->calibB[i] = g_i16CalibCoefB[i];
	}
	for (i = 0U; i < BMS_CONFIG_OTHER_WORD_COUNT; ++i)
	{
		config->other[i] = *(&OtherElement.u16Balance_OpenVoltage + i);
	}
}

static UINT8 EEPROM_ConfigAfeIsValid(const BMS_CONFIG *config)
{
	UINT16 i;
	AFE_Value_Typedef *afe = &AFE_Parameters_RS485_Struction.u16VcellOvp;

	for (i = 0U; i < BMS_CONFIG_AFE_WORD_COUNT; ++i)
	{
		if ((config->afe[i] < (afe + i)->minValue) ||
			(config->afe[i] > (afe + i)->maxValue))
		{
			return 0U;
		}
	}
	return 1U;
}

static UINT8 EEPROM_ConfigCalibrationIsValid(const BMS_CONFIG *config)
{
	UINT16 i;

	for (i = 0U; i < BMS_CONFIG_CALIB_WORD_COUNT; ++i)
	{
		if ((config->calibK[i] < SYSKMIN) || (config->calibK[i] > SYSKMAX) ||
			(config->calibB[i] < SYSBMIN) || (config->calibB[i] > SYSBMAX))
		{
			return 0U;
		}
	}
	return 1U;
}

static UINT8 EEPROM_ConfigIsValid(const BMS_CONFIG *config)
{
	const struct PRT_E2ROM_PARAS protect_min = E2P_PROTECT_MIN_PRT;
	const struct PRT_E2ROM_PARAS protect_max = E2P_PROTECT_MAX_PRT;
	const struct OTHER_ELEMENT other_min = OtherElement_min;
	const struct OTHER_ELEMENT other_max = OtherElement_max;

	if ((config == 0) ||
		(config->u16FormatVersion != FLASH_STORAGE_CONFIG_FORMAT_VERSION))
	{
		return 0U;
	}
	if (!EEPROM_ConfigAfeIsValid(config))
	{
		return 0U;
	}
	if (!EEPROM_WordBlockInRange(config->protect,
									 &protect_min.u16VcellOvp_First,
									 &protect_max.u16VcellOvp_First,
									 BMS_CONFIG_PROTECT_WORD_COUNT))
	{
		return 0U;
	}
	if (!EEPROM_ConfigCalibrationIsValid(config))
	{
		return 0U;
	}
	if (!EEPROM_WordBlockInRange(config->other,
									 &other_min.u16Balance_OpenVoltage,
									 &other_max.u16Balance_OpenVoltage,
									 BMS_CONFIG_OTHER_WORD_COUNT))
	{
		return 0U;
	}
	return 1U;
}

static void EEPROM_ApplyConfig(const BMS_CONFIG *config)
{
	UINT16 i;
	AFE_Value_Typedef *afe = &AFE_Parameters_RS485_Struction.u16VcellOvp;

	for (i = 0U; i < BMS_CONFIG_AFE_WORD_COUNT; ++i)
	{
		(afe + i)->curValue = config->afe[i];
	}
	for (i = 0U; i < BMS_CONFIG_PROTECT_WORD_COUNT; ++i)
	{
		*(&PRT_E2ROMParas.u16VcellOvp_First + i) = config->protect[i];
	}
	for (i = 0U; i < BMS_CONFIG_CALIB_WORD_COUNT; ++i)
	{
		g_u16CalibCoefK[i] = config->calibK[i];
		g_i16CalibCoefB[i] = config->calibB[i];
	}
	for (i = 0U; i < BMS_CONFIG_OTHER_WORD_COUNT; ++i)
	{
		*(&OtherElement.u16Balance_OpenVoltage + i) = config->other[i];
	}

	s_u16ConfigPolicyVersion = config->u16AppliedPolicyVersion;
	AFE_PARAM_WRITE_Flag = 1;
	EEPROM_UpdateOtherElementRuntime();
}

UINT8 EEPROM_SaveConfigToFlash(void)
{
	BMS_CONFIG config;
	UINT8 result;

	EEPROM_BuildConfig(&config);
	if (!EEPROM_ConfigIsValid(&config))
	{
		System_ERROR_UserCallback(ERROR_EEPROM_STORE);
		return 0U;
	}

	result = StorageFlash_SaveConfigData(&config);
	if (result != 0U)
	{
		System_ERROR_UserCallback(ERROR_REMOVE_EEPROM_STORE);
	}
	else
	{
		System_ERROR_UserCallback(ERROR_EEPROM_STORE);
	}
	return result;
}

UINT8 EEPROM_SaveRWParametersToFlash(void)
{
	/* Compatibility entry point for existing parameter writers. The persisted
	 * object is the complete BMS_CONFIG, never a category-specific record. */
	return EEPROM_SaveConfigToFlash();
}

static void EEPROM_LoadConfigFromFlash(void)
{
	BMS_CONFIG config;

	if (StorageFlash_LoadConfigData(&config) && EEPROM_ConfigIsValid(&config))
	{
		EEPROM_ApplyConfig(&config);
		System_ERROR_UserCallback(ERROR_REMOVE_EEPROM_STORE);
		return;
	}

	/* Defaults are already active. Missing, corrupt, out-of-range or old-format
	 * Config is intentionally not migrated: write one fresh current image. */
	if (!EEPROM_SaveConfigToFlash())
	{
		System_ERROR_UserCallback(ERROR_EEPROM_STORE);
	}
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
	return 1U;
#else
	UINT8 result = 1U;
	UINT8 config_dirty = 0U;

#if (!UPGRADE_PARAM_FORCE_REAPPLY)
	if (s_u16ConfigPolicyVersion == UPGRADE_PARAM_POLICY_VERSION)
	{
		return 1U;
	}
#endif

#if UPGRADE_PARAM_RESET_AFE
	EEPROM_LoadDefaultAfe();
	config_dirty = 1U;
#endif
#if UPGRADE_PARAM_RESET_PROTECT
	EEPROM_LoadDefaultProtect();
	config_dirty = 1U;
#endif
#if UPGRADE_PARAM_RESET_BALANCE_OPEN_VOLTAGE
	EEPROM_LoadDefaultBalanceOpenVoltage();
	config_dirty = 1U;
#endif
#if UPGRADE_PARAM_RESET_SOC_CONFIG
	EEPROM_LoadDefaultSocConfig();
	config_dirty = 1U;
#endif
#if UPGRADE_PARAM_UPDATE_OTHER_ELEMENT
	EEPROM_LoadDefaultOtherElement();
	config_dirty = 1U;
#endif

	if (config_dirty && !EEPROM_SaveConfigToFlash())
	{
		result = 0U;
	}
#if UPGRADE_PARAM_RESET_SOC_SNAPSHOT
	if (result && !SOC_ResetStoredSnapshotToDefault())
	{
		result = 0U;
	}
#endif
#if UPGRADE_PARAM_RESET_EVENT_RECORD
	if (result && !EEPROM_ResetData_EventRecord_ToDefault())
	{
		result = 0U;
	}
#endif
	if (!result)
	{
		return 0U;
	}

	s_u16ConfigPolicyVersion = UPGRADE_PARAM_POLICY_VERSION;
	if (!EEPROM_SaveConfigToFlash())
	{
		s_u16ConfigPolicyVersion = FLASH_UPGRADE_PARAM_FLAG_RESET;
		System_ERROR_UserCallback(ERROR_EEPROM_STORE);
		return 0U;
	}
	return 1U;
#endif
}

void InitE2PROM(void)
{
	EEPROM_LoadDefaultRuntimeData();
	EEPROM_LoadConfigFromFlash();
	ReadEEPROM_EventRecord_Parameters();
	UpgradeParamPolicy_ApplyOnce();
}
