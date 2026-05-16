#ifndef CONF_H
#define CONF_H

#include "stdio.h"
#include "stdint.h"
#include "stdbool.h"
#include "Project_Types.h"
#include "Project_Features.h"
#include "stm32f10x.h"
//#include "stm32f0xx.h"
#include "conf_gpio.h"
#include "Project_Config.h"

#define _UL_RENZHENG_ENABLE_

#ifndef PROJECT_CFG_SOC_TEST_MODE_ENABLE
#define PROJECT_CFG_SOC_TEST_MODE_ENABLE 0
#endif

#ifndef PROJECT_CFG_SOC_TEST_ACCEL_TICKS_MAX
#define PROJECT_CFG_SOC_TEST_ACCEL_TICKS_MAX 300
#endif

#define EEPROM_VALUE_BEGIN_FLAG PROJECT_CFG_EEPROM_VALUE_BEGIN_FLAG

#define T3MAX         0
#define T3            1

// #define FD_BMS_TYPE   C11_AND_C11pro

#define BAT_MASTER  (0)    //20A
#define BAT_SLAVE   (1)    //40A

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




#if PROJECT_FEATURE_WATCHDOG
#define wdog_enable
#endif

#if PROJECT_FEATURE_RTC_LOW_POWER
#define __FUNC_RTC__
#endif

#if PROJECT_FEATURE_HEAT
#define __FUNC__HEAT__
#endif

#if PROJECT_CFG_LOAD_REMOVE_SHORT_ENABLE
#define __LOAD_REMOVE_SHORT_FUNC__
#endif

#if PROJECT_CFG_UART1_WAKEUP_ENABLE
#define UART1_WAKEUP_ENABLE
#endif

#if PROJECT_CFG_UART2_WAKEUP_ENABLE
#define UART2_WAKEUP_ENABLE
#endif

#if PROJECT_CFG_RS485_WAKEUP_ENABLE
#define RS485_WAKEUP_ENABLE
#endif

#if PROJECT_CFG_SECOND_CURR_PROTECT_ENABLE
#define _SECOND_CURR_PROTECT_FUNC_
#endif

#if PROJECT_CFG_VIRTUAL_CURRENT_ENABLE
#define __VIRTURE_CURRENT__
#endif

/* Enable only for destructive rear-64KB application storage test. */
#if PROJECT_CFG_FLASH64K_QUICK_TEST_ENABLE
#define FLASH64K_APP_QUICK_TEST_ENABLE
#endif
#define FLASH64K_APP_QUICK_TEST_CYCLES PROJECT_CFG_FLASH64K_QUICK_TEST_CYCLES

/* Enable for customer-use style SOC/AFE storage verification while app runs. */
#if PROJECT_CFG_FLASH64K_USE_TEST_ENABLE
#define FLASH64K_APP_USE_TEST_ENABLE
#endif
#define FLASH64K_APP_USE_TEST_PRINT_PERIOD_SEC PROJECT_CFG_FLASH64K_USE_TEST_PRINT_PERIOD_SEC

#if PROJECT_CFG_FLASH64K_USE_TEST_ACCEL_ENABLE
#define FLASH64K_APP_USE_TEST_ACCEL_ENABLE
#endif
#define FLASH64K_APP_USE_TEST_ACCEL_SOC_PERIOD_SEC PROJECT_CFG_FLASH64K_USE_TEST_ACCEL_SOC_PERIOD_SEC
#define FLASH64K_APP_USE_TEST_ACCEL_AFE_PERIOD_SEC PROJECT_CFG_FLASH64K_USE_TEST_ACCEL_AFE_PERIOD_SEC

#if PROJECT_CFG_DI_SWITCH_SYS_ONOFF_ENABLE
#define _DI_SWITCH_SYS_ONOFF
#endif

#if PROJECT_CFG_DI_SWITCH_DSG_ONOFF_ENABLE
#define _DI_SWITCH_DSG_ONOFF
#endif

#if PROJECT_CFG_DI_SWITCH_LONGKEY_ONOFF_ENABLE
#define _DI_SWITCH_longKEY_ONOFF
#endif

#if PROJECT_CFG_LED_FUNC_ENABLE
#define __FUNC__LED__
#endif

#if PROJECT_CFG_DEBUG_CODE_ENABLE
#define _DEBUG_CODE
#endif

#if PROJECT_CFG_SOC_TEST_MODE_ENABLE
#define SOC_TEST_MODE_ENABLE
#endif

#if PROJECT_CFG_SLEEP_WITH_CURRENT_ENABLE
#define _SLEEP_WITH_CURRENT
#endif

#if PROJECT_CFG_IAP_ENABLE
#define _IAP
#endif

#if (PROJECT_CFG_SCI1_ROLE == 1)
#define _COMMOM_UPPER_SCI1
#elif (PROJECT_CFG_SCI1_ROLE == 2)
#define _CLIENT_SCI1
#elif (PROJECT_CFG_SCI1_ROLE == 3)
#define _LCD_SCI1
#elif (PROJECT_CFG_SCI1_ROLE != 0)
#error "Invalid PROJECT_CFG_SCI1_ROLE"
#endif

#if (PROJECT_CFG_SCI2_ROLE == 1)
#define _COMMOM_UPPER_SCI2
#elif (PROJECT_CFG_SCI2_ROLE == 2)
#define _CLIENT_SCI2
#elif (PROJECT_CFG_SCI2_ROLE == 3)
#define _LCD_SCI2
#elif (PROJECT_CFG_SCI2_ROLE != 0)
#error "Invalid PROJECT_CFG_SCI2_ROLE"
#endif

#if (PROJECT_CFG_SCI3_ROLE == 1)
#define _COMMOM_UPPER_SCI3
#elif (PROJECT_CFG_SCI3_ROLE == 2)
#define _CLIENT_SCI3
#elif (PROJECT_CFG_SCI3_ROLE == 3)
#define _LCD_SCI3
#elif (PROJECT_CFG_SCI3_ROLE != 0)
#error "Invalid PROJECT_CFG_SCI3_ROLE"
#endif

#define VERSION         (PROJECT_CFG_VERSION)


#define   CURR_80A      0
#define   CURR_100A     1
#define   CURR_150A     2
#define   CURR_200A     3
#define   CURR_250A     4

#define bq76xx_afe  0
#define sh36xx      1


#define   LEVEL_CURR     PROJECT_CFG_LEVEL_CURR
#define   AFE_TYPE        PROJECT_CFG_AFE_TYPE

#ifdef __FUNC__HEAT__
#define CHG_LOWTEMP_PARAM   120
#define HEAT_OPEN_CURR      50
#else
#define CHG_LOWTEMP_PARAM   380
#define HEAT_OPEN_CURR      500
#endif // DEBUG

typedef enum GPIO_TYPE {
	GPIO_PreCHG = 0,
	GPIO_CHG,
	GPIO_DSG,
	GPIO_MAIN,
}GPIO_Type;

typedef struct 
{
  uint32_t    can_rcv_cnt;

  // uint16_t    cov1_cnt;
  // uint16_t    cov2_cnt;
  // uint16_t    cov3_cnt;

  // uint16_t    Bov1_cnt;
  // uint16_t    Bov2_cnt;
  // uint16_t    Bov3_cnt;

  // uint16_t    cuv1_cnt;
  // uint16_t    cuv2_cnt;
  // uint16_t    cuv3_cnt;

  // uint16_t    Buv1_cnt;
  // uint16_t    Buv2_cnt;
  // uint16_t    Buv3_cnt;

  // uint16_t    occ1_cnt;
  // uint16_t    occ2_cnt;
  // uint16_t    occ3_cnt;

  // uint16_t    odc1_cnt;
  // uint16_t    odc2_cnt;
  // uint16_t    odc3_cnt;

  uint32_t    test_driver_cnt;
  uint64_t    test_main_cycle;
  uint32_t    App_AFEGet_cnt;
  uint32_t    App_SH367309_Monitor_cnt;
  uint32_t    App_beep_cnt;

  uint32_t    sci1_irq_cnt;
  uint32_t    sci2_irq_cnt;
  uint32_t    sci3_irq_cnt;

  // uint16_t    test_afe_write_cnt;
  // uint16_t    test_compare_cnt;
  // uint16_t    test_compare_exceptioncnt;

  // uint16_t    uart1_ore_err;
  // uint16_t    uart2_ore_err;
  // uint16_t    uart2_err2;
  // uint16_t    uart2_err3;
  // uint16_t    uart2_err4;

  // uint16_t    test_current_cnt;
  // uint16_t    test_sci2_err_cnt;

  uint16_t    cnt_PA0_irq;
  uint16_t cnt_bms1_keyirq;
  uint16_t    pec_err_cnt;
  
  uint8_t isdebugenable;
	uint16_t CHG;
	uint16_t DSG;

  uint16_t  cnt_enter_chg_open;
  uint16_t  cnt_enter_dsg_open;

   uint8_t  wakeup_reason;
  bool     wakeup_rtc;
  uint8_t time_enter_rtc;
  bool power_on;
  uint16_t test_cnt1;

  uint16_t enter_rtc_delay;
  uint32_t rtc_sleep_cnt;
  uint32_t rtc_sec_cnt;
  uint32_t rtc_alm_cnt;
  uint32_t rtc_irq_cnt;

  uint16_t typc_curr;
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
void IORecover_RTCMode(void);
void IORecover_NormalMode(void);
void IORecover_DeepMode(void);
void LowPower_ClearWakeupPending(void);
void LowPower_DisableWakeupExti(void);
void Sys_StopMode(void);
void InitRtcWakeupCheck(void);
void InitRunAfterStopWakeup(void);
void Init(void);

#include "Project_BuildGuard.h"

#endif
