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
#define LP_BLOCK_EXT_COMM     (1UL << 4)
#define LP_BLOCK_FLASH_BUSY   (1UL << 5)
#define LP_BLOCK_UPGRADE      (1UL << 6)
#define LP_BLOCK_FAULT        (1UL << 7)
#define LP_BLOCK_LED_ACTIVE   (1UL << 8)
#define LP_BLOCK_AGING        (1UL << 9)

struct LOW_POWER_RTC_STATUS {
  uint8_t mode;
  uint8_t rtc;
  uint8_t comm;
  uint8_t reserved;
  uint16_t idle;
  uint16_t idleMax;
  uint16_t force;
  uint16_t reserved16;
  uint32_t vlow;
  uint32_t block;
  uint32_t sleep;
  uint32_t last;
  uint32_t cycles;
  uint16_t test_sample_voltage;
};

extern volatile struct LOW_POWER_RTC_STATUS g_stLowPowerRtcStatus;
extern enum irqWakeup g_irq_t;

void LowPower_Request(enum _SLEEP_MODE mode);
uint32_t LP_GetBlockReason(void);
void rtc_sleep(void);
void cpu_frequency_conf(void);

#endif
