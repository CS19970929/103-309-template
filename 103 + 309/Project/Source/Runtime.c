#include "main.h"
#include "AppInit.h"
#include "FactoryAging.h"
#include "Runtime.h"
#include "SystemDebug.h"
#include "DebugWatch.h"

typedef struct APP_RUNTIME_TAG
{
	uint32_t dbg_tick;
	uint16_t fault;
	uint8_t lp;
	uint8_t reserved;
} APP_RUNTIME;

static APP_RUNTIME s_rt = {
	0U,
	0U,
	3U,
	0U
};

#if DEBUG_WATCH_ENABLED
void Runtime_DebugWatchBind(DEBUG_WATCH_ROOT *watch)
{
	watch->runtime.app = &s_rt;
}
#endif

static void Runtime_RunFrontTasks(void)
{
	SysTime_LatchTaskFlags();
	SystemDebug_ModuleHeartbeat((uint8_t)DBG_MODULE_SYSTIME, DBG_MODULE_STATE_READY);

	FactoryAging_Task();
	SystemDebug_ModuleHeartbeat((uint8_t)DBG_MODULE_AGING,
		(uint8_t)(DBG_MODULE_STATE_READY |
		((FactoryAging_GetState() == FACTORY_AGING_PUBLIC_STATE_RUNNING) ?
			DBG_MODULE_STATE_BUSY : 0U)));

	APP_LedBar();
	SystemDebug_ModuleHeartbeat((uint8_t)DBG_MODULE_LED, DBG_MODULE_STATE_READY);

	App_AFEGet();
	SystemDebug_ModuleHeartbeat((uint8_t)DBG_MODULE_AFE, DBG_MODULE_STATE_READY);
	SystemDebug_ModuleHeartbeat((uint8_t)DBG_MODULE_SOC, DBG_MODULE_STATE_READY);

	SystemDebug_Snapshot();
}

static void Runtime_RunIoAndPowerTasks(void)
{
	AppInit_ServiceSci();
	SystemDebug_ModuleHeartbeat((uint8_t)DBG_MODULE_SCI, DBG_MODULE_STATE_READY);

	App_AnlogCal();
	SystemDebug_ModuleHeartbeat((uint8_t)DBG_MODULE_ADC, DBG_MODULE_STATE_READY);

	rtc_sleep();
	SystemDebug_ModuleHeartbeat((uint8_t)DBG_MODULE_LOW_POWER,
		(uint8_t)(DBG_MODULE_STATE_READY |
		((g_stLowPowerRtcStatus.mode != (uint8_t)NO_SLEEP) ? DBG_MODULE_STATE_BUSY : 0U)));

	App_Can();
	SystemDebug_ModuleHeartbeat((uint8_t)DBG_MODULE_CAN,
		(uint8_t)(DBG_MODULE_STATE_READY |
		((Can_PeekBusy() != 0U) ? DBG_MODULE_STATE_BUSY : 0U)));
}

static void Runtime_RunBackgroundTasks(void)
{
	App_FlashUpdate();
	SystemDebug_ModuleHeartbeat((uint8_t)DBG_MODULE_FLASH,
		(uint8_t)(DBG_MODULE_STATE_READY |
		((StorageFlash_IsBusy() != 0U) ? DBG_MODULE_STATE_BUSY : 0U)));

	App_LogRecord();
	SystemDebug_ModuleHeartbeat((uint8_t)DBG_MODULE_LOG, DBG_MODULE_STATE_READY);

	App_ProID_Deal();
	SystemDebug_ModuleHeartbeat((uint8_t)DBG_MODULE_PROID, DBG_MODULE_STATE_READY);

	Feed_IWatchDog;
}

/* ---- debug periodic print (every ~5s) ---- */
#if defined(_DEBUG_)
static void Runtime_DebugPrintHook(void)
{
	uint32_t now = SysTime_Get10msTickCount();
	if ((now - s_rt.dbg_tick) >= 500U) {
		s_rt.dbg_tick = now;
		DbgPrint_Summary();
	}
	SystemDebug_ModuleHeartbeat((uint8_t)DBG_MODULE_DEBUG_PRINT, DBG_MODULE_STATE_READY);
}
#else
#define Runtime_DebugPrintHook() do{}while(0)
#endif

static void Runtime_RunNormalOnce(void)
{
	uint32_t loop_start = SystemDebug_GetCycleCount();
	uint32_t section_start;

	section_start = SystemDebug_GetCycleCount();
	Runtime_RunFrontTasks();
	SystemDebug_ProfileRecord((uint8_t)DBG_PROFILE_FRONT, section_start);

	/* event: fault detection */
	{
		uint16_t now_fault = g_stCellInfoReport.unMdlFault_Third.all & 0x3FFBU;
		if ((now_fault != 0U) && (s_rt.fault == 0U)) {
			SystemDebug_Event(0x02, (uint8_t)now_fault, (uint8_t)(now_fault >> 8), 0U);
		} else if ((now_fault == 0U) && (s_rt.fault != 0U)) {
			SystemDebug_Event(0x07, 0U, 0U, s_rt.fault);
		}
		s_rt.fault = now_fault;
	}

	/* event: LP mode change */
	{
		uint8_t now_lp = g_stLowPowerRtcStatus.mode;
		if (now_lp != s_rt.lp) {
			SystemDebug_Event(0x03, now_lp, (uint8_t)g_stLowPowerRtcStatus.block, 0U);
			s_rt.lp = now_lp;
		}
	}

	section_start = SystemDebug_GetCycleCount();
	Runtime_RunIoAndPowerTasks();
	SystemDebug_ProfileRecord((uint8_t)DBG_PROFILE_IO_POWER, section_start);

	section_start = SystemDebug_GetCycleCount();
	Runtime_RunBackgroundTasks();
	SystemDebug_ProfileRecord((uint8_t)DBG_PROFILE_BACKGROUND, section_start);

	section_start = SystemDebug_GetCycleCount();
	Runtime_DebugPrintHook();
	SystemDebug_ProfileRecord((uint8_t)DBG_PROFILE_DEBUG_PRINT, section_start);

	SystemDebug_LoopEnter(loop_start);
	SystemDebug_ModuleHeartbeat((uint8_t)DBG_MODULE_RUNTIME, DBG_MODULE_STATE_READY);
}

void Runtime_RunOnce(void)
{
	Runtime_RunNormalOnce();
}
