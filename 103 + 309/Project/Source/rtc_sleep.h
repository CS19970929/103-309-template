#ifndef __RTC_SLEEP__
#define __RTC_SLEEP__

#include <stdint.h>
#include <stdbool.h>

// #include "stm32f0xx_it.h"			//锟斤拷锟斤拷锟斤拷一些硬锟斤拷锟斤拷锟斤拷之锟斤拷锟斤拷卸希锟斤拷锟斤拷锟斤拷锟揭拷锟?
enum irqWakeup
{
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
typedef enum _SLEEP_MODE {
NORMAL_MODE = 0, HICCUP_MODE, DEEP_MODE, NO_SLEEP,
}SLEEP_MODE;

#define LP_BLOCK_CHARGE       (1UL << 0)
#define LP_BLOCK_DISCHARGE    (1UL << 1)
#define LP_BLOCK_COMM         (1UL << 2)
#define LP_BLOCK_KEY          (1UL << 3)
#define LP_BLOCK_FLASH_BUSY   (1UL << 5)
#define LP_BLOCK_UPGRADE      (1UL << 6)
#define LP_BLOCK_FAULT        (1UL << 7)
#define LP_BLOCK_LED_ACTIVE   (1UL << 8)

enum LOW_POWER_RTC_BLOCK_REASON {
  LOW_POWER_RTC_BLOCK_NONE = 0,
  LOW_POWER_RTC_BLOCK_CURRENT,
  LOW_POWER_RTC_BLOCK_RESERVED_2,
  LOW_POWER_RTC_BLOCK_MOS_OFF,
  LOW_POWER_RTC_BLOCK_MCU_WAKE,
  LOW_POWER_RTC_BLOCK_FACTORY_AGING,
  LOW_POWER_RTC_BLOCK_EXT_COMM,
  LOW_POWER_RTC_BLOCK_AFE_NOT_IDLE,
  LOW_POWER_RTC_BLOCK_FRAMEWORK
};

struct LOW_POWER_RTC_STATUS {
  uint8_t mode;
  uint8_t blockReason;
  uint8_t rtcWake;
  uint16_t delaySeconds;
  uint16_t delayTargetSeconds;
  uint32_t elapsedSeconds;
};

extern volatile struct LOW_POWER_RTC_STATUS g_stLowPowerRtcStatus;
extern enum irqWakeup g_irq_t;

void LowPower_Request(enum _SLEEP_MODE mode);
uint32_t LP_GetBlockReason(void);
uint32_t LP_GetLastSleepSeconds(void);
void LP_RecordLastSleepSeconds(uint32_t seconds);
void rtc_sleep(void);
void cpu_frequency_conf(void);

#endif
