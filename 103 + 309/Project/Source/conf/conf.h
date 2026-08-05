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

#if PROJECT_CFG_UART1_WAKEUP_ENABLE
#define UART1_WAKEUP_ENABLE
#endif

#if PROJECT_CFG_RS485_WAKEUP_ENABLE
#define RS485_WAKEUP_ENABLE
#endif

#if PROJECT_CFG_VIRTUAL_CURRENT_ENABLE
#define __VIRTURE_CURRENT__
#endif

#if PROJECT_CFG_DI_SWITCH_LONGKEY_ONOFF_ENABLE
#define _DI_SWITCH_longKEY_ONOFF
#endif

#if (PROJECT_CFG_SCI1_ROLE == 1)
#define _COMMOM_UPPER_SCI1
#elif (PROJECT_CFG_SCI1_ROLE != 0)
#error "Invalid PROJECT_CFG_SCI1_ROLE"
#endif

#define VERSION  (PROJECT_CFG_VERSION)

#define   CURR_10A     0
#define   CURR_20A_15A     1
#define   CURR_30A     2
#define   CURR_40A     3

#define bq76xx_afe  0
#define sh36xx      1

#define LEVEL_CURR  CURR_20A_15A
#define AFE_TYPE    PROJECT_CFG_AFE_TYPE

#if (LEVEL_CURR == CURR_10A)
#define CS_Res			2
#define CS_Res_Num		2
#define CBC_DelayT		128
#define CBC_Cur_DSG		(50)
#elif (LEVEL_CURR == CURR_20A_15A)
#define CS_Res			2
#define CS_Res_Num		3
#define CBC_DelayT		128
#define CBC_Cur_DSG		(100)

#elif (LEVEL_CURR == CURR_30A)
#define CS_Res			2
#define CS_Res_Num		3
#define CBC_DelayT		128
#define CBC_Cur_DSG		(80)
#elif (LEVEL_CURR == CURR_40A)
#define CS_Res			2
#define CS_Res_Num		6
#define CBC_DelayT		128
#define CBC_Cur_DSG		(100)
#endif	

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
  uint32_t    can_rcv_cnt;
  uint64_t    test_main_cycle;
  uint32_t    App_AFEGet_cnt;
  uint32_t    App_SH367309_Monitor_cnt;

  uint32_t    sci1_irq_cnt;
  uint32_t    sci2_irq_cnt;
  uint32_t    sci3_irq_cnt;

  uint16_t    cnt_PA0_irq;
  uint16_t    cnt_bms1_keyirq;
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
  uint32_t    rtc_alm_cnt;
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
