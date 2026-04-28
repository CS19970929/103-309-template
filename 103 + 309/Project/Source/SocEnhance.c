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
	UINT16 u16SocNow;
	UINT16 u16DsgSocInt;
	UINT32 u32CycleTimes;
} STORAGE_FLASH_SOC_DATA;

extern UINT8 StorageFlash_LoadSocData(STORAGE_FLASH_SOC_DATA *data);
extern UINT8 StorageFlash_SaveSocData(const STORAGE_FLASH_SOC_DATA *data);
#endif

#define SOC_VIRTUAL_CURRENT_CHG (UINT16)2 // A*10，1和2都认为是0，带=号，0.2就开始算了
#define SOC_VIRTUAL_CURRENT_DSG (UINT16)2 // A*10，1和2都认为是0，这个不能为0的同时，把=号判断上去，不然就会卡在DSG那里计算出不来。
#define SOC_INTEGRATE_PERIOD_MS ((UINT32)200U)
#define SOC_INTEGRATE_MS_PER_SECOND ((UINT32)1000U)

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

struct SOC_RUNTIME_CONTEXT
{
	UINT8 u8DisplaySoc;
	UINT8 u8DisplayReady;
	UINT8 u8RestBucketApplied;
	UINT8 u8IntegrateDirection;
	UINT32 u32RestTicks;
};

static struct SOC_RUNTIME_CONTEXT g_soc_runtime;

static void SOC_LoadFactoryRuntimeConfig(void);
static void SOC_ResetCalculateState(void);
static void SOC_LoadDefaultSnapshot(void);
static UINT8 SOC_DealEEPROM_Data(enum EEPROM_COMMAND Command);
static void SOC_ResetRuntimeContext(void);
static UINT8 SOC_IsVoltageValid(void);
static UINT32 SOC_GetCapBase(void);
static void SOC_SelectIntegrateDirection(UINT8 direction);
static UINT8 SOC_ApplyCapacityDelta(enum _CUR current_type, UINT16 current);
static void SOC_AddDischargeCyclePercent(UINT8 delta_soc);
static void SOC_SetSocValue(UINT8 soc_now, UINT8 clear_cap_change);
static void SOC_ApplySocNow(UINT8 soc_now);
static void SOC_ApplySocCorrection(UINT8 soc_now);
static UINT8 SOC_StepTowardTarget(UINT8 current_soc, UINT8 target_soc, UINT8 max_step);
static void SOC_PersistSnapshotIfChanged(void);
static UINT8 SOC_GetRestBucket(UINT32 rest_seconds);
static void SOC_ApplyRestCompensation(UINT32 rest_seconds);
static void SOC_UpdateRestMonitor(void);
static void SOC_ApplyWeakCellGuard(void);
static void SOC_UpdateDisplaySoc(void);
static UINT16 SOC_CapAs10ToAh100(UINT32 cap_as10);
static void SOC_SyncOutputData(UINT8 force_display_follow);
static UINT8 SOC_GetCurrentDirection(void);
static void SOC_RunCoulombCounter(UINT8 direction);
static void SOC_ApplyTerminalCorrection(UINT8 direction);
static UINT16 SOC_GetFullCellConfirmVoltage(void);
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
	return (SOC_Enhance_Element.u16_VCellMin >= 2000U) ? 1U : 0U;
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

	SOC_Calculate_Element.u32CapChange += delta_as10;
	percent = (UINT32)(((uint64_t)SOC_Calculate_Element.u32CapChange * 100ULL) / (uint64_t)cap_base);
	if (percent > 100U)
	{
		percent = 100U;
	}

	if ((current_type == CurCHG) && (SOC_Calculate_Element.u32CapNow >= cap_base))
	{
		SOC_Calculate_Element.u8SOC_Now = 100U;
		SOC_Calculate_Element.u32CapChange = 0U;
		SOC_Calculate_Element.u32IntegrateRemainderMs = 0U;
	}
	else if ((current_type == CurDSG) && (SOC_Calculate_Element.u32CapNow == 0U))
	{
		SOC_Calculate_Element.u8SOC_Now = 0U;
		SOC_Calculate_Element.u32CapChange = 0U;
		SOC_Calculate_Element.u32IntegrateRemainderMs = 0U;
	}
	else if (percent != 0U)
	{
		if (current_type == CurCHG)
		{
			if (SOC_Calculate_Element.u8SOC_Now > (UINT8)(100U - percent))
			{
				SOC_Calculate_Element.u8SOC_Now = 100U;
			}
			else
			{
				SOC_Calculate_Element.u8SOC_Now = (UINT8)(SOC_Calculate_Element.u8SOC_Now + percent);
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
		(SOC_Calculate_Element.u32Cycle_times != SOC_Calculate_Element_backup.u32Cycle_times))
	{
		if (SOC_DealEEPROM_Data(EEPROM_DATA_REFRESH))
		{
			SOC_Calculate_Element_backup.u8SOC_Now = SOC_Calculate_Element.u8SOC_Now;
			SOC_Calculate_Element_backup.u8DSG_SOC_Int = SOC_Calculate_Element.u8DSG_SOC_Int;
			SOC_Calculate_Element_backup.u32Cycle_times = SOC_Calculate_Element.u32Cycle_times;
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

	if (!SOC_IsVoltageValid() || isCHG() || isDSG())
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

	if (isCHG() || isDSG() || !SOC_IsVoltageValid())
	{
		g_soc_runtime.u32RestTicks = 0U;
		g_soc_runtime.u8RestBucketApplied = 0U;
		return;
	}

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
	}
	else if (SOC_Enhance_Element.u16_VCellMin <= (UINT16)(SOC_Enhance_Element.u16_SOC_0_Vol + SOC_WEAK_CELL_CRITICAL_WINDOW_MV))
	{
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

	/* Guard path may only lower one step per execution. */
	current_soc = SOC_StepTowardTarget(current_soc, guard_soc, 1U);
	SOC_ApplySocCorrection(current_soc);
}

static void SOC_UpdateDisplaySoc(void)
{
	UINT8 target_soc;

	target_soc = SOC_Calculate_Element.u8SOC_Now;

	if (!g_soc_runtime.u8DisplayReady)
	{
		g_soc_runtime.u8DisplaySoc = target_soc;
		g_soc_runtime.u8DisplayReady = 1U;
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
		if ((UINT8)(g_soc_runtime.u8DisplaySoc - target_soc) > 1U)
		{
			g_soc_runtime.u8DisplaySoc = (UINT8)(g_soc_runtime.u8DisplaySoc - 1U);
		}
		else
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

	flash_data.u16SocNow = SOC_DEFAULT_STARTUP_PERCENT;
	flash_data.u16DsgSocInt = 0U;
	flash_data.u32CycleTimes = (UINT32)OtherElement.u16Soc_Cycle_times * 100U;

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
	switch (SOC_Enhance_Element.u16_SOC_TableSelect)
	{
	case SOC_TABLE_TERNARYLI:
		return 4000U;
	case SOC_TABLE_LIFEPO:
	case SOC_TABLE_LIFEPO2:
		return 3300U;
	default:
		return (SOC_Enhance_Element.u16_SOC_100_Vol >= 3900U) ? 4000U : 3300U;
	}
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
	static UINT16 s_u16ChgTerminalTicks = 0U;
	static UINT16 s_u16DsgTerminalTicks = 0U;
	UINT16 limit_ticks = 0U;
	UINT16 chg_near_full;
	UINT16 dsg_near_empty;

	if (direction == SOC_INTEGRATE_DIRECTION_CHG)
	{
		s_u16DsgTerminalTicks = 0U;
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
			s_u16ChgTerminalTicks = 0U;
		}

		if ((limit_ticks != 0U) && SOC_TerminalCounterReady(&s_u16ChgTerminalTicks, limit_ticks))
		{
			SOC_ApplySocCorrection((UINT8)(SOC_Calculate_Element.u8SOC_Now + 1U));
		}
	}
	else if (direction == SOC_INTEGRATE_DIRECTION_DSG)
	{
		s_u16ChgTerminalTicks = 0U;
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
			s_u16DsgTerminalTicks = 0U;
		}

		if ((limit_ticks != 0U) && SOC_TerminalCounterReady(&s_u16DsgTerminalTicks, limit_ticks))
		{
			SOC_ApplySocCorrection((UINT8)(SOC_Calculate_Element.u8SOC_Now - 1U));
		}
	}
	else
	{
		s_u16ChgTerminalTicks = 0U;
		s_u16DsgTerminalTicks = 0U;
	}
}

static UINT8 SOC_GetCurrentDirection(void)
{
	if ((SOC_Enhance_Element.u16_Ichg >= SOC_VIRTUAL_CURRENT_CHG) &&
		(SOC_Enhance_Element.u16_Ichg >= SOC_Enhance_Element.u16_Idsg))
	{
		return SOC_INTEGRATE_DIRECTION_CHG;
	}

	if (SOC_Enhance_Element.u16_Idsg >= SOC_VIRTUAL_CURRENT_DSG)
	{
		return SOC_INTEGRATE_DIRECTION_DSG;
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

static UINT8 SOC_DealEEPROM_Data(enum EEPROM_COMMAND Command)
{
	STORAGE_FLASH_SOC_DATA flash_data;
	UINT8 valid = 0;
	UINT8 save_ok = 1U;

	switch (Command)
	{
	case EEPROM_DATA_REFRESH:
		flash_data.u16SocNow = SOC_Calculate_Element.u8SOC_Now;
		flash_data.u16DsgSocInt = SOC_Calculate_Element.u8DSG_SOC_Int;
		flash_data.u32CycleTimes = SOC_Calculate_Element.u32Cycle_times;
		return StorageFlash_SaveSocData(&flash_data);

	case EEPROM_DATA_READ:
		valid = StorageFlash_LoadSocData(&flash_data);
		if (valid)
		{
			if ((flash_data.u16SocNow > 100U) || (flash_data.u16DsgSocInt > 100U))
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
			SOC_Calculate_Element.u8DSG_SOC_Int = (UINT8)flash_data.u16DsgSocInt;
			SOC_Calculate_Element.u32Cycle_times = flash_data.u32CycleTimes;
			SOC_Calculate_Element.u32CapFull = (UINT32)SOC_Calculate_Element.u32CapFactory;
			SOC_ApplySocNow((UINT8)flash_data.u16SocNow);
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
		target_soc = Get_OpenCircuit_Value();
		if ((!isCHG()) && (target_soc > SOC_Calculate_Element.u8SOC_Now))
		{
			target_soc = SOC_Calculate_Element.u8SOC_Now;
		}
		SOC_ApplySocNow(target_soc);
		break;

	case 2:
		target_soc = SOC_Calculate_Element.u8SOC_Now;
		SOC_LoadFactoryRuntimeConfig();
		SOC_Calculate_Element.u32CapFull = SOC_Calculate_Element.u32CapFactory;
		SOC_Calculate_Element.u8DSG_SOC_Int = 0U;
		SOC_ApplySocNow(target_soc);
		break;

	case 3:
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
	// return g_stCellInfoReport.u16Ichg > SOC_VIRTUAL_CURRENT_CHG ? 1 : 0;
	return SOC_Enhance_Element.u16_Ichg >= SOC_VIRTUAL_CURRENT_CHG ? 1U : 0U;
}

static UINT8 isDSG(void)
{
	// return g_stCellInfoReport.u16IDischg > SOC_VIRTUAL_CURRENT_DSG ? 1 : 0;
	return SOC_Enhance_Element.u16_Idsg >= SOC_VIRTUAL_CURRENT_DSG ? 1U : 0U;
}

static void SOC_ApplyVoltageCalibration(void)
{
	UINT16 full_confirm_mv;

	if (!isCHG())
	{
		return;
	}

	full_confirm_mv = SOC_GetFullCellConfirmVoltage();
	if ((SOC_Enhance_Element.u16_VCellMax >= SOC_Enhance_Element.u16_SOC_100_Vol) &&
		(SOC_Enhance_Element.u16_VCellMin >= full_confirm_mv) &&
		(SOC_Calculate_Element.u8SOC_Now < 100U))
	{
		SOC_ApplySocNow(100U);
	}
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
