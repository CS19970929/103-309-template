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
#define SOC_LOW_START_MV 3050U
#define SOC_EMPTY_MV 3000U
#define SOC_CELL_MAX_MV 5000U
#define SOC_CONFIRM_TICKS 1000U /* 10 s, in real 10 ms RUN ticks. */
#define SOC_MAX_SAMPLE_GAP_TICKS 50U
#define SOC_FULL_RELEASE_MV 50U
#define SOC_SMOOTH_DENOMINATOR 100000U /* 1 percentage point per 10 s. */
#define SOC_NO_MANUAL_TARGET 0xFFFFU
enum {
    SOC_TARGET_FULL = 1U,
    SOC_FULL_LATCHED = 2U,
    SOC_TARGET_EMPTY = 4U,
    SOC_CALIBRATION_FLAGS = 7U
};

/* Capacity uses 0.1 A*s; only this module owns the integrator and remainder. */
typedef struct {
    UINT32 full_as10;
    UINT32 now_as10;
    UINT32 cycle_x100;
    UINT32 passed_as10;
    int32_t remainder_mams;
    UINT16 cycle_generation;
    UINT32 low_entry_as10;
    UINT16 manual_target;
    UINT16 calibration_flags;
} SOC_STATE;
static SOC_STATE s_soc;
/* Timers and fractional correction credit never include STOP or invalid reads. */
static struct {
    UINT32 tick;
    UINT32 remainder;
    UINT16 full_ticks;
    UINT16 low_ticks;
    UINT16 charge_ticks;
    UINT8 clock_valid;
} s_cal;
static struct {
    UINT32 cycles;
    UINT32 retry_tick;
    UINT8 percent;
    UINT8 valid;
    UINT8 retry_active;
} s_saved;

void SOC_PauseCalibration(void)
{
    memset(&s_cal, 0, sizeof(s_cal));
}

void SOC_ResetVoltageCalibration(void)
{
    s_soc.low_entry_as10 = 0U;
    s_soc.calibration_flags = 0U;
    s_saved.valid = 0U;
    SOC_PauseCalibration();
}

void SOC_NotifyOverdischarge(void)
{
    /* SleepDeal calls once per new UV event, including the RTC status path.
     * The existing sleep save persists this intent and the unchanged capacity. */
    s_soc.manual_target = SOC_NO_MANUAL_TARGET;
    s_soc.calibration_flags = SOC_TARGET_EMPTY;
    s_saved.valid = 0U;
    SOC_PauseCalibration();
}

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
    /* Retired V2 word: bind a pending full target to its voltage setting. */
    data.u16MaxErrorPercent = OtherElement.u16Soc_V_100;
    data.u32CycleTimes = s_soc.cycle_x100;
    data.u32CapNow = s_soc.now_as10;
    data.u32CapFull = s_soc.full_as10;
    data.u32LearnPassedAs10 = s_soc.passed_as10;
    data.u16DsgSocInt = (UINT16)((uint64_t)s_soc.passed_as10 * 100U / unit);
    data.u16Reserved[STORAGE_SOC_CYCLE_SNAPSHOT_INDEX]=s_soc.cycle_generation;
    data.u16LearnState = STORAGE_SOC_SMOOTH_TAG;
    data.u16LearnAnchorSoc = s_soc.manual_target;
    data.u16Flags = s_soc.calibration_flags;
    data.u16Reserved[STORAGE_SOC_LOW_ENTRY_LO_INDEX] = (UINT16)s_soc.low_entry_as10;
    data.u16Reserved[STORAGE_SOC_LOW_ENTRY_HI_INDEX] = (UINT16)(s_soc.low_entry_as10 >> 16);
    data.u16Reserved[STORAGE_SOC_SMOOTH_INV_INDEX] = (UINT16)(STORAGE_SOC_SMOOTH_TAG ^ 0xFFFFU);
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
    UINT8 full_setting_changed = 0U;
    memset(&s_soc, 0, sizeof(s_soc));
    memset(&s_saved, 0, sizeof(s_saved));
    SOC_PauseCalibration();
    s_soc.manual_target = SOC_NO_MANUAL_TARGET;
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
        if (data.u16LearnState == STORAGE_SOC_SMOOTH_TAG &&
            data.u16Reserved[STORAGE_SOC_SMOOTH_INV_INDEX] == (UINT16)(STORAGE_SOC_SMOOTH_TAG ^ 0xFFFFU) &&
            (data.u16Flags & (UINT16)(~SOC_CALIBRATION_FLAGS)) == 0U &&
            (data.u16Flags & (SOC_TARGET_FULL | SOC_TARGET_EMPTY)) != (SOC_TARGET_FULL | SOC_TARGET_EMPTY) &&
            (!(data.u16Flags & SOC_TARGET_FULL) || (data.u16Flags & SOC_FULL_LATCHED)) &&
            (data.u16LearnAnchorSoc <= 100U || data.u16LearnAnchorSoc == SOC_NO_MANUAL_TARGET) &&
            data.u32CapFull != 0U) {
            UINT32 entry = (UINT32)data.u16Reserved[STORAGE_SOC_LOW_ENTRY_LO_INDEX] |
                ((UINT32)data.u16Reserved[STORAGE_SOC_LOW_ENTRY_HI_INDEX] << 16);
            if (entry <= data.u32CapFull) {
                s_soc.low_entry_as10 = (UINT32)((uint64_t)entry * s_soc.full_as10 / data.u32CapFull);
                s_soc.manual_target = data.u16LearnAnchorSoc;
                s_soc.calibration_flags = data.u16Flags;
                if (data.u16MaxErrorPercent != OtherElement.u16Soc_V_100) {
                    s_soc.calibration_flags &= (UINT16)(~(SOC_TARGET_FULL | SOC_FULL_LATCHED));
                    full_setting_changed = 1U;
                }
            }
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
        } else if (full_setting_changed) {
            /* Retire the stale goal durably; a later voltage change back must
             * not revive an old confirmation. Failure uses normal retries. */
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
    s_soc.manual_target = percent;
    s_soc.calibration_flags = 0U;
    s_soc.low_entry_as10 = 0U;
    if (!SOC_SaveSnapshot()) {
        s_soc = previous;
        return 0U;
    }
    /* ACK means the target is durable; current capacity has not jumped. */
    SOC_PauseCalibration();
    return 1U;
}

void SOC_ApplyParameters(UINT8 cycle_count_written)
{
    UINT32 full = (UINT32)OtherElement.u16Soc_Ah * 3600U;
    if (full == 0U) return;
    if (s_soc.full_as10 != 0U) {
        s_soc.now_as10 = (UINT32)((uint64_t)s_soc.now_as10 * full / s_soc.full_as10);
        s_soc.passed_as10 = (UINT32)((uint64_t)s_soc.passed_as10 * full / s_soc.full_as10);
        s_soc.low_entry_as10 = (UINT32)((uint64_t)s_soc.low_entry_as10 * full / s_soc.full_as10);
    } else {
        s_soc.now_as10 = (UINT32)((uint64_t)full * SOC_DEFAULT_PERCENT / 100U);
    }
    s_soc.full_as10 = full;
    s_soc.remainder_mams = 0;
    SOC_PauseCalibration();
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

static UINT16 soc_confirm_ticks(UINT16 ticks, UINT32 elapsed)
{
    return elapsed >= SOC_CONFIRM_TICKS - ticks ? SOC_CONFIRM_TICKS : (UINT16)(ticks + elapsed);
}

static UINT8 soc_cell_voltages_valid(void)
{
    UINT8 i;
    if (SeriesNum == 0U || SeriesNum > 16U ||
        g_stCellInfoReport.u16VCellMin == 0U ||
        g_stCellInfoReport.u16VCellMin > g_stCellInfoReport.u16VCellMax ||
        g_stCellInfoReport.u16VCellMax > SOC_CELL_MAX_MV) return 0U;
    for (i = 0U; i < SeriesNum; ++i)
        if (g_stCellInfoReport.u16VCell[i] == 0U ||
            g_stCellInfoReport.u16VCell[i] > SOC_CELL_MAX_MV) return 0U;
    return 1U;
}

static UINT32 soc_calibration_elapsed(void)
{
    UINT32 tick = SysTime_Get10msTickCount();
    UINT32 elapsed = tick - s_cal.tick;
    if (!s_cal.clock_valid || elapsed > SOC_MAX_SAMPLE_GAP_TICKS) {
        SOC_PauseCalibration();
        elapsed = 0U;
    }
    s_cal.tick = tick;
    s_cal.clock_valid = 1U;
    return elapsed;
}

static void soc_smooth_towards(UINT32 target, UINT32 elapsed)
{
    uint64_t credit;
    UINT32 step;
    /* A backlog cannot spend several sampling periods of correction at once. */
    if (elapsed > SOC_TICK_MS / 10U) elapsed = SOC_TICK_MS / 10U;
    credit = (uint64_t)s_soc.full_as10 * elapsed + s_cal.remainder;
    step = (UINT32)(credit / SOC_SMOOTH_DENOMINATOR);
    s_cal.remainder = (UINT32)(credit % SOC_SMOOTH_DENOMINATOR);
    if (target > s_soc.now_as10) {
        if (step > target - s_soc.now_as10) step = target - s_soc.now_as10;
        s_soc.now_as10 += step;
    } else {
        if (step > s_soc.now_as10 - target) step = s_soc.now_as10 - target;
        s_soc.now_as10 -= step;
    }
    if (s_soc.now_as10 == target) s_cal.remainder = 0U;
}

static void soc_update_calibration(INT32 current_ma, UINT32 elapsed)
{
    UINT32 target;
    UINT16 flags = s_soc.calibration_flags;
    UINT16 full_mv = OtherElement.u16Soc_V_100;
    UINT16 min_mv = g_stCellInfoReport.u16VCellMin;
    UINT16 max_mv = g_stCellInfoReport.u16VCellMax;

    if (s_soc.manual_target != SOC_NO_MANUAL_TARGET) {
        target = (UINT32)((uint64_t)s_soc.full_as10 * s_soc.manual_target / 100U);
        if (current_ma < 0 && (min_mv > SOC_LOW_START_MV || target > s_soc.now_as10)) {
            s_cal.remainder = 0U;
            return;
        }
        soc_smooth_towards(target, elapsed);
        if (s_soc.now_as10 == target) {
            s_soc.manual_target = SOC_NO_MANUAL_TARGET;
            s_saved.valid = 0U;
        }
        return;
    }

    if (full_mv > SOC_LOW_START_MV && full_mv <= SOC_CELL_MAX_MV) {
        if (flags & SOC_FULL_LATCHED) {
            if (max_mv < full_mv - SOC_FULL_RELEASE_MV) {
                s_cal.full_ticks = soc_confirm_ticks(s_cal.full_ticks, elapsed);
                if (s_cal.full_ticks == SOC_CONFIRM_TICKS) {
                    flags &= (UINT16)(~(SOC_FULL_LATCHED | SOC_TARGET_FULL));
                    s_cal.full_ticks = 0U;
                }
            } else s_cal.full_ticks = 0U;
        } else if (max_mv >= full_mv) {
            s_cal.full_ticks = soc_confirm_ticks(s_cal.full_ticks, elapsed);
            if (s_cal.full_ticks == SOC_CONFIRM_TICKS ||
                g_stCellInfoReport.unMdlFault_Third.bits.b1CellOvp) {
                flags = (UINT16)((flags & (UINT16)(~SOC_TARGET_EMPTY)) |
                    SOC_FULL_LATCHED | SOC_TARGET_FULL);
                if (s_soc.low_entry_as10) s_saved.valid = 0U;
                s_soc.low_entry_as10 = 0U;
                s_cal.full_ticks = 0U;
            }
        } else s_cal.full_ticks = 0U;
    } else {
        flags &= (UINT16)(~(SOC_FULL_LATCHED | SOC_TARGET_FULL));
        s_cal.full_ticks = 0U;
    }
    if (flags != s_soc.calibration_flags) s_saved.valid = 0U;
    s_soc.calibration_flags = flags;

    /* Even a restored/manual UV target never authorizes automatic correction
     * above 3050 mV, at rest, or during charging. Keep one entry reference
     * across voltage rebounds; re-entering the same dip must not compound it. */
    if (current_ma < 0 && min_mv <= SOC_LOW_START_MV) {
        s_cal.low_ticks = soc_confirm_ticks(s_cal.low_ticks, elapsed);
        if (s_cal.low_ticks == SOC_CONFIRM_TICKS) {
            if (s_soc.low_entry_as10 == 0U && s_soc.now_as10 != 0U) {
                s_soc.low_entry_as10 = s_soc.now_as10;
                s_saved.valid = 0U;
            }
            target = 0U;
            if (!(flags & SOC_TARGET_EMPTY) && min_mv > SOC_EMPTY_MV)
                target = (UINT32)((uint64_t)s_soc.low_entry_as10 *
                    (min_mv - SOC_EMPTY_MV) / (SOC_LOW_START_MV - SOC_EMPTY_MV));
            if (s_soc.now_as10 > target) {
                soc_smooth_towards(target, elapsed);
                return;
            }
        }
    } else s_cal.low_ticks = 0U;

    if ((flags & SOC_TARGET_FULL) && current_ma >= 0 &&
        max_mv >= full_mv - SOC_FULL_RELEASE_MV) {
        soc_smooth_towards(s_soc.full_as10, elapsed);
        return;
    }
    s_cal.remainder = 0U;
}

void App_SOC(UINT8 sample_valid, UINT8 fault_valid)
{
    INT32 current_ma = AfeCurrent_GetCurrent_mA();
    UINT32 elapsed;
    UINT8 current_valid = (UINT8)(sample_valid && Fault_IsCurrentSampleFresh() &&
        g_stLowPowerRtcStatus.mode != DEEP_MODE);
    if (s_soc.full_as10 != 0U && current_valid) soc_integrate_current(current_ma);
    if (s_soc.full_as10 != 0U && current_valid && fault_valid &&
        !AFE_PARAM_WRITE_Flag && soc_cell_voltages_valid()) {
        elapsed = soc_calibration_elapsed();
        /* Reuse the 10s confirmation to reject tiny positive noise pulses.
         * This only retires an old low reference; it never qualifies full. */
        if (current_ma > 0 && (s_soc.low_entry_as10 ||
            (s_soc.calibration_flags & SOC_TARGET_EMPTY))) {
            s_cal.charge_ticks = soc_confirm_ticks(s_cal.charge_ticks, elapsed);
            if (s_cal.charge_ticks == SOC_CONFIRM_TICKS) {
                s_soc.low_entry_as10 = 0U;
                s_soc.calibration_flags &= (UINT16)(~SOC_TARGET_EMPTY);
                s_saved.valid = 0U;
            }
        } else s_cal.charge_ticks = 0U;
        if (g_stCellInfoReport.unMdlFault_Third.bits.b1CellUvp) {
            s_cal.full_ticks = s_cal.low_ticks = 0U;
            s_cal.remainder = 0U;
        } else soc_update_calibration(current_ma, elapsed);
    } else SOC_PauseCalibration();
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
