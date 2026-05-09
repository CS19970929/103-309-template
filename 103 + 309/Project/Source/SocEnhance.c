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

#define SOC_TICK_MS                  ((UINT32)200U)
#define SOC_TICKS_PER_SECOND         ((UINT16)5U)
#define SOC_CURRENT_ACTIVE_A10       ((UINT16)4U)
#define SOC_DEFAULT_CAP_A10          ((UINT16)270U)
#define SOC_SOH_MIN                  ((UINT8)80U)
#define SOC_SOH_CYCLE_STEP           ((UINT16)100U)
#define SOC_FULL_SECONDS             ((UINT16)PROJECT_CFG_SOC_FULL_CONFIRM_SECONDS)
#define SOC_FULL_FAST_SECONDS        ((UINT16)PROJECT_CFG_SOC_FULL_CONFIRM_FAST_SECONDS)
#define SOC_FULL_MIN_SOC             ((UINT8)PROJECT_CFG_SOC_FULL_CONFIRM_MIN_SOC_PERCENT)
#define SOC_DEFAULT_FULL_MV          ((UINT16)4180U)
#define SOC_FULL_FAST_MARGIN_MV      ((UINT16)PROJECT_CFG_SOC_FULL_CONFIRM_FAST_MARGIN_MV)
#define SOC_FULL_MIN_MARGIN_MV       ((UINT16)PROJECT_CFG_SOC_FULL_CONFIRM_MIN_CELL_MARGIN_MV)
#define SOC_FULL_MAX_DELTA_MV        ((UINT16)PROJECT_CFG_SOC_FULL_CONFIRM_MAX_CELL_DELTA_MV)
#define SOC_EMPTY_MV                 ((UINT16)3000U)
#define SOC_EMPTY_FAST_MV            ((UINT16)PROJECT_CFG_SOC_EMPTY_FAST_MV)
#define SOC_EMPTY_FORCE_MV           ((UINT16)PROJECT_CFG_SOC_EMPTY_FORCE_MV)
#define SOC_EMPTY_CUR_LIGHT_DIVIDER  ((UINT16)5U)
#define SOC_EMPTY_CUR_MID_DIVIDER    ((UINT16)2U)
#define SOC_SAG_HOLDOFF_SECONDS      ((UINT16)PROJECT_CFG_SOC_SAG_HOLDOFF_SECONDS)
#define SOC_SAG_ALLOW_OFFSET_MV      ((int16_t)PROJECT_CFG_SOC_SAG_ALLOW_OFFSET_MV)
#define SOC_REST_OCV_SECONDS         ((UINT32)PROJECT_CFG_SOC_REST_OCV_SECONDS)
#define SOC_LOW_GUARD_MV             ((UINT16)PROJECT_CFG_SOC_LOW_GUARD_MV)
#define SOC_LOW_GUARD_CRITICAL_MV    ((UINT16)PROJECT_CFG_SOC_LOW_GUARD_CRITICAL_MV)
#define SOC_LOW_GUARD_MARGIN         ((UINT8)PROJECT_CFG_SOC_LOW_GUARD_MARGIN_PERCENT)
#define SOC_LOW_GUARD_CRIT_MARGIN    ((UINT8)PROJECT_CFG_SOC_LOW_GUARD_CRIT_MARGIN_PERCENT)
#define SOC_LOW_GUARD_SECONDS        ((UINT16)PROJECT_CFG_SOC_LOW_GUARD_SECONDS)
#define SOC_LOW_GUARD_CUR_DIVIDER    ((UINT16)PROJECT_CFG_SOC_LOW_GUARD_CURRENT_DIVIDER)
#define SOC_CAL_STEP                 ((UINT8)PROJECT_CFG_SOC_CALIBRATION_STEP_PERCENT)
#define SOC_DISPLAY_NORMAL_SECONDS   ((UINT8)5U)
#define SOC_DISPLAY_CHG_SECONDS      SOC_DISPLAY_NORMAL_SECONDS
#define SOC_DISPLAY_LOW_SECONDS      ((UINT8)1U)
#define SOC_VALID_MIN_MV             ((UINT16)PROJECT_CFG_SOC_CALIBRATION_MIN_CELL_VALID_MV)
#define SOC_VALID_MAX_MV             ((UINT16)PROJECT_CFG_SOC_CALIBRATION_MAX_CELL_VALID_MV)
#define SOC_VALID_MAX_DELTA_MV       ((UINT16)PROJECT_CFG_SOC_CALIBRATION_MAX_CELL_DELTA_MV)
#define SOC_MODE_RELAX               ((UINT8)0U)
#define SOC_MODE_CHG                 ((UINT8)1U)
#define SOC_MODE_DSG                 ((UINT8)2U)
#define SOC_EMPTY_BAND_RELAX         ((UINT8)0U)
#define SOC_EMPTY_BAND_LIGHT         ((UINT8)1U)
#define SOC_EMPTY_BAND_MID           ((UINT8)2U)
#define SOC_EMPTY_BAND_HEAVY         ((UINT8)3U)
#define SOC_EMPTY_BAND_COUNT         ((UINT8)4U)

typedef struct
{
	UINT32 cap_factory_as10;
	UINT32 cap_full_as10;
	UINT32 cap_now_as10;
	UINT32 cycle_x100;
	UINT32 dsg_acc_as10;
	UINT32 rem_ms;
	UINT16 full_ticks;
	UINT16 empty_ticks;
	UINT16 rest_ticks;
	UINT16 low_guard_ticks;
	UINT16 display_ticks;
	UINT16 sag_hold_ticks;
	UINT8 soc;
	UINT8 soh;
	UINT8 display_soc;
	UINT8 mode;
	UINT8 last_mode;
	UINT8 display_ready;
	UINT8 full_anchor;
	UINT8 force_display;
} SOC_STATE;

typedef struct
{
	int16_t offset_mv;
	UINT8 target[SOC_EMPTY_BAND_COUNT];
	UINT8 ticks[SOC_EMPTY_BAND_COUNT];
} SOC_EMPTY_TAIL_RULE;

struct SOC_ENHANCE_ELEMENT SOC_Enhance_Element;

static SOC_STATE s_soc;
static SOC_STATE s_saved_soc;

static const SOC_EMPTY_TAIL_RULE s_empty_tail_table[] = {
	{-50, {0U, 0U, 0U, 0U}, {1U, 1U, 1U, 1U}},
	{-25, {0U, 0U, 0U, 0U}, {5U, 5U, 1U, 1U}},
	{0, {0U, 0U, 0U, 0U}, {10U, 5U, 5U, 5U}},
	{50, {4U, 5U, 8U, 12U}, {20U, 15U, 10U, 8U}},
	{100, {8U, 10U, 14U, 18U}, {35U, 30U, 25U, 20U}},
	{200, {12U, 14U, 20U, 25U}, {60U, 50U, 40U, 30U}},
	{300, {14U, 18U, 25U, 32U}, {90U, 75U, 60U, 45U}},
	{400, {18U, 22U, 30U, 40U}, {120U, 100U, 80U, 60U}},
};

const UINT16 SOC_Table_LiFePO[SOC_Size_LiFePO] = {
	3336, 100, 3332, 90, 3330, 80, 3327, 75, 3316, 70, 3301, 65,
	3294, 60, 3291, 55, 3290, 50, 3288, 45, 3286, 40, 3279, 35,
	3266, 30, 3254, 25, 3236, 20, 3212, 15, 3198, 10, 3112, 5,
	2526, 0, 1000, 0, 1000, 0,
};

const UINT16 SocTable_TernaryLi[SOC_Size_TernaryLi] = {
	4160, 100, 4100, 95, 4050, 90, 3995, 85, 3935, 80, 3880, 75,
	3835, 70, 3795, 65, 3760, 60, 3725, 55, 3695, 50, 3670, 45,
	3645, 40, 3615, 35, 3585, 30, 3555, 25, 3525, 20, 3480, 15,
	3400, 10, 3250, 5, 3000, 0,
};

const UINT16 SocTable_LiFePO2[SOC_Size_LiFePO2] = {
	3650, 100, 3600, 98, 3550, 95, 3500, 92, 3400, 90, 3350, 87,
	3340, 85, 3335, 82, 3330, 80, 3325, 78, 3320, 75, 3300, 70,
	3275, 65, 3250, 60, 3200, 50, 3150, 45, 3100, 30, 3000, 20,
	2850, 10, 2750, 5, 2650, 0,
};

static UINT16 soc_cell_delta(void)
{
	return (SOC_Enhance_Element.u16_VCellMax >= SOC_Enhance_Element.u16_VCellMin) ?
		(UINT16)(SOC_Enhance_Element.u16_VCellMax - SOC_Enhance_Element.u16_VCellMin) :
		(UINT16)(SOC_Enhance_Element.u16_VCellMin - SOC_Enhance_Element.u16_VCellMax);
}

static UINT8 soc_step(UINT8 now, UINT8 target, UINT8 step)
{
	if (now < target)
	{
		return (UINT8)(((UINT16)now + step > target) ? target : (now + step));
	}
	if (now > target)
	{
		return (UINT8)((now > (UINT8)(target + step)) ? (now - step) : target);
	}
	return now;
}

static UINT32 soc_factory_cap_as10_from(UINT16 cap_a10)
{
	if (cap_a10 == 0U)
	{
		cap_a10 = SOC_DEFAULT_CAP_A10;
	}
	return (UINT32)cap_a10 * 3600U;
}

static UINT8 soc_soh_from_cycle(UINT32 cycle_x100)
{
	UINT32 drop = (cycle_x100 / 100U) / SOC_SOH_CYCLE_STEP;
	return (drop >= (UINT32)(100U - SOC_SOH_MIN)) ? SOC_SOH_MIN : (UINT8)(100U - drop);
}

static void soc_refresh_capacity_base(void)
{
	s_soc.soh = soc_soh_from_cycle(s_soc.cycle_x100);
	s_soc.cap_full_as10 = (UINT32)(((uint64_t)s_soc.cap_factory_as10 * s_soc.soh) / 100ULL);
	if (s_soc.cap_full_as10 == 0U)
	{
		s_soc.cap_full_as10 = s_soc.cap_factory_as10;
	}
	if (s_soc.cap_now_as10 > s_soc.cap_full_as10)
	{
		s_soc.cap_now_as10 = s_soc.cap_full_as10;
	}
}

static UINT8 soc_from_cap(void)
{
	UINT32 soc;

	if (s_soc.cap_full_as10 == 0U)
	{
		return 0U;
	}
	if (s_soc.cap_now_as10 >= s_soc.cap_full_as10)
	{
		return 100U;
	}
	soc = (UINT32)(((uint64_t)s_soc.cap_now_as10 * 100ULL +
		(s_soc.cap_full_as10 / 2U)) / s_soc.cap_full_as10);
	return (soc > 100U) ? 100U : (UINT8)soc;
}

static UINT16 soc_cap_to_ah100(UINT32 cap_as10)
{
	UINT32 cap = (cap_as10 + 180U) / 360U;
	return (cap > 0xFFFFU) ? 0xFFFFU : (UINT16)cap;
}

static void soc_set(UINT8 soc)
{
	if (soc > 100U)
	{
		soc = 100U;
	}
	s_soc.soc = soc;
	s_soc.cap_now_as10 = (UINT32)(((uint64_t)s_soc.cap_full_as10 * soc) / 100ULL);
	s_soc.rem_ms = 0U;
	s_soc.full_anchor = (soc >= 100U) ? 1U : 0U;
}

static UINT8 soc_direction(void)
{
	if ((SOC_Enhance_Element.u16_Ichg >= SOC_CURRENT_ACTIVE_A10) &&
		(SOC_Enhance_Element.u16_Ichg >= SOC_Enhance_Element.u16_Idsg))
	{
		return SOC_MODE_CHG;
	}
	if (SOC_Enhance_Element.u16_Idsg >= SOC_CURRENT_ACTIVE_A10)
	{
		return SOC_MODE_DSG;
	}
	return SOC_MODE_RELAX;
}

static UINT8 soc_voltage_valid(void)
{
	if ((SOC_Enhance_Element.u16_VCellMin < SOC_VALID_MIN_MV) ||
		(SOC_Enhance_Element.u16_VCellMax < SOC_VALID_MIN_MV) ||
		(SOC_Enhance_Element.u16_VCellMin > SOC_VALID_MAX_MV) ||
		(SOC_Enhance_Element.u16_VCellMax > SOC_VALID_MAX_MV) ||
		(SOC_Enhance_Element.u16_VCellMax < SOC_Enhance_Element.u16_VCellMin))
	{
		return 0U;
	}
	return 1U;
}

static UINT16 soc_voltage_with_margin(UINT16 base_mv, UINT16 margin_mv)
{
	return (base_mv > margin_mv) ? (UINT16)(base_mv - margin_mv) : 0U;
}

static UINT8 soc_protection_fault_blocks_calibration(void)
{
#if PROJECT_CFG_SOC_CALIBRATION_BLOCK_PROTECTION_FAULT
	return (UINT8)(g_stCellInfoReport.unMdlFault_Third.all != 0U);
#else
	return 0U;
#endif
}

static UINT8 soc_system_fault_blocks_calibration(void)
{
#if PROJECT_CFG_SOC_CALIBRATION_BLOCK_SYSTEM_FAULT
	return (UINT8)((System_ERROR_UserCallback(ERROR_STATUS_AFE1) != 0U) ||
		(System_ERROR_UserCallback(ERROR_STATUS_AFE2) != 0U) ||
		(System_ERROR_UserCallback(ERROR_STATUS_ADC) != 0U) ||
		(System_ERROR_UserCallback(ERROR_STATUS_CBC_CHG) != 0U) ||
		(System_ERROR_UserCallback(ERROR_STATUS_CBC_DSG) != 0U) ||
		(System_ERROR_UserCallback(ERROR_STATUS_TEMP_BREAK) != 0U));
#else
	return 0U;
#endif
}

static UINT8 soc_calibration_allowed(void)
{
	if (!soc_voltage_valid() || (soc_cell_delta() > SOC_VALID_MAX_DELTA_MV))
	{
		return 0U;
	}
	return (UINT8)((!soc_protection_fault_blocks_calibration()) &&
		(!soc_system_fault_blocks_calibration()));
}

static const UINT16 *soc_ocv_table(UINT16 *size)
{
	switch (SOC_Enhance_Element.u16_SOC_TableSelect)
	{
	case SOC_TABLE_TEST:
		*size = SOC_Size_TableCanSet;
		return SOC_Enhance_Element.SOC_Table_CanSet;
	case SOC_TABLE_LIFEPO:
		*size = SOC_Size_LiFePO;
		return SOC_Table_LiFePO;
	case SOC_TABLE_LIFEPO2:
		*size = SOC_Size_LiFePO2;
		return SocTable_LiFePO2;
	case SOC_TABLE_TERNARYLI:
	default:
		*size = SOC_Size_TernaryLi;
		return SocTable_TernaryLi;
	}
}

static UINT8 soc_ocv_percent(void)
{
	UINT16 size;
	UINT16 soc = GetEndValue(soc_ocv_table(&size), size, SOC_Enhance_Element.u16_VCellMin);
	return (soc > 100U) ? 100U : (UINT8)soc;
}

static UINT16 soc_empty_mv(void)
{
	return (SOC_Enhance_Element.u16_SOC_0_Vol != 0U) ?
		SOC_Enhance_Element.u16_SOC_0_Vol : SOC_EMPTY_MV;
}

static UINT16 soc_empty_threshold_mv(int16_t offset_mv)
{
	UINT16 empty_mv = soc_empty_mv();
	UINT16 offset;

	if (offset_mv >= 0)
	{
		offset = (UINT16)offset_mv;
		return (empty_mv > (UINT16)(SOC_VALID_MAX_MV - offset)) ?
			SOC_VALID_MAX_MV : (UINT16)(empty_mv + offset);
	}
	offset = (UINT16)(-offset_mv);
	return (empty_mv > offset) ? (UINT16)(empty_mv - offset) : 0U;
}

static UINT16 soc_full_mv(void)
{
	return (SOC_Enhance_Element.u16_SOC_100_Vol != 0U) ?
		SOC_Enhance_Element.u16_SOC_100_Vol : SOC_DEFAULT_FULL_MV;
}

static UINT16 soc_current_limit_a10(UINT16 divider)
{
	UINT16 cap_a10 = (SOC_Enhance_Element.u16_SOC_Ah != 0U) ?
		SOC_Enhance_Element.u16_SOC_Ah : SOC_DEFAULT_CAP_A10;
	UINT16 limit;

	if (divider == 0U)
	{
		divider = 1U;
	}
	limit = (UINT16)((cap_a10 + divider - 1U) / divider);
	return (limit < SOC_CURRENT_ACTIVE_A10) ? SOC_CURRENT_ACTIVE_A10 : limit;
}

static UINT16 soc_low_guard_current_a10(void)
{
	return soc_current_limit_a10(SOC_LOW_GUARD_CUR_DIVIDER);
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

static UINT8 soc_save(void)
{
	STORAGE_FLASH_SOC_DATA data;
	UINT32 unit = s_soc.cap_factory_as10 / 100U;

	memset(&data, 0, sizeof(data));
	data.u16FormatVersion = FLASH_STORAGE_SOC_DATA_VERSION_V2;
	data.u16SocNow = s_soc.soc;
	data.u16MaxErrorPercent = 100U;
	data.u32CycleTimes = s_soc.cycle_x100;
	data.u32CapNow = s_soc.cap_now_as10;
	data.u32CapFull = s_soc.cap_full_as10;
	data.u32LearnPassedAs10 = s_soc.dsg_acc_as10;
	if (unit != 0U)
	{
		data.u16DsgSocInt = (UINT16)(((uint64_t)s_soc.dsg_acc_as10 * 100ULL) / unit);
		if (data.u16DsgSocInt > 100U)
		{
			data.u16DsgSocInt = 100U;
		}
	}
	return StorageFlash_SaveSocData(&data);
}

static void soc_save_if_needed(void)
{
	if (!SOC_Enhance_Element.u16_SOC_InitOver)
	{
		return;
	}
	if ((s_soc.soc != s_saved_soc.soc) ||
		(s_soc.cycle_x100 != s_saved_soc.cycle_x100) ||
		(s_soc.cap_full_as10 != s_saved_soc.cap_full_as10))
	{
		if (soc_save())
		{
			s_saved_soc = s_soc;
		}
	}
}

static void soc_load_or_default(void)
{
	STORAGE_FLASH_SOC_DATA data;
	UINT8 valid = StorageFlash_LoadSocData(&data);
	UINT32 unit = s_soc.cap_factory_as10 / 100U;

	if (valid && (data.u16SocNow <= 100U) && (data.u16DsgSocInt <= 100U))
	{
		s_soc.cycle_x100 = data.u32CycleTimes;
		soc_refresh_capacity_base();
		if (unit != 0U)
		{
			s_soc.dsg_acc_as10 = (data.u32LearnPassedAs10 != 0U) ?
				(data.u32LearnPassedAs10 % unit) :
				(UINT32)(((uint64_t)unit * data.u16DsgSocInt) / 100ULL);
		}
		if (((data.u32CapNow != 0U) || (data.u16SocNow == 0U)) &&
			(data.u32CapNow <= s_soc.cap_full_as10))
		{
			s_soc.cap_now_as10 = data.u32CapNow;
			s_soc.soc = soc_from_cap();
		}
		else
		{
			soc_set((UINT8)data.u16SocNow);
		}
	}
	else
	{
		s_soc.cycle_x100 = (UINT32)SOC_Enhance_Element.u16_SOC_CycleT_Ever * 100U;
		soc_refresh_capacity_base();
		if (soc_calibration_allowed())
		{
			soc_set(soc_ocv_percent());
		}
		else
		{
			soc_set(SOC_DEFAULT_STARTUP_PERCENT);
		}
		(void)soc_save();
	}
	s_soc.display_soc = s_soc.soc;
	s_soc.display_ready = 1U;
	s_saved_soc = s_soc;
}

static void soc_add_discharge(UINT32 delta_as10)
{
	UINT32 unit = s_soc.cap_factory_as10 / 100U;
	UINT8 old_soh = s_soc.soh;

	if ((delta_as10 == 0U) || (unit == 0U))
	{
		return;
	}
	s_soc.dsg_acc_as10 += delta_as10;
	while (s_soc.dsg_acc_as10 >= unit)
	{
		s_soc.dsg_acc_as10 -= unit;
		++s_soc.cycle_x100;
	}
	soc_refresh_capacity_base();
	if (s_soc.soh != old_soh)
	{
		s_soc.soc = soc_from_cap();
	}
}

static void soc_integrate(UINT8 mode)
{
	UINT16 current = (mode == SOC_MODE_CHG) ? SOC_Enhance_Element.u16_Ichg :
		SOC_Enhance_Element.u16_Idsg;
	UINT32 acc_ms;
	UINT32 delta_as10;

	if (mode != s_soc.last_mode)
	{
		s_soc.rem_ms = 0U;
		s_soc.last_mode = mode;
	}
	if (mode == SOC_MODE_RELAX)
	{
		s_soc.rem_ms = 0U;
		return;
	}
	acc_ms = ((UINT32)current * SOC_TICK_MS) + s_soc.rem_ms;
	delta_as10 = acc_ms / 1000U;
	s_soc.rem_ms = acc_ms % 1000U;
	if (delta_as10 == 0U)
	{
		return;
	}
	if (mode == SOC_MODE_CHG)
	{
		s_soc.cap_now_as10 = ((s_soc.cap_full_as10 - s_soc.cap_now_as10) < delta_as10) ?
			s_soc.cap_full_as10 : (s_soc.cap_now_as10 + delta_as10);
	}
	else
	{
		s_soc.full_anchor = 0U;
		soc_add_discharge(delta_as10);
		s_soc.cap_now_as10 = (s_soc.cap_now_as10 > delta_as10) ?
			(s_soc.cap_now_as10 - delta_as10) : 0U;
	}
	s_soc.soc = soc_from_cap();
	if ((mode == SOC_MODE_CHG) && (!s_soc.full_anchor) && (s_soc.soc >= 100U))
	{
		s_soc.soc = 99U;
		s_soc.cap_now_as10 = (UINT32)(((uint64_t)s_soc.cap_full_as10 * 99ULL) / 100ULL);
	}
}

static UINT8 soc_apply_rest_ocv(UINT32 rest_seconds, UINT8 mode)
{
	UINT8 target;
	UINT8 old_soc = s_soc.soc;

	if ((rest_seconds < SOC_REST_OCV_SECONDS) || !soc_calibration_allowed())
	{
		return 0U;
	}
	target = soc_ocv_percent();
	if (((mode == SOC_MODE_CHG) && (target <= s_soc.soc)) ||
		((mode == SOC_MODE_DSG) && (target >= s_soc.soc)))
	{
		return 0U;
	}
	soc_set(soc_step(s_soc.soc, target, SOC_CAL_STEP));
	return (UINT8)(s_soc.soc != old_soc);
}

static UINT8 soc_full_confirm_seconds(void)
{
	UINT16 full_mv = soc_full_mv();
	UINT16 vmax_min = soc_voltage_with_margin(full_mv, SOC_FULL_MIN_MARGIN_MV);
	UINT16 vmin_min = vmax_min;
	UINT16 vmin_fast = soc_voltage_with_margin(full_mv, SOC_FULL_FAST_MARGIN_MV);
	UINT16 delta;

	if (!soc_calibration_allowed() ||
		(SOC_Enhance_Element.u16_VCellMax < vmax_min))
	{
		return 0U;
	}

	delta = soc_cell_delta();
	if ((SOC_Enhance_Element.u16_VCellMin >= vmin_fast) &&
		(delta <= SOC_FULL_MAX_DELTA_MV))
	{
		return (UINT8)SOC_FULL_FAST_SECONDS;
	}
	if ((s_soc.soc >= SOC_FULL_MIN_SOC) &&
		(SOC_Enhance_Element.u16_VCellMin >= vmin_min) &&
		(delta <= SOC_FULL_MAX_DELTA_MV))
	{
		return (UINT8)SOC_FULL_SECONDS;
	}
	return 0U;
}

static UINT8 soc_empty_current_band(UINT8 mode)
{
	if (mode == SOC_MODE_RELAX)
	{
		return SOC_EMPTY_BAND_RELAX;
	}
	if (SOC_Enhance_Element.u16_Idsg <=
		soc_current_limit_a10(SOC_EMPTY_CUR_LIGHT_DIVIDER))
	{
		return SOC_EMPTY_BAND_LIGHT;
	}
	if (SOC_Enhance_Element.u16_Idsg <=
		soc_current_limit_a10(SOC_EMPTY_CUR_MID_DIVIDER))
	{
		return SOC_EMPTY_BAND_MID;
	}
	return SOC_EMPTY_BAND_HEAVY;
}

static UINT8 soc_heavy_discharge_active(UINT8 mode)
{
	return (UINT8)((mode == SOC_MODE_DSG) &&
		(SOC_Enhance_Element.u16_Idsg >
		 soc_current_limit_a10(SOC_EMPTY_CUR_MID_DIVIDER)));
}

static void soc_update_sag_hold(UINT8 mode)
{
	if (soc_heavy_discharge_active(mode))
	{
		s_soc.sag_hold_ticks = (UINT16)(SOC_SAG_HOLDOFF_SECONDS *
			SOC_TICKS_PER_SECOND);
	}
	else if (s_soc.sag_hold_ticks > 0U)
	{
		--s_soc.sag_hold_ticks;
	}
}

static UINT8 soc_sag_hold_blocks_calibration(void)
{
	return (UINT8)((s_soc.sag_hold_ticks > 0U) &&
		soc_voltage_valid() &&
		(SOC_Enhance_Element.u16_VCellMin >
		 soc_empty_threshold_mv(SOC_SAG_ALLOW_OFFSET_MV)));
}

static UINT8 soc_empty_tail_config(UINT8 mode, UINT8 *target, UINT8 *ticks)
{
	UINT8 band = soc_empty_current_band(mode);
	UINT16 vmin = SOC_Enhance_Element.u16_VCellMin;
	UINT16 threshold;
	UINT16 i;

	for (i = 0U; i < (UINT16)(sizeof(s_empty_tail_table) / sizeof(s_empty_tail_table[0])); ++i)
	{
		threshold = soc_empty_threshold_mv(s_empty_tail_table[i].offset_mv);
		if (vmin <= threshold)
		{
			*target = s_empty_tail_table[i].target[band];
			*ticks = s_empty_tail_table[i].ticks[band];
			if (*ticks == 0U)
			{
				*ticks = 1U;
			}
			return 1U;
		}
	}
	return 0U;
}

static UINT8 soc_empty_tail_active(UINT8 mode)
{
	UINT8 target;
	UINT8 ticks;

	if ((mode == SOC_MODE_CHG) || !soc_voltage_valid())
	{
		return 0U;
	}
	if (soc_sag_hold_blocks_calibration())
	{
		return 0U;
	}
	if ((SOC_Enhance_Element.u16_VCellMin <= SOC_EMPTY_FORCE_MV) ||
		(SOC_Enhance_Element.u16_VCellMin <= SOC_EMPTY_FAST_MV))
	{
		return 1U;
	}
	return soc_empty_tail_config(mode, &target, &ticks);
}

static UINT8 soc_apply_full_empty(UINT8 mode)
{
	UINT8 full_seconds;
	UINT8 empty_target;
	UINT8 empty_ticks;
	UINT8 old_soc = s_soc.soc;

	if (mode != SOC_MODE_DSG)
	{
		full_seconds = soc_full_confirm_seconds();
		if (full_seconds != 0U)
		{
			s_soc.empty_ticks = 0U;
			if (s_soc.full_ticks < (UINT16)(full_seconds * SOC_TICKS_PER_SECOND))
			{
				++s_soc.full_ticks;
			}
			if (s_soc.full_ticks >= (UINT16)(full_seconds * SOC_TICKS_PER_SECOND))
			{
				soc_set(soc_step(s_soc.soc, 100U, SOC_CAL_STEP));
				s_soc.full_ticks = 0U;
				s_soc.full_anchor = (s_soc.soc >= 100U) ? 1U : 0U;
			}
			return (UINT8)(s_soc.soc != old_soc);
		}
		if (s_soc.full_ticks > 0U)
		{
			--s_soc.full_ticks;
		}
	}
	else
	{
		s_soc.full_ticks = 0U;
	}

	if (mode == SOC_MODE_CHG)
	{
		s_soc.empty_ticks = 0U;
		return 0U;
	}
	if (!soc_voltage_valid())
	{
		s_soc.empty_ticks = 0U;
		return 0U;
	}
	if (soc_sag_hold_blocks_calibration())
	{
		s_soc.empty_ticks = 0U;
		return 0U;
	}
	if (SOC_Enhance_Element.u16_VCellMin <= SOC_EMPTY_FORCE_MV)
	{
		empty_target = 0U;
		empty_ticks = 1U;
	}
	else if ((SOC_Enhance_Element.u16_VCellMin <= SOC_EMPTY_FAST_MV) ||
		soc_empty_tail_config(mode, &empty_target, &empty_ticks))
	{
		if (SOC_Enhance_Element.u16_VCellMin <= SOC_EMPTY_FAST_MV)
		{
			empty_target = 0U;
			empty_ticks = 1U;
		}
		if (++s_soc.empty_ticks >= (UINT16)empty_ticks)
		{
			if (s_soc.soc > empty_target)
			{
				soc_set(soc_step(s_soc.soc, empty_target, SOC_CAL_STEP));
			}
			s_soc.empty_ticks = 0U;
		}
	}
	else
	{
		s_soc.empty_ticks = 0U;
	}
	return (UINT8)(s_soc.soc != old_soc);
}

static UINT8 soc_apply_low_voltage_guard(UINT8 mode)
{
	UINT8 target;
	UINT8 limit;
	UINT8 margin;
	UINT8 old_soc = s_soc.soc;

	if ((mode == SOC_MODE_CHG) || !soc_voltage_valid() ||
		(SOC_Enhance_Element.u16_VCellMin > SOC_LOW_GUARD_MV))
	{
		s_soc.low_guard_ticks = 0U;
		return 0U;
	}
	if ((mode == SOC_MODE_DSG) &&
		(SOC_Enhance_Element.u16_Idsg > soc_low_guard_current_a10()))
	{
		s_soc.low_guard_ticks = 0U;
		return 0U;
	}
	target = soc_ocv_percent();
	margin = (SOC_Enhance_Element.u16_VCellMin <= SOC_LOW_GUARD_CRITICAL_MV) ?
		SOC_LOW_GUARD_CRIT_MARGIN : SOC_LOW_GUARD_MARGIN;
	limit = (target > (UINT8)(100U - margin)) ? 100U : (UINT8)(target + margin);
	if (s_soc.soc > limit)
	{
		if (++s_soc.low_guard_ticks >= (UINT16)(SOC_LOW_GUARD_SECONDS * SOC_TICKS_PER_SECOND))
		{
			soc_set(soc_step(s_soc.soc, limit, SOC_CAL_STEP));
			s_soc.low_guard_ticks = 0U;
		}
	}
	else
	{
		s_soc.low_guard_ticks = 0U;
	}
	return (UINT8)(s_soc.soc != old_soc);
}

static void soc_update_rest_timer(UINT8 mode)
{
	if (mode != SOC_MODE_RELAX)
	{
		s_soc.rest_ticks = 0U;
		return;
	}
	if (s_soc.rest_ticks < (UINT16)(SOC_REST_OCV_SECONDS * SOC_TICKS_PER_SECOND))
	{
		++s_soc.rest_ticks;
	}
	if (s_soc.rest_ticks >= (UINT16)(SOC_REST_OCV_SECONDS * SOC_TICKS_PER_SECOND))
	{
		(void)soc_apply_rest_ocv(SOC_REST_OCV_SECONDS, SOC_MODE_RELAX);
		s_soc.rest_ticks = 0U;
	}
}

static UINT8 soc_display_target(void)
{
	if (System_OnOFF_Func.bits.b1OnOFF_SOC_Zero)
	{
		return 0U;
	}
	if (System_OnOFF_Func.bits.b1OnOFF_SOC_Fixed)
	{
		return 60U;
	}
	return s_soc.soc;
}

static void soc_publish(UINT8 force_display)
{
	UINT8 target = soc_display_target();
	UINT8 seconds = SOC_DISPLAY_NORMAL_SECONDS;
	UINT8 ticks;
	UINT32 cycles = s_soc.cycle_x100 / 100U;

	if (force_display || !s_soc.display_ready ||
		System_OnOFF_Func.bits.b1OnOFF_SOC_Zero ||
		System_OnOFF_Func.bits.b1OnOFF_SOC_Fixed)
	{
		s_soc.display_soc = target;
		s_soc.display_ready = 1U;
		s_soc.display_ticks = 0U;
	}
	else if (s_soc.display_soc != target)
	{
		if ((target < s_soc.display_soc) &&
			(SOC_Enhance_Element.u16_VCellMin <= soc_empty_threshold_mv(50)))
		{
			ticks = (SOC_Enhance_Element.u16_VCellMin <= soc_empty_threshold_mv(-50)) ?
				1U : (UINT8)(SOC_DISPLAY_LOW_SECONDS * SOC_TICKS_PER_SECOND);
		}
		else
		{
			if ((target > s_soc.display_soc) && (s_soc.mode == SOC_MODE_CHG))
			{
				seconds = SOC_DISPLAY_CHG_SECONDS;
			}
			ticks = (UINT8)(seconds * SOC_TICKS_PER_SECOND);
		}
		if (++s_soc.display_ticks >= ticks)
		{
			if (s_soc.display_soc < target)
			{
				++s_soc.display_soc;
			}
			else
			{
				--s_soc.display_soc;
			}
			s_soc.display_ticks = 0U;
		}
	}
	else
	{
		s_soc.display_ticks = 0U;
	}

	SOC_Enhance_Element.u8_SOC = s_soc.display_soc;
	SOC_Enhance_Element.u8_SOH = s_soc.soh;
	SOC_Enhance_Element.u16_CapacityNow = soc_cap_to_ah100(s_soc.cap_now_as10);
	SOC_Enhance_Element.u16_CapacityFull = soc_cap_to_ah100(s_soc.cap_full_as10);
	SOC_Enhance_Element.u16_CapacityFactory = soc_cap_to_ah100(s_soc.cap_factory_as10);
	SOC_Enhance_Element.u16_Cycle_times = (cycles > 0xFFFFU) ? 0xFFFFU : (UINT16)cycles;
	SOC_Enhance_Element.u8_SOC_OCV_Cali = (UINT8)(s_soc.cycle_x100 % 100U);
	SOC_PublishReportData();
}

static void soc_handle_command(void)
{
	UINT8 save = 0U;
	UINT8 soc_keep = s_soc.soc;

	switch (SOC_Enhance_Element.u16_RefreshData_Flag)
	{
	case 1:
		if (!System_OnOFF_Func.bits.b1OnOFF_SOC_Fixed)
		{
			save = soc_apply_rest_ocv(SOC_REST_OCV_SECONDS, soc_direction());
		}
		break;
	case 2:
		if (!System_OnOFF_Func.bits.b1OnOFF_SOC_Zero)
		{
			s_soc.cap_factory_as10 = soc_factory_cap_as10_from(SOC_Enhance_Element.u16_SOC_Ah);
			s_soc.cycle_x100 = (UINT32)SOC_Enhance_Element.u16_SOC_CycleT_Ever * 100U;
			s_soc.dsg_acc_as10 = 0U;
			soc_refresh_capacity_base();
			soc_set(soc_keep);
			save = 1U;
		}
		break;
	case 3:
		soc_set(SOC_Enhance_Element.u8_SetSocOnce);
		save = 1U;
		break;
	default:
		break;
	}
	SOC_Enhance_Element.u16_RefreshData_Flag = 0U;
	if (save)
	{
		if (soc_save())
		{
			s_saved_soc = s_soc;
		}
	}
	soc_publish(1U);
}

void soc_param_lib_init(void)
{
	memset(&s_soc, 0, sizeof(s_soc));
	s_soc.cap_factory_as10 = soc_factory_cap_as10_from(SOC_Enhance_Element.u16_SOC_Ah);
	s_soc.cycle_x100 = (UINT32)SOC_Enhance_Element.u16_SOC_CycleT_Ever * 100U;
	soc_refresh_capacity_base();
	SOC_UpdateSampleData(g_stCellInfoReport.u16VCellMax,
						 g_stCellInfoReport.u16VCellMin,
						 g_stCellInfoReport.u16Ichg,
						 g_stCellInfoReport.u16IDischg);
	soc_load_or_default();
	SOC_Enhance_Element.u16_SOC_InitOver = 1U;
	soc_publish(1U);
}

UINT8 SOC_ResetStoredSnapshotToDefault(void)
{
	STORAGE_FLASH_SOC_DATA data;
	UINT32 cap_factory = soc_factory_cap_as10_from(OtherElement.u16Soc_Ah);
	UINT32 cycle_x100 = (UINT32)OtherElement.u16Soc_Cycle_times * 100U;
	UINT32 cap_full = (UINT32)(((uint64_t)cap_factory * soc_soh_from_cycle(cycle_x100)) / 100ULL);

	memset(&data, 0, sizeof(data));
	data.u16FormatVersion = FLASH_STORAGE_SOC_DATA_VERSION_V2;
	data.u16SocNow = SOC_DEFAULT_STARTUP_PERCENT;
	data.u16MaxErrorPercent = 100U;
	data.u32CycleTimes = cycle_x100;
	data.u32CapFull = cap_full;
	data.u32CapNow = (UINT32)(((uint64_t)cap_full * SOC_DEFAULT_STARTUP_PERCENT) / 100ULL);
	return StorageFlash_SaveSocData(&data);
}

void SOC_IntEnhance_Ctrl(void)
{
	UINT8 force;
	UINT8 calibrated;
	UINT8 empty_active;

	if (!SOC_Enhance_Element.u16_SOC_InitOver)
	{
		return;
	}
	if (SOC_Enhance_Element.u16_RefreshData_Flag != 0U)
	{
		soc_handle_command();
		return;
	}
	s_soc.mode = soc_direction();
	soc_integrate(s_soc.mode);
	soc_update_sag_hold(s_soc.mode);
	empty_active = soc_empty_tail_active(s_soc.mode);
	calibrated = soc_apply_full_empty(s_soc.mode);
	if (!empty_active && !calibrated && !soc_sag_hold_blocks_calibration())
	{
		calibrated = soc_apply_low_voltage_guard(s_soc.mode);
	}
	if (!empty_active && !calibrated && !soc_sag_hold_blocks_calibration())
	{
		soc_update_rest_timer(s_soc.mode);
	}
	else if (empty_active || soc_sag_hold_blocks_calibration())
	{
		s_soc.rest_ticks = 0U;
	}
	soc_save_if_needed();
	force = s_soc.force_display;
	s_soc.force_display = 0U;
	soc_publish(force);
}

void SOC_ApplyRtcRelaxationCompensation(UINT32 rest_seconds, UINT16 vcell_min, UINT16 vcell_max)
{
	UINT8 changed;

	if (!SOC_Enhance_Element.u16_SOC_InitOver)
	{
		return;
	}
	SOC_Enhance_Element.u16_VCellMin = vcell_min;
	SOC_Enhance_Element.u16_VCellMax = vcell_max;
	changed = soc_apply_rest_ocv(rest_seconds, SOC_MODE_RELAX);
	if (changed && soc_save())
	{
		s_saved_soc = s_soc;
	}
	soc_publish(0U);
}
