#ifndef SYSTEM_DEBUG_H
#define SYSTEM_DEBUG_H

#include "Project_Config.h"

#if PROJECT_CFG_DEBUG_MONITOR_ENABLE

#include <stdint.h>

struct SYSTEM_DEBUG {
	/* --- GPIO input states (bit-packed) --- */
	uint16_t gpioA_in;
	uint16_t gpioB_in;
	uint16_t gpioA_out;
	uint16_t gpioB_out;

	/* --- Key IO singular --- */
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

	/* --- MOS state --- */
	uint8_t  mos_chg;        /* software charge MOS */
	uint8_t  mos_dsg;        /* software discharge MOS */
	uint8_t  afe_dsg_fet;    /* AFE DSG_FET hardware */
	uint8_t  afe_chg_fet;    /* AFE CHG_FET hardware */

	/* --- System state --- */
	uint32_t sys_status;     /* SystemRuntime_GetStatusSnapshot() */
	uint32_t sys_feature;    /* SystemFeature_GetMask() */
	uint16_t sys_err_lo;
	uint16_t sys_err_hi;

	/* --- CAN --- */
	uint8_t  can_bus_active;
	uint8_t  can_power_on;
	uint8_t  can_bus_off;
	uint8_t  can_no_ack_cnt;
	uint8_t  can_tx_queue;
	uint8_t  can_probe;
	uint8_t  can_rtc_svc;
	uint16_t can_esr;

	/* --- RTC / Low Power --- */
	uint8_t  lp_mode;
	uint8_t  lp_ready;
	uint8_t  lp_block_reason;
	uint32_t lp_block_mask;
	uint32_t lp_sleep_sec;
	uint32_t lp_elapsed_sec;

	/* --- ADC --- */
	uint16_t adc_mos_temp;
	uint16_t adc_typec_cur_ma;
	uint32_t adc_vbat_mv;
	uint16_t adc_raw_vbus;
	uint16_t adc_raw_cur;
	uint16_t adc_raw_mos;

	/* --- SOC --- */
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

	/* --- AFE --- */
	uint16_t afe_bstatus1;
	uint16_t afe_bstatus3;
	uint16_t afe_fault1;
	uint16_t afe_cur_raw;
	uint16_t afe_pec_err;
	uint16_t afe_cell_min_mv;
	uint16_t afe_cell_max_mv;

	/* --- Fault --- */
	uint16_t fault_first;
	uint16_t fault_third;
	uint16_t fault_mdl1;
	uint16_t fault_mdl3;

	/* --- Factory Aging --- */
	uint8_t  aging_state;
	uint32_t aging_remain_sec;

	/* --- Flash --- */
	uint8_t  flash_update_flag;
	uint8_t  flash_e2prom_flag;
	uint8_t  flash_busy;

	/* --- Runtime counters --- */
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

extern struct SYSTEM_DEBUG g_dbg;

void SystemDebug_Snapshot(void);

#else

#define SystemDebug_Snapshot() do { } while (0)

#endif /* PROJECT_CFG_DEBUG_MONITOR_ENABLE */

#endif
