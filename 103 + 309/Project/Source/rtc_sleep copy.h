#ifndef __RTC_SLEEP__
#define __RTC_SLEEP__


typedef enum _SLEEP_MODE {
NORMAL_MODE = 0, HICCUP_MODE, DEEP_MODE, NO_SLEEP,
}SLEEP_MODE;

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
extern enum irqWakeup g_irq_t;

extern bool is_wakeup;

void sleep(void);
void entersleep(enum _SLEEP_MODE mode);


#endif

