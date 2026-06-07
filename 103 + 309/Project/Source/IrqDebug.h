#ifndef IRQ_DEBUG_H
#define IRQ_DEBUG_H

#include "Project_Config.h"
#include "stm32f10x.h"
#include <stdint.h>

enum IRQ_DEBUG_ID {
	IRQDBG_NMI = 0,
	IRQDBG_HARDFAULT,
	IRQDBG_MEMMANAGE,
	IRQDBG_BUSFAULT,
	IRQDBG_USAGEFAULT,
	IRQDBG_SVC,
	IRQDBG_DEBUGMON,
	IRQDBG_PENDSV,
	IRQDBG_SYSTICK,
	IRQDBG_EXTI0_CHG_IN,
	IRQDBG_EXTI2_STRAY,
	IRQDBG_EXTI3_STRAY,
	IRQDBG_EXTI5_STRAY,
	IRQDBG_EXTI6_SOC_KEY,
	IRQDBG_EXTI7_UART1_WAKE,
	IRQDBG_EXTI9_MAIN_SW,
	IRQDBG_EXTI12_CMNT_WAKE,
	IRQDBG_EXTI13_RESERVED,
	IRQDBG_USART1,
	IRQDBG_USART2,
	IRQDBG_USART3,
	IRQDBG_RTC_SEC,
	IRQDBG_RTC_ALARM,
	IRQDBG_RTC_ALARM_IN_RTC_IRQ,
	IRQDBG_TIM3_10MS,
	IRQDBG_TIM4_LEDBAR,
	IRQDBG_CAN1_RX0,
	IRQDBG_EXTI_GROUP_SPURIOUS,
	IRQDBG_UNHANDLED_VECTOR,
	IRQDBG_COUNT
};

enum IRQ_DEBUG_PHASE {
	IRQDBG_PHASE_BOOT = 0,
	IRQDBG_PHASE_RUN,
	IRQDBG_PHASE_SLEEP_PREPARE,
	IRQDBG_PHASE_STOP_WAIT,
	IRQDBG_PHASE_STOP_WAKE_RAW,
	IRQDBG_PHASE_STOP_RESTORE,
	IRQDBG_PHASE_RESET_SLEEP_WAIT,
	IRQDBG_PHASE_FAULT,
	IRQDBG_PHASE_COUNT
};

#define IRQ_DEBUG_EVENT_RING_SIZE 32U

struct IRQ_DEBUG_EVENT {
	uint32_t seq;
	uint32_t tick_10ms;
	uint32_t exti_pr;
	uint32_t nvic_ispr0;
	uint32_t nvic_iabr0;
	uint16_t id;
	uint16_t vectactive;
	uint8_t phase;
	uint8_t reserved;
};

struct IRQ_DEBUG_STATE {
	volatile uint32_t total[IRQDBG_COUNT];
	volatile uint32_t phase[IRQDBG_PHASE_COUNT][IRQDBG_COUNT];
	volatile uint32_t phase_enter_count[IRQDBG_PHASE_COUNT];
	volatile uint32_t event_seq;
	volatile uint16_t last_id;
	volatile uint16_t last_vectactive;
	volatile uint8_t current_phase;
	volatile uint8_t last_phase;
	volatile uint32_t last_tick_10ms;
	volatile uint32_t last_exti_pr;
	volatile uint32_t last_nvic_ispr0;
	volatile uint32_t last_nvic_iabr0;
	volatile uint8_t event_head;
	volatile uint8_t event_count;
	volatile struct IRQ_DEBUG_EVENT events[IRQ_DEBUG_EVENT_RING_SIZE];
};

extern volatile struct IRQ_DEBUG_STATE g_stIrqDebug;

#if PROJECT_CFG_IRQ_DEBUG_ENABLE

void IrqDebug_SetPhase(uint8_t phase);
void IrqDebug_RecordEvent(uint8_t id);
void IrqDebug_RecordUnhandledVector(void);

static __INLINE void IrqDebug_CountFast(uint8_t id)
{
	uint8_t phase;

	if (id >= (uint8_t)IRQDBG_COUNT)
	{
		return;
	}

	phase = g_stIrqDebug.current_phase;
	g_stIrqDebug.total[id]++;
	if (phase < (uint8_t)IRQDBG_PHASE_COUNT)
	{
		g_stIrqDebug.phase[phase][id]++;
	}

	g_stIrqDebug.last_id = (uint16_t)id;
	g_stIrqDebug.last_phase = phase;
	g_stIrqDebug.last_vectactive = (uint16_t)(SCB->ICSR & 0x01FFU);
	g_stIrqDebug.last_exti_pr = EXTI->PR;
	g_stIrqDebug.last_nvic_ispr0 = NVIC->ISPR[0];
	g_stIrqDebug.last_nvic_iabr0 = NVIC->IABR[0];
}

static __INLINE void IrqDebug_Count(uint8_t id)
{
	IrqDebug_CountFast(id);
	IrqDebug_RecordEvent(id);
}

#else

#define IrqDebug_SetPhase(phase)          do{}while(0)
#define IrqDebug_CountFast(id)            do{}while(0)
#define IrqDebug_Count(id)                do{}while(0)
void IrqDebug_RecordUnhandledVector(void);

#endif

#endif
