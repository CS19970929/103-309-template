#include "main.h"

#if PROJECT_CFG_IRQ_DEBUG_ENABLE
void IrqDebug_Count(volatile uint32_t *counter, uint32_t vector)
{
    uint32_t primask = __get_PRIMASK();
    /* RTC and RTCAlarm share flag counters; an interrupt may preempt another.
     * Keep the shared increment and STOP bitmap together, preserving PRIMASK. */
    __disable_irq();
    ++*counter;
    if (vector != 0U) {
        sys_time.irq.last_vector = vector;
        if (sys_time.irq.stop.active) {
            if (!sys_time.irq.stop.first_vector) sys_time.irq.stop.first_vector = vector;
            sys_time.irq.stop.last_vector = vector;
            if (vector >= 16U && vector < 80U) {
                uint32_t irq = vector - 16U;
                sys_time.irq.stop.serviced_nvic[irq / 32U] |= 1UL << (irq % 32U);
            }
        }
    }
    if (!primask) __enable_irq();
}

/* These two calls run with interrupts masked, on both RTC and DEEP paths.
 * Do not clear pending flags or read USART DR for observation. */
void IrqDebug_BeginStop(void)
{
    ++sys_time.irq.stop.entries;
    sys_time.irq.stop.active = 1U;
    sys_time.irq.stop.first_vector = 0U;
    sys_time.irq.stop.last_vector = 0U;
    sys_time.irq.stop.serviced_nvic[0] = 0U;
    sys_time.irq.stop.serviced_nvic[1] = 0U;
}

void IrqDebug_CaptureStopReturn(void)
{
    ++sys_time.irq.stop.returns;
    sys_time.irq.stop.pending_nvic[0] = NVIC->ISPR[0];
    sys_time.irq.stop.pending_nvic[1] = NVIC->ISPR[1];
    sys_time.irq.stop.enabled_pending_nvic[0] = NVIC->ISPR[0] & NVIC->ISER[0];
    sys_time.irq.stop.enabled_pending_nvic[1] = NVIC->ISPR[1] & NVIC->ISER[1];
    sys_time.irq.stop.exti_pr = EXTI->PR;
    sys_time.irq.stop.exti_imr = EXTI->IMR;
    sys_time.irq.stop.rtc_crl = RTC->CRL;
    sys_time.irq.stop.rtc_crh = RTC->CRH;
    sys_time.irq.stop.gpioa_idr = GPIOA->IDR;
    sys_time.irq.stop.gpiob_idr = GPIOB->IDR;
    sys_time.irq.stop.scb_icsr = SCB->ICSR;
}
#endif
