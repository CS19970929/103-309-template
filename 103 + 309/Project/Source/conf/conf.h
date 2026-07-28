#ifndef CONF_H
#define CONF_H

#include "stdio.h"
#include "stdint.h"
#include "stdbool.h"
#include "stm32f10x.h"
//#include "stm32f0xx.h"
#include "conf_gpio.h"

#define EEPROM_VALUE_BEGIN_FLAG				0x1330		//Ĭ��0x1133������Լ����?ˢһ�飬���Լ������ٸĻ�0x1133

#define  wdog_enable
// #define __FUNC_RTC__
// #define __FUNC__HEAT__
#define __FUNC__CAN__
// #define __LOAD_REMOVE_SHORT_FUNC__

// #define _SECOND_CURR_PROTECT_FUNC_

// #define __VIRTURE_CURRENT__

//#define _DI_SWITCH_SYS_ONOFF	//DI������������
//#define _DI_SWITCH_DSG_ONOFF	//DI�����������Ʒŵ�Ӵ�������MOS
#define _DI_SWITCH_longKEY_ONOFF



#define VERSION         (5)


#define   CURR_80A      0
#define   CURR_100A     1
#define   CURR_150A     2
#define   CURR_200A     3
#define   CURR_250A     4

#define bq76xx_afe  0
#define sh36xx      1


#define   LEVEL_CURR     CURR_150A
#define   AFE_TYPE        sh36xx

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

#define MAX_BATSNUM_LEN			11

typedef struct 
{
  uint64_t    sys_tick_10ms;
  uint64_t    sys_tick_1ms;
  uint32_t    can_rcv_cnt;

  uint8_t isdebugenable;
	uint16_t CHG;
	uint16_t DSG;
  uint8_t  test_charger_online;

  uint16_t sim_vcell[19];

  uint16_t  cnt_enter_chg_open;
  uint16_t  cnt_enter_dsg_open;

  uint32_t    test_driver_cnt;
  uint64_t    test_main_cycle;
  uint32_t    App_AFEGet_cnt;
  uint32_t    App_SH367309_Monitor_cnt;
  uint32_t    App_SleepDeal_cnt;
  uint32_t    App_beep_cnt;

  uint16_t    test_afe_write_cnt;
  uint16_t    test_compare_cnt;
  uint16_t    test_compare_exceptioncnt;



  uint16_t    test_current_cnt;
  uint16_t    test_sci2_err_cnt;

  uint16_t    cnt_PA0_irq;
  uint16_t cnt_bms1_keyirq;
  uint16_t    bq33100_read_cnt;
  uint16_t    pec_err_cnt;
  



   uint8_t  wakeup_reason;
  bool     wakeup_rtc;
  uint8_t time_enter_rtc;
  bool power_on;
  uint16_t test_cnt1;

  uint16_t enter_rtc_delay;
  uint32_t rtc_sleep_cnt;
  
  bool     clear_cov;
  bool     clear_cuv;
  bool     clear_occ;
  bool     clear_odc1;
  bool     clear_odc2;
  bool     clear_short;
  bool     clear_otc;
  bool     clear_utc;
  bool     clear_otd;
  bool     clear_utd;

  bool     charger_online1;
  bool     charger_online2;
  bool     load_online1;
  bool     load_online2;
  bool     bal_cell[20];
  uint32_t bal_channel;
  uint16_t bal_time;
  bool crc_err;
	uint32_t crc_err_cnt;
  bool     can_enable;
  bool     canPow_enable;
  uint8_t  can_pow_sel;
  
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
void Sys_StopMode(void);
// void Init(void);


#endif
