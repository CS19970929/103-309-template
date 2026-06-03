#include "IrqDebug.h"
#include "System_Init.h"

volatile struct IRQ_DEBUG_STATE g_stIrqDebug = {
	.current_phase = (uint8_t)IRQDBG_PHASE_BOOT,
};

#if PROJECT_CFG_IRQ_DEBUG_ENABLE

void IrqDebug_SetPhase(uint8_t phase)
{
	if (phase >= (uint8_t)IRQDBG_PHASE_COUNT)
	{
		return;
	}

	g_stIrqDebug.last_phase = g_stIrqDebug.current_phase;
	g_stIrqDebug.current_phase = phase;
	g_stIrqDebug.phase_enter_count[phase]++;
	g_stIrqDebug.last_tick_10ms = SysTime_Get10msTickCount();
}

void IrqDebug_RecordEvent(uint8_t id)
{
#if PROJECT_CFG_IRQ_DEBUG_EVENT_ENABLE
	uint8_t idx;

	if (id >= (uint8_t)IRQDBG_COUNT)
	{
		return;
	}

	idx = g_stIrqDebug.event_head;
	g_stIrqDebug.events[idx].seq = ++g_stIrqDebug.event_seq;
	g_stIrqDebug.events[idx].tick_10ms = SysTime_Get10msTickCount();
	g_stIrqDebug.events[idx].exti_pr = g_stIrqDebug.last_exti_pr;
	g_stIrqDebug.events[idx].nvic_ispr0 = g_stIrqDebug.last_nvic_ispr0;
	g_stIrqDebug.events[idx].nvic_iabr0 = g_stIrqDebug.last_nvic_iabr0;
	g_stIrqDebug.events[idx].id = (uint16_t)id;
	g_stIrqDebug.events[idx].vectactive = g_stIrqDebug.last_vectactive;
	g_stIrqDebug.events[idx].phase = g_stIrqDebug.last_phase;
	g_stIrqDebug.events[idx].reserved = 0U;
	g_stIrqDebug.event_head = (uint8_t)((idx + 1U) % IRQ_DEBUG_EVENT_RING_SIZE);
	if (g_stIrqDebug.event_count < IRQ_DEBUG_EVENT_RING_SIZE)
	{
		g_stIrqDebug.event_count++;
	}
#else
	(void)id;
#endif
}

void IrqDebug_RecordUnhandledVector(void)
{
	IrqDebug_Count((uint8_t)IRQDBG_UNHANDLED_VECTOR);
}

#else

void IrqDebug_RecordUnhandledVector(void)
{
}

#endif
