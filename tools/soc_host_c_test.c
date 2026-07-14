#include "main.h"
#include "Flash.h"
#include "SocEnhance.h"

#include <stdio.h>
#include <string.h>

#define HOST_CAP_A10               ((UINT16)270U)
#define HOST_CAP_FACTORY_AS10      ((UINT32)HOST_CAP_A10 * 3600U)
#define HOST_CAP_RESERVE_AS10      ((UINT32)PROJECT_CFG_SOC_RESERVE_CAPACITY_AH10 * 3600U)
#define HOST_CAP_USABLE_AS10       ((HOST_CAP_RESERVE_AS10 < HOST_CAP_FACTORY_AS10) ? \
	(HOST_CAP_FACTORY_AS10 - HOST_CAP_RESERVE_AS10) : HOST_CAP_FACTORY_AS10)
#define HOST_REBOUND_FLAG          ((UINT16)0x0001U)
#define HOST_TICKS_PER_SECOND      ((UINT16)5U)
#define HOST_MAMS_PER_AS10         ((UINT32)100000U)
#define HOST_LONG_REST_DOWN_STEP_SECONDS ((UINT16)PROJECT_CFG_SOC_REST_DOWN_STEP_SECONDS)
#define HOST_REST_DOWN_START_SECONDS ((UINT16)PROJECT_CFG_SOC_REST_OCV_SECONDS)

extern UINT16 SOC_GetTypeCBatEquivCurrentA10(void);

struct OTHER_ELEMENT OtherElement;
struct stCell_Info g_stCellInfoReport;
volatile struct SYSTEM_ERROR System_ErrFlag;

UINT8 SeriesNum = 10U;
static UINT16 s_host_typec_out_current_mA;
static UINT32 s_host_vbat_mV;
static UINT32 s_host_afe_current_sample_seq;

static STORAGE_FLASH_SOC_DATA s_flash_soc;
static UINT8 s_flash_soc_valid;
static UINT32 s_flash_save_count;
static unsigned s_failures;
static unsigned s_tests_run;

#define CHECK_TRUE(expr) host_check((expr) ? 1 : 0, #expr, __LINE__)
#define CHECK_EQ_U32(actual, expected) host_check_u32((UINT32)(actual), (UINT32)(expected), #actual, __LINE__)
#define CHECK_RANGE_U32(actual, min_v, max_v) host_check_range_u32((UINT32)(actual), (UINT32)(min_v), (UINT32)(max_v), #actual, __LINE__)

UINT8 StorageFlash_LoadSocData(STORAGE_FLASH_SOC_DATA *data)
{
	if ((data == 0) || !s_flash_soc_valid)
	{
		return 0U;
	}
	*data = s_flash_soc;
	return 1U;
}

UINT8 StorageFlash_SaveSocData(const STORAGE_FLASH_SOC_DATA *data)
{
	if (data == 0)
	{
		return 0U;
	}
	s_flash_soc = *data;
	s_flash_soc_valid = 1U;
	++s_flash_save_count;
	return 1U;
}

UINT8 System_ERROR_UserCallback(enum SYSTEM_ERROR_COMMAND errorCode)
{
	(void)errorCode;
	return 0U;
}

UINT16 ADC_GetTypeCOutCurrentMilliAmp(void)
{
	return s_host_typec_out_current_mA;
}

UINT32 ADC_GetVbatMilliVolt(void)
{
	return s_host_vbat_mV;
}

UINT32 AfeCurrent_GetSeq(void)
{
	return s_host_afe_current_sample_seq;
}

static void host_check(int ok, const char *expr, int line)
{
	if (!ok)
	{
		printf("FAIL line %d: %s\n", line, expr);
		++s_failures;
	}
}

static void host_check_u32(UINT32 actual, UINT32 expected, const char *expr, int line)
{
	if (actual != expected)
	{
		printf("FAIL line %d: %s actual=%lu expected=%lu\n",
			   line, expr, (unsigned long)actual, (unsigned long)expected);
		++s_failures;
	}
}

static void host_check_range_u32(UINT32 actual, UINT32 min_v, UINT32 max_v, const char *expr, int line)
{
	if ((actual < min_v) || (actual > max_v))
	{
		printf("FAIL line %d: %s actual=%lu expected_range=[%lu,%lu]\n",
			   line, expr, (unsigned long)actual, (unsigned long)min_v, (unsigned long)max_v);
		++s_failures;
	}
}

static UINT32 host_cap_now_from_soc(UINT16 soc)
{
	return (UINT32)(((uint64_t)HOST_CAP_USABLE_AS10 * (uint64_t)soc) / 100ULL);
}

static UINT8 host_soh_from_cycle(UINT32 cycle_x100)
{
	UINT32 drop = (cycle_x100 / 100U) / 100U;

	return (drop >= 20U) ? 80U : (UINT8)(100U - drop);
}

static UINT32 host_full_cap_from_cycle(UINT32 cycle_x100)
{
	return (UINT32)(((uint64_t)HOST_CAP_FACTORY_AS10 * host_soh_from_cycle(cycle_x100)) / 100ULL);
}

static UINT32 host_usable_cap_from_full(UINT32 cap_full_as10)
{
	return (HOST_CAP_RESERVE_AS10 < cap_full_as10) ?
		(cap_full_as10 - HOST_CAP_RESERVE_AS10) : cap_full_as10;
}

static UINT16 host_soc_from_cap(UINT32 cap_as10)
{
	UINT32 soc;

	if (cap_as10 >= HOST_CAP_USABLE_AS10)
	{
		return 100U;
	}
	soc = (UINT32)(((uint64_t)cap_as10 * 100ULL +
		(HOST_CAP_USABLE_AS10 / 2U)) / HOST_CAP_USABLE_AS10);
	return (soc > 100U) ? 100U : (UINT16)soc;
}

static UINT16 host_cap_to_ah100(UINT32 cap_as10)
{
	return (UINT16)((cap_as10 + 180U) / 360U);
}

static UINT16 host_report_cap_to_ah100(UINT32 cap_as10)
{
	UINT32 report_as10 = (UINT32)(((uint64_t)cap_as10 * HOST_CAP_FACTORY_AS10 +
		(HOST_CAP_USABLE_AS10 / 2U)) / HOST_CAP_USABLE_AS10);

	return host_cap_to_ah100(report_as10);
}

static UINT32 host_self_delta_as10(UINT32 current_ma, UINT32 seconds)
{
	return (UINT32)(((uint64_t)current_ma * (uint64_t)seconds * 1000ULL) /
		(uint64_t)HOST_MAMS_PER_AS10);
}

static UINT16 host_charge_a10_over_self(void)
{
	UINT32 a10 = ((UINT32)PROJECT_CFG_SOC_BOARD_SELF_CONSUMPTION_MA + 99U) / 100U;

	a10 += 2U;
	if (a10 < 4U)
	{
		a10 = 4U;
	}
	return (a10 > 0xFFFFU) ? 0xFFFFU : (UINT16)a10;
}

static void host_apply_default_config(void)
{
	memset(&OtherElement, 0, sizeof(OtherElement));
	OtherElement.u16Soc_Ah = HOST_CAP_A10;
	OtherElement.u16Soc_Cycle_times = 3U;
	OtherElement.u16Soc_TableSelect = SOC_TABLE_TERNARYLI;
	OtherElement.u16Soc_V_100 = 4180U;
	OtherElement.u16Soc_V_0 = 3000U;
	OtherElement.u16Sys_SeriesNum = 10U;
	SeriesNum = 10U;
}

static void host_reset_state(void)
{
	memset(&g_stCellInfoReport, 0, sizeof(g_stCellInfoReport));
	memset((void *)&System_ErrFlag, 0, sizeof(System_ErrFlag));
	memset(&s_flash_soc, 0, sizeof(s_flash_soc));
	s_flash_soc_valid = 0U;
	s_flash_save_count = 0U;
	s_host_typec_out_current_mA = 0U;
	s_host_vbat_mV = 0U;
	s_host_afe_current_sample_seq = 0U;
	host_apply_default_config();
}

static void host_set_snapshot(UINT16 soc, UINT16 flags)
{
	memset(&s_flash_soc, 0, sizeof(s_flash_soc));
	s_flash_soc.u16FormatVersion = FLASH_STORAGE_SOC_DATA_VERSION_V2;
	s_flash_soc.u16SocNow = soc;
	s_flash_soc.u16DsgSocInt = 0U;
	s_flash_soc.u16MaxErrorPercent = 100U;
	s_flash_soc.u32CycleTimes = 300U;
	s_flash_soc.u32CapFull = HOST_CAP_USABLE_AS10;
	s_flash_soc.u32CapNow = host_cap_now_from_soc(soc);
	s_flash_soc.u32LearnPassedAs10 = 0U;
	s_flash_soc.u16Flags = flags;
	s_flash_soc_valid = 1U;
}

static void host_set_snapshot_cycles(UINT16 soc, UINT16 flags, UINT32 cycle_x100)
{
	UINT32 cap_full_as10 = host_full_cap_from_cycle(cycle_x100);
	UINT32 cap_usable_as10 = host_usable_cap_from_full(cap_full_as10);

	host_set_snapshot(soc, flags);
	s_flash_soc.u32CycleTimes = cycle_x100;
	s_flash_soc.u32CapFull = cap_usable_as10;
	s_flash_soc.u32CapNow = (UINT32)(((uint64_t)cap_usable_as10 * soc) / 100ULL);
}

static void host_set_legacy_snapshot(UINT16 soc)
{
	host_set_snapshot(soc, 0U);
	s_flash_soc.u32CapFull = HOST_CAP_FACTORY_AS10;
	s_flash_soc.u32CapNow = (UINT32)(((uint64_t)HOST_CAP_FACTORY_AS10 * soc) / 100ULL);
}

static void host_init_with_voltage(UINT16 vmax, UINT16 vmin)
{
	g_stCellInfoReport.u16VCellMax = vmax;
	g_stCellInfoReport.u16VCellMin = vmin;
	g_stCellInfoReport.u16VCellTotle = (UINT16)(((UINT32)vmin * (UINT32)SeriesNum) / 10U);
	g_stCellInfoReport.u16Ichg = 0U;
	g_stCellInfoReport.u16IDischg = 0U;
	InitData_SOC();
}

static void host_tick(UINT16 vmax, UINT16 vmin, UINT16 ichg, UINT16 idsg)
{
	g_stCellInfoReport.u16VCellMax = vmax;
	g_stCellInfoReport.u16VCellMin = vmin;
	g_stCellInfoReport.u16VCellTotle = (UINT16)(((UINT32)vmin * (UINT32)SeriesNum) / 10U);
	s_host_vbat_mV = (UINT32)vmin * (UINT32)SeriesNum;
	g_stCellInfoReport.u16Ichg = ichg;
	g_stCellInfoReport.u16IDischg = idsg;
	++s_host_afe_current_sample_seq;
	App_SOC();
}

static void host_run_seconds(UINT16 seconds, UINT16 vmax, UINT16 vmin, UINT16 ichg, UINT16 idsg)
{
	UINT32 ticks = (UINT32)seconds * HOST_TICKS_PER_SECOND;
	while (ticks-- > 0U)
	{
		host_tick(vmax, vmin, ichg, idsg);
	}
}

static void host_run_ticks(UINT32 ticks, UINT16 vmax, UINT16 vmin, UINT16 ichg, UINT16 idsg)
{
	while (ticks-- > 0U)
	{
		host_tick(vmax, vmin, ichg, idsg);
	}
}

static UINT16 host_internal_soc(void)
{
	return s_flash_soc.u16SocNow;
}

static void test_startup_ocv_uses_real_c_code(void)
{
	host_reset_state();
	host_init_with_voltage(3835U, 3835U);
	CHECK_EQ_U32(g_stCellInfoReport.SocElement.u16Soc, 70U);
	CHECK_EQ_U32(host_internal_soc(), 70U);
}

static void test_startup_without_loaded_voltage_defaults_to_60(void)
{
	host_reset_state();
	host_init_with_voltage(0U, 0U);
	CHECK_EQ_U32(g_stCellInfoReport.SocElement.u16Soc, 60U);
	CHECK_EQ_U32(host_internal_soc(), 60U);
}

static void test_deep_sleep_wake_ocv_replaces_only_large_valid_difference(void)
{
	UINT32 saves_before;
	UINT8 threshold = (UINT8)PROJECT_CFG_SOC_DEEP_SLEEP_WAKE_OCV_DIFF_PERCENT;

	host_reset_state();
	host_set_snapshot(90U, 0U);
	host_init_with_voltage(3835U, 3835U);
	saves_before = s_flash_save_count;
	CHECK_EQ_U32(SOC_ApplyDeepSleepWakeOcvCalibration(), 1U);
	CHECK_EQ_U32(host_internal_soc(), 70U);
	CHECK_EQ_U32(s_flash_save_count, saves_before + 1U);

	host_reset_state();
	host_set_snapshot(50U, 0U);
	host_init_with_voltage(4050U, 4050U);
	CHECK_EQ_U32(SOC_ApplyDeepSleepWakeOcvCalibration(), 1U);
	CHECK_EQ_U32(host_internal_soc(), 90U);

	host_reset_state();
	host_set_snapshot(threshold, 0U);
	host_init_with_voltage(3000U, 3000U);
	CHECK_EQ_U32(SOC_ApplyDeepSleepWakeOcvCalibration(), 1U);
	CHECK_EQ_U32(host_internal_soc(), 0U);

	host_reset_state();
	host_set_snapshot((UINT16)(threshold - 1U), 0U);
	host_init_with_voltage(3000U, 3000U);
	CHECK_EQ_U32(SOC_ApplyDeepSleepWakeOcvCalibration(), 0U);
	CHECK_EQ_U32(host_internal_soc(), (UINT32)(threshold - 1U));

	host_reset_state();
	host_set_snapshot(90U, 0U);
	host_init_with_voltage(4036U, 3835U);
	CHECK_EQ_U32(SOC_ApplyDeepSleepWakeOcvCalibration(), 0U);
	CHECK_EQ_U32(host_internal_soc(), 90U);
}

static void test_discharge_integration_uses_app_soc_path(void)
{
	host_reset_state();
	host_set_snapshot(60U, 0U);
	host_init_with_voltage(3835U, 3835U);
	host_run_seconds(360U, 3835U, 3835U, 0U, 270U);
	CHECK_RANGE_U32(host_internal_soc(), 49U, 51U);
	CHECK_EQ_U32(g_stCellInfoReport.SocElement.u16Soc, host_internal_soc());
}

static void test_board_self_consumption_integrates_during_relax(void)
{
	UINT32 start_cap_as10 = host_cap_now_from_soc(70U);
	UINT32 expected_delta_as10 = host_self_delta_as10(
		(UINT32)PROJECT_CFG_SOC_BOARD_SELF_CONSUMPTION_MA,
		3600U);
	UINT32 expected_cap_as10 = start_cap_as10 - expected_delta_as10;

	host_reset_state();
	host_set_snapshot(70U, 0U);
	host_init_with_voltage(3835U, 3835U);
	host_run_seconds(3600U, 3835U, 3835U, 0U, 0U);
	CHECK_EQ_U32(g_stCellInfoReport.SocElement.u16CapacityNow,
		host_report_cap_to_ah100(expected_cap_as10));
	CHECK_EQ_U32(host_internal_soc(), host_soc_from_cap(expected_cap_as10));
}

static void test_board_self_consumption_works_at_high_non_full_voltage(void)
{
	UINT32 start_cap_as10 = host_cap_now_from_soc(80U);
	UINT32 expected_delta_as10 = host_self_delta_as10(
		(UINT32)PROJECT_CFG_SOC_BOARD_SELF_CONSUMPTION_MA,
		3600U);
	UINT32 expected_cap_as10 = start_cap_as10 - expected_delta_as10;

	host_reset_state();
	host_set_snapshot(80U, 0U);
	host_init_with_voltage(4050U, 4050U);
	host_run_seconds(3600U, 4050U, 4050U, 0U, 0U);
	CHECK_EQ_U32(g_stCellInfoReport.SocElement.u16CapacityNow,
		host_report_cap_to_ah100(expected_cap_as10));
	CHECK_EQ_U32(host_internal_soc(), host_soc_from_cap(expected_cap_as10));
}

static void test_full_voltage_anchor_can_override_self_consumption(void)
{
	host_reset_state();
	host_set_snapshot(99U, 0U);
	host_init_with_voltage(4181U, 4181U);
	host_run_seconds(PROJECT_CFG_SOC_FULL_CONFIRM_SECONDS, 4181U, 4181U, 0U, 0U);
	CHECK_EQ_U32(host_internal_soc(), 100U);
	CHECK_EQ_U32(g_stCellInfoReport.SocElement.u16CapacityNow,
		host_cap_to_ah100(HOST_CAP_FACTORY_AS10));
}

static void test_full_voltage_holds_100_during_long_zero_current_idle(void)
{
	host_reset_state();
	host_set_snapshot(100U, 0U);
	host_init_with_voltage(4200U, 4200U);
	host_run_seconds(3600U, 4200U, 4200U, 0U, 0U);
	CHECK_EQ_U32(host_internal_soc(), 100U);
	CHECK_EQ_U32(g_stCellInfoReport.SocElement.u16CapacityNow, 2700U);
}

static void test_rtc_sleep_does_not_apply_board_self_consumption(void)
{
	UINT32 start_cap_as10 = host_cap_now_from_soc(80U);

	host_reset_state();
	host_set_snapshot(80U, 0U);
	host_init_with_voltage(4050U, 4050U);
	SOC_ApplyRtcRelaxationCompensation(3600U, 4050U, 4050U);
	CHECK_EQ_U32(g_stCellInfoReport.SocElement.u16CapacityNow,
		host_report_cap_to_ah100(start_cap_as10));
	CHECK_EQ_U32(host_internal_soc(), 80U);
}

static void test_board_self_consumption_adjusts_charge_and_discharge_current(void)
{
	UINT32 start_cap_as10 = host_cap_now_from_soc(70U);
	UINT32 board_ma = (UINT32)PROJECT_CFG_SOC_BOARD_SELF_CONSUMPTION_MA;
	UINT32 expected_delta_as10;
	UINT32 expected_cap_as10;

	host_reset_state();
	host_set_snapshot(70U, 0U);
	host_init_with_voltage(3835U, 3835U);
	host_run_seconds(100U, 3835U, 3835U, 0U, 2U);
	expected_delta_as10 = host_self_delta_as10(200U + board_ma, 100U);
	expected_cap_as10 = start_cap_as10 - expected_delta_as10;
	CHECK_EQ_U32(host_internal_soc(), host_soc_from_cap(expected_cap_as10));

	host_reset_state();
	host_set_snapshot(70U, 0U);
	host_init_with_voltage(3835U, 3835U);
	host_run_seconds(100U, 3835U, 3835U, 2U, 0U);
	if (board_ma >= 200U)
	{
		expected_delta_as10 = host_self_delta_as10(board_ma - 200U, 100U);
		expected_cap_as10 = start_cap_as10 - expected_delta_as10;
	}
	else
	{
		expected_delta_as10 = host_self_delta_as10(200U - board_ma, 100U);
		expected_cap_as10 = start_cap_as10 + expected_delta_as10;
	}
	CHECK_EQ_U32(host_internal_soc(), host_soc_from_cap(expected_cap_as10));
}

static void test_typec_output_current_converts_to_battery_equivalent(void)
{
	UINT32 start_cap_as10 = host_cap_now_from_soc(60U);
	UINT32 expected_cap_as10;
	UINT32 net_charge_ma;

	host_reset_state();
	host_set_snapshot(60U, 0U);
	host_init_with_voltage(3835U, 3835U);
	s_host_typec_out_current_mA = 9000U;
	host_run_seconds(360U, 3835U, 3835U, 23U, 0U);
	expected_cap_as10 = start_cap_as10 - host_self_delta_as10(
		(UINT32)PROJECT_CFG_SOC_BOARD_SELF_CONSUMPTION_MA,
		360U);
	CHECK_EQ_U32(host_internal_soc(), host_soc_from_cap(expected_cap_as10));

	s_host_typec_out_current_mA = 0U;
	host_run_seconds(360U, 3835U, 3835U, 23U, 0U);
	net_charge_ma = (23U * 100U > (UINT32)PROJECT_CFG_SOC_BOARD_SELF_CONSUMPTION_MA) ?
		(23U * 100U - (UINT32)PROJECT_CFG_SOC_BOARD_SELF_CONSUMPTION_MA) : 0U;
	expected_cap_as10 += host_self_delta_as10(net_charge_ma, 360U);
	CHECK_RANGE_U32(host_internal_soc(),
		(UINT32)(host_soc_from_cap(expected_cap_as10) - 1U),
		(UINT32)(host_soc_from_cap(expected_cap_as10) + 1U));
}

static void test_full_confirm_reaches_100_only_after_voltage_anchor(void)
{
	host_reset_state();
	host_set_snapshot(98U, 0U);
	host_init_with_voltage(4100U, 4050U);
	host_run_seconds(900U, 4100U, 4050U, 270U, 0U);
	CHECK_EQ_U32(host_internal_soc(), 99U);

	host_run_seconds(15U, 4180U, 4100U, 270U, 0U);
	CHECK_EQ_U32(host_internal_soc(), 99U);

	host_run_seconds(15U, 4181U, 4100U, 270U, 0U);
	CHECK_EQ_U32(host_internal_soc(), 100U);
}

static void test_low_voltage_tail_reaches_zero(void)
{
	host_reset_state();
	host_set_snapshot(30U, 0U);
	host_init_with_voltage(3000U, 3000U);
	host_run_seconds(60U, 2950U, 2950U, 0U, 145U);
	CHECK_RANGE_U32(host_internal_soc(), 17U, 19U);
}

static void test_short_rest_ocv_ignores_upward_target_during_charge(void)
{
	UINT16 active_charge_a10 = host_charge_a10_over_self();
	UINT16 before_active;

	host_reset_state();
	host_set_snapshot(50U, 0U);
	host_init_with_voltage(3835U, 3835U);
	host_run_seconds(599U, 3835U, 3835U, 0U, 0U);
	CHECK_TRUE(host_internal_soc() <= 50U);
	host_run_seconds(1U, 3835U, 3835U, 0U, 0U);
	before_active = host_internal_soc();
	CHECK_TRUE(before_active <= 50U);
	host_run_seconds(600U, 3835U, 3835U, active_charge_a10, 0U);
	CHECK_TRUE(host_internal_soc() <= 50U);
}

static void test_short_rest_ocv_is_not_consumed_during_active_discharge(void)
{
	UINT16 before_discharge;

	host_reset_state();
	host_set_snapshot(80U, 0U);
	host_init_with_voltage(3835U, 3835U);
	host_run_seconds(600U, 3835U, 3835U, 0U, 0U);
	before_discharge = host_internal_soc();
	CHECK_TRUE(before_discharge <= 80U);
	host_run_seconds(600U, 3835U, 3835U, 0U, 4U);
	CHECK_RANGE_U32(host_internal_soc(), (UINT32)(before_discharge - 1U), before_discharge);
}

static void test_rtc_ocv_ignores_upward_stable_target(void)
{
	UINT32 seconds;
	UINT16 active_charge_a10 = host_charge_a10_over_self();
	UINT16 before_active;

	host_reset_state();
	host_set_snapshot(50U, 0U);
	host_init_with_voltage(3835U, 3835U);

	for (seconds = 10U; seconds < 610U; seconds += 10U)
	{
		SOC_ApplyRtcRelaxationCompensation(seconds, 3835U, 3835U);
	}
	CHECK_TRUE(host_internal_soc() <= 50U);

	SOC_ApplyRtcRelaxationCompensation(610U, 3835U, 3835U);
	before_active = host_internal_soc();
	CHECK_TRUE(before_active <= 50U);
	host_run_seconds(600U, 3835U, 3835U, active_charge_a10, 0U);
	CHECK_TRUE(host_internal_soc() <= 50U);
}

static void test_rtc_ocv_waits_for_voltage_convergence(void)
{
	UINT32 seconds;
	UINT16 vcell;

	host_reset_state();
	host_set_snapshot(80U, 0U);
	host_init_with_voltage(3835U, 3835U);

	for (seconds = 60U; seconds <= 600U; seconds += 60U)
	{
		vcell = (((seconds / 60U) & 1U) == 0U) ? 3770U : 3835U;
		SOC_ApplyRtcRelaxationCompensation(seconds, vcell, vcell);
	}
	CHECK_TRUE(host_internal_soc() <= 80U);

	for (seconds = 660U; seconds < 1260U; seconds += 60U)
	{
		SOC_ApplyRtcRelaxationCompensation(seconds, 3835U, 3835U);
	}
	CHECK_TRUE(host_internal_soc() <= 80U);

	SOC_ApplyRtcRelaxationCompensation(1260U, 3835U, 3835U);
	CHECK_TRUE(host_internal_soc() <= 80U);
}

static void test_unstable_long_rest_waits_for_voltage_convergence(void)
{
	UINT16 i;
	UINT16 vcell;
	UINT16 soc_before_stable;

	host_reset_state();
	host_set_snapshot(50U, 0U);
	host_init_with_voltage(3835U, 3835U);
	for (i = 0U; i < 9U; ++i)
	{
		vcell = ((i & 1U) == 0U) ? 3835U : 3770U;
		host_run_seconds(200U, vcell, vcell, 0U, 0U);
	}
	CHECK_TRUE(host_internal_soc() <= 50U);

	soc_before_stable = host_internal_soc();
	host_run_seconds(399U, 3835U, 3835U, 0U, 0U);
	CHECK_TRUE(host_internal_soc() <= soc_before_stable);
	host_run_seconds(2U, 3835U, 3835U, 0U, 0U);
	CHECK_TRUE(host_internal_soc() <= soc_before_stable);
}

static void test_long_rest_ocv_slowly_reduces_soc_above_low_tail(void)
{
	UINT32 wait_seconds = (UINT32)HOST_REST_DOWN_START_SECONDS +
		(UINT32)HOST_LONG_REST_DOWN_STEP_SECONDS;
	UINT16 before_final;

	host_reset_state();
	host_set_snapshot(80U, 0U);
	host_init_with_voltage(3725U, 3725U);
	host_run_seconds((UINT16)(wait_seconds - 1U), 3725U, 3725U, 0U, 0U);
	before_final = host_internal_soc();
	CHECK_TRUE(before_final <= 80U);
	host_run_seconds(1U, 3725U, 3725U, 0U, 0U);
	CHECK_TRUE(host_internal_soc() <= before_final);
}

static void test_rebound_flag_clears_when_holdoff_expires(void)
{
	host_reset_state();
	host_set_snapshot(80U, HOST_REBOUND_FLAG);
	host_init_with_voltage(3500U, 3500U);
	host_run_seconds(299U, 3500U, 3500U, 0U, 0U);
	CHECK_TRUE((s_flash_soc.u16Flags & HOST_REBOUND_FLAG) != 0U);

	host_run_seconds(1U, 3500U, 3500U, 0U, 0U);
	CHECK_TRUE((s_flash_soc.u16Flags & HOST_REBOUND_FLAG) == 0U);

	host_run_seconds(90U, 3500U, 3500U, 0U, 0U);
	CHECK_EQ_U32(host_internal_soc(), 80U);
}

static void test_set_soc_once_command_saves_snapshot(void)
{
	host_reset_state();
	host_set_snapshot(72U, 0U);
	host_init_with_voltage(3835U, 3835U);
	SOC_RequestSetOnce(35U);
	CHECK_EQ_U32(host_internal_soc(), 35U);
	CHECK_EQ_U32(g_stCellInfoReport.SocElement.u16Soc, 35U);
}

static void test_reserved_capacity_keeps_reported_full_and_reaches_zero_early(void)
{
	host_reset_state();
	host_set_snapshot(100U, 0U);
	host_init_with_voltage(4050U, 4050U);
	CHECK_EQ_U32(g_stCellInfoReport.SocElement.u16CapacityNow, 2700U);
	CHECK_EQ_U32(g_stCellInfoReport.SocElement.u16CapacityFull, 2700U);
	CHECK_EQ_U32(g_stCellInfoReport.SocElement.u16CapacityFactory, 2700U);
	CHECK_EQ_U32(s_flash_soc.u32CapFull, HOST_CAP_USABLE_AS10);

	host_run_seconds(360U, 4050U, 4050U, 0U,
		(UINT16)(HOST_CAP_USABLE_AS10 / 360U));
	CHECK_EQ_U32(host_internal_soc(), 0U);
	CHECK_EQ_U32(g_stCellInfoReport.SocElement.u16CapacityNow, 0U);
}

static void test_legacy_snapshot_migrates_by_saved_soc(void)
{
	host_reset_state();
	host_set_legacy_snapshot(70U);
	host_init_with_voltage(3835U, 3835U);
	CHECK_EQ_U32(g_stCellInfoReport.SocElement.u16Soc, 70U);
	CHECK_EQ_U32(g_stCellInfoReport.SocElement.u16CapacityNow, 1890U);

	SOC_RequestSetOnce(70U);
	CHECK_EQ_U32(s_flash_soc.u32CapFull, HOST_CAP_USABLE_AS10);
	CHECK_EQ_U32(s_flash_soc.u32CapNow, host_cap_now_from_soc(70U));
}

static void test_no_new_afe_sample_never_repeats_integration(void)
{
	UINT16 cap_before;
	UINT32 flash_cap_before;
	UINT16 i;

	host_reset_state();
	host_set_snapshot(60U, 0U);
	host_init_with_voltage(3835U, 3835U);
	/* Synchronize App_SOC's function-local last-sequence state between host cases. */
	host_tick(3835U, 3835U, 0U, 0U);
	host_tick(3835U, 3835U, 0U, 0U);
	cap_before = g_stCellInfoReport.SocElement.u16CapacityNow;
	flash_cap_before = s_flash_soc.u32CapNow;
	g_stCellInfoReport.u16Ichg = 0U;
	g_stCellInfoReport.u16IDischg = 270U;
	for (i = 0U; i < 1000U; ++i)
	{
		App_SOC();
	}
	CHECK_EQ_U32(g_stCellInfoReport.SocElement.u16CapacityNow, cap_before);
	CHECK_EQ_U32(s_flash_soc.u32CapNow, flash_cap_before);
}

static void test_typec_equivalent_current_uses_pack_voltage_and_fallback(void)
{
	host_reset_state();
	s_host_typec_out_current_mA = 9000U;
	g_stCellInfoReport.u16VCellTotle = 3835U;
	CHECK_EQ_U32(SOC_GetTypeCBatEquivCurrentA10(), 21U);

	g_stCellInfoReport.u16VCellTotle = 0U;
	s_host_vbat_mV = 36000U;
	CHECK_EQ_U32(SOC_GetTypeCBatEquivCurrentA10(), 23U);

	s_host_typec_out_current_mA = 0U;
	CHECK_EQ_U32(SOC_GetTypeCBatEquivCurrentA10(), 0U);
}

static void test_reported_capacity_maps_usable_domain_back_to_full_domain(void)
{
	host_reset_state();
	host_set_snapshot(50U, 0U);
	host_init_with_voltage(3835U, 3835U);
	CHECK_EQ_U32(g_stCellInfoReport.SocElement.u16Soc, 50U);
	CHECK_EQ_U32(g_stCellInfoReport.SocElement.u16CapacityNow, 1350U);
	CHECK_EQ_U32(g_stCellInfoReport.SocElement.u16CapacityFull, 2700U);
	CHECK_EQ_U32(g_stCellInfoReport.SocElement.u16CapacityFactory, 2700U);
}

static void test_soh_cycle_mapping_and_capacity_floor(void)
{
	UINT32 cap_full_as10;

	host_reset_state();
	host_set_snapshot_cycles(100U, 0U, 10000U);
	host_init_with_voltage(4050U, 4050U);
	cap_full_as10 = host_full_cap_from_cycle(10000U);
	CHECK_EQ_U32(g_stCellInfoReport.SocElement.u16Soh, 99U);
	CHECK_EQ_U32(g_stCellInfoReport.SocElement.u16CapacityFull,
		host_cap_to_ah100(cap_full_as10));
	CHECK_EQ_U32(g_stCellInfoReport.SocElement.u16CapacityNow,
		host_cap_to_ah100(cap_full_as10));

	host_reset_state();
	host_set_snapshot_cycles(100U, 0U, 200000U);
	host_init_with_voltage(4050U, 4050U);
	cap_full_as10 = host_full_cap_from_cycle(200000U);
	CHECK_EQ_U32(g_stCellInfoReport.SocElement.u16Soh, 80U);
	CHECK_EQ_U32(g_stCellInfoReport.SocElement.u16CapacityFull,
		host_cap_to_ah100(cap_full_as10));
	CHECK_EQ_U32(g_stCellInfoReport.SocElement.u16CapacityFactory, 2700U);
}

static void test_full_confirm_voltage_delta_mode_and_counter_boundaries(void)
{
	host_reset_state();
	host_set_snapshot(99U, 0U);
	host_init_with_voltage(4180U, 4100U);
	host_run_seconds(PROJECT_CFG_SOC_FULL_CONFIRM_SECONDS, 4180U, 4100U, 0U, 0U);
	CHECK_EQ_U32(host_internal_soc(), 99U);

	host_run_seconds((UINT16)(PROJECT_CFG_SOC_FULL_CONFIRM_SECONDS - 1U),
		4181U, 4100U, 0U, 0U);
	CHECK_EQ_U32(host_internal_soc(), 99U);
	host_tick(4000U, 4000U, 0U, 0U);
	host_run_seconds(1U, 4181U, 4100U, 0U, 0U);
	CHECK_EQ_U32(host_internal_soc(), 99U);
	host_run_seconds((UINT16)(PROJECT_CFG_SOC_FULL_CONFIRM_SECONDS - 1U),
		4181U, 4100U, 0U, 0U);
	CHECK_EQ_U32(host_internal_soc(), 100U);

	host_reset_state();
	host_set_snapshot(99U, 0U);
	host_init_with_voltage(4221U, 4100U);
	host_run_seconds(PROJECT_CFG_SOC_FULL_CONFIRM_SECONDS, 4221U, 4100U, 0U, 0U);
	CHECK_EQ_U32(host_internal_soc(), 99U);

	host_reset_state();
	host_set_snapshot(99U, 0U);
	host_init_with_voltage(4181U, 4100U);
	host_run_seconds(PROJECT_CFG_SOC_FULL_CONFIRM_SECONDS, 4181U, 4100U, 0U, 2U);
	CHECK_EQ_U32(host_internal_soc(), 99U);
}

static void test_full_confirm_steps_one_percent_per_window(void)
{
	host_reset_state();
	host_set_snapshot(95U, 0U);
	host_init_with_voltage(4181U, 4100U);
	host_run_seconds((UINT16)(PROJECT_CFG_SOC_FULL_CONFIRM_SECONDS * 5U - 1U),
		4181U, 4100U, 0U, 0U);
	CHECK_EQ_U32(host_internal_soc(), 99U);
	host_run_seconds(1U, 4181U, 4100U, 0U, 0U);
	CHECK_EQ_U32(host_internal_soc(), 100U);
}

static void test_empty_tail_critical_relax_and_dynamic_rate_boundaries(void)
{
	UINT32 dynamic_ticks = 120U + ((UINT32)20U * 480U / 135U);

	host_reset_state();
	host_set_snapshot(5U, 0U);
	host_init_with_voltage(3000U, 3000U);
	host_run_seconds(4U, 3000U, 3000U, 0U, 0U);
	CHECK_EQ_U32(host_internal_soc(), 5U);
	host_run_seconds(1U, 3000U, 3000U, 0U, 0U);
	CHECK_EQ_U32(host_internal_soc(), 4U);

	host_reset_state();
	host_set_snapshot(5U, 0U);
	host_init_with_voltage(3050U, 3050U);
	host_run_seconds(10U, 3050U, 3050U, 0U, 0U);
	CHECK_EQ_U32(host_internal_soc(), 4U);

	host_reset_state();
	host_set_snapshot(5U, 0U);
	host_init_with_voltage(3100U, 3100U);
	host_run_seconds(120U, 3100U, 3100U, 0U, 0U);
	CHECK_EQ_U32(host_internal_soc(), 5U);

	host_reset_state();
	host_set_snapshot(40U, 0U);
	host_init_with_voltage(3400U, 3400U);
	host_run_ticks(dynamic_ticks - 1U, 3400U, 3400U, 0U, 20U);
	CHECK_EQ_U32(host_internal_soc(), 40U);
	host_run_ticks(1U, 3400U, 3400U, 0U, 20U);
	CHECK_EQ_U32(host_internal_soc(), 39U);
}

static void test_sag_hold_blocks_soft_tail_but_not_critical_empty_tail(void)
{
	host_reset_state();
	host_set_snapshot(30U, 0U);
	host_init_with_voltage(3400U, 3400U);
	host_tick(3400U, 3400U, 0U, 140U);
	CHECK_TRUE((s_flash_soc.u16Flags & HOST_REBOUND_FLAG) != 0U);
	host_run_seconds(29U, 3400U, 3400U, 0U, 0U);
	CHECK_EQ_U32(host_internal_soc(), 30U);
	CHECK_TRUE((s_flash_soc.u16Flags & HOST_REBOUND_FLAG) != 0U);
	host_run_seconds(1U, 3400U, 3400U, 0U, 0U);
	CHECK_TRUE((s_flash_soc.u16Flags & HOST_REBOUND_FLAG) == 0U);

	host_reset_state();
	host_set_snapshot(30U, 0U);
	host_init_with_voltage(3000U, 3000U);
	host_run_seconds(5U, 3000U, 3000U, 0U, 140U);
	CHECK_EQ_U32(host_internal_soc(), 29U);
}

static void test_long_rest_is_downward_only_and_requires_runtime_voltage_gate(void)
{
	if (PROJECT_CFG_SOC_BOARD_SELF_CONSUMPTION_MA > 100U)
	{
		return;
	}

	host_reset_state();
	host_set_snapshot(80U, 0U);
	host_init_with_voltage(3695U, 3695U);
	host_run_seconds((UINT16)(PROJECT_CFG_SOC_REST_OCV_SECONDS +
		PROJECT_CFG_SOC_REST_DOWN_STEP_SECONDS), 3695U, 3695U, 0U, 0U);
	CHECK_EQ_U32(host_internal_soc(), 79U);

	host_reset_state();
	host_set_snapshot(40U, 0U);
	host_init_with_voltage(3695U, 3695U);
	host_run_seconds((UINT16)(PROJECT_CFG_SOC_REST_OCV_SECONDS +
		PROJECT_CFG_SOC_REST_DOWN_STEP_SECONDS + 60U), 3695U, 3695U, 0U, 0U);
	CHECK_EQ_U32(host_internal_soc(), 40U);

	host_reset_state();
	host_set_snapshot(52U, 0U);
	host_init_with_voltage(3695U, 3695U);
	host_run_seconds((UINT16)(PROJECT_CFG_SOC_REST_OCV_SECONDS +
		PROJECT_CFG_SOC_REST_DOWN_STEP_SECONDS + 60U), 3695U, 3695U, 0U, 0U);
	CHECK_EQ_U32(host_internal_soc(), 52U);

	host_reset_state();
	host_set_snapshot(95U, 0U);
	host_init_with_voltage(4050U, 4050U);
	host_run_seconds((UINT16)(PROJECT_CFG_SOC_REST_OCV_SECONDS +
		PROJECT_CFG_SOC_REST_DOWN_STEP_SECONDS + 60U), 4050U, 4050U, 0U, 0U);
	CHECK_EQ_U32(host_internal_soc(), 95U);
}

static void test_rtc_rest_can_down_calibrate_above_runtime_3700mv_gate(void)
{
	UINT32 seconds;

	host_reset_state();
	host_set_snapshot(95U, 0U);
	host_init_with_voltage(4050U, 4050U);
	for (seconds = 60U;
		 seconds <= (UINT32)PROJECT_CFG_SOC_REST_OCV_SECONDS +
			(UINT32)PROJECT_CFG_SOC_REST_DOWN_STEP_SECONDS;
		 seconds += 60U)
	{
		SOC_ApplyRtcRelaxationCompensation(seconds, 4050U, 4050U);
	}
	CHECK_EQ_U32(host_internal_soc(), 94U);
}

static void test_rtc_new_session_keeps_fired_latch_without_active_current(void)
{
	UINT32 seconds;
	UINT32 session_seconds = (UINT32)PROJECT_CFG_SOC_REST_OCV_SECONDS +
		(UINT32)PROJECT_CFG_SOC_REST_DOWN_STEP_SECONDS;

	host_reset_state();
	host_set_snapshot(95U, 0U);
	host_init_with_voltage(4050U, 4050U);
	for (seconds = 10U; seconds <= session_seconds; seconds += 10U)
	{
		SOC_ApplyRtcRelaxationCompensation(seconds, 4050U, 4050U);
	}
	CHECK_EQ_U32(host_internal_soc(), 94U);

	/* A smaller cumulative value starts another RTC session but does not clear rest_ocv_fired. */
	for (seconds = 10U; seconds <= session_seconds; seconds += 10U)
	{
		SOC_ApplyRtcRelaxationCompensation(seconds, 4050U, 4050U);
	}
	CHECK_EQ_U32(host_internal_soc(), 94U);
}

static void test_sleep_snapshot_currently_skips_subpercent_capacity_change(void)
{
	UINT16 cap_before;
	UINT16 cap_after_run;
	UINT32 flash_cap_before;
	UINT32 saves_before_sleep;

	host_reset_state();
	host_set_snapshot(50U, 0U);
	host_init_with_voltage(3835U, 3835U);
	cap_before = g_stCellInfoReport.SocElement.u16CapacityNow;
	flash_cap_before = s_flash_soc.u32CapNow;
	host_run_seconds(36U, 3835U, 3835U, 0U, 100U);
	cap_after_run = g_stCellInfoReport.SocElement.u16CapacityNow;
	CHECK_TRUE(cap_after_run < cap_before);
	CHECK_EQ_U32(g_stCellInfoReport.SocElement.u16Soc, 50U);
	CHECK_EQ_U32(s_flash_soc.u32CapNow, flash_cap_before);

	saves_before_sleep = s_flash_save_count;
	SOC_SaveSnapshotBeforeSleep();
	CHECK_EQ_U32(s_flash_save_count, saves_before_sleep);
	CHECK_EQ_U32(s_flash_soc.u32CapNow, flash_cap_before);

	InitData_SOC();
	CHECK_EQ_U32(g_stCellInfoReport.SocElement.u16CapacityNow, cap_before);
}

static void test_soc_zero_extra_discharge_debt_is_not_tracked_on_recharge(void)
{
	host_reset_state();
	host_set_snapshot(100U, 0U);
	host_init_with_voltage(4050U, 4050U);
	host_run_seconds(360U, 4050U, 4050U, 0U,
		(UINT16)(HOST_CAP_USABLE_AS10 / 360U));
	CHECK_EQ_U32(host_internal_soc(), 0U);

	/* Discharge another 1Ah after visible SOC is already zero. */
	host_run_seconds(360U, 4050U, 4050U, 0U, 100U);
	CHECK_EQ_U32(host_internal_soc(), 0U);

	/* The extra deficit is saturated away, so 1Ah recharge immediately raises visible SOC. */
	host_run_seconds(360U, 3800U, 3800U, 100U, 0U);
	CHECK_RANGE_U32(host_internal_soc(), 3U, 4U);
}

int main(void)
{
#define RUN_TEST(fn) do { \
	unsigned failures_before = s_failures; \
	fn(); \
	++s_tests_run; \
	printf("%s %s\n", (s_failures == failures_before) ? "PASS" : "FAIL", #fn); \
} while (0)

	RUN_TEST(test_startup_ocv_uses_real_c_code);
	RUN_TEST(test_startup_without_loaded_voltage_defaults_to_60);
	RUN_TEST(test_deep_sleep_wake_ocv_replaces_only_large_valid_difference);
	RUN_TEST(test_discharge_integration_uses_app_soc_path);
	RUN_TEST(test_board_self_consumption_integrates_during_relax);
	RUN_TEST(test_board_self_consumption_works_at_high_non_full_voltage);
	RUN_TEST(test_full_voltage_anchor_can_override_self_consumption);
	RUN_TEST(test_full_voltage_holds_100_during_long_zero_current_idle);
	RUN_TEST(test_rtc_sleep_does_not_apply_board_self_consumption);
	RUN_TEST(test_board_self_consumption_adjusts_charge_and_discharge_current);
	RUN_TEST(test_typec_output_current_converts_to_battery_equivalent);
	RUN_TEST(test_full_confirm_reaches_100_only_after_voltage_anchor);
	RUN_TEST(test_low_voltage_tail_reaches_zero);
	RUN_TEST(test_short_rest_ocv_ignores_upward_target_during_charge);
	RUN_TEST(test_short_rest_ocv_is_not_consumed_during_active_discharge);
	RUN_TEST(test_rtc_ocv_ignores_upward_stable_target);
	RUN_TEST(test_rtc_ocv_waits_for_voltage_convergence);
	RUN_TEST(test_unstable_long_rest_waits_for_voltage_convergence);
	RUN_TEST(test_long_rest_ocv_slowly_reduces_soc_above_low_tail);
	RUN_TEST(test_rebound_flag_clears_when_holdoff_expires);
	RUN_TEST(test_set_soc_once_command_saves_snapshot);
	RUN_TEST(test_reserved_capacity_keeps_reported_full_and_reaches_zero_early);
	RUN_TEST(test_legacy_snapshot_migrates_by_saved_soc);
	RUN_TEST(test_no_new_afe_sample_never_repeats_integration);
	RUN_TEST(test_typec_equivalent_current_uses_pack_voltage_and_fallback);
	RUN_TEST(test_reported_capacity_maps_usable_domain_back_to_full_domain);
	RUN_TEST(test_soh_cycle_mapping_and_capacity_floor);
	RUN_TEST(test_full_confirm_voltage_delta_mode_and_counter_boundaries);
	RUN_TEST(test_full_confirm_steps_one_percent_per_window);
	RUN_TEST(test_empty_tail_critical_relax_and_dynamic_rate_boundaries);
	RUN_TEST(test_sag_hold_blocks_soft_tail_but_not_critical_empty_tail);
	RUN_TEST(test_long_rest_is_downward_only_and_requires_runtime_voltage_gate);
	RUN_TEST(test_rtc_rest_can_down_calibrate_above_runtime_3700mv_gate);
	RUN_TEST(test_rtc_new_session_keeps_fired_latch_without_active_current);
	RUN_TEST(test_sleep_snapshot_currently_skips_subpercent_capacity_change);
	RUN_TEST(test_soc_zero_extra_discharge_debt_is_not_tracked_on_recharge);

	if (s_failures != 0U)
	{
		printf("SOC host C tests failed: tests=%u failures=%u\n", s_tests_run, s_failures);
		return 1;
	}
	printf("SOC host C tests passed: %u\n", s_tests_run);
	return 0;
}
