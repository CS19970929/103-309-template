#include "main.h"
#include "app_lowpower.h"
#include "AppInit.h"
#include "FactoryAging.h"
#include "Runtime.h"
#include "SystemDebug.h"

static void Runtime_RunFrontTasks(void)
{
	SysTime_LatchTaskFlags();
	FactoryAging_Task();
	APP_LedBar();
	App_AFEGet();
	SystemDebug_Snapshot();
}

static void Runtime_RunIoAndPowerTasks(void)
{
	AppInit_ServiceSci();
	App_AnlogCal();
	LP_Task();
	App_Can();
}

static void Runtime_RunBackgroundTasks(void)
{
	App_FlashUpdate();
	App_LogRecord();
	App_ProID_Deal();
	Feed_IWatchDog;
}

/* ---- debug periodic print (every ~5s) ---- */
#if defined(_DEBUG_)
static uint32_t s_dbg_print_tick;
static void Runtime_DebugPrintHook(void)
{
	uint32_t now = SysTime_Get10msTickCount();
	if ((now - s_dbg_print_tick) >= 500U) {
		s_dbg_print_tick = now;
		DbgPrint_Summary();
	}
}
#else
#define Runtime_DebugPrintHook() do{}while(0)
#endif

static void Runtime_RunNormalOnce(void)
{
	volatile uint32_t *dwt_cyccnt = (volatile uint32_t *)0xE0001004;
	uint32_t loop_start = *dwt_cyccnt;

	Runtime_RunFrontTasks();

	/* event: fault detection */
	{
		static uint16_t s_last_fault;
		uint16_t now_fault = g_stCellInfoReport.unMdlFault_Third.all & 0x3FFBU;
		if ((now_fault != 0U) && (s_last_fault == 0U)) {
			SystemDebug_Event(0x02, (uint8_t)now_fault, (uint8_t)(now_fault >> 8), 0U);
		} else if ((now_fault == 0U) && (s_last_fault != 0U)) {
			SystemDebug_Event(0x07, 0U, 0U, s_last_fault);
		}
		s_last_fault = now_fault;
	}

	/* event: LP mode change */
	{
		static uint8_t s_last_lp_mode = 3U;
		uint8_t now_lp = g_stLowPowerRtcStatus.mode;
		if (now_lp != s_last_lp_mode) {
			SystemDebug_Event(0x03, now_lp, g_stLowPowerRtcStatus.blockReason, 0U);
			s_last_lp_mode = now_lp;
		}
	}

	Runtime_RunIoAndPowerTasks();
	Runtime_RunBackgroundTasks();
	Runtime_DebugPrintHook();

	SystemDebug_LoopEnter(loop_start);
}

void Runtime_RunOnce(void)
{
	Runtime_RunNormalOnce();
}
