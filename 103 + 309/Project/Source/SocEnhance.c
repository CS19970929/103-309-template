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
#define SOC_DEFAULT_CAP_A10          ((UINT16)270U)
#define SOC_SOH_MIN                  ((UINT8)80U)
#define SOC_SOH_CYCLE_STEP           ((UINT16)100U)
#define SOC_FULL_SECONDS             ((UINT16)PROJECT_CFG_SOC_FULL_CONFIRM_SECONDS)
#define SOC_DEFAULT_FULL_MV          ((UINT16)4180U)
#define SOC_FULL_CONFIRM_MIN_VMAX_MV SOC_DEFAULT_FULL_MV
#define SOC_FULL_MIN_MARGIN_MV       ((UINT16)PROJECT_CFG_SOC_FULL_CONFIRM_MIN_CELL_MARGIN_MV)
#define SOC_FULL_MAX_DELTA_MV        ((UINT16)PROJECT_CFG_SOC_FULL_CONFIRM_MAX_CELL_DELTA_MV)
#define SOC_EMPTY_MV                 ((UINT16)3000U)
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
#define SOC_MODE_RELAX               ((UINT8)0U)
#define SOC_MODE_CHG                 ((UINT8)1U)
#define SOC_MODE_DSG                 ((UINT8)2U)
#define SOC_EMPTY_BAND_RELAX         ((UINT8)0U)
#define SOC_EMPTY_BAND_LIGHT         ((UINT8)1U)
#define SOC_EMPTY_BAND_MID           ((UINT8)2U)
#define SOC_EMPTY_BAND_HEAVY         ((UINT8)3U)
#define SOC_EMPTY_BAND_COUNT         ((UINT8)4U)
#define SOC_REST_MAX_DELTA_MV        ((UINT16)200U)
#define SOC_REST_STABLE_DELTA_MV     ((UINT16)30U)
#define SOC_REBOUND_BOOT_HOLDOFF_SECONDS ((UINT32)300U)
#define SOC_SNAPSHOT_FLAG_REBOUND_HOLD   ((UINT16)0x0001U)

/* Compile-time tick equivalents of time-based config */
#define SOC_SEC_TO_TICKS(sec)               ((UINT32)(sec) * (UINT32)SOC_TICKS_PER_SECOND)
#define SOC_REST_LIMIT_TICKS                SOC_SEC_TO_TICKS(SOC_REST_OCV_SECONDS)
#define SOC_STABLE_LIMIT_TICKS              SOC_REST_LIMIT_TICKS
#define SOC_LONG_REST_DOWN_STEP_TICKS       SOC_SEC_TO_TICKS(SOC_LONG_REST_DOWN_STEP_SECONDS)

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
/* Cumulative RTC rest seconds already applied to SOC in the current sleep session. */
static UINT32 s_u32SocRtcRestAppliedSeconds;

static UINT8 soc_sag_hold_blocks_calibration(void);
static UINT16 soc_table_percent(const UINT16 *table, UINT16 size, UINT16 voltage_mv);
static void soc_save_current_snapshot(void);
static void soc_publish(void);

#define DELAY_SOC_TEST		(5 * 60)
static const SOC_EMPTY_TAIL_RULE s_empty_tail_table[] = {
	{-50, {0U, 0U, 0U, 0U}, {DELAY_SOC_TEST, DELAY_SOC_TEST, DELAY_SOC_TEST, DELAY_SOC_TEST}},
	{-25, {0U, 0U, 0U, 0U}, {DELAY_SOC_TEST, DELAY_SOC_TEST, DELAY_SOC_TEST, DELAY_SOC_TEST}},
	{0, {0U, 0U, 0U, 0U}, 	{DELAY_SOC_TEST, DELAY_SOC_TEST, DELAY_SOC_TEST, DELAY_SOC_TEST}},
	{50, {4U, 5U, 8U, 12U}, {DELAY_SOC_TEST, DELAY_SOC_TEST, DELAY_SOC_TEST, DELAY_SOC_TEST}},
	{100, {8U, 10U, 14U, 18U}, {DELAY_SOC_TEST, DELAY_SOC_TEST, DELAY_SOC_TEST, DELAY_SOC_TEST}},
	{200, {12U, 14U, 20U, 25U}, {DELAY_SOC_TEST, DELAY_SOC_TEST, DELAY_SOC_TEST, DELAY_SOC_TEST}},
	{300, {14U, 18U, 25U, 32U}, {DELAY_SOC_TEST, DELAY_SOC_TEST, DELAY_SOC_TEST, DELAY_SOC_TEST}},
	{400, {18U, 22U, 30U, 40U}, {DELAY_SOC_TEST, DELAY_SOC_TEST, DELAY_SOC_TEST, DELAY_SOC_TEST}},
};

#if (PROJECT_CFG_BAT_CHEMISTRY == 1)
const UINT16 SOC_Table_LiFePO[SOC_Size_LiFePO] = {
	3336, 100, 3332, 90, 3330, 80, 3327, 75, 3316, 70, 3301, 65,
	3294, 60, 3291, 55, 3290, 50, 3288, 45, 3286, 40, 3279, 35,
	3266, 30, 3254, 25, 3236, 20, 3212, 15, 3198, 10, 3112, 5,
	2526, 0, 1000, 0, 1000, 0,
};
#endif

#if (PROJECT_CFG_BAT_CHEMISTRY == 0)
const UINT16 SocTable_TernaryLi[SOC_Size_TernaryLi] = {
	4160, 100, 4100, 95, 4050, 90, 3995, 85, 3935, 80, 3880, 75,
	3835, 70, 3795, 65, 3760, 60, 3725, 55, 3695, 50, 3670, 45,
	3645, 40, 3615, 35, 3585, 30, 3555, 25, 3525, 20, 3480, 15,
	3400, 10, 3250, 5, 3000, 0,
};
#endif

static UINT16 soc_cell_delta(void)
{
	return (g_stCellInfoReport.u16VCellMax >= g_stCellInfoReport.u16VCellMin) ?
		(UINT16)(g_stCellInfoReport.u16VCellMax - g_stCellInfoReport.u16VCellMin) :
		(UINT16)(g_stCellInfoReport.u16VCellMin - g_stCellInfoReport.u16VCellMax);
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
	s_soc.rem_mams = 0U;
}

static UINT8 soc_direction(int32_t net_current_ma)
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

static UINT16 soc_voltage_with_margin(UINT16 base_mv, UINT16 margin_mv)
{
	return (base_mv > margin_mv) ? (UINT16)(base_mv - margin_mv) : 0U;
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

static const UINT16 *soc_ocv_table(UINT16 *size)
{
#if (PROJECT_CFG_BAT_CHEMISTRY == 1)
	*size = SOC_Size_LiFePO;
	return SOC_Table_LiFePO;
#else
	*size = SOC_Size_TernaryLi;
	return SocTable_TernaryLi;
#endif
}

static UINT8 soc_ocv_percent(void)
{
	UINT16 size;
	const UINT16 *table = soc_ocv_table(&size);
	UINT16 soc = soc_table_percent(table, size, g_stCellInfoReport.u16VCellMin);
	return (soc > 100U) ? 100U : (UINT8)soc;
}

static UINT16 soc_table_percent(const UINT16 *table, UINT16 size, UINT16 voltage_mv)
{
	UINT16 i;
	UINT16 last;
	UINT16 x1;
	UINT16 x2;
	int32_t value;
	int32_t dx;

	if ((table == 0) || (size < 4U))
	{
		return 0U;
	}

	for (i = 0U; i <= (UINT16)(size - 4U); i = (UINT16)(i + 2U))
	{
		x1 = table[i];
		x2 = table[i + 2U];
		if (((voltage_mv >= x1) && (voltage_mv <= x2)) ||
			((voltage_mv <= x1) && (voltage_mv >= x2)))
		{
			if (x1 == x2)
			{
				return table[i + 1U];
			}
			dx = (int32_t)x2 - (int32_t)x1;
			value = (int32_t)table[i + 1U] +
				(((int32_t)voltage_mv - (int32_t)x1) *
				 ((int32_t)table[i + 3U] - (int32_t)table[i + 1U])) / dx;
			if (value <= 0)
			{
				return 0U;
			}
			return (value > (int32_t)0xFFFFU) ? 0xFFFFU : (UINT16)value;
		}
	}

	last = (UINT16)(size - 2U);
	if (table[0] <= table[last])
	{
		return (voltage_mv >= table[last]) ? table[last + 1U] : table[1U];
	}
	return (voltage_mv >= table[0]) ? table[1U] : table[last + 1U];
}

static UINT16 soc_empty_threshold_mv(int16_t offset_mv)
{
	UINT16 empty_mv = (OtherElement.u16Soc_V_0 != 0U) ?
		OtherElement.u16Soc_V_0 : SOC_EMPTY_MV;
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

static UINT16 soc_current_limit_a10(UINT16 divider)
{
	UINT16 cap_a10 = (OtherElement.u16Soc_Ah != 0U) ?
		OtherElement.u16Soc_Ah : SOC_DEFAULT_CAP_A10;
	UINT16 limit;

	if (divider == 0U)
	{
		divider = 1U;
	}
	limit = (UINT16)((cap_a10 + divider - 1U) / divider);
	return (limit < SOC_CURRENT_ACTIVE_A10) ? SOC_CURRENT_ACTIVE_A10 : limit;
}

static UINT16 soc_current_ma_to_a10(int32_t current_ma)
{
	uint64_t magnitude_ma;
	uint32_t current_a10;

	magnitude_ma = (current_ma >= 0) ? (uint64_t)current_ma : (uint64_t)(0 - (int64_t)current_ma);
	current_a10 = (uint32_t)((magnitude_ma + 50ULL) / 100ULL);
	return (current_a10 > (uint32_t)0xFFFFU) ? (UINT16)0xFFFFU : (UINT16)current_a10;
}

static UINT16 soc_net_current_idsg_a10(int32_t net_current_ma)
{
	return (net_current_ma < 0) ? soc_current_ma_to_a10(net_current_ma) : 0U;
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
	soc_publish();
}

void SOC_RequestSetOnce(UINT8 soc)
{
	soc_set(soc);
	soc_save_current_snapshot();
	soc_publish();
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
			soc_set(SOC_DEFAULT_STARTUP_PERCENT);
		}
		(void)soc_save();
	}
	soc_update_save_mark();
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
	int32_t current_ma_signed;
	int32_t delta_as10;
	int64_t acc_mams;
	int64_t cap_now_as10;
	UINT8 old_soc;

	current_ma_signed = net_current_ma -
		(int32_t)SOC_BOARD_SELF_CONSUMPTION_MA;
	if (current_ma_signed == 0)
	{
		return;
	}
	acc_mams = ((int64_t)current_ma_signed * (int64_t)SOC_TICK_MS) +
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
		soc_add_discharge((UINT32)(-(int64_t)delta_as10));
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
	UINT16 full_mv = (OtherElement.u16Soc_V_100 != 0U) ?
		OtherElement.u16Soc_V_100 : SOC_DEFAULT_FULL_MV;
	UINT16 vmax_min = soc_voltage_with_margin(full_mv, SOC_FULL_MIN_MARGIN_MV);
	UINT16 vmin_min = vmax_min;
	UINT16 delta;

	if (!soc_calibration_allowed() ||
		(g_stCellInfoReport.u16VCellMax <= SOC_FULL_CONFIRM_MIN_VMAX_MV) ||
		(g_stCellInfoReport.u16VCellMax < vmax_min))
	{
		return 0U;
	}

	delta = soc_cell_delta();
	if ((g_stCellInfoReport.u16VCellMin >= vmin_min) &&
		(delta <= SOC_FULL_MAX_DELTA_MV))
	{
		return 1U;
	}
	return 0U;
}

static void soc_update_sag_hold(UINT8 mode, int32_t net_current_ma)
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

static UINT8 soc_apply_tail_step(const SOC_TAIL_STEP *step, UINT16 *counter)
{
	UINT8 old_soc = s_soc.soc;

	if (++(*counter) >= step->ticks)
	{
		if (s_soc.soc > step->target)
		{
			soc_set(soc_step(s_soc.soc, step->target, SOC_CAL_STEP));
		}
		*counter = 0U;
	}
	return (UINT8)(s_soc.soc != old_soc);
}

static UINT8 soc_low_tail_config(UINT8 mode, int32_t net_current_ma, SOC_TAIL_STEP *step)
{
	UINT8 band;
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

static UINT8 soc_apply_full_empty(UINT8 mode,
								  UINT8 empty_active,
								  const SOC_TAIL_STEP *empty_step)
{
	UINT16 full_confirm_ticks;
	UINT8 old_soc = s_soc.soc;

	if (mode != SOC_MODE_DSG)
	{
		if (soc_full_confirm_allowed())
		{
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
		s_soc.full_ticks = 0U;
	}
	else
	{
		s_soc.full_ticks = 0U;
	}

	if (!empty_active)
	{
		s_soc.empty_ticks = 0U;
		return 0U;
	}
	if (soc_apply_tail_step(empty_step, &s_soc.empty_ticks))
	{
		return 1U;
	}
	return 0U;
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
	UINT8 changed;
	UINT8 old_soc = s_soc.soc;

	if ((!s_soc.rest_down_valid) ||
		(s_soc.rest_down_target >= s_soc.soc) ||
		(s_soc.rest_soc_ticks < SOC_REST_LIMIT_TICKS))
	{
		s_soc.long_rest_down_soc_ticks = 0U;
		return 0U;
	}
	if (s_soc.long_rest_down_soc_ticks < SOC_LONG_REST_DOWN_STEP_TICKS)
	{
		if (delta_soc_ticks > (SOC_LONG_REST_DOWN_STEP_TICKS - s_soc.long_rest_down_soc_ticks))
		{
			s_soc.long_rest_down_soc_ticks = SOC_LONG_REST_DOWN_STEP_TICKS;
		}
		else
		{
			s_soc.long_rest_down_soc_ticks += delta_soc_ticks;
		}
	}
	if (s_soc.long_rest_down_soc_ticks < SOC_LONG_REST_DOWN_STEP_TICKS)
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

static void soc_update_rest_timer(UINT8 mode)
{

	if (mode != SOC_MODE_RELAX)
	{
		soc_reset_rest_confidence();
		return;
	}
	if (s_soc.rest_soc_ticks < SOC_REST_LIMIT_TICKS)
	{
		++s_soc.rest_soc_ticks;
	}
	if (soc_rest_voltage_stable())
	{
		if (s_soc.stable_rest_soc_ticks < SOC_STABLE_LIMIT_TICKS)
		{
			++s_soc.stable_rest_soc_ticks;
		}
	}
	else
	{
		s_soc.stable_rest_soc_ticks = 0U;
		soc_clear_rest_down_target();
	}
	if ((s_soc.rest_soc_ticks >= SOC_REST_LIMIT_TICKS) &&
		(s_soc.stable_rest_soc_ticks >= SOC_STABLE_LIMIT_TICKS))
	{
		soc_set_rest_down_target(soc_ocv_percent());
	}
	(void)soc_apply_long_rest_down_step(1U);
}

static void soc_add_rest_seconds(UINT32 *soc_ticks, UINT32 seconds, UINT32 limit_seconds)
{
	UINT32 limit_soc_ticks = SOC_SEC_TO_TICKS(limit_seconds);
	UINT32 delta_soc_ticks = SOC_SEC_TO_TICKS(seconds);

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

	if ((s_soc.rest_soc_ticks >= SOC_REST_LIMIT_TICKS) &&
		(s_soc.stable_rest_soc_ticks >= SOC_STABLE_LIMIT_TICKS))
	{
		soc_set_rest_down_target(soc_ocv_percent());
	}
	changed |= soc_apply_long_rest_down_step(SOC_SEC_TO_TICKS(delta_seconds));
	return changed;
}

static void soc_publish(void)
{
	SOC_PublishReportData();
}

void soc_param_lib_init(void)
{
	memset(&s_soc, 0, sizeof(s_soc));
	s_soc.cap_factory_as10 = soc_factory_cap_as10_from(OtherElement.u16Soc_Ah);
	s_soc.cycle_x100 = (UINT32)OtherElement.u16Soc_Cycle_times * 100U;
	s_u32SocRtcRestAppliedSeconds = 0U;
	soc_refresh_capacity_base();
	soc_load_or_default();
	soc_publish();
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

void SOC_SaveSnapshotBeforeSleep(void)
{
	soc_save_if_needed();
}

void SOC_IntEnhance_Ctrl(int32_t net_current_ma)
{
	SOC_TAIL_STEP low_tail_step;
	UINT8 calibration_applied;
	UINT8 low_tail_active;
	UINT8 sag_hold_blocked;
	UINT8 mode;

	/* Keep the calibration order stable: integrate, low-tail/full, rest, save, publish. */
	mode = soc_direction(net_current_ma);
	soc_integrate(net_current_ma);
	soc_update_sag_hold(mode, net_current_ma);

	low_tail_active = soc_low_tail_config(mode, net_current_ma, &low_tail_step);

	calibration_applied = soc_apply_full_empty(mode, low_tail_active, &low_tail_step);

	sag_hold_blocked = soc_sag_hold_blocks_calibration();
	if (!low_tail_active && !calibration_applied && !sag_hold_blocked)
	{
		soc_update_rest_timer(mode);
	}
	else if (low_tail_active || sag_hold_blocked)
	{
		soc_reset_rest_confidence();
	}

	soc_save_if_needed();
	soc_publish();
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
	soc_publish();
}
