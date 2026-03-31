#ifndef CONF_H
#define CONF_H

#include "stdio.h"
#include "stdint.h"
#include "stdbool.h"
#include "stm32f10x.h"
//#include "stm32f0xx.h"
#include "conf_gpio.h"

#define EEPROM_VALUE_BEGIN_FLAG				0x0331		//Ĭ��0x1133������Լ���Ҫˢһ�飬���Լ������ٸĻ�0x1133

#define  wdog_enable
// #define __FUNC_RTC__
// #define __FUNC__HEAT__
// #define __FUNC__CAN__
// #define __LOAD_REMOVE_SHORT_FUNC__

// #define _SECOND_CURR_PROTECT_FUNC_

// #define __VIRTURE_CURRENT__

#define _DI_SWITCH_SYS_ONOFF	//DI������������
//#define _DI_SWITCH_DSG_ONOFF	//DI�����������Ʒŵ�Ӵ�������MOS
// #define _DI_SWITCH_longKEY_ONOFF



#define VERSION         (5)


#define   CURR_10A     0
#define   CURR_20A     1
#define   CURR_40A     2

#define bq76xx_afe  0
#define sh36xx      1


#define   LEVEL_CURR     CURR_40A
#define   AFE_TYPE        sh36xx

#if (LEVEL_CURR == CURR_10A)
#define CS_Res			2
#define CS_Res_Num		2
#define CBC_DelayT		1280
#define CBC_Cur_DSG		(1000)

#define OCC_1       (200) 
#define OCC_2       (200) 
#define OCC_3       (200) 
#define OCC_recover (100) 
#define OCC_filter1  (100 * 5) 
#define OCC_filter2  (100 * 5) 
#define OCC_filter3  10 

#define ODC_1       (300) 
#define ODC_2       (300) 
#define ODC_3       (300) 
#define ODC_recover (100) 
#define ODC_filter1  (100 * 5) 
#define ODC_filter2  (100 * 5) 
#define ODC_filter3  10

//***********AFE*****
#define AFE_OCC1       		(500) 
#define AFE_OCC1_filter  	(10)
#define AFE_OCC2       		(500) 
#define AFE_OCC2_filter  	(10)

#define AFE_ODC1       		(500) 
#define AFE_ODC1_filter  	(10)
#define AFE_ODC2       		(500) 
#define AFE_ODC2_filter  	(10)

#elif (LEVEL_CURR == CURR_20A)
#define CS_Res			2
#define CS_Res_Num		3
#define CBC_DelayT		1280
#define CBC_Cur_DSG		(1000)

#define OCC_1       (300) 
#define OCC_2       (300) 
#define OCC_3       (400) 
#define OCC_recover (100) 
#define OCC_filter1  (100 * 5) 
#define OCC_filter2  (100 * 5) 
#define OCC_filter3  10 

#define ODC_1       (300) 
#define ODC_2       (300) 
#define ODC_3       (400) 
#define ODC_recover (100) 
#define ODC_filter1  (100 * 5) 
#define ODC_filter2  (100 * 5) 
#define ODC_filter3  10

//***********AFE*****
#define AFE_OCC1       		(500) 
#define AFE_OCC1_filter  	(10)
#define AFE_OCC2       		(500) 
#define AFE_OCC2_filter  	(10)

#define AFE_ODC1       		(500) 
#define AFE_ODC1_filter  	(10)
#define AFE_ODC2       		(500) 
#define AFE_ODC2_filter  	(10)

#elif (LEVEL_CURR == CURR_40A)
#define CS_Res			2
#define CS_Res_Num		6
#define CBC_DelayT		1280
#define CBC_Cur_DSG		(2000)

#define OCC_1       (200) 
#define OCC_2       (300) 
#define OCC_3       (300) 
#define OCC_recover (100) 
#define OCC_filter1  (100 * 5) 
#define OCC_filter2  (100 * 5) 
#define OCC_filter3  10 

#define ODC_1       (600) 
#define ODC_2       (600) 
#define ODC_3       (600) 
#define ODC_recover (100) 
#define ODC_filter1  (100 * 5) 
#define ODC_filter2  (100 * 5) 
#define ODC_filter3  10
//***********AFE*****
#define AFE_OCC1       		(500) 
#define AFE_OCC1_filter  	(10)
#define AFE_OCC2       		(500) 
#define AFE_OCC2_filter  	(10)

#define AFE_ODC1       		(1000) 
#define AFE_ODC1_filter  	(10)
#define AFE_ODC2       		(1000) 
#define AFE_ODC2_filter  	(10)
#endif	

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

  uint16_t    occ1_cnt;
  uint16_t    occ2_cnt;
  uint16_t    occ3_cnt;

  uint16_t    odc1_cnt;
  uint16_t    odc2_cnt;
  uint16_t    odc3_cnt;

  uint32_t    test_driver_cnt;
  uint64_t    test_main_cycle;
  uint32_t    App_AFEGet_cnt;
  uint32_t    App_SH367309_Monitor_cnt;
  uint32_t    App_SleepDeal_cnt;
  uint32_t    App_beep_cnt;

  uint32_t    sci1_irq_cnt;
  uint32_t    sci2_irq_cnt;
  uint32_t    sci3_irq_cnt;

  uint16_t    test_afe_write_cnt;
  uint16_t    test_compare_cnt;
  uint16_t    test_compare_exceptioncnt;

  uint16_t    uart1_ore_err;
  uint16_t    uart2_ore_err;
  uint16_t    uart2_err2;
  uint16_t    uart2_err3;
  uint16_t    uart2_err4;

  uint16_t    test_current_cnt;
  uint16_t    test_sci2_err_cnt;

  uint16_t    cnt_PA0_irq;
  uint16_t cnt_bms1_keyirq;
  uint16_t    bq33100_read_cnt;
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
