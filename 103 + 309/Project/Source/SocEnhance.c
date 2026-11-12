#include "main.h"
#include <stdint.h>
#include <string.h>

#define SOC_TICK_MS ((UINT32)200U)
#define SOC_MAMS_PER_AS10 ((UINT32)100000U)

typedef struct {
    UINT32 cap_factory_as10;
    UINT32 cap_full_as10;
    UINT32 cap_now_as10;
    UINT32 cycle_x100;
    UINT32 dsg_acc_as10;
    int32_t rem_mams;
    UINT8 soc;
} SOC_STATE;
typedef struct {
    UINT32 cycle_x100;
    UINT32 cap_full_as10;
    UINT8 soc;
} SOC_SAVE_MARK;
static SOC_STATE s_soc;
static SOC_SAVE_MARK s_saved_soc;
static const UINT8 s_soc_default_startup_percent = 60U;
static void soc_save_current_snapshot(void);

/* Legacy read-only protocol tables; not used by SOC calculation. */
#if (PROJECT_CFG_BAT_CHEMISTRY == 1)
const UINT16 SOC_Table_LiFePO[SOC_TABLE_SIZE] = {
	3336, 100, 3332, 90, 3330, 80, 3327, 75, 3316, 70, 3301, 65,
	3294, 60, 3291, 55, 3290, 50, 3288, 45, 3286, 40, 3279, 35,
	3266, 30, 3254, 25, 3236, 20, 3212, 15, 3198, 10, 3112, 5,
	2950, 0, 1000, 0, 1000, 0,
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



static UINT32 soc_factory_cap_as10_from(UINT16 cap_a10)
{
	return (UINT32)cap_a10 * 3600U;
}

static void soc_refresh_capacity_base(void)
{
    s_soc.cap_full_as10 = s_soc.cap_factory_as10;
    if (s_soc.cap_now_as10 > s_soc.cap_full_as10)
        s_soc.cap_now_as10 = s_soc.cap_full_as10;
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

void SOC_PublishReportData(void)
{
	UINT32 cycles = s_soc.cycle_x100 / 100U;

	g_stCellInfoReport.SocElement.u16Soc = s_soc.soc;
	g_stCellInfoReport.SocElement.u16Soh = 100U;
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
	data.u16FormatVersion = FLASH_STORAGE_SOC_DATA_VERSION;
	data.u16SocNow = s_soc.soc;
	data.u16MaxErrorPercent = 100U;
	data.u32CycleTimes = s_soc.cycle_x100;
	data.u32CapNow = s_soc.cap_now_as10;
	data.u32CapFull = s_soc.cap_full_as10;
	data.u32LearnPassedAs10 = s_soc.dsg_acc_as10;
	/* Reserved calibration fields remain zero in the existing V2 format. */
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
		(s_soc.cap_full_as10 != s_saved_soc.cap_full_as10))
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
		if ((data.u32CapFull > 0U) && (data.u32CapNow <= data.u32CapFull) &&
			((data.u32CapNow != 0U) || (data.u16SocNow == 0U)))
		{
            /* Preserve the stored SOC ratio when removing old SOH derating
             * or applying a changed rated capacity. The V2 layout is unchanged. */
			s_soc.cap_now_as10 = (UINT32)(((uint64_t)data.u32CapNow *
                s_soc.cap_full_as10) / data.u32CapFull);
			s_soc.soc = soc_from_cap();
		}
		else
		{
			soc_set((UINT8)data.u16SocNow);
		}

	}
	else
	{
		s_soc.cycle_x100 = (UINT32)OtherElement.u16Soc_Cycle_times * 100U;
		soc_refresh_capacity_base();
		soc_set(s_soc_default_startup_percent);
		(void)soc_save();
	}
	soc_update_save_mark();
}

static void soc_integrate(int32_t net_current_ma)
{
	int32_t delta_as10;
	int64_t acc_mams;
	int64_t cap_now_as10;

	acc_mams = ((int64_t)net_current_ma * (int64_t)SOC_TICK_MS) +
		(int64_t)s_soc.rem_mams;
	delta_as10 = (int32_t)(acc_mams / (int64_t)SOC_MAMS_PER_AS10);
	s_soc.rem_mams = (int32_t)(acc_mams % (int64_t)SOC_MAMS_PER_AS10);
	if (delta_as10 == 0)
	{
		return;
	}
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
}

void soc_param_lib_init(void)
{
	memset(&s_soc, 0, sizeof(s_soc));
	s_soc.cap_factory_as10 = soc_factory_cap_as10_from(OtherElement.u16Soc_Ah);
	s_soc.cycle_x100 = (UINT32)OtherElement.u16Soc_Cycle_times * 100U;
	soc_refresh_capacity_base();
	soc_load_or_default();
	SOC_PublishReportData();
}

UINT8 SOC_ResetStoredSnapshotToDefault(void)
{
	STORAGE_FLASH_SOC_DATA data;
	UINT32 cap_factory = soc_factory_cap_as10_from(OtherElement.u16Soc_Ah);
	UINT32 cycle_x100 = (UINT32)OtherElement.u16Soc_Cycle_times * 100U;
	UINT32 cap_full = cap_factory;

	memset(&data, 0, sizeof(data));
	data.u16FormatVersion = FLASH_STORAGE_SOC_DATA_VERSION;
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

void SOC_IntEnhance_Ctrl(int32_t measured_current_ma)
{
    soc_integrate(measured_current_ma);
    soc_save_if_needed();
    SOC_PublishReportData();
}
