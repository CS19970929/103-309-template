#include "SocEnhance.h"
#include "conf.h"
#include "EEPROM.h"
#include "DataDeal.h"
#include "Storage.h"
#include "Sci_Upper.h"
#include <string.h>
#include <stdint.h>

#define SOC_TICK_MS                  ((UINT32)200U)
#define SOC_TICKS_PER_SECOND         ((UINT16)5U)
#define SOC_CURRENT_ACTIVE_A10       ((UINT16)2U)
#define SOC_MA_PER_A10               ((int32_t)100)
#define SOC_MAMS_PER_AS10            ((UINT32)100000U)
#define SOC_BOARD_SELF_CONSUMPTION_MA ((UINT16)PROJECT_CFG_SOC_BOARD_SELF_CONSUMPTION_MA)
#define SOC_RESERVE_CAPACITY_AH10    ((UINT16)PROJECT_CFG_SOC_RESERVE_CAPACITY_AH10)
#define SOC_SOH_MIN                  ((UINT8)80U)
#define SOC_SOH_CYCLE_STEP           ((UINT16)100U)
#define SOC_FULL_SECONDS             ((UINT16)PROJECT_CFG_SOC_FULL_CONFIRM_SECONDS)
#define SOC_FULL_CONFIRM_MIN_VMAX_MV ((UINT16)4180U)
#define SOC_FULL_MIN_MARGIN_MV       ((UINT16)PROJECT_CFG_SOC_FULL_CONFIRM_MIN_CELL_MARGIN_MV)
#define SOC_FULL_MAX_DELTA_MV        ((UINT16)PROJECT_CFG_SOC_FULL_CONFIRM_MAX_CELL_DELTA_MV)
#define SOC_SAG_CURRENT_DIVIDER      ((UINT16)2U)
#define SOC_SAG_HOLDOFF_SECONDS      ((UINT16)PROJECT_CFG_SOC_SAG_HOLDOFF_SECONDS)
#define SOC_SAG_ALLOW_OFFSET_MV      ((int16_t)PROJECT_CFG_SOC_SAG_ALLOW_OFFSET_MV)
#define SOC_REST_OCV_SECONDS         ((UINT32)PROJECT_CFG_SOC_REST_OCV_SECONDS)
#define SOC_LONG_REST_DOWN_STEP_SECONDS ((UINT32)PROJECT_CFG_SOC_REST_DOWN_STEP_SECONDS)
#define SOC_CAL_STEP                 ((UINT8)PROJECT_CFG_SOC_CALIBRATION_STEP_PERCENT)
#define SOC_REST_OCV_DEADBAND_PERCENT ((UINT8)3U)
#define SOC_ZERO_CONFIRM_SECONDS     ((UINT16)PROJECT_CFG_SOC_ZERO_CONFIRM_SECONDS)
#define SOC_ZERO_CONVERGE_STEP_SECONDS ((UINT16)PROJECT_CFG_SOC_ZERO_CONVERGE_STEP_SECONDS)
#define SOC_VALID_MIN_MV             ((UINT16)PROJECT_CFG_SOC_CALIBRATION_MIN_CELL_VALID_MV)
#define SOC_VALID_MAX_MV             ((UINT16)PROJECT_CFG_SOC_CALIBRATION_MAX_CELL_VALID_MV)
#define SOC_VALID_MAX_DELTA_MV       ((UINT16)300)
#define SOC_REST_MAX_DELTA_MV        ((UINT16)200U)
#define SOC_REST_STABLE_DELTA_MV     ((UINT16)30U)
#define SOC_RTC_OCV_MAX_CELL_MV      ((UINT16)4000U)
#define SOC_REBOUND_BOOT_HOLDOFF_SECONDS ((UINT32)300U)
#define SOC_SNAPSHOT_FLAG_REBOUND_HOLD   ((UINT16)0x0001U)

/*
 * Flash-life policy:
 * - Persist visible SOC after a 2% delta instead of every 1%.
 * - A cycle counter alone does not force a write until one equivalent cycle.
 * - Rare structural state changes still persist immediately.
 * - Dirty state is eventually committed after 30 minutes and always before
 *   reset-based sleep.
 *
 * This keeps unexpected-power-loss SOC error bounded while cutting the normal
 * full-cycle write count from roughly 300 snapshots toward ~100 snapshots.
 */
#define SOC_FLASH_SAVE_SOC_STEP_PERCENT    ((UINT8)2U)
#define SOC_FLASH_SAVE_CYCLE_STEP_X100     ((UINT32)100U)
#define SOC_FLASH_SAVE_MAX_SECONDS         ((UINT32)1800U)
#define SOC_FLASH_SAVE_MAX_TICKS \
    (SOC_FLASH_SAVE_MAX_SECONDS * (UINT32)SOC_TICKS_PER_SECOND)

typedef enum
{
	SOC_MODE_RELAX = 0,
	SOC_MODE_CHG = 1,
	SOC_MODE_DSG = 2
} SOC_MODE;

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
	UINT16 zero_ocv_ticks;
	UINT16 zero_ocv_step_ticks;
	UINT16 sag_hold_ticks;
	UINT16 rest_ref_vmin;
	UINT16 rest_ref_vmax;
	UINT16 snapshot_flags;
	UINT8 soc;
	UINT8 soh;
	UINT8 rest_ocv_ready;
} SOC_STATE;

typedef struct SOC_SAVE_MARK_TAG
{
	UINT32 cycle_x100;
	UINT32 cap_full_as10;
	UINT32 ticks_since_save;
	UINT16 snapshot_flags;
	UINT8 soc;
	UINT8 valid;
} SOC_SAVE_MARK;

static SOC_STATE s_soc;
static SOC_SAVE_MARK s_saved_soc;
static const UINT8 s_soc_default_startup_percent = 60U;
/* Cumulative RTC rest seconds already applied to SOC in the current sleep session. */
static UINT32 s_u32SocRtcRestAppliedSeconds;

#if PROJECT_CFG_SOC_REST_OCV_ENABLE
static UINT32 soc_seconds_to_ticks(UINT32 seconds);
#endif
static UINT8 soc_sag_hold_blocks_calibration(void);
static UINT16 soc_table_percent(const UINT16 *table, UINT16 voltage_mv);
static void soc_save_current_snapshot(void);

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
	3400, 10, 3300, 5, 3200, 0,
};
#endif

#if PROJECT_CFG_SOC_REST_OCV_ENABLE
static UINT32 soc_seconds_to_ticks(UINT32 seconds)
{
	return seconds * (UINT32)SOC_TICKS_PER_SECOND;
}
#endif

static UINT16 soc_cell_delta(void)
{
	return (UINT16)(g_stCellInfoReport.u16VCellMax - g_stCellInfoReport.u16VCellMin);
}

#if PROJECT_CFG_SOC_REST_OCV_ENABLE
static UINT16 soc_abs_diff_u16(UINT16 a, UINT16 b)
{
	return (a >= b) ? (UINT16)(a - b) : (UINT16)(b - a);
}
#endif

static UINT8 soc_abs_diff_u8(UINT8 a, UINT8 b)
{
	return (a >= b) ? (UINT8)(a - b) : (UINT8)(b - a);
}

static UINT32 soc_abs_diff_u32(UINT32 a, UINT32 b)
{
	return (a >= b) ? (a - b) : (b - a);
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

static UINT32 soc_usable_cap_as10_from(UINT32 cap_full_as10)
{
	UINT32 reserve_as10 = soc_factory_cap_as10_from(SOC_RESERVE_CAPACITY_AH10);

	/* A reserve that consumes the whole pack is invalid at runtime; disable it safely. */
	return (reserve_as10 < cap_full_as10) ?
		(cap_full_as10 - reserve_as10) : cap_full_as10;
}

static UINT32 soc_usable_cap_as10(void)
{
	return soc_usable_cap_as10_from(s_soc.cap_full_as10);
}

static UINT8 soc_soh_from_cycle(UINT32 cycle_x100)
{
	UINT32 drop = (cycle_x100 / 100U) / SOC_SOH_CYCLE_STEP;
	return (drop >= (UINT32)(100U - SOC_SOH_MIN)) ? SOC_SOH_MIN : (UINT8)(100U - drop);
}

static void soc_refresh_capacity_base(void)
{
	UINT32 cap_usable_as10;

	s_soc.soh = soc_soh_from_cycle(s_soc.cycle_x100);
	s_soc.cap_full_as10 = (UINT32)(((uint64_t)s_soc.cap_factory_as10 * s_soc.soh) / 100ULL);
	cap_usable_as10 = soc_usable_cap_as10();
	if (s_soc.cap_now_as10 > cap_usable_as10)
	{
		s_soc.cap_now_as10 = cap_usable_as10;
	}
}

static UINT8 soc_from_cap(void)
{
	UINT32 cap_usable_as10 = soc_usable_cap_as10();
	UINT32 soc;

	if (s_soc.cap_now_as10 >= cap_usable_as10)
	{
		return 100U;
	}
	soc = (UINT32)(((uint64_t)s_soc.cap_now_as10 * 100ULL +
		(cap_usable_as10 / 2U)) / cap_usable_as10);
	return (soc > 100U) ? 100U : (UINT8)soc;
}

static UINT16 soc_cap_to_ah100(UINT32 cap_as10)
{
	UINT32 cap = (cap_as10 + 180U) / 360U;
	return (cap > 0xFFFFU) ? 0xFFFFU : (UINT16)cap;
}

static UINT32 soc_report_cap_now_as10(void)
{
	UINT32 cap_usable_as10 = soc_usable_cap_as10();

	return (UINT32)(((uint64_t)s_soc.cap_now_as10 * s_soc.cap_full_as10 +
		(cap_usable_as10 / 2U)) / cap_usable_as10);
}

static void soc_set(UINT8 soc)
{
	if (soc > 100U)
	{
		soc = 100U;
	}
	s_soc.soc = soc;
	s_soc.cap_now_as10 = (UINT32)(((uint64_t)soc_usable_cap_as10() * soc) / 100ULL);
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

static UINT16 soc_voltage_threshold_mv(int16_t offset_mv)
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

static UINT16 soc_sag_current_limit_a10(void)
{
	UINT16 cap_a10 = OtherElement.u16Soc_Ah;
	UINT16 limit;

	limit = (UINT16)((cap_a10 + SOC_SAG_CURRENT_DIVIDER - 1U) /
		SOC_SAG_CURRENT_DIVIDER);
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
	g_stCellInfoReport.SocElement.u16CapacityNow = soc_cap_to_ah100(soc_report_cap_now_as10());
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
	STORAGE_SOC_DATA data;
	UINT32 cap_usable_as10 = soc_usable_cap_as10();
	UINT32 unit = s_soc.cap_factory_as10 / 100U;

	memset(&data, 0, sizeof(data));
	data.u16FormatVersion = STORAGE_SOC_DATA_VERSION_V2;
	data.u16SocNow = s_soc.soc;
	data.u16MaxErrorPercent = 100U;
	data.u32CycleTimes = s_soc.cycle_x100;
	data.u32CapNow = s_soc.cap_now_as10;
	/* Store the calculation base so snapshots survive reserve-config changes safely. */
	data.u32CapFull = cap_usable_as10;
	data.u32LearnPassedAs10 = s_soc.dsg_acc_as10;
	data.u16Flags = (UINT16)(s_soc.snapshot_flags & SOC_SNAPSHOT_FLAG_REBOUND_HOLD);
	data.u16DsgSocInt = (UINT16)(((uint64_t)s_soc.dsg_acc_as10 * 100ULL) / unit);
	if (data.u16DsgSocInt > 100U)
	{
		data.u16DsgSocInt = 100U;
	}
	return Storage_SaveSocData(&data);
}

static UINT8 soc_save_mark_dirty(void)
{
	if (s_saved_soc.valid == 0U)
	{
		return 1U;
	}
	return (UINT8)((s_soc.soc != s_saved_soc.soc) ||
		(s_soc.cycle_x100 != s_saved_soc.cycle_x100) ||
		(s_soc.cap_full_as10 != s_saved_soc.cap_full_as10) ||
		(s_soc.snapshot_flags != s_saved_soc.snapshot_flags));
}

static void soc_update_save_mark(void)
{
	s_saved_soc.soc = s_soc.soc;
	s_saved_soc.cycle_x100 = s_soc.cycle_x100;
	s_saved_soc.cap_full_as10 = s_soc.cap_full_as10;
	s_saved_soc.snapshot_flags = s_soc.snapshot_flags;
	s_saved_soc.ticks_since_save = 0U;
	s_saved_soc.valid = 1U;
}

static void soc_save_current_snapshot(void)
{
	if (Storage_IsReady() == 0U)
	{
		return;
	}
	if (soc_save())
	{
		soc_update_save_mark();
	}
}

static void soc_save_runtime_if_needed(void)
{
	if (Storage_IsReady() == 0U)
	{
		return;
	}

	if (s_saved_soc.ticks_since_save < SOC_FLASH_SAVE_MAX_TICKS)
	{
		++s_saved_soc.ticks_since_save;
	}

	if (soc_save_mark_dirty() == 0U)
	{
		return;
	}

	if ((s_saved_soc.valid == 0U) ||
		(soc_abs_diff_u8(s_soc.soc, s_saved_soc.soc) >= SOC_FLASH_SAVE_SOC_STEP_PERCENT) ||
		(soc_abs_diff_u32(s_soc.cycle_x100, s_saved_soc.cycle_x100) >= SOC_FLASH_SAVE_CYCLE_STEP_X100) ||
		(s_soc.cap_full_as10 != s_saved_soc.cap_full_as10) ||
		(s_soc.snapshot_flags != s_saved_soc.snapshot_flags) ||
		(s_saved_soc.ticks_since_save >= SOC_FLASH_SAVE_MAX_TICKS))
	{
		soc_save_current_snapshot();
	}
}

static void soc_save_before_sleep(void)
{
	if ((Storage_IsReady() != 0U) && (soc_save_mark_dirty() != 0U))
	{
		soc_save_current_snapshot();
	}
}

static void soc_load_or_default(void)
{
	STORAGE_SOC_DATA data;
	UINT8 valid = Storage_LoadSocData(&data);
	UINT32 cap_usable_as10;
	UINT32 unit = s_soc.cap_factory_as10 / 100U;

	if (valid && (data.u16SocNow <= 100U) && (data.u16DsgSocInt <= 100U))
	{
		s_soc.cycle_x100 = data.u32CycleTimes;
		soc_refresh_capacity_base();
		cap_usable_as10 = soc_usable_cap_as10();
		s_soc.dsg_acc_as10 = (data.u32LearnPassedAs10 != 0U) ?
			(data.u32LearnPassedAs10 % unit) :
			(UINT32)(((uint64_t)unit * data.u16DsgSocInt) / 100ULL);
		if ((data.u32CapFull == cap_usable_as10) &&
			((data.u32CapNow != 0U) || (data.u16SocNow == 0U)) &&
			(data.u32CapNow <= cap_usable_as10))
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
		soc_update_save_mark();
	}
	else
	{
		s_soc.cycle_x100 = (UINT32)OtherElement.u16Soc_Cycle_times * 100U;
		soc_refresh_capacity_base();
		if (soc_calibration_allowed())
		{
			UINT8 startup_soc = soc_ocv_percent();
			/* Zero percent requires the runtime dwell confirmation, not one boot sample. */
			soc_set((startup_soc == 0U) ? 1U : startup_soc);
		}
		else
		{
			soc_set(s_soc_default_startup_percent);
		}

		/* Mark only after a successful write. Failed initialization is retried later. */
		s_saved_soc.valid = 0U;
		soc_save_current_snapshot();
	}
}

static void soc_reset_rest_ocv_step(void)
{
	s_soc.long_rest_down_soc_ticks = 0U;
	s_soc.rest_ocv_ready = 0U;
}

static void soc_integrate(int32_t net_current_ma)
{
	int32_t delta_as10;
	int64_t acc_mams;
	int64_t cap_now_as10;
	UINT32 cap_usable_as10;
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
	cap_usable_as10 = soc_usable_cap_as10();
	cap_now_as10 = (int64_t)s_soc.cap_now_as10 + (int64_t)delta_as10;
	if (cap_now_as10 < 0)
	{
		cap_now_as10 = 0;
	}
	else if (cap_now_as10 > (int64_t)cap_usable_as10)
	{
		cap_now_as10 = (int64_t)cap_usable_as10;
	}
	s_soc.cap_now_as10 = (UINT32)cap_now_as10;
	s_soc.soc = soc_from_cap();
	if ((delta_as10 > 0) && (old_soc < 100U) && (s_soc.soc >= 100U))
	{
		s_soc.soc = 99U;
		s_soc.cap_now_as10 = (UINT32)(((uint64_t)cap_usable_as10 * 99ULL) / 100ULL);
	}
}

static UINT8 soc_full_confirm_allowed(void)
{
	UINT16 full_mv = OtherElement.u16Soc_V_100;
	UINT16 full_min_mv = (full_mv > SOC_FULL_MIN_MARGIN_MV) ?
		(UINT16)(full_mv - SOC_FULL_MIN_MARGIN_MV) : 0U;
	UINT16 delta;

	if (!soc_calibration_allowed() ||
		(g_stCellInfoReport.u16VCellMax < SOC_FULL_CONFIRM_MIN_VMAX_MV) ||
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
		 soc_sag_current_limit_a10()))
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
		 soc_voltage_threshold_mv(SOC_SAG_ALLOW_OFFSET_MV)));
}

static UINT8 soc_zero_ocv_active(SOC_MODE mode)
{
	return (UINT8)((mode != SOC_MODE_CHG) &&
		soc_voltage_valid() &&
		(g_stCellInfoReport.u16VCellMin <=
		 soc_voltage_threshold_mv(0)));
}

static UINT16 soc_zero_confirm_ticks(void)
{
	return (UINT16)((UINT32)SOC_ZERO_CONFIRM_SECONDS *
		(UINT32)SOC_TICKS_PER_SECOND);
}

static UINT8 soc_apply_zero_ocv(SOC_MODE mode, UINT32 delta_soc_ticks)
{
	UINT32 confirm_ticks = (UINT32)soc_zero_confirm_ticks();
	UINT32 step_ticks = (UINT32)SOC_ZERO_CONVERGE_STEP_SECONDS *
		(UINT32)SOC_TICKS_PER_SECOND;
	UINT8 old_soc = s_soc.soc;

	if (!soc_zero_ocv_active(mode))
	{
		s_soc.zero_ocv_ticks = 0U;
		s_soc.zero_ocv_step_ticks = 0U;
		return 0U;
	}

	if ((UINT32)s_soc.zero_ocv_ticks < confirm_ticks)
	{
		UINT32 ticks = (UINT32)s_soc.zero_ocv_ticks + delta_soc_ticks;
		s_soc.zero_ocv_ticks = (UINT16)((ticks >= confirm_ticks) ? confirm_ticks : ticks);
		if ((UINT32)s_soc.zero_ocv_ticks < confirm_ticks)
		{
			return 0U;
		}
		s_soc.zero_ocv_step_ticks = 0U;
		soc_set(soc_step(s_soc.soc, 0U, 1U));
		return (UINT8)(s_soc.soc != old_soc);
	}

	if ((UINT32)s_soc.zero_ocv_step_ticks < step_ticks)
	{
		UINT32 ticks = (UINT32)s_soc.zero_ocv_step_ticks + delta_soc_ticks;
		s_soc.zero_ocv_step_ticks = (UINT16)((ticks >= step_ticks) ? step_ticks : ticks);
	}
	if ((UINT32)s_soc.zero_ocv_step_ticks < step_ticks)
	{
		return 0U;
	}
	s_soc.zero_ocv_step_ticks = 0U;
	soc_set(soc_step(s_soc.soc, 0U, 1U));
	return (UINT8)(s_soc.soc != old_soc);
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

static void soc_reset_rest_confidence(void)
{
	s_soc.rest_soc_ticks = 0U;
	s_soc.stable_rest_soc_ticks = 0U;
	soc_reset_rest_ocv_step();
	s_soc.rest_ref_vmin = 0U;
	s_soc.rest_ref_vmax = 0U;
}

#if PROJECT_CFG_SOC_REST_OCV_ENABLE
static UINT8 soc_apply_continuous_rest_ocv(UINT32 delta_soc_ticks, UINT8 apply_now)
{
	UINT32 step_ticks = soc_seconds_to_ticks(SOC_LONG_REST_DOWN_STEP_SECONDS);
	UINT8 ocv_target;
	UINT8 old_soc = s_soc.soc;

	if (!s_soc.rest_ocv_ready)
	{
		s_soc.long_rest_down_soc_ticks = 0U;
		return 0U;
	}
	if (!apply_now && (s_soc.long_rest_down_soc_ticks < step_ticks))
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
	if (!apply_now && (s_soc.long_rest_down_soc_ticks < step_ticks))
	{
		return 0U;
	}
	s_soc.long_rest_down_soc_ticks = 0U;
	if (!soc_calibration_allowed() || soc_sag_hold_blocks_calibration())
	{
		return 0U;
	}

	/* Re-read the OCV target on every step; only safe downward correction is automatic. */
	ocv_target = soc_ocv_percent();
	if ((s_soc.soc > ocv_target) &&
		((UINT8)(s_soc.soc - ocv_target) > SOC_REST_OCV_DEADBAND_PERCENT))
	{
		soc_set(soc_step(s_soc.soc, ocv_target, SOC_CAL_STEP));
	}
	return (UINT8)(s_soc.soc != old_soc);
}
#endif

#if PROJECT_CFG_SOC_REST_OCV_ENABLE
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
#endif

static void soc_update_rest_timer(SOC_MODE mode)
{
#if !PROJECT_CFG_SOC_REST_OCV_ENABLE
	return;
#else
	UINT32 rest_ocv_ticks = soc_seconds_to_ticks(SOC_REST_OCV_SECONDS);

	if (mode != SOC_MODE_RELAX || g_stCellInfoReport.u16VCellMin >= 3700)
	// if (mode != SOC_MODE_RELAX)
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
		soc_reset_rest_ocv_step();
	}
	if ((s_soc.rest_soc_ticks >= rest_ocv_ticks) &&
		(s_soc.stable_rest_soc_ticks >= rest_ocv_ticks))
	{
		if (!s_soc.rest_ocv_ready)
		{
			s_soc.rest_ocv_ready = 1U;
			(void)soc_apply_continuous_rest_ocv(0U, 1U);
		}
		else
		{
			(void)soc_apply_continuous_rest_ocv(1U, 0U);
		}
	}
#endif
}

#if PROJECT_CFG_SOC_REST_OCV_ENABLE
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
#endif

static UINT8 soc_apply_rtc_rest_ocv(UINT32 rest_seconds)
{
	UINT32 delta_seconds;
#if PROJECT_CFG_SOC_REST_OCV_ENABLE
	UINT32 rest_ocv_ticks = soc_seconds_to_ticks(SOC_REST_OCV_SECONDS);
	UINT8 has_rest_ref;
#endif
	UINT8 changed = 0U;

	if (rest_seconds < s_u32SocRtcRestAppliedSeconds)
	{
		soc_reset_rest_confidence();
		s_soc.zero_ocv_ticks = 0U;
		s_soc.zero_ocv_step_ticks = 0U;
		s_u32SocRtcRestAppliedSeconds = 0U;
	}

	delta_seconds = rest_seconds - s_u32SocRtcRestAppliedSeconds;
	s_u32SocRtcRestAppliedSeconds = rest_seconds;
	if (delta_seconds == 0U)
	{
		(void)soc_apply_zero_ocv(SOC_MODE_RELAX, 0U);
		return 0U;
	}

	changed |= soc_apply_zero_ocv(SOC_MODE_RELAX,
		delta_seconds * (UINT32)SOC_TICKS_PER_SECOND);
	if (soc_zero_ocv_active(SOC_MODE_RELAX))
	{
		soc_reset_rest_confidence();
		return changed;
	}

#if PROJECT_CFG_SOC_REST_OCV_ENABLE
	/* High-voltage RTC time is not valid OCV-rest time; require a fresh dwell below 4.0V. */
	if (g_stCellInfoReport.u16VCellMin >= SOC_RTC_OCV_MAX_CELL_MV)
	{
		soc_reset_rest_confidence();
		return changed;
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
		soc_reset_rest_ocv_step();
		return changed;
	}

	if ((s_soc.rest_soc_ticks >= rest_ocv_ticks) &&
		(s_soc.stable_rest_soc_ticks >= rest_ocv_ticks))
	{
		if (!s_soc.rest_ocv_ready)
		{
			s_soc.rest_ocv_ready = 1U;
			changed |= soc_apply_continuous_rest_ocv(0U, 1U);
		}
		else
		{
			changed |= soc_apply_continuous_rest_ocv(
				soc_seconds_to_ticks(delta_seconds), 0U);
		}
	}
#endif
	return changed;
}

void soc_param_lib_init(void)
{
	memset(&s_soc, 0, sizeof(s_soc));
	memset(&s_saved_soc, 0, sizeof(s_saved_soc));
	s_soc.cap_factory_as10 = soc_factory_cap_as10_from(OtherElement.u16Soc_Ah);
	s_soc.cycle_x100 = (UINT32)OtherElement.u16Soc_Cycle_times * 100U;
	s_u32SocRtcRestAppliedSeconds = 0U;
	soc_refresh_capacity_base();
	soc_load_or_default();
	SOC_PublishReportData();
}

UINT8 SOC_ResetStoredSnapshotToDefault(void)
{
	STORAGE_SOC_DATA data;
	UINT32 cap_factory = soc_factory_cap_as10_from(OtherElement.u16Soc_Ah);
	UINT32 cycle_x100 = (UINT32)OtherElement.u16Soc_Cycle_times * 100U;
	UINT32 cap_full = (UINT32)(((uint64_t)cap_factory * soc_soh_from_cycle(cycle_x100)) / 100ULL);
	UINT32 cap_usable = soc_usable_cap_as10_from(cap_full);

	memset(&data, 0, sizeof(data));
	data.u16FormatVersion = STORAGE_SOC_DATA_VERSION_V2;
	data.u16SocNow = s_soc_default_startup_percent;
	data.u16MaxErrorPercent = 100U;
	data.u32CycleTimes = cycle_x100;
	data.u32CapFull = cap_usable;
	data.u32CapNow = (UINT32)(((uint64_t)cap_usable * s_soc_default_startup_percent) / 100ULL);
	return Storage_SaveSocData(&data);
}

void SOC_SaveSnapshotBeforeSleep(void)
{
	soc_save_before_sleep();
}

void SOC_IntEnhance_Ctrl(int32_t net_current_ma)
{
	UINT8 voltage_calibrated;
	UINT8 zero_ocv_active;
	UINT8 full_confirm_active;
	UINT8 sag_hold_blocked;
	SOC_MODE mode;

	/* Order: integrate, sag hold, voltage calibration, rest, save, publish. */
	mode = soc_direction(net_current_ma);
	soc_integrate(net_current_ma);
	soc_update_sag_hold(mode, net_current_ma);
	zero_ocv_active = soc_zero_ocv_active(mode);
	voltage_calibrated = soc_apply_zero_ocv(mode, 1U);

	full_confirm_active = (UINT8)((mode != SOC_MODE_DSG) && soc_full_confirm_allowed());
	if (soc_apply_full_confirm(full_confirm_active))
	{
		voltage_calibrated = 1U;
	}

	sag_hold_blocked = soc_sag_hold_blocks_calibration();
	if (zero_ocv_active || sag_hold_blocked)
	{
		soc_reset_rest_confidence();
	}
	else if (!voltage_calibrated)
	{
		soc_update_rest_timer(mode);
	}

	soc_save_runtime_if_needed();
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
