#include "main.h"
#include "DebugWatch.h"

#if DEBUG_WATCH_ENABLED

DEBUG_WATCH_ROOT g_dbg_watch DEBUG_WATCH_USED;

void ADC_DebugWatchBind(DEBUG_WATCH_ROOT *watch);
void DataDeal_DebugWatchBind(DEBUG_WATCH_ROOT *watch);
void Can_DebugWatchBind(DEBUG_WATCH_ROOT *watch);
void LedBar_DebugWatchBind(DEBUG_WATCH_ROOT *watch);
void SleepDeal_DebugWatchBind(DEBUG_WATCH_ROOT *watch);
void Flash_DebugWatchBind(DEBUG_WATCH_ROOT *watch);
void LogRecord_DebugWatchBind(DEBUG_WATCH_ROOT *watch);
void SystemInit_DebugWatchBind(DEBUG_WATCH_ROOT *watch);
void SystemMonitor_DebugWatchBind(DEBUG_WATCH_ROOT *watch);
void SocEnhance_DebugWatchBind(DEBUG_WATCH_ROOT *watch);

void DebugWatch_BindAll(void)
{
	ADC_DebugWatchBind(&g_dbg_watch);
	DataDeal_DebugWatchBind(&g_dbg_watch);
	Can_DebugWatchBind(&g_dbg_watch);
	LedBar_DebugWatchBind(&g_dbg_watch);
	SleepDeal_DebugWatchBind(&g_dbg_watch);
	Flash_DebugWatchBind(&g_dbg_watch);
	LogRecord_DebugWatchBind(&g_dbg_watch);
	SystemInit_DebugWatchBind(&g_dbg_watch);
	SystemMonitor_DebugWatchBind(&g_dbg_watch);
	SocEnhance_DebugWatchBind(&g_dbg_watch);

	g_dbg_watch.system_error = &System_ErrFlag;
	g_dbg_watch.low_power = &g_stLowPowerRtcStatus;
	g_dbg_watch.irq_wakeup = &g_irq_t;
	g_dbg_watch.cell_report = &g_stCellInfoReport;
	g_dbg_watch.other = &OtherElement;
	g_dbg_watch.protect = &PRT_E2ROMParas;
}

#endif
