#include "SocEnhance.h"
#include "PubFunc.h"
#include "conf.h"
#include "EEPROM.h"
#include "DataDeal.h"
#include "Flash.h"
#include "Sci_Upper.h"
#include "System_Monitor.h"
#include <string.h>
#include <stdint.h>

#ifndef STORAGE_FLASH_SOC_API_DECLARED
typedef struct
{
	UINT16 u16FormatVersion;
	UINT16 u16SocNow;
	UINT16 u16DsgSocInt;
	UINT16 u16MaxErrorPercent;
	UINT32 u32CycleTimes;
	UINT32 u32CapNow;
	UINT32 u32CapFull;
	UINT32 u32LearnPassedAs10;
	UINT16 u16LearnAnchorSoc;
	UINT16 u16LearnState;
	UINT16 u16Flags;
	UINT16 u16Reserved[4];
} STORAGE_FLASH_SOC_DATA;

extern UINT8 StorageFlash_LoadSocData(STORAGE_FLASH_SOC_DATA *data);
extern UINT8 StorageFlash_SaveSocData(const STORAGE_FLASH_SOC_DATA *data);
#endif

#define SOC_INTEGRATE_PERIOD_MS              ((UINT32)200U)
#define SOC_INTEGRATE_MS_PER_SECOND          ((UINT32)1000U)
#define SOC_TICKS_PER_SECOND                 (SOC_INTEGRATE_MS_PER_SECOND / SOC_INTEGRATE_PERIOD_MS)
#define SOC_CURRENT_ENTER_A10                ((UINT16)4U)
#define SOC_DISPLAY_STEP_SECONDS             ((UINT16)1U)
#define SOC_RELAX_ENTRY_SECONDS              ((UINT16)5U)
#define SOC_FULL_CONFIRM_SECONDS             ((UINT16)60U)
#define SOC_FULL_CONFIRM_MIN_CELL_MARGIN_MV  ((UINT16)PROJECT_CFG_SOC_FULL_CONFIRM_MIN_CELL_MARGIN_MV)
#define SOC_FULL_CONFIRM_MAX_CELL_DELTA_MV   ((UINT16)PROJECT_CFG_SOC_FULL_CONFIRM_MAX_CELL_DELTA_MV)
#define SOC_CALIBRATION_MIN_CELL_VALID_MV    ((UINT16)PROJECT_CFG_SOC_CALIBRATION_MIN_CELL_VALID_MV)
#define SOC_CALIBRATION_MAX_CELL_VALID_MV    ((UINT16)PROJECT_CFG_SOC_CALIBRATION_MAX_CELL_VALID_MV)
#define SOC_CALIBRATION_MAX_CELL_DELTA_MV    ((UINT16)PROJECT_CFG_SOC_CALIBRATION_MAX_CELL_DELTA_MV)
#define SOC_CALIBRATION_BLOCK_PROTECTION_FAULT ((UINT8)PROJECT_CFG_SOC_CALIBRATION_BLOCK_PROTECTION_FAULT)
#define SOC_CALIBRATION_BLOCK_SYSTEM_FAULT   ((UINT8)PROJECT_CFG_SOC_CALIBRATION_BLOCK_SYSTEM_FAULT)
#define SOC_DEFAULT_MAX_ERROR_PERCENT        ((UINT16)100U)
#define SOC_CYCLE_PERCENT_PER_COUNT          ((UINT16)80U)
#define SOC_MODE_RELAX                       ((UINT8)0U)
#define SOC_MODE_CHG                         ((UINT8)1U)
#define SOC_MODE_DSG                         ((UINT8)2U)
#define SOC_WEAK_CELL_GUARD_WINDOW_MV        ((UINT16)120U)
#define SOC_WEAK_CELL_CRITICAL_WINDOW_MV     ((UINT16)30U)
#define SOC_REST_CORRECT_MIN_SECONDS         ((UINT32)1800U)
#define SOC_REST_CORRECT_MAX_STEP_PERCENT    ((UINT8)3U)
#define SOC_CHG_HIGH_CATCHUP_SECONDS         ((UINT16)20U)
#define SOC_CHG_HIGH_CATCHUP_TARGET          ((UINT8)95U)

enum EEPROM_COMMAND
{
	EEPROM_DATA_REFRESH = 0,
	EEPROM_DATA_READ
};

struct SOC_CALCULATE_ELEMENT
{
	UINT32 u32CapFactory;
	UINT32 u32CapFull;
	UINT32 u32CapNow;
	UINT32 u32IntegrateRemainderMs;
	UINT32 u32Cycle_times;
	UINT16 u16MaxErrorPercent;
	UINT8 u8SOC_Now;
	UINT8 u8DSG_SOC_Int;
};

struct SOC_RUNTIME_CONTEXT
{
	UINT8 u8Mode;
	UINT8 u8DisplaySoc;
	UINT8 u8DisplayReady;
	UINT8 u8DisplayStepTicks;
	UINT16 u16RelaxEntryTicks;
	UINT16 u16FullConfirmTicks;
	UINT16 u16EmptyConfirmTicks;
	UINT16 u16HighChargeTicks;
};

struct SOC_ENHANCE_ELEMENT SOC_Enhance_Element;
struct SOC_CALCULATE_ELEMENT SOC_Calculate_Element;
struct SOC_CALCULATE_ELEMENT SOC_Calculate_Element_backup;

static struct SOC_RUNTIME_CONTEXT g_soc_runtime;

static void SOC_ResetRuntimeContext(void);
static void SOC_LoadFactoryRuntimeConfig(void);
static void SOC_ResetCalculateState(void);
static void SOC_LoadDefaultSnapshot(void);
static UINT8 SOC_DealEEPROM_Data(enum EEPROM_COMMAND command);
static UINT32 SOC_GetCapBase(void);
static UINT8 SOC_CalcSocFromCap(UINT32 cap_now, UINT32 cap_base);
static void SOC_SetSocValue(UINT8 soc_now, UINT8 reset_integrator);
static void SOC_ApplySocNow(UINT8 soc_now);
static void SOC_ApplySocCorrection(UINT8 soc_now);
static UINT8 SOC_IsVoltageValid(void);
static UINT16 SOC_GetCellDeltaMv(void);
static UINT8 SOC_IsCalibrationVoltageValid(void);
static UINT8 SOC_HasBlockingProtectionFault(void);
static UINT8 SOC_HasBlockingSystemFault(void);
static UINT8 SOC_IsCalibrationAllowed(void);
static const UINT16 *SOC_GetSelectedOcvTable(UINT16 *table_size);
static UINT8 Get_OpenCircuit_Value(void);
static UINT8 SOC_GetMeasuredCurrentDirection(void);
static UINT8 SOC_GetCurrentDirection(void);
static UINT8 SOC_ApplyCapacityDelta(enum _CUR current_type, UINT16 current);
static void SOC_AddDischargeCyclePercent(UINT8 delta_soc);
static void SOC_RunCoulombCounter(UINT8 direction);
static UINT16 SOC_GetFullCellConfirmVoltage(void);
static UINT8 SOC_IsFullConfirmCellDeltaValid(void);
static UINT8 SOC_StepTowardTarget(UINT8 current_soc, UINT8 target_soc, UINT8 max_step);
static void SOC_ApplyChargeCatchup(UINT8 direction);
static void SOC_ApplyFullEmptyAnchor(UINT8 direction);
static void SOC_ApplyWeakCellGuard(void);
static void SOC_ApplyRestOcvCorrection(UINT32 rest_seconds);
static void SOC_UpdateDisplaySoc(void);
static UINT16 SOC_CapAs10ToAh100(UINT32 cap_as10);
static void SOC_SyncOutputData(UINT8 force_display_follow);
static void SOC_PersistSnapshotIfChanged(void);
static void SOC_RefreshData_Monitor(void);
static void SOC_Update_StartUp(void);
static UINT8 isCHG(void);
static UINT8 isDSG(void);

const UINT16 SOC_Table_LiFePO[SOC_Size_LiFePO] = {
	3336, 100, 3332, 90, 3330, 80, 3327, 75, 3316, 70, 3301, 65,
	3294, 60, 3291, 55, 3290, 50, 3288, 45, 3286, 40, 3279, 35,
	3266, 30, 3254, 25, 3236, 20, 3212, 15, 3198, 10, 3112, 5,
	2526, 0, 1000, 0, 1000, 0,
};

const UINT16 SocTable_TernaryLi[SOC_Size_TernaryLi] = {
	4126, 100, 4066, 95, 4011, 90, 3955, 85, 3888, 80, 3837, 75,
	3793, 70, 3756, 65, 3724, 60, 3699, 55, 3675, 50, 3658, 45,
	3632, 40, 3605, 35, 3584, 30, 3557, 25, 3535, 20, 3497, 15,
	3475, 10, 3371, 5, 3136, 3,
};

const UINT16 SocTable_LiFePO2[SOC_Size_LiFePO2] = {
	3650, 100, 3600, 98, 3550, 95, 3500, 92, 3400, 90, 3350, 87,
	3340, 85, 3335, 82, 3330, 80, 3325, 78, 3320, 75, 3300, 70,
	3275, 65, 3250, 60, 3200, 50, 3150, 45, 3100, 30, 3000, 20,
	2850, 10, 2750, 5, 2650, 0,
};

static void SOC_ResetRuntimeContext(void)
{
	memset(&g_soc_runtime, 0, sizeof(g_soc_runtime));
	g_soc_runtime.u8Mode = SOC_MODE_RELAX;
}

void SOC_UpdateSampleData(UINT16 vcell_max, UINT16 vcell_min, UINT16 ichg, UINT16 idsg)
{
	SOC_Enhance_Element.u16_VCellMax = vcell_max;
	SOC_Enhance_Element.u16_VCellMin = vcell_min;
	SOC_Enhance_Element.u16_Ichg = ichg;
	SOC_Enhance_Element.u16_Idsg = idsg;
}

void SOC_PublishReportData(void)
{
	g_stCellInfoReport.SocElement.u16Soc = SOC_Enhance_Element.u8_SOC;
	g_stCellInfoReport.SocElement.u16Soh = SOC_Enhance_Element.u8_SOH;
	g_stCellInfoReport.SocElement.u16CapacityNow = SOC_Enhance_Element.u16_CapacityNow;
	g_stCellInfoReport.SocElement.u16CapacityFull = SOC_Enhance_Element.u16_CapacityFull;
	g_stCellInfoReport.SocElement.u16CapacityFactory = SOC_Enhance_Element.u16_CapacityFactory;
	g_stCellInfoReport.SocElement.u16Cycle_times = SOC_Enhance_Element.u16_Cycle_times;
}

static void SOC_LoadFactoryRuntimeConfig(void)
{
	SOC_Calculate_Element.u32CapFactory = (UINT32)SOC_Enhance_Element.u16_SOC_Ah * 3600U;
	SOC_Calculate_Element.u32CapFull = SOC_Calculate_Element.u32CapFactory;
	SOC_Calculate_Element.u32Cycle_times = (UINT32)SOC_Enhance_Element.u16_SOC_CycleT_Ever * 100U;
}

static void SOC_ResetCalculateState(void)
{
	memset(&SOC_Calculate_Element, 0, sizeof(SOC_Calculate_Element));
}

static UINT32 SOC_GetCapBase(void)
{
	return (SOC_Calculate_Element.u32CapFull != 0U) ?
		SOC_Calculate_Element.u32CapFull : SOC_Calculate_Element.u32CapFactory;
}

static UINT8 SOC_CalcSocFromCap(UINT32 cap_now, UINT32 cap_base)
{
	UINT32 soc;

	if (cap_base == 0U)
	{
		return 0U;
	}
	if (cap_now >= cap_base)
	{
		return 100U;
	}

	soc = (UINT32)(((uint64_t)cap_now * 100ULL + ((uint64_t)cap_base / 2ULL)) / (uint64_t)cap_base);
	if (soc > 100U)
	{
		soc = 100U;
	}

	return (UINT8)soc;
}

static void SOC_SetSocValue(UINT8 soc_now, UINT8 reset_integrator)
{
	UINT32 cap_base;

	if (soc_now > 100U)
	{
		soc_now = 100U;
	}

	cap_base = SOC_GetCapBase();
	SOC_Calculate_Element.u8SOC_Now = soc_now;
	SOC_Calculate_Element.u32CapNow = (UINT32)(((uint64_t)soc_now * (uint64_t)cap_base) / 100ULL);
	if (reset_integrator)
	{
		SOC_Calculate_Element.u32IntegrateRemainderMs = 0U;
	}
}

static void SOC_ApplySocNow(UINT8 soc_now)
{
	SOC_SetSocValue(soc_now, 1U);
}

static void SOC_ApplySocCorrection(UINT8 soc_now)
{
	SOC_SetSocValue(soc_now, 1U);
}

static void SOC_LoadDefaultSnapshot(void)
{
	SOC_Calculate_Element.u8SOC_Now = SOC_DEFAULT_STARTUP_PERCENT;
	SOC_Calculate_Element.u8DSG_SOC_Int = 0U;
	SOC_Calculate_Element.u32Cycle_times = (UINT32)SOC_Enhance_Element.u16_SOC_CycleT_Ever * 100U;
	SOC_Calculate_Element.u32CapFull = SOC_Calculate_Element.u32CapFactory;
	SOC_Calculate_Element.u32CapNow = (UINT32)SOC_Calculate_Element.u8SOC_Now * SOC_Calculate_Element.u32CapFactory / 100U;
	SOC_Calculate_Element.u16MaxErrorPercent = SOC_DEFAULT_MAX_ERROR_PERCENT;
}

void soc_param_lib_init(void)
{
	SOC_ResetCalculateState();
	SOC_LoadFactoryRuntimeConfig();
	SOC_DealEEPROM_Data(EEPROM_DATA_READ);
	SOC_Enhance_Element.u16_SOC_InitOver = 1U;
	SOC_ResetRuntimeContext();
	SOC_SyncOutputData(1U);
}

UINT8 SOC_ResetStoredSnapshotToDefault(void)
{
	STORAGE_FLASH_SOC_DATA flash_data;
	UINT32 cap_factory;

	memset(&flash_data, 0, sizeof(flash_data));
	cap_factory = (UINT32)OtherElement.u16Soc_Ah * 3600U;
	flash_data.u16FormatVersion = FLASH_STORAGE_SOC_DATA_VERSION_V2;
	flash_data.u16SocNow = SOC_DEFAULT_STARTUP_PERCENT;
	flash_data.u16DsgSocInt = 0U;
	flash_data.u16MaxErrorPercent = SOC_DEFAULT_MAX_ERROR_PERCENT;
	flash_data.u32CycleTimes = (UINT32)OtherElement.u16Soc_Cycle_times * 100U;
	flash_data.u32CapFull = cap_factory;
	flash_data.u32CapNow = (UINT32)SOC_DEFAULT_STARTUP_PERCENT * cap_factory / 100U;

	return StorageFlash_SaveSocData(&flash_data);
}

static UINT8 SOC_IsVoltageValid(void)
{
	if ((SOC_Enhance_Element.u16_VCellMin < SOC_CALIBRATION_MIN_CELL_VALID_MV) ||
		(SOC_Enhance_Element.u16_VCellMax < SOC_CALIBRATION_MIN_CELL_VALID_MV))
	{
		return 0U;
	}
	if ((SOC_Enhance_Element.u16_VCellMin > SOC_CALIBRATION_MAX_CELL_VALID_MV) ||
		(SOC_Enhance_Element.u16_VCellMax > SOC_CALIBRATION_MAX_CELL_VALID_MV))
	{
		return 0U;
	}

	return (UINT8)(SOC_Enhance_Element.u16_VCellMax >= SOC_Enhance_Element.u16_VCellMin);
}

static UINT16 SOC_GetCellDeltaMv(void)
{
	return (SOC_Enhance_Element.u16_VCellMax >= SOC_Enhance_Element.u16_VCellMin) ?
		(UINT16)(SOC_Enhance_Element.u16_VCellMax - SOC_Enhance_Element.u16_VCellMin) :
		(UINT16)(SOC_Enhance_Element.u16_VCellMin - SOC_Enhance_Element.u16_VCellMax);
}

static UINT8 SOC_IsCalibrationVoltageValid(void)
{
	if (!SOC_IsVoltageValid())
	{
		return 0U;
	}
	if (SOC_CALIBRATION_MAX_CELL_DELTA_MV == 0U)
	{
		return 1U;
	}

	return (UINT8)(SOC_GetCellDeltaMv() <= SOC_CALIBRATION_MAX_CELL_DELTA_MV);
}

static UINT8 SOC_HasBlockingProtectionFault(void)
{
	if (!SOC_CALIBRATION_BLOCK_PROTECTION_FAULT)
	{
		return 0U;
	}

	return (UINT8)(g_stCellInfoReport.unMdlFault_Third.all != 0U);
}

static UINT8 SOC_HasBlockingSystemFault(void)
{
	if (!SOC_CALIBRATION_BLOCK_SYSTEM_FAULT)
	{
		return 0U;
	}

	return (UINT8)((System_ERROR_UserCallback(ERROR_STATUS_AFE1) != 0U) ||
		(System_ERROR_UserCallback(ERROR_STATUS_AFE2) != 0U) ||
		(System_ERROR_UserCallback(ERROR_STATUS_ADC) != 0U) ||
		(System_ERROR_UserCallback(ERROR_STATUS_CBC_CHG) != 0U) ||
		(System_ERROR_UserCallback(ERROR_STATUS_CBC_DSG) != 0U) ||
		(System_ERROR_UserCallback(ERROR_STATUS_TEMP_BREAK) != 0U));
}

static UINT8 SOC_IsCalibrationAllowed(void)
{
	return (UINT8)(SOC_IsCalibrationVoltageValid() &&
		(!SOC_HasBlockingProtectionFault()) &&
		(!SOC_HasBlockingSystemFault()));
}

static const UINT16 *SOC_GetSelectedOcvTable(UINT16 *table_size)
{
	if (table_size == 0)
	{
		return SOC_Table_LiFePO;
	}

	switch (SOC_Enhance_Element.u16_SOC_TableSelect)
	{
	case SOC_TABLE_TEST:
		*table_size = SOC_Size_TableCanSet;
		return SOC_Enhance_Element.SOC_Table_CanSet;
	case SOC_TABLE_TERNARYLI:
		*table_size = SOC_Size_TernaryLi;
		return SocTable_TernaryLi;
	case SOC_TABLE_LIFEPO2:
		*table_size = SOC_Size_LiFePO2;
		return SocTable_LiFePO2;
	case SOC_TABLE_LIFEPO:
	default:
		*table_size = SOC_Size_LiFePO;
		return SOC_Table_LiFePO;
	}
}

static UINT8 Get_OpenCircuit_Value(void)
{
	UINT16 table_size;
	UINT16 result;
	const UINT16 *table;

	table = SOC_GetSelectedOcvTable(&table_size);
	result = GetEndValue(table, table_size, SOC_Enhance_Element.u16_VCellMin);
	if (result > 100U)
	{
		result = 100U;
	}

	return (UINT8)result;
}

static UINT8 SOC_GetMeasuredCurrentDirection(void)
{
	if ((SOC_Enhance_Element.u16_Ichg >= SOC_CURRENT_ENTER_A10) &&
		(SOC_Enhance_Element.u16_Ichg >= SOC_Enhance_Element.u16_Idsg))
	{
		return SOC_MODE_CHG;
	}
	if (SOC_Enhance_Element.u16_Idsg >= SOC_CURRENT_ENTER_A10)
	{
		return SOC_MODE_DSG;
	}

	return SOC_MODE_RELAX;
}

static UINT8 SOC_GetCurrentDirection(void)
{
	UINT8 measured_direction;
	UINT16 relax_limit_ticks;

	measured_direction = SOC_GetMeasuredCurrentDirection();
	if (measured_direction == SOC_MODE_CHG)
	{
		g_soc_runtime.u8Mode = SOC_MODE_CHG;
		g_soc_runtime.u16RelaxEntryTicks = 0U;
		return SOC_MODE_CHG;
	}
	if (measured_direction == SOC_MODE_DSG)
	{
		g_soc_runtime.u8Mode = SOC_MODE_DSG;
		g_soc_runtime.u16RelaxEntryTicks = 0U;
		return SOC_MODE_DSG;
	}

	relax_limit_ticks = (UINT16)(SOC_RELAX_ENTRY_SECONDS * SOC_TICKS_PER_SECOND);
	if (g_soc_runtime.u16RelaxEntryTicks < relax_limit_ticks)
	{
		++g_soc_runtime.u16RelaxEntryTicks;
	}
	if (g_soc_runtime.u16RelaxEntryTicks >= relax_limit_ticks)
	{
		g_soc_runtime.u8Mode = SOC_MODE_RELAX;
	}

	return SOC_MODE_RELAX;
}

static UINT8 SOC_ApplyCapacityDelta(enum _CUR current_type, UINT16 current)
{
	UINT8 old_soc;
	UINT8 new_soc;
	UINT32 cap_base;
	UINT32 delta_as10;
	UINT32 integrate_acc_ms;

	cap_base = SOC_GetCapBase();
	if ((cap_base == 0U) || (current == 0U))
	{
		return 0U;
	}

	integrate_acc_ms = ((UINT32)current * SOC_INTEGRATE_PERIOD_MS) + SOC_Calculate_Element.u32IntegrateRemainderMs;
	delta_as10 = integrate_acc_ms / SOC_INTEGRATE_MS_PER_SECOND;
	SOC_Calculate_Element.u32IntegrateRemainderMs = integrate_acc_ms % SOC_INTEGRATE_MS_PER_SECOND;
	if (delta_as10 == 0U)
	{
		return 0U;
	}

	old_soc = SOC_Calculate_Element.u8SOC_Now;
	if (current_type == CurCHG)
	{
		if ((delta_as10 >= cap_base) || (SOC_Calculate_Element.u32CapNow > (cap_base - delta_as10)))
		{
			SOC_Calculate_Element.u32CapNow = cap_base;
		}
		else
		{
			SOC_Calculate_Element.u32CapNow += delta_as10;
		}
	}
	else
	{
		if (SOC_Calculate_Element.u32CapNow > delta_as10)
		{
			SOC_Calculate_Element.u32CapNow -= delta_as10;
		}
		else
		{
			SOC_Calculate_Element.u32CapNow = 0U;
		}
	}

	new_soc = SOC_CalcSocFromCap(SOC_Calculate_Element.u32CapNow, cap_base);
	SOC_Calculate_Element.u8SOC_Now = new_soc;
	return (old_soc > new_soc) ? (UINT8)(old_soc - new_soc) : (UINT8)(new_soc - old_soc);
}

static void SOC_AddDischargeCyclePercent(UINT8 delta_soc)
{
	UINT16 accumulated;

	if (delta_soc == 0U)
	{
		return;
	}

	accumulated = (UINT16)SOC_Calculate_Element.u8DSG_SOC_Int + delta_soc;
	while (accumulated >= SOC_CYCLE_PERCENT_PER_COUNT)
	{
		accumulated = (UINT16)(accumulated - SOC_CYCLE_PERCENT_PER_COUNT);
		SOC_Calculate_Element.u32Cycle_times += 100U;
	}
	SOC_Calculate_Element.u8DSG_SOC_Int = (UINT8)accumulated;
}

static void SOC_RunCoulombCounter(UINT8 direction)
{
	UINT8 delta_soc;

	if (direction == SOC_MODE_CHG)
	{
		delta_soc = SOC_ApplyCapacityDelta(CurCHG, SOC_Enhance_Element.u16_Ichg);
		(void)delta_soc;
		g_soc_runtime.u16EmptyConfirmTicks = 0U;
	}
	else if (direction == SOC_MODE_DSG)
	{
		delta_soc = SOC_ApplyCapacityDelta(CurDSG, SOC_Enhance_Element.u16_Idsg);
		SOC_AddDischargeCyclePercent(delta_soc);
		g_soc_runtime.u16FullConfirmTicks = 0U;
		g_soc_runtime.u16HighChargeTicks = 0U;
	}
	else
	{
		SOC_Calculate_Element.u32IntegrateRemainderMs = 0U;
		g_soc_runtime.u16FullConfirmTicks = 0U;
		g_soc_runtime.u16EmptyConfirmTicks = 0U;
		g_soc_runtime.u16HighChargeTicks = 0U;
	}
}

static UINT16 SOC_GetFullCellConfirmVoltage(void)
{
	UINT16 chemistry_floor;
	UINT16 min_cell_mv;

	switch (SOC_Enhance_Element.u16_SOC_TableSelect)
	{
	case SOC_TABLE_TERNARYLI:
		chemistry_floor = 4000U;
		break;
	case SOC_TABLE_LIFEPO:
	case SOC_TABLE_LIFEPO2:
		chemistry_floor = 3300U;
		break;
	default:
		chemistry_floor = (SOC_Enhance_Element.u16_SOC_100_Vol >= 3900U) ? 4000U : 3300U;
		break;
	}

	if (SOC_Enhance_Element.u16_SOC_100_Vol > SOC_FULL_CONFIRM_MIN_CELL_MARGIN_MV)
	{
		min_cell_mv = (UINT16)(SOC_Enhance_Element.u16_SOC_100_Vol - SOC_FULL_CONFIRM_MIN_CELL_MARGIN_MV);
	}
	else
	{
		min_cell_mv = 0U;
	}
	if (min_cell_mv < chemistry_floor)
	{
		min_cell_mv = chemistry_floor;
	}

	return min_cell_mv;
}

static UINT8 SOC_IsFullConfirmCellDeltaValid(void)
{
	if (SOC_FULL_CONFIRM_MAX_CELL_DELTA_MV == 0U)
	{
		return 1U;
	}

	return (UINT8)(SOC_GetCellDeltaMv() <= SOC_FULL_CONFIRM_MAX_CELL_DELTA_MV);
}

static UINT8 SOC_StepTowardTarget(UINT8 current_soc, UINT8 target_soc, UINT8 max_step)
{
	UINT8 step;

	if ((max_step == 0U) || (current_soc == target_soc))
	{
		return current_soc;
	}
	if (current_soc < target_soc)
	{
		step = (UINT8)(target_soc - current_soc);
		if (step > max_step)
		{
			step = max_step;
		}
		return (UINT8)(current_soc + step);
	}

	step = (UINT8)(current_soc - target_soc);
	if (step > max_step)
	{
		step = max_step;
	}
	return (UINT8)(current_soc - step);
}

static void SOC_ApplyChargeCatchup(UINT8 direction)
{
	UINT16 high_mv;
	UINT16 limit_ticks;

	if ((direction != SOC_MODE_CHG) || !SOC_IsCalibrationAllowed())
	{
		g_soc_runtime.u16HighChargeTicks = 0U;
		return;
	}
	if (SOC_Calculate_Element.u8SOC_Now >= SOC_CHG_HIGH_CATCHUP_TARGET)
	{
		g_soc_runtime.u16HighChargeTicks = 0U;
		return;
	}

	high_mv = SOC_GetFullCellConfirmVoltage();
	if ((SOC_Enhance_Element.u16_VCellMax < high_mv) ||
		(SOC_Enhance_Element.u16_VCellMin < high_mv))
	{
		g_soc_runtime.u16HighChargeTicks = 0U;
		return;
	}

	limit_ticks = (UINT16)(SOC_CHG_HIGH_CATCHUP_SECONDS * SOC_TICKS_PER_SECOND);
	if (++g_soc_runtime.u16HighChargeTicks >= limit_ticks)
	{
		g_soc_runtime.u16HighChargeTicks = 0U;
		SOC_ApplySocCorrection(SOC_StepTowardTarget(SOC_Calculate_Element.u8SOC_Now, SOC_CHG_HIGH_CATCHUP_TARGET, 1U));
	}
}

static void SOC_ApplyFullEmptyAnchor(UINT8 direction)
{
	UINT16 confirm_limit_ticks;

	if (!SOC_IsCalibrationAllowed())
	{
		g_soc_runtime.u16FullConfirmTicks = 0U;
		g_soc_runtime.u16EmptyConfirmTicks = 0U;
		return;
	}

	if (direction == SOC_MODE_CHG)
	{
		confirm_limit_ticks = (UINT16)(SOC_FULL_CONFIRM_SECONDS * SOC_TICKS_PER_SECOND);
		if (confirm_limit_ticks == 0U)
		{
			confirm_limit_ticks = 1U;
		}
		if ((SOC_Enhance_Element.u16_VCellMax >= SOC_Enhance_Element.u16_SOC_100_Vol) &&
			(SOC_Enhance_Element.u16_VCellMin >= SOC_GetFullCellConfirmVoltage()) &&
			SOC_IsFullConfirmCellDeltaValid())
		{
			if (g_soc_runtime.u16FullConfirmTicks < confirm_limit_ticks)
			{
				++g_soc_runtime.u16FullConfirmTicks;
			}
			if (g_soc_runtime.u16FullConfirmTicks >= confirm_limit_ticks)
			{
				SOC_ApplySocNow(100U);
				g_soc_runtime.u16FullConfirmTicks = 0U;
				g_soc_runtime.u16HighChargeTicks = 0U;
			}
			return;
		}
		g_soc_runtime.u16FullConfirmTicks = 0U;
		return;
	}

	if (direction == SOC_MODE_DSG)
	{
		if (SOC_Enhance_Element.u16_VCellMin <= SOC_Enhance_Element.u16_SOC_0_Vol)
		{
			if (g_soc_runtime.u16EmptyConfirmTicks < (UINT16)(4U * SOC_TICKS_PER_SECOND))
			{
				++g_soc_runtime.u16EmptyConfirmTicks;
			}
			if (g_soc_runtime.u16EmptyConfirmTicks >= (UINT16)(4U * SOC_TICKS_PER_SECOND))
			{
				SOC_ApplySocNow(0U);
				g_soc_runtime.u16EmptyConfirmTicks = 0U;
			}
			return;
		}
		g_soc_runtime.u16EmptyConfirmTicks = 0U;
	}
}

static void SOC_ApplyWeakCellGuard(void)
{
	UINT8 guard_soc;
	UINT8 target_soc;
	UINT8 current_soc;
	UINT8 max_step;

	if (isCHG() || !SOC_IsVoltageValid())
	{
		return;
	}
	if (SOC_Enhance_Element.u16_VCellMin > (UINT16)(SOC_Enhance_Element.u16_SOC_0_Vol + SOC_WEAK_CELL_GUARD_WINDOW_MV))
	{
		return;
	}

	guard_soc = 100U;
	target_soc = Get_OpenCircuit_Value();
	if (target_soc < guard_soc)
	{
		guard_soc = target_soc;
	}

	max_step = 1U;
	if (SOC_Enhance_Element.u16_VCellMin <= SOC_Enhance_Element.u16_SOC_0_Vol)
	{
		guard_soc = 0U;
		max_step = 5U;
	}
	else if (SOC_Enhance_Element.u16_VCellMin <= (UINT16)(SOC_Enhance_Element.u16_SOC_0_Vol + SOC_WEAK_CELL_CRITICAL_WINDOW_MV))
	{
		if (guard_soc > 2U)
		{
			guard_soc = 2U;
		}
		max_step = 2U;
	}
	else if (guard_soc > 8U)
	{
		guard_soc = 8U;
	}

	current_soc = SOC_Calculate_Element.u8SOC_Now;
	if (current_soc > guard_soc)
	{
		SOC_ApplySocCorrection(SOC_StepTowardTarget(current_soc, guard_soc, max_step));
	}
}

static void SOC_ApplyRestOcvCorrection(UINT32 rest_seconds)
{
	UINT8 target_soc;
	UINT8 current_soc;
	UINT8 max_step;

	if ((rest_seconds < SOC_REST_CORRECT_MIN_SECONDS) ||
		(!SOC_IsCalibrationAllowed()) ||
		isCHG() || isDSG())
	{
		return;
	}

	target_soc = Get_OpenCircuit_Value();
	current_soc = SOC_Calculate_Element.u8SOC_Now;
	max_step = SOC_REST_CORRECT_MAX_STEP_PERCENT;
	if (rest_seconds < 3600U)
	{
		max_step = 1U;
	}
	else if (rest_seconds < 21600U)
	{
		max_step = 2U;
	}

	if (target_soc != current_soc)
	{
		SOC_ApplySocCorrection(SOC_StepTowardTarget(current_soc, target_soc, max_step));
	}
}

static void SOC_UpdateDisplaySoc(void)
{
	UINT8 target_soc;
	UINT8 step_limit;

	target_soc = SOC_Calculate_Element.u8SOC_Now;
	if (!g_soc_runtime.u8DisplayReady)
	{
		g_soc_runtime.u8DisplaySoc = target_soc;
		g_soc_runtime.u8DisplayReady = 1U;
		g_soc_runtime.u8DisplayStepTicks = 0U;
		return;
	}
	if (g_soc_runtime.u8DisplaySoc == target_soc)
	{
		g_soc_runtime.u8DisplayStepTicks = 0U;
		return;
	}

	step_limit = (UINT8)(SOC_DISPLAY_STEP_SECONDS * SOC_TICKS_PER_SECOND);
	if (step_limit == 0U)
	{
		step_limit = 1U;
	}
	if (++g_soc_runtime.u8DisplayStepTicks < step_limit)
	{
		return;
	}

	g_soc_runtime.u8DisplayStepTicks = 0U;
	if (g_soc_runtime.u8DisplaySoc < target_soc)
	{
		++g_soc_runtime.u8DisplaySoc;
	}
	else
	{
		--g_soc_runtime.u8DisplaySoc;
	}
}

static UINT16 SOC_CapAs10ToAh100(UINT32 cap_as10)
{
	UINT32 cap_ah100;

	cap_ah100 = (cap_as10 + 180U) / 360U;
	if (cap_ah100 > 0xFFFFU)
	{
		return 0xFFFFU;
	}

	return (UINT16)cap_ah100;
}

static void SOC_SyncOutputData(UINT8 force_display_follow)
{
	UINT32 cap_base;

	if (force_display_follow)
	{
		g_soc_runtime.u8DisplaySoc = SOC_Calculate_Element.u8SOC_Now;
		g_soc_runtime.u8DisplayReady = 1U;
		g_soc_runtime.u8DisplayStepTicks = 0U;
	}
	else
	{
		SOC_UpdateDisplaySoc();
	}

	cap_base = SOC_GetCapBase();
	SOC_Enhance_Element.u8_SOC = g_soc_runtime.u8DisplaySoc;
	SOC_Enhance_Element.u8_SOH = 100U;
	SOC_Enhance_Element.u16_CapacityNow = SOC_CapAs10ToAh100(SOC_Calculate_Element.u32CapNow);
	SOC_Enhance_Element.u16_CapacityFull = SOC_CapAs10ToAh100(cap_base);
	SOC_Enhance_Element.u16_CapacityFactory = SOC_CapAs10ToAh100(SOC_Calculate_Element.u32CapFactory);
	if ((SOC_Calculate_Element.u32Cycle_times / 100U) > 0xFFFFU)
	{
		SOC_Enhance_Element.u16_Cycle_times = 0xFFFFU;
	}
	else
	{
		SOC_Enhance_Element.u16_Cycle_times = (UINT16)(SOC_Calculate_Element.u32Cycle_times / 100U);
	}
	SOC_Enhance_Element.u8_SOC_OCV_Cali = SOC_Calculate_Element.u8DSG_SOC_Int;

	SOC_PublishReportData();
}

static UINT8 SOC_DealEEPROM_Data(enum EEPROM_COMMAND command)
{
	STORAGE_FLASH_SOC_DATA flash_data;
	UINT8 valid;
	UINT8 save_ok;
	UINT32 cap_base;

	valid = 0U;
	save_ok = 1U;
	switch (command)
	{
	case EEPROM_DATA_REFRESH:
		memset(&flash_data, 0, sizeof(flash_data));
		flash_data.u16FormatVersion = FLASH_STORAGE_SOC_DATA_VERSION_V2;
		flash_data.u16SocNow = SOC_Calculate_Element.u8SOC_Now;
		flash_data.u16DsgSocInt = SOC_Calculate_Element.u8DSG_SOC_Int;
		flash_data.u16MaxErrorPercent = SOC_Calculate_Element.u16MaxErrorPercent;
		flash_data.u32CycleTimes = SOC_Calculate_Element.u32Cycle_times;
		flash_data.u32CapNow = SOC_Calculate_Element.u32CapNow;
		flash_data.u32CapFull = SOC_GetCapBase();
		return StorageFlash_SaveSocData(&flash_data);

	case EEPROM_DATA_READ:
		valid = StorageFlash_LoadSocData(&flash_data);
		if (valid)
		{
			if ((flash_data.u16SocNow > 100U) ||
				(flash_data.u16DsgSocInt > 100U) ||
				(flash_data.u16MaxErrorPercent > 100U))
			{
				valid = 0U;
			}
		}

		if (!valid)
		{
			SOC_LoadDefaultSnapshot();
			save_ok = SOC_DealEEPROM_Data(EEPROM_DATA_REFRESH);
		}
		else
		{
			SOC_Calculate_Element.u8SOC_Now = (UINT8)flash_data.u16SocNow;
			SOC_Calculate_Element.u8DSG_SOC_Int = (UINT8)flash_data.u16DsgSocInt;
			SOC_Calculate_Element.u32Cycle_times = flash_data.u32CycleTimes;
			SOC_Calculate_Element.u16MaxErrorPercent = (flash_data.u16MaxErrorPercent == 0U) ?
				SOC_DEFAULT_MAX_ERROR_PERCENT : flash_data.u16MaxErrorPercent;
			SOC_Calculate_Element.u32CapFull = SOC_Calculate_Element.u32CapFactory;
			cap_base = SOC_GetCapBase();
			if ((flash_data.u32CapNow != 0U) && (flash_data.u32CapNow <= cap_base))
			{
				SOC_Calculate_Element.u32CapNow = flash_data.u32CapNow;
				SOC_Calculate_Element.u8SOC_Now = SOC_CalcSocFromCap(SOC_Calculate_Element.u32CapNow, cap_base);
			}
			else
			{
				SOC_Calculate_Element.u32CapNow = (UINT32)(((uint64_t)SOC_Calculate_Element.u8SOC_Now * (uint64_t)cap_base) / 100ULL);
			}
		}

		SOC_Calculate_Element_backup = SOC_Calculate_Element;
		if (!save_ok)
		{
			SOC_Calculate_Element_backup.u8SOC_Now = 0xFFU;
		}
		return save_ok;

	default:
		break;
	}

	return 0U;
}

static void SOC_PersistSnapshotIfChanged(void)
{
	if (!SOC_Enhance_Element.u16_SOC_InitOver)
	{
		return;
	}

	if ((SOC_Calculate_Element.u8SOC_Now != SOC_Calculate_Element_backup.u8SOC_Now) ||
		(SOC_Calculate_Element.u8DSG_SOC_Int != SOC_Calculate_Element_backup.u8DSG_SOC_Int) ||
		(SOC_Calculate_Element.u32Cycle_times != SOC_Calculate_Element_backup.u32Cycle_times) ||
		(SOC_Calculate_Element.u32CapFull != SOC_Calculate_Element_backup.u32CapFull) ||
		(SOC_Calculate_Element.u16MaxErrorPercent != SOC_Calculate_Element_backup.u16MaxErrorPercent))
	{
		if (SOC_DealEEPROM_Data(EEPROM_DATA_REFRESH))
		{
			SOC_Calculate_Element_backup = SOC_Calculate_Element;
		}
	}
}

static void SOC_Update_StartUp(void)
{
	UINT8 target_soc;

	switch (SOC_Enhance_Element.u16_RefreshData_Flag)
	{
	case 1:
		if (SOC_IsCalibrationAllowed())
		{
			target_soc = Get_OpenCircuit_Value();
			if ((!isCHG()) && (target_soc > SOC_Calculate_Element.u8SOC_Now))
			{
				target_soc = SOC_Calculate_Element.u8SOC_Now;
			}
			SOC_ApplySocNow(target_soc);
		}
		break;

	case 2:
		target_soc = SOC_Calculate_Element.u8SOC_Now;
		SOC_LoadFactoryRuntimeConfig();
		SOC_Calculate_Element.u8DSG_SOC_Int = 0U;
		SOC_Calculate_Element.u16MaxErrorPercent = SOC_DEFAULT_MAX_ERROR_PERCENT;
		SOC_ApplySocNow(target_soc);
		break;

	case 3:
		SOC_Calculate_Element.u16MaxErrorPercent = SOC_DEFAULT_MAX_ERROR_PERCENT;
		SOC_ApplySocNow(SOC_Enhance_Element.u8_SetSocOnce);
		break;

	default:
		break;
	}

	SOC_Enhance_Element.u16_RefreshData_Flag = 0U;
	SOC_ResetRuntimeContext();
	SOC_SyncOutputData(1U);
}

static void SOC_RefreshData_Monitor(void)
{
	if (SOC_Enhance_Element.u16_SOC_InitOver && (SOC_Enhance_Element.u16_RefreshData_Flag != 0U))
	{
		SOC_Update_StartUp();
	}
}

static UINT8 isCHG(void)
{
	return (UINT8)((g_soc_runtime.u8Mode == SOC_MODE_CHG) ||
		(SOC_GetMeasuredCurrentDirection() == SOC_MODE_CHG));
}

static UINT8 isDSG(void)
{
	return (UINT8)((g_soc_runtime.u8Mode == SOC_MODE_DSG) ||
		(SOC_GetMeasuredCurrentDirection() == SOC_MODE_DSG));
}

void SOC_IntEnhance_Ctrl(void)
{
	UINT8 direction;

	if (!SOC_Enhance_Element.u16_SOC_InitOver)
	{
		return;
	}

	SOC_RefreshData_Monitor();
	direction = SOC_GetCurrentDirection();
	SOC_RunCoulombCounter(direction);
	SOC_ApplyChargeCatchup(direction);
	SOC_ApplyFullEmptyAnchor(direction);
	SOC_ApplyWeakCellGuard();
	SOC_PersistSnapshotIfChanged();
	SOC_SyncOutputData(0U);
}

void SOC_ApplyRtcRelaxationCompensation(UINT32 rest_seconds, UINT16 vcell_min, UINT16 vcell_max)
{
	if (!SOC_Enhance_Element.u16_SOC_InitOver)
	{
		return;
	}

	SOC_Enhance_Element.u16_VCellMin = vcell_min;
	SOC_Enhance_Element.u16_VCellMax = vcell_max;
	SOC_ApplyRestOcvCorrection(rest_seconds);
	SOC_ApplyWeakCellGuard();
	SOC_PersistSnapshotIfChanged();
	SOC_SyncOutputData(1U);
}
