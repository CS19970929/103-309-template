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
#define SOC_SOH_MIN                  ((UINT8)70U)
#define SOC_SOH_CYCLE_STEP           ((UINT16)50U)
#define SOC_FULL_SECONDS             ((UINT16)60U)
#define SOC_FULL_MIN_VMAX_MV         ((UINT16)4180U)
#define SOC_FULL_MIN_VMIN_MV         ((UINT16)4120U)
#define SOC_FULL_MAX_DELTA_MV        ((UINT16)120U)
#define SOC_TAPER_CURRENT_DIVIDER    ((UINT16)20U)
#define SOC_EMPTY_MV                 ((UINT16)3000U)
#define SOC_EMPTY_FAST_MV            ((UINT16)2750U)
#define SOC_EMPTY_FORCE_MV           ((UINT16)2500U)
#define SOC_EMPTY_SECONDS            ((UINT16)4U)
#define SOC_EMPTY_STEP               ((UINT8)5U)
#define SOC_REST_OCV_SECONDS         ((UINT32)1800U)
#define SOC_LOW_GUARD_MV             ((UINT16)3400U)
#define SOC_LOW_GUARD_CRITICAL_MV    ((UINT16)3250U)
#define SOC_LOW_GUARD_MARGIN         ((UINT8)8U)
#define SOC_LOW_GUARD_CRIT_MARGIN    ((UINT8)3U)
#define SOC_LOW_GUARD_SECONDS        ((UINT16)10U)
#define SOC_LOW_GUARD_CUR_DIVIDER    ((UINT16)5U)
#define SOC_DISPLAY_NORMAL_SECONDS   ((UINT8)5U)
#define SOC_DISPLAY_CHG_SECONDS      ((UINT8)10U)
#define SOC_DISPLAY_LOW_SECONDS      ((UINT8)1U)
#define SOC_VALID_MIN_MV             ((UINT16)2000U)
#define SOC_VALID_MAX_MV             ((UINT16)5000U)
#define SOC_VALID_MAX_DELTA_MV       ((UINT16)1000U)
#define SOC_MODE_RELAX               ((UINT8)0U)
#define SOC_MODE_CHG                 ((UINT8)1U)
#define SOC_MODE_DSG                 ((UINT8)2U)

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
	UINT8 soc;
	UINT8 soh;
	UINT8 display_soc;
	UINT8 mode;
	UINT8 last_mode;
	UINT8 display_ready;
	UINT8 full_anchor;
	UINT8 force_display;
} SOC_STATE;

struct SOC_ENHANCE_ELEMENT SOC_Enhance_Element;

static SOC_STATE s_soc;
static SOC_STATE s_saved_soc;

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

static UINT8 soc_calibration_allowed(void)
{
	if (!soc_voltage_valid() || (soc_cell_delta() > SOC_VALID_MAX_DELTA_MV))
	{
		return 0U;
	}
	return (UINT8)((g_stCellInfoReport.unMdlFault_Third.all == 0U) &&
		(System_ERROR_UserCallback(ERROR_STATUS_AFE1) == 0U) &&
		(System_ERROR_UserCallback(ERROR_STATUS_AFE2) == 0U) &&
		(System_ERROR_UserCallback(ERROR_STATUS_ADC) == 0U) &&
		(System_ERROR_UserCallback(ERROR_STATUS_CBC_CHG) == 0U) &&
		(System_ERROR_UserCallback(ERROR_STATUS_CBC_DSG) == 0U) &&
		(System_ERROR_UserCallback(ERROR_STATUS_TEMP_BREAK) == 0U));
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

static UINT16 soc_full_mv(void)
{
	return (SOC_Enhance_Element.u16_SOC_100_Vol != 0U) ?
		SOC_Enhance_Element.u16_SOC_100_Vol : SOC_FULL_MIN_VMAX_MV;
}

static UINT16 soc_taper_current_a10(void)
{
	UINT16 cap_a10 = (SOC_Enhance_Element.u16_SOC_Ah != 0U) ?
		SOC_Enhance_Element.u16_SOC_Ah : SOC_DEFAULT_CAP_A10;
	UINT16 taper = (UINT16)((cap_a10 + SOC_TAPER_CURRENT_DIVIDER - 1U) /
		SOC_TAPER_CURRENT_DIVIDER);
	return (taper < SOC_CURRENT_ACTIVE_A10) ? SOC_CURRENT_ACTIVE_A10 : taper;
}

static UINT16 soc_low_guard_current_a10(void)
{
	UINT16 cap_a10 = (SOC_Enhance_Element.u16_SOC_Ah != 0U) ?
		SOC_Enhance_Element.u16_SOC_Ah : SOC_DEFAULT_CAP_A10;
	UINT16 limit = (UINT16)((cap_a10 + SOC_LOW_GUARD_CUR_DIVIDER - 1U) /
		SOC_LOW_GUARD_CUR_DIVIDER);
	return (limit < SOC_CURRENT_ACTIVE_A10) ? SOC_CURRENT_ACTIVE_A10 : limit;
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
	UINT8 step = 1U;
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
	if (rest_seconds >= 21600U)
	{
		step = 3U;
	}
	else if (rest_seconds >= 3600U)
	{
		step = 2U;
	}
	soc_set(soc_step(s_soc.soc, target, step));
	return (UINT8)(s_soc.soc != old_soc);
}

static void soc_apply_full_empty(UINT8 mode)
{
	if (mode == SOC_MODE_CHG)
	{
		s_soc.empty_ticks = 0U;
		if (soc_calibration_allowed() &&
			(SOC_Enhance_Element.u16_Ichg <= soc_taper_current_a10()) &&
			(SOC_Enhance_Element.u16_VCellMax >= soc_full_mv()) &&
			(SOC_Enhance_Element.u16_VCellMax >= SOC_FULL_MIN_VMAX_MV) &&
			(SOC_Enhance_Element.u16_VCellMin >= SOC_FULL_MIN_VMIN_MV) &&
			(soc_cell_delta() <= SOC_FULL_MAX_DELTA_MV))
		{
			if (++s_soc.full_ticks >= (UINT16)(SOC_FULL_SECONDS * SOC_TICKS_PER_SECOND))
			{
				soc_set(100U);
				s_soc.full_ticks = 0U;
				s_soc.full_anchor = 1U;
				s_soc.force_display = 1U;
			}
			return;
		}
		s_soc.full_ticks = 0U;
		return;
	}

	s_soc.full_ticks = 0U;
	if ((mode != SOC_MODE_DSG) || !soc_voltage_valid())
	{
		s_soc.empty_ticks = 0U;
		return;
	}
	if (SOC_Enhance_Element.u16_VCellMin <= SOC_EMPTY_FORCE_MV)
	{
		soc_set(0U);
		s_soc.empty_ticks = 0U;
		s_soc.force_display = 1U;
	}
	else if (SOC_Enhance_Element.u16_VCellMin <= SOC_EMPTY_FAST_MV)
	{
		if (s_soc.soc > 1U)
		{
			soc_set(1U);
		}
		s_soc.empty_ticks = 0U;
		s_soc.force_display = 1U;
	}
	else if (SOC_Enhance_Element.u16_VCellMin <= soc_empty_mv())
	{
		if (++s_soc.empty_ticks >= (UINT16)(SOC_EMPTY_SECONDS * SOC_TICKS_PER_SECOND))
		{
			soc_set(soc_step(s_soc.soc, 0U, SOC_EMPTY_STEP));
			s_soc.empty_ticks = 0U;
		}
	}
	else
	{
		s_soc.empty_ticks = 0U;
	}
}

static void soc_apply_low_voltage_guard(UINT8 mode)
{
	UINT8 target;
	UINT8 limit;
	UINT8 margin;

	if ((mode == SOC_MODE_CHG) || !soc_voltage_valid() ||
		(SOC_Enhance_Element.u16_VCellMin > SOC_LOW_GUARD_MV))
	{
		s_soc.low_guard_ticks = 0U;
		return;
	}
	if ((mode == SOC_MODE_DSG) &&
		(SOC_Enhance_Element.u16_Idsg > soc_low_guard_current_a10()))
	{
		s_soc.low_guard_ticks = 0U;
		return;
	}
	target = soc_ocv_percent();
	margin = (SOC_Enhance_Element.u16_VCellMin <= SOC_LOW_GUARD_CRITICAL_MV) ?
		SOC_LOW_GUARD_CRIT_MARGIN : SOC_LOW_GUARD_MARGIN;
	limit = (target > (UINT8)(100U - margin)) ? 100U : (UINT8)(target + margin);
	if (s_soc.soc > limit)
	{
		if (++s_soc.low_guard_ticks >= (UINT16)(SOC_LOW_GUARD_SECONDS * SOC_TICKS_PER_SECOND))
		{
			soc_set(soc_step(s_soc.soc, limit, 1U));
			s_soc.low_guard_ticks = 0U;
		}
	}
	else
	{
		s_soc.low_guard_ticks = 0U;
	}
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
			(SOC_Enhance_Element.u16_VCellMin <= soc_empty_mv()))
		{
			seconds = SOC_DISPLAY_LOW_SECONDS;
		}
		else if ((target > s_soc.display_soc) && (s_soc.mode == SOC_MODE_CHG))
		{
			seconds = SOC_DISPLAY_CHG_SECONDS;
		}
		ticks = (UINT8)(seconds * SOC_TICKS_PER_SECOND);
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
	soc_apply_full_empty(s_soc.mode);
	soc_apply_low_voltage_guard(s_soc.mode);
	soc_update_rest_timer(s_soc.mode);
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
