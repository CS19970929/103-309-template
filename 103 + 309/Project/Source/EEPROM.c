#include "main.h"
#include "UpgradeParamPolicy.h"
#include "SocEnhance.h"
#include "BmsParamSchema.h"
#include "AfeParamAccess.h"

#if BMS_CONFIG_AFE_WORD_COUNT != AFE_PARAMETES_TOTAL_LENGTH
#error "BMS config AFE word count mismatch"
#endif
#if BMS_CONFIG_PROTECT_WORD_COUNT != E2P_PARA_NUM_PROTECT
#error "BMS config protect word count mismatch"
#endif
/* KB_NUM is an enum constant, so it is invisible to #if preprocessing.
 * Use a C constant-expression check instead; a mismatch creates an invalid
 * array bound and stops the ARMCC5 build. */
typedef char EEPROM_ConfigCalibrationWordCountCheck[
	(BMS_CONFIG_CALIB_WORD_COUNT == KB_NUM) ? 1 : -1];
#if BMS_CONFIG_OTHER_WORD_COUNT != E2P_PARA_NUM_OTHER_ELEMENT1
#error "BMS config other word count mismatch"
#endif
#if BMS_CONFIG_RESERVED_WORD_COUNT != E2P_PARA_NUM_RESERVED_RW_PARAM
#error "BMS config reserved word count mismatch"
#endif

typedef char EEPROM_ProtectLayoutCheck[
	(sizeof(struct PRT_E2ROM_PARAS) == (BMS_CONFIG_PROTECT_WORD_COUNT * sizeof(UINT16))) ? 1 : -1];
typedef char EEPROM_OtherLayoutCheck[
	(sizeof(struct OTHER_ELEMENT) == (BMS_CONFIG_OTHER_WORD_COUNT * sizeof(UINT16))) ? 1 : -1];

#define BMS_SH3673520_SERIES_MIN ((UINT16)5U)
#define BMS_SH3673520_SERIES_MAX ((UINT16)20U)
#define BMS_CS_RES_SCALE ((UINT32)1000U)

/* Single ROM instances shared by persistent storage and all host protocols. */
const UINT16 g_u16ProtectParamMin[E2P_PARA_NUM_PROTECT] = E2P_PROTECT_MIN_PRT;
const UINT16 g_u16ProtectParamDefault[E2P_PARA_NUM_PROTECT] = E2P_PROTECT_DEFAULT_PRT;
const UINT16 g_u16ProtectParamMax[E2P_PARA_NUM_PROTECT] = E2P_PROTECT_MAX_PRT;

const UINT16 g_u16OtherParamMin[E2P_PARA_NUM_OTHER_ELEMENT1] = OtherElement_min;
const UINT16 g_u16OtherParamDefault[E2P_PARA_NUM_OTHER_ELEMENT1] = OtherElement_default;
const UINT16 g_u16OtherParamMax[E2P_PARA_NUM_OTHER_ELEMENT1] = OtherElement_max;

static UINT16 s_u16ConfigPolicyVersion = FLASH_UPGRADE_PARAM_FLAG_RESET;

/* Config load/save and host edits are serialized by the main loop. Keep the
 * 482-byte persistent image out of the 3KB call stack and use it as the single
 * candidate transaction buffer. It is never accessed from interrupt context. */
static BMS_CONFIG s_stConfigScratch;

static UINT8 EEPROM_OtherRuntimeValuesAreValid(const UINT16 *other)
{
	UINT16 series_num;
	UINT16 cs_res;
	UINT16 cs_res_num;
	UINT32 current_ratio;

	if (other == 0)
	{
		return 0U;
	}

	series_num = other[BMS_OTHER_PARAM_WORD_INDEX(u16Sys_SeriesNum)];
	cs_res = other[BMS_OTHER_PARAM_WORD_INDEX(u16Sys_CS_Res)];
	cs_res_num = other[BMS_OTHER_PARAM_WORD_INDEX(u16Sys_CS_Res_Num)];

	if ((series_num < BMS_SH3673520_SERIES_MIN) ||
		(series_num > BMS_SH3673520_SERIES_MAX) ||
		(cs_res == 0U) ||
		(cs_res_num == 0U))
	{
		return 0U;
	}

	current_ratio = ((UINT32)cs_res_num * BMS_CS_RES_SCALE) / cs_res;
	return (current_ratio != 0U) ? 1U : 0U;
}

void BmsParam_ApplyRuntime(void)
{
	UINT32 current_ratio;

	if ((OtherElement.u16Sys_SeriesNum < BMS_SH3673520_SERIES_MIN) ||
		(OtherElement.u16Sys_SeriesNum > BMS_SH3673520_SERIES_MAX) ||
		(OtherElement.u16Sys_CS_Res == 0U) ||
		(OtherElement.u16Sys_CS_Res_Num == 0U))
	{
		System_ERROR_UserCallback(ERROR_EEPROM_STORE);
		return;
	}

	current_ratio = ((UINT32)OtherElement.u16Sys_CS_Res_Num * BMS_CS_RES_SCALE) /
		OtherElement.u16Sys_CS_Res;
	if (current_ratio == 0U)
	{
		System_ERROR_UserCallback(ERROR_EEPROM_STORE);
		return;
	}

	SeriesNum = (UINT8)OtherElement.u16Sys_SeriesNum;
	g_u32CS_Res_AFE = current_ratio;
}

static void EEPROM_LoadDefaultAfe(void)
{
	UINT16 i;

	for (i = 0U; i < AFE_PARAMETES_TOTAL_LENGTH; ++i)
	{
		AFE_Value_Typedef *param = AfeParam_At(i);
		param->curValue = param->defaultValue;
	}
	AFE_PARAM_WRITE_Flag = 1;
}

static void EEPROM_LoadDefaultProtect(void)
{
	memcpy(&PRT_E2ROMParas, g_u16ProtectParamDefault, sizeof(PRT_E2ROMParas));
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
	memcpy(&OtherElement, g_u16OtherParamDefault, sizeof(OtherElement));
	BmsParam_ApplyRuntime();
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

	memset(config, 0xFF, sizeof(*config));
	config->u16FormatVersion = FLASH_STORAGE_CONFIG_FORMAT_VERSION;
	config->u16AppliedPolicyVersion = s_u16ConfigPolicyVersion;

	for (i = 0U; i < BMS_CONFIG_AFE_WORD_COUNT; ++i)
	{
		config->afe[i] = AfeParam_AtConst(i)->curValue;
	}
	memcpy(config->protect, &PRT_E2ROMParas, sizeof(config->protect));
	for (i = 0U; i < BMS_CONFIG_CALIB_WORD_COUNT; ++i)
	{
		config->calibK[i] = g_u16CalibCoefK[i];
		config->calibB[i] = g_i16CalibCoefB[i];
	}
	memcpy(config->other, &OtherElement, sizeof(config->other));
}

static UINT8 EEPROM_ConfigAfeIsValid(const BMS_CONFIG *config)
{
	UINT16 i;

	for (i = 0U; i < BMS_CONFIG_AFE_WORD_COUNT; ++i)
	{
		if (!AfeParam_ValueIsValid(i, config->afe[i]))
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
									 g_u16ProtectParamMin,
									 g_u16ProtectParamMax,
									 BMS_CONFIG_PROTECT_WORD_COUNT))
	{
		return 0U;
	}
	if (!EEPROM_ConfigCalibrationIsValid(config))
	{
		return 0U;
	}
	if (!EEPROM_WordBlockInRange(config->other,
									 g_u16OtherParamMin,
									 g_u16OtherParamMax,
									 BMS_CONFIG_OTHER_WORD_COUNT))
	{
		return 0U;
	}
	if (!EEPROM_OtherRuntimeValuesAreValid(config->other))
	{
		return 0U;
	}
	return 1U;
}

void EEPROM_ConfigEditBegin(void)
{
	EEPROM_BuildConfig(&s_stConfigScratch);
}

UINT8 EEPROM_ConfigEditSetAfeWord(UINT16 index, UINT16 value)
{
	if (index >= BMS_CONFIG_AFE_WORD_COUNT)
	{
		return 0U;
	}
	s_stConfigScratch.afe[index] = value;
	return 1U;
}

UINT8 EEPROM_ConfigEditSetProtectWord(UINT16 index, UINT16 value)
{
	if (index >= BMS_CONFIG_PROTECT_WORD_COUNT)
	{
		return 0U;
	}
	s_stConfigScratch.protect[index] = value;
	return 1U;
}

UINT8 EEPROM_ConfigEditSetCalibPair(UINT16 index, UINT16 k_value, INT16 b_value)
{
	if (index >= BMS_CONFIG_CALIB_WORD_COUNT)
	{
		return 0U;
	}
	s_stConfigScratch.calibK[index] = k_value;
	s_stConfigScratch.calibB[index] = b_value;
	return 1U;
}

UINT8 EEPROM_ConfigEditSetOtherWord(UINT16 index, UINT16 value)
{
	if (index >= BMS_CONFIG_OTHER_WORD_COUNT)
	{
		return 0U;
	}
	s_stConfigScratch.other[index] = value;
	return 1U;
}

UINT8 EEPROM_ConfigEditCommit(void)
{
	UINT8 result;

	if (!EEPROM_ConfigIsValid(&s_stConfigScratch))
	{
		System_ERROR_UserCallback(ERROR_EEPROM_STORE);
		return 0U;
	}

	result = StorageFlash_SaveConfigData(&s_stConfigScratch);
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

static void EEPROM_ApplyConfig(const BMS_CONFIG *config)
{
	UINT16 i;

	for (i = 0U; i < BMS_CONFIG_AFE_WORD_COUNT; ++i)
	{
		AfeParam_At(i)->curValue = config->afe[i];
	}
	memcpy(&PRT_E2ROMParas, config->protect, sizeof(PRT_E2ROMParas));
	for (i = 0U; i < BMS_CONFIG_CALIB_WORD_COUNT; ++i)
	{
		g_u16CalibCoefK[i] = config->calibK[i];
		g_i16CalibCoefB[i] = config->calibB[i];
	}
	memcpy(&OtherElement, config->other, sizeof(OtherElement));

	s_u16ConfigPolicyVersion = config->u16AppliedPolicyVersion;
	AFE_PARAM_WRITE_Flag = 1;
	BmsParam_ApplyRuntime();
}

UINT8 EEPROM_SaveConfigToFlash(void)
{
	EEPROM_ConfigEditBegin();
	return EEPROM_ConfigEditCommit();
}

static void EEPROM_LoadConfigFromFlash(void)
{
	BMS_CONFIG *config = &s_stConfigScratch;

	if (StorageFlash_LoadConfigData(config) && EEPROM_ConfigIsValid(config))
	{
		EEPROM_ApplyConfig(config);
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
	OtherElement.u16Soc_TableSelect =
		g_u16OtherParamDefault[BMS_OTHER_PARAM_WORD_INDEX(u16Soc_TableSelect)];
	OtherElement.u16Soc_Ah =
		g_u16OtherParamDefault[BMS_OTHER_PARAM_WORD_INDEX(u16Soc_Ah)];
	OtherElement.u16Soc_Cycle_times =
		g_u16OtherParamDefault[BMS_OTHER_PARAM_WORD_INDEX(u16Soc_Cycle_times)];
	OtherElement.u16Soc_V_100 =
		g_u16OtherParamDefault[BMS_OTHER_PARAM_WORD_INDEX(u16Soc_V_100)];
	OtherElement.u16Soc_V_0 =
		g_u16OtherParamDefault[BMS_OTHER_PARAM_WORD_INDEX(u16Soc_V_0)];
}
#endif

#if UPGRADE_PARAM_POLICY_ENABLE && UPGRADE_PARAM_RESET_BALANCE_OPEN_VOLTAGE
static void EEPROM_LoadDefaultBalanceOpenVoltage(void)
{
	OtherElement.u16Balance_OpenVoltage =
		g_u16OtherParamDefault[BMS_OTHER_PARAM_WORD_INDEX(u16Balance_OpenVoltage)];
}
#endif

UINT8 UpgradeParamPolicy_ApplyOnce(void)
{
#if (!UPGRADE_PARAM_POLICY_ENABLE) || (!UPGRADE_PARAM_POLICY_HAS_ACTION)
	return 1U;
#else
	UINT16 i;

#if (!UPGRADE_PARAM_FORCE_REAPPLY)
	if (s_u16ConfigPolicyVersion == UPGRADE_PARAM_POLICY_VERSION)
	{
		return 1U;
	}
#endif

#if (UPGRADE_PARAM_RESET_AFE || UPGRADE_PARAM_RESET_PROTECT || \
	 UPGRADE_PARAM_RESET_BALANCE_OPEN_VOLTAGE || UPGRADE_PARAM_RESET_SOC_CONFIG || \
	 UPGRADE_PARAM_UPDATE_OTHER_ELEMENT)
	/* Build all persistent parameter changes in the candidate image first.
	 * Runtime remains untouched if candidate validation or Flash commit fails. */
	EEPROM_ConfigEditBegin();
#if UPGRADE_PARAM_RESET_AFE
	for (i = 0U; i < BMS_CONFIG_AFE_WORD_COUNT; ++i)
	{
		s_stConfigScratch.afe[i] = AfeParam_AtConst(i)->defaultValue;
	}
#endif
#if UPGRADE_PARAM_RESET_PROTECT
	memcpy(s_stConfigScratch.protect,
		   g_u16ProtectParamDefault,
		   sizeof(s_stConfigScratch.protect));
#endif
#if UPGRADE_PARAM_RESET_BALANCE_OPEN_VOLTAGE
	s_stConfigScratch.other[BMS_OTHER_PARAM_WORD_INDEX(u16Balance_OpenVoltage)] =
		g_u16OtherParamDefault[BMS_OTHER_PARAM_WORD_INDEX(u16Balance_OpenVoltage)];
#endif
#if UPGRADE_PARAM_RESET_SOC_CONFIG
	s_stConfigScratch.other[BMS_OTHER_PARAM_WORD_INDEX(u16Soc_TableSelect)] =
		g_u16OtherParamDefault[BMS_OTHER_PARAM_WORD_INDEX(u16Soc_TableSelect)];
	s_stConfigScratch.other[BMS_OTHER_PARAM_WORD_INDEX(u16Soc_Ah)] =
		g_u16OtherParamDefault[BMS_OTHER_PARAM_WORD_INDEX(u16Soc_Ah)];
	s_stConfigScratch.other[BMS_OTHER_PARAM_WORD_INDEX(u16Soc_Cycle_times)] =
		g_u16OtherParamDefault[BMS_OTHER_PARAM_WORD_INDEX(u16Soc_Cycle_times)];
	s_stConfigScratch.other[BMS_OTHER_PARAM_WORD_INDEX(u16Soc_V_100)] =
		g_u16OtherParamDefault[BMS_OTHER_PARAM_WORD_INDEX(u16Soc_V_100)];
	s_stConfigScratch.other[BMS_OTHER_PARAM_WORD_INDEX(u16Soc_V_0)] =
		g_u16OtherParamDefault[BMS_OTHER_PARAM_WORD_INDEX(u16Soc_V_0)];
#endif
#if UPGRADE_PARAM_UPDATE_OTHER_ELEMENT
	memcpy(s_stConfigScratch.other,
		   g_u16OtherParamDefault,
		   sizeof(s_stConfigScratch.other));
#endif
	if (!EEPROM_ConfigEditCommit())
	{
		return 0U;
	}

	/* Persistent commit succeeded. Apply the exact same policy to runtime. */
#if UPGRADE_PARAM_RESET_AFE
	EEPROM_LoadDefaultAfe();
#endif
#if UPGRADE_PARAM_RESET_PROTECT
	EEPROM_LoadDefaultProtect();
#endif
#if UPGRADE_PARAM_RESET_BALANCE_OPEN_VOLTAGE
	EEPROM_LoadDefaultBalanceOpenVoltage();
#endif
#if UPGRADE_PARAM_RESET_SOC_CONFIG
	EEPROM_LoadDefaultSocConfig();
#endif
#if UPGRADE_PARAM_UPDATE_OTHER_ELEMENT
	EEPROM_LoadDefaultOtherElement();
#endif
#else
	(void)i;
#endif

#if UPGRADE_PARAM_RESET_SOC_SNAPSHOT
	if (!SOC_ResetStoredSnapshotToDefault())
	{
		return 0U;
	}
#endif
#if UPGRADE_PARAM_RESET_EVENT_RECORD
	if (!EEPROM_ResetData_EventRecord_ToDefault())
	{
		return 0U;
	}
#endif

	/* Mark the policy complete only after every requested reset succeeded. */
	EEPROM_ConfigEditBegin();
	s_stConfigScratch.u16AppliedPolicyVersion = UPGRADE_PARAM_POLICY_VERSION;
	if (!EEPROM_ConfigEditCommit())
	{
		return 0U;
	}
	s_u16ConfigPolicyVersion = UPGRADE_PARAM_POLICY_VERSION;
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
