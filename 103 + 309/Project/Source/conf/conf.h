#ifndef CONF_H
#define CONF_H

#include "stdio.h"
#include "stdint.h"
#include "stdbool.h"
#include "stm32f10x.h"
#include "conf_gpio.h"
#include "Project_Config.h"

// #define __SOC_5_PROTECT_
// #define DISP_VBAT_AND_TEMP_
// #define __EnableLowPowerDebug__
// #define VCELL_DISP_TEST
#define _COMMOM_UPPER_SCI2

// #define __VIRTURE_CURRENT__

#define T3MAX         0
#define T3            1

#define BAT_MASTER  (0)
#define BAT_SLAVE   (1)

#define BAT_TYPE     PROJECT_CFG_BAT_TYPE

#define FD_YEAR      PROJECT_CFG_FD_YEAR
#define FD_MONTH     PROJECT_CFG_FD_MONTH
#define FD_DAY       PROJECT_CFG_FD_DAY

#if (PROJECT_CFG_BAT_CHEMISTRY == 0)
#define TERNARYLI
#elif (PROJECT_CFG_BAT_CHEMISTRY == 1)
#define LIFEPO
#else
#error "Invalid PROJECT_CFG_BAT_CHEMISTRY"
#endif

#if PROJECT_CFG_WDOG_ENABLE
#define wdog_enable
#endif

#if PROJECT_CFG_RTC_ENABLE
#define __FUNC_RTC__
#endif

#if (PROJECT_CFG_SCI1_ROLE == 1)
#define _COMMOM_UPPER_SCI1
#elif (PROJECT_CFG_SCI1_ROLE != 0)
#error "Invalid PROJECT_CFG_SCI1_ROLE"
#endif

#define VERSION  (PROJECT_CFG_VERSION)

/* Board shunt values and software current-protection defaults.
 * AFE hardware thresholds live in afe3520/Afe3520Config.h. */
#define CS_Res			2
#define CS_Res_Num		8 /* Reference board: eight 2 mOhm resistors in parallel = 0.25 mOhm. */
#define CBC_DelayT		128
#define CBC_Cur_DSG		(50)

#define BMS_SW_OCC1       		(120)
#define BMS_SW_OCC2       		(120)
#define BMS_SW_ODC1       		(150)
#define BMS_SW_ODC2       		(150)

typedef enum GPIO_TYPE {
	GPIO_PreCHG = 0,
	GPIO_CHG,
	GPIO_DSG,
	GPIO_MAIN,
}GPIO_Type;

typedef struct
{
  uint32_t    can_rcv_cnt_test;
  uint32_t    last_ext_comm_cnt_can;
  volatile uint32_t can_rcv_cnt;
  uint64_t    test_main_cycle;
  uint32_t    App_AFEGet_cnt;

  volatile uint32_t sci1_irq_cnt;
  volatile uint32_t sci2_irq_cnt;
  volatile uint32_t sci3_irq_cnt;

  volatile uint16_t cnt_PA0_irq;
  volatile uint16_t cnt_bms1_keyirq;
  uint16_t    pec_err_cnt;

  uint16_t    CHG;
  uint16_t    DSG;

  uint16_t    cnt_enter_chg_open;
  uint16_t    cnt_enter_dsg_open;

  uint8_t     wakeup_reason;
  bool        wakeup_rtc;
  uint8_t     time_enter_rtc;
  bool        power_on;

  uint16_t    enter_rtc_delay;
  uint32_t    rtc_sleep_cnt;
  uint32_t    rtc_sec_cnt;
  volatile uint32_t rtc_alm_cnt;
  uint32_t    rtc_irq_cnt;

  uint8_t     isdebugenable;
  bool        typec_curr_sim;
  uint16_t    typc_curr;
}Time_T;

extern Time_T  sys_time;

void InitIO(void);
void InitWakeUp_Base(void);
void InitWakeUp_NormalMode(void);
void InitWakeUp_RTCMode(void);
void InitWakeUp_DeepMode(void);
void IOstatus_Base(void);
void IOstatus_RTCMode(void);
void IOstatus_NormalMode(void);
void IOstatus_DeepMode(void);
void LowPower_ClearWakeupPending(void);
void LowPower_DisableWakeupExti(void);
void Sys_StopMode(void);
void InitRunAfterStopWakeup(void);

#include "Project_BuildGuard.h"

#endif
