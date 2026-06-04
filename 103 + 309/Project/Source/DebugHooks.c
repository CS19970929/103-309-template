#include "main.h"
#include "DebugHooks.h"

#include "DebugWatch.h"

#if (defined(PROJECT_CFG_DEBUG_MONITOR_ENABLE) && (PROJECT_CFG_DEBUG_MONITOR_ENABLE != 0)) || \
	(defined(PROJECT_CFG_DEBUG_WATCH_ENABLE) && (PROJECT_CFG_DEBUG_WATCH_ENABLE != 0))

#if defined(PROJECT_CFG_DEBUG_MONITOR_ENABLE) && (PROJECT_CFG_DEBUG_MONITOR_ENABLE != 0)
#include "FactoryAging.h"
#include "SystemDebug.h"
#endif

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

#if defined(PROJECT_CFG_DEBUG_MONITOR_ENABLE) && (PROJECT_CFG_DEBUG_MONITOR_ENABLE != 0)

void DebugHooks_RuntimeAfterSysTime(void)
{
	SystemDebug_ModuleHeartbeat((uint8_t)DBG_MODULE_SYSTIME, DBG_MODULE_STATE_READY);
}

void DebugHooks_RuntimeAfterAging(void)
{
	SystemDebug_ModuleHeartbeat((uint8_t)DBG_MODULE_AGING,
		(uint8_t)(DBG_MODULE_STATE_READY |
		((FactoryAging_GetState() == FACTORY_AGING_PUBLIC_STATE_RUNNING) ?
			DBG_MODULE_STATE_BUSY : 0U)));
}

void DebugHooks_RuntimeAfterLed(void)
{
	SystemDebug_ModuleHeartbeat((uint8_t)DBG_MODULE_LED, DBG_MODULE_STATE_READY);
}

void DebugHooks_RuntimeAfterAfe(void)
{
	SystemDebug_ModuleHeartbeat((uint8_t)DBG_MODULE_AFE, DBG_MODULE_STATE_READY);
	SystemDebug_ModuleHeartbeat((uint8_t)DBG_MODULE_SOC, DBG_MODULE_STATE_READY);
}

void DebugHooks_RuntimeSnapshot(void)
{
	SystemDebug_Snapshot();
}

void DebugHooks_RuntimeAfterSci(void)
{
	SystemDebug_ModuleHeartbeat((uint8_t)DBG_MODULE_SCI, DBG_MODULE_STATE_READY);
}

void DebugHooks_RuntimeAfterAdc(void)
{
	SystemDebug_ModuleHeartbeat((uint8_t)DBG_MODULE_ADC, DBG_MODULE_STATE_READY);
}

void DebugHooks_RuntimeAfterLowPower(void)
{
	SystemDebug_ModuleHeartbeat((uint8_t)DBG_MODULE_LOW_POWER,
		(uint8_t)(DBG_MODULE_STATE_READY |
		((g_stLowPowerRtcStatus.mode != (uint8_t)NO_SLEEP) ? DBG_MODULE_STATE_BUSY : 0U)));
}

void DebugHooks_RuntimeAfterCan(void)
{
	SystemDebug_ModuleHeartbeat((uint8_t)DBG_MODULE_CAN,
		(uint8_t)(DBG_MODULE_STATE_READY |
		((Can_PeekBusy() != 0U) ? DBG_MODULE_STATE_BUSY : 0U)));
}

void DebugHooks_RuntimeAfterFlash(void)
{
	SystemDebug_ModuleHeartbeat((uint8_t)DBG_MODULE_FLASH,
		(uint8_t)(DBG_MODULE_STATE_READY |
		((StorageFlash_IsBusy() != 0U) ? DBG_MODULE_STATE_BUSY : 0U)));
}

void DebugHooks_RuntimeAfterLog(void)
{
	SystemDebug_ModuleHeartbeat((uint8_t)DBG_MODULE_LOG, DBG_MODULE_STATE_READY);
}

void DebugHooks_RuntimeAfterProId(void)
{
	SystemDebug_ModuleHeartbeat((uint8_t)DBG_MODULE_PROID, DBG_MODULE_STATE_READY);
}

void DebugHooks_RuntimeDebugPrint(void)
{
#if defined(_DEBUG_)
	uint32_t now = SysTime_Get10msTickCount();
	if ((now - s_rt.dbg_tick) >= 500U)
	{
		s_rt.dbg_tick = now;
		DbgPrint_Summary();
	}
#endif
	SystemDebug_ModuleHeartbeat((uint8_t)DBG_MODULE_DEBUG_PRINT, DBG_MODULE_STATE_READY);
}

uint32_t DebugHooks_RuntimeLoopStart(void)
{
	return SystemDebug_GetCycleCount();
}

uint32_t DebugHooks_RuntimeSectionStart(void)
{
	return SystemDebug_GetCycleCount();
}

static void DebugHooks_RuntimeRecordEvents(void)
{
	uint16_t now_fault = g_stCellInfoReport.unMdlFault_Third.all & 0x3FFBU;
	uint8_t now_lp = g_stLowPowerRtcStatus.mode;

	if ((now_fault != 0U) && (s_rt.fault == 0U))
	{
		SystemDebug_Event(0x02, (uint8_t)now_fault, (uint8_t)(now_fault >> 8), 0U);
	}
	else if ((now_fault == 0U) && (s_rt.fault != 0U))
	{
		SystemDebug_Event(0x07, 0U, 0U, s_rt.fault);
	}
	s_rt.fault = now_fault;

	if (now_lp != s_rt.lp)
	{
		SystemDebug_Event(0x03, now_lp, (uint8_t)g_stLowPowerRtcStatus.block, 0U);
		s_rt.lp = now_lp;
	}
}

void DebugHooks_RuntimeAfterFrontSection(uint32_t section_start)
{
	SystemDebug_ProfileRecord((uint8_t)DBG_PROFILE_FRONT, section_start);
	DebugHooks_RuntimeRecordEvents();
}

void DebugHooks_RuntimeAfterIoPowerSection(uint32_t section_start)
{
	SystemDebug_ProfileRecord((uint8_t)DBG_PROFILE_IO_POWER, section_start);
}

void DebugHooks_RuntimeAfterBackgroundSection(uint32_t section_start)
{
	SystemDebug_ProfileRecord((uint8_t)DBG_PROFILE_BACKGROUND, section_start);
}

void DebugHooks_RuntimeAfterDebugPrintSection(uint32_t section_start)
{
	SystemDebug_ProfileRecord((uint8_t)DBG_PROFILE_DEBUG_PRINT, section_start);
}

void DebugHooks_RuntimeLoopDone(uint32_t loop_start)
{
	SystemDebug_LoopEnter(loop_start);
	SystemDebug_ModuleHeartbeat((uint8_t)DBG_MODULE_RUNTIME, DBG_MODULE_STATE_READY);
}

#endif /* PROJECT_CFG_DEBUG_MONITOR_ENABLE */

#endif /* PROJECT_CFG_DEBUG_MONITOR_ENABLE || PROJECT_CFG_DEBUG_WATCH_ENABLE */
