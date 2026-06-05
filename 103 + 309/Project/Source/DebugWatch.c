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

	g_dbg_watch.system.irq = &g_stIrqDebug;
	g_dbg_watch.system.low_power = &g_stLowPowerRtcStatus;
	g_dbg_watch.system.irq_wakeup = &g_irq_t;

	g_dbg_watch.app.series_num = &SeriesNum;
}

#endif
