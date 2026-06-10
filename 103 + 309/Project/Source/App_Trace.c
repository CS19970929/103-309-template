#include "main.h"
#include "App_Trace.h"

#if APP_TRACE_ENABLE
volatile APP_TRACE_TASK g_stAppTraceTask[APP_TRACE_TASK_NUM];
volatile APP_TRACE_WARN_CHECK g_stAppTraceWarnCheck[APP_WARN_CHECK_NUM];
volatile UINT32 gu32_AppTraceLoopCnt = 0;
volatile UINT32 gu32_AppTrace1msTick = 0;
volatile UINT32 gu32_AppTrace10msPhaseTick = 0;
volatile UINT32 gu32_AppTrace10msFlag1Tick = 0;
volatile UINT8 gu8_AppTraceCurrentTask = APP_TRACE_TASK_NONE;
volatile UINT8 gu8_AppTraceCurrentWarnCheck = APP_WARN_CHECK_NONE;
volatile UINT8 gu8_AppTraceLast10msPhase = 0;

static UINT16 AppTrace_DeltaToU16(UINT32 u32Now, UINT32 u32Last)
{
	UINT32 u32Delta = u32Now - u32Last;

	if (u32Delta > 0xFFFF)
	{
		return 0xFFFF;
	}
	return (UINT16)u32Delta;
}

void AppTrace_LoopBegin(void)
{
	gu32_AppTraceLoopCnt++;
}

void AppTrace_TaskBegin(UINT8 u8TaskId)
{
	volatile APP_TRACE_TASK *pstTrace;
	UINT16 u16Delta;

	if (u8TaskId >= APP_TRACE_TASK_NUM)
	{
		return;
	}

	pstTrace = &g_stAppTraceTask[u8TaskId];
	pstTrace->active = 1;
	pstTrace->runCnt++;

	if (pstTrace->lastLoopCnt != 0)
	{
		u16Delta = AppTrace_DeltaToU16(gu32_AppTraceLoopCnt, pstTrace->lastLoopCnt);
		pstTrace->lastLoopInterval = u16Delta;
		if (u16Delta > pstTrace->maxLoopInterval)
		{
			pstTrace->maxLoopInterval = u16Delta;
		}
	}

	if (pstTrace->last10msFlag1Tick != 0)
	{
		u16Delta = AppTrace_DeltaToU16(gu32_AppTrace10msFlag1Tick, pstTrace->last10msFlag1Tick);
		pstTrace->last10msFlag1Interval = u16Delta;
		if (u16Delta > pstTrace->max10msFlag1Interval)
		{
			pstTrace->max10msFlag1Interval = u16Delta;
		}
	}

	pstTrace->lastLoopCnt = gu32_AppTraceLoopCnt;
	pstTrace->last1msTick = gu32_AppTrace1msTick;
	pstTrace->last10msPhaseTick = gu32_AppTrace10msPhaseTick;
	pstTrace->last10msFlag1Tick = gu32_AppTrace10msFlag1Tick;
	gu8_AppTraceCurrentTask = u8TaskId;
}

void AppTrace_TaskEnd(UINT8 u8TaskId)
{
	if (u8TaskId == APP_TRACE_TASK_SYS_TIME)
	{
		if (g_st_SysTimeFlag.bits.b1Sys1msFlag)
		{
			gu32_AppTrace1msTick++;
		}
		if (g_st_SysTimeFlag.bits.b1Sys10msFlag1)
		{
			gu8_AppTraceLast10msPhase = 0;
			gu32_AppTrace10msPhaseTick++;
			gu32_AppTrace10msFlag1Tick++;
		}
		else if (g_st_SysTimeFlag.bits.b1Sys10msFlag2)
		{
			gu8_AppTraceLast10msPhase = 1;
			gu32_AppTrace10msPhaseTick++;
		}
		else if (g_st_SysTimeFlag.bits.b1Sys10msFlag3)
		{
			gu8_AppTraceLast10msPhase = 2;
			gu32_AppTrace10msPhaseTick++;
		}
		else if (g_st_SysTimeFlag.bits.b1Sys10msFlag4)
		{
			gu8_AppTraceLast10msPhase = 3;
			gu32_AppTrace10msPhaseTick++;
		}
		else if (g_st_SysTimeFlag.bits.b1Sys10msFlag5)
		{
			gu8_AppTraceLast10msPhase = 4;
			gu32_AppTrace10msPhaseTick++;
		}
	}

	if (u8TaskId < APP_TRACE_TASK_NUM)
	{
		g_stAppTraceTask[u8TaskId].active = 0;
	}
	if (gu8_AppTraceCurrentTask == u8TaskId)
	{
		gu8_AppTraceCurrentTask = APP_TRACE_TASK_NONE;
	}
}

void AppTrace_WarnCheckBegin(UINT8 u8CheckId)
{
	volatile APP_TRACE_WARN_CHECK *pstTrace;
	UINT32 u32WarnCtrlCnt;
	UINT16 u16Delta;

	if (u8CheckId >= APP_WARN_CHECK_NUM)
	{
		return;
	}

	u32WarnCtrlCnt = g_stAppTraceTask[APP_TRACE_TASK_WARN_CTRL].runCnt;
	pstTrace = &g_stAppTraceWarnCheck[u8CheckId];
	pstTrace->active = 1;
	pstTrace->runCnt++;

	if (pstTrace->lastWarnCtrlCnt != 0)
	{
		u16Delta = AppTrace_DeltaToU16(u32WarnCtrlCnt, pstTrace->lastWarnCtrlCnt);
		pstTrace->lastWarnCtrlInterval = u16Delta;
		if (u16Delta > pstTrace->maxWarnCtrlInterval)
		{
			pstTrace->maxWarnCtrlInterval = u16Delta;
		}
	}

	pstTrace->lastWarnCtrlCnt = u32WarnCtrlCnt;
	gu8_AppTraceCurrentWarnCheck = u8CheckId;
}

void AppTrace_WarnCheckEnd(UINT8 u8CheckId)
{
	if (u8CheckId < APP_WARN_CHECK_NUM)
	{
		g_stAppTraceWarnCheck[u8CheckId].active = 0;
	}
	if (gu8_AppTraceCurrentWarnCheck == u8CheckId)
	{
		gu8_AppTraceCurrentWarnCheck = APP_WARN_CHECK_NONE;
	}
}
#endif
