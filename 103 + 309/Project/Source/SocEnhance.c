#include "SocEnhance.h"
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
#define SOC_CURRENT_ACTIVE_A10       ((UINT16)2U)
#define SOC_MA_PER_A10               ((int32_t)100)
#define SOC_MAMS_PER_AS10            ((UINT32)100000U)
#define SOC_BOARD_SELF_CONSUMPTION_MA ((UINT16)PROJECT_CFG_SOC_BOARD_SELF_CONSUMPTION_MA)
#define SOC_DEFAULT_CAP_A10          ((UINT16)270U)
#define SOC_SOH_MIN                  ((UINT8)80U)
#define SOC_SOH_CYCLE_STEP           ((UINT16)100U)
#define SOC_FULL_SECONDS             ((UINT16)PROJECT_CFG_SOC_FULL_CONFIRM_SECONDS)
#define SOC_FULL_FAST_SECONDS        ((UINT16)PROJECT_CFG_SOC_FULL_CONFIRM_FAST_SECONDS)
#define SOC_FULL_MIN_SOC             ((UINT8)PROJECT_CFG_SOC_FULL_CONFIRM_MIN_SOC_PERCENT)
#define SOC_DEFAULT_FULL_MV          ((UINT16)4180U)
#define SOC_FULL_CONFIRM_MIN_VMAX_MV SOC_DEFAULT_FULL_MV
#define SOC_FULL_FAST_MARGIN_MV      ((UINT16)PROJECT_CFG_SOC_FULL_CONFIRM_FAST_MARGIN_MV)
#define SOC_FULL_MIN_MARGIN_MV       ((UINT16)PROJECT_CFG_SOC_FULL_CONFIRM_MIN_CELL_MARGIN_MV)
#define SOC_FULL_MAX_DELTA_MV        ((UINT16)PROJECT_CFG_SOC_FULL_CONFIRM_MAX_CELL_DELTA_MV)
#define SOC_EMPTY_MV                 ((UINT16)3000U)
#define SOC_EMPTY_CUR_LIGHT_DIVIDER  ((UINT16)5U)
#define SOC_EMPTY_CUR_MID_DIVIDER    ((UINT16)2U)
#define SOC_SAG_HOLDOFF_SECONDS      ((UINT16)PROJECT_CFG_SOC_SAG_HOLDOFF_SECONDS)
#define SOC_SAG_ALLOW_OFFSET_MV      ((int16_t)PROJECT_CFG_SOC_SAG_ALLOW_OFFSET_MV)
#define SOC_REST_OCV_SECONDS         ((UINT32)PROJECT_CFG_SOC_REST_OCV_SECONDS)
#define SOC_SHORT_REST_MIN_SECONDS   ((UINT32)PROJECT_CFG_SOC_REST_STABLE_MIN_SECONDS)
#define SOC_SHORT_REST_STEP_SECONDS  ((UINT32)PROJECT_CFG_SOC_REST_TARGET_STEP_SECONDS)
#define SOC_LONG_REST_DOWN_STEP_SECONDS ((UINT32)PROJECT_CFG_SOC_REST_DOWN_STEP_SECONDS)
#define SOC_CAL_STEP                 ((UINT8)PROJECT_CFG_SOC_CALIBRATION_STEP_PERCENT)
#define SOC_EMPTY_TAIL_START_OFFSET_MV ((UINT16)PROJECT_CFG_SOC_EMPTY_TAIL_START_OFFSET_MV)
#define SOC_EMPTY_TAIL_SOFT_TARGET_LIFT_PERCENT ((UINT8)PROJECT_CFG_SOC_EMPTY_TAIL_SOFT_TARGET_LIFT_PERCENT)
#define SOC_EMPTY_TAIL_SOFT_TICK_SCALE_PERCENT ((UINT16)PROJECT_CFG_SOC_EMPTY_TAIL_SOFT_TICK_SCALE_PERCENT)
#define SOC_DISPLAY_NORMAL_SECONDS   ((UINT8)PROJECT_CFG_SOC_DISPLAY_NORMAL_SECONDS)
#define SOC_DISPLAY_CHG_SECONDS      ((UINT8)PROJECT_CFG_SOC_DISPLAY_CHG_SECONDS)
#define SOC_DISPLAY_LOW_SECONDS      ((UINT8)PROJECT_CFG_SOC_DISPLAY_LOW_SECONDS)
#define SOC_DISPLAY_LOW_OFFSET_MV    ((int16_t)PROJECT_CFG_SOC_DISPLAY_LOW_OFFSET_MV)
#define SOC_DISPLAY_EMPTY_FAST_BELOW_V0_MV ((int16_t)PROJECT_CFG_SOC_DISPLAY_EMPTY_FAST_BELOW_V0_MV)
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
#define SOC_MID_TARGET_DISABLED      ((UINT8)0xFFU)
#define SOC_MID_MAX_DELTA_MV         ((UINT16)200U)
#define SOC_REST_STABLE_DELTA_MV     ((UINT16)30U)
#define SOC_REBOUND_BOOT_HOLDOFF_SECONDS ((UINT32)300U)
#define SOC_SNAPSHOT_FLAG_REBOUND_HOLD   ((UINT16)0x0001U)

/* Compile-time tick equivalents of time-based config */
#define SOC_SEC_TO_TICKS(sec)               ((UINT32)(sec) * (UINT32)SOC_TICKS_PER_SECOND)
#define SOC_REST_STABLE_LIMIT_SECS          ((SOC_REST_OCV_SECONDS) < (SOC_SHORT_REST_MIN_SECONDS) ? \
                                             (SOC_SHORT_REST_MIN_SECONDS) : (SOC_REST_OCV_SECONDS))
#define SOC_REST_LIMIT_TICKS                SOC_SEC_TO_TICKS(SOC_REST_OCV_SECONDS)
#define SOC_STABLE_LIMIT_TICKS              SOC_SEC_TO_TICKS(SOC_REST_STABLE_LIMIT_SECS)
#define SOC_SHORT_MIN_TICKS                 SOC_SEC_TO_TICKS(SOC_SHORT_REST_MIN_SECONDS)
#define SOC_SHORT_STEP_TICKS                SOC_SEC_TO_TICKS(SOC_SHORT_REST_STEP_SECONDS)
#define SOC_LONG_REST_DOWN_STEP_TICKS       SOC_SEC_TO_TICKS(SOC_LONG_REST_DOWN_STEP_SECONDS)

typedef struct
{
	UINT32 cap_factory_as10;
	UINT32 cap_full_as10;
	UINT32 cap_now_as10;
	UINT32 cycle_x100;
	UINT32 dsg_acc_as10;
	UINT32 rem_mams;
	UINT32 rest_ticks;
	UINT32 stable_rest_ticks;
	UINT32 short_rest_ticks;
	UINT32 long_rest_down_ticks;
	UINT16 full_ticks;
	UINT16 empty_ticks;
	UINT16 mid_ticks;
	UINT16 display_ticks;
	UINT16 sag_hold_ticks;
	UINT16 deferred_ocv_ticks;
	UINT16 rest_ref_vmin;
	UINT16 rest_ref_vmax;
	UINT16 snapshot_flags;
	UINT8 soc;
	UINT8 soh;
	UINT8 display_soc;
	UINT8 deferred_ocv_target;
	UINT8 deferred_ocv_valid;
	UINT8 mode;
#if PROJECT_CFG_DEBUG_WATCH_ENABLE || PROJECT_CFG_DEBUG_MONITOR_ENABLE
	UINT8 last_mode;
#endif
	UINT8 integrate_mode;
	UINT8 display_ready;
	UINT8 full_anchor;
} SOC_STATE;

typedef struct
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

struct SOC_ENHANCE_ELEMENT SOC_Enhance_Element;

static SOC_STATE s_soc;
static SOC_STATE s_saved_soc;
static UINT32 s_u32RtcRestCursorSeconds;

#if PROJECT_CFG_DEBUG_WATCH_ENABLE
#if defined(__GNUC__) || defined(__CC_ARM)
#define SOC_DEBUG_USED __attribute__((used))
#else
#define SOC_DEBUG_USED
#endif
static struct SOC_DEBUG_WATCH s_soc_debug_watch;
struct SOC_DEBUG_WATCH * const g_dbg_soc_watch SOC_DEBUG_USED = &s_soc_debug_watch;
static UINT8 s_soc_watch_rest_voltage_stable;
static void soc_watch_set_block_reason(UINT8 reason);
static void soc_watch_set_calib_source(UINT8 source, UINT8 before, UINT8 after);
static void soc_watch_set_tail_state(UINT8 low_active, const SOC_TAIL_STEP *low_step,
									  UINT8 mid_active, const SOC_TAIL_STEP *mid_step);
static void soc_watch_set_rest_voltage_stable(UINT8 stable);
static void soc_watch_refresh(UINT8 force_display);
#else
#define soc_watch_set_block_reason(reason) ((void)(reason))
#define soc_watch_set_calib_source(source, before, after) \
	((void)(source), (void)(before), (void)(after))
#define soc_watch_set_tail_state(low_active, low_step, mid_active, mid_step) \
	((void)(low_active), (void)(low_step), (void)(mid_active), (void)(mid_step))
#define soc_watch_set_rest_voltage_stable(stable) ((void)(stable))
#define soc_watch_refresh(force_display) ((void)(force_display))
#endif

static UINT8 soc_sag_hold_blocks_calibration(void);
static UINT8 soc_apply_ocv_target_step(UINT8 target, UINT8 mode);
static UINT16 soc_table_percent(const UINT16 *table, UINT16 size, UINT16 voltage_mv);

/* offset_mv is relative to V0; target limits SOC, ticks are 200ms SOC ticks per 1%. */
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

/* Mid-tail table limits high SOC above V0; SOC_MID_TARGET_DISABLED skips a load band. */
static const SOC_EMPTY_TAIL_RULE s_mid_tail_table[] = {
	{500, {25U, 32U, 42U, SOC_MID_TARGET_DISABLED}, {450U, 450U, 600U, 0U}},
	{600, {35U, 42U, 50U, SOC_MID_TARGET_DISABLED}, {600U, 600U, 750U, 0U}},
	{650, {45U, 50U, 58U, SOC_MID_TARGET_DISABLED}, {750U, 750U, 900U, 0U}},
	{700, {55U, 60U, SOC_MID_TARGET_DISABLED, SOC_MID_TARGET_DISABLED}, {900U, 900U, 0U, 0U}},
};

#if PROJECT_CFG_SOC_RUNTIME_TABLE_ENABLE || (PROJECT_CFG_BAT_CHEMISTRY == 1)
const UINT16 SOC_Table_LiFePO[SOC_Size_LiFePO] = {
	3336, 100, 3332, 90, 3330, 80, 3327, 75, 3316, 70, 3301, 65,
	3294, 60, 3291, 55, 3290, 50, 3288, 45, 3286, 40, 3279, 35,
	3266, 30, 3254, 25, 3236, 20, 3212, 15, 3198, 10, 3112, 5,
	2526, 0, 1000, 0, 1000, 0,
};
#endif

#if PROJECT_CFG_SOC_RUNTIME_TABLE_ENABLE || (PROJECT_CFG_BAT_CHEMISTRY == 0)
const UINT16 SocTable_TernaryLi[SOC_Size_TernaryLi] = {
	4160, 100, 4100, 95, 4050, 90, 3995, 85, 3935, 80, 3880, 75,
	3835, 70, 3795, 65, 3760, 60, 3725, 55, 3695, 50, 3670, 45,
	3645, 40, 3615, 35, 3585, 30, 3555, 25, 3525, 20, 3480, 15,
	3400, 10, 3250, 5, 3000, 0,
};
#endif

#if PROJECT_CFG_SOC_RUNTIME_TABLE_ENABLE
const UINT16 SocTable_LiFePO2[SOC_Size_LiFePO2] = {
	3650, 100, 3600, 98, 3550, 95, 3500, 92, 3400, 90, 3350, 87,
	3340, 85, 3335, 82, 3330, 80, 3325, 78, 3320, 75, 3300, 70,
	3275, 65, 3250, 60, 3200, 50, 3150, 45, 3100, 30, 3000, 20,
	2850, 10, 2750, 5, 2650, 0,
};
#endif

static UINT16 soc_cell_delta(void)
{
	return (SOC_Enhance_Element.u16_VCellMax >= SOC_Enhance_Element.u16_VCellMin) ?
		(UINT16)(SOC_Enhance_Element.u16_VCellMax - SOC_Enhance_Element.u16_VCellMin) :
		(UINT16)(SOC_Enhance_Element.u16_VCellMin - SOC_Enhance_Element.u16_VCellMax);
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

static int32_t soc_integrate_current_ma(UINT8 mode)
{
	int32_t board_ma = (int32_t)SOC_BOARD_SELF_CONSUMPTION_MA;

	if (mode == SOC_MODE_CHG)
	{
		return ((int32_t)SOC_Enhance_Element.u16_Ichg * SOC_MA_PER_A10) - board_ma;
	}
	if (mode == SOC_MODE_DSG)
	{
		return 0 - (((int32_t)SOC_Enhance_Element.u16_Idsg * SOC_MA_PER_A10) + board_ma);
	}
	return 0 - board_ma;
}

static UINT8 soc_integrate_mode_from_current(int32_t current_ma)
{
	if (current_ma > 0)
	{
		return SOC_MODE_CHG;
	}
	if (current_ma < 0)
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

#if PROJECT_CFG_SOC_CALIBRATION_BLOCK_PROTECTION_FAULT
static UINT8 soc_protection_fault_blocks_calibration(void)
{
	return (UINT8)(g_stCellInfoReport.unMdlFault_Third.all != 0U);
}
#else
#define soc_protection_fault_blocks_calibration() ((UINT8)0U)
#endif

#if PROJECT_CFG_SOC_CALIBRATION_BLOCK_SYSTEM_FAULT
static UINT8 soc_system_fault_blocks_calibration(void)
{
	return (UINT8)((System_ERROR_UserCallback(ERROR_STATUS_AFE1) != 0U) ||
		(System_ERROR_UserCallback(ERROR_STATUS_AFE2) != 0U) ||
		(System_ERROR_UserCallback(ERROR_STATUS_ADC) != 0U) ||
		(System_ERROR_UserCallback(ERROR_STATUS_CBC_CHG) != 0U) ||
		(System_ERROR_UserCallback(ERROR_STATUS_CBC_DSG) != 0U) ||
		(System_ERROR_UserCallback(ERROR_STATUS_TEMP_BREAK) != 0U));
}
#else
#define soc_system_fault_blocks_calibration() ((UINT8)0U)
#endif

static UINT8 soc_calibration_allowed(void)
{
	if (!soc_voltage_valid())
	{
		soc_watch_set_block_reason(SOC_WATCH_BLOCK_VOLTAGE_INVALID);
		return 0U;
	}
	if (soc_cell_delta() > SOC_VALID_MAX_DELTA_MV)
	{
		soc_watch_set_block_reason(SOC_WATCH_BLOCK_CELL_DELTA);
		return 0U;
	}
	if (soc_protection_fault_blocks_calibration())
	{
		soc_watch_set_block_reason(SOC_WATCH_BLOCK_PROTECTION_FAULT);
		return 0U;
	}
	if (soc_system_fault_blocks_calibration())
	{
		soc_watch_set_block_reason(SOC_WATCH_BLOCK_SYSTEM_FAULT);
		return 0U;
	}
	soc_watch_set_block_reason(SOC_WATCH_BLOCK_NONE);
	return 1U;
}

static const UINT16 *soc_ocv_table(UINT16 *size)
{
#if !PROJECT_CFG_SOC_RUNTIME_TABLE_ENABLE
	(void)SOC_Enhance_Element.u16_SOC_TableSelect;
#if (PROJECT_CFG_BAT_CHEMISTRY == 1)
	*size = SOC_Size_LiFePO;
	return SOC_Table_LiFePO;
#else
	*size = SOC_Size_TernaryLi;
	return SocTable_TernaryLi;
#endif
#else
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
#endif
}

static UINT8 soc_ocv_percent(void)
{
	UINT16 size;
	const UINT16 *table = soc_ocv_table(&size);
	UINT16 soc = soc_table_percent(table, size, SOC_Enhance_Element.u16_VCellMin);
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

static UINT8 soc_vmin_above_empty_offset(UINT16 offset_mv)
{
	UINT16 empty_mv = soc_empty_mv();
	UINT16 threshold = (empty_mv > (UINT16)(SOC_VALID_MAX_MV - offset_mv)) ?
		SOC_VALID_MAX_MV : (UINT16)(empty_mv + offset_mv);

	return (UINT8)(SOC_Enhance_Element.u16_VCellMin > threshold);
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

static void soc_save_current_snapshot(void)
{
	if (soc_save())
	{
		s_saved_soc = s_soc;
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
		soc_watch_set_calib_source(SOC_WATCH_CALIB_STARTUP_SNAPSHOT,
			s_soc.soc,
			s_soc.soc);
	}
	else
	{
		s_soc.cycle_x100 = (UINT32)SOC_Enhance_Element.u16_SOC_CycleT_Ever * 100U;
		soc_refresh_capacity_base();
		if (soc_calibration_allowed())
		{
			soc_set(soc_ocv_percent());
			soc_watch_set_calib_source(SOC_WATCH_CALIB_STARTUP_OCV,
				SOC_DEFAULT_STARTUP_PERCENT,
				s_soc.soc);
		}
		else
		{
			soc_set(SOC_DEFAULT_STARTUP_PERCENT);
			soc_watch_set_calib_source(SOC_WATCH_CALIB_STARTUP_DEFAULT,
				SOC_DEFAULT_STARTUP_PERCENT,
				s_soc.soc);
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

static UINT8 soc_apply_discharge_delta(UINT32 delta_as10, UINT8 watch_source, UINT8 old_soc)
{
	s_soc.full_anchor = 0U;
	soc_add_discharge(delta_as10);
	s_soc.cap_now_as10 = (s_soc.cap_now_as10 > delta_as10) ?
		(s_soc.cap_now_as10 - delta_as10) : 0U;
	s_soc.soc = soc_from_cap();
	if (s_soc.soc != old_soc)
	{
		soc_watch_set_calib_source(watch_source, old_soc, s_soc.soc);
		return 1U;
	}
	return 0U;
}

static void soc_clear_deferred_ocv(void)
{
	s_soc.deferred_ocv_valid = 0U;
	s_soc.deferred_ocv_target = 0U;
	s_soc.deferred_ocv_ticks = 0U;
	s_soc.long_rest_down_ticks = 0U;
}

static void soc_set_deferred_ocv_target(UINT8 target)
{
	if (target >= s_soc.soc)
	{
		soc_clear_deferred_ocv();
		return;
	}
	if ((!s_soc.deferred_ocv_valid) || (s_soc.deferred_ocv_target != target))
	{
		s_soc.deferred_ocv_target = target;
		s_soc.deferred_ocv_valid = 1U;
		s_soc.deferred_ocv_ticks = 0U;
		s_soc.long_rest_down_ticks = 0U;
	}
}

static UINT8 soc_latch_rest_ocv_target(void)
{
	UINT8 before = s_soc.soc;

	if (!soc_calibration_allowed() || soc_sag_hold_blocks_calibration())
	{
		return 0U;
	}
	soc_set_deferred_ocv_target(soc_ocv_percent());
	if (s_soc.deferred_ocv_valid)
	{
		soc_watch_set_calib_source(SOC_WATCH_CALIB_REST_TARGET, before, s_soc.soc);
	}
	return s_soc.deferred_ocv_valid;
}

static void soc_integrate(UINT8 mode)
{
	int32_t current_ma_signed = soc_integrate_current_ma(mode);
	UINT8 integrate_mode = soc_integrate_mode_from_current(current_ma_signed);
	UINT32 current_ma;
	UINT32 acc_mams;
	UINT32 delta_as10;
	UINT8 old_soc = s_soc.soc;

	if (integrate_mode != s_soc.integrate_mode)
	{
		s_soc.rem_mams = 0U;
		s_soc.integrate_mode = integrate_mode;
	}
#if PROJECT_CFG_DEBUG_WATCH_ENABLE || PROJECT_CFG_DEBUG_MONITOR_ENABLE
	s_soc.last_mode = mode;
#endif
	if (integrate_mode == SOC_MODE_RELAX)
	{
		s_soc.rem_mams = 0U;
		return;
	}
	current_ma = (current_ma_signed > 0) ?
		(UINT32)current_ma_signed : (UINT32)(0 - current_ma_signed);
	acc_mams = (current_ma * SOC_TICK_MS) + s_soc.rem_mams;
	delta_as10 = acc_mams / SOC_MAMS_PER_AS10;
	s_soc.rem_mams = acc_mams % SOC_MAMS_PER_AS10;
	if (delta_as10 == 0U)
	{
		return;
	}
	if (integrate_mode == SOC_MODE_CHG)
	{
		s_soc.cap_now_as10 = ((s_soc.cap_full_as10 - s_soc.cap_now_as10) < delta_as10) ?
			s_soc.cap_full_as10 : (s_soc.cap_now_as10 + delta_as10);
	}
	else
	{
		(void)soc_apply_discharge_delta(delta_as10,
			(mode == SOC_MODE_RELAX) ? SOC_WATCH_CALIB_BOARD_SELF_CONSUMPTION :
				SOC_WATCH_CALIB_INTEGRATE_DSG,
			old_soc);
		return;
	}
	s_soc.soc = soc_from_cap();
	if ((integrate_mode == SOC_MODE_CHG) && (!s_soc.full_anchor) && (s_soc.soc >= 100U))
	{
		s_soc.soc = 99U;
		s_soc.cap_now_as10 = (UINT32)(((uint64_t)s_soc.cap_full_as10 * 99ULL) / 100ULL);
	}
	if (s_soc.soc != old_soc)
	{
		soc_watch_set_calib_source((integrate_mode == SOC_MODE_CHG) ?
			SOC_WATCH_CALIB_INTEGRATE_CHG :
			((mode == SOC_MODE_RELAX) ? SOC_WATCH_CALIB_BOARD_SELF_CONSUMPTION :
			 SOC_WATCH_CALIB_INTEGRATE_DSG),
			old_soc,
			s_soc.soc);
	}
}

static UINT8 soc_apply_board_self_consumption_seconds(UINT32 seconds)
{
	uint64_t acc_mams;
	uint64_t delta_as10_64;
	UINT32 delta_as10;
	UINT8 old_soc = s_soc.soc;

	if ((SOC_BOARD_SELF_CONSUMPTION_MA == 0U) || (seconds == 0U))
	{
		return 0U;
	}

	if (s_soc.integrate_mode != SOC_MODE_DSG)
	{
		s_soc.rem_mams = 0U;
		s_soc.integrate_mode = SOC_MODE_DSG;
	}
#if PROJECT_CFG_DEBUG_WATCH_ENABLE || PROJECT_CFG_DEBUG_MONITOR_ENABLE
	s_soc.last_mode = SOC_MODE_RELAX;
#endif
	acc_mams = ((uint64_t)SOC_BOARD_SELF_CONSUMPTION_MA * (uint64_t)seconds * 1000ULL) +
		(uint64_t)s_soc.rem_mams;
	delta_as10_64 = acc_mams / (uint64_t)SOC_MAMS_PER_AS10;
	s_soc.rem_mams = (UINT32)(acc_mams % (uint64_t)SOC_MAMS_PER_AS10);
	if (delta_as10_64 == 0ULL)
	{
		return 0U;
	}
	delta_as10 = (delta_as10_64 > 0xFFFFFFFFULL) ? 0xFFFFFFFFU : (UINT32)delta_as10_64;
	return soc_apply_discharge_delta(delta_as10, SOC_WATCH_CALIB_BOARD_SELF_CONSUMPTION, old_soc);
}

static UINT8 soc_apply_rest_ocv(UINT32 rest_seconds, UINT8 mode)
{
	UINT8 target;

	if ((rest_seconds < SOC_SHORT_REST_MIN_SECONDS) || !soc_calibration_allowed())
	{
		return 0U;
	}
	target = soc_ocv_percent();
	if (target >= s_soc.soc)
	{
		return 0U;
	}
	return soc_apply_ocv_target_step(target, mode);
}

static UINT8 soc_apply_ocv_target_step(UINT8 target, UINT8 mode)
{
	UINT8 old_soc = s_soc.soc;

	if (!soc_calibration_allowed() || soc_sag_hold_blocks_calibration())
	{
		return 0U;
	}
	if (((mode == SOC_MODE_CHG) && (target <= s_soc.soc)) ||
		((mode == SOC_MODE_DSG) && (target >= s_soc.soc)))
	{
		soc_watch_set_block_reason(SOC_WATCH_BLOCK_DIRECTION);
		return 0U;
	}
	soc_set(soc_step(s_soc.soc, target, SOC_CAL_STEP));
	return (UINT8)(s_soc.soc != old_soc);
}

static UINT8 soc_apply_deferred_ocv_step(UINT8 mode)
{
	UINT8 changed;
	UINT32 active_step_ticks;
	UINT8 old_soc = s_soc.soc;

	if ((!s_soc.deferred_ocv_valid) || (mode == SOC_MODE_RELAX))
	{
		return 0U;
	}
	if (s_soc.deferred_ocv_target == s_soc.soc)
	{
		soc_clear_deferred_ocv();
		return 0U;
	}
	if ((s_soc.deferred_ocv_target > s_soc.soc) ||
		((s_soc.deferred_ocv_target < s_soc.soc) && (mode != SOC_MODE_DSG)))
	{
		soc_clear_deferred_ocv();
		return 0U;
	}
	active_step_ticks = SOC_SHORT_STEP_TICKS;
	if (s_soc.deferred_ocv_ticks < active_step_ticks)
	{
		++s_soc.deferred_ocv_ticks;
	}
	if (s_soc.deferred_ocv_ticks < active_step_ticks)
	{
		return 0U;
	}
	changed = soc_apply_ocv_target_step(s_soc.deferred_ocv_target, mode);
	s_soc.deferred_ocv_ticks = 0U;
	if (changed)
	{
		soc_watch_set_calib_source(SOC_WATCH_CALIB_DEFERRED_OCV, old_soc, s_soc.soc);
	}
	if (s_soc.deferred_ocv_target == s_soc.soc)
	{
		soc_clear_deferred_ocv();
	}
	return changed;
}

static UINT16 soc_full_confirm_seconds(void)
{
	UINT16 full_mv = soc_full_mv();
	UINT16 vmax_min = soc_voltage_with_margin(full_mv, SOC_FULL_MIN_MARGIN_MV);
	UINT16 vmin_min = vmax_min;
	UINT16 vmin_fast = soc_voltage_with_margin(full_mv, SOC_FULL_FAST_MARGIN_MV);
	UINT16 delta;

	if (!soc_calibration_allowed() ||
		(SOC_Enhance_Element.u16_VCellMax <= SOC_FULL_CONFIRM_MIN_VMAX_MV) ||
		(SOC_Enhance_Element.u16_VCellMax < vmax_min))
	{
		return 0U;
	}

	delta = soc_cell_delta();
	if ((SOC_Enhance_Element.u16_VCellMin >= vmin_fast) &&
		(delta <= SOC_FULL_MAX_DELTA_MV))
	{
		return SOC_FULL_FAST_SECONDS;
	}
	if ((s_soc.soc >= SOC_FULL_MIN_SOC) &&
		(SOC_Enhance_Element.u16_VCellMin >= vmin_min) &&
		(delta <= SOC_FULL_MAX_DELTA_MV))
	{
		return SOC_FULL_SECONDS;
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
	UINT8 blocked = (UINT8)((s_soc.sag_hold_ticks > 0U) &&
		soc_voltage_valid() &&
		(SOC_Enhance_Element.u16_VCellMin >
		 soc_empty_threshold_mv(SOC_SAG_ALLOW_OFFSET_MV)));

	if (blocked)
	{
		soc_watch_set_block_reason(SOC_WATCH_BLOCK_SAG_HOLD);
	}
	return blocked;
}

#if PROJECT_CFG_DEBUG_WATCH_ENABLE
static void soc_watch_set_block_reason(UINT8 reason)
{
	s_soc_debug_watch.u8LastBlockReason = reason;
}

static void soc_watch_set_calib_source(UINT8 source, UINT8 before, UINT8 after)
{
	s_soc_debug_watch.u8LastCalibSource = source;
	s_soc_debug_watch.u8LastSocBefore = before;
	s_soc_debug_watch.u8LastSocAfter = after;
	if (source != SOC_WATCH_CALIB_NONE)
	{
		s_soc_debug_watch.u8LastBlockReason = SOC_WATCH_BLOCK_NONE;
	}
}

static void soc_watch_set_tail_state(UINT8 low_active, const SOC_TAIL_STEP *low_step,
									  UINT8 mid_active, const SOC_TAIL_STEP *mid_step)
{
	s_soc_debug_watch.u8LowTailActive = low_active;
	s_soc_debug_watch.u8MidTailActive = mid_active;
	s_soc_debug_watch.u16EmptyTailTarget = (low_active && (low_step != 0)) ? low_step->target : 0U;
	s_soc_debug_watch.u16EmptyTailTicks = (low_active && (low_step != 0)) ? low_step->ticks : 0U;
	s_soc_debug_watch.u16MidTailTarget = (mid_active && (mid_step != 0)) ? mid_step->target : 0U;
	s_soc_debug_watch.u16MidTailTicks = (mid_active && (mid_step != 0)) ? mid_step->ticks : 0U;
}

static void soc_watch_set_rest_voltage_stable(UINT8 stable)
{
	s_soc_watch_rest_voltage_stable = stable;
}

static void soc_watch_refresh(UINT8 force_display)
{
	UINT8 cal_allowed = 0U;
	UINT8 sag_blocked;

	if (soc_voltage_valid() &&
		(soc_cell_delta() <= SOC_VALID_MAX_DELTA_MV) &&
		(!soc_protection_fault_blocks_calibration()) &&
		(!soc_system_fault_blocks_calibration()))
	{
		cal_allowed = 1U;
	}
	sag_blocked = (UINT8)((s_soc.sag_hold_ticks > 0U) &&
		soc_voltage_valid() &&
		(SOC_Enhance_Element.u16_VCellMin >
		 soc_empty_threshold_mv(SOC_SAG_ALLOW_OFFSET_MV)));

	s_soc_debug_watch.u32CapFactoryAs10 = s_soc.cap_factory_as10;
	s_soc_debug_watch.u32CapFullAs10 = s_soc.cap_full_as10;
	s_soc_debug_watch.u32CapNowAs10 = s_soc.cap_now_as10;
	s_soc_debug_watch.u32CycleX100 = s_soc.cycle_x100;
	s_soc_debug_watch.u32DsgAccAs10 = s_soc.dsg_acc_as10;
	s_soc_debug_watch.u32RestTicks = s_soc.rest_ticks;
	s_soc_debug_watch.u32StableRestTicks = s_soc.stable_rest_ticks;
	s_soc_debug_watch.u32ShortRestTicks = s_soc.short_rest_ticks;
	s_soc_debug_watch.u32LongRestDownTicks = s_soc.long_rest_down_ticks;
	s_soc_debug_watch.u16VCellMax = SOC_Enhance_Element.u16_VCellMax;
	s_soc_debug_watch.u16VCellMin = SOC_Enhance_Element.u16_VCellMin;
	s_soc_debug_watch.u16CellDelta = soc_cell_delta();
	s_soc_debug_watch.u16Ichg = SOC_Enhance_Element.u16_Ichg;
	s_soc_debug_watch.u16Idsg = SOC_Enhance_Element.u16_Idsg;
	s_soc_debug_watch.u16FullTicks = s_soc.full_ticks;
	s_soc_debug_watch.u16EmptyTicks = s_soc.empty_ticks;
	s_soc_debug_watch.u16MidTicks = s_soc.mid_ticks;
	s_soc_debug_watch.u16DisplayTicks = s_soc.display_ticks;
	s_soc_debug_watch.u16SagHoldTicks = s_soc.sag_hold_ticks;
	s_soc_debug_watch.u16DeferredOcvTicks = s_soc.deferred_ocv_ticks;
	s_soc_debug_watch.u16RestRefVmin = s_soc.rest_ref_vmin;
	s_soc_debug_watch.u16RestRefVmax = s_soc.rest_ref_vmax;
	s_soc_debug_watch.u16SnapshotFlags = s_soc.snapshot_flags;
	s_soc_debug_watch.u8Mode = s_soc.mode;
	s_soc_debug_watch.u8LastMode = s_soc.last_mode;
	s_soc_debug_watch.u8InternalSoc = s_soc.soc;
	s_soc_debug_watch.u8DisplaySoc = s_soc.display_soc;
	s_soc_debug_watch.u8Soh = s_soc.soh;
	s_soc_debug_watch.u8DeferredOcvValid = s_soc.deferred_ocv_valid;
	s_soc_debug_watch.u8DeferredOcvTarget = s_soc.deferred_ocv_target;
	s_soc_debug_watch.u8FullAnchor = s_soc.full_anchor;
	s_soc_debug_watch.u8CalibrationAllowed = cal_allowed;
	s_soc_debug_watch.u8SagHoldBlocksCalibration = sag_blocked;
	s_soc_debug_watch.u8RestVoltageStable = s_soc_watch_rest_voltage_stable;
	s_soc_debug_watch.u8LastPublishForce = force_display;
}
#endif

static UINT8 soc_tail_rule_lookup(const SOC_EMPTY_TAIL_RULE *rules,
								  UINT16 count,
								  UINT8 band,
								  UINT8 disabled_target,
								  UINT8 zero_ticks_to_one,
								  SOC_TAIL_STEP *step,
								  int16_t *matched_offset)
{
	UINT16 i;
	UINT16 threshold;

	for (i = 0U; i < count; ++i)
	{
		threshold = soc_empty_threshold_mv(rules[i].offset_mv);
		if (SOC_Enhance_Element.u16_VCellMin <= threshold)
		{
			if (rules[i].target[band] == disabled_target)
			{
				return 0U;
			}
			step->target = rules[i].target[band];
			step->ticks = rules[i].ticks[band];
			if (step->ticks == 0U)
			{
				if (!zero_ticks_to_one)
				{
					return 0U;
				}
				step->ticks = 1U;
			}
			if (matched_offset != 0)
			{
				*matched_offset = rules[i].offset_mv;
			}
			return 1U;
		}
	}
	return 0U;
}

#if (PROJECT_CFG_SOC_EMPTY_TAIL_SOFT_TARGET_LIFT_PERCENT != 0) || \
	(PROJECT_CFG_SOC_EMPTY_TAIL_SOFT_TICK_SCALE_PERCENT != 100)
static void soc_apply_empty_tail_tuning(int16_t offset_mv, SOC_TAIL_STEP *step)
{
	UINT32 ticks;
	UINT16 target;

	if (offset_mv <= 0)
	{
		return;
	}
	if (SOC_EMPTY_TAIL_SOFT_TARGET_LIFT_PERCENT != 0U)
	{
		target = (UINT16)step->target + SOC_EMPTY_TAIL_SOFT_TARGET_LIFT_PERCENT;
		step->target = (target > 100U) ? 100U : (UINT8)target;
	}
	if (SOC_EMPTY_TAIL_SOFT_TICK_SCALE_PERCENT != 100U)
	{
		ticks = ((UINT32)step->ticks * SOC_EMPTY_TAIL_SOFT_TICK_SCALE_PERCENT + 50U) / 100U;
		if (ticks == 0U)
		{
			ticks = 1U;
		}
		step->ticks = (ticks > 0xFFFFU) ? 0xFFFFU : (UINT16)ticks;
	}
}
#else
#define soc_apply_empty_tail_tuning(offset_mv, step) \
	((void)(offset_mv), (void)(step))
#endif

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

static UINT8 soc_empty_tail_config(UINT8 mode, SOC_TAIL_STEP *step)
{
	UINT8 band = soc_empty_current_band(mode);
	int16_t matched_offset = 0;
	UINT8 found;

	if (soc_vmin_above_empty_offset(SOC_EMPTY_TAIL_START_OFFSET_MV))
	{
		return 0U;
	}
	found = soc_tail_rule_lookup(s_empty_tail_table,
		(UINT16)(sizeof(s_empty_tail_table) / sizeof(s_empty_tail_table[0])),
		band,
		SOC_MID_TARGET_DISABLED,
		1U,
		step,
		&matched_offset);
	if (found)
	{
		soc_apply_empty_tail_tuning(matched_offset, step);
	}
	return found;
}

static UINT8 soc_low_tail_config(UINT8 mode, SOC_TAIL_STEP *step)
{
	if ((mode == SOC_MODE_CHG) || !soc_voltage_valid())
	{
		return 0U;
	}
	if (soc_sag_hold_blocks_calibration())
	{
		return 0U;
	}
	return soc_empty_tail_config(mode, step);
}

static UINT8 soc_mid_tail_config(UINT8 mode, SOC_TAIL_STEP *step)
{
	UINT8 band = soc_empty_current_band(mode);
	UINT16 vmin = SOC_Enhance_Element.u16_VCellMin;

	if ((mode == SOC_MODE_CHG) ||
		!soc_voltage_valid() ||
		(soc_cell_delta() > SOC_MID_MAX_DELTA_MV) ||
		soc_sag_hold_blocks_calibration() ||
		(vmin <= soc_empty_threshold_mv(400)))
	{
		return 0U;
	}

	return soc_tail_rule_lookup(s_mid_tail_table,
		(UINT16)(sizeof(s_mid_tail_table) / sizeof(s_mid_tail_table[0])),
		band,
		SOC_MID_TARGET_DISABLED,
		0U,
		step,
		0);
}

static UINT8 soc_apply_mid_tail(const SOC_TAIL_STEP *step)
{
	UINT8 old_soc = s_soc.soc;
	UINT8 changed;

	if (s_soc.soc <= step->target)
	{
		s_soc.mid_ticks = 0U;
		return 0U;
	}
	changed = soc_apply_tail_step(step, &s_soc.mid_ticks);
	if (changed)
	{
		soc_watch_set_calib_source(SOC_WATCH_CALIB_MID_TAIL, old_soc, s_soc.soc);
	}
	return changed;
}

static UINT8 soc_apply_full_empty(UINT8 mode,
								  UINT8 empty_active,
								  const SOC_TAIL_STEP *empty_step)
{
	UINT16 full_seconds;
	UINT16 full_confirm_ticks;
	UINT8 old_soc = s_soc.soc;

	if (mode != SOC_MODE_DSG)
	{
		full_seconds = soc_full_confirm_seconds();
		if (full_seconds != 0U)
		{
			full_confirm_ticks = (UINT16)(full_seconds * SOC_TICKS_PER_SECOND);
			s_soc.empty_ticks = 0U;
			if (s_soc.full_ticks < full_confirm_ticks)
			{
				++s_soc.full_ticks;
			}
			if (s_soc.full_ticks >= full_confirm_ticks)
			{
				soc_set(soc_step(s_soc.soc, 100U, SOC_CAL_STEP));
				s_soc.full_ticks = 0U;
				s_soc.full_anchor = (s_soc.soc >= 100U) ? 1U : 0U;
			}
			if (s_soc.soc != old_soc)
			{
				soc_watch_set_calib_source(SOC_WATCH_CALIB_FULL_ANCHOR, old_soc, s_soc.soc);
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

	if (!empty_active)
	{
		s_soc.empty_ticks = 0U;
		return 0U;
	}
	if (soc_apply_tail_step(empty_step, &s_soc.empty_ticks))
	{
		soc_watch_set_calib_source(SOC_WATCH_CALIB_EMPTY_TAIL, old_soc, s_soc.soc);
		return 1U;
	}
	return 0U;
}

static void soc_reset_rest_confidence(void)
{
	s_soc.rest_ticks = 0U;
	s_soc.stable_rest_ticks = 0U;
	s_soc.short_rest_ticks = 0U;
	s_soc.long_rest_down_ticks = 0U;
	s_soc.rest_ref_vmin = 0U;
	s_soc.rest_ref_vmax = 0U;
}

static UINT8 soc_apply_long_rest_down_step(UINT32 delta_ticks)
{
	UINT8 changed;
	UINT8 old_soc = s_soc.soc;

	if ((!s_soc.deferred_ocv_valid) ||
		(s_soc.deferred_ocv_target >= s_soc.soc) ||
		(s_soc.rest_ticks < SOC_REST_LIMIT_TICKS))
	{
		s_soc.long_rest_down_ticks = 0U;
		return 0U;
	}
	if (s_soc.long_rest_down_ticks < SOC_LONG_REST_DOWN_STEP_TICKS)
	{
		if (delta_ticks > (SOC_LONG_REST_DOWN_STEP_TICKS - s_soc.long_rest_down_ticks))
		{
			s_soc.long_rest_down_ticks = SOC_LONG_REST_DOWN_STEP_TICKS;
		}
		else
		{
			s_soc.long_rest_down_ticks += delta_ticks;
		}
	}
	if (s_soc.long_rest_down_ticks < SOC_LONG_REST_DOWN_STEP_TICKS)
	{
		return 0U;
	}
	changed = soc_apply_ocv_target_step(s_soc.deferred_ocv_target, SOC_MODE_RELAX);
	s_soc.long_rest_down_ticks = 0U;
	if (changed)
	{
		soc_watch_set_calib_source(SOC_WATCH_CALIB_LONG_REST_DOWN, old_soc, s_soc.soc);
	}
	if (s_soc.deferred_ocv_target == s_soc.soc)
	{
		soc_clear_deferred_ocv();
	}
	return changed;
}

static UINT8 soc_rest_voltage_stable(void)
{
	if (!soc_calibration_allowed() ||
		(soc_cell_delta() > SOC_MID_MAX_DELTA_MV) ||
		soc_sag_hold_blocks_calibration())
	{
		soc_watch_set_rest_voltage_stable(0U);
		return 0U;
	}
	if ((s_soc.rest_ref_vmin == 0U) || (s_soc.rest_ref_vmax == 0U))
	{
		s_soc.rest_ref_vmin = SOC_Enhance_Element.u16_VCellMin;
		s_soc.rest_ref_vmax = SOC_Enhance_Element.u16_VCellMax;
		soc_watch_set_rest_voltage_stable(1U);
		return 1U;
	}
	if ((soc_abs_diff_u16(SOC_Enhance_Element.u16_VCellMin, s_soc.rest_ref_vmin) <= SOC_REST_STABLE_DELTA_MV) &&
		(soc_abs_diff_u16(SOC_Enhance_Element.u16_VCellMax, s_soc.rest_ref_vmax) <= SOC_REST_STABLE_DELTA_MV))
	{
		soc_watch_set_rest_voltage_stable(1U);
		return 1U;
	}
	s_soc.rest_ref_vmin = SOC_Enhance_Element.u16_VCellMin;
	s_soc.rest_ref_vmax = SOC_Enhance_Element.u16_VCellMax;
	soc_watch_set_rest_voltage_stable(0U);
	return 0U;
}

static void soc_update_rest_timer(UINT8 mode)
{

	if (mode != SOC_MODE_RELAX)
	{
		soc_watch_set_block_reason(SOC_WATCH_BLOCK_NOT_RELAX);
		soc_reset_rest_confidence();
		return;
	}
	if (s_soc.rest_ticks < SOC_REST_LIMIT_TICKS)
	{
		++s_soc.rest_ticks;
	}
	if (soc_rest_voltage_stable())
	{
		if (s_soc.stable_rest_ticks < SOC_STABLE_LIMIT_TICKS)
		{
			++s_soc.stable_rest_ticks;
		}
		if (s_soc.short_rest_ticks < SOC_SHORT_STEP_TICKS)
		{
			++s_soc.short_rest_ticks;
		}
	}
	else
	{
		s_soc.stable_rest_ticks = 0U;
		s_soc.short_rest_ticks = 0U;
		s_soc.long_rest_down_ticks = 0U;
		soc_clear_deferred_ocv();
		soc_watch_set_block_reason(SOC_WATCH_BLOCK_REST_UNSTABLE);
	}
	if ((s_soc.stable_rest_ticks >= SOC_SHORT_MIN_TICKS) &&
		(s_soc.short_rest_ticks >= SOC_SHORT_STEP_TICKS))
	{
		(void)soc_latch_rest_ocv_target();
		s_soc.short_rest_ticks = 0U;
	}
	(void)soc_apply_long_rest_down_step(1U);
}

static void soc_add_rest_seconds(UINT32 *ticks, UINT32 seconds, UINT32 limit_seconds)
{
	UINT32 limit_ticks = SOC_SEC_TO_TICKS(limit_seconds);
	UINT32 delta_ticks = SOC_SEC_TO_TICKS(seconds);

	if (*ticks >= limit_ticks)
	{
		return;
	}
	if (delta_ticks > (limit_ticks - *ticks))
	{
		*ticks = limit_ticks;
	}
	else
	{
		*ticks += delta_ticks;
	}
}

static UINT8 soc_apply_rtc_rest_ocv(UINT32 rest_seconds)
{
	UINT32 delta_seconds;
	UINT8 has_rest_ref;
	UINT8 changed = 0U;

	if (rest_seconds < s_u32RtcRestCursorSeconds)
	{
		soc_reset_rest_confidence();
		soc_clear_deferred_ocv();
		s_u32RtcRestCursorSeconds = 0U;
	}

	delta_seconds = rest_seconds - s_u32RtcRestCursorSeconds;
	s_u32RtcRestCursorSeconds = rest_seconds;
	if (delta_seconds == 0U)
	{
		return 0U;
	}

	changed = soc_apply_board_self_consumption_seconds(delta_seconds);
	soc_add_rest_seconds(&s_soc.rest_ticks, delta_seconds, SOC_REST_OCV_SECONDS);
	has_rest_ref = (UINT8)((s_soc.rest_ref_vmin != 0U) && (s_soc.rest_ref_vmax != 0U));
	if (soc_rest_voltage_stable())
	{
		if (!has_rest_ref)
		{
			return changed;
		}
		soc_add_rest_seconds(&s_soc.stable_rest_ticks, delta_seconds,
			SOC_REST_STABLE_LIMIT_SECS);
		soc_add_rest_seconds(&s_soc.short_rest_ticks, delta_seconds, SOC_SHORT_REST_STEP_SECONDS);
	}
	else
	{
		s_soc.stable_rest_ticks = 0U;
		s_soc.short_rest_ticks = 0U;
		s_soc.long_rest_down_ticks = 0U;
		soc_clear_deferred_ocv();
		return changed;
	}

	if ((s_soc.stable_rest_ticks >= SOC_SHORT_MIN_TICKS) &&
		(s_soc.short_rest_ticks >= SOC_SHORT_STEP_TICKS))
	{
		(void)soc_latch_rest_ocv_target();
		s_soc.short_rest_ticks = 0U;
	}
	changed |= soc_apply_long_rest_down_step(SOC_SEC_TO_TICKS(delta_seconds));
	return changed;
}

static UINT8 soc_display_target(void)
{
	if (SystemFeature_IsSocZero())
	{
		return 0U;
	}
	if (SystemFeature_IsSocFixed())
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
		SystemFeature_IsSocZero() ||
		SystemFeature_IsSocFixed())
	{
		s_soc.display_soc = target;
		s_soc.display_ready = 1U;
		s_soc.display_ticks = 0U;
	}
	else if (s_soc.display_soc != target)
	{
		if ((target < s_soc.display_soc) &&
			(SOC_Enhance_Element.u16_VCellMin <= soc_empty_threshold_mv(SOC_DISPLAY_LOW_OFFSET_MV)))
		{
			ticks = (SOC_Enhance_Element.u16_VCellMin <=
				soc_empty_threshold_mv((int16_t)(0 - SOC_DISPLAY_EMPTY_FAST_BELOW_V0_MV))) ?
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
	soc_watch_refresh(force_display);
	SOC_PublishReportData();
}

static void soc_handle_command(void)
{
	UINT8 save = 0U;
	UINT8 soc_keep = s_soc.soc;

	switch (SOC_Enhance_Element.u16_RefreshData_Flag)
	{
	case 1:
		if (!SystemFeature_IsSocFixed())
		{
			save = soc_apply_rest_ocv(SOC_REST_OCV_SECONDS, soc_direction());
			if (save)
			{
				soc_watch_set_calib_source(SOC_WATCH_CALIB_MANUAL_OCV, soc_keep, s_soc.soc);
			}
		}
		else
		{
			soc_watch_set_block_reason(SOC_WATCH_BLOCK_REFRESH_FIXED);
		}
		break;
	case 2:
		if (!SystemFeature_IsSocZero())
		{
			s_soc.cap_factory_as10 = soc_factory_cap_as10_from(SOC_Enhance_Element.u16_SOC_Ah);
			s_soc.cycle_x100 = (UINT32)SOC_Enhance_Element.u16_SOC_CycleT_Ever * 100U;
			s_soc.dsg_acc_as10 = 0U;
			soc_refresh_capacity_base();
			soc_set(soc_keep);
			save = 1U;
			soc_watch_set_calib_source(SOC_WATCH_CALIB_PARAM_RESET, soc_keep, s_soc.soc);
		}
		else
		{
			soc_watch_set_block_reason(SOC_WATCH_BLOCK_REFRESH_ZERO);
		}
		break;
	case 3:
		soc_set(SOC_Enhance_Element.u8_SetSocOnce);
		save = 1U;
		soc_watch_set_calib_source(SOC_WATCH_CALIB_SET_ONCE, soc_keep, s_soc.soc);
		break;
	default:
		break;
	}
	SOC_Enhance_Element.u16_RefreshData_Flag = 0U;
	if (save)
	{
		soc_save_current_snapshot();
	}
	soc_publish(1U);
}

void soc_param_lib_init(void)
{
	memset(&s_soc, 0, sizeof(s_soc));
	s_soc.cap_factory_as10 = soc_factory_cap_as10_from(SOC_Enhance_Element.u16_SOC_Ah);
	s_soc.cycle_x100 = (UINT32)SOC_Enhance_Element.u16_SOC_CycleT_Ever * 100U;
	s_u32RtcRestCursorSeconds = 0U;
	soc_refresh_capacity_base();
	SOC_UpdateSampleData(g_stCellInfoReport.u16VCellMax,
						 g_stCellInfoReport.u16VCellMin,
						 g_stCellInfoReport.u16Ichg,
						 g_stCellInfoReport.u16IDischg);
	soc_load_or_default();
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

void SOC_SaveSnapshotBeforeSleep(void)
{
	soc_save_if_needed();
}

void SOC_IntEnhance_Ctrl(void)
{
	SOC_TAIL_STEP low_tail_step;
	SOC_TAIL_STEP mid_tail_step;
	UINT8 calibrated;
	UINT8 low_tail_active;
	UINT8 mid_tail_active;

	if (SOC_Enhance_Element.u16_RefreshData_Flag != 0U)
	{
		soc_handle_command();
		return;
	}
	s_soc.mode = soc_direction();
	soc_integrate(s_soc.mode);
	soc_update_sag_hold(s_soc.mode);
	low_tail_active = soc_low_tail_config(s_soc.mode, &low_tail_step);
	mid_tail_active = soc_mid_tail_config(s_soc.mode, &mid_tail_step);
	soc_watch_set_tail_state(low_tail_active, &low_tail_step, mid_tail_active, &mid_tail_step);
	calibrated = soc_apply_full_empty(s_soc.mode, low_tail_active, &low_tail_step);
	if (!mid_tail_active)
	{
		s_soc.mid_ticks = 0U;
	}
	if (!calibrated && !low_tail_active && mid_tail_active)
	{
		calibrated = soc_apply_mid_tail(&mid_tail_step);
	}
	if (!calibrated && !low_tail_active)
	{
		calibrated = soc_apply_deferred_ocv_step(s_soc.mode);
	}
	if (!low_tail_active && !calibrated && !soc_sag_hold_blocks_calibration())
	{
		soc_update_rest_timer(s_soc.mode);
	}
	else if (low_tail_active || soc_sag_hold_blocks_calibration())
	{
		if (low_tail_active)
		{
			soc_watch_set_block_reason(SOC_WATCH_BLOCK_LOW_TAIL);
		}
		soc_reset_rest_confidence();
	}
	soc_save_if_needed();
	soc_publish(0U);
}

void SOC_ApplyRtcRelaxationCompensation(UINT32 rest_seconds, UINT16 vcell_min, UINT16 vcell_max)
{
	UINT8 changed;
#if PROJECT_CFG_DEBUG_WATCH_ENABLE
	UINT8 old_soc;
#endif
#if PROJECT_CFG_DEBUG_WATCH_ENABLE
	old_soc = s_soc.soc;
#endif
	SOC_Enhance_Element.u16_VCellMin = vcell_min;
	SOC_Enhance_Element.u16_VCellMax = vcell_max;
	changed = soc_apply_rtc_rest_ocv(rest_seconds);
	if (changed)
	{
#if PROJECT_CFG_DEBUG_WATCH_ENABLE
		soc_watch_set_calib_source(SOC_WATCH_CALIB_RTC_REST, old_soc, s_soc.soc);
#endif
		soc_save_current_snapshot();
	}
	soc_publish(0U);
}

#if PROJECT_CFG_DEBUG_MONITOR_ENABLE
void SOC_GetDebugInternals(uint8_t *mode, uint8_t *last_mode,
                           uint32_t *rest_ticks, uint32_t *stable_ticks,
                           uint16_t *full_ticks, uint16_t *empty_ticks,
                           uint16_t *mid_ticks, uint8_t *full_anchor,
                           uint8_t *cal_allowed, uint8_t *sag_blocked,
                           uint8_t *rest_stable, uint8_t *low_tail,
                           uint8_t *mid_tail, uint16_t *display_ticks)
{
	if (mode)          *mode          = s_soc.mode;
	if (last_mode)     *last_mode     = s_soc.last_mode;
	if (rest_ticks)    *rest_ticks    = s_soc.rest_ticks;
	if (stable_ticks)  *stable_ticks  = s_soc.stable_rest_ticks;
	if (full_ticks)    *full_ticks    = s_soc.full_ticks;
	if (empty_ticks)   *empty_ticks   = s_soc.empty_ticks;
	if (mid_ticks)     *mid_ticks     = s_soc.mid_ticks;
	if (full_anchor)   *full_anchor   = s_soc.full_anchor;
	if (cal_allowed)   *cal_allowed   = s_soc.full_anchor; /* simplified */
	if (sag_blocked)   *sag_blocked   = soc_sag_hold_blocks_calibration();
	if (rest_stable)   *rest_stable   = 0U; /* DEBUG_WATCH disabled */
	if (low_tail)      *low_tail      = 0U;
	if (mid_tail)      *mid_tail      = 0U;
	if (display_ticks) *display_ticks = s_soc.display_ticks;
}
#endif
