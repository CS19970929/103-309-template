#ifndef SYSTEM_DEBUG_H
#define SYSTEM_DEBUG_H

#include "Project_Config.h"
#include <stdint.h>

enum DBG_PROFILE_SLOT {
	DBG_PROFILE_LOOP = 0,
	DBG_PROFILE_FRONT,
	DBG_PROFILE_IO_POWER,
	DBG_PROFILE_BACKGROUND,
	DBG_PROFILE_DEBUG_PRINT,
	DBG_PROFILE_COUNT
};

enum DBG_WDG_SOURCE {
	DBG_WDG_SRC_INIT = 1,
	DBG_WDG_SRC_FEED = 2
};

enum DBG_MODULE_ID {
	DBG_MODULE_RUNTIME = 0,
	DBG_MODULE_SYSTIME,
	DBG_MODULE_AGING,
	DBG_MODULE_LED,
	DBG_MODULE_AFE,
	DBG_MODULE_SNAPSHOT,
	DBG_MODULE_SCI,
	DBG_MODULE_ADC,
	DBG_MODULE_LOW_POWER,
	DBG_MODULE_CAN,
	DBG_MODULE_FLASH,
	DBG_MODULE_LOG,
	DBG_MODULE_PROID,
	DBG_MODULE_WATCHDOG,
	DBG_MODULE_DEBUG_PRINT,
	DBG_MODULE_PROTECT,
	DBG_MODULE_SOC,
	DBG_MODULE_COUNT
};

#define DBG_MODULE_STATE_READY ((uint8_t)0x01U)
#define DBG_MODULE_STATE_BUSY  ((uint8_t)0x02U)
#define DBG_MODULE_STATE_ERROR ((uint8_t)0x04U)

#if PROJECT_CFG_DEBUG_MONITOR_ENABLE

/* ---- debug enums for Keil Watch readable state names ---- */

enum DBG_LP_MODE {
	DBG_LP_MODE_NORMAL  = 0,
	DBG_LP_MODE_HICCUP  = 1,
	DBG_LP_MODE_DEEP    = 2,
	DBG_LP_MODE_NO_SLP  = 3
};

enum DBG_WAKE_SRC {
	DBG_WAKE_SRC_UART1   = 1,
	DBG_WAKE_SRC_UART2   = 2,
	DBG_WAKE_SRC_UART3   = 3,
	DBG_WAKE_SRC_PA0     = 4,
	DBG_WAKE_SRC_BMS_KEY = 5,
	DBG_WAKE_SRC_SOC_KEY = 6,
	DBG_WAKE_SRC_CHG_IRQ = 7,
	DBG_WAKE_SRC_CURRENT = 8,
	DBG_WAKE_SRC_RS485   = 12,
	DBG_WAKE_SRC_NO_IRQ  = 14
};

enum DBG_AGING_STATE {
	DBG_AGING_STOPPED = 0,
	DBG_AGING_RUNNING = 1,
	DBG_AGING_DONE    = 2
};

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

struct DBG_MODULE_ITEM {
	uint32_t last_tick;
	uint32_t max_gap_ticks;
	uint32_t run_cnt;
};

struct DBG_MODULE {
	uint32_t alive_mask;
	uint32_t ready_mask;
	uint32_t busy_mask;
	uint32_t error_mask;
	uint32_t stale_mask;
	uint8_t  last_id;
	uint32_t last_tick;
	struct DBG_MODULE_ITEM runtime;
	struct DBG_MODULE_ITEM systime;
	struct DBG_MODULE_ITEM aging;
	struct DBG_MODULE_ITEM led;
	struct DBG_MODULE_ITEM afe;
	struct DBG_MODULE_ITEM snapshot;
	struct DBG_MODULE_ITEM sci;
	struct DBG_MODULE_ITEM adc;
	struct DBG_MODULE_ITEM low_power;
	struct DBG_MODULE_ITEM can;
	struct DBG_MODULE_ITEM flash;
	struct DBG_MODULE_ITEM log;
	struct DBG_MODULE_ITEM proid;
	struct DBG_MODULE_ITEM watchdog;
	struct DBG_MODULE_ITEM debug_print;
	struct DBG_MODULE_ITEM protect;
	struct DBG_MODULE_ITEM soc;
};

struct DBG_RCC {
	uint32_t cr;
	uint32_t cfgr;
	uint32_t ahbenr;
	uint32_t apb1enr;
	uint32_t apb2enr;
	uint32_t bdcr;
	uint32_t csr;
	uint8_t  sysclk_src;     /* 0=HSI 1=HSE 2=PLL */
	uint8_t  hse_ready;
	uint8_t  pll_ready;
	uint8_t  lsi_ready;
};

struct DBG_IRQ {
	uint32_t iser0;
	uint32_t ispr0;
	uint32_t iabr0;
	uint32_t scb_icsr;
	uint32_t scb_shcsr;
	uint32_t systick_ctrl;
	uint32_t systick_val;
	uint32_t exti_imr;
	uint32_t exti_pr;
	uint32_t irq_tim3_10ms;
	uint32_t irq_tim4_ledbar;
	uint32_t irq_rtc_sec;
	uint32_t irq_rtc_alarm;
	uint32_t irq_exti0_chg;
	uint32_t irq_exti9_key;
	uint32_t irq_usart1;
	uint32_t irq_can1_rx0;
	uint32_t irq_unhandled;
	uint16_t last_id;
	uint16_t last_vectactive;
	uint8_t  current_phase;
	uint8_t  event_count;
};

struct DBG_PERIPH {
	uint16_t usart1_sr;
	uint16_t usart2_sr;
	uint16_t usart3_sr;
	uint16_t can_msr;
	uint32_t can_tsr;
	uint32_t can_rf0r;
	uint32_t can_esr;
	uint16_t adc1_sr;
	uint32_t dma1_isr;
	uint16_t tim3_sr;
	uint16_t tim4_sr;
	uint16_t flash_sr;
	uint16_t pwr_csr;
};

struct DBG_RESET {
	uint32_t rcc_csr;
	uint8_t  pin;
	uint8_t  por;
	uint8_t  software;
	uint8_t  iwdg;
	uint8_t  wwdg;
	uint8_t  low_power;
};

struct DBG_CAN {
	uint8_t  power_on;
	uint8_t  bus_off;
	uint8_t  tx_queue;
	uint16_t esr;
};

struct DBG_LP {
	uint8_t  mode;           /* 0=NORMAL 1=HICCUP 2=DEEP 3=NO_SLP */
	uint8_t  ready;
	uint16_t reserved;
	uint32_t block;
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
	uint16_t vtotal;
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

struct DBG_PROFILE_ITEM {
	uint32_t last_us;
	uint32_t max_us;
	uint32_t call_cnt;
};

struct DBG_PROFILE {
	struct DBG_PROFILE_ITEM loop;
	struct DBG_PROFILE_ITEM front;
	struct DBG_PROFILE_ITEM io_power;
	struct DBG_PROFILE_ITEM background;
	struct DBG_PROFILE_ITEM debug_print;
};

struct DBG_WATCHDOG {
	uint32_t feed_cnt;
	uint32_t last_feed_tick;
	uint32_t last_gap_ticks;
	uint32_t max_gap_ticks;
	uint16_t pr;
	uint16_t rlr;
	uint16_t sr;
	uint8_t  last_source;
	uint8_t  iwdg_reset;
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
	struct DBG_MODULE  module;
	struct DBG_RCC     rcc;
	struct DBG_IRQ     irq;
	struct DBG_PERIPH  periph;
	struct DBG_RESET   reset;
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
	struct DBG_PROFILE profile;
	struct DBG_WATCHDOG watchdog;
	struct DBG_COUNTER ctr;
};

/* ---- public interface ---- */

extern struct SYSTEM_DEBUG g_system_dbg;

void SystemDebug_Snapshot(void);
uint32_t SystemDebug_GetCycleCount(void);
void SystemDebug_ModuleHeartbeat(uint8_t module, uint8_t state_flags);
void SystemDebug_ProfileRecord(uint8_t slot, uint32_t start_cyccnt);
void SystemDebug_RecordWatchdogFeed(uint8_t source);
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
#define SystemDebug_GetCycleCount()     ((uint32_t)0U)
#define SystemDebug_ModuleHeartbeat(m,s) do{}while(0)
#define SystemDebug_ProfileRecord(s,c)  do{}while(0)
#define SystemDebug_RecordWatchdogFeed(s) do{}while(0)
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
