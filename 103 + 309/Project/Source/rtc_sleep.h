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



#define enumToStr(WEEK)    #WEEK

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
  uint8_t readyToSleep;
  uint8_t blockReason;
  uint8_t rtcWake;
  uint16_t delaySeconds;
  uint16_t delayTargetSeconds;
  uint32_t elapsedSeconds;
};

void App_LowPowerProcess(void);
void LowPower_Request(enum _SLEEP_MODE mode);
void LowPower_ClearToSleepFlag(void);
uint8_t LowPower_IsToSleepPending(void);
void rtc_sleep(void);
void cpu_frequency_conf(void);

uint8_t get_rtc_soc(void);
void set_rtc_soc(uint8_t _soc);

// void set_irq_wksource(enum irqWakeup irq);
void set_irq_wksource(uint8_t irq);


void entersleep(enum _SLEEP_MODE mode);

void sleep(void);



#endif
