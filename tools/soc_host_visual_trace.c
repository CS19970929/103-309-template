#include "main.h"
#include "Flash.h"
#include "SocEnhance.h"

#include <stdio.h>
#include <string.h>

#define HOST_CAP_A10          ((UINT16)270U)
#define HOST_CAP_FACTORY_AS10 ((double)HOST_CAP_A10 * 3600.0)
#define HOST_TICKS_PER_SECOND ((UINT16)5U)
#define HOST_PERIOD_MS        ((UINT16)200U)
#define HOST_SOC_BKP_MAGIC    ((UINT16)0x5C0CU)
#define HOST_SOC_BKP_VERSION  ((UINT16)0x0001U)
#define HOST_SOC_BKP_CRC_SEED ((UINT16)0xA55AU)

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

typedef struct
{
	const char *name;
	UINT16 seconds;
	UINT16 idsg_a10;
	UINT16 ichg_a10;
	UINT16 imbalance_mv;
} HOST_SEGMENT;

typedef struct
{
	const char *name;
	double start_soc;
	const HOST_SEGMENT *segments;
	UINT16 segment_count;
	UINT16 repeats;
} HOST_SCENARIO;

static const UINT16 s_ocv_table[] = {
	4160, 100, 4100, 95, 4050, 90, 3995, 85, 3935, 80,
	3880, 75, 3835, 70, 3795, 65, 3760, 60, 3725, 55,
	3695, 50, 3670, 45, 3645, 40, 3615, 35, 3585, 30,
	3555, 25, 3525, 20, 3480, 15, 3400, 10, 3250, 5,
	3000, 0,
};

static const HOST_SEGMENT s_city_commute[] = {
	{"idle-before-ride", 60, 0, 0, 4},
	{"flat-cruise", 300, 80, 0, 4},
	{"traffic-bursts", 120, 220, 0, 6},
	{"steady-cruise", 300, 120, 0, 4},
	{"traffic-stop", 60, 0, 0, 4},
	{"short-hill", 180, 350, 0, 10},
	{"return-cruise", 300, 100, 0, 4},
};

static const HOST_SEGMENT s_hill_climb[] = {
	{"approach", 120, 120, 0, 4},
	{"long-hill", 240, 420, 0, 12},
	{"coast-recovery", 180, 0, 0, 4},
	{"post-hill-cruise", 180, 100, 0, 4},
};

static const HOST_SEGMENT s_fast_current_pulses[] = {
	{"pulse-light", 1, 30, 0, 4},
	{"pulse-accelerate", 1, 260, 0, 8},
	{"pulse-cruise", 1, 80, 0, 4},
	{"pulse-steep", 1, 420, 0, 12},
	{"pulse-coast", 1, 0, 0, 4},
	{"pulse-recover", 1, 160, 0, 6},
	{"pulse-restart", 1, 320, 0, 10},
	{"pulse-roll", 1, 40, 0, 4},
};

static const HOST_SEGMENT s_deep_cutoff[] = {
	{"low-soc-cruise", 240, 120, 0, 6},
	{"low-soc-load", 420, 180, 0, 8},
	{"controller-cutoff-area", 240, 220, 0, 10},
};

static const HOST_SEGMENT s_charge_anchor[] = {
	{"bulk-charge", 600, 0, 270, 4},
	{"near-full-confirm", 20, 0, 270, 4},
};

static const HOST_SCENARIO s_scenarios[] = {
	{"city_commute", 80.0, s_city_commute, (UINT16)(sizeof(s_city_commute) / sizeof(s_city_commute[0])), 1},
	{"hill_climb", 60.0, s_hill_climb, (UINT16)(sizeof(s_hill_climb) / sizeof(s_hill_climb[0])), 1},
	{"fast_current_pulses", 70.0, s_fast_current_pulses, (UINT16)(sizeof(s_fast_current_pulses) / sizeof(s_fast_current_pulses[0])), 45},
	{"deep_cutoff", 18.0, s_deep_cutoff, (UINT16)(sizeof(s_deep_cutoff) / sizeof(s_deep_cutoff[0])), 1},
	{"charge_anchor", 88.0, s_charge_anchor, (UINT16)(sizeof(s_charge_anchor) / sizeof(s_charge_anchor[0])), 1},
};

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

static UINT32 host_cap_now_from_soc(UINT16 soc)
{
	return (UINT32)((HOST_CAP_FACTORY_AS10 * (double)soc) / 100.0);
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
								UINT32 dsg_acc_as10)
{
	UINT32 cap_factory_as10 = (UINT32)HOST_CAP_FACTORY_AS10;
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

static void host_write_bkp_snapshot(UINT16 soc, UINT32 cycle_x100, UINT32 cap_now_as10)
{
	UINT16 soc_flags = (UINT16)(soc & 0x00FFU);
	UINT16 crc = host_bkp_crc_calc(soc_flags, cycle_x100, cap_now_as10, 0U);

	BKP_WriteBackupRegister(BKP_DR2, (UINT16)(~HOST_SOC_BKP_MAGIC));
	BKP_WriteBackupRegister(BKP_DR3, soc_flags);
	BKP_WriteBackupRegister(BKP_DR4, (UINT16)(cycle_x100 & 0xFFFFU));
	BKP_WriteBackupRegister(BKP_DR5, (UINT16)(cycle_x100 >> 16U));
	BKP_WriteBackupRegister(BKP_DR6, (UINT16)(cap_now_as10 & 0xFFFFU));
	BKP_WriteBackupRegister(BKP_DR7, (UINT16)(cap_now_as10 >> 16U));
	BKP_WriteBackupRegister(BKP_DR8, 0U);
	BKP_WriteBackupRegister(BKP_DR9, 0U);
	BKP_WriteBackupRegister(BKP_DR10, crc);
	BKP_WriteBackupRegister(BKP_DR1, HOST_SOC_BKP_MAGIC);
}

static UINT16 host_soc_from_cap_as10(UINT32 cap_as10)
{
	UINT32 cap_full = (UINT32)HOST_CAP_FACTORY_AS10;
	UINT32 soc;

	if (cap_as10 >= cap_full)
	{
		return 100U;
	}
	soc = (UINT32)(((uint64_t)cap_as10 * 100ULL + (cap_full / 2U)) / cap_full);
	return (soc > 100U) ? 100U : (UINT16)soc;
}

static UINT8 host_read_bkp_soc(UINT16 *soc)
{
	UINT16 soc_flags;
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
	if (host_bkp_crc_calc(soc_flags, cycle_x100, cap_now_as10, dsg_acc_as10) !=
		BKP_ReadBackupRegister(BKP_DR10))
	{
		return 0U;
	}
	if ((soc_flags & 0x00FFU) > 100U)
	{
		return 0U;
	}
	if (((cap_now_as10 != 0U) || ((soc_flags & 0x00FFU) == 0U)) &&
		(cap_now_as10 <= (UINT32)HOST_CAP_FACTORY_AS10))
	{
		*soc = host_soc_from_cap_as10(cap_now_as10);
	}
	else
	{
		*soc = (UINT16)(soc_flags & 0x00FFU);
	}
	return 1U;
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
	s_host_typec_out_current_mA = 0U;
	s_host_vbat_mV = 0U;
	s_host_afe_current_sample_seq = 0U;
	host_apply_default_config();
}

static void host_set_snapshot(UINT16 soc)
{
	memset(&s_flash_soc, 0, sizeof(s_flash_soc));
	s_flash_soc.u16FormatVersion = FLASH_STORAGE_SOC_DATA_VERSION_V2;
	s_flash_soc.u16SocNow = soc;
	s_flash_soc.u16DsgSocInt = 0U;
	s_flash_soc.u16MaxErrorPercent = 100U;
	s_flash_soc.u32CycleTimes = 300U;
	s_flash_soc.u32CapFull = (UINT32)HOST_CAP_FACTORY_AS10;
	s_flash_soc.u32CapNow = host_cap_now_from_soc(soc);
	s_flash_soc.u32LearnPassedAs10 = 0U;
	s_flash_soc.u16Flags = 0U;
	s_flash_soc_valid = 1U;
	host_write_bkp_snapshot(soc, s_flash_soc.u32CycleTimes, s_flash_soc.u32CapNow);
}

static UINT16 host_voltage_from_soc(double soc)
{
	UINT16 table_count = (UINT16)(sizeof(s_ocv_table) / sizeof(s_ocv_table[0]) / 2U);
	UINT16 rounded_soc;
	UINT16 i;

	if (soc < 0.0)
	{
		soc = 0.0;
	}
	else if (soc > 100.0)
	{
		soc = 100.0;
	}
	rounded_soc = (UINT16)(soc + 0.5);
	for (i = 0U; i < (UINT16)(table_count - 1U); ++i)
	{
		UINT16 v1 = s_ocv_table[i * 2U];
		UINT16 s1 = s_ocv_table[i * 2U + 1U];
		UINT16 v2 = s_ocv_table[(i + 1U) * 2U];
		UINT16 s2 = s_ocv_table[(i + 1U) * 2U + 1U];
		if ((s1 >= rounded_soc) && (rounded_soc >= s2))
		{
			if (s1 == s2)
			{
				return v1;
			}
			return (UINT16)((((UINT32)v1 * (UINT32)(rounded_soc - s2)) +
				((UINT32)v2 * (UINT32)(s1 - rounded_soc))) / (UINT32)(s1 - s2));
		}
	}
	return (rounded_soc >= s_ocv_table[1]) ? s_ocv_table[0] : s_ocv_table[(table_count - 1U) * 2U];
}

static void host_pack_voltage(double true_soc,
							  UINT16 idsg_a10,
							  UINT16 ichg_a10,
							  UINT16 imbalance_mv,
							  UINT16 *vmax,
							  UINT16 *vmin)
{
	UINT16 base = host_voltage_from_soc(true_soc);
	UINT16 local_vmin;

	if (idsg_a10 != 0U)
	{
		UINT16 sag_mv = (UINT16)(15U + (idsg_a10 / 2U));
		if (sag_mv > 560U)
		{
			sag_mv = 560U;
		}
		local_vmin = (base > (UINT16)(sag_mv + imbalance_mv)) ?
			(UINT16)(base - sag_mv - imbalance_mv) : 2400U;
		if (local_vmin < 2400U)
		{
			local_vmin = 2400U;
		}
	}
	else if (ichg_a10 != 0U)
	{
		UINT16 rise_mv = (UINT16)(10U + (ichg_a10 / 8U));
		if (rise_mv > 130U)
		{
			rise_mv = 130U;
		}
		local_vmin = (UINT16)(base + rise_mv);
		if (local_vmin > 4200U)
		{
			local_vmin = 4200U;
		}
	}
	else
	{
		local_vmin = base;
	}
	*vmin = local_vmin;
	*vmax = (UINT16)(local_vmin + imbalance_mv);
	if (*vmax > 5000U)
	{
		*vmax = 5000U;
	}
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
	g_stCellInfoReport.u16VCellDelta = (UINT16)(vmax - vmin);
	g_stCellInfoReport.u16VCellTotle = (UINT16)(((UINT32)vmin * (UINT32)SeriesNum) / 10U);
	s_host_vbat_mV = (UINT32)vmin * (UINT32)SeriesNum;
	g_stCellInfoReport.u16Ichg = ichg;
	g_stCellInfoReport.u16IDischg = idsg;
	++s_host_afe_current_sample_seq;
	App_SOC();
}

static void host_pack_step(double *cap_now, UINT16 idsg_a10, UINT16 ichg_a10)
{
	double delta = ((double)ichg_a10 - (double)idsg_a10) * (double)HOST_PERIOD_MS / 1000.0;
	*cap_now += delta;
	if (*cap_now < 0.0)
	{
		*cap_now = 0.0;
	}
	else if (*cap_now > HOST_CAP_FACTORY_AS10)
	{
		*cap_now = HOST_CAP_FACTORY_AS10;
	}
}

static double host_true_soc(double cap_now)
{
	double soc = cap_now * 100.0 / HOST_CAP_FACTORY_AS10;
	if (soc < 0.0)
	{
		return 0.0;
	}
	if (soc > 100.0)
	{
		return 100.0;
	}
	return soc;
}

static UINT16 host_internal_soc(void)
{
	UINT16 soc;

	if (host_read_bkp_soc(&soc))
	{
		return soc;
	}
	return s_flash_soc.u16SocNow;
}

static void host_emit_row(const char *scenario,
						  UINT32 time_s,
						  const char *segment,
						  double true_soc,
						  UINT16 internal_soc,
						  UINT16 vmax,
						  UINT16 vmin,
						  UINT16 ichg_a10,
						  UINT16 idsg_a10)
{
	printf("%s,%lu,%s,%.2f,%u,%u,%u,%u,%u,%u,%u,%u\n",
		   scenario,
		   (unsigned long)time_s,
		   segment,
		   true_soc,
		   (unsigned int)internal_soc,
		   (unsigned int)g_stCellInfoReport.SocElement.u16Soc,
		   (unsigned int)vmin,
		   (unsigned int)vmax,
		   (unsigned int)ichg_a10,
		   (unsigned int)idsg_a10,
		   (unsigned int)g_stCellInfoReport.SocElement.u16Soh,
		   (unsigned int)g_stCellInfoReport.SocElement.u16CapacityNow);
}

static void host_run_scenario(const HOST_SCENARIO *scenario)
{
	double cap_now = HOST_CAP_FACTORY_AS10 * scenario->start_soc / 100.0;
	UINT16 vmax = 0U;
	UINT16 vmin = 0U;
	UINT16 repeat;
	UINT32 time_s = 0U;
	UINT16 initial_soc = (UINT16)(scenario->start_soc + 0.5);

	host_reset_state();
	host_set_snapshot(initial_soc);
	host_pack_voltage(host_true_soc(cap_now), 0U, 0U, 4U, &vmax, &vmin);
	host_init_with_voltage(vmax, vmin);
	host_emit_row(scenario->name, 0U, "start", host_true_soc(cap_now), host_internal_soc(),
		vmax, vmin, 0U, 0U);

	for (repeat = 0U; repeat < scenario->repeats; ++repeat)
	{
		UINT16 i;
		for (i = 0U; i < scenario->segment_count; ++i)
		{
			const HOST_SEGMENT *segment = &scenario->segments[i];
			UINT32 ticks = (UINT32)segment->seconds * HOST_TICKS_PER_SECOND;
			UINT32 tick;
			for (tick = 0U; tick < ticks; ++tick)
			{
				if ((strcmp(scenario->name, "charge_anchor") == 0) &&
					(strcmp(segment->name, "bulk-charge") == 0))
				{
					vmax = 4100U;
					vmin = 4050U;
				}
				else if ((strcmp(scenario->name, "charge_anchor") == 0) &&
					(strcmp(segment->name, "near-full-confirm") == 0))
				{
					vmax = 4181U;
					vmin = 4100U;
				}
				else
				{
					host_pack_voltage(host_true_soc(cap_now),
						segment->idsg_a10,
						segment->ichg_a10,
						segment->imbalance_mv,
						&vmax,
						&vmin);
				}
				host_tick(vmax, vmin, segment->ichg_a10, segment->idsg_a10);
				host_pack_step(&cap_now, segment->idsg_a10, segment->ichg_a10);
				if ((tick % HOST_TICKS_PER_SECOND) == (UINT32)(HOST_TICKS_PER_SECOND - 1U))
				{
					++time_s;
					host_emit_row(scenario->name, time_s, segment->name,
						host_true_soc(cap_now), host_internal_soc(), vmax, vmin,
						segment->ichg_a10, segment->idsg_a10);
				}
			}
		}
	}
}

int main(void)
{
	UINT16 i;

	printf("scenario,time_s,segment,true_soc,internal_soc,public_soc,vmin_mv,vmax_mv,ichg_a10,idsg_a10,soh,cap_now_ah100\n");
	for (i = 0U; i < (UINT16)(sizeof(s_scenarios) / sizeof(s_scenarios[0])); ++i)
	{
		host_run_scenario(&s_scenarios[i]);
	}
	return 0;
}
