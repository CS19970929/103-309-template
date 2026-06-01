#ifndef SYSTEM_DEBUG_H
#define SYSTEM_DEBUG_H

#include "Project_Config.h"

#if PROJECT_CFG_DEBUG_MONITOR_ENABLE

#include <stdint.h>

/* ---- sub-structs for Keil Watch hierarchical browsing ---- */

struct DBG_GPIO {
	uint16_t a_in;           /* GPIOA IDR */
	uint16_t b_in;           /* GPIOB IDR */
	uint16_t a_out;          /* GPIOA ODR */
	uint16_t b_out;          /* GPIOB ODR */
	uint8_t  chg_in;         /* PA0 充电检测 (1=插入) */
	uint8_t  sw_key;         /* PA9 按键 (1=按下, 低有效) */
	uint8_t  mcu_wk;         /* PB13 MCU唤醒 */
	uint8_t  cmnt_en;        /* PB4 CAN收发器供电 */
	uint8_t  dc_en;          /* PA10 DC使能 */
	uint8_t  dbg_led;        /* PB15 调试LED */
	uint8_t  afe_ctlc;       /* PB14 AFE CTLC */
	uint8_t  afe_pro_en;     /* PB0  AFE保护使能 */
	uint8_t  m_stb;          /* PA15 主电源待机 */
	uint8_t  ad_en;          /* PB3  AD使能 */
	uint8_t  adc_bus_en;     /* PB5  ADC总线使能 */
	uint8_t  _2727_en;       /* PA3  升压使能 */
};

struct DBG_MOS {
	uint8_t  sw_chg;         /* 软件充电MOS */
	uint8_t  sw_dsg;         /* 软件放电MOS */
	uint8_t  hw_dsg_fet;     /* AFE DSG_FET 硬件 */
	uint8_t  hw_chg_fet;     /* AFE CHG_FET 硬件 */
};

struct DBG_SYS {
	uint32_t status;         /* SystemRuntime_GetStatusSnapshot() */
	uint32_t feature;        /* SystemFeature_GetMask() */
	uint16_t err_lo;         /* System_ErrFlag 低16字节 */
	uint16_t err_hi;         /* System_ErrFlag 高16字节 */
};

struct DBG_CAN {
	uint8_t  bus_active;
	uint8_t  power_on;
	uint8_t  bus_off;
	uint8_t  no_ack_cnt;
	uint8_t  tx_queue;
	uint8_t  probe;
	uint8_t  rtc_svc;
	uint16_t esr;
	uint16_t tx_ok_cnt;
	uint16_t tx_fail_cnt;
	uint16_t busoff_in_cnt;
	uint16_t busoff_out_cnt;
	uint16_t last_tx_id;     /* 最后发送的CAN ID */
};

struct DBG_LP {
	uint8_t  mode;           /* 0=NORMAL 1=HICCUP 2=DEEP 3=NO_SLP */
	uint8_t  ready;
	uint8_t  block_reason;
	uint32_t block_mask;
	uint32_t sleep_sec;
	uint32_t elapsed_sec;
	uint32_t hiccup_cycles;  /* HICCUP 唤醒轮次 */
	uint8_t  last_wake_src;  /* 上次唤醒源 */
};

struct DBG_ADC {
	uint16_t mos_temp;
	uint16_t typec_cur_ma;
	uint32_t vbat_mv;
	uint16_t raw_vbus;
	uint16_t raw_cur;
	uint16_t raw_mos;
};

struct DBG_SOC {
	/* basic */
	uint8_t  pct;
	uint8_t  soh;
	uint16_t cap_now;
	uint16_t vmax;
	uint16_t vmin;
	uint16_t ichg;
	uint16_t idsg;
	uint8_t  init_over;
	uint8_t  ocv_cali;
	uint16_t vtotal;
	/* calibration internals */
	uint8_t  mode;           /* CHG/DSG/RELAX */
	uint8_t  last_mode;
	uint32_t rest_ticks;
	uint32_t stable_ticks;
	uint16_t full_ticks;
	uint16_t empty_ticks;
	uint16_t mid_ticks;
	uint8_t  full_anchor;
	uint8_t  cal_allowed;
	uint8_t  sag_blocked;
	uint8_t  rest_stable;
	uint8_t  low_tail;
	uint8_t  mid_tail;
	uint16_t display_ticks;
	uint8_t  ocv_target;     /* OCV校准目标 SOC% */
	uint8_t  last_calib_soc; /* 上次校准前 SOC% */
};

struct DBG_AFE {
	uint8_t  bstatus1;
	uint8_t  bstatus3;
	uint8_t  fault1;
	uint16_t cur_raw;
	uint16_t pec_err;
	uint16_t cell_min_mv;
	uint16_t cell_max_mv;
};

struct DBG_FAULT {
	uint16_t first;
	uint16_t third;
	uint16_t mdl1;
	uint16_t mdl3;
};

struct DBG_AGING {
	uint8_t  state;
	uint32_t remain_sec;
};

struct DBG_FLASH {
	uint8_t  update_flag;
	uint8_t  e2prom_flag;
	uint8_t  busy;
};

struct DBG_LED {
	uint8_t  sleep;
	uint8_t  blank;
	uint8_t  number;
	uint8_t  indicators;
	uint16_t disp_10ms;
	uint8_t  frame_len;
	uint8_t  scan_idx;
	uint8_t  key_active;
	uint8_t  charge_icon;    /* 充电图标 (1=亮) */
	uint8_t  percent_icon;   /* % 图标 (1=亮) */
};

struct DBG_TIMING {
	uint32_t loop_last_us;
	uint32_t loop_max_us;
};

struct DBG_COUNTER {
	uint32_t main_cycle;
	uint32_t afe_get_cnt;
	uint32_t can_rcv_cnt;
	uint32_t rtc_sleep_cnt;
	uint32_t rtc_sec_cnt;
	uint32_t rtc_alm_cnt;
	uint32_t sci1_irq_cnt;
	uint16_t pa0_irq_cnt;
	uint16_t key_irq_cnt;
	uint32_t tick_10ms;
};

struct SYSTEM_DEBUG {
	struct DBG_GPIO    gpio;
	struct DBG_MOS     mos;
	struct DBG_SYS     sys;
	struct DBG_CAN     can;
	struct DBG_LP      lp;
	struct DBG_ADC     adc;
	struct DBG_SOC     soc;
	struct DBG_AFE     afe;
	struct DBG_FAULT   fault;
	struct DBG_AGING   aging;
	struct DBG_FLASH   flash;
	struct DBG_LED     led;
	struct DBG_TIMING  timing;
	struct DBG_COUNTER ctr;
};

/* ---- public interface ---- */

extern struct SYSTEM_DEBUG g_dbg;

void SystemDebug_Snapshot(void);
void SystemDebug_LoopEnter(uint32_t start_cyccnt);
void SystemDebug_Event(uint8_t type, uint8_t val0, uint8_t val1, uint16_t extra);

/* ---- printf helpers (UART1, _DEBUG_ only) ---- */

#if defined(_DEBUG_)
void DbgPrint_Summary(void);
void DbgPrint_All(void);
void DbgPrint_IO(void);
void DbgPrint_LP(void);
void DbgPrint_CAN(void);
void DbgPrint_SOC(void);
void DbgPrint_Wakeup(void);
void DbgPrint_EventRing(void);
#else
#define DbgPrint_Summary()    do{}while(0)
#define DbgPrint_All()        do{}while(0)
#define DbgPrint_IO()         do{}while(0)
#define DbgPrint_LP()         do{}while(0)
#define DbgPrint_CAN()        do{}while(0)
#define DbgPrint_SOC()        do{}while(0)
#define DbgPrint_Wakeup()     do{}while(0)
#define DbgPrint_EventRing()  do{}while(0)
#endif

#else

#define SystemDebug_Snapshot()          do{}while(0)
#define SystemDebug_LoopEnter(cnt)      do{}while(0)
#define SystemDebug_Event(t,v0,v1,e)    do{}while(0)
#define DbgPrint_Summary()              do{}while(0)
#define DbgPrint_All()                  do{}while(0)
#define DbgPrint_IO()                   do{}while(0)
#define DbgPrint_LP()                   do{}while(0)
#define DbgPrint_CAN()                  do{}while(0)
#define DbgPrint_SOC()                  do{}while(0)
#define DbgPrint_Wakeup()               do{}while(0)
#define DbgPrint_EventRing()            do{}while(0)

#endif /* PROJECT_CFG_DEBUG_MONITOR_ENABLE */

#endif
