#ifndef APP_LOWPOWER_H
#define APP_LOWPOWER_H

#include <stdint.h>

typedef enum
{
    LP_STATE_RUN = 0,
    LP_STATE_IDLE_CHECK,
    LP_STATE_PREPARE_SLEEP,
    LP_STATE_STOP_SLEEP,
    LP_STATE_WAKEUP_RESTORE,
    LP_STATE_DEEP_STANDBY,
    LP_STATE_ERROR
} LP_State_t;

#define LP_BLOCK_CHARGE       (1UL << 0)
#define LP_BLOCK_DISCHARGE    (1UL << 1)
#define LP_BLOCK_COMM         (1UL << 2)
#define LP_BLOCK_KEY          (1UL << 3)
#define LP_BLOCK_AFE_BUSY     (1UL << 4)
#define LP_BLOCK_FLASH_BUSY   (1UL << 5)
#define LP_BLOCK_UPGRADE      (1UL << 6)
#define LP_BLOCK_FAULT        (1UL << 7)
#define LP_BLOCK_LED_ACTIVE   (1UL << 8)
#define LP_BLOCK_IWDG_UNSAFE  (1UL << 9)

void LP_Init(void);
void LP_Task(void);
uint8_t LP_CanSleep(void);
uint8_t LP_CanEnterRtcSleep(void);
uint32_t LP_GetBlockReason(void);
uint32_t LP_GetRtcBlockReason(void);
void LP_SetWakeupPeriod(uint32_t seconds);
void LP_EnterStop(uint32_t seconds);
void LP_BeforeSleep(void);
void LP_AfterWakeup(void);
uint32_t LP_GetLastSleepSeconds(void);
LP_State_t LP_GetState(void);
void LP_RecordLastSleepSeconds(uint32_t seconds);

#endif
