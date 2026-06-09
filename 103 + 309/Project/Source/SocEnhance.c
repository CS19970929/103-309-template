#include "SocEnhance.h"
#include "conf.h"
#include "EEPROM.h"
#include "DataDeal.h"
#include "Flash.h"
#include "Sci_Upper.h"
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
#define SOC_CURRENT_ACTIVE_A10       ((UINT16)2U)
#define SOC_MA_PER_A10               ((int32_t)100)
#define SOC_MAMS_PER_AS10            ((UINT32)100000U)
#define SOC_BOARD_SELF_CONSUMPTION_MA ((UINT16)PROJECT_CFG_SOC_BOARD_SELF_CONSUMPTION_MA)
#define SOC_SOH_MIN                  ((UINT8)80U)
#define SOC_SOH_CYCLE_STEP           ((UINT16)100U)
#define SOC_FULL_SECONDS             ((UINT16)PROJECT_CFG_SOC_FULL_CONFIRM_SECONDS)
#define SOC_FULL_CONFIRM_MIN_VMAX_MV ((UINT16)4180U)
#define SOC_FULL_MIN_MARGIN_MV       ((UINT16)PROJECT_CFG_SOC_FULL_CONFIRM_MIN_CELL_MARGIN_MV)
#define SOC_FULL_MAX_DELTA_MV        ((UINT16)PROJECT_CFG_SOC_FULL_CONFIRM_MAX_CELL_DELTA_MV)
#define SOC_EMPTY_CUR_LIGHT_DIVIDER  ((UINT16)5U)
#define SOC_EMPTY_CUR_MID_DIVIDER    ((UINT16)2U)
#define SOC_SAG_HOLDOFF_SECONDS      ((UINT16)PROJECT_CFG_SOC_SAG_HOLDOFF_SECONDS)
#define SOC_SAG_ALLOW_OFFSET_MV      ((int16_t)PROJECT_CFG_SOC_SAG_ALLOW_OFFSET_MV)
#define SOC_REST_OCV_SECONDS         ((UINT32)PROJECT_CFG_SOC_REST_OCV_SECONDS)
#define SOC_LONG_REST_DOWN_STEP_SECONDS ((UINT32)PROJECT_CFG_SOC_REST_DOWN_STEP_SECONDS)
#define SOC_CAL_STEP                 ((UINT8)PROJECT_CFG_SOC_CALIBRATION_STEP_PERCENT)
#define SOC_EMPTY_TAIL_START_OFFSET_MV ((UINT16)PROJECT_CFG_SOC_EMPTY_TAIL_START_OFFSET_MV)
#define SOC_VALID_MIN_MV             ((UINT16)PROJECT_CFG_SOC_CALIBRATION_MIN_CELL_VALID_MV)
#define SOC_VALID_MAX_MV             ((UINT16)PROJECT_CFG_SOC_CALIBRATION_MAX_CELL_VALID_MV)
#define SOC_VALID_MAX_DELTA_MV       ((UINT16)300)
#define SOC_REST_MAX_DELTA_MV        ((UINT16)200U)
#define SOC_REST_STABLE_DELTA_MV     ((UINT16)30U)
#define SOC_REBOUND_BOOT_HOLDOFF_SECONDS ((UINT32)300U)
#define SOC_SNAPSHOT_FLAG_REBOUND_HOLD   ((UINT16)0x0001U)

typedef enum
{
	SOC_MODE_RELAX = 0,
	SOC_MODE_CHG = 1,
	SOC_MODE_DSG = 2
} SOC_MODE;

typedef enum
{
	SOC_EMPTY_BAND_RELAX = 0,
	SOC_EMPTY_BAND_LIGHT,
	SOC_EMPTY_BAND_MID,
	SOC_EMPTY_BAND_HEAVY,
	SOC_EMPTY_BAND_COUNT
} SOC_EMPTY_BAND;

typedef struct SOC_STATE_TAG
{
	UINT32 cap_factory_as10;
	UINT32 cap_full_as10;
	UINT32 cap_now_as10;
	UINT32 cycle_x100;
	UINT32 dsg_acc_as10;
	int32_t rem_mams;
	/* SOC rest counters below use 200ms SOC ticks, not RTC seconds. */
	UINT32 rest_soc_ticks;
	UINT32 stable_rest_soc_ticks;
	UINT32 long_rest_down_soc_ticks;
	UINT16 full_ticks;
	UINT16 empty_ticks;
	UINT16 sag_hold_ticks;
	UINT16 rest_ref_vmin;
	UINT16 rest_ref_vmax;
	UINT16 snapshot_flags;
	UINT8 soc;
	UINT8 soh;
	UINT8 rest_down_target;
	UINT8 rest_down_valid;
} SOC_STATE;

typedef struct SOC_SAVE_MARK_TAG
{
	UINT32 cycle_x100;
	UINT32 cap_full_as10;
	UINT16 snapshot_flags;
	UINT8 soc;
} SOC_SAVE_MARK;

typedef struct SOC_EMPTY_TAIL_RULE_TAG
{
	int16_t offset_mv;
	UINT8 target[SOC_EMPTY_BAND_COUNT];
	UINT16 ticks[SOC_EMPTY_BAND_COUNT];
} SOC_EMPTY_TAIL_RULE;

typedef struct
{
	UINT8 target;
	UINT16 ticks;
} SOC_TAIL_STEP;

static SOC_STATE s_soc;
static SOC_SAVE_MARK s_saved_soc;
static const UINT8 s_soc_default_startup_percent = 60U;
/* Cumulative RTC rest seconds already applied to SOC in the current sleep session. */
static UINT32 s_u32SocRtcRestAppliedSeconds;

static UINT32 soc_seconds_to_ticks(UINT32 seconds);
static UINT8 soc_sag_hold_blocks_calibration(void);
static UINT16 soc_table_percent(const UINT16 *table, UINT16 voltage_mv);
static void soc_save_current_snapshot(void);

// #define SOC_EMPTY_TAIL_STEP_TICKS		(5U * 60U)
#define SOC_EMPTY_TAIL_STEP_TICKS		(5U)
// static const SOC_EMPTY_TAIL_RULE s_empty_tail_table[] = {
// 	{
// 		-50,
// 		{0U, 0U, 0U, 0U},
// 		{SOC_EMPTY_TAIL_STEP_TICKS, SOC_EMPTY_TAIL_STEP_TICKS,
// 		 SOC_EMPTY_TAIL_STEP_TICKS, SOC_EMPTY_TAIL_STEP_TICKS}
// 	},
// 	{
// 		-25,
// 		{0U, 0U, 0U, 0U},
// 		{SOC_EMPTY_TAIL_STEP_TICKS, SOC_EMPTY_TAIL_STEP_TICKS,
// 		 SOC_EMPTY_TAIL_STEP_TICKS, SOC_EMPTY_TAIL_STEP_TICKS}
// 	},
// 	{
// 		0,
// 		{0U, 0U, 0U, 0U},
// 		{SOC_EMPTY_TAIL_STEP_TICKS, SOC_EMPTY_TAIL_STEP_TICKS,
// 		 SOC_EMPTY_TAIL_STEP_TICKS, SOC_EMPTY_TAIL_STEP_TICKS}
// 	},
// 	{
// 		50,
// 		{3U, 5U, 8U, 12U},
// 		{SOC_EMPTY_TAIL_STEP_TICKS, SOC_EMPTY_TAIL_STEP_TICKS,
// 		 SOC_EMPTY_TAIL_STEP_TICKS, SOC_EMPTY_TAIL_STEP_TICKS}
// 	},
// 	{
// 		100,
// 		{5U, 10U, 14U, 18U},
// 		{SOC_EMPTY_TAIL_STEP_TICKS, SOC_EMPTY_TAIL_STEP_TICKS,
// 		 SOC_EMPTY_TAIL_STEP_TICKS, SOC_EMPTY_TAIL_STEP_TICKS}
// 	},
// 	{
// 		200,
// 		{8U, 14U, 20U, 25U},
// 		{SOC_EMPTY_TAIL_STEP_TICKS, SOC_EMPTY_TAIL_STEP_TICKS,
// 		 SOC_EMPTY_TAIL_STEP_TICKS, SOC_EMPTY_TAIL_STEP_TICKS}
// 	},
// 	{
// 		300,
// 		{14U, 18U, 25U, 32U},
// 		{SOC_EMPTY_TAIL_STEP_TICKS, SOC_EMPTY_TAIL_STEP_TICKS,
// 		 SOC_EMPTY_TAIL_STEP_TICKS, SOC_EMPTY_TAIL_STEP_TICKS}
// 	},
// 	{
// 		400,
// 		{18U, 22U, 30U, 40U},
// 		{SOC_EMPTY_TAIL_STEP_TICKS, SOC_EMPTY_TAIL_STEP_TICKS,
// 		 SOC_EMPTY_TAIL_STEP_TICKS, SOC_EMPTY_TAIL_STEP_TICKS}
// 	},
// };
static const SOC_EMPTY_TAIL_RULE s_empty_tail_table[] = {
	{-50, {0U, 0U, 0U, 0U}, 	{10U, 10U, 10U, 10U}},
	{-25, {0U, 0U, 0U, 0U}, 	{10U, 10U, 10U, 10U}},
	{0,   {0U, 0U, 0U, 0U}, 	{50U, 50U, 50U, 50U}},
	{50,  {2U, 5U, 8U, 12U}, 	{50U, 50U, 50U, 50U}},
	{100, {5U, 10U, 14U, 18U},  {50U, 50U, 50U, 50U}},
	{200, {8U, 14U, 20U, 25U},  {60U, 50U, 40U, 30U}},
	{300, {14U, 18U, 25U, 32U}, {90U, 75U, 60U, 45U}},
	{400, {18U, 22U, 30U, 40U}, {120U, 100U, 80U, 60U}},
};

#if (PROJECT_CFG_BAT_CHEMISTRY == 1)
const UINT16 SOC_Table_LiFePO[SOC_TABLE_SIZE] = {
	3336, 100, 3332, 90, 3330, 80, 3327, 75, 3316, 70, 3301, 65,
	3294, 60, 3291, 55, 3290, 50, 3288, 45, 3286, 40, 3279, 35,
	3266, 30, 3254, 25, 3236, 20, 3212, 15, 3198, 10, 3112, 5,
	2526, 0, 1000, 0, 1000, 0,
};
#endif

#if (PROJECT_CFG_BAT_CHEMISTRY == 0)
const UINT16 SocTable_TernaryLi[SOC_TABLE_SIZE] = {
	4160, 100, 4100, 95, 4050, 90, 3995, 85, 3935, 80, 3880, 75,
	3835, 70, 3795, 65, 3760, 60, 3725, 55, 3695, 50, 3670, 45,
	3645, 40, 3615, 35, 3585, 30, 3555, 25, 3525, 20, 3480, 15,
	3400, 10, 3250, 5, 3000, 0,
};
#endif

static UINT32 soc_seconds_to_ticks(UINT32 seconds)
{
	return seconds * (UINT32)SOC_TICKS_PER_SECOND;
}

static UINT16 soc_cell_delta(void)
{
	return (UINT16)(g_stCellInfoReport.u16VCellMax - g_stCellInfoReport.u16VCellMin);
}

static UINT16 soc_abs_diff_u16(UINT16 a, UINT16 b)
{
	return (a >= b) ? (UINT16)(a - b) : (UINT16)(b - a);
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
	if (s_soc.cap_now_as10 > s_soc.cap_full_as10)
	{
		s_soc.cap_now_as10 = s_soc.cap_full_as10;
	}
}

static UINT8 soc_from_cap(void)
{
	UINT32 soc;

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
	s_soc.rem_mams = 0U;
}

static SOC_MODE soc_direction(int32_t net_current_ma)
{
	if (net_current_ma >=
		((int32_t)SOC_CURRENT_ACTIVE_A10 * SOC_MA_PER_A10))
	{
		return SOC_MODE_CHG;
	}
	if (net_current_ma <=
		(0 - ((int32_t)SOC_CURRENT_ACTIVE_A10 * SOC_MA_PER_A10)))
	{
		return SOC_MODE_DSG;
	}
	return SOC_MODE_RELAX;
}

static UINT8 soc_voltage_valid(void)
{
	if ((g_stCellInfoReport.u16VCellMin < SOC_VALID_MIN_MV) ||
		(g_stCellInfoReport.u16VCellMax < SOC_VALID_MIN_MV) ||
		(g_stCellInfoReport.u16VCellMin > SOC_VALID_MAX_MV) ||
		(g_stCellInfoReport.u16VCellMax > SOC_VALID_MAX_MV) ||
		(g_stCellInfoReport.u16VCellMax < g_stCellInfoReport.u16VCellMin))
	{
		return 0U;
	}
	return 1U;
}

static UINT8 soc_calibration_allowed(void)
{
	if (!soc_voltage_valid())
	{
		return 0U;
	}
	if (soc_cell_delta() > SOC_VALID_MAX_DELTA_MV)
	{
		return 0U;
	}
	return 1U;
}

static UINT8 soc_ocv_percent(void)
{
	const UINT16 *table;
	UINT16 soc;

#if (PROJECT_CFG_BAT_CHEMISTRY == 1)
	table = SOC_Table_LiFePO;
#else
	table = SocTable_TernaryLi;
#endif
	soc = soc_table_percent(table, g_stCellInfoReport.u16VCellMin);
	return (soc > 100U) ? 100U : (UINT8)soc;
}

static UINT16 soc_table_percent(const UINT16 *table, UINT16 voltage_mv)
{
	UINT16 i;
	int32_t value;
	int32_t dx;

	if (voltage_mv >= table[0])
	{
		return table[1U];
	}

	for (i = 0U; i <= (UINT16)(SOC_TABLE_SIZE - 4U); i = (UINT16)(i + 2U))
	{
		if (table[i] == table[i + 2U])
		{
			continue;
		}
		if (voltage_mv >= table[i + 2U])
		{
			dx = (int32_t)table[i + 2U] - (int32_t)table[i];
			value = (int32_t)table[i + 1U] +
				(((int32_t)voltage_mv - (int32_t)table[i]) *
				 ((int32_t)table[i + 3U] - (int32_t)table[i + 1U])) / dx;
			return (UINT16)value;
		}
	}
	return table[SOC_TABLE_SIZE - 1U];
}

static UINT16 soc_empty_threshold_mv(int16_t offset_mv)
{
	int32_t threshold = (int32_t)OtherElement.u16Soc_V_0 + (int32_t)offset_mv;

	if (threshold < 0)
	{
		return 0U;
	}
	if (threshold > (int32_t)SOC_VALID_MAX_MV)
	{
		return SOC_VALID_MAX_MV;
	}
	return (UINT16)threshold;
}

static UINT16 soc_current_limit_a10(UINT16 divider)
{
	UINT16 cap_a10 = OtherElement.u16Soc_Ah;
	UINT16 limit;

	limit = (UINT16)((cap_a10 + divider - 1U) / divider);
	return (limit < SOC_CURRENT_ACTIVE_A10) ? SOC_CURRENT_ACTIVE_A10 : limit;
}

static UINT16 soc_net_current_idsg_a10(int32_t net_current_ma)
{
	uint32_t current_a10;

	if (net_current_ma >= 0)
	{
		return 0U;
	}
	current_a10 = (uint32_t)((((uint64_t)(-(int64_t)net_current_ma)) + 50ULL) /
		100ULL);
	return (current_a10 > (uint32_t)0xFFFFU) ? (UINT16)0xFFFFU : (UINT16)current_a10;
}

void SOC_PublishReportData(void)
{
	UINT32 cycles = s_soc.cycle_x100 / 100U;

	g_stCellInfoReport.SocElement.u16Soc = s_soc.soc;
	g_stCellInfoReport.SocElement.u16Soh = s_soc.soh;
	g_stCellInfoReport.SocElement.u16CapacityNow = soc_cap_to_ah100(s_soc.cap_now_as10);
	g_stCellInfoReport.SocElement.u16CapacityFull = soc_cap_to_ah100(s_soc.cap_full_as10);
	g_stCellInfoReport.SocElement.u16CapacityFactory = soc_cap_to_ah100(s_soc.cap_factory_as10);
	g_stCellInfoReport.SocElement.u16Cycle_times = (cycles > 0xFFFFU) ? 0xFFFFU : (UINT16)cycles;
}

void SOC_RequestCapacityReset(void)
{
	UINT8 soc_keep = s_soc.soc;

	s_soc.cap_factory_as10 = soc_factory_cap_as10_from(OtherElement.u16Soc_Ah);
	s_soc.cycle_x100 = (UINT32)OtherElement.u16Soc_Cycle_times * 100U;
	s_soc.dsg_acc_as10 = 0U;
	soc_refresh_capacity_base();
	soc_set(soc_keep);
	soc_save_current_snapshot();
	SOC_PublishReportData();
}

void SOC_RequestSetOnce(UINT8 soc)
{
	soc_set(soc);
	soc_save_current_snapshot();
	SOC_PublishReportData();
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
	data.u16Flags = (UINT16)(s_soc.snapshot_flags & SOC_SNAPSHOT_FLAG_REBOUND_HOLD);
	data.u16DsgSocInt = (UINT16)(((uint64_t)s_soc.dsg_acc_as10 * 100ULL) / unit);
	if (data.u16DsgSocInt > 100U)
	{
		data.u16DsgSocInt = 100U;
	}
	return StorageFlash_SaveSocData(&data);
}

static void soc_update_save_mark(void)
{
	s_saved_soc.soc = s_soc.soc;
	s_saved_soc.cycle_x100 = s_soc.cycle_x100;
	s_saved_soc.cap_full_as10 = s_soc.cap_full_as10;
	s_saved_soc.snapshot_flags = s_soc.snapshot_flags;
}

static void soc_save_current_snapshot(void)
{
	if (soc_save())
	{
		soc_update_save_mark();
	}
}

static void soc_save_if_needed(void)
{
	if ((s_soc.soc != s_saved_soc.soc) ||
		(s_soc.cycle_x100 != s_saved_soc.cycle_x100) ||
		(s_soc.cap_full_as10 != s_saved_soc.cap_full_as10) ||
		(s_soc.snapshot_flags != s_saved_soc.snapshot_flags))
	{
		soc_save_current_snapshot();
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
		s_soc.dsg_acc_as10 = (data.u32LearnPassedAs10 != 0U) ?
			(data.u32LearnPassedAs10 % unit) :
			(UINT32)(((uint64_t)unit * data.u16DsgSocInt) / 100ULL);
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
		s_soc.snapshot_flags = (UINT16)(data.u16Flags & SOC_SNAPSHOT_FLAG_REBOUND_HOLD);
		if ((s_soc.snapshot_flags & SOC_SNAPSHOT_FLAG_REBOUND_HOLD) != 0U)
		{
			s_soc.sag_hold_ticks = (UINT16)(SOC_REBOUND_BOOT_HOLDOFF_SECONDS *
				SOC_TICKS_PER_SECOND);
		}
	}
	else
	{
		s_soc.cycle_x100 = (UINT32)OtherElement.u16Soc_Cycle_times * 100U;
		soc_refresh_capacity_base();
		if (soc_calibration_allowed())
		{
			soc_set(soc_ocv_percent());
		}
		else
		{
			soc_set(s_soc_default_startup_percent);
		}
		(void)soc_save();
	}
	soc_update_save_mark();
}

static void soc_clear_rest_down_target(void)
{
	s_soc.rest_down_valid = 0U;
	s_soc.rest_down_target = 0U;
	s_soc.long_rest_down_soc_ticks = 0U;
}

static void soc_set_rest_down_target(UINT8 target)
{
	if (target >= s_soc.soc)
	{
		soc_clear_rest_down_target();
		return;
	}
	if ((!s_soc.rest_down_valid) || (s_soc.rest_down_target != target))
	{
		s_soc.rest_down_target = target;
		s_soc.rest_down_valid = 1U;
		s_soc.long_rest_down_soc_ticks = 0U;
	}
}

static void soc_integrate(int32_t net_current_ma)
{
	int32_t delta_as10;
	int64_t acc_mams;
	int64_t cap_now_as10;
	UINT8 old_soc;

	acc_mams = (((int64_t)net_current_ma -
		(int64_t)SOC_BOARD_SELF_CONSUMPTION_MA) * (int64_t)SOC_TICK_MS) +
		(int64_t)s_soc.rem_mams;
	delta_as10 = (int32_t)(acc_mams / (int64_t)SOC_MAMS_PER_AS10);
	s_soc.rem_mams = (int32_t)(acc_mams % (int64_t)SOC_MAMS_PER_AS10);
	if (delta_as10 == 0)
	{
		return;
	}
	old_soc = s_soc.soc;
	if (delta_as10 < 0)
	{
		UINT32 dsg_as10 = (UINT32)(-(int64_t)delta_as10);
		UINT32 unit = s_soc.cap_factory_as10 / 100U;

		s_soc.dsg_acc_as10 += dsg_as10;
		s_soc.cycle_x100 += s_soc.dsg_acc_as10 / unit;
		s_soc.dsg_acc_as10 %= unit;
		soc_refresh_capacity_base();
	}
	cap_now_as10 = (int64_t)s_soc.cap_now_as10 + (int64_t)delta_as10;
	if (cap_now_as10 < 0)
	{
		cap_now_as10 = 0;
	}
	else if (cap_now_as10 > (int64_t)s_soc.cap_full_as10)
	{
		cap_now_as10 = (int64_t)s_soc.cap_full_as10;
	}
	s_soc.cap_now_as10 = (UINT32)cap_now_as10;
	s_soc.soc = soc_from_cap();
	if ((delta_as10 > 0) && (old_soc < 100U) && (s_soc.soc >= 100U))
	{
		s_soc.soc = 99U;
		s_soc.cap_now_as10 = (UINT32)(((uint64_t)s_soc.cap_full_as10 * 99ULL) / 100ULL);
	}
}

static UINT8 soc_full_confirm_allowed(void)
{
	UINT16 full_mv = OtherElement.u16Soc_V_100;
	UINT16 full_min_mv = (full_mv > SOC_FULL_MIN_MARGIN_MV) ?
		(UINT16)(full_mv - SOC_FULL_MIN_MARGIN_MV) : 0U;
	UINT16 delta;

	if (!soc_calibration_allowed() ||
		(g_stCellInfoReport.u16VCellMax <= SOC_FULL_CONFIRM_MIN_VMAX_MV) ||
		(g_stCellInfoReport.u16VCellMax < full_min_mv))
	{
		return 0U;
	}

	delta = soc_cell_delta();
	if ((g_stCellInfoReport.u16VCellMin >= full_min_mv) &&
		(delta <= SOC_FULL_MAX_DELTA_MV))
	{
		return 1U;
	}
	return 0U;
}

static void soc_update_sag_hold(SOC_MODE mode, int32_t net_current_ma)
{
	if ((mode == SOC_MODE_DSG) &&
		(soc_net_current_idsg_a10(net_current_ma) >
		 soc_current_limit_a10(SOC_EMPTY_CUR_MID_DIVIDER)))
	{
		s_soc.sag_hold_ticks = (UINT16)(SOC_SAG_HOLDOFF_SECONDS *
			SOC_TICKS_PER_SECOND);
		s_soc.snapshot_flags |= SOC_SNAPSHOT_FLAG_REBOUND_HOLD;
	}
	else if (s_soc.sag_hold_ticks > 0U)
	{
		--s_soc.sag_hold_ticks;
		if (s_soc.sag_hold_ticks == 0U)
		{
			s_soc.snapshot_flags &= (UINT16)(~SOC_SNAPSHOT_FLAG_REBOUND_HOLD);
		}
	}
	else
	{
		s_soc.snapshot_flags &= (UINT16)(~SOC_SNAPSHOT_FLAG_REBOUND_HOLD);
	}
}

static UINT8 soc_sag_hold_blocks_calibration(void)
{
	return (UINT8)((s_soc.sag_hold_ticks > 0U) &&
		soc_voltage_valid() &&
		(g_stCellInfoReport.u16VCellMin >
		 soc_empty_threshold_mv(SOC_SAG_ALLOW_OFFSET_MV)));
}

static UINT8 soc_select_empty_tail_step(SOC_MODE mode, int32_t net_current_ma, SOC_TAIL_STEP *step)
{
	SOC_EMPTY_BAND band;
	UINT16 i;
	UINT16 threshold;

	if ((mode == SOC_MODE_CHG) || !soc_voltage_valid())
	{
		return 0U;
	}
	if (soc_sag_hold_blocks_calibration())
	{
		return 0U;
	}
	if (g_stCellInfoReport.u16VCellMin >
		soc_empty_threshold_mv(SOC_EMPTY_TAIL_START_OFFSET_MV))
	{
		return 0U;
	}

	if (mode == SOC_MODE_RELAX)
	{
		band = SOC_EMPTY_BAND_RELAX;
	}
	else if (soc_net_current_idsg_a10(net_current_ma) <=
		soc_current_limit_a10(SOC_EMPTY_CUR_LIGHT_DIVIDER))
	{
		band = SOC_EMPTY_BAND_LIGHT;
	}
	else if (soc_net_current_idsg_a10(net_current_ma) <=
		soc_current_limit_a10(SOC_EMPTY_CUR_MID_DIVIDER))
	{
		band = SOC_EMPTY_BAND_MID;
	}
	else
	{
		band = SOC_EMPTY_BAND_HEAVY;
	}

	for (i = 0U; i < (UINT16)(sizeof(s_empty_tail_table) / sizeof(s_empty_tail_table[0])); ++i)
	{
		threshold = soc_empty_threshold_mv(s_empty_tail_table[i].offset_mv);
		if (g_stCellInfoReport.u16VCellMin <= threshold)
		{
			step->target = s_empty_tail_table[i].target[band];
			step->ticks = s_empty_tail_table[i].ticks[band];
			if (step->ticks == 0U)
			{
				step->ticks = 1U;
			}
			return 1U;
		}
	}
	return 0U;
}

static UINT8 soc_apply_full_confirm(UINT8 active)
{
	UINT16 full_confirm_ticks;
	UINT8 old_soc = s_soc.soc;

	if (!active)
	{
		s_soc.full_ticks = 0U;
		return 0U;
	}
	full_confirm_ticks = (UINT16)(SOC_FULL_SECONDS * SOC_TICKS_PER_SECOND);
	s_soc.empty_ticks = 0U;
	if (s_soc.full_ticks < full_confirm_ticks)
	{
		++s_soc.full_ticks;
	}
	if (s_soc.full_ticks >= full_confirm_ticks)
	{
		soc_set(soc_step(s_soc.soc, 100U, SOC_CAL_STEP));
		s_soc.full_ticks = 0U;
	}
	return (UINT8)(s_soc.soc != old_soc);
}

static UINT8 soc_apply_empty_tail(UINT8 empty_active, const SOC_TAIL_STEP *empty_step)
{
	UINT8 old_soc = s_soc.soc;

	if (!empty_active)
	{
		s_soc.empty_ticks = 0U;
		return 0U;
	}
	if (++s_soc.empty_ticks < empty_step->ticks)
	{
		return 0U;
	}
	if (s_soc.soc > empty_step->target)
	{
		soc_set(soc_step(s_soc.soc, empty_step->target, SOC_CAL_STEP));
	}
	s_soc.empty_ticks = 0U;
	return (UINT8)(s_soc.soc != old_soc);
}

static void soc_reset_rest_confidence(void)
{
	s_soc.rest_soc_ticks = 0U;
	s_soc.stable_rest_soc_ticks = 0U;
	soc_clear_rest_down_target();
	s_soc.rest_ref_vmin = 0U;
	s_soc.rest_ref_vmax = 0U;
}

static UINT8 soc_apply_long_rest_down_step(UINT32 delta_soc_ticks)
{
	UINT32 step_ticks = soc_seconds_to_ticks(SOC_LONG_REST_DOWN_STEP_SECONDS);
	UINT8 changed;
	UINT8 old_soc = s_soc.soc;

	if ((!s_soc.rest_down_valid) ||
		(s_soc.rest_down_target >= s_soc.soc) ||
		(s_soc.rest_soc_ticks < soc_seconds_to_ticks(SOC_REST_OCV_SECONDS)))
	{
		s_soc.long_rest_down_soc_ticks = 0U;
		return 0U;
	}
	if (s_soc.long_rest_down_soc_ticks < step_ticks)
	{
		if (delta_soc_ticks > (step_ticks - s_soc.long_rest_down_soc_ticks))
		{
			s_soc.long_rest_down_soc_ticks = step_ticks;
		}
		else
		{
			s_soc.long_rest_down_soc_ticks += delta_soc_ticks;
		}
	}
	if (s_soc.long_rest_down_soc_ticks < step_ticks)
	{
		return 0U;
	}
	if (!soc_calibration_allowed() || soc_sag_hold_blocks_calibration())
	{
		changed = 0U;
	}
	else
	{
		soc_set(soc_step(s_soc.soc, s_soc.rest_down_target, SOC_CAL_STEP));
		changed = (UINT8)(s_soc.soc != old_soc);
	}
	s_soc.long_rest_down_soc_ticks = 0U;
	if (s_soc.rest_down_target == s_soc.soc)
	{
		soc_clear_rest_down_target();
	}
	return changed;
}

static UINT8 soc_rest_voltage_stable(void)
{
	if (!soc_calibration_allowed() ||
		(soc_cell_delta() > SOC_REST_MAX_DELTA_MV) ||
		soc_sag_hold_blocks_calibration())
	{
		return 0U;
	}
	if ((s_soc.rest_ref_vmin == 0U) || (s_soc.rest_ref_vmax == 0U))
	{
		s_soc.rest_ref_vmin = g_stCellInfoReport.u16VCellMin;
		s_soc.rest_ref_vmax = g_stCellInfoReport.u16VCellMax;
		return 1U;
	}
	if ((soc_abs_diff_u16(g_stCellInfoReport.u16VCellMin, s_soc.rest_ref_vmin) <= SOC_REST_STABLE_DELTA_MV) &&
		(soc_abs_diff_u16(g_stCellInfoReport.u16VCellMax, s_soc.rest_ref_vmax) <= SOC_REST_STABLE_DELTA_MV))
	{
		return 1U;
	}
	s_soc.rest_ref_vmin = g_stCellInfoReport.u16VCellMin;
	s_soc.rest_ref_vmax = g_stCellInfoReport.u16VCellMax;
	return 0U;
}

static void soc_update_rest_timer(SOC_MODE mode)
{
	UINT32 rest_ocv_ticks = soc_seconds_to_ticks(SOC_REST_OCV_SECONDS);

	if (mode != SOC_MODE_RELAX)
	{
		soc_reset_rest_confidence();
		return;
	}
	if (s_soc.rest_soc_ticks < rest_ocv_ticks)
	{
		++s_soc.rest_soc_ticks;
	}
	if (soc_rest_voltage_stable())
	{
		if (s_soc.stable_rest_soc_ticks < rest_ocv_ticks)
		{
			++s_soc.stable_rest_soc_ticks;
		}
	}
	else
	{
		s_soc.stable_rest_soc_ticks = 0U;
		soc_clear_rest_down_target();
	}
	if ((s_soc.rest_soc_ticks >= rest_ocv_ticks) &&
		(s_soc.stable_rest_soc_ticks >= rest_ocv_ticks))
	{
		soc_set_rest_down_target(soc_ocv_percent());
	}
	(void)soc_apply_long_rest_down_step(1U);
}

static void soc_add_rest_seconds(UINT32 *soc_ticks, UINT32 seconds, UINT32 limit_seconds)
{
	UINT32 limit_soc_ticks = soc_seconds_to_ticks(limit_seconds);
	UINT32 delta_soc_ticks = soc_seconds_to_ticks(seconds);

	if (*soc_ticks >= limit_soc_ticks)
	{
		return;
	}
	if (delta_soc_ticks > (limit_soc_ticks - *soc_ticks))
	{
		*soc_ticks = limit_soc_ticks;
	}
	else
	{
		*soc_ticks += delta_soc_ticks;
	}
}

static UINT8 soc_apply_rtc_rest_ocv(UINT32 rest_seconds)
{
	UINT32 delta_seconds;
	UINT32 rest_ocv_ticks = soc_seconds_to_ticks(SOC_REST_OCV_SECONDS);
	UINT8 has_rest_ref;
	UINT8 changed = 0U;

	if (rest_seconds < s_u32SocRtcRestAppliedSeconds)
	{
		soc_reset_rest_confidence();
		s_u32SocRtcRestAppliedSeconds = 0U;
	}

	delta_seconds = rest_seconds - s_u32SocRtcRestAppliedSeconds;
	s_u32SocRtcRestAppliedSeconds = rest_seconds;
	if (delta_seconds == 0U)
	{
		return 0U;
	}

	soc_add_rest_seconds(&s_soc.rest_soc_ticks, delta_seconds, SOC_REST_OCV_SECONDS);
	has_rest_ref = (UINT8)((s_soc.rest_ref_vmin != 0U) && (s_soc.rest_ref_vmax != 0U));
	if (soc_rest_voltage_stable())
	{
		if (!has_rest_ref)
		{
			return changed;
		}
		soc_add_rest_seconds(&s_soc.stable_rest_soc_ticks, delta_seconds,
			SOC_REST_OCV_SECONDS);
	}
	else
	{
		s_soc.stable_rest_soc_ticks = 0U;
		soc_clear_rest_down_target();
		return changed;
	}

	if ((s_soc.rest_soc_ticks >= rest_ocv_ticks) &&
		(s_soc.stable_rest_soc_ticks >= rest_ocv_ticks))
	{
		soc_set_rest_down_target(soc_ocv_percent());
	}
	changed |= soc_apply_long_rest_down_step(soc_seconds_to_ticks(delta_seconds));
	return changed;
}

void soc_param_lib_init(void)
{
	memset(&s_soc, 0, sizeof(s_soc));
	s_soc.cap_factory_as10 = soc_factory_cap_as10_from(OtherElement.u16Soc_Ah);
	s_soc.cycle_x100 = (UINT32)OtherElement.u16Soc_Cycle_times * 100U;
	s_u32SocRtcRestAppliedSeconds = 0U;
	soc_refresh_capacity_base();
	soc_load_or_default();
	SOC_PublishReportData();
}

UINT8 SOC_ResetStoredSnapshotToDefault(void)
{
	STORAGE_FLASH_SOC_DATA data;
	UINT32 cap_factory = soc_factory_cap_as10_from(OtherElement.u16Soc_Ah);
	UINT32 cycle_x100 = (UINT32)OtherElement.u16Soc_Cycle_times * 100U;
	UINT32 cap_full = (UINT32)(((uint64_t)cap_factory * soc_soh_from_cycle(cycle_x100)) / 100ULL);

	memset(&data, 0, sizeof(data));
	data.u16FormatVersion = FLASH_STORAGE_SOC_DATA_VERSION_V2;
	data.u16SocNow = s_soc_default_startup_percent;
	data.u16MaxErrorPercent = 100U;
	data.u32CycleTimes = cycle_x100;
	data.u32CapFull = cap_full;
	data.u32CapNow = (UINT32)(((uint64_t)cap_full * s_soc_default_startup_percent) / 100ULL);
	return StorageFlash_SaveSocData(&data);
}

void SOC_SaveSnapshotBeforeSleep(void)
{
	soc_save_if_needed();
}

void SOC_IntEnhance_Ctrl(int32_t net_current_ma)
{
	SOC_TAIL_STEP empty_tail_step;
	UINT8 voltage_calibrated;
	UINT8 empty_tail_active;
	UINT8 full_confirm_active;
	UINT8 sag_hold_blocked;
	SOC_MODE mode;

	/* Order: integrate, sag hold, voltage calibration, rest, save, publish. */
	mode = soc_direction(net_current_ma);
	soc_integrate(net_current_ma);
	soc_update_sag_hold(mode, net_current_ma);

	full_confirm_active = (UINT8)((mode != SOC_MODE_DSG) && soc_full_confirm_allowed());
	voltage_calibrated = soc_apply_full_confirm(full_confirm_active);

	empty_tail_active = 0U;
	if (!full_confirm_active)
	{
		empty_tail_active = soc_select_empty_tail_step(mode, net_current_ma, &empty_tail_step);
		if (soc_apply_empty_tail(empty_tail_active, &empty_tail_step))
		{
			voltage_calibrated = 1U;
		}
	}

	sag_hold_blocked = soc_sag_hold_blocks_calibration();
	if (empty_tail_active || sag_hold_blocked)
	{
		soc_reset_rest_confidence();
	}
	else if (!voltage_calibrated)
	{
		soc_update_rest_timer(mode);
	}

	soc_save_if_needed();
	SOC_PublishReportData();
}

void SOC_ApplyRtcRelaxationCompensation(UINT32 rest_seconds, UINT16 vcell_min, UINT16 vcell_max)
{
	UINT8 changed;

	g_stCellInfoReport.u16VCellMin = vcell_min;
	g_stCellInfoReport.u16VCellMax = vcell_max;
	changed = soc_apply_rtc_rest_ocv(rest_seconds);
	if (changed)
	{
		soc_save_current_snapshot();
	}
	SOC_PublishReportData();
}
