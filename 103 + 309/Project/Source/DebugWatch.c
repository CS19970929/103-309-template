#include "main.h"
#include "DebugWatch.h"
#include "IrqDebug.h"
#include "SystemDebug.h"

#if DEBUG_WATCH_ENABLED

DEBUG_WATCH_ROOT g_dbg_watch DEBUG_WATCH_USED;

void ADC_DebugWatchBind(DEBUG_WATCH_ROOT *watch);
void DataDeal_DebugWatchBind(DEBUG_WATCH_ROOT *watch);
void Can_DebugWatchBind(DEBUG_WATCH_ROOT *watch);
void LedBar_DebugWatchBind(DEBUG_WATCH_ROOT *watch);
void SleepDeal_DebugWatchBind(DEBUG_WATCH_ROOT *watch);
void Flash_DebugWatchBind(DEBUG_WATCH_ROOT *watch);
void LogRecord_DebugWatchBind(DEBUG_WATCH_ROOT *watch);
void FactoryAging_DebugWatchBind(DEBUG_WATCH_ROOT *watch);
void RTC_DebugWatchBind(DEBUG_WATCH_ROOT *watch);
void Runtime_DebugWatchBind(DEBUG_WATCH_ROOT *watch);
void Sci_DebugWatchBind(DEBUG_WATCH_ROOT *watch);
void I2C_AFE1_DebugWatchBind(DEBUG_WATCH_ROOT *watch);
void SH367309Data_DebugWatchBind(DEBUG_WATCH_ROOT *watch);
void SH367309Func_DebugWatchBind(DEBUG_WATCH_ROOT *watch);
void Fault_DebugWatchBind(DEBUG_WATCH_ROOT *watch);
void ProductionID_DebugWatchBind(DEBUG_WATCH_ROOT *watch);
void CanFeidaoFrames_DebugWatchBind(DEBUG_WATCH_ROOT *watch);
void SystemInit_DebugWatchBind(DEBUG_WATCH_ROOT *watch);
void SystemMonitor_DebugWatchBind(DEBUG_WATCH_ROOT *watch);
void SocEnhance_DebugWatchBind(DEBUG_WATCH_ROOT *watch);
#if DEBUG_WATCH_SYSTEM_DEBUG_ENABLED
void SystemDebug_DebugWatchBind(DEBUG_WATCH_ROOT *watch);
#endif

void DebugWatch_BindAll(void)
{
	ADC_DebugWatchBind(&g_dbg_watch);
	DataDeal_DebugWatchBind(&g_dbg_watch);
	Can_DebugWatchBind(&g_dbg_watch);
	LedBar_DebugWatchBind(&g_dbg_watch);
	SleepDeal_DebugWatchBind(&g_dbg_watch);
	Flash_DebugWatchBind(&g_dbg_watch);
	LogRecord_DebugWatchBind(&g_dbg_watch);
	FactoryAging_DebugWatchBind(&g_dbg_watch);
	RTC_DebugWatchBind(&g_dbg_watch);
	Runtime_DebugWatchBind(&g_dbg_watch);
	Sci_DebugWatchBind(&g_dbg_watch);
	I2C_AFE1_DebugWatchBind(&g_dbg_watch);
	SH367309Data_DebugWatchBind(&g_dbg_watch);
	SH367309Func_DebugWatchBind(&g_dbg_watch);
	Fault_DebugWatchBind(&g_dbg_watch);
	ProductionID_DebugWatchBind(&g_dbg_watch);
	CanFeidaoFrames_DebugWatchBind(&g_dbg_watch);
	SystemInit_DebugWatchBind(&g_dbg_watch);
	SystemMonitor_DebugWatchBind(&g_dbg_watch);
	SocEnhance_DebugWatchBind(&g_dbg_watch);
#if DEBUG_WATCH_SYSTEM_DEBUG_ENABLED
	SystemDebug_DebugWatchBind(&g_dbg_watch);
#endif

	g_dbg_watch.system_error = &System_ErrFlag;
	g_dbg_watch.low_power = &g_stLowPowerRtcStatus;
	g_dbg_watch.irq_wakeup = &g_irq_t;
	g_dbg_watch.cell_report = &g_stCellInfoReport;
	g_dbg_watch.other = &OtherElement;
	g_dbg_watch.protect = &PRT_E2ROMParas;

	g_dbg_watch.runtime.adc = g_dbg_watch.adc;
	g_dbg_watch.runtime.data = g_dbg_watch.data;
	g_dbg_watch.runtime.can_tx = g_dbg_watch.can_tx;
	g_dbg_watch.runtime.can_runtime = g_dbg_watch.can_runtime;
	g_dbg_watch.runtime.can_app = g_dbg_watch.can_app;
	g_dbg_watch.runtime.ledbar = g_dbg_watch.ledbar;
	g_dbg_watch.runtime.sleep = g_dbg_watch.sleep;
	g_dbg_watch.runtime.flash = g_dbg_watch.flash;
	g_dbg_watch.runtime.log_record = g_dbg_watch.log_record;
	g_dbg_watch.runtime.soc = g_dbg_watch.soc;
	g_dbg_watch.runtime.soc_public = g_dbg_watch.soc_public;

	g_dbg_watch.system.irq = &g_stIrqDebug;
	g_dbg_watch.system.time_latched = g_dbg_watch.sys_time_latched;
	g_dbg_watch.system.time_pending = g_dbg_watch.sys_time_pending;
	g_dbg_watch.system.tick_10ms = g_dbg_watch.sys_10ms_tick_count;
	g_dbg_watch.system.cnt50ms = g_dbg_watch.sys_cnt50ms;
	g_dbg_watch.system.cnt100ms = g_dbg_watch.sys_cnt100ms;
	g_dbg_watch.system.cnt200ms = g_dbg_watch.sys_cnt200ms;
	g_dbg_watch.system.cnt1000ms = g_dbg_watch.sys_cnt1000ms;
	g_dbg_watch.system.pending_200ms = g_dbg_watch.sys_200ms_pending_periods;
	g_dbg_watch.system.overflow_200ms = g_dbg_watch.sys_200ms_overflow_count;
	g_dbg_watch.system.feature = g_dbg_watch.system_feature;
	g_dbg_watch.system.status = g_dbg_watch.system_status;
	g_dbg_watch.system.error = g_dbg_watch.system_error;
	g_dbg_watch.system.low_power = g_dbg_watch.low_power;
	g_dbg_watch.system.irq_wakeup = g_dbg_watch.irq_wakeup;

	g_dbg_watch.public_data.cell_report = g_dbg_watch.cell_report;
	g_dbg_watch.public_data.other = g_dbg_watch.other;
	g_dbg_watch.public_data.protect = g_dbg_watch.protect;
	g_dbg_watch.public_data.soc = g_dbg_watch.soc_public;

	g_dbg_watch.app.series_num = &SeriesNum;
}

#endif
