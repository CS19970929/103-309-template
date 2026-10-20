#include "low_power_port.h"

#define LP_MAX_SPURIOUS_RETURNS 8U
volatile LowPowerDiag g_low_power_diag;
static volatile uint32_t s_pending_wake;
static uint32_t s_start_counter;
static uint8_t s_board_prepared;
static uint8_t s_spurious_returns;

/* Kept out-of-line so Keil can break here even with compiler optimization. */
#if defined(__CC_ARM)
__attribute__((noinline))
#endif
void LowPower_DebugHook(uint32_t state, uint32_t reason)
{
    (void)state;
    (void)reason;
}

static void lp_set_state(LowPowerState state, uint32_t reason)
{
    uint32_t sequence;
    uint32_t index;

    g_low_power_diag.state = (uint32_t)state;
    BoardLowPower_Snapshot();
    sequence = ++g_low_power_diag.trace_sequence;
    index = (sequence - 1U) % LOW_POWER_TRACE_COUNT;
    g_low_power_diag.trace[index].sequence = 0U;
    g_low_power_diag.trace[index].state = (uint32_t)state;
    g_low_power_diag.trace[index].reason = reason;
    g_low_power_diag.trace[index].rtc_counter = g_low_power_diag.rtc_counter;
    g_low_power_diag.trace[index].exti_pr = g_low_power_diag.exti_pr;
    g_low_power_diag.trace[index].sequence = sequence;
    LowPower_DebugHook((uint32_t)state, reason);
}

void LowPower_Init(void)
{
    g_low_power_diag.magic = 0x4C504431UL;
    g_low_power_diag.version = 1U;
    g_low_power_diag.debug_profile = LOW_POWER_DEBUG_PROFILE;
    BoardLowPower_Init();
    lp_set_state(LP_RUN, 0U);
}

void LowPower_Request(LowPowerTarget target, uint32_t reason)
{
    g_low_power_diag.request_reason = reason;
    BmsSleep_Request(target);
}

void LowPower_NotifyWake(uint32_t sources)
{
    uint32_t mask = BoardLowPower_Lock();
    /* RUN interrupts are application activity, not a STOP wake history. */
    if (g_low_power_diag.state != LP_RUN && sources != 0U)
    {
        s_pending_wake |= sources;
        g_low_power_diag.wake_mask |= sources;
        if (g_low_power_diag.first_wake == 0U)
            g_low_power_diag.first_wake = sources & (0U - sources);
    }
    BoardLowPower_Unlock(mask);
}

void LowPower_RecordFault(uint32_t faults)
{
    uint32_t mask = BoardLowPower_Lock();
    g_low_power_diag.fault_mask |= faults;
    BoardLowPower_Unlock(mask);
}

const volatile LowPowerDiag *LowPower_GetDiag(void)
{
    return &g_low_power_diag;
}

static LowPowerRunResult lp_restore(uint8_t aborted)
{
    lp_set_state(LP_RESTORE, g_low_power_diag.exit_mask |
                 g_low_power_diag.fault_mask | g_low_power_diag.block_mask);
    if (s_board_prepared)
    {
        BoardLowPower_AccountSeconds(BoardLowPower_RtcCounter() - s_start_counter);
        BoardLowPower_Restore();
        ++g_low_power_diag.restore_count;
    }
    if (aborted)
        ++g_low_power_diag.abort_count;
    s_board_prepared = 0U;
    BmsSleep_ResetQualification();
    lp_set_state(LP_RUN, g_low_power_diag.wake_mask);
    return LP_RUN_APPLICATION;
}

LowPowerRunResult LowPower_Poll(void)
{
    uint32_t mask;
    uint32_t external;
    uint8_t quiet;

    switch ((LowPowerState)g_low_power_diag.state)
    {
    case LP_RUN:
        BmsSleep_PollRequest();
        g_low_power_diag.target = (uint32_t)BmsSleep_GetTarget();
        if (g_low_power_diag.target == LP_TARGET_RUN)
            return LP_RUN_APPLICATION;
        g_low_power_diag.block_mask = BmsSleep_BlockMask(1U);
        /* Preserve the forced reset-sleep policy; MOS/protection/low voltage
           are its reasons to sleep, not RTC-idle eligibility requirements. */
        if (g_low_power_diag.target != LP_TARGET_RTC)
            g_low_power_diag.block_mask &= LP_BLOCK_COMM | LP_BLOCK_STORAGE;
        if (g_low_power_diag.block_mask != 0U)
            return LP_RUN_APPLICATION;
        if (g_low_power_diag.target != LP_TARGET_RTC)
        {
            if (!BmsSleep_CommitReset())
            {
                LowPower_RecordFault(LP_FAULT_RESET_SLEEP);
                return lp_restore(1U);
            }
            return LP_OWNS_CPU;
        }
        if (!BoardLowPower_RtcInit())
        {
            LowPower_RecordFault(LP_FAULT_RTC_INIT);
            return lp_restore(1U);
        }
        mask = BoardLowPower_Lock();
        s_pending_wake = 0U;
        g_low_power_diag.wake_mask = 0U;
        g_low_power_diag.first_wake = 0U;
        g_low_power_diag.exit_mask = 0U;
        g_low_power_diag.fault_mask = 0U;
        s_spurious_returns = 0U;
        lp_set_state(LP_PREPARE, 0U);
        BoardLowPower_Unlock(mask);
        return LP_OWNS_CPU;

    case LP_PREPARE:
        g_low_power_diag.block_mask = BmsSleep_BlockMask(1U);
        if (g_low_power_diag.block_mask != 0U || !BoardLowPower_Begin())
            return lp_restore(1U);
        s_board_prepared = 1U;
        s_start_counter = BoardLowPower_RtcCounter();
        ++g_low_power_diag.prepare_count;
        lp_set_state(LP_WAIT_QUIET, 0U);
        return LP_OWNS_CPU;

    case LP_WAIT_QUIET:
        /* The millisecond timer and interrupts remain live. The application
           stays gated while its UART/ADC/power GPIOs belong to this module. */
        external = BoardLowPower_ActiveWake() & ~LP_WAKE_COMM;
        LowPower_NotifyWake(external);
        if ((s_pending_wake & ~LP_WAKE_RTC) != 0U ||
            BmsSleep_GetTarget() != LP_TARGET_RTC)
            return lp_restore(1U);
        quiet = BoardLowPower_WaitQuiet();
        if (quiet == 2U)
        {
            LowPower_RecordFault(LP_FAULT_PREPARE_TIMEOUT);
            return lp_restore(1U);
        }
        if (quiet == 1U)
            lp_set_state(LP_COMMIT, 0U);
        return LP_OWNS_CPU;

    case LP_COMMIT:
        /* No AFE or EEPROM transactions in the final interrupt-masked handoff. */
        g_low_power_diag.block_mask = BmsSleep_BlockMask(0U);
        if (g_low_power_diag.block_mask != 0U ||
            BmsSleep_GetTarget() != LP_TARGET_RTC)
            return lp_restore(1U);
        mask = BoardLowPower_Lock();
        s_pending_wake &= ~LP_WAKE_RTC;
        BoardLowPower_Unlock(mask);
        if (!BoardLowPower_RtcArm())
        {
            LowPower_RecordFault(LP_FAULT_RTC_ARM);
            return lp_restore(1U);
        }
        mask = BoardLowPower_Lock();
        BoardLowPower_PrepareNextStop();
        BoardLowPower_ArmWake();
        LowPower_NotifyWake(BoardLowPower_ActiveWake());
        if ((s_pending_wake & ~LP_WAKE_RTC) != 0U)
        {
            BoardLowPower_Unlock(mask);
            return lp_restore(1U);
        }
        lp_set_state(LP_STOP, 0U);
        ++g_low_power_diag.stop_count;
        BoardLowPower_Stop();
        lp_set_state(LP_WAKE_CAPTURE, 0U);
        if (!BoardLowPower_CaptureWake())
            LowPower_RecordFault(LP_FAULT_RTC_SYNC);
        BoardLowPower_Unlock(mask);
        external = s_pending_wake & ~LP_WAKE_RTC;
        if (external != 0U)
        {
            ++g_low_power_diag.external_wake_count;
            return lp_restore(0U);
        }
        if (g_low_power_diag.fault_mask != 0U)
            return lp_restore(1U);
        if ((s_pending_wake & LP_WAKE_RTC) == 0U)
        {
            ++g_low_power_diag.spurious_count;
            if (++s_spurious_returns >= LP_MAX_SPURIOUS_RETURNS)
            {
                LowPower_RecordFault(LP_FAULT_SPURIOUS_LIMIT);
                return lp_restore(1U);
            }
            lp_set_state(LP_COMMIT, 0U);
            return LP_OWNS_CPU;
        }
        s_spurious_returns = 0U;
        ++g_low_power_diag.rtc_wake_count;
        lp_set_state(LP_RTC_SAMPLE, 0U);
        return LP_OWNS_CPU;

    case LP_RTC_SAMPLE:
        BoardLowPower_BeginSample();
        quiet = BoardLowPower_PollSample();
        if (quiet == 0U)
            return LP_OWNS_CPU;
        if (quiet == 2U)
        {
            LowPower_RecordFault(LP_FAULT_ADC);
            return lp_restore(1U);
        }
        g_low_power_diag.exit_mask = BmsSleep_SampleExit();
        if (g_low_power_diag.exit_mask != 0U ||
            (s_pending_wake & ~LP_WAKE_RTC) != 0U ||
            BmsSleep_GetTarget() != LP_TARGET_RTC)
            return lp_restore(0U);
#if LOW_POWER_DEBUG_PROFILE == LP_DEBUG_SIMULATE
        g_low_power_diag.exit_mask |= LP_EXIT_REQUEST;
        return lp_restore(0U);
#else
        lp_set_state(LP_COMMIT, 0U);
        return LP_OWNS_CPU;
#endif

    default:
        return lp_restore(1U);
    }
}
