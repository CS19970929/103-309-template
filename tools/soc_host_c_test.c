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
#define HOST_SHORT_REST_STEP_SECONDS ((UINT16)PROJECT_CFG_SOC_REST_TARGET_STEP_SECONDS)
#define HOST_LONG_REST_DOWN_STEP_SECONDS ((UINT16)PROJECT_CFG_SOC_REST_DOWN_STEP_SECONDS)
#define HOST_REST_DOWN_START_SECONDS \
	(((UINT16)PROJECT_CFG_SOC_REST_OCV_SECONDS > HOST_SHORT_REST_STEP_SECONDS) ? \
	 (UINT16)PROJECT_CFG_SOC_REST_OCV_SECONDS : HOST_SHORT_REST_STEP_SECONDS)

struct OTHER_ELEMENT OtherElement;
struct stCell_Info g_stCellInfoReport;
volatile struct SYSTEM_ERROR System_ErrFlag;
volatile union System_Function_StartUp System_Func_StartUp;
volatile union System_OnOFF_Function System_OnOFF_Func_StartUpRec;
volatile union System_OnOFF_Function System_OnOFF_Func;
volatile union System_Status SystemStatus;

UINT8 SeriesNum = 10U;
UINT16 g_u16TypeCOutCurrent_mA;
UINT16 g_u16TypeCOutCurrent_A10;
UINT16 g_u16TypeCBatEquivCurrent_mA;
UINT16 g_u16TypeCBatEquivCurrent_A10;
UINT32 g_u32Vbat_mV;
UINT32 g_u32AfeCurrentSampleSeq;

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

static UINT16 host_cap_to_ah100(UINT32 cap_as10)
{
	return (UINT16)((cap_as10 + 180U) / 360U);
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
	memset((void *)&System_Func_StartUp, 0, sizeof(System_Func_StartUp));
	memset((void *)&System_OnOFF_Func_StartUpRec, 0, sizeof(System_OnOFF_Func_StartUpRec));
	memset((void *)&System_OnOFF_Func, 0, sizeof(System_OnOFF_Func));
	memset((void *)&SystemStatus, 0, sizeof(SystemStatus));
	memset(&SOC_Enhance_Element, 0, sizeof(SOC_Enhance_Element));
	memset(&s_flash_soc, 0, sizeof(s_flash_soc));
	s_flash_soc_valid = 0U;
	g_u16TypeCOutCurrent_mA = 0U;
	g_u16TypeCOutCurrent_A10 = 0U;
	g_u16TypeCBatEquivCurrent_mA = 0U;
	g_u16TypeCBatEquivCurrent_A10 = 0U;
	g_u32Vbat_mV = 0U;
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
	g_stCellInfoReport.u16Ichg = ichg;
	g_stCellInfoReport.u16IDischg = idsg;
	++g_u32AfeCurrentSampleSeq;
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
#if PROJECT_CFG_DEBUG_WATCH_ENABLE
	CHECK_TRUE(g_dbg_soc_watch != 0);
	CHECK_EQ_U32(g_dbg_soc_watch->u8InternalSoc, 70U);
	CHECK_EQ_U32(g_dbg_soc_watch->u8DisplaySoc, 70U);
	CHECK_EQ_U32(g_dbg_soc_watch->u8LastCalibSource, SOC_WATCH_CALIB_STARTUP_OCV);
#endif
}

static void test_discharge_integration_uses_app_soc_path(void)
{
	host_reset_state();
	host_set_snapshot(60U, 0U);
	host_init_with_voltage(3835U, 3835U);
	host_run_seconds(360U, 3835U, 3835U, 0U, 270U);
	CHECK_RANGE_U32(host_internal_soc(), 49U, 51U);
	CHECK_TRUE(g_stCellInfoReport.SocElement.u16Soc >= host_internal_soc());
}

static void test_board_self_consumption_integrates_during_relax(void)
{
	UINT32 start_cap_as10 = host_cap_now_from_soc(70U);
	UINT32 expected_delta_as10 = ((UINT32)PROJECT_CFG_SOC_BOARD_SELF_CONSUMPTION_MA *
		3600U * 1000U) / HOST_MAMS_PER_AS10;

	host_reset_state();
	host_set_snapshot(70U, 0U);
	host_init_with_voltage(3835U, 3835U);
	host_run_seconds(3600U, 3835U, 3835U, 0U, 0U);
	CHECK_EQ_U32(SOC_Enhance_Element.u16_Idsg, 0U);
	CHECK_EQ_U32(SOC_Enhance_Element.u16_CapacityNow,
		host_cap_to_ah100(start_cap_as10 - expected_delta_as10));
	CHECK_EQ_U32(host_internal_soc(), 70U);
#if PROJECT_CFG_DEBUG_WATCH_ENABLE
	CHECK_EQ_U32(g_dbg_soc_watch->u8Mode, 0U);
#endif
}

static void test_typec_output_current_converts_to_battery_equivalent(void)
{
	host_reset_state();
	host_set_snapshot(60U, 0U);
	host_init_with_voltage(3835U, 3835U);
	g_u16TypeCOutCurrent_mA = 9000U;
	host_run_seconds(360U, 3835U, 3835U, 23U, 0U);
	CHECK_EQ_U32(host_internal_soc(), 60U);
	CHECK_RANGE_U32(g_u16TypeCBatEquivCurrent_A10, 23U, 24U);

	g_u16TypeCOutCurrent_mA = 0U;
	host_run_seconds(360U, 3835U, 3835U, 23U, 0U);
	CHECK_RANGE_U32(host_internal_soc(), 61U, 62U);
}

static void test_full_confirm_reaches_100_only_after_voltage_anchor(void)
{
	host_reset_state();
	host_set_snapshot(98U, 0U);
	host_init_with_voltage(4100U, 4050U);
	host_run_seconds(900U, 4100U, 4050U, 270U, 0U);
	CHECK_EQ_U32(host_internal_soc(), 99U);

	host_run_seconds(15U, 4180U, 4100U, 270U, 0U);
	CHECK_EQ_U32(host_internal_soc(), 100U);
}

static void test_low_voltage_tail_reaches_zero(void)
{
	host_reset_state();
	host_set_snapshot(30U, 0U);
	host_init_with_voltage(3000U, 3000U);
	host_run_seconds(6U, 2950U, 2950U, 0U, 145U);
	CHECK_EQ_U32(host_internal_soc(), 0U);
}

static void test_short_rest_ocv_defers_until_active_charge(void)
{
	host_reset_state();
	host_set_snapshot(50U, 0U);
	host_init_with_voltage(3835U, 3835U);
	host_run_seconds(599U, 3835U, 3835U, 0U, 0U);
	CHECK_EQ_U32(host_internal_soc(), 50U);
	host_run_seconds(1U, 3835U, 3835U, 0U, 0U);
	CHECK_EQ_U32(host_internal_soc(), 50U);
	host_run_seconds(600U, 3835U, 3835U, 4U, 0U);
	CHECK_EQ_U32(host_internal_soc(), 51U);
}

static void test_short_rest_ocv_defers_until_active_discharge(void)
{
	host_reset_state();
	host_set_snapshot(80U, 0U);
	host_init_with_voltage(3835U, 3835U);
	host_run_seconds(600U, 3835U, 3835U, 0U, 0U);
	CHECK_EQ_U32(host_internal_soc(), 80U);
	host_run_seconds(600U, 3835U, 3835U, 0U, 4U);
	CHECK_EQ_U32(host_internal_soc(), 79U);
}

static void test_rtc_ocv_uses_stable_target_and_active_charge(void)
{
	UINT32 seconds;

	host_reset_state();
	host_set_snapshot(50U, 0U);
	host_init_with_voltage(3835U, 3835U);

	for (seconds = 10U; seconds < 610U; seconds += 10U)
	{
		SOC_ApplyRtcRelaxationCompensation(seconds, 3835U, 3835U);
	}
	CHECK_EQ_U32(host_internal_soc(), 50U);

	SOC_ApplyRtcRelaxationCompensation(610U, 3835U, 3835U);
	CHECK_EQ_U32(host_internal_soc(), 50U);
	host_run_seconds(600U, 3835U, 3835U, 4U, 0U);
	CHECK_EQ_U32(host_internal_soc(), 51U);
}

static void test_rtc_ocv_waits_for_voltage_convergence(void)
{
	UINT32 seconds;
	UINT16 vcell;

	host_reset_state();
	host_set_snapshot(50U, 0U);
	host_init_with_voltage(3835U, 3835U);

	for (seconds = 60U; seconds <= 600U; seconds += 60U)
	{
		vcell = (((seconds / 60U) & 1U) == 0U) ? 3770U : 3835U;
		SOC_ApplyRtcRelaxationCompensation(seconds, vcell, vcell);
	}
	CHECK_EQ_U32(host_internal_soc(), 50U);

	for (seconds = 660U; seconds < 1260U; seconds += 60U)
	{
		SOC_ApplyRtcRelaxationCompensation(seconds, 3835U, 3835U);
	}
	CHECK_EQ_U32(host_internal_soc(), 50U);

	SOC_ApplyRtcRelaxationCompensation(1260U, 3835U, 3835U);
	CHECK_EQ_U32(host_internal_soc(), 50U);
	host_run_seconds(600U, 3835U, 3835U, 4U, 0U);
	CHECK_EQ_U32(host_internal_soc(), 51U);
}

static void test_unstable_long_rest_waits_for_voltage_convergence(void)
{
	UINT16 i;
	UINT16 vcell;

	host_reset_state();
	host_set_snapshot(50U, 0U);
	host_init_with_voltage(3835U, 3835U);
	for (i = 0U; i < 9U; ++i)
	{
		vcell = ((i & 1U) == 0U) ? 3835U : 3770U;
		host_run_seconds(200U, vcell, vcell, 0U, 0U);
	}
	CHECK_EQ_U32(host_internal_soc(), 50U);

	host_run_seconds(399U, 3835U, 3835U, 0U, 0U);
	CHECK_EQ_U32(host_internal_soc(), 50U);
	host_run_seconds(2U, 3835U, 3835U, 0U, 0U);
	CHECK_EQ_U32(host_internal_soc(), 50U);
}

static void test_long_rest_ocv_slowly_reduces_soc_above_low_tail(void)
{
	UINT32 wait_seconds = (UINT32)HOST_REST_DOWN_START_SECONDS +
		(UINT32)HOST_LONG_REST_DOWN_STEP_SECONDS;

	host_reset_state();
	host_set_snapshot(80U, 0U);
	host_init_with_voltage(3725U, 3725U);
	host_run_seconds((UINT16)(wait_seconds - 1U), 3725U, 3725U, 0U, 0U);
	CHECK_EQ_U32(host_internal_soc(), 80U);
	host_run_seconds(1U, 3725U, 3725U, 0U, 0U);
	CHECK_EQ_U32(host_internal_soc(), 79U);
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
	CHECK_EQ_U32(host_internal_soc(), 79U);
}

static void test_display_overlays_do_not_modify_internal_soc(void)
{
	host_reset_state();
	host_set_snapshot(72U, 0U);
	host_init_with_voltage(3835U, 3835U);

	System_OnOFF_Func.bits.b1OnOFF_SOC_Fixed = 1U;
	host_tick(3835U, 3835U, 0U, 0U);
	CHECK_EQ_U32(g_stCellInfoReport.SocElement.u16Soc, 60U);
	CHECK_EQ_U32(host_internal_soc(), 72U);

	System_OnOFF_Func.bits.b1OnOFF_SOC_Fixed = 0U;
	System_OnOFF_Func.bits.b1OnOFF_SOC_Zero = 1U;
	host_tick(3835U, 3835U, 0U, 0U);
	CHECK_EQ_U32(g_stCellInfoReport.SocElement.u16Soc, 0U);
	CHECK_EQ_U32(host_internal_soc(), 72U);
}

static void test_set_soc_once_command_saves_snapshot(void)
{
	host_reset_state();
	host_set_snapshot(72U, 0U);
	host_init_with_voltage(3835U, 3835U);
	SOC_Enhance_Element.u8_SetSocOnce = 35U;
	SOC_Enhance_Element.u16_RefreshData_Flag = 3U;
	host_tick(3835U, 3835U, 0U, 0U);
	CHECK_EQ_U32(host_internal_soc(), 35U);
	CHECK_EQ_U32(g_stCellInfoReport.SocElement.u16Soc, 35U);
}

int main(void)
{
	test_startup_ocv_uses_real_c_code();
	test_discharge_integration_uses_app_soc_path();
	test_board_self_consumption_integrates_during_relax();
	test_typec_output_current_converts_to_battery_equivalent();
	test_full_confirm_reaches_100_only_after_voltage_anchor();
	test_low_voltage_tail_reaches_zero();
	test_short_rest_ocv_defers_until_active_charge();
	test_short_rest_ocv_defers_until_active_discharge();
	test_rtc_ocv_uses_stable_target_and_active_charge();
	test_rtc_ocv_waits_for_voltage_convergence();
	test_unstable_long_rest_waits_for_voltage_convergence();
	test_long_rest_ocv_slowly_reduces_soc_above_low_tail();
	test_rebound_flag_clears_when_holdoff_expires();
	test_display_overlays_do_not_modify_internal_soc();
	test_set_soc_once_command_saves_snapshot();

	if (s_failures != 0U)
	{
		printf("SOC host C tests failed: %u\n", s_failures);
		return 1;
	}
	printf("SOC host C tests passed: 15\n");
	return 0;
}
