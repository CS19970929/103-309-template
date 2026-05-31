#ifndef LOW_POWER_H
#define LOW_POWER_H

#include <stdint.h>
#include <stdbool.h>

/* ── Sleep mode ── */
typedef enum {
    LP_SLEEP_NORMAL  = 0,
    LP_SLEEP_HICCUP  = 1,
    LP_SLEEP_DEEP    = 2,
    LP_SLEEP_NONE    = 3
} LP_SleepMode;

#define NORMAL_MODE  LP_SLEEP_NORMAL
#define HICCUP_MODE  LP_SLEEP_HICCUP
#define DEEP_MODE    LP_SLEEP_DEEP
#define NO_SLEEP     LP_SLEEP_NONE

/* ── Wakeup source ── */
enum irqWakeup {
    uart1_irq = 1,
    uart2_irq,
    uart3_irq,
    PA0_irq,
    bms_keyirq,
    soc_key,
    CHG_IRQ,
    current_wake,
    chg_dsg_close,
    error_wake,
    cuv_wake,
    cov_wake,
    rs485_irq,
    NO_IRQ
};

/* ── State machine ── */
typedef enum {
    LP_STATE_RUN = 0,
    LP_STATE_IDLE_CHECK,
    LP_STATE_STOP_SLEEP
} LP_State;

/* ── Block reason bitmap ── */
#define LP_BLOCK_CHARGE        (1UL << 0)
#define LP_BLOCK_DISCHARGE     (1UL << 1)
#define LP_BLOCK_COMM          (1UL << 2)
#define LP_BLOCK_KEY           (1UL << 3)
#define LP_BLOCK_AFE_BUSY      (1UL << 4)
#define LP_BLOCK_FLASH_BUSY    (1UL << 5)
#define LP_BLOCK_UPGRADE       (1UL << 6)
#define LP_BLOCK_FAULT         (1UL << 7)
#define LP_BLOCK_LED_ACTIVE    (1UL << 8)
#define LP_BLOCK_FACTORY_AGING (1UL << 9)

/* ── Public API ── */
void     LP_Init(void);
void     LP_Task(void);
uint8_t  LP_CanSleep(void);
uint32_t LP_GetBlockReason(void);
void     LP_RequestSleep(uint8_t mode);
uint8_t  LP_IsToSleepPending(void);
void     LP_ClearToSleepFlag(void);
uint32_t LP_GetLastSleepSeconds(void);
void     LP_RecordLastSleepSeconds(uint32_t seconds);
void     LP_NotifyExternalComm(void);

#endif /* LOW_POWER_H */