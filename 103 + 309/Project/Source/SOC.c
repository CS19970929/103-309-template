#include "main.h"
#include <stdint.h>
#include <string.h>

#define SOC_TICK_MS 200U
#define SOC_MAMS_PER_AS10 100000L
#define SOC_DEFAULT_PERCENT 60U
#define SOC_SAVE_RETRY_10MS_TICKS 100U
#define SOC_TIME_CURRENT_MIN_MA 200L
#define SOC_TIME_UNKNOWN_MINUTES 0xFFFFU
#define SOC_TIME_MAX_MINUTES 0xFFFEU

/* Capacity uses 0.1 A*s; only this module owns the integrator and remainder. */
typedef struct {
    UINT32 full_as10;
    UINT32 now_as10;
    UINT32 cycle_x100;
    UINT32 passed_as10;
    int32_t remainder_mams;
    UINT16 cycle_generation;
} SOC_STATE;
static SOC_STATE s_soc;
static struct {
    UINT32 cycles;
    UINT32 retry_tick;
    UINT8 percent;
    UINT8 valid;
    UINT8 retry_active;
} s_saved;

static UINT8 soc_percent(void)
{
    if (s_soc.full_as10 == 0U) return 0U;
    return (UINT8)(((uint64_t)s_soc.now_as10 * 100U + s_soc.full_as10 / 2U) /
                   s_soc.full_as10);
}

static UINT16 soc_capacity_ah100(UINT32 capacity_as10)
{
    UINT32 value = (capacity_as10 + 180U) / 360U;
    return value > 0xFFFFU ? 0xFFFFU : (UINT16)value;
}

static void soc_publish_report(void)
{
    UINT32 cycles = s_soc.cycle_x100 / 100U;
    g_stCellInfoReport.SocElement.u16Soc = soc_percent();
    /* Keep the historical cycle-based SOH estimate separate from SOC capacity. */
    g_stCellInfoReport.SocElement.u16Soh = cycles / 100U >= 20U ?
        80U : (UINT16)(100U - cycles / 100U);
    g_stCellInfoReport.SocElement.u16CapacityNow = soc_capacity_ah100(s_soc.now_as10);
    g_stCellInfoReport.SocElement.u16CapacityFull = soc_capacity_ah100(s_soc.full_as10);
    g_stCellInfoReport.SocElement.u16CapacityFactory = soc_capacity_ah100(s_soc.full_as10);
    g_stCellInfoReport.SocElement.u16Cycle_times = cycles > 0xFFFFU ? 0xFFFFU : (UINT16)cycles;
}

UINT8 SOC_SaveSnapshot(void)
{
    STORAGE_FLASH_SOC_DATA data;
    UINT32 unit = s_soc.full_as10 / 100U;
    if (unit == 0U) return 0U;
    memset(&data, 0, sizeof(data));
    data.u16FormatVersion = FLASH_STORAGE_SOC_DATA_VERSION;
    data.u16SocNow = soc_percent();
    data.u16MaxErrorPercent = 100U;
    data.u32CycleTimes = s_soc.cycle_x100;
    data.u32CapNow = s_soc.now_as10;
    data.u32CapFull = s_soc.full_as10;
    data.u32LearnPassedAs10 = s_soc.passed_as10;
    data.u16DsgSocInt = (UINT16)((uint64_t)s_soc.passed_as10 * 100U / unit);
    data.u16Reserved[STORAGE_SOC_CYCLE_SNAPSHOT_INDEX]=s_soc.cycle_generation;
    /* Keep the V2 layout; all retired calibration fields stay zero. */
    if (!StorageFlash_SaveSocData(&data)) {
        s_saved.retry_tick = SysTime_Get10msTickCount();
        s_saved.retry_active = 1U;
        return 0U;
    }
    s_saved.percent = (UINT8)data.u16SocNow;
    s_saved.cycles = s_soc.cycle_x100 / 100U;
    s_saved.valid = 1U;
    s_saved.retry_active = 0U;
    return 1U;
}

static void soc_save_if_changed(void)
{
    if (s_soc.full_as10 == 0U) return;
    /* Whole-cycle statistics share SOC saves. The old 0.01-cycle trigger
     * caused a second write between adjacent integer SOC transitions. */
    if (s_saved.valid && s_saved.percent == soc_percent() &&
        s_saved.cycles == s_soc.cycle_x100 / 100U) return;
    if (s_saved.retry_active && (UINT32)(SysTime_Get10msTickCount() -
        s_saved.retry_tick) < SOC_SAVE_RETRY_10MS_TICKS) return;
    (void)SOC_SaveSnapshot();
}

void InitData_SOC(void)
{
    STORAGE_FLASH_SOC_DATA data;
    UINT32 unit;
    memset(&s_soc, 0, sizeof(s_soc));
    memset(&s_saved, 0, sizeof(s_saved));
    s_soc.cycle_generation = StorageFlash_GetSocCycleGeneration();
    s_soc.full_as10 = (UINT32)OtherElement.u16Soc_Ah * 3600U;
    unit = s_soc.full_as10 / 100U;
    if (unit != 0U && StorageFlash_LoadSocData(&data) &&
        data.u16FormatVersion == FLASH_STORAGE_SOC_DATA_VERSION &&
        data.u16SocNow <= 100U && data.u16DsgSocInt <= 100U) {
        s_soc.cycle_x100 = data.u32CycleTimes;
        s_soc.passed_as10 = data.u32LearnPassedAs10 != 0U ?
            data.u32LearnPassedAs10 % unit : (UINT32)((uint64_t)unit * data.u16DsgSocInt / 100U) % unit;
        if (data.u32CapFull != 0U && data.u32CapNow <= data.u32CapFull &&
            (data.u32CapNow != 0U || data.u16SocNow == 0U)) {
            /* Preserve SOC, including its fraction, across old SOH snapshots
             * and rated-capacity changes without changing the Flash format. */
            s_soc.now_as10 = (UINT32)((uint64_t)data.u32CapNow *
                s_soc.full_as10 / data.u32CapFull);
        } else {
            s_soc.now_as10 = (UINT32)((uint64_t)s_soc.full_as10 * data.u16SocNow / 100U);
        }
        s_saved.percent = soc_percent();
        s_saved.cycles = s_soc.cycle_x100 / 100U;
        s_saved.valid = 1U;
        if (s_soc.cycle_generation != STORAGE_SOC_CYCLE_GENERATION_NONE &&
            data.u16Reserved[STORAGE_SOC_CYCLE_SNAPSHOT_INDEX] != s_soc.cycle_generation) {
            s_soc.cycle_x100 = (UINT32)OtherElement.u16Soc_Cycle_times * 100U;
            s_soc.passed_as10 = 0U;
            s_saved.valid = 0U;
            (void)SOC_SaveSnapshot();
        }
    } else if (unit != 0U) {
        s_soc.cycle_x100 = (UINT32)OtherElement.u16Soc_Cycle_times * 100U;
        s_soc.now_as10 = (UINT32)((uint64_t)s_soc.full_as10 * SOC_DEFAULT_PERCENT / 100U);
        (void)SOC_SaveSnapshot();
    }
    soc_publish_report();
}

UINT8 SOC_RequestSetOnce(UINT8 percent)
{
    SOC_STATE previous = s_soc;
    if (percent > 100U || s_soc.full_as10 == 0U) return 0U;
    s_soc.now_as10 = (UINT32)((uint64_t)s_soc.full_as10 * percent / 100U);
    s_soc.remainder_mams = 0;
    if (!SOC_SaveSnapshot()) {
        s_soc = previous;
        return 0U;
    }
    soc_publish_report();
    return 1U;
}

void SOC_ApplyParameters(UINT8 cycle_count_written)
{
    UINT32 full = (UINT32)OtherElement.u16Soc_Ah * 3600U;
    if (full == 0U) return;
    if (s_soc.full_as10 != 0U) {
        s_soc.now_as10 = (UINT32)((uint64_t)s_soc.now_as10 * full / s_soc.full_as10);
        s_soc.passed_as10 = (UINT32)((uint64_t)s_soc.passed_as10 * full / s_soc.full_as10);
    } else {
        s_soc.now_as10 = (UINT32)((uint64_t)full * SOC_DEFAULT_PERCENT / 100U);
    }
    s_soc.full_as10 = full;
    s_soc.remainder_mams = 0;
    if (cycle_count_written) {
        /* The config tag is already durable, so reboot can finish a failed save. */
        s_soc.cycle_generation = StorageFlash_GetSocCycleGeneration();
        s_soc.cycle_x100 = (UINT32)OtherElement.u16Soc_Cycle_times * 100U;
        s_soc.passed_as10 = 0U;
    }
    s_saved.valid = 0U;
    (void)SOC_SaveSnapshot();
    soc_publish_report();
}

static void soc_integrate_current(int32_t current_ma)
{
    int64_t total_mams = (int64_t)current_ma * SOC_TICK_MS + s_soc.remainder_mams;
    int32_t delta_as10 = (int32_t)(total_mams / SOC_MAMS_PER_AS10);
    int64_t capacity;
    s_soc.remainder_mams = (int32_t)(total_mams % SOC_MAMS_PER_AS10);
    if (delta_as10 == 0) return;
    if (delta_as10 < 0) {
        UINT32 unit = s_soc.full_as10 / 100U;
        UINT32 steps;
        s_soc.passed_as10 += (UINT32)(-(int64_t)delta_as10);
        steps = s_soc.passed_as10 / unit;
        s_soc.passed_as10 %= unit;
        s_soc.cycle_x100 = UINT32_MAX - s_soc.cycle_x100 < steps ?
            UINT32_MAX : s_soc.cycle_x100 + steps;
    }
    capacity = (int64_t)s_soc.now_as10 + delta_as10;
    if (capacity <= 0) {
        capacity = 0;
        s_soc.remainder_mams = 0;
    } else if (capacity >= s_soc.full_as10) {
        capacity = s_soc.full_as10;
        s_soc.remainder_mams = 0;
    }
    s_soc.now_as10 = (UINT32)capacity;
}

void App_SOC(void)
{
    if (s_soc.full_as10 != 0U && Fault_IsCurrentSampleFresh())
        soc_integrate_current(AfeCurrent_GetCurrent_mA());
    /* A failed first save must retry even when measurement is unavailable. */
    soc_save_if_changed();
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
