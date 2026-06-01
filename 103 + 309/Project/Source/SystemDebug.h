#ifndef SYSTEM_DEBUG_H
#define SYSTEM_DEBUG_H

#include "Project_Config.h"
#include "stm32f10x.h"

#if PROJECT_CFG_DEBUG_MONITOR_ENABLE

#include <stdint.h>

struct SYSTEM_DEBUG {
	/* ========== GPIO (16 fields) ========== */
	uint16_t gpioA_in;
	uint16_t gpioB_in;
	uint16_t gpioA_out;
	uint16_t gpioB_out;

	uint8_t  chg_in;         /* PA0  */
	uint8_t  sw_key;         /* PA9  */
	uint8_t  mcu_wk;         /* PB13 */
	uint8_t  cmnt_en;        /* PB4  */
	uint8_t  dc_en;          /* PA10 */
	uint8_t  dbg_led;        /* PB15 */
	uint8_t  afe_ctlc;       /* PB14 */
	uint8_t  afe_pro_en;     /* PB0  */
	uint8_t  m_stb;          /* PA15 */
	uint8_t  ad_en;          /* PB3  */
	uint8_t  adc_bus_en;     /* PB5  */
	uint8_t  _2727_en;       /* PA3  */

	/* ========== MOS (4 fields) ========== */
	uint8_t  mos_chg;
	uint8_t  mos_dsg;
	uint8_t  afe_dsg_fet;
	uint8_t  afe_chg_fet;

	/* ========== System (4 fields) ========== */
	uint32_t sys_status;
	uint32_t sys_feature;
	uint16_t sys_err_lo;
	uint16_t sys_err_hi;

	/* ========== CAN (12 fields) ========== */
	uint8_t  can_bus_active;
	uint8_t  can_power_on;
	uint8_t  can_bus_off;
	uint8_t  can_no_ack_cnt;
	uint8_t  can_tx_queue;
	uint8_t  can_probe;
	uint8_t  can_rtc_svc;
	uint16_t can_esr;
	uint16_t can_tx_ok_cnt;
	uint16_t can_tx_fail_cnt;
	uint16_t can_busoff_cnt;
	uint16_t can_busoff_rec_cnt;

	/* ========== RTC / Low Power (6 fields) ========== */
	uint8_t  lp_mode;
	uint8_t  lp_ready;
	uint8_t  lp_block_reason;
	uint32_t lp_block_mask;
	uint32_t lp_sleep_sec;
	uint32_t lp_elapsed_sec;

	/* ========== ADC (6 fields) ========== */
	uint16_t adc_mos_temp;
	uint16_t adc_typec_cur_ma;
	uint32_t adc_vbat_mv;
	uint16_t adc_raw_vbus;
	uint16_t adc_raw_cur;
	uint16_t adc_raw_mos;

	/* ========== SOC basic (10 fields) ========== */
	uint8_t  soc_pct;
	uint8_t  soh_pct;
	uint16_t soc_cap_now;
	uint16_t soc_vmax;
	uint16_t soc_vmin;
	uint16_t soc_ichg;
	uint16_t soc_idsg;
	uint8_t  soc_init_over;
	uint8_t  soc_ocv_cali;
	uint16_t soc_vtotal;

	/* ========== SOC calibration internals (14 fields) ========== */
	uint8_t  soc_mode;          /* SOC_MODE_CHG/DSG/RELAX */
	uint8_t  soc_last_mode;
	uint32_t soc_rest_ticks;    /* rest counter */
	uint32_t soc_stable_ticks;  /* stable rest counter */
	uint16_t soc_full_ticks;    /* full confirm counter */
	uint16_t soc_empty_ticks;   /* empty tail counter */
	uint16_t soc_mid_ticks;     /* mid tail counter */
	uint8_t  soc_full_anchor;   /* full-anchored flag */
	uint8_t  soc_cal_allowed;   /* calibration allowed */
	uint8_t  soc_sag_blocked;   /* sag hold blocking */
	uint8_t  soc_rest_stable;   /* voltage stable flag */
	uint8_t  soc_low_tail;      /* low tail active */
	uint8_t  soc_mid_tail;      /* mid tail active */
	uint16_t soc_display_ticks; /* display smooth counter */

	/* ========== AFE (8 fields) ========== */
	uint8_t  afe_bstatus1;
	uint8_t  afe_bstatus3;
	uint8_t  afe_fault1;
	uint16_t afe_cur_raw;
	uint16_t afe_pec_err;
	uint16_t afe_cell_min_mv;
	uint16_t afe_cell_max_mv;

	/* ========== Fault (4 fields) ========== */
	uint16_t fault_first;
	uint16_t fault_third;
	uint16_t fault_mdl1;
	uint16_t fault_mdl3;

	/* ========== Factory Aging (2 fields) ========== */
	uint8_t  aging_state;
	uint32_t aging_remain_sec;

	/* ========== Flash (3 fields) ========== */
	uint8_t  flash_update_flag;
	uint8_t  flash_e2prom_flag;
	uint8_t  flash_busy;

	/* ========== LED display (8 fields) ========== */
	uint8_t  led_sleep;
	uint8_t  led_blank;
	uint8_t  led_number;
	uint8_t  led_indicators;
	uint16_t led_disp_10ms;
	uint8_t  led_frame_len;
	uint8_t  led_scan_idx;
	uint8_t  led_key_active;

	/* ========== Loop timing (2 fields) ========== */
	uint32_t loop_last_us;
	uint32_t loop_max_us;

	/* ========== Runtime counters (10 fields) ========== */
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

/* ---- public interface ---- */

extern struct SYSTEM_DEBUG g_dbg;

void SystemDebug_Snapshot(void);
void SystemDebug_LoopEnter(uint32_t start_cyccnt);
void SystemDebug_Event(uint8_t type, uint8_t val0, uint8_t val1, uint16_t extra);

/* ---- printf helpers (UART1, _DEBUG_ only) ---- */

#if defined(_DEBUG_)
void DbgPrint_Summary(void);   /* one-line: SOC Vmax Vmin Imax MOS LP */
void DbgPrint_IO(void);        /* all GPIO states */
void DbgPrint_LP(void);        /* low-power block details */
void DbgPrint_CAN(void);       /* CAN state */
void DbgPrint_SOC(void);       /* SOC calibration state */
void DbgPrint_EventRing(void); /* last 32 events */
#else
#define DbgPrint_Summary()    do{}while(0)
#define DbgPrint_IO()         do{}while(0)
#define DbgPrint_LP()         do{}while(0)
#define DbgPrint_CAN()        do{}while(0)
#define DbgPrint_SOC()        do{}while(0)
#define DbgPrint_EventRing()  do{}while(0)
#endif

#else

#define SystemDebug_Snapshot()          do{}while(0)
#define SystemDebug_LoopEnter(cnt)      do{}while(0)
#define SystemDebug_Event(t,v0,v1,e)    do{}while(0)
#define DbgPrint_Summary()              do{}while(0)
#define DbgPrint_IO()                   do{}while(0)
#define DbgPrint_LP()                   do{}while(0)
#define DbgPrint_CAN()                  do{}while(0)
#define DbgPrint_SOC()                  do{}while(0)
#define DbgPrint_EventRing()            do{}while(0)

#endif /* PROJECT_CFG_DEBUG_MONITOR_ENABLE */

#endif
