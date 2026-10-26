#ifndef IRQ_DEBUG_H
#define IRQ_DEBUG_H

#include <stdint.h>

typedef struct {
    uint32_t irq, rxne, txe, tc, idle;
    uint32_t ore, noise, framing, parity;
    uint32_t last_sr, last_cr1, last_cr3, last_error_sr;
} UART_IRQ_DEBUG;

typedef struct {
    struct {
        uint32_t irq0, irq2, irq3, irq9_5, irq15_10;
        uint32_t line0, line2, line3, line5, line6, line7, line9, line12, line13, line17;
    } exti;
    UART_IRQ_DEBUG uart1, uart2, uart3;
    struct {
        uint32_t irq, alarm_irq, second, alarm_flag, overflow_seen;
        uint32_t last_crl, last_crh;
    } rtc;
    struct { uint32_t irq, update; } tim3;
    struct { uint32_t rx0_irq, rx0_frames, last_rf0r; } can;
    struct {
        uint32_t systick, nmi, hardfault, memmanage, busfault, usagefault;
        uint32_t svc, debugmon, pendsv;
    } core;
    uint32_t unhandled, last_unhandled_vector, last_vector;
    struct {
        uint32_t entries, returns, active, first_vector, last_vector;
        uint32_t serviced_nvic[2], pending_nvic[2], enabled_pending_nvic[2];
        uint32_t exti_pr, exti_imr, rtc_crl, rtc_crh, gpioa_idr, gpiob_idr, scb_icsr;
    } stop;
} IRQ_DEBUG;

#if PROJECT_CFG_IRQ_DEBUG_ENABLE
void IrqDebug_Count(volatile uint32_t *counter, uint32_t vector);
void IrqDebug_BeginStop(void);
void IrqDebug_CaptureStopReturn(void);
#define IRQ_DEBUG_ENTER(member) IrqDebug_Count(&sys_time.irq.member, SCB->ICSR & SCB_ICSR_VECTACTIVE_Msk)
#define IRQ_DEBUG_COUNT(member) IrqDebug_Count(&sys_time.irq.member, 0U)
/* Fault/NMI paths must not call diagnostic helpers or mask interrupts. */
#define IRQ_DEBUG_CORE(member) (++sys_time.irq.core.member)
#define IRQ_DEBUG_SET(member, value) (sys_time.irq.member = (value))
#define IRQ_DEBUG_STOP_BEGIN() IrqDebug_BeginStop()
#define IRQ_DEBUG_STOP_CAPTURE() IrqDebug_CaptureStopReturn()
#define IRQ_DEBUG_STOP_END() (sys_time.irq.stop.active = 0U)
#else
#define IRQ_DEBUG_ENTER(member) ((void)0)
#define IRQ_DEBUG_COUNT(member) ((void)0)
#define IRQ_DEBUG_CORE(member) ((void)0)
#define IRQ_DEBUG_SET(member, value) ((void)0)
#define IRQ_DEBUG_STOP_BEGIN() ((void)0)
#define IRQ_DEBUG_STOP_CAPTURE() ((void)0)
#define IRQ_DEBUG_STOP_END() ((void)0)
#endif

#endif
