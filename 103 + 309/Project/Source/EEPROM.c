#include "main.h"
#include "SOC.h"
#include "Heat_Cool.h"
#include <stddef.h>

struct HEAT_COOL_ELEMENT Heat_Cool_Element;

typedef char EEPROM_HeatCoolSizeCheck[(sizeof(struct HEAT_COOL_ELEMENT) ==
	FLASH_STORAGE_RW_PARAM_HEAT_COOL_WORD_COUNT * sizeof(UINT16)) ? 1 : -1];

#if FLASH_STORAGE_RW_PARAM_PROTECT_WORD_COUNT != E2P_PARA_NUM_PROTECT
#error "RW parameter protect word count mismatch"
#endif
#if FLASH_STORAGE_RW_PARAM_OTHER_WORD_COUNT != E2P_PARA_NUM_OTHER_ELEMENT1
#error "RW parameter other word count mismatch"
#endif
#if FLASH_STORAGE_RW_PARAM_RESERVED_WORD_COUNT != E2P_PARA_NUM_RESERVED_RW_PARAM
#error "RW parameter reserved word count mismatch"
#endif

typedef char EEPROM_ParamVersionSizeCheck[(STORAGE_PARAM_VERSION_FIRST+STORAGE_PARAM_COUNT
    <=FLASH_STORAGE_RW_PARAM_RESERVED_WORD_COUNT) ? 1 : -1];

static const UINT16 s_param_versions[STORAGE_PARAM_COUNT] = {
    PROJECT_CFG_PARAM_AFE_VERSION, PROJECT_CFG_PARAM_PROTECT_VERSION,
    PROJECT_CFG_PARAM_SOC_VERSION, PROJECT_CFG_PARAM_OTHER_VERSION,
    PROJECT_CFG_PARAM_CALIB_VERSION, PROJECT_CFG_PARAM_HEAT_COOL_VERSION,
    PROJECT_CFG_PARAM_SOC_STATE_VERSION, PROJECT_CFG_PARAM_AGING_STATE_VERSION,
    PROJECT_CFG_PARAM_LOG_VERSION
};

/*
 * Store current K/B in existing reserved words so the RW parameter record size
 * includes both charge and discharge calibration.
 */
#define RW_PARAM_CURRENT_CAL_MAGIC_INDEX       ((UINT16)0U)
#define RW_PARAM_CURRENT_CAL_VERSION_INDEX     ((UINT16)1U)
#define RW_PARAM_CURRENT_CAL_CHG_K_INDEX       ((UINT16)2U)
#define RW_PARAM_CURRENT_CAL_CHG_B_INDEX       ((UINT16)3U)
#define RW_PARAM_CURRENT_CAL_DSG_K_INDEX       ((UINT16)4U)
#define RW_PARAM_CURRENT_CAL_DSG_B_INDEX       ((UINT16)5U)
#define RW_PARAM_CURRENT_CAL_MAGIC             ((UINT16)0x4349U)
#define RW_PARAM_CURRENT_CAL_VERSION           ((UINT16)3U)

static UINT8 EEPROM_CurrentCalibValuesValid(UINT16 chg_k, INT16 chg_b,
                                            UINT16 dsg_k, INT16 dsg_b)
{
	if ((chg_k < CURRENT_CAL_K_MIN) || (chg_k > CURRENT_CAL_K_MAX) ||
		(dsg_k < CURRENT_CAL_K_MIN) || (dsg_k > CURRENT_CAL_K_MAX))
	{
		return 0U;
	}
	if ((chg_b < CURRENT_CAL_B_MIN_Q10_A) || (chg_b > CURRENT_CAL_B_MAX_Q10_A) ||
		(dsg_b < CURRENT_CAL_B_MIN_Q10_A) || (dsg_b > CURRENT_CAL_B_MAX_Q10_A))
	{
		return 0U;
	}
	return 1U;
}

static UINT8 EEPROM_RWParamHasCurrentCalib(const STORAGE_FLASH_RW_PARAM_DATA *data)
{
	return (UINT8)((data->reserved[RW_PARAM_CURRENT_CAL_MAGIC_INDEX] == RW_PARAM_CURRENT_CAL_MAGIC) &&
				   (data->reserved[RW_PARAM_CURRENT_CAL_VERSION_INDEX] == RW_PARAM_CURRENT_CAL_VERSION));
}

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

static void EEPROM_LoadDefaultHeatCool(void)
{
	const struct HEAT_COOL_ELEMENT heat_cool_default = HeatCoolElement_Default;
	Heat_Cool_Element = heat_cool_default;
}

static void EEPROM_BuildRWParamData(STORAGE_FLASH_RW_PARAM_DATA *data)
{
	UINT16 i;

	for (i = 0; i < E2P_PARA_NUM_PROTECT; ++i)
	{
		data->protect[i] = *(&PRT_E2ROMParas.u16VcellOvp_First + i);
	}
	for (i = 0; i < E2P_PARA_NUM_OTHER_ELEMENT1; ++i)
	{
		data->other[i] = *(&OtherElement.u16Balance_OpenVoltage + i);
	}
	for (i = 0; i < E2P_PARA_NUM_RESERVED_RW_PARAM; ++i)
	{
		data->reserved[i] = 0xFFFFU;
	}
	for (i = 0; i < FLASH_STORAGE_RW_PARAM_HEAT_COOL_WORD_COUNT; ++i)
	{
		data->heat_cool[i] = *(&Heat_Cool_Element.u16Heat_OpenTemp + i);
	}

	data->reserved[RW_PARAM_CURRENT_CAL_MAGIC_INDEX] = RW_PARAM_CURRENT_CAL_MAGIC;
	data->reserved[RW_PARAM_CURRENT_CAL_VERSION_INDEX] = RW_PARAM_CURRENT_CAL_VERSION;
	data->reserved[RW_PARAM_CURRENT_CAL_CHG_K_INDEX] = g_u16CalibCoefK[MDL_ICHG];
	data->reserved[RW_PARAM_CURRENT_CAL_CHG_B_INDEX] = (UINT16)g_i16CalibCoefB[MDL_ICHG];
	data->reserved[RW_PARAM_CURRENT_CAL_DSG_K_INDEX] = g_u16CalibCoefK[MDL_IDSG];
	data->reserved[RW_PARAM_CURRENT_CAL_DSG_B_INDEX] = (UINT16)g_i16CalibCoefB[MDL_IDSG];
}

static UINT8 EEPROM_WordBlockInRange(const UINT16 *values, const UINT16 *min_values, const UINT16 *max_values, UINT16 count)
{
	UINT16 i;

	for (i = 0; i < count; ++i)
	{
		if ((values[i] < min_values[i]) || (values[i] > max_values[i]))
		{
			return 0;
		}
	}

	return 1;
}

static UINT8 EEPROM_RWParamDataIsValid(const STORAGE_FLASH_RW_PARAM_DATA *data)
{
	const struct PRT_E2ROM_PARAS protect_min = E2P_PROTECT_MIN_PRT;
	const struct PRT_E2ROM_PARAS protect_max = E2P_PROTECT_MAX_PRT;
	const struct OTHER_ELEMENT other_min = OtherElement_min;
	const struct OTHER_ELEMENT other_max = OtherElement_max;
	const struct HEAT_COOL_ELEMENT heat_cool_min = HeatCoolElement_Min;
	const struct HEAT_COOL_ELEMENT heat_cool_max = HeatCoolElement_Max;
	if (!EEPROM_WordBlockInRange(data->protect,
								 &protect_min.u16VcellOvp_First,
								 &protect_max.u16VcellOvp_First,
								 E2P_PARA_NUM_PROTECT))
	{
		return 0;
	}
	if (!EEPROM_WordBlockInRange(data->other,
								 &other_min.u16Balance_OpenVoltage,
								 &other_max.u16Balance_OpenVoltage,
								 E2P_PARA_NUM_OTHER_ELEMENT1))
	{
		return 0;
	}
	if (!EEPROM_WordBlockInRange(data->heat_cool,
								 &heat_cool_min.u16Heat_OpenTemp,
								 &heat_cool_max.u16Heat_OpenTemp,
								 FLASH_STORAGE_RW_PARAM_HEAT_COOL_WORD_COUNT))
	{
		return 0;
	}

	if (!EEPROM_RWParamHasCurrentCalib(data) ||
		!EEPROM_CurrentCalibValuesValid(
			data->reserved[RW_PARAM_CURRENT_CAL_CHG_K_INDEX],
			(INT16)data->reserved[RW_PARAM_CURRENT_CAL_CHG_B_INDEX],
			data->reserved[RW_PARAM_CURRENT_CAL_DSG_K_INDEX],
			(INT16)data->reserved[RW_PARAM_CURRENT_CAL_DSG_B_INDEX]))
	{
		return 0;
	}
	return 1;
}

static void EEPROM_ApplyRWParamData(const STORAGE_FLASH_RW_PARAM_DATA *data)
{
	UINT16 i;

	for (i = 0; i < E2P_PARA_NUM_PROTECT; ++i)
	{
		*(&PRT_E2ROMParas.u16VcellOvp_First + i) = data->protect[i];
	}
	for (i = 0; i < E2P_PARA_NUM_OTHER_ELEMENT1; ++i)
	{
		*(&OtherElement.u16Balance_OpenVoltage + i) = data->other[i];
	}
	for (i = 0; i < FLASH_STORAGE_RW_PARAM_HEAT_COOL_WORD_COUNT; ++i)
	{
		*(&Heat_Cool_Element.u16Heat_OpenTemp + i) = data->heat_cool[i];
	}

	g_u16CalibCoefK[MDL_ICHG] = data->reserved[RW_PARAM_CURRENT_CAL_CHG_K_INDEX];
	g_i16CalibCoefB[MDL_ICHG] = (INT16)data->reserved[RW_PARAM_CURRENT_CAL_CHG_B_INDEX];
	g_u16CalibCoefK[MDL_IDSG] = data->reserved[RW_PARAM_CURRENT_CAL_DSG_K_INDEX];
	g_i16CalibCoefB[MDL_IDSG] = (INT16)data->reserved[RW_PARAM_CURRENT_CAL_DSG_B_INDEX];

	EEPROM_UpdateOtherElementRuntime();
}

UINT8 EEPROM_SaveRWParametersToFlash(UINT8 cycle_count_written)
{
	STORAGE_FLASH_RW_PARAM_DATA data;

	EEPROM_BuildRWParamData(&data);
	if (!EEPROM_RWParamDataIsValid(&data))
	{
		System_ERROR_UserCallback(ERROR_EEPROM_STORE);
		return 0;
	}

	return StorageFlash_SaveRwParamData(&data,cycle_count_written);
}

static void EEPROM_LoadDefaultRuntimeData(void)
{
	EEPROM_LoadDefaultProtect();
	EEPROM_LoadDefaultCalib();
	EEPROM_LoadDefaultOtherElement();
	EEPROM_LoadDefaultHeatCool();

	System_ERROR_UserCallback(ERROR_REMOVE_EEPROM_COM);
}


typedef char EEPROM_SocCycleOffsetCheck[(offsetof(struct OTHER_ELEMENT,u16Soc_Cycle_times)/
    sizeof(UINT16)==STORAGE_SOC_CYCLE_PARAMETER_INDEX) ? 1 : -1];

static UINT8 EEPROM_IsSocParameter(UINT16 word)
{
    return (UINT8)(word==offsetof(struct OTHER_ELEMENT,u16Soc_TableSelect)/sizeof(UINT16) ||
        (word>=offsetof(struct OTHER_ELEMENT,u16Soc_Ah)/sizeof(UINT16) &&
         word<=offsetof(struct OTHER_ELEMENT,u16Soc_V_0)/sizeof(UINT16)));
}

static UINT8 EEPROM_UpdateModules(STORAGE_FLASH_CONFIG_DATA *stored,
                                 const STORAGE_FLASH_CONFIG_DATA *defaults)
{
    UINT16 group, word;
    UINT8 changed=0U;
    for (group=0U; group<STORAGE_PARAM_COUNT; ++group) {
        if (!s_param_versions[group] ||
            stored->rw.reserved[STORAGE_PARAM_VERSION_FIRST+group]==s_param_versions[group]) continue;
        switch (group) {
        case STORAGE_PARAM_AFE:
            memcpy(stored->afe,defaults->afe,sizeof(stored->afe)); break;
        case STORAGE_PARAM_PROTECT:
            memcpy(stored->rw.protect,defaults->rw.protect,sizeof(stored->rw.protect)); break;
        case STORAGE_PARAM_SOC:
        case STORAGE_PARAM_OTHER:
            for (word=0U; word<FLASH_STORAGE_RW_PARAM_OTHER_WORD_COUNT; ++word)
                if (EEPROM_IsSocParameter(word)==(group==STORAGE_PARAM_SOC))
                    stored->rw.other[word]=defaults->rw.other[word];
            break;
        case STORAGE_PARAM_CALIB:
            memcpy(stored->rw.reserved,defaults->rw.reserved,STORAGE_PARAM_VERSION_FIRST*sizeof(UINT16)); break;
        case STORAGE_PARAM_HEAT_COOL:
            memcpy(stored->rw.heat_cool,defaults->rw.heat_cool,sizeof(stored->rw.heat_cool)); break;
        case STORAGE_PARAM_SOC_STATE: stored->initializing |= STORAGE_PARAM_RESET_SOC; break;
        case STORAGE_PARAM_AGING_STATE: stored->initializing |= STORAGE_PARAM_RESET_AGING; break;
        case STORAGE_PARAM_LOG: stored->initializing |= STORAGE_PARAM_RESET_LOG; break;
        }
        stored->rw.reserved[STORAGE_PARAM_VERSION_FIRST+group]=s_param_versions[group];
        changed=1U;
    }
    return (UINT8)(!changed || StorageFlash_CommitParameterUpdate(stored));
}

UINT8 InitE2PROM(void)
{
    STORAGE_FLASH_CONFIG_DATA defaults, stored;
    UINT16 i;
    UINT8 valid;
    const AFE_Value_Typedef *afe=&AFE_Parameters_RS485_Struction.u16VcellOvp;
    static const UINT8 version[32]=BMS_SOFTWARE_VERDION_DEFAULT;
    typedef char VersionFitsConfig[(sizeof(BMS_SOFTWARE_VERDION_DEFAULT)<=sizeof(version)) ? 1 : -1];

    EEPROM_LoadDefaultRuntimeData();
    memset(&defaults,0,sizeof(defaults));
    EEPROM_BuildRWParamData(&defaults.rw);
    for (i=0U; i<STORAGE_PARAM_COUNT; ++i)
        defaults.rw.reserved[STORAGE_PARAM_VERSION_FIRST+i]=s_param_versions[i];
    for (i=0; i<FLASH_STORAGE_AFE_WORD_COUNT; ++i) defaults.afe[i]=afe[i].defaultValue;
    memcpy(defaults.reset_version,version,sizeof(version));
    if (!EEPROM_RWParamDataIsValid(&defaults.rw)) return 0U;
    for (i=0; i<FLASH_STORAGE_AFE_WORD_COUNT; ++i)
        if (defaults.afe[i]<afe[i].minValue || defaults.afe[i]>afe[i].maxValue) return 0U;
    valid=StorageFlash_LoadConfig(&stored);
    if (valid) {
        valid=(UINT8)((stored.initializing & ~STORAGE_PARAM_RESET_MASK)==0U && EEPROM_RWParamDataIsValid(&stored.rw));
        for (i=0; i<FLASH_STORAGE_AFE_WORD_COUNT; ++i)
            if (stored.afe[i]<afe[i].minValue || stored.afe[i]>afe[i].maxValue) valid=0U;
    }
    /* Finish a committed intent even when this firmware disables that group. */
    if (valid && stored.initializing && !StorageFlash_CommitParameterUpdate(&stored)) goto failed;
    if (valid && !StorageFlash_ResumeSocCycleWrite(&stored)) goto failed;
    if (!valid || (PROJECT_CFG_RESET_PARAMS_ON_NEW_VERSION &&
                  memcmp(stored.reset_version,version,sizeof(version))!=0)) {
        defaults.initializing=STORAGE_PARAM_RESET_ALL;
        if (!StorageFlash_CommitParameterUpdate(&defaults)) goto failed;
        stored=defaults;
    }
    else if (!EEPROM_UpdateModules(&stored,&defaults)) goto failed;
    EEPROM_ApplyRWParamData(&stored.rw);
    for (i=0; i<FLASH_STORAGE_AFE_WORD_COUNT; ++i)
        (&AFE_Parameters_RS485_Struction.u16VcellOvp)[i].curValue=stored.afe[i];
    ReadEEPROM_EventRecord_Parameters();
    return 1U;
failed:
    System_ERROR_UserCallback(ERROR_EEPROM_STORE);
    return 0U;
}
