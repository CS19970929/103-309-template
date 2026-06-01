#include "SystemDebug.h"

#if PROJECT_CFG_DEBUG_MONITOR_ENABLE

#include "main.h"
#include "conf.h"
#include "AppInit.h"
#include "System_Monitor.h"
#include "Can_HDX.h"
#include "rtc_sleep.h"
#include "app_lowpower.h"
#include "FactoryAging.h"
#include "Flash.h"
#include "I2C_AFE1.h"
#include "SH367309_Func.h"
#include "Fault.h"

struct SYSTEM_DEBUG g_dbg;

void SystemDebug_Snapshot(void)
{
	/* --- GPIO --- */
	g_dbg.gpioA_in  = GPIO_ReadInputData(GPIOA);
	g_dbg.gpioB_in  = GPIO_ReadInputData(GPIOB);
	g_dbg.gpioA_out = GPIO_ReadOutputData(GPIOA);
	g_dbg.gpioB_out = GPIO_ReadOutputData(GPIOB);

	g_dbg.chg_in     = (GPIO_ReadInputDataBit(GPIO_CHG_IN,  PIN_CHG_IN)  != Bit_RESET);
	g_dbg.sw_key     = (GPIO_ReadInputDataBit(GPIO_SW,      PIN_SW)      == Bit_RESET); /* active low */
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
	                     &g_dbg.can_rtc_svc, &g_dbg.can_esr);

	/* --- RTC / Low Power --- */
	g_dbg.lp_mode         = g_stLowPowerRtcStatus.mode;
	g_dbg.lp_ready        = g_stLowPowerRtcStatus.readyToSleep;
	g_dbg.lp_block_reason = g_stLowPowerRtcStatus.blockReason;
	g_dbg.lp_block_mask   = LP_GetBlockReason();
	g_dbg.lp_sleep_sec    = LP_GetLastSleepSeconds();
	g_dbg.lp_elapsed_sec  = g_stLowPowerRtcStatus.elapsedSeconds;

	/* --- ADC --- */
	{
		g_dbg.adc_mos_temp    = (uint16_t)g_i32ADCResult[ADC_TEMP_MOS1];
		g_dbg.adc_typec_cur_ma = g_u16TypeCOutCurrent_mA;
		g_dbg.adc_vbat_mv     = g_u32Vbat_mV;
		g_dbg.adc_raw_vbus    = g_u16ADCValFilter[ADC_VBC];
		g_dbg.adc_raw_cur     = g_u16ADCValFilter[ADC_CUR_AMP];
		g_dbg.adc_raw_mos     = g_u16ADCValFilter[ADC_TEMP_MOS1];
	}

	/* --- SOC --- */
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

	/* --- Runtime counters --- */
	g_dbg.main_cycle   = (uint32_t)sys_time.test_main_cycle;
	g_dbg.afe_get_cnt  = sys_time.App_AFEGet_cnt;
	g_dbg.can_rcv_cnt  = (uint32_t)sys_time.can_rcv_cnt;
	g_dbg.rtc_sleep_cnt = sys_time.rtc_sleep_cnt;
	g_dbg.rtc_sec_cnt  = sys_time.rtc_sec_cnt;
	g_dbg.rtc_alm_cnt  = sys_time.rtc_alm_cnt;
	g_dbg.sci1_irq_cnt = sys_time.sci1_irq_cnt;
	g_dbg.pa0_irq_cnt  = sys_time.cnt_PA0_irq;
	g_dbg.key_irq_cnt  = sys_time.cnt_bms1_keyirq;
	g_dbg.tick_10ms    = SysTime_Get10msTickCount();
}

#endif /* PROJECT_CFG_DEBUG_MONITOR_ENABLE */
