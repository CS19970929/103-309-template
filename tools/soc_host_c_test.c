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
#define HOST_SOC_BKP_MAGIC         ((UINT16)0x5C0CU)
#define HOST_SOC_BKP_VERSION       ((UINT16)0x0001U)
#define HOST_SOC_BKP_CRC_SEED      ((UINT16)0xA55AU)

struct OTHER_ELEMENT OtherElement;
struct stCell_Info g_stCellInfoReport;
volatile struct SYSTEM_ERROR System_ErrFlag;

UINT8 SeriesNum = 10U;
static UINT16 s_host_typec_out_current_mA;
static UINT32 s_host_vbat_mV;
static UINT32 s_host_afe_current_sample_seq;

static STORAGE_FLASH_SOC_DATA s_flash_soc;
static UINT8 s_flash_soc_valid;
static UINT16 s_bkp_regs[11];
static unsigned s_flash_soc_load_count;
static unsigned s_flash_soc_save_count;
static unsigned s_bkp_write_count;
static unsigned s_failures;
static unsigned s_tests_run;

#define CHECK_TRUE(expr) host_check((expr) ? 1 : 0, #expr, __LINE__)
#define CHECK_EQ_U32(actual, expected) host_check_u32((UINT32)(actual), (UINT32)(expected), #actual, __LINE__)
#define CHECK_RANGE_U32(actual, min_v, max_v) host_check_range_u32((UINT32)(actual), (UINT32)(min_v), (UINT32)(max_v), #actual, __LINE__)
#define RUN_TEST(fn) do { ++s_tests_run; fn(); } while (0)

UINT8 StorageFlash_LoadSocData(STORAGE_FLASH_SOC_DATA *data)
{
	++s_flash_soc_load_count;
	if ((data == 0) || !s_flash_soc_valid)
	{
		return 0U;
	}
	*data = s_flash_soc;
	return 1U;
}

UINT8 StorageFlash_SaveSocData(const STORAGE_FLASH_SOC_DATA *data)
{
	++s_flash_soc_save_count;
	if (data == 0)
	{
		return 0U;
	}
	s_flash_soc = *data;
	s_flash_soc_valid = 1U;
	return 1U;
}

void RCC_APB1PeriphClockCmd(uint32_t RCC_APB1Periph, FunctionalState NewState)
{
	(void)RCC_APB1Periph;
	(void)NewState;
}

void PWR_BackupAccessCmd(FunctionalState NewState)
{
	(void)NewState;
}

static UINT16 host_bkp_index(uint16_t bkp_dr)
{
	return (UINT16)(bkp_dr >> 2U);
}

void BKP_WriteBackupRegister(uint16_t BKP_DR, uint16_t Data)
{
	UINT16 index = host_bkp_index(BKP_DR);

	if (index < (UINT16)(sizeof(s_bkp_regs) / sizeof(s_bkp_regs[0])))
	{
		s_bkp_regs[index] = Data;
		++s_bkp_write_count;
	}
}

uint16_t BKP_ReadBackupRegister(uint16_t BKP_DR)
{
	UINT16 index = host_bkp_index(BKP_DR);

	if (index < (UINT16)(sizeof(s_bkp_regs) / sizeof(s_bkp_regs[0])))
	{
		return s_bkp_regs[index];
	}
	return 0U;
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

static UINT16 host_bkp_crc_update(UINT16 crc, UINT16 data)
{
	UINT8 i;

	crc ^= data;
	for (i = 0U; i < 16U; ++i)
	{
		if ((crc & 0x0001U) != 0U)
		{
			crc = (UINT16)((crc >> 1U) ^ 0xA001U);
		}
		else
		{
			crc = (UINT16)(crc >> 1U);
		}
	}
	return crc;
}

static UINT16 host_bkp_crc_calc(UINT16 soc_flags,
								UINT32 cycle_x100,
								UINT32 cap_now_as10,
								UINT32 dsg_acc_as10,
								UINT32 cap_factory_as10)
{
	UINT16 crc = HOST_SOC_BKP_CRC_SEED;

	crc = host_bkp_crc_update(crc, HOST_SOC_BKP_MAGIC);
	crc = host_bkp_crc_update(crc, HOST_SOC_BKP_VERSION);
	crc = host_bkp_crc_update(crc, soc_flags);
	crc = host_bkp_crc_update(crc, (UINT16)(cycle_x100 & 0xFFFFU));
	crc = host_bkp_crc_update(crc, (UINT16)(cycle_x100 >> 16U));
	crc = host_bkp_crc_update(crc, (UINT16)(cap_now_as10 & 0xFFFFU));
	crc = host_bkp_crc_update(crc, (UINT16)(cap_now_as10 >> 16U));
	crc = host_bkp_crc_update(crc, (UINT16)(dsg_acc_as10 & 0xFFFFU));
	crc = host_bkp_crc_update(crc, (UINT16)(dsg_acc_as10 >> 16U));
	crc = host_bkp_crc_update(crc, (UINT16)(cap_factory_as10 & 0xFFFFU));
	crc = host_bkp_crc_update(crc, (UINT16)(cap_factory_as10 >> 16U));
	return crc;
}

static void host_write_bkp_snapshot(UINT16 soc,
									UINT16 flags,
									UINT32 cycle_x100,
									UINT32 cap_now_as10,
									UINT32 dsg_acc_as10)
{
	UINT16 soc_flags = (UINT16)(((flags & 0x00FFU) << 8U) | (soc & 0x00FFU));
	UINT16 crc = host_bkp_crc_calc(soc_flags,
								   cycle_x100,
								   cap_now_as10,
								   dsg_acc_as10,
								   HOST_CAP_FACTORY_AS10);

	BKP_WriteBackupRegister(BKP_DR2, (UINT16)(~HOST_SOC_BKP_MAGIC));
	BKP_WriteBackupRegister(BKP_DR3, soc_flags);
	BKP_WriteBackupRegister(BKP_DR4, (UINT16)(cycle_x100 & 0xFFFFU));
	BKP_WriteBackupRegister(BKP_DR5, (UINT16)(cycle_x100 >> 16U));
	BKP_WriteBackupRegister(BKP_DR6, (UINT16)(cap_now_as10 & 0xFFFFU));
	BKP_WriteBackupRegister(BKP_DR7, (UINT16)(cap_now_as10 >> 16U));
	BKP_WriteBackupRegister(BKP_DR8, (UINT16)(dsg_acc_as10 & 0xFFFFU));
	BKP_WriteBackupRegister(BKP_DR9, (UINT16)(dsg_acc_as10 >> 16U));
	BKP_WriteBackupRegister(BKP_DR10, crc);
	BKP_WriteBackupRegister(BKP_DR1, HOST_SOC_BKP_MAGIC);
}

static UINT8 host_read_bkp_snapshot(UINT16 *soc, UINT16 *flags)
{
	UINT16 soc_flags;
	UINT16 crc;
	UINT32 cycle_x100;
	UINT32 cap_now_as10;
	UINT32 dsg_acc_as10;

	if (BKP_ReadBackupRegister(BKP_DR1) != HOST_SOC_BKP_MAGIC)
	{
		return 0U;
	}
	if (BKP_ReadBackupRegister(BKP_DR2) != (UINT16)(~HOST_SOC_BKP_MAGIC))
	{
		return 0U;
	}
	soc_flags = BKP_ReadBackupRegister(BKP_DR3);
	cycle_x100 = (UINT32)BKP_ReadBackupRegister(BKP_DR4) |
		((UINT32)BKP_ReadBackupRegister(BKP_DR5) << 16U);
	cap_now_as10 = (UINT32)BKP_ReadBackupRegister(BKP_DR6) |
		((UINT32)BKP_ReadBackupRegister(BKP_DR7) << 16U);
	dsg_acc_as10 = (UINT32)BKP_ReadBackupRegister(BKP_DR8) |
		((UINT32)BKP_ReadBackupRegister(BKP_DR9) << 16U);
	crc = host_bkp_crc_calc(soc_flags,
							cycle_x100,
							cap_now_as10,
							dsg_acc_as10,
							HOST_CAP_FACTORY_AS10);
	if (crc != BKP_ReadBackupRegister(BKP_DR10))
	{
		return 0U;
	}
	if ((soc_flags & 0x00FFU) > 100U)
	{
		return 0U;
	}
	if (soc != 0)
	{
		if (((cap_now_as10 != 0U) || ((soc_flags & 0x00FFU) == 0U)) &&
			(cap_now_as10 <= HOST_CAP_FACTORY_AS10))
		{
			*soc = host_soc_from_cap(cap_now_as10);
		}
		else
		{
			*soc = (UINT16)(soc_flags & 0x00FFU);
		}
	}
	if (flags != 0)
	{
		*flags = (UINT16)((soc_flags >> 8U) & HOST_REBOUND_FLAG);
	}
	return 1U;
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
	memset(s_bkp_regs, 0, sizeof(s_bkp_regs));
	s_flash_soc_valid = 0U;
	s_flash_soc_load_count = 0U;
	s_flash_soc_save_count = 0U;
	s_bkp_write_count = 0U;
	s_host_typec_out_current_mA = 0U;
	s_host_vbat_mV = 0U;
	s_host_afe_current_sample_seq = 0U;
	host_apply_default_config();
}

static void host_reset_runtime_keep_storage(void)
{
	memset(&g_stCellInfoReport, 0, sizeof(g_stCellInfoReport));
	memset((void *)&System_ErrFlag, 0, sizeof(System_ErrFlag));
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
	s_flash_soc.u32CapFull = HOST_CAP_FACTORY_AS10;
	s_flash_soc.u32CapNow = host_cap_now_from_soc(soc);
	s_flash_soc.u32LearnPassedAs10 = 0U;
	s_flash_soc.u16Flags = flags;
	s_flash_soc_valid = 1U;
	host_write_bkp_snapshot(soc, flags, s_flash_soc.u32CycleTimes,
		s_flash_soc.u32CapNow, s_flash_soc.u32LearnPassedAs10);
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
	UINT16 soc;

	if (host_read_bkp_snapshot(&soc, 0))
	{
		return soc;
	}
	return s_flash_soc.u16SocNow;
}

static UINT16 host_snapshot_flags(void)
{
	UINT16 flags;

	if (host_read_bkp_snapshot(0, &flags))
	{
		return flags;
	}
	return s_flash_soc.u16Flags;
}

static void test_startup_ocv_uses_real_c_code(void)
{
	host_reset_state();
	host_init_with_voltage(3835U, 3835U);
#if (PROJECT_CFG_BAT_CHEMISTRY == 1)
	CHECK_EQ_U32(g_stCellInfoReport.SocElement.u16Soc, 100U);
	CHECK_EQ_U32(host_internal_soc(), 100U);
#else
	CHECK_EQ_U32(g_stCellInfoReport.SocElement.u16Soc, 70U);
	CHECK_EQ_U32(host_internal_soc(), 70U);
#endif
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
	CHECK_RANGE_U32(host_internal_soc(),
		(UINT32)(host_soc_from_cap(expected_cap_as10) - 1U),
		(UINT32)(host_soc_from_cap(expected_cap_as10) + 1U));

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
	CHECK_EQ_U32(host_internal_soc(), 100U);

	host_run_seconds(15U, 4181U, 4100U, 270U, 0U);
	CHECK_EQ_U32(host_internal_soc(), 100U);
}

static void test_low_voltage_tail_reaches_zero(void)
{
	host_reset_state();
	host_set_snapshot(30U, 0U);
	host_init_with_voltage(3000U, 3000U);
	host_run_seconds(60U, 2950U, 2950U, 0U, 145U);
#if (PROJECT_CFG_BAT_CHEMISTRY == 1)
	CHECK_RANGE_U32(host_internal_soc(), 15U, 29U);
#else
	CHECK_RANGE_U32(host_internal_soc(), 28U, 29U);
#endif
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
	CHECK_TRUE((host_snapshot_flags() & HOST_REBOUND_FLAG) != 0U);

	host_run_seconds(1U, 3500U, 3500U, 0U, 0U);
	CHECK_TRUE((host_snapshot_flags() & HOST_REBOUND_FLAG) == 0U);

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

static void test_bkp_snapshot_recovers_after_runtime_reset(void)
{
	host_reset_state();
	host_set_snapshot(72U, 0U);
	host_init_with_voltage(3835U, 3835U);
	SOC_RequestSetOnce(42U);
	CHECK_EQ_U32(host_internal_soc(), 42U);

	host_reset_runtime_keep_storage();
	host_init_with_voltage(3835U, 3835U);
	CHECK_EQ_U32(g_stCellInfoReport.SocElement.u16Soc, 42U);
	CHECK_EQ_U32(host_internal_soc(), 42U);
}

static void test_soc_storage_mode_flash_behavior(void)
{
#if (PROJECT_CFG_SOC_STORAGE_MODE == PROJECT_CFG_SOC_STORAGE_MODE_BKP_ONLY)
	host_reset_state();
	host_init_with_voltage(3835U, 3835U);
	CHECK_EQ_U32(s_flash_soc_load_count, 0U);
	CHECK_EQ_U32(s_flash_soc_save_count, 0U);
	CHECK_TRUE(s_bkp_write_count != 0U);

	SOC_RequestSetOnce(45U);
	host_run_seconds(20U, 3835U, 3835U, 0U, 0U);
	SOC_SaveSnapshotBeforeSleep();
	CHECK_EQ_U32(s_flash_soc_load_count, 0U);
	CHECK_EQ_U32(s_flash_soc_save_count, 0U);
	CHECK_EQ_U32(host_internal_soc(), g_stCellInfoReport.SocElement.u16Soc);
#elif (PROJECT_CFG_SOC_STORAGE_MODE == PROJECT_CFG_SOC_STORAGE_MODE_BKP_FLASH)
	unsigned save_count;

	host_reset_state();
	host_init_with_voltage(3835U, 3835U);
	save_count = s_flash_soc_save_count;
	SOC_RequestSetOnce(45U);
	host_run_seconds(20U, 3835U, 3835U, 0U, 0U);
	CHECK_EQ_U32(s_flash_soc_save_count, save_count);
	SOC_SaveSnapshotBeforeSleep();
	CHECK_EQ_U32(s_flash_soc_save_count, save_count + 1U);
	SOC_SaveSnapshotBeforeSleep();
	CHECK_EQ_U32(s_flash_soc_save_count, save_count + 1U);
#else
#error "Unhandled PROJECT_CFG_SOC_STORAGE_MODE"
#endif
}

int main(void)
{
	RUN_TEST(test_startup_ocv_uses_real_c_code);
	RUN_TEST(test_discharge_integration_uses_app_soc_path);
	RUN_TEST(test_board_self_consumption_integrates_during_relax);
	RUN_TEST(test_board_self_consumption_works_at_high_non_full_voltage);
	RUN_TEST(test_full_voltage_anchor_can_override_self_consumption);
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
	RUN_TEST(test_bkp_snapshot_recovers_after_runtime_reset);
	RUN_TEST(test_soc_storage_mode_flash_behavior);

	if (s_failures != 0U)
	{
		printf("SOC host C tests failed: %u\n", s_failures);
		return 1;
	}
	printf("SOC host C tests passed: %u\n", s_tests_run);
	return 0;
}
