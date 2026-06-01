#include "SystemDebug.h"

#if PROJECT_CFG_DEBUG_MONITOR_ENABLE

#include "main.h"
#include "conf.h"
#include "AppInit.h"
#include "System_Init.h"
#include "System_Monitor.h"
#include "Can_HDX.h"
#include "rtc_sleep.h"
#include "app_lowpower.h"
#include "FactoryAging.h"
#include "Flash.h"
#include "I2C_AFE1.h"
#include "SH367309_Func.h"
#include "Fault.h"
#include "LedBar.h"

/* ===== DWT CYCCNT init ===== */
/* Raw register addresses for Cortex-M3 DWT (CMSIS v1 compat) */
#define DWT_CYCCNT  (*(volatile uint32_t *)0xE0001004)
#define DWT_CONTROL (*(volatile uint32_t *)0xE0001000)
#define DEMCR       (*(volatile uint32_t *)0xE000EDFC)

static void SystemDebug_InitCycCnt(void)
{
	DEMCR |= (1U << 24);       /* TRCENA */
	DWT_CYCCNT = 0;
	DWT_CONTROL |= 1U;          /* CYCCNTENA */
}

static uint32_t SystemDebug_CycCntToUs(uint32_t cycles)
{
	uint32_t hclk_mhz = SystemCoreClock / 1000000U;
	if (hclk_mhz == 0U) hclk_mhz = 72U;
	return cycles / hclk_mhz;
}

/* ===== event ring buffer ===== */
#define DBG_EVENT_RING_SIZE 32

struct DBG_EVENT {
	uint32_t tick;
	uint8_t  type;
	uint8_t  val0;
	uint8_t  val1;
	uint16_t extra;
};

static struct DBG_EVENT s_dbg_events[DBG_EVENT_RING_SIZE];
static uint8_t s_dbg_event_head;
static uint8_t s_dbg_event_count;
static struct SYSTEM_DEBUG s_dbg_fault_snap;
static uint8_t s_dbg_fault_valid;

void SystemDebug_Event(uint8_t type, uint8_t val0, uint8_t val1, uint16_t extra)
{
	uint8_t idx = s_dbg_event_head;
	s_dbg_events[idx].tick  = SysTime_Get10msTickCount();
	s_dbg_events[idx].type  = type;
	s_dbg_events[idx].val0  = val0;
	s_dbg_events[idx].val1  = val1;
	s_dbg_events[idx].extra = extra;
	s_dbg_event_head = (idx + 1U) % DBG_EVENT_RING_SIZE;
	if (s_dbg_event_count < DBG_EVENT_RING_SIZE) {
		s_dbg_event_count++;
	}

	/* capture fault snapshot on protection events */
	if ((type == 0x01) || (type == 0x02)) {
		s_dbg_fault_snap = g_dbg;
		s_dbg_fault_valid = 1U;
	}
}

static uint16_t SystemDebug_ReadEventRing(uint8_t index, uint32_t *tick,
										   uint8_t *type, uint8_t *val0,
										   uint8_t *val1, uint16_t *extra)
{
	if (index >= s_dbg_event_count) return 0U;
	uint8_t idx;
	if (s_dbg_event_count < DBG_EVENT_RING_SIZE) {
		idx = index;
	} else {
		idx = (s_dbg_event_head + index) % DBG_EVENT_RING_SIZE;
	}
	if (tick)  *tick  = s_dbg_events[idx].tick;
	if (type)  *type  = s_dbg_events[idx].type;
	if (val0)  *val0  = s_dbg_events[idx].val0;
	if (val1)  *val1  = s_dbg_events[idx].val1;
	if (extra) *extra = s_dbg_events[idx].extra;
	return 1U;
}

/* ===== snapshot ===== */

struct SYSTEM_DEBUG g_dbg;
static uint32_t s_dbg_loop_max_cycles;

void SystemDebug_LoopEnter(uint32_t start_cyccnt)
{
	uint32_t now = DWT_CYCCNT;
	uint32_t elapsed = (now >= start_cyccnt) ? (now - start_cyccnt) : 0U;
	g_dbg.loop_last_us = SystemDebug_CycCntToUs(elapsed);
	if (elapsed > s_dbg_loop_max_cycles) {
		s_dbg_loop_max_cycles = elapsed;
	}
	g_dbg.loop_max_us = SystemDebug_CycCntToUs(s_dbg_loop_max_cycles);
}

void SystemDebug_Snapshot(void)
{
	static uint8_t s_init_done = 0U;
	if (s_init_done == 0U) {
		SystemDebug_InitCycCnt();
		s_init_done = 1U;
	}

	/* --- GPIO --- */
	g_dbg.gpioA_in  = GPIO_ReadInputData(GPIOA);
	g_dbg.gpioB_in  = GPIO_ReadInputData(GPIOB);
	g_dbg.gpioA_out = GPIO_ReadOutputData(GPIOA);
	g_dbg.gpioB_out = GPIO_ReadOutputData(GPIOB);

	g_dbg.chg_in     = (GPIO_ReadInputDataBit(GPIO_CHG_IN,  PIN_CHG_IN)  != Bit_RESET);
	g_dbg.sw_key     = (GPIO_ReadInputDataBit(GPIO_SW,      PIN_SW)      == Bit_RESET);
	g_dbg.mcu_wk     = (GPIO_ReadInputDataBit(GPIO_MCU_WK,  PIN_MCU_WK)  != Bit_RESET);
	g_dbg.cmnt_en    = (GPIO_ReadOutputDataBit(GPIO_CMNT_EN, PIN_CMNT_EN) != Bit_RESET);
	g_dbg.dc_en      = (GPIO_ReadOutputDataBit(GPIO_DC_EN,   PIN_DC_EN)   != Bit_RESET);
	g_dbg.dbg_led    = (GPIO_ReadOutputDataBit(GPIO_DBG_LED, PIN_DBG_LED) != Bit_RESET);
	g_dbg.afe_ctlc   = (GPIO_ReadOutputDataBit(GPIO_AFE1_CTL, PIN_AFE1_CTL) != Bit_RESET);
	g_dbg.afe_pro_en = (GPIO_ReadOutputDataBit(GPIO_AFE1_PRO_EN, PIN_AFE1_PRO_EN) != Bit_RESET);
	g_dbg.m_stb      = (GPIO_ReadOutputDataBit(GPIO_M_STB,  PIN_M_STB)   != Bit_RESET);
	g_dbg.ad_en      = (GPIO_ReadOutputDataBit(GPIO_AD_EN,   PIN_AD_EN)   != Bit_RESET);
	g_dbg.adc_bus_en = (GPIO_ReadOutputDataBit(GPIO_ADC_BUS_EN, PIN_ADC_BUS_EN) != Bit_RESET);
	g_dbg._2727_en   = (GPIO_ReadOutputDataBit(GPIO_2727_EN, PIN_2737_EN) != Bit_RESET);

	/* --- MOS --- */
	g_dbg.mos_chg = SystemRuntime_IsChargeMosOpen();
	g_dbg.mos_dsg = SystemRuntime_IsDischargeMosOpen();
	g_dbg.afe_dsg_fet = (uint8_t)SH367309_Reg_Store.REG_BSTATUS3.bits.DSG_FET;
	g_dbg.afe_chg_fet = (uint8_t)SH367309_Reg_Store.REG_BSTATUS3.bits.CHG_FET;

	/* --- System state --- */
	g_dbg.sys_status  = SystemRuntime_GetStatusSnapshot();
	g_dbg.sys_feature = SystemFeature_GetMask();
	{
		const volatile uint8_t *p = (const volatile uint8_t *)&System_ErrFlag;
		g_dbg.sys_err_lo = (uint16_t)(((uint16_t)p[1] << 8) | p[0]);
		g_dbg.sys_err_hi = (uint16_t)(((uint16_t)p[3] << 8) | p[2]);
	}

	/* --- CAN --- */
	Can_GetDebugSnapshot(&g_dbg.can_bus_active, &g_dbg.can_power_on,
	                     &g_dbg.can_bus_off, &g_dbg.can_no_ack_cnt,
	                     &g_dbg.can_tx_queue, &g_dbg.can_probe,
	                     &g_dbg.can_rtc_svc, &g_dbg.can_esr,
	                     &g_dbg.can_tx_ok_cnt, &g_dbg.can_tx_fail_cnt,
	                     &g_dbg.can_busoff_cnt, &g_dbg.can_busoff_rec_cnt);

	/* --- RTC / Low Power --- */
	g_dbg.lp_mode         = g_stLowPowerRtcStatus.mode;
	g_dbg.lp_ready        = g_stLowPowerRtcStatus.readyToSleep;
	g_dbg.lp_block_reason = g_stLowPowerRtcStatus.blockReason;
	g_dbg.lp_block_mask   = LP_GetBlockReason();
	g_dbg.lp_sleep_sec    = LP_GetLastSleepSeconds();
	g_dbg.lp_elapsed_sec  = g_stLowPowerRtcStatus.elapsedSeconds;

	/* --- ADC --- */
	{
		g_dbg.adc_mos_temp     = (uint16_t)g_i32ADCResult[ADC_TEMP_MOS1];
		g_dbg.adc_typec_cur_ma  = g_u16TypeCOutCurrent_mA;
		g_dbg.adc_vbat_mv      = g_u32Vbat_mV;
		g_dbg.adc_raw_vbus     = g_u16ADCValFilter[ADC_VBC];
		g_dbg.adc_raw_cur      = g_u16ADCValFilter[ADC_CUR_AMP];
		g_dbg.adc_raw_mos      = g_u16ADCValFilter[ADC_TEMP_MOS1];
	}

	/* --- SOC basic --- */
	g_dbg.soc_pct       = SOC_Enhance_Element.u8_SOC;
	g_dbg.soh_pct       = SOC_Enhance_Element.u8_SOH;
	g_dbg.soc_cap_now   = SOC_Enhance_Element.u16_CapacityNow;
	g_dbg.soc_vmax      = SOC_Enhance_Element.u16_VCellMax;
	g_dbg.soc_vmin      = SOC_Enhance_Element.u16_VCellMin;
	g_dbg.soc_ichg      = SOC_Enhance_Element.u16_Ichg;
	g_dbg.soc_idsg      = SOC_Enhance_Element.u16_Idsg;
	g_dbg.soc_init_over = (uint8_t)SOC_Enhance_Element.u16_SOC_InitOver;
	g_dbg.soc_ocv_cali  = SOC_Enhance_Element.u8_SOC_OCV_Cali;
	g_dbg.soc_vtotal    = g_stCellInfoReport.u16VCellTotle;

	/* --- SOC calibration internals --- */
	SOC_GetDebugInternals(&g_dbg.soc_mode, &g_dbg.soc_last_mode,
	                      &g_dbg.soc_rest_ticks, &g_dbg.soc_stable_ticks,
	                      &g_dbg.soc_full_ticks, &g_dbg.soc_empty_ticks,
	                      &g_dbg.soc_mid_ticks, &g_dbg.soc_full_anchor,
	                      &g_dbg.soc_cal_allowed, &g_dbg.soc_sag_blocked,
	                      &g_dbg.soc_rest_stable, &g_dbg.soc_low_tail,
	                      &g_dbg.soc_mid_tail, &g_dbg.soc_display_ticks);

	/* --- AFE --- */
	g_dbg.afe_bstatus1    = SH367309_Reg_Store.REG_BSTATUS1.all;
	g_dbg.afe_bstatus3    = SH367309_Reg_Store.REG_BSTATUS3.all;
	g_dbg.afe_fault1      = SH367309_Reg_Store.REG_BSTATUS1.all;
	g_dbg.afe_cur_raw     = SH367309_Read_AFE1.u16Current;
	g_dbg.afe_pec_err     = sys_time.pec_err_cnt;
	g_dbg.afe_cell_min_mv = g_stCellInfoReport.u16VCellMin;
	g_dbg.afe_cell_max_mv = g_stCellInfoReport.u16VCellMax;

	/* --- Fault --- */
	g_dbg.fault_first = Fault_Flag_Fisrt.all;
	g_dbg.fault_third = Fault_Flag_Third.all;
	g_dbg.fault_mdl1  = g_stCellInfoReport.unMdlFault_First.all;
	g_dbg.fault_mdl3  = g_stCellInfoReport.unMdlFault_Third.all;

	/* --- Factory Aging --- */
	g_dbg.aging_state      = FactoryAging_GetState();
	g_dbg.aging_remain_sec = FactoryAging_GetRemainingSeconds();

	/* --- Flash --- */
	g_dbg.flash_update_flag = u8FlashUpdateFlag;
	g_dbg.flash_e2prom_flag = u8FlashUpdateE2PROM;
	g_dbg.flash_busy        = StorageFlash_IsBusy();

	/* --- LED display --- */
	LedBar_GetDebugSnapshot(&g_dbg.led_sleep, &g_dbg.led_blank,
	                        &g_dbg.led_number, &g_dbg.led_indicators,
	                        &g_dbg.led_disp_10ms, &g_dbg.led_frame_len,
	                        &g_dbg.led_scan_idx, &g_dbg.led_key_active);

	/* --- Runtime counters --- */
	g_dbg.main_cycle    = (uint32_t)sys_time.test_main_cycle;
	g_dbg.afe_get_cnt   = sys_time.App_AFEGet_cnt;
	g_dbg.can_rcv_cnt   = (uint32_t)sys_time.can_rcv_cnt;
	g_dbg.rtc_sleep_cnt = sys_time.rtc_sleep_cnt;
	g_dbg.rtc_sec_cnt   = sys_time.rtc_sec_cnt;
	g_dbg.rtc_alm_cnt   = sys_time.rtc_alm_cnt;
	g_dbg.sci1_irq_cnt  = sys_time.sci1_irq_cnt;
	g_dbg.pa0_irq_cnt   = sys_time.cnt_PA0_irq;
	g_dbg.key_irq_cnt   = sys_time.cnt_bms1_keyirq;
	g_dbg.tick_10ms     = SysTime_Get10msTickCount();
}

/* ===== printf helpers ===== */

#if defined(_DEBUG_)

#include <stdio.h>

static void dbg_uart_putc(char c)
{
	while ((USART1->SR & USART_FLAG_TXE) == 0);
	USART1->DR = c;
}

static void dbg_puts(const char *s)
{
	while (*s) dbg_uart_putc(*s++);
}

static void dbg_put_hex8(uint8_t v)
{
	static const char hex[] = "0123456789ABCDEF";
	dbg_uart_putc(hex[v >> 4]);
	dbg_uart_putc(hex[v & 0x0F]);
}

static void dbg_put_hex16(uint16_t v)
{
	dbg_put_hex8((uint8_t)(v >> 8));
	dbg_put_hex8((uint8_t)v);
}

static void dbg_put_dec16(uint16_t v)
{
	char buf[6];
	uint8_t i = 0;
	if (v == 0) { dbg_uart_putc('0'); return; }
	while (v > 0) { buf[i++] = '0' + (v % 10); v /= 10; }
	while (i > 0) dbg_uart_putc(buf[--i]);
}

static const char *dbg_lp_mode_name(uint8_t m)
{
	static const char *names[] = {"NORMAL","HICCUP","DEEP","NO_SLP"};
	return (m < 4) ? names[m] : "???";
}

static const char *dbg_aging_state_name(uint8_t s)
{
	static const char *names[] = {"STOP","RUN","DONE"};
	return (s < 3) ? names[s] : "???";
}

static const char *dbg_event_type_name(uint8_t t)
{
	switch (t) {
	case 0x01: return "MOS";
	case 0x02: return "PROT";
	case 0x03: return "LP";
	case 0x04: return "SOC";
	case 0x05: return "CANBO";
	case 0x06: return "RTCWK";
	case 0x07: return "FLTRCV";
	default:   return "----";
	}
}

void DbgPrint_Summary(void)
{
	dbg_puts("\r\n[DBG] ");
	dbg_put_dec16(g_dbg.tick_10ms / 100U);
	dbg_puts("s |SOC="); dbg_put_dec16(g_dbg.soc_pct);
	dbg_puts("% V="); dbg_put_dec16(g_dbg.soc_vmin / 1000U);
	dbg_puts("."); dbg_put_hex8((uint8_t)((g_dbg.soc_vmin % 1000U) / 100U));
	dbg_puts("~"); dbg_put_dec16(g_dbg.soc_vmax / 1000U);
	dbg_puts("V I="); dbg_put_dec16(g_dbg.soc_ichg / 10U);
	dbg_puts("/"); dbg_put_dec16(g_dbg.soc_idsg / 10U);
	dbg_puts("A |MOS=");
	dbg_uart_putc(g_dbg.mos_chg ? 'C' : 'c');
	dbg_uart_putc(g_dbg.mos_dsg ? 'D' : 'd');
	dbg_puts(" |LP="); dbg_puts(dbg_lp_mode_name(g_dbg.lp_mode));
	dbg_puts(" blk="); dbg_put_hex8(g_dbg.lp_block_reason);
	dbg_puts(" |CAN=");
	dbg_uart_putc(g_dbg.can_bus_active ? 'A' : '-');
	dbg_uart_putc(g_dbg.can_bus_off ? 'B' : '-');
	dbg_puts(" |AGE="); dbg_puts(dbg_aging_state_name(g_dbg.aging_state));
	dbg_puts(" |loop="); dbg_put_dec16((uint16_t)g_dbg.loop_last_us);
	dbg_puts("us max="); dbg_put_dec16((uint16_t)g_dbg.loop_max_us);
	dbg_puts("us\r\n");
}

void DbgPrint_IO(void)
{
	dbg_puts("\r\n[DBG-IO]\r\n");
	dbg_puts("GPIOA IN=");  dbg_put_hex16(g_dbg.gpioA_in);
	dbg_puts(" OUT="); dbg_put_hex16(g_dbg.gpioA_out);
	dbg_puts("\r\nGPIOB IN="); dbg_put_hex16(g_dbg.gpioB_in);
	dbg_puts(" OUT="); dbg_put_hex16(g_dbg.gpioB_out);
	dbg_puts("\r\nCHG="); dbg_put_hex8(g_dbg.chg_in);
	dbg_puts(" SW=");   dbg_put_hex8(g_dbg.sw_key);
	dbg_puts(" MCUWK="); dbg_put_hex8(g_dbg.mcu_wk);
	dbg_puts(" CMNT="); dbg_put_hex8(g_dbg.cmnt_en);
	dbg_puts(" DC=");   dbg_put_hex8(g_dbg.dc_en);
	dbg_puts(" STB=");  dbg_put_hex8(g_dbg.m_stb);
	dbg_puts(" AD=");   dbg_put_hex8(g_dbg.ad_en);
	dbg_puts(" ADCBUS="); dbg_put_hex8(g_dbg.adc_bus_en);
	dbg_puts(" 2727="); dbg_put_hex8(g_dbg._2727_en);
	dbg_puts("\r\nMOS: CHG="); dbg_put_hex8(g_dbg.mos_chg);
	dbg_puts(" DSG="); dbg_put_hex8(g_dbg.mos_dsg);
	dbg_puts(" aFE_D="); dbg_put_hex8(g_dbg.afe_dsg_fet);
	dbg_puts(" aFE_C="); dbg_put_hex8(g_dbg.afe_chg_fet);
	dbg_puts("\r\n");
}

void DbgPrint_LP(void)
{
	dbg_puts("\r\n[DBG-LP] mode="); dbg_puts(dbg_lp_mode_name(g_dbg.lp_mode));
	dbg_puts(" ready="); dbg_put_hex8(g_dbg.lp_ready);
	dbg_puts(" block="); dbg_put_hex8(g_dbg.lp_block_reason);
	dbg_puts(" mask="); dbg_put_hex16((uint16_t)g_dbg.lp_block_mask);
	dbg_puts(" sleep_s="); dbg_put_dec16((uint16_t)g_dbg.lp_sleep_sec);
	dbg_puts(" elap_s="); dbg_put_dec16((uint16_t)g_dbg.lp_elapsed_sec);
	dbg_puts("\r\nblock bits: ");
	if (g_dbg.lp_block_mask & (1<<0))  dbg_puts("CHG ");
	if (g_dbg.lp_block_mask & (1<<1))  dbg_puts("DSG ");
	if (g_dbg.lp_block_mask & (1<<2))  dbg_puts("COMM ");
	if (g_dbg.lp_block_mask & (1<<3))  dbg_puts("KEY ");
	if (g_dbg.lp_block_mask & (1<<4))  dbg_puts("AFE ");
	if (g_dbg.lp_block_mask & (1<<5))  dbg_puts("FLASH ");
	if (g_dbg.lp_block_mask & (1<<6))  dbg_puts("UPG ");
	if (g_dbg.lp_block_mask & (1<<7))  dbg_puts("FAULT ");
	if (g_dbg.lp_block_mask & (1<<8))  dbg_puts("LED ");
	dbg_puts("\r\n");
}

void DbgPrint_CAN(void)
{
	dbg_puts("\r\n[DBG-CAN] act="); dbg_put_hex8(g_dbg.can_bus_active);
	dbg_puts(" pwr="); dbg_put_hex8(g_dbg.can_power_on);
	dbg_puts(" boff="); dbg_put_hex8(g_dbg.can_bus_off);
	dbg_puts(" noAck="); dbg_put_hex8(g_dbg.can_no_ack_cnt);
	dbg_puts(" q="); dbg_put_hex8(g_dbg.can_tx_queue);
	dbg_puts(" prb="); dbg_put_hex8(g_dbg.can_probe);
	dbg_puts(" rtc="); dbg_put_hex8(g_dbg.can_rtc_svc);
	dbg_puts(" ESR="); dbg_put_hex16(g_dbg.can_esr);
	dbg_puts("\r\nTX ok="); dbg_put_dec16(g_dbg.can_tx_ok_cnt);
	dbg_puts(" fail="); dbg_put_dec16(g_dbg.can_tx_fail_cnt);
	dbg_puts(" boff_in="); dbg_put_dec16(g_dbg.can_busoff_cnt);
	dbg_puts(" boff_out="); dbg_put_dec16(g_dbg.can_busoff_rec_cnt);
	dbg_puts("\r\n");
}

void DbgPrint_SOC(void)
{
	dbg_puts("\r\n[DBG-SOC] pct="); dbg_put_dec16(g_dbg.soc_pct);
	dbg_puts("% SOH="); dbg_put_dec16(g_dbg.soh_pct);
	dbg_puts("% cap="); dbg_put_dec16(g_dbg.soc_cap_now / 100U);
	dbg_puts("Ah\r\nVmax="); dbg_put_dec16(g_dbg.soc_vmax);
	dbg_puts("mV Vmin="); dbg_put_dec16(g_dbg.soc_vmin);
	dbg_puts("mV I="); dbg_put_dec16(g_dbg.soc_ichg / 10U);
	dbg_puts("/"); dbg_put_dec16(g_dbg.soc_idsg / 10U);
	dbg_puts("A init="); dbg_put_hex8(g_dbg.soc_init_over);
	dbg_puts(" cali="); dbg_put_hex8(g_dbg.soc_ocv_cali);
	dbg_puts("\r\nmode="); dbg_put_hex8(g_dbg.soc_mode);
	dbg_puts(" restT="); dbg_put_dec16((uint16_t)g_dbg.soc_rest_ticks);
	dbg_puts(" stableT="); dbg_put_dec16((uint16_t)g_dbg.soc_stable_ticks);
	dbg_puts(" fullT="); dbg_put_dec16(g_dbg.soc_full_ticks);
	dbg_puts("\r\nemptyT="); dbg_put_dec16(g_dbg.soc_empty_ticks);
	dbg_puts(" midT="); dbg_put_dec16(g_dbg.soc_mid_ticks);
	dbg_puts(" fullAnc="); dbg_put_hex8(g_dbg.soc_full_anchor);
	dbg_puts(" calOk="); dbg_put_hex8(g_dbg.soc_cal_allowed);
	dbg_puts("\r\nsagBlk="); dbg_put_hex8(g_dbg.soc_sag_blocked);
	dbg_puts(" stable="); dbg_put_hex8(g_dbg.soc_rest_stable);
	dbg_puts(" lowTail="); dbg_put_hex8(g_dbg.soc_low_tail);
	dbg_puts(" midTail="); dbg_put_hex8(g_dbg.soc_mid_tail);
	dbg_puts("\r\n");
}

void DbgPrint_EventRing(void)
{
	dbg_puts("\r\n[DBG-EVENT] count=");
	dbg_put_dec16(s_dbg_event_count);
	dbg_puts("\r\n");
	uint8_t i;
	for (i = 0; i < s_dbg_event_count && i < 16U; i++) {
		uint32_t tick;
		uint8_t  type, v0, v1;
		uint16_t extra;
		if (SystemDebug_ReadEventRing(i, &tick, &type, &v0, &v1, &extra)) {
			dbg_put_dec16((uint16_t)(i & 0xFF));
			dbg_puts(": t="); dbg_put_dec16((uint16_t)(tick / 100U));
			dbg_puts("s "); dbg_puts(dbg_event_type_name(type));
			dbg_puts(" v0="); dbg_put_hex8(v0);
			dbg_puts(" v1="); dbg_put_hex8(v1);
			dbg_puts(" ex="); dbg_put_hex16(extra);
			dbg_puts("\r\n");
		}
	}
}

#endif /* _DEBUG_ */

#endif /* PROJECT_CFG_DEBUG_MONITOR_ENABLE */
