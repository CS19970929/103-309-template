#include "main.h"
#include "Flash.h"
#include "SocEnhance.h"

#include <stdio.h>
#include <string.h>

#define HOST_CAP_A10               ((UINT16)270U)
#define HOST_CAP_FACTORY_AS10      ((UINT32)HOST_CAP_A10 * 3600U)
#define HOST_REBOUND_FLAG          ((UINT16)0x0001U)
#define HOST_TICKS_PER_SECOND      ((UINT16)5U)
#define HOST_MAMS_PER_AS10         ((UINT32)100000U)
#define HOST_LONG_REST_DOWN_STEP_SECONDS ((UINT16)PROJECT_CFG_SOC_REST_DOWN_STEP_SECONDS)
#define HOST_REST_DOWN_START_SECONDS ((UINT16)PROJECT_CFG_SOC_REST_OCV_SECONDS)

struct OTHER_ELEMENT OtherElement;
struct stCell_Info g_stCellInfoReport;
volatile struct SYSTEM_ERROR System_ErrFlag;

UINT8 SeriesNum = 10U;
static UINT32 s_host_vbat_mV;
static UINT32 s_host_afe_current_sample_seq;

static STORAGE_FLASH_SOC_DATA s_flash_soc;
static UINT8 s_flash_soc_valid;
static unsigned s_failures;

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
	return 1U;
}

UINT8 System_ERROR_UserCallback(enum SYSTEM_ERROR_COMMAND errorCode)
{
	(void)errorCode;
	return 0U;
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
	return (UINT32)(((uint64_t)HOST_CAP_FACTORY_AS10 * (uint64_t)soc) / 100ULL);
}

static UINT16 host_soc_from_cap(UINT32 cap_as10)
{
	UINT32 soc;

	if (cap_as10 >= HOST_CAP_FACTORY_AS10)
	{
		return 100U;
	}
	soc = (UINT32)(((uint64_t)cap_as10 * 100ULL +
		(HOST_CAP_FACTORY_AS10 / 2U)) / HOST_CAP_FACTORY_AS10);
	return (soc > 100U) ? 100U : (UINT16)soc;
}

static UINT16 host_cap_to_ah100(UINT32 cap_as10)
{
	return (UINT16)((cap_as10 + 180U) / 360U);
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
	s_flash_soc.u32CapFull = HOST_CAP_FACTORY_AS10;
	s_flash_soc.u32CapNow = host_cap_now_from_soc(soc);
	s_flash_soc.u32LearnPassedAs10 = 0U;
	s_flash_soc.u16Flags = flags;
	s_flash_soc_valid = 1U;
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
		host_cap_to_ah100(expected_cap_as10));
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
		host_cap_to_ah100(expected_cap_as10));
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

static void test_rtc_sleep_does_not_apply_board_self_consumption(void)
{
	UINT32 start_cap_as10 = host_cap_now_from_soc(80U);

	host_reset_state();
	host_set_snapshot(80U, 0U);
	host_init_with_voltage(4050U, 4050U);
	SOC_ApplyRtcRelaxationCompensation(3600U, 4050U, 4050U);
	CHECK_EQ_U32(g_stCellInfoReport.SocElement.u16CapacityNow,
		host_cap_to_ah100(start_cap_as10));
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
	CHECK_RANGE_U32(host_internal_soc(), 28U, 29U);
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
	CHECK_EQ_U32(host_internal_soc(), before_discharge);
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

int main(void)
{
	test_startup_ocv_uses_real_c_code();
	test_discharge_integration_uses_app_soc_path();
	test_board_self_consumption_integrates_during_relax();
	test_board_self_consumption_works_at_high_non_full_voltage();
	test_full_voltage_anchor_can_override_self_consumption();
	test_rtc_sleep_does_not_apply_board_self_consumption();
	test_board_self_consumption_adjusts_charge_and_discharge_current();
	test_full_confirm_reaches_100_only_after_voltage_anchor();
	test_low_voltage_tail_reaches_zero();
	test_short_rest_ocv_ignores_upward_target_during_charge();
	test_short_rest_ocv_is_not_consumed_during_active_discharge();
	test_rtc_ocv_ignores_upward_stable_target();
	test_rtc_ocv_waits_for_voltage_convergence();
	test_unstable_long_rest_waits_for_voltage_convergence();
	test_long_rest_ocv_slowly_reduces_soc_above_low_tail();
	test_rebound_flag_clears_when_holdoff_expires();
	test_set_soc_once_command_saves_snapshot();

	if (s_failures != 0U)
	{
		printf("SOC host C tests failed: %u\n", s_failures);
		return 1;
	}
	printf("SOC host C tests passed: 18\n");
	return 0;
}
