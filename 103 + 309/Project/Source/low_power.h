#ifndef LOW_POWER_H
#define LOW_POWER_H

#include <stdint.h>

typedef enum {
    LP_RUN = 0, LP_PREPARE, LP_WAIT_QUIET, LP_COMMIT, LP_STOP,
    LP_WAKE_CAPTURE, LP_RTC_SAMPLE, LP_RESTORE
} LowPowerState;

typedef enum {
    LP_TARGET_RUN = 0, LP_TARGET_RTC, LP_TARGET_NORMAL_RESET, LP_TARGET_DEEP_RESET
} LowPowerTarget;

typedef enum { LP_RUN_APPLICATION = 0, LP_OWNS_CPU } LowPowerRunResult;

enum {
    LP_BLOCK_COMM = 1UL << 0, LP_BLOCK_STORAGE = 1UL << 1,
    LP_BLOCK_CURRENT = 1UL << 2, LP_BLOCK_HEAT = 1UL << 3,
    LP_BLOCK_BALANCE = 1UL << 4, LP_BLOCK_MOS = 1UL << 5,
    LP_BLOCK_AFE = 1UL << 6, LP_BLOCK_WAKE_PIN = 1UL << 7,
    LP_BLOCK_COMM_CHANGED = 1UL << 8, LP_BLOCK_CONFIG = 1UL << 9,
    LP_BLOCK_PROTECTION = 1UL << 10
};

enum {
    LP_WAKE_RTC = 1UL << 0, LP_WAKE_PA0 = 1UL << 1,
    LP_WAKE_KEY = 1UL << 2, LP_WAKE_COMM = 1UL << 3,
    LP_WAKE_UART1 = 1UL << 4, LP_WAKE_UART2 = 1UL << 5,
    LP_WAKE_UART3 = 1UL << 6, LP_WAKE_OTHER = 1UL << 7
};

enum {
    LP_EXIT_CURRENT = 1UL << 0, LP_EXIT_AFE = 1UL << 1,
    LP_EXIT_MOS = 1UL << 2, LP_EXIT_UV = 1UL << 3,
    LP_EXIT_OV = 1UL << 4, LP_EXIT_CONFIG = 1UL << 5,
    LP_EXIT_REQUEST = 1UL << 6
};

enum {
    LP_FAULT_RTC_INIT = 1UL << 0, LP_FAULT_RTC_ARM = 1UL << 1,
    LP_FAULT_RTC_SYNC = 1UL << 2, LP_FAULT_PREPARE_TIMEOUT = 1UL << 3,
    LP_FAULT_CLOCK = 1UL << 4, LP_FAULT_RESET_SLEEP = 1UL << 5,
    LP_FAULT_SPURIOUS_LIMIT = 1UL << 6, LP_FAULT_NO_TIMEBASE = 1UL << 7,
    LP_FAULT_STORAGE_WRITE = 1UL << 8, LP_FAULT_STORAGE_RANGE = 1UL << 9
};

/* Set at build time: 0 = production STOP; 1 = STOP with debug support;
   2 = simulate one RTC sample without WFI, then restore RUN. */
#define LP_DEBUG_RELEASE 0U
#define LP_DEBUG_REAL_STOP 1U
#define LP_DEBUG_SIMULATE 2U
#ifndef LOW_POWER_DEBUG_PROFILE
#define LOW_POWER_DEBUG_PROFILE LP_DEBUG_RELEASE
#endif
#if LOW_POWER_DEBUG_PROFILE > LP_DEBUG_SIMULATE
#error Invalid_LOW_POWER_DEBUG_PROFILE
#endif

#define LOW_POWER_TRACE_COUNT 8U
typedef struct {
    uint32_t sequence;
    uint32_t state;
    uint32_t reason;
    uint32_t rtc_counter;
    uint32_t exti_pr;
} LowPowerTrace;

typedef struct {
    uint32_t magic, version;
    uint32_t state, target, request_reason;
    uint32_t block_mask, wake_mask, first_wake, exit_mask, fault_mask;
    uint32_t prepare_count, stop_count, rtc_wake_count, external_wake_count;
    uint32_t restore_count, abort_count, spurious_count, quarantine_edges;
    uint32_t idle_seconds, uptime_ms, rtc_counter;
    uint32_t gpioa_idr, gpiob_idr, exti_imr, exti_pr;
    uint32_t nvic_pending[2], rtc_crl, rcc_cfgr, scb_icsr, primask;
    uint32_t debug_profile, iwdg_compiled, rcc_reset_flags;
    uint32_t comm_pending_mask, storage_pending_mask;
    uint32_t storage_write_failures, storage_last_address, storage_last_value;
    uint32_t uart_partial_timeouts, uart1_errors, uart2_errors;
    uint32_t trace_sequence;
    LowPowerTrace trace[LOW_POWER_TRACE_COUNT];
} LowPowerDiag;

extern volatile LowPowerDiag g_low_power_diag;
void LowPower_Init(void);
LowPowerRunResult LowPower_Poll(void);
void LowPower_Request(LowPowerTarget target, uint32_t reason);
void LowPower_NotifyWake(uint32_t sources);
void LowPower_RecordFault(uint32_t faults);
const volatile LowPowerDiag *LowPower_GetDiag(void);
/* Stable conditional-breakpoint site; no BKPT instruction in firmware. */
void LowPower_DebugHook(uint32_t state, uint32_t reason);

#endif
