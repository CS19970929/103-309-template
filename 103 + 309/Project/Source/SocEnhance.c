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

#define SOC_INTEGRATE_PERIOD_MS ((UINT32)200U)
#define SOC_INTEGRATE_MS_PER_SECOND ((UINT32)1000U)
#define SOC_CURRENT_ENTER_A10 ((UINT16)4U)
#define SOC_MODE_RELAX_ENTRY_SECONDS ((UINT16)5U)
#define SOC_FULL_CONFIRM_SECONDS ((UINT16)60U)
#define SOC_DISPLAY_STEP_SECONDS ((UINT16)1U)
#define SOC_RELAX_STABLE_SECONDS ((UINT16)30U)
#define SOC_RELAX_VOLT_STABLE_WINDOW_MV ((UINT16)3U)
#define SOC_LEARN_FIRST_SPAN_PERCENT ((UINT16)90U)
#define SOC_LEARN_NEXT_SPAN_PERCENT ((UINT16)40U)
#define SOC_LEARN_CAP_MIN_PERCENT ((UINT16)50U)
#define SOC_LEARN_CAP_MAX_PERCENT ((UINT16)110U)
#define SOC_LEARN_CAP_MAX_STEP_PERCENT ((UINT16)5U)
#define SOC_MAX_ERROR_DEFAULT_PERCENT ((UINT16)100U)
#define SOC_MAX_ERROR_LEARNED_PERCENT ((UINT16)5U)
#define SOC_FULL_CONFIRM_MIN_CELL_MARGIN_MV ((UINT16)PROJECT_CFG_SOC_FULL_CONFIRM_MIN_CELL_MARGIN_MV)
#define SOC_FULL_CONFIRM_MAX_CELL_DELTA_MV ((UINT16)PROJECT_CFG_SOC_FULL_CONFIRM_MAX_CELL_DELTA_MV)
#define SOC_ONLINE_OCV_GUARD_ENABLE ((UINT8)PROJECT_CFG_SOC_ONLINE_OCV_GUARD_ENABLE)
#define SOC_ONLINE_OCV_CORRECTION_SECONDS ((UINT16)PROJECT_CFG_SOC_ONLINE_OCV_CORRECTION_SECONDS)
#define SOC_ONLINE_OCV_MIN_DELTA_PERCENT ((UINT8)PROJECT_CFG_SOC_ONLINE_OCV_MIN_DELTA_PERCENT)
#define SOC_ONLINE_OCV_CURRENT_DIVIDER ((UINT16)PROJECT_CFG_SOC_ONLINE_OCV_CURRENT_DIVIDER)
#define SOC_CALIBRATION_MIN_CELL_VALID_MV ((UINT16)PROJECT_CFG_SOC_CALIBRATION_MIN_CELL_VALID_MV)
#define SOC_CALIBRATION_MAX_CELL_VALID_MV ((UINT16)PROJECT_CFG_SOC_CALIBRATION_MAX_CELL_VALID_MV)
#define SOC_CALIBRATION_MAX_CELL_DELTA_MV ((UINT16)PROJECT_CFG_SOC_CALIBRATION_MAX_CELL_DELTA_MV)
#define SOC_CALIBRATION_BLOCK_PROTECTION_FAULT ((UINT8)PROJECT_CFG_SOC_CALIBRATION_BLOCK_PROTECTION_FAULT)
#define SOC_CALIBRATION_BLOCK_SYSTEM_FAULT ((UINT8)PROJECT_CFG_SOC_CALIBRATION_BLOCK_SYSTEM_FAULT)
#define SOC_ONLINE_OCV_TARGET_MIN_PERCENT ((UINT8)5U)
#define SOC_ONLINE_OCV_TARGET_MAX_PERCENT ((UINT8)95U)
#define SOC_ONLINE_OCV_LFP_LOW_EDGE_PERCENT ((UINT8)20U)
#define SOC_ONLINE_OCV_LFP_HIGH_EDGE_PERCENT ((UINT8)90U)

enum EEPROM_COMMAND
{
	EEPROM_DATA_REFRESH = 0,
	EEPROM_DATA_READ
};

struct SOC_CALCULATE_ELEMENT
{
	UINT32 u32CapFactory;            // As * 10
	UINT32 u32CapFull;               // As * 10, SOH capacity base
	UINT32 u32CapNow;                // As * 10
	UINT32 u32CapChange;             // As * 10 accumulated until SOC changes by 1%
	UINT32 u32IntegrateRemainderMs;  // current(A*10) * ms remainder
	UINT32 u32Cycle_times;           // cycle count * 100
	UINT32 u32LearnPassedAs10;       // As * 10 counted between trusted anchors
	UINT16 u16LearnAnchorSoc;        // 0 or 100 for current anchor
	UINT16 u16LearnState;            // SOC_LEARN_STATE_*
	UINT16 u16MaxErrorPercent;       // coarse confidence exported by snapshot
	UINT16 u16LearnFlags;            // SOC_LEARN_FLAG_*
	UINT8 u8SOC_Now;                 // 0..100
	UINT8 u8DSG_SOC_Int;             // discharged percent accumulator
};

struct SOC_ENHANCE_ELEMENT SOC_Enhance_Element;			   // 对外交互结构体,lib文件的桥梁
struct SOC_CALCULATE_ELEMENT SOC_Calculate_Element;		   // 内部计算结构体
struct SOC_CALCULATE_ELEMENT SOC_Calculate_Element_backup; // 内部计算结构体

#define SOC_REST_BUCKET_1_SECONDS ((UINT32)600)
#define SOC_REST_BUCKET_2_SECONDS ((UINT32)1800)
#define SOC_REST_BUCKET_3_SECONDS ((UINT32)3600)
#define SOC_REST_BUCKET_4_SECONDS ((UINT32)21600)
#define SOC_TICKS_PER_SECOND (SOC_INTEGRATE_MS_PER_SECOND / SOC_INTEGRATE_PERIOD_MS)
#define SOC_WEAK_CELL_GUARD_WINDOW_MV ((UINT16)120)
#define SOC_WEAK_CELL_CRITICAL_WINDOW_MV ((UINT16)30)
#define SOC_CYCLE_PERCENT_PER_COUNT ((UINT16)80)
#define SOC_INTEGRATE_DIRECTION_IDLE ((UINT8)0)
#define SOC_INTEGRATE_DIRECTION_CHG ((UINT8)1)
#define SOC_INTEGRATE_DIRECTION_DSG ((UINT8)2)
#define SOC_MODE_RELAX SOC_INTEGRATE_DIRECTION_IDLE
#define SOC_MODE_CHG SOC_INTEGRATE_DIRECTION_CHG
#define SOC_MODE_DSG SOC_INTEGRATE_DIRECTION_DSG
#define SOC_LEARN_STATE_NONE ((UINT16)0)
#define SOC_LEARN_STATE_FULL_ANCHOR ((UINT16)1)
#define SOC_LEARN_STATE_EMPTY_ANCHOR ((UINT16)2)
#define SOC_LEARN_FLAG_LEARNED ((UINT16)0x0001)

struct SOC_RUNTIME_CONTEXT
{
	UINT8 u8DisplaySoc;
	UINT8 u8DisplayReady;
	UINT8 u8RestBucketApplied;
	UINT8 u8IntegrateDirection;
	UINT8 u8Mode;
	UINT8 u8DisplayStepTicks;
	UINT16 u16RelaxEntryTicks;
	UINT16 u16RelaxStableTicks;
	UINT16 u16ChgTerminalTicks;
	UINT16 u16DsgTerminalTicks;
	UINT16 u16FullConfirmTicks;
	UINT16 u16OnlineOcvTicks;
	UINT16 u16RelaxRefVCellMin;
	UINT16 u16RelaxRefVCellMax;
	UINT32 u32RestTicks;
	UINT8 u8OnlineOcvDirection;
};

static struct SOC_RUNTIME_CONTEXT g_soc_runtime;

static void SOC_LoadFactoryRuntimeConfig(void);
static void SOC_ResetCalculateState(void);
static void SOC_LoadDefaultSnapshot(void);
static UINT8 SOC_DealEEPROM_Data(enum EEPROM_COMMAND Command);
static void SOC_ResetRuntimeContext(void);
static UINT8 SOC_IsVoltageValid(void);
static UINT16 SOC_GetCellDeltaMv(void);
static UINT8 SOC_IsCalibrationVoltageValid(void);
static UINT8 SOC_HasBlockingProtectionFault(void);
static UINT8 SOC_HasBlockingSystemFault(void);
static UINT8 SOC_IsCalibrationAllowed(void);
static UINT32 SOC_GetCapBase(void);
static void SOC_SelectIntegrateDirection(UINT8 direction);
static UINT8 SOC_ApplyCapacityDelta(enum _CUR current_type, UINT16 current);
static void SOC_AddLearnPassed(UINT8 direction, UINT32 delta_as10);
static void SOC_ResetLearningState(void);
static void SOC_OnTrustedFullAnchor(void);
static void SOC_OnTrustedEmptyAnchor(void);
static void SOC_ApplyLearnedFullCapacity(UINT32 learned_cap);
static void SOC_AddDischargeCyclePercent(UINT8 delta_soc);
static void SOC_SetSocValue(UINT8 soc_now, UINT8 clear_cap_change);
static void SOC_ApplySocNow(UINT8 soc_now);
static void SOC_ApplySocCorrection(UINT8 soc_now);
static UINT8 SOC_StepTowardTarget(UINT8 current_soc, UINT8 target_soc, UINT8 max_step);
static void SOC_PersistSnapshotIfChanged(void);
static UINT8 SOC_GetRestBucket(UINT32 rest_seconds);
static void SOC_ResetRestMonitor(void);
static void SOC_UpdateRelaxVoltageStable(void);
static UINT8 SOC_IsRelaxVoltageStable(void);
static void SOC_ApplyRestCompensation(UINT32 rest_seconds);
static void SOC_UpdateRestMonitor(void);
static void SOC_ApplyWeakCellGuard(void);
static void SOC_UpdateDisplaySoc(void);
static UINT16 SOC_CapAs10ToAh100(UINT32 cap_as10);
static void SOC_SyncOutputData(UINT8 force_display_follow);
static UINT8 SOC_GetMeasuredCurrentDirection(void);
static UINT8 SOC_GetCurrentDirection(void);
static void SOC_RunCoulombCounter(UINT8 direction);
static void SOC_ApplyOnlineOcvGuard(UINT8 direction);
static void SOC_ApplyTerminalCorrection(UINT8 direction);
static UINT16 SOC_GetFullCellConfirmVoltage(void);
static UINT8 SOC_IsFullConfirmCellDeltaValid(void);
static UINT16 SOC_GetCurrentLimitA10(UINT16 divider);
static UINT16 SOC_GetTaperCurrentA10(void);
static UINT8 Get_OpenCircuit_Value(void);
static UINT8 isCHG(void);
static UINT8 isDSG(void);
// 古瑞瓦特
const UINT16 SOC_Table_LiFePO[SOC_Size_LiFePO] = {
	3336,
	100,
	3332,
	90,
	3330,
	80,
	3327,
	75,
	3316,
	70,
	3301,
	65,
	3294,
	60,
	3291,
	55,
	3290,
	50,
	3288,
	45,
	3286,
	40,
	3279,
	35,
	3266,
	30,
	3254,
	25,
	3236,
	20,
	3212,
	15,
	3198,
	10,
	3112,
	5,
	2526,
	0,
	1000,
	0,
	1000,
	0,
};

// 单位为mV和SOC
const UINT16 SocTable_TernaryLi[SOC_Size_TernaryLi] = {
	4126,
	100,
	4066,
	95,
	4011,
	90,
	3955,
	85,
	3888,
	80,
	3837,
	75,
	3793,
	70,
	3756,
	65,
	3724,
	60,
	3699,
	55,
	3675,
	50,
	3658,
	45,
	3632,
	40,
	3605,
	35,
	3584,
	30,
	3557,
	25,
	3535,
	20,
	3497,
	15,
	3475,
	10,
	3371,
	5,
	3136,
	3,
};

// 单位为mV和SOC
const UINT16 SocTable_LiFePO2[SOC_Size_LiFePO2] = {
	3650,
	100,
	3600,
	98,
	3550,
	95,
	3500,
	92,
	3400,
	90,
	3350,
	87,
	3340,
	85,
	3335,
	82,
	3330,
	80,
	3325,
	78,
	3320,
	75,
	3300,
	70,
	3275,
	65,
	3250,
	60,
	3200,
	50,
	3150,
	45,
	3100,
	30,
	3000,
	20,
	2850,
	10,
	2750,
	5,
	2650,
	0,
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

	if (System_OnOFF_Func.bits.b1OnOFF_SOC_Fixed)
	{
		g_stCellInfoReport.SocElement.u16Soc = 60U;
	}
	if (System_OnOFF_Func.bits.b1OnOFF_SOC_Zero)
	{
		g_stCellInfoReport.SocElement.u16Soc = 0U;
	}
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

	if (SOC_Enhance_Element.u16_VCellMax < SOC_Enhance_Element.u16_VCellMin)
	{
		return 0U;
	}

	return 1U;
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

static UINT32 SOC_GetCapBase(void)
{
	if (SOC_Calculate_Element.u32CapFull != 0U)
	{
		return SOC_Calculate_Element.u32CapFull;
	}

	return SOC_Calculate_Element.u32CapFactory;
}

static void SOC_SelectIntegrateDirection(UINT8 direction)
{
	if (g_soc_runtime.u8IntegrateDirection != direction)
	{
		SOC_Calculate_Element.u32CapChange = 0U;
		SOC_Calculate_Element.u32IntegrateRemainderMs = 0U;
		g_soc_runtime.u16ChgTerminalTicks = 0U;
		g_soc_runtime.u16DsgTerminalTicks = 0U;
		g_soc_runtime.u16FullConfirmTicks = 0U;
		g_soc_runtime.u8IntegrateDirection = direction;
	}
}

static UINT8 SOC_ApplyCapacityDelta(enum _CUR current_type, UINT16 current)
{
	UINT8 old_soc;
	UINT8 new_soc;
	UINT8 direction;
	UINT32 cap_base;
	UINT32 delta_as10;
	UINT32 integrate_acc_ms;
	UINT32 percent;
	uint64_t used_as10;

	cap_base = SOC_GetCapBase();
	if ((cap_base == 0U) || (current == 0U))
	{
		return 0U;
	}

	direction = (current_type == CurCHG) ? SOC_INTEGRATE_DIRECTION_CHG : SOC_INTEGRATE_DIRECTION_DSG;
	SOC_SelectIntegrateDirection(direction);

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

	SOC_AddLearnPassed(direction, delta_as10);
	SOC_Calculate_Element.u32CapChange += delta_as10;
	percent = (UINT32)(((uint64_t)SOC_Calculate_Element.u32CapChange * 100ULL) / (uint64_t)cap_base);
	if (percent > 100U)
	{
		percent = 100U;
	}

	if ((current_type == CurCHG) && (SOC_Calculate_Element.u32CapNow >= cap_base))
	{
		if (SOC_Calculate_Element.u8SOC_Now < 100U)
		{
			SOC_Calculate_Element.u8SOC_Now = 99U;
		}
		SOC_Calculate_Element.u32CapChange = 0U;
		SOC_Calculate_Element.u32IntegrateRemainderMs = 0U;
	}
	else if ((current_type == CurDSG) && (SOC_Calculate_Element.u32CapNow == 0U))
	{
		SOC_Calculate_Element.u8SOC_Now = 0U;
		SOC_Calculate_Element.u32CapChange = 0U;
		SOC_Calculate_Element.u32IntegrateRemainderMs = 0U;
		SOC_OnTrustedEmptyAnchor();
	}
	else if (percent != 0U)
	{
		if (current_type == CurCHG)
		{
			if (SOC_Calculate_Element.u8SOC_Now < 100U)
			{
				if ((SOC_Calculate_Element.u8SOC_Now >= 99U) ||
					(percent >= (UINT32)(99U - SOC_Calculate_Element.u8SOC_Now)))
				{
					SOC_Calculate_Element.u8SOC_Now = 99U;
				}
				else
				{
					SOC_Calculate_Element.u8SOC_Now = (UINT8)(SOC_Calculate_Element.u8SOC_Now + percent);
				}
			}
		}
		else
		{
			if (SOC_Calculate_Element.u8SOC_Now > percent)
			{
				SOC_Calculate_Element.u8SOC_Now = (UINT8)(SOC_Calculate_Element.u8SOC_Now - percent);
			}
			else
			{
				SOC_Calculate_Element.u8SOC_Now = 0U;
			}
		}

		used_as10 = ((uint64_t)percent * (uint64_t)cap_base + 50ULL) / 100ULL;
		if (SOC_Calculate_Element.u32CapChange > used_as10)
		{
			SOC_Calculate_Element.u32CapChange -= (UINT32)used_as10;
		}
		else
		{
			SOC_Calculate_Element.u32CapChange = 0U;
		}
	}

	new_soc = SOC_Calculate_Element.u8SOC_Now;
	return (old_soc > new_soc) ? (UINT8)(old_soc - new_soc) : (UINT8)(new_soc - old_soc);
}


static void SOC_ResetLearningState(void)
{
	SOC_Calculate_Element.u32LearnPassedAs10 = 0U;
	SOC_Calculate_Element.u16LearnAnchorSoc = 0U;
	SOC_Calculate_Element.u16LearnState = SOC_LEARN_STATE_NONE;
}

static void SOC_AddLearnPassed(UINT8 direction, UINT32 delta_as10)
{
	if ((delta_as10 == 0U) || (SOC_Calculate_Element.u16LearnState == SOC_LEARN_STATE_NONE))
	{
		return;
	}

	if (((SOC_Calculate_Element.u16LearnState == SOC_LEARN_STATE_FULL_ANCHOR) && (direction == SOC_INTEGRATE_DIRECTION_DSG)) ||
		((SOC_Calculate_Element.u16LearnState == SOC_LEARN_STATE_EMPTY_ANCHOR) && (direction == SOC_INTEGRATE_DIRECTION_CHG)))
	{
		if (SOC_Calculate_Element.u32LearnPassedAs10 <= (0xFFFFFFFFU - delta_as10))
		{
			SOC_Calculate_Element.u32LearnPassedAs10 += delta_as10;
		}
		else
		{
			SOC_Calculate_Element.u32LearnPassedAs10 = 0xFFFFFFFFU;
		}
	}
	else if (SOC_Calculate_Element.u32LearnPassedAs10 != 0U)
	{
		SOC_ResetLearningState();
	}
}

static void SOC_ApplyLearnedFullCapacity(UINT32 learned_cap)
{
	UINT32 min_cap;
	UINT32 max_cap;
	UINT32 max_step;
	UINT32 old_cap;
	UINT32 delta;

	if (SOC_Calculate_Element.u32CapFactory == 0U)
	{
		return;
	}

	min_cap = (UINT32)(((uint64_t)SOC_Calculate_Element.u32CapFactory * SOC_LEARN_CAP_MIN_PERCENT) / 100ULL);
	max_cap = (UINT32)(((uint64_t)SOC_Calculate_Element.u32CapFactory * SOC_LEARN_CAP_MAX_PERCENT) / 100ULL);
	if ((learned_cap < min_cap) || (learned_cap > max_cap))
	{
		return;
	}

	old_cap = SOC_GetCapBase();
	if (old_cap == 0U)
	{
		old_cap = SOC_Calculate_Element.u32CapFactory;
	}

	max_step = (UINT32)(((uint64_t)old_cap * SOC_LEARN_CAP_MAX_STEP_PERCENT) / 100ULL);
	if (max_step == 0U)
	{
		max_step = 1U;
	}

	if (learned_cap > old_cap)
	{
		delta = learned_cap - old_cap;
		SOC_Calculate_Element.u32CapFull = old_cap + ((delta > max_step) ? max_step : delta);
	}
	else
	{
		delta = old_cap - learned_cap;
		SOC_Calculate_Element.u32CapFull = old_cap - ((delta > max_step) ? max_step : delta);
	}

	SOC_Calculate_Element.u16LearnFlags |= SOC_LEARN_FLAG_LEARNED;
	SOC_Calculate_Element.u16MaxErrorPercent = SOC_MAX_ERROR_LEARNED_PERCENT;
}

static void SOC_OnTrustedFullAnchor(void)
{
	UINT16 span;
	UINT16 required_span;
	UINT32 learned_cap;

	if (SOC_Calculate_Element.u16LearnState == SOC_LEARN_STATE_EMPTY_ANCHOR)
	{
		span = (SOC_Calculate_Element.u16LearnAnchorSoc <= 100U) ?
			(UINT16)(100U - SOC_Calculate_Element.u16LearnAnchorSoc) : 0U;
		required_span = (SOC_Calculate_Element.u16LearnFlags & SOC_LEARN_FLAG_LEARNED) ?
			SOC_LEARN_NEXT_SPAN_PERCENT : SOC_LEARN_FIRST_SPAN_PERCENT;
		if ((span >= required_span) && (SOC_Calculate_Element.u32LearnPassedAs10 != 0U))
		{
			learned_cap = (UINT32)(((uint64_t)SOC_Calculate_Element.u32LearnPassedAs10 * 100ULL) / span);
			SOC_ApplyLearnedFullCapacity(learned_cap);
		}
	}

	SOC_Calculate_Element.u16LearnState = SOC_LEARN_STATE_FULL_ANCHOR;
	SOC_Calculate_Element.u16LearnAnchorSoc = 100U;
	SOC_Calculate_Element.u32LearnPassedAs10 = 0U;
	SOC_Calculate_Element.u32CapNow = SOC_GetCapBase();
}

static void SOC_OnTrustedEmptyAnchor(void)
{
	UINT16 span;
	UINT16 required_span;
	UINT32 learned_cap;

	if (SOC_Calculate_Element.u16LearnState == SOC_LEARN_STATE_FULL_ANCHOR)
	{
		span = (SOC_Calculate_Element.u16LearnAnchorSoc <= 100U) ?
			SOC_Calculate_Element.u16LearnAnchorSoc : 0U;
		required_span = (SOC_Calculate_Element.u16LearnFlags & SOC_LEARN_FLAG_LEARNED) ?
			SOC_LEARN_NEXT_SPAN_PERCENT : SOC_LEARN_FIRST_SPAN_PERCENT;
		if ((span >= required_span) && (SOC_Calculate_Element.u32LearnPassedAs10 != 0U))
		{
			learned_cap = (UINT32)(((uint64_t)SOC_Calculate_Element.u32LearnPassedAs10 * 100ULL) / span);
			SOC_ApplyLearnedFullCapacity(learned_cap);
		}
	}

	SOC_Calculate_Element.u16LearnState = SOC_LEARN_STATE_EMPTY_ANCHOR;
	SOC_Calculate_Element.u16LearnAnchorSoc = 0U;
	SOC_Calculate_Element.u32LearnPassedAs10 = 0U;
	SOC_Calculate_Element.u32CapNow = 0U;
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

static void SOC_SetSocValue(UINT8 soc_now, UINT8 clear_cap_change)
{
	UINT32 cap_base;

	if (soc_now > 100U)
	{
		soc_now = 100U;
	}

	cap_base = SOC_GetCapBase();
	SOC_Calculate_Element.u8SOC_Now = soc_now;
	if (cap_base == 0U)
	{
		SOC_Calculate_Element.u32CapNow = 0U;
	}
	else
	{
		SOC_Calculate_Element.u32CapNow = (UINT32)(((uint64_t)soc_now * (uint64_t)cap_base) / 100ULL);
	}

	if (clear_cap_change)
	{
		SOC_Calculate_Element.u32CapChange = 0U;
		SOC_Calculate_Element.u32IntegrateRemainderMs = 0U;
		g_soc_runtime.u8IntegrateDirection = SOC_INTEGRATE_DIRECTION_IDLE;
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
		(SOC_Calculate_Element.u16LearnAnchorSoc != SOC_Calculate_Element_backup.u16LearnAnchorSoc) ||
		(SOC_Calculate_Element.u16LearnState != SOC_Calculate_Element_backup.u16LearnState) ||
		(SOC_Calculate_Element.u16MaxErrorPercent != SOC_Calculate_Element_backup.u16MaxErrorPercent) ||
		(SOC_Calculate_Element.u16LearnFlags != SOC_Calculate_Element_backup.u16LearnFlags))
	{
		if (SOC_DealEEPROM_Data(EEPROM_DATA_REFRESH))
		{
			SOC_Calculate_Element_backup = SOC_Calculate_Element;
		}
	}
}

static UINT8 SOC_GetRestBucket(UINT32 rest_seconds)
{
	if (rest_seconds < SOC_REST_BUCKET_1_SECONDS)
	{
		return 0U;
	}
	if (rest_seconds < SOC_REST_BUCKET_2_SECONDS)
	{
		return 1U;
	}
	if (rest_seconds < SOC_REST_BUCKET_3_SECONDS)
	{
		return 2U;
	}
	if (rest_seconds < SOC_REST_BUCKET_4_SECONDS)
	{
		return 3U;
	}

	return 4U;
}


static void SOC_ResetRestMonitor(void)
{
	g_soc_runtime.u32RestTicks = 0U;
	g_soc_runtime.u8RestBucketApplied = 0U;
	g_soc_runtime.u16RelaxStableTicks = 0U;
	g_soc_runtime.u16RelaxRefVCellMin = 0U;
	g_soc_runtime.u16RelaxRefVCellMax = 0U;
}

static void SOC_UpdateRelaxVoltageStable(void)
{
	UINT16 diff_min;
	UINT16 diff_max;
	UINT16 stable_limit;

	stable_limit = (UINT16)(SOC_RELAX_STABLE_SECONDS * SOC_TICKS_PER_SECOND);
	if ((g_soc_runtime.u16RelaxRefVCellMin == 0U) || (g_soc_runtime.u16RelaxRefVCellMax == 0U))
	{
		g_soc_runtime.u16RelaxRefVCellMin = SOC_Enhance_Element.u16_VCellMin;
		g_soc_runtime.u16RelaxRefVCellMax = SOC_Enhance_Element.u16_VCellMax;
		g_soc_runtime.u16RelaxStableTicks = 0U;
		return;
	}

	diff_min = (SOC_Enhance_Element.u16_VCellMin >= g_soc_runtime.u16RelaxRefVCellMin) ?
		(UINT16)(SOC_Enhance_Element.u16_VCellMin - g_soc_runtime.u16RelaxRefVCellMin) :
		(UINT16)(g_soc_runtime.u16RelaxRefVCellMin - SOC_Enhance_Element.u16_VCellMin);
	diff_max = (SOC_Enhance_Element.u16_VCellMax >= g_soc_runtime.u16RelaxRefVCellMax) ?
		(UINT16)(SOC_Enhance_Element.u16_VCellMax - g_soc_runtime.u16RelaxRefVCellMax) :
		(UINT16)(g_soc_runtime.u16RelaxRefVCellMax - SOC_Enhance_Element.u16_VCellMax);

	if ((diff_min <= SOC_RELAX_VOLT_STABLE_WINDOW_MV) && (diff_max <= SOC_RELAX_VOLT_STABLE_WINDOW_MV))
	{
		if (g_soc_runtime.u16RelaxStableTicks < stable_limit)
		{
			++g_soc_runtime.u16RelaxStableTicks;
		}
		return;
	}

	g_soc_runtime.u16RelaxRefVCellMin = SOC_Enhance_Element.u16_VCellMin;
	g_soc_runtime.u16RelaxRefVCellMax = SOC_Enhance_Element.u16_VCellMax;
	g_soc_runtime.u16RelaxStableTicks = 0U;
}

static UINT8 SOC_IsRelaxVoltageStable(void)
{
	return (UINT8)(g_soc_runtime.u16RelaxStableTicks >= (UINT16)(SOC_RELAX_STABLE_SECONDS * SOC_TICKS_PER_SECOND));
}

static void SOC_ApplyRestCompensation(UINT32 rest_seconds)
{
	UINT8 bucket;
	UINT8 target_soc;
	UINT8 current_soc;
	UINT8 new_soc;
	UINT8 max_step = 0U;

	if (!SOC_Enhance_Element.u16_SOC_InitOver)
	{
		return;
	}

	if (!SOC_IsCalibrationAllowed() || isCHG() || isDSG())
	{
		return;
	}
	if ((g_soc_runtime.u32RestTicks != 0U) && !SOC_IsRelaxVoltageStable())
	{
		return;
	}

	bucket = SOC_GetRestBucket(rest_seconds);
	if ((bucket == 0U) || (bucket <= g_soc_runtime.u8RestBucketApplied))
	{
		return;
	}

	target_soc = Get_OpenCircuit_Value();
	current_soc = SOC_Calculate_Element.u8SOC_Now;
	if (target_soc < current_soc)
	{
		if (bucket >= 4U)
		{
			max_step = 3U;
		}
		else if (bucket >= 3U)
		{
			max_step = 2U;
		}
		else
		{
			max_step = 1U;
		}
		if (((SOC_Enhance_Element.u16_SOC_TableSelect == SOC_TABLE_LIFEPO) ||
			 (SOC_Enhance_Element.u16_SOC_TableSelect == SOC_TABLE_LIFEPO2)) &&
			(current_soc > 20U) && (target_soc < 90U) && (max_step > 1U))
		{
			max_step = 1U;
		}
	}
	else if ((target_soc > current_soc) && (current_soc >= 90U) && (bucket >= 3U))
	{
		max_step = (bucket >= 4U) ? 2U : 1U;
	}

	new_soc = SOC_StepTowardTarget(current_soc, target_soc, max_step);
	if (new_soc != current_soc)
	{
		SOC_ApplySocCorrection(new_soc);
	}
	g_soc_runtime.u8RestBucketApplied = bucket;
}

static void SOC_UpdateRestMonitor(void)
{
	UINT32 rest_seconds;

	if (!SOC_Enhance_Element.u16_SOC_InitOver)
	{
		return;
	}

	if ((g_soc_runtime.u8Mode != SOC_MODE_RELAX) || !SOC_IsCalibrationAllowed())
	{
		SOC_ResetRestMonitor();
		return;
	}

	SOC_UpdateRelaxVoltageStable();
	if (g_soc_runtime.u32RestTicks < 0xFFFFFFFFU)
	{
		++g_soc_runtime.u32RestTicks;
	}

	rest_seconds = g_soc_runtime.u32RestTicks / SOC_TICKS_PER_SECOND;
	SOC_ApplyRestCompensation(rest_seconds);
}

static void SOC_ApplyWeakCellGuard(void)
{
	UINT8 guard_soc = 100U;
	UINT8 target_soc;
	UINT8 current_soc;
	UINT8 max_step = 1U;

	if (isCHG() || !SOC_IsVoltageValid())
	{
		return;
	}

	if (SOC_Enhance_Element.u16_VCellMin > (UINT16)(SOC_Enhance_Element.u16_SOC_0_Vol + SOC_WEAK_CELL_GUARD_WINDOW_MV))
	{
		return;
	}

	target_soc = Get_OpenCircuit_Value();
	if (target_soc < guard_soc)
	{
		guard_soc = target_soc;
	}

	if (SOC_Enhance_Element.u16_VCellMin <= SOC_Enhance_Element.u16_SOC_0_Vol)
	{
		guard_soc = 0U;
		max_step = 5U;
	}
	else if (SOC_Enhance_Element.u16_VCellMin <= (UINT16)(SOC_Enhance_Element.u16_SOC_0_Vol + SOC_WEAK_CELL_CRITICAL_WINDOW_MV))
	{
		max_step = 2U;
		if (guard_soc > 2U)
		{
			guard_soc = 2U;
		}
	}
	else if (SOC_Enhance_Element.u16_VCellMin <= (UINT16)(SOC_Enhance_Element.u16_SOC_0_Vol + 60U))
	{
		if (guard_soc > 4U)
		{
			guard_soc = 4U;
		}
	}
	else if (SOC_Enhance_Element.u16_VCellMin <= (UINT16)(SOC_Enhance_Element.u16_SOC_0_Vol + 90U))
	{
		if (guard_soc > 6U)
		{
			guard_soc = 6U;
		}
	}
	else
	{
		if (guard_soc > 8U)
		{
			guard_soc = 8U;
		}
	}

	current_soc = SOC_Calculate_Element.u8SOC_Now;
	if (current_soc <= guard_soc)
	{
		return;
	}

	current_soc = SOC_StepTowardTarget(current_soc, guard_soc, max_step);
	SOC_ApplySocCorrection(current_soc);
	if ((current_soc == 0U) && (SOC_Enhance_Element.u16_VCellMin <= SOC_Enhance_Element.u16_SOC_0_Vol))
	{
		SOC_OnTrustedEmptyAnchor();
	}
}

static void SOC_UpdateDisplaySoc(void)
{
	UINT8 target_soc;
	UINT8 step_ready = 0U;
	UINT8 critical_low = 0U;
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

	critical_low = (UINT8)(SOC_IsVoltageValid() &&
		(SOC_Enhance_Element.u16_VCellMin <= (UINT16)(SOC_Enhance_Element.u16_SOC_0_Vol + SOC_WEAK_CELL_CRITICAL_WINDOW_MV)) &&
		(g_soc_runtime.u8DisplaySoc > target_soc));
	step_limit = critical_low ? 1U : (UINT8)(SOC_DISPLAY_STEP_SECONDS * SOC_TICKS_PER_SECOND);
	if (step_limit == 0U)
	{
		step_limit = 1U;
	}

	if (++g_soc_runtime.u8DisplayStepTicks >= step_limit)
	{
		g_soc_runtime.u8DisplayStepTicks = 0U;
		step_ready = 1U;
	}

	if (!step_ready)
	{
		return;
	}

	if (g_soc_runtime.u8DisplaySoc < target_soc)
	{
		++g_soc_runtime.u8DisplaySoc;
		if (g_soc_runtime.u8DisplaySoc > target_soc)
		{
			g_soc_runtime.u8DisplaySoc = target_soc;
		}
		return;
	}

	if (g_soc_runtime.u8DisplaySoc > target_soc)
	{
		--g_soc_runtime.u8DisplaySoc;
		if (g_soc_runtime.u8DisplaySoc < target_soc)
		{
			g_soc_runtime.u8DisplaySoc = target_soc;
		}
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
	if (force_display_follow)
	{
		g_soc_runtime.u8DisplaySoc = SOC_Calculate_Element.u8SOC_Now;
		g_soc_runtime.u8DisplayReady = 1U;
	}
	else
	{
		SOC_UpdateDisplaySoc();
	}

	SOC_Enhance_Element.u8_SOC = g_soc_runtime.u8DisplaySoc;
	if (SOC_Calculate_Element.u32CapFactory == 0U)
	{
		SOC_Enhance_Element.u8_SOH = 0U;
		SOC_Enhance_Element.u16_CapacityNow = 0U;
		SOC_Enhance_Element.u16_CapacityFull = 0U;
		SOC_Enhance_Element.u16_CapacityFactory = 0U;
	}
	else if (SOC_Calculate_Element.u32CapFull >= SOC_Calculate_Element.u32CapFactory)
	{
		SOC_Enhance_Element.u8_SOH = 100U;
		SOC_Enhance_Element.u16_CapacityNow = SOC_CapAs10ToAh100(SOC_Calculate_Element.u32CapNow);
		SOC_Enhance_Element.u16_CapacityFull = SOC_CapAs10ToAh100(SOC_Calculate_Element.u32CapFull);
		SOC_Enhance_Element.u16_CapacityFactory = SOC_CapAs10ToAh100(SOC_Calculate_Element.u32CapFactory);
	}
	else
	{
		SOC_Enhance_Element.u8_SOH = (UINT8)(((uint64_t)100U * (uint64_t)SOC_Calculate_Element.u32CapFull) / (uint64_t)SOC_Calculate_Element.u32CapFactory);
		SOC_Enhance_Element.u16_CapacityNow = SOC_CapAs10ToAh100(SOC_Calculate_Element.u32CapNow);
		SOC_Enhance_Element.u16_CapacityFull = SOC_CapAs10ToAh100(SOC_Calculate_Element.u32CapFull);
		SOC_Enhance_Element.u16_CapacityFactory = SOC_CapAs10ToAh100(SOC_Calculate_Element.u32CapFactory);
	}
	if ((SOC_Calculate_Element.u32Cycle_times / 100U) > 0xFFFFU)
	{
		SOC_Enhance_Element.u16_Cycle_times = 0xFFFFU;
	}
	else
	{
		SOC_Enhance_Element.u16_Cycle_times = (UINT16)(SOC_Calculate_Element.u32Cycle_times / 100U);
	}

	SOC_Enhance_Element.u8_SOC_OCV_Cali = SOC_Calculate_Element.u8DSG_SOC_Int; // 留着，自己知道

	SOC_PublishReportData();
}

static void SOC_LoadFactoryRuntimeConfig(void)
{
	SOC_Calculate_Element.u32CapFactory = (UINT32)SOC_Enhance_Element.u16_SOC_Ah * 3600U;
	SOC_Calculate_Element.u32Cycle_times = (UINT32)SOC_Enhance_Element.u16_SOC_CycleT_Ever * 100U;
}

static void SOC_ResetCalculateState(void)
{
	memset(&SOC_Calculate_Element, 0, sizeof(SOC_Calculate_Element));
}

static void SOC_LoadDefaultSnapshot(void)
{
	SOC_Calculate_Element.u8SOC_Now = SOC_DEFAULT_STARTUP_PERCENT;
	SOC_Calculate_Element.u8DSG_SOC_Int = 0U;
	SOC_Calculate_Element.u32Cycle_times = (UINT32)SOC_Enhance_Element.u16_SOC_CycleT_Ever * 100U;
	SOC_Calculate_Element.u32CapFull = SOC_Calculate_Element.u32CapFactory;
	SOC_Calculate_Element.u32CapNow = (UINT32)SOC_Calculate_Element.u8SOC_Now * SOC_Calculate_Element.u32CapFactory / 100U;
	SOC_Calculate_Element.u16MaxErrorPercent = SOC_MAX_ERROR_DEFAULT_PERCENT;
	SOC_Calculate_Element.u16LearnFlags = 0U;
	SOC_ResetLearningState();
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
	flash_data.u16MaxErrorPercent = SOC_MAX_ERROR_DEFAULT_PERCENT;
	flash_data.u32CycleTimes = (UINT32)OtherElement.u16Soc_Cycle_times * 100U;
	flash_data.u32CapFull = cap_factory;
	flash_data.u32CapNow = (UINT32)SOC_DEFAULT_STARTUP_PERCENT * cap_factory / 100U;

	return StorageFlash_SaveSocData(&flash_data);
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
	UINT16 delta_mv;

	if (SOC_FULL_CONFIRM_MAX_CELL_DELTA_MV == 0U)
	{
		return 1U;
	}

	delta_mv = (SOC_Enhance_Element.u16_VCellMax >= SOC_Enhance_Element.u16_VCellMin) ?
		(UINT16)(SOC_Enhance_Element.u16_VCellMax - SOC_Enhance_Element.u16_VCellMin) :
		(UINT16)(SOC_Enhance_Element.u16_VCellMin - SOC_Enhance_Element.u16_VCellMax);

	return (UINT8)(delta_mv <= SOC_FULL_CONFIRM_MAX_CELL_DELTA_MV);
}

static UINT16 SOC_GetCurrentLimitA10(UINT16 divider)
{
	UINT16 limit;

	if (divider == 0U)
	{
		divider = 1U;
	}

	limit = (UINT16)(SOC_Enhance_Element.u16_SOC_Ah / divider);
	if (limit < SOC_CURRENT_ENTER_A10)
	{
		limit = SOC_CURRENT_ENTER_A10;
	}

	return limit;
}

static UINT8 SOC_TerminalCounterReady(UINT16 *counter, UINT16 limit_ticks)
{
	if (counter == 0)
	{
		return 0U;
	}

	if (++(*counter) >= limit_ticks)
	{
		*counter = 0U;
		return 1U;
	}

	return 0U;
}

static void SOC_ApplyTerminalCorrection(UINT8 direction)
{
	UINT16 limit_ticks = 0U;
	UINT16 chg_near_full;
	UINT16 dsg_near_empty;

	if (direction == SOC_INTEGRATE_DIRECTION_CHG)
	{
		g_soc_runtime.u16DsgTerminalTicks = 0U;
		chg_near_full = (SOC_Enhance_Element.u16_SOC_100_Vol > 100U) ?
			(UINT16)(SOC_Enhance_Element.u16_SOC_100_Vol - 100U) : 0U;

		if ((SOC_Enhance_Element.u16_VCellMax >= (UINT16)(SOC_Enhance_Element.u16_SOC_100_Vol + 50U)) &&
			(SOC_Calculate_Element.u8SOC_Now < 100U))
		{
			limit_ticks = (UINT16)(2U * SOC_TICKS_PER_SECOND);
		}
		else if ((SOC_Enhance_Element.u16_VCellMax >= SOC_Enhance_Element.u16_SOC_100_Vol) &&
				 (SOC_Calculate_Element.u8SOC_Now < 100U))
		{
			limit_ticks = (SOC_Calculate_Element.u8SOC_Now > 95U) ?
				(UINT16)(8U * SOC_TICKS_PER_SECOND) : (UINT16)(4U * SOC_TICKS_PER_SECOND);
		}
		else if ((SOC_Enhance_Element.u16_VCellMax >= chg_near_full) &&
				 (SOC_Enhance_Element.u16_VCellMax < SOC_Enhance_Element.u16_SOC_100_Vol) &&
				 (SOC_Calculate_Element.u8SOC_Now < 95U))
		{
			limit_ticks = (UINT16)(10U * SOC_TICKS_PER_SECOND);
		}
		else
		{
			g_soc_runtime.u16ChgTerminalTicks = 0U;
		}

		if ((limit_ticks != 0U) && SOC_TerminalCounterReady(&g_soc_runtime.u16ChgTerminalTicks, limit_ticks))
		{
			if (SOC_Calculate_Element.u8SOC_Now < 99U)
			{
				SOC_ApplySocCorrection((UINT8)(SOC_Calculate_Element.u8SOC_Now + 1U));
			}
		}
	}
	else if (direction == SOC_INTEGRATE_DIRECTION_DSG)
	{
		g_soc_runtime.u16ChgTerminalTicks = 0U;
		dsg_near_empty = (UINT16)(SOC_Enhance_Element.u16_SOC_0_Vol + 100U);

		if ((SOC_Enhance_Element.u16_SOC_0_Vol > 50U) &&
			(SOC_Enhance_Element.u16_VCellMin <= (UINT16)(SOC_Enhance_Element.u16_SOC_0_Vol - 50U)) &&
			(SOC_Calculate_Element.u8SOC_Now > 0U))
		{
			limit_ticks = (UINT16)(2U * SOC_TICKS_PER_SECOND);
		}
		else if ((SOC_Enhance_Element.u16_VCellMin <= SOC_Enhance_Element.u16_SOC_0_Vol) &&
				 (SOC_Calculate_Element.u8SOC_Now > 0U))
		{
			limit_ticks = (SOC_Calculate_Element.u8SOC_Now < 5U) ?
				(UINT16)(8U * SOC_TICKS_PER_SECOND) : (UINT16)(4U * SOC_TICKS_PER_SECOND);
		}
		else if ((SOC_Enhance_Element.u16_VCellMin <= dsg_near_empty) &&
				 (SOC_Enhance_Element.u16_VCellMin > SOC_Enhance_Element.u16_SOC_0_Vol) &&
				 (SOC_Calculate_Element.u8SOC_Now > 5U))
		{
			limit_ticks = (UINT16)(10U * SOC_TICKS_PER_SECOND);
		}
		else
		{
			g_soc_runtime.u16DsgTerminalTicks = 0U;
		}

		if ((limit_ticks != 0U) && SOC_TerminalCounterReady(&g_soc_runtime.u16DsgTerminalTicks, limit_ticks))
		{
			SOC_ApplySocCorrection((UINT8)(SOC_Calculate_Element.u8SOC_Now - 1U));
		}
	}
	else
	{
		g_soc_runtime.u16ChgTerminalTicks = 0U;
		g_soc_runtime.u16DsgTerminalTicks = 0U;
	}
}

static UINT8 SOC_GetMeasuredCurrentDirection(void)
{
	if ((SOC_Enhance_Element.u16_Ichg >= SOC_CURRENT_ENTER_A10) &&
		(SOC_Enhance_Element.u16_Ichg >= SOC_Enhance_Element.u16_Idsg))
	{
		return SOC_INTEGRATE_DIRECTION_CHG;
	}

	if (SOC_Enhance_Element.u16_Idsg >= SOC_CURRENT_ENTER_A10)
	{
		return SOC_INTEGRATE_DIRECTION_DSG;
	}

	return SOC_INTEGRATE_DIRECTION_IDLE;
}

static UINT8 SOC_GetCurrentDirection(void)
{
	UINT8 measured_direction;
	UINT16 relax_limit_ticks;

	measured_direction = SOC_GetMeasuredCurrentDirection();
	if (measured_direction == SOC_INTEGRATE_DIRECTION_CHG)
	{
		g_soc_runtime.u8Mode = SOC_MODE_CHG;
		g_soc_runtime.u16RelaxEntryTicks = 0U;
		SOC_ResetRestMonitor();
		return SOC_INTEGRATE_DIRECTION_CHG;
	}
	if (measured_direction == SOC_INTEGRATE_DIRECTION_DSG)
	{
		g_soc_runtime.u8Mode = SOC_MODE_DSG;
		g_soc_runtime.u16RelaxEntryTicks = 0U;
		SOC_ResetRestMonitor();
		return SOC_INTEGRATE_DIRECTION_DSG;
	}

	relax_limit_ticks = (UINT16)(SOC_MODE_RELAX_ENTRY_SECONDS * SOC_TICKS_PER_SECOND);
	if (g_soc_runtime.u16RelaxEntryTicks < relax_limit_ticks)
	{
		++g_soc_runtime.u16RelaxEntryTicks;
	}
	if (g_soc_runtime.u16RelaxEntryTicks >= relax_limit_ticks)
	{
		g_soc_runtime.u8Mode = SOC_MODE_RELAX;
	}

	return SOC_INTEGRATE_DIRECTION_IDLE;
}

static void SOC_RunCoulombCounter(UINT8 direction)
{
	UINT8 delta_soc;

	if (direction == SOC_INTEGRATE_DIRECTION_CHG)
	{
		SOC_ApplyTerminalCorrection(direction);
		(void)SOC_ApplyCapacityDelta(CurCHG, SOC_Enhance_Element.u16_Ichg);
	}
	else if (direction == SOC_INTEGRATE_DIRECTION_DSG)
	{
		SOC_ApplyTerminalCorrection(direction);
		delta_soc = SOC_ApplyCapacityDelta(CurDSG, SOC_Enhance_Element.u16_Idsg);
		SOC_AddDischargeCyclePercent(delta_soc);
	}
	else
	{
		SOC_SelectIntegrateDirection(SOC_INTEGRATE_DIRECTION_IDLE);
		SOC_ApplyTerminalCorrection(SOC_INTEGRATE_DIRECTION_IDLE);
	}
}

static void SOC_ResetOnlineOcvGuard(void)
{
	g_soc_runtime.u16OnlineOcvTicks = 0U;
	g_soc_runtime.u8OnlineOcvDirection = SOC_INTEGRATE_DIRECTION_IDLE;
}

static UINT8 SOC_IsOnlineOcvTargetTrusted(UINT8 target_soc)
{
	if ((target_soc < SOC_ONLINE_OCV_TARGET_MIN_PERCENT) ||
		(target_soc > SOC_ONLINE_OCV_TARGET_MAX_PERCENT))
	{
		return 0U;
	}

	if (((SOC_Enhance_Element.u16_SOC_TableSelect == SOC_TABLE_LIFEPO) ||
		 (SOC_Enhance_Element.u16_SOC_TableSelect == SOC_TABLE_LIFEPO2)) &&
		(target_soc > SOC_ONLINE_OCV_LFP_LOW_EDGE_PERCENT) &&
		(target_soc < SOC_ONLINE_OCV_LFP_HIGH_EDGE_PERCENT))
	{
		return 0U;
	}

	return 1U;
}

static UINT8 SOC_IsOnlineOcvLightCurrent(UINT8 direction)
{
	UINT16 current;
	UINT16 limit;

	if (direction == SOC_INTEGRATE_DIRECTION_CHG)
	{
		current = SOC_Enhance_Element.u16_Ichg;
	}
	else if (direction == SOC_INTEGRATE_DIRECTION_DSG)
	{
		current = SOC_Enhance_Element.u16_Idsg;
	}
	else
	{
		return 0U;
	}

	limit = SOC_GetCurrentLimitA10(SOC_ONLINE_OCV_CURRENT_DIVIDER);
	return (UINT8)((current >= SOC_CURRENT_ENTER_A10) && (current <= limit));
}

static void SOC_ApplyOnlineOcvGuard(UINT8 direction)
{
	UINT8 target_soc;
	UINT8 current_soc;
	UINT8 new_soc;
	UINT16 limit_ticks;

	if ((!SOC_ONLINE_OCV_GUARD_ENABLE) ||
		(!SOC_Enhance_Element.u16_SOC_InitOver) ||
		(!SOC_IsCalibrationAllowed()) ||
		((direction != SOC_INTEGRATE_DIRECTION_CHG) && (direction != SOC_INTEGRATE_DIRECTION_DSG)) ||
		(!SOC_IsOnlineOcvLightCurrent(direction)))
	{
		SOC_ResetOnlineOcvGuard();
		return;
	}

	target_soc = Get_OpenCircuit_Value();
	if (!SOC_IsOnlineOcvTargetTrusted(target_soc))
	{
		SOC_ResetOnlineOcvGuard();
		return;
	}

	current_soc = SOC_Calculate_Element.u8SOC_Now;
	if (direction == SOC_INTEGRATE_DIRECTION_CHG)
	{
		if (target_soc > SOC_ONLINE_OCV_TARGET_MAX_PERCENT)
		{
			target_soc = SOC_ONLINE_OCV_TARGET_MAX_PERCENT;
		}
		if (target_soc <= (UINT8)(current_soc + SOC_ONLINE_OCV_MIN_DELTA_PERCENT))
		{
			SOC_ResetOnlineOcvGuard();
			return;
		}
	}
	else
	{
		if (target_soc < SOC_ONLINE_OCV_TARGET_MIN_PERCENT)
		{
			target_soc = SOC_ONLINE_OCV_TARGET_MIN_PERCENT;
		}
		if (current_soc <= (UINT8)(target_soc + SOC_ONLINE_OCV_MIN_DELTA_PERCENT))
		{
			SOC_ResetOnlineOcvGuard();
			return;
		}
	}

	if (g_soc_runtime.u8OnlineOcvDirection != direction)
	{
		g_soc_runtime.u8OnlineOcvDirection = direction;
		g_soc_runtime.u16OnlineOcvTicks = 0U;
	}

	limit_ticks = (UINT16)(SOC_ONLINE_OCV_CORRECTION_SECONDS * SOC_TICKS_PER_SECOND);
	if (limit_ticks == 0U)
	{
		limit_ticks = 1U;
	}
	if (g_soc_runtime.u16OnlineOcvTicks < limit_ticks)
	{
		++g_soc_runtime.u16OnlineOcvTicks;
	}
	if (g_soc_runtime.u16OnlineOcvTicks < limit_ticks)
	{
		return;
	}

	g_soc_runtime.u16OnlineOcvTicks = 0U;
	new_soc = SOC_StepTowardTarget(current_soc, target_soc, 1U);
	if (new_soc != current_soc)
	{
		SOC_ApplySocCorrection(new_soc);
	}
}

static UINT8 SOC_DealEEPROM_Data(enum EEPROM_COMMAND Command)
{
	STORAGE_FLASH_SOC_DATA flash_data;
	UINT8 valid = 0;
	UINT8 save_ok = 1U;
	UINT32 min_cap;
	UINT32 max_cap;
	UINT32 cap_base;

	switch (Command)
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
		flash_data.u32LearnPassedAs10 = SOC_Calculate_Element.u32LearnPassedAs10;
		flash_data.u16LearnAnchorSoc = SOC_Calculate_Element.u16LearnAnchorSoc;
		flash_data.u16LearnState = SOC_Calculate_Element.u16LearnState;
		flash_data.u16Flags = SOC_Calculate_Element.u16LearnFlags;
		return StorageFlash_SaveSocData(&flash_data);

	case EEPROM_DATA_READ:
		valid = StorageFlash_LoadSocData(&flash_data);
		if (valid)
		{
			if ((flash_data.u16SocNow > 100U) ||
				(flash_data.u16DsgSocInt > 100U) ||
				(flash_data.u16MaxErrorPercent > 100U) ||
				(flash_data.u16LearnAnchorSoc > 100U) ||
				(flash_data.u16LearnState > SOC_LEARN_STATE_EMPTY_ANCHOR))
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
				SOC_MAX_ERROR_DEFAULT_PERCENT : flash_data.u16MaxErrorPercent;
			SOC_Calculate_Element.u16LearnAnchorSoc = flash_data.u16LearnAnchorSoc;
			SOC_Calculate_Element.u16LearnState = flash_data.u16LearnState;
			SOC_Calculate_Element.u16LearnFlags = flash_data.u16Flags;
			SOC_Calculate_Element.u32LearnPassedAs10 = flash_data.u32LearnPassedAs10;

			min_cap = (UINT32)(((uint64_t)SOC_Calculate_Element.u32CapFactory * SOC_LEARN_CAP_MIN_PERCENT) / 100ULL);
			max_cap = (UINT32)(((uint64_t)SOC_Calculate_Element.u32CapFactory * SOC_LEARN_CAP_MAX_PERCENT) / 100ULL);
			if ((flash_data.u32CapFull >= min_cap) && (flash_data.u32CapFull <= max_cap))
			{
				SOC_Calculate_Element.u32CapFull = flash_data.u32CapFull;
			}
			else
			{
				SOC_Calculate_Element.u32CapFull = SOC_Calculate_Element.u32CapFactory;
				SOC_Calculate_Element.u16LearnFlags = 0U;
			}

			cap_base = SOC_GetCapBase();
			if ((flash_data.u32CapNow != 0U) && (flash_data.u32CapNow <= cap_base))
			{
				SOC_Calculate_Element.u32CapNow = flash_data.u32CapNow;
			}
			else
			{
				SOC_Calculate_Element.u32CapNow = (UINT32)(((uint64_t)SOC_Calculate_Element.u8SOC_Now * cap_base) / 100ULL);
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

static void SOC_Update_StartUp(void)
{
	UINT8 target_soc;

	switch (SOC_Enhance_Element.u16_RefreshData_Flag)
	{
	case 1:
		if (!SOC_IsCalibrationAllowed())
		{
			break;
		}
		target_soc = Get_OpenCircuit_Value();
		if ((!isCHG()) && (target_soc > SOC_Calculate_Element.u8SOC_Now))
		{
			target_soc = SOC_Calculate_Element.u8SOC_Now;
		}
		SOC_ResetLearningState();
		SOC_ApplySocNow(target_soc);
		break;

	case 2:
		target_soc = SOC_Calculate_Element.u8SOC_Now;
		SOC_LoadFactoryRuntimeConfig();
		SOC_Calculate_Element.u32CapFull = SOC_Calculate_Element.u32CapFactory;
		SOC_Calculate_Element.u8DSG_SOC_Int = 0U;
		SOC_Calculate_Element.u16MaxErrorPercent = SOC_MAX_ERROR_DEFAULT_PERCENT;
		SOC_Calculate_Element.u16LearnFlags = 0U;
		SOC_ResetLearningState();
		SOC_ApplySocNow(target_soc);
		break;

	case 3:
		SOC_ResetLearningState();
		SOC_Calculate_Element.u16MaxErrorPercent = SOC_MAX_ERROR_DEFAULT_PERCENT;
		SOC_ApplySocNow(SOC_Enhance_Element.u8_SetSocOnce);
		break;

	default:
		break;
	}

	SOC_Enhance_Element.u16_RefreshData_Flag = 0U;
	SOC_ResetRuntimeContext();
	SOC_SyncOutputData(1U);
}

static void SOC_EEPROM_Deal_Monitor(void)
{
	SOC_PersistSnapshotIfChanged();
}

static void SOC_RefreshData_Monitor(void)
{
	if (!SOC_Enhance_Element.u16_SOC_InitOver)
	{
		return;
	}

	if (SOC_Enhance_Element.u16_RefreshData_Flag != 0U)
	{
		SOC_Update_StartUp();
	}
}

static UINT8 isCHG(void)
{
	return (UINT8)((g_soc_runtime.u8Mode == SOC_MODE_CHG) ||
		(SOC_GetMeasuredCurrentDirection() == SOC_INTEGRATE_DIRECTION_CHG));
}

static UINT8 isDSG(void)
{
	return (UINT8)((g_soc_runtime.u8Mode == SOC_MODE_DSG) ||
		(SOC_GetMeasuredCurrentDirection() == SOC_INTEGRATE_DIRECTION_DSG));
}

static UINT16 SOC_GetTaperCurrentA10(void)
{
	return SOC_GetCurrentLimitA10(20U);
}

static void SOC_ApplyVoltageCalibration(void)
{
	UINT16 full_confirm_mv;
	UINT16 confirm_limit_ticks;

	if ((g_soc_runtime.u8Mode != SOC_MODE_CHG) || !SOC_IsCalibrationAllowed())
	{
		g_soc_runtime.u16FullConfirmTicks = 0U;
		return;
	}

	full_confirm_mv = SOC_GetFullCellConfirmVoltage();
	confirm_limit_ticks = (UINT16)(SOC_FULL_CONFIRM_SECONDS * SOC_TICKS_PER_SECOND);
	if ((SOC_Enhance_Element.u16_VCellMax >= SOC_Enhance_Element.u16_SOC_100_Vol) &&
		(SOC_Enhance_Element.u16_VCellMin >= full_confirm_mv) &&
		(SOC_IsFullConfirmCellDeltaValid()) &&
		(SOC_Enhance_Element.u16_Ichg != 0U) &&
		(SOC_Enhance_Element.u16_Ichg <= SOC_GetTaperCurrentA10()))
	{
		if (g_soc_runtime.u16FullConfirmTicks < confirm_limit_ticks)
		{
			++g_soc_runtime.u16FullConfirmTicks;
		}
		if (g_soc_runtime.u16FullConfirmTicks >= confirm_limit_ticks)
		{
			SOC_ApplySocNow(100U);
			SOC_OnTrustedFullAnchor();
			g_soc_runtime.u16FullConfirmTicks = 0U;
		}
		return;
	}

	g_soc_runtime.u16FullConfirmTicks = 0U;
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
	SOC_ApplyVoltageCalibration();
	SOC_ApplyOnlineOcvGuard(direction);
	SOC_UpdateRestMonitor();
	SOC_ApplyWeakCellGuard();
	SOC_EEPROM_Deal_Monitor();
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
	SOC_ApplyRestCompensation(rest_seconds);
	SOC_ApplyWeakCellGuard();
	SOC_PersistSnapshotIfChanged();
	SOC_SyncOutputData(1U);
}
