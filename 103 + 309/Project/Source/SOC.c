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
#define SOC_CELL_MAX_MV 5000U
#define SOC_CELL_MAX_DELTA_MV 800U /* SOC endpoint evidence requires plausible spread. */
#define SOC_FULL_REST_MARGIN_MV 50U /* Whole-pack near SOC100: zero tail current. */
#define SOC_FULL_PREPARE_MV 50U /* Preconfirm near endpoint before charger stops. */
#define SOC_CONFIRM_TICKS 1000U /* 10 s, in real 10 ms RUN ticks. */
#define SOC_MAX_SAMPLE_GAP_TICKS 50U
#define SOC_SMOOTH_DENOMINATOR 100000U /* Low-end: 1% per 10 seconds. */
#define SOC_FULL_SMOOTH_DENOMINATOR 10000U /* Confirmed full: 1% per second. */
#define SOC_CHARGE_EVIDENCE_MA 100L /* Positive current seen before the terminal. */
#define SOC_SAVE_MIN_INTERVAL_TICKS 1000U /* Normal integer-SOC saves: >=10s apart. */
#define SOC_NO_MANUAL_TARGET 0xFFFFU
enum {
    SOC_TARGET_FULL = 1U,
    SOC_FULL_LATCHED = 2U,
    SOC_TARGET_EMPTY = 4U, /* Legacy pending UV intent; retired on new events. */
    SOC_CHARGE_SEEN = 8U,
    SOC_MANUAL_OVERRIDE = 16U, /* Manual SOC stays authoritative until real current. */
    SOC_CALIBRATION_FLAGS = 31U
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
    UINT32 last_write_tick;
    UINT8 percent;
    UINT8 valid;
    UINT8 retry_active;
} s_saved;

static void soc_publish_report(void);

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
    /* Called only after fresh, plausible cell data and a verified AFE UV bit.
     * Protective MOS shutdown/DEEP entry must not depend on this calibration. */
    if (s_soc.now_as10 == 0U && s_soc.calibration_flags == 0U)
        return;
    s_soc.now_as10 = 0U;
    s_soc.remainder_mams = 0;
    s_soc.low_entry_as10 = 0U;
    s_soc.calibration_flags = 0U;
    s_saved.valid = 0U;
    SOC_PauseCalibration();
    soc_publish_report();
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
    if (s_soc.full_as10 == 0U) return 0U;
    memset(&data, 0, sizeof(data));
    data.u16FormatVersion = FLASH_STORAGE_SOC_DATA_VERSION;
    /* Retired V2 word: bind a pending full target to its voltage setting. */
    data.u16MaxErrorPercent = OtherElement.u16Soc_V_100;
    data.u32CycleTimes = s_soc.cycle_x100;
    data.u32CapNow = s_soc.now_as10;
    data.u32CapFull = s_soc.full_as10;
    data.u32LearnPassedAs10 = s_soc.passed_as10;
    data.u16Reserved[STORAGE_SOC_CYCLE_SNAPSHOT_INDEX]=s_soc.cycle_generation;
    data.u16LearnState = STORAGE_SOC_STATE_TAG;
    data.u16LearnAnchorSoc = SOC_NO_MANUAL_TARGET;
    data.u16Flags = s_soc.calibration_flags;
    data.u16Reserved[STORAGE_SOC_LOW_ENTRY_LO_INDEX] = (UINT16)s_soc.low_entry_as10;
    data.u16Reserved[STORAGE_SOC_LOW_ENTRY_HI_INDEX] = (UINT16)(s_soc.low_entry_as10 >> 16);
    if (!StorageFlash_SaveSocData(&data)) {
        s_saved.retry_tick = SysTime_Get10msTickCount();
        s_saved.retry_active = 1U;
        return 0U;
    }
    s_saved.percent = soc_percent();
    s_saved.cycles = s_soc.cycle_x100 / 100U;
    s_saved.last_write_tick = SysTime_Get10msTickCount();
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
    /* Critical flag changes set valid=0 and bypass this normal wear limit. */
    if (s_saved.valid && (UINT32)(SysTime_Get10msTickCount() -
        s_saved.last_write_tick) < SOC_SAVE_MIN_INTERVAL_TICKS) return;
    if (s_saved.retry_active && (UINT32)(SysTime_Get10msTickCount() -
        s_saved.retry_tick) < SOC_SAVE_RETRY_10MS_TICKS) return;
    (void)SOC_SaveSnapshot();
}

/* Decode historical V2 fields once. Current snapshots have one capacity source;
 * record CRC/commit checking is owned by StorageFlash_LoadSocData(). */
/* Only the current tagged V2 payload is accepted; CRC and commit are checked by Flash.c. */
static UINT8 soc_restore_snapshot(const STORAGE_FLASH_SOC_DATA *data)
{
    UINT32 full=data->u32CapFull;
    UINT32 unit=s_soc.full_as10/100U;
    UINT32 entry=(UINT32)data->u16Reserved[STORAGE_SOC_LOW_ENTRY_LO_INDEX] |
        ((UINT32)data->u16Reserved[STORAGE_SOC_LOW_ENTRY_HI_INDEX]<<16);
    UINT16 flags=data->u16Flags;

    if (data->u16LearnState!=STORAGE_SOC_STATE_TAG || full==0U ||
        data->u32CapNow>full || unit==0U) return 0U;

    s_soc.now_as10=(UINT32)((uint64_t)data->u32CapNow*s_soc.full_as10/full);
    s_soc.cycle_x100=data->u32CycleTimes;
    s_soc.passed_as10=(UINT32)(((uint64_t)data->u32LearnPassedAs10 *
        s_soc.full_as10 / full) % unit);
    if ((flags & (UINT16)(~SOC_CALIBRATION_FLAGS))==0U &&
        (flags & (SOC_TARGET_FULL | SOC_TARGET_EMPTY))!=(SOC_TARGET_FULL | SOC_TARGET_EMPTY) &&
        (!(flags & SOC_TARGET_FULL) || (flags & SOC_FULL_LATCHED)) &&
        entry<=full) {
        s_soc.low_entry_as10=(UINT32)((uint64_t)entry*s_soc.full_as10/full);
        s_soc.calibration_flags=(UINT16)(flags & (UINT16)~SOC_TARGET_EMPTY);
    }
    return 1U;
}

void InitData_SOC(void)
{
    STORAGE_FLASH_SOC_DATA data;
    memset(&s_soc, 0, sizeof(s_soc));
    memset(&s_saved, 0, sizeof(s_saved));
    SOC_PauseCalibration();
    /* Runtime_Boot calls StorageFlash_InitParameters first; Flash owns cycle-command recovery. */
    s_soc.cycle_generation = StorageFlash_GetSocCycleGeneration();
    s_soc.full_as10 = (UINT32)OtherElement.u16Soc_Ah * 3600U;
    s_soc.cycle_x100 = (UINT32)OtherElement.u16Soc_Cycle_times * 100U;
    s_soc.now_as10 = (UINT32)((uint64_t)s_soc.full_as10 * SOC_DEFAULT_PERCENT / 100U);
    if (s_soc.full_as10 != 0U && StorageFlash_LoadSocData(&data) && soc_restore_snapshot(&data)) {
        s_saved.percent = soc_percent();
        s_saved.cycles = s_soc.cycle_x100 / 100U;
        s_saved.valid = (UINT8)((data.u16Flags & SOC_TARGET_EMPTY) == 0U);
        if (data.u16MaxErrorPercent != OtherElement.u16Soc_V_100 &&
            (s_soc.calibration_flags & (SOC_TARGET_FULL | SOC_FULL_LATCHED))) {
            /* Retire a stale full goal even if power failed after config commit. */
            s_soc.calibration_flags &= (UINT16)(~(SOC_TARGET_FULL | SOC_FULL_LATCHED));
            s_saved.valid = 0U;
        }
    }
    soc_save_if_changed();
    soc_publish_report();
}

UINT8 SOC_RequestSetOnce(UINT8 percent)
{
    SOC_STATE previous = s_soc;
    if (percent > 100U || s_soc.full_as10 == 0U) return 0U;
    /* An explicit host command is one of the two authorized SOC jumps.
     * Only acknowledge when the new precise capacity has been committed. */
    s_soc.now_as10 = (UINT32)((uint64_t)s_soc.full_as10 * percent / 100U);
    s_soc.remainder_mams = 0;
    s_soc.low_entry_as10 = 0U;
    s_soc.calibration_flags = SOC_MANUAL_OVERRIDE;
    if (!SOC_SaveSnapshot()) {
        s_soc = previous;
        return 0U;
    }
    SOC_PauseCalibration();
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

UINT8 SOC_HasPendingFull(void)
{
    return (UINT8)((s_soc.calibration_flags & SOC_TARGET_FULL) != 0U &&
                   s_soc.now_as10 < s_soc.full_as10);
}

UINT8 SOC_CellVoltagesValid(void)
{
    UINT8 i;
    if (SeriesNum < 5U || SeriesNum > 16U ||
        g_stCellInfoReport.u16VCellMin == 0U ||
        g_stCellInfoReport.u16VCellMin > g_stCellInfoReport.u16VCellMax ||
        g_stCellInfoReport.u16VCellMax > SOC_CELL_MAX_MV ||
        (UINT32)g_stCellInfoReport.u16VCellMax -
            g_stCellInfoReport.u16VCellMin > SOC_CELL_MAX_DELTA_MV) return 0U;
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

static void soc_smooth_towards(UINT32 target, UINT32 elapsed, UINT32 denominator)
{
    uint64_t credit;
    UINT32 step;
    if (elapsed > SOC_TICK_MS / 10U) elapsed = SOC_TICK_MS / 10U;
    credit = (uint64_t)s_soc.full_as10 * elapsed + s_cal.remainder;
    step = (UINT32)(credit / denominator);
    s_cal.remainder = (UINT32)(credit % denominator);
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
    UINT16 flags = s_soc.calibration_flags;
    UINT16 full_mv = OtherElement.u16Soc_V_100;
    UINT16 low_mv = OtherElement.u16Soc_V_0;
    UINT16 empty_mv = AFE_Parameters_RS485_Struction.u16VcellUvp.curValue;
    UINT16 min_mv = g_stCellInfoReport.u16VCellMin;
    UINT16 max_mv = g_stCellInfoReport.u16VCellMax;
    UINT32 target;

    /* No charger-specific tail current. Prior charging OR all cells near
     * SOC100 qualifies. Manual setting blocks static full until real current. */
    if (current_ma >= SOC_CHARGE_EVIDENCE_MA) {
        flags &= (UINT16)~SOC_MANUAL_OVERRIDE;
        if (!(flags & SOC_TARGET_FULL) && s_soc.now_as10 < s_soc.full_as10)
            flags |= SOC_CHARGE_SEEN;
    } else if (current_ma <= -SOC_CHARGE_EVIDENCE_MA) {
        flags &= (UINT16)~(SOC_MANUAL_OVERRIDE | SOC_CHARGE_SEEN |
                           SOC_TARGET_FULL | SOC_FULL_LATCHED);
        s_cal.full_ticks = 0U;
        s_cal.remainder = 0U;
    }

    /* Preconfirm at SOC100-50mV; after 10s near the endpoint one brief
     * crossing of SOC100 is sufficient to catch a fast-cutoff charger.
     * This retains the original no-tail-current and no-jump requirements. */
    if (full_mv > low_mv && full_mv <= SOC_CELL_MAX_MV &&
        !(flags & (SOC_TARGET_FULL | SOC_MANUAL_OVERRIDE)) &&
        current_ma > -SOC_CHARGE_EVIDENCE_MA &&
        ((flags & SOC_CHARGE_SEEN) ||
         min_mv >= (UINT16)(full_mv - SOC_FULL_REST_MARGIN_MV)) &&
        max_mv >= (UINT16)(full_mv - SOC_FULL_PREPARE_MV) &&
        min_mv > empty_mv) {
        s_cal.full_ticks = soc_confirm_ticks(s_cal.full_ticks, elapsed);
        if (s_cal.full_ticks == SOC_CONFIRM_TICKS && max_mv >= full_mv) {
            flags = (UINT16)((flags & (UINT16)~(SOC_TARGET_EMPTY | SOC_CHARGE_SEEN)) |
                SOC_TARGET_FULL | SOC_FULL_LATCHED);
            s_soc.low_entry_as10 = 0U;
            s_cal.full_ticks = 0U;
            s_cal.remainder = 0U;
        }
    } else if (!(flags & SOC_TARGET_FULL)) {
        s_cal.full_ticks = 0U;
    }
    if (full_mv <= low_mv || full_mv > SOC_CELL_MAX_MV ||
        ((flags & SOC_TARGET_FULL) && min_mv <= low_mv)) {
        /* Relaxation at normal voltage is fine. A collapse into the
         * low-SOC window invalidates an unfinished upper calibration. */
        flags &= (UINT16)~(SOC_TARGET_FULL | SOC_FULL_LATCHED | SOC_CHARGE_SEEN);
        s_cal.remainder = 0U;
    }

    if (flags != s_soc.calibration_flags) s_saved.valid = 0U;
    s_soc.calibration_flags = flags;

    /* After a trustworthy full confirmation, keep the 100% target through
     * charger stop and voltage relaxation, tolerating zero-current noise.
     * Ordinary rest has no upward SOC correction. */
    if (flags & SOC_TARGET_FULL) {
        s_cal.low_ticks = 0U;
        if (current_ma > -SOC_CHARGE_EVIDENCE_MA) {
            soc_smooth_towards(s_soc.full_as10, elapsed,
                               SOC_FULL_SMOOTH_DENOMINATOR);
            if (s_soc.now_as10 == s_soc.full_as10) {
                s_soc.calibration_flags &= (UINT16)~(SOC_TARGET_FULL |
                    SOC_FULL_LATCHED | SOC_CHARGE_SEEN);
                s_saved.valid = 0U;
            }
        } else s_cal.remainder = 0U;
        return;
    }

    /* Only discharging cells may receive a low-end correction. SOC_V_0 is
     * the start of the smoothing window; the actual AFE UV is the end. */
    if (low_mv > empty_mv && low_mv <= SOC_CELL_MAX_MV &&
        current_ma < 0 && min_mv <= low_mv) {
        s_cal.low_ticks = soc_confirm_ticks(s_cal.low_ticks, elapsed);
        if (s_cal.low_ticks == SOC_CONFIRM_TICKS) {
            if (s_soc.low_entry_as10 == 0U && s_soc.now_as10 != 0U) {
                s_soc.low_entry_as10 = s_soc.now_as10;
                s_saved.valid = 0U;
            }
            target = 0U;
            if (min_mv > empty_mv)
                target = (UINT32)((uint64_t)s_soc.low_entry_as10 *
                                  (min_mv - empty_mv) / (low_mv - empty_mv));
            if (s_soc.now_as10 > target) {
                soc_smooth_towards(target, elapsed, SOC_SMOOTH_DENOMINATOR);
                return;
            }
        }
    } else s_cal.low_ticks = 0U;
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
        !AFE_PARAM_WRITE_Flag && SOC_CellVoltagesValid()) {
        elapsed = soc_calibration_elapsed();
        /* Reuse the 10s confirmation to reject tiny positive noise pulses.
         * This only retires an old low reference; it never qualifies full. */
        if (current_ma >= SOC_CHARGE_EVIDENCE_MA && (s_soc.low_entry_as10 ||
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
