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

static void soc_publish_report(void)
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
	soc_publish_report();
}

void SOC_RequestSetOnce(UINT8 soc)
{
	soc_set(soc);
	soc_save_current_snapshot();
	soc_publish_report();
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

void InitData_SOC(void)
{
	memset(&s_soc, 0, sizeof(s_soc));
	s_soc.cap_factory_as10 = soc_factory_cap_as10_from(OtherElement.u16Soc_Ah);
	s_soc.cycle_x100 = (UINT32)OtherElement.u16Soc_Cycle_times * 100U;
	soc_refresh_capacity_base();
	soc_load_or_default();
	soc_publish_report();
}

void SOC_SaveSnapshotBeforeSleep(void)
{
	soc_save_if_needed();
}



#define SOC_TIME_CURRENT_MIN_MA 200L
#define SOC_TIME_UNKNOWN_MINUTES 0xFFFFU
#define SOC_TIME_MAX_MINUTES 0xFFFEU

void App_SOC(void)
{
	if (!Fault_IsCurrentSampleFresh())
        return;
    soc_integrate(AfeCurrent_GetCurrent_mA());
    soc_save_if_needed();
    soc_publish_report();
}

UINT16 SOC_GetRemainingMinutes(UINT8 charging)
{
    UINT32 capacity, minutes;
    int64_t current_ma;
    INT32 measured_ma=AfeCurrent_GetCurrent_mA();
    UINT16 full=g_stCellInfoReport.SocElement.u16CapacityFull;
    UINT16 now=g_stCellInfoReport.SocElement.u16CapacityNow;

    if (!Fault_IsCurrentSampleFresh() || full==0U)
        return SOC_TIME_UNKNOWN_MINUTES;
    if (measured_ma>-SOC_TIME_CURRENT_MIN_MA && measured_ma<SOC_TIME_CURRENT_MIN_MA)
        return SOC_TIME_UNKNOWN_MINUTES;
    if (now>full) now=full;
    /* Use the same measured current as coulomb counting. */
    current_ma=(int64_t)measured_ma;
    if (charging) {
        if (measured_ma<SOC_TIME_CURRENT_MIN_MA) return 0U;
        if (current_ma<=0) return SOC_TIME_UNKNOWN_MINUTES;
        capacity=(UINT32)(full-now);
    }
    else {
        if (measured_ma>-SOC_TIME_CURRENT_MIN_MA) return 0U;
        capacity=now;
        current_ma=-current_ma;
    }
    /* Capacity is Ah*100: one unit is 10 mAh, or 600 mA*min.
     * Round up a nonzero remainder; the largest numerator is 39,321,000. */
    minutes=(UINT32)(((int64_t)capacity*600+current_ma-1)/current_ma);
    /* Reserve 0xFFFF for unavailable estimates on the customer wire format. */
    return (UINT16)(minutes>SOC_TIME_MAX_MINUTES ? SOC_TIME_MAX_MINUTES : minutes);
}
