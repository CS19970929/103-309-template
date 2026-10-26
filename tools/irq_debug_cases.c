#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
typedef uint8_t UINT8; typedef uint16_t UINT16; typedef uint32_t UINT32;
#include "conf/Project_Config.h"
#include "IrqDebug.h"
#include "rtc_sleep.h"
#include "FaultSnapshot.h"
static struct {
#if PROJECT_CFG_IRQ_DEBUG_ENABLE
    volatile IRQ_DEBUG irq;
#endif
    UINT32 can_rcv_cnt;
} sys_time;
static struct { UINT32 PR,IMR; } exti;
static struct { UINT32 ISPR[2],ISER[2]; } nvic;
static struct { UINT32 CRL,CRH; } rtc;
static struct { UINT32 ICSR; } scb;
static struct { UINT32 IDR; } gpio_a,gpio_b;
static struct { UINT32 RF0R; } can;
#define EXTI (&exti)
#define NVIC (&nvic)
#define RTC (&rtc)
#define SCB (&scb)
#define GPIOA (&gpio_a)
#define GPIOB (&gpio_b)
#define CAN1 (&can)
#define SCB_ICSR_VECTACTIVE_Msk 0x1FFU
#define RESET 0
#define ENABLE 1
#define DISABLE 0
#define EXTI_Line0 (1U<<0)
#define EXTI_Line2 (1U<<2)
#define EXTI_Line3 (1U<<3)
#define EXTI_Line5 (1U<<5)
#define EXTI_Line6 (1U<<6)
#define EXTI_Line7 (1U<<7)
#define EXTI_Line9 (1U<<9)
#define EXTI_Line12 (1U<<12)
#define EXTI_Line13 (1U<<13)
#define EXTI_Line17 (1U<<17)
#define RTC_IT_SEC 1U
#define RTC_IT_ALR 2U
#define RTC_FLAG_OW 4U
#define RTC_FLAG_RTOFF 32U
#define TIM3 3
#define TIM_IT_Update 1
#define RCC_APB1Periph_PWR 1
#define RCC_APB1Periph_TIM3 2
#define PWR_Regulator_LowPower 1
#define PWR_STOPEntry_WFI 1
#define CAN_FIFO0 0
#define _COMMOM_UPPER_SCI1
#define _COMMOM_UPPER_SCI2
#define _COMMOM_UPPER_SCI3
volatile enum irqWakeup g_irq_t;
static struct { bool wake; UINT8 disp; UINT32 stop_start_counter,last_wakeup_seconds; } s_rtc;
static UINT32 primask,timer_pending,post_ticks,uart_dispatch[3],can_pending,can_handled;
static UINT16 fault_reason;
static int stop_dispatch,clock_restored;
static void EXTI9_5_IRQHandler(void);
static void RTCAlarm_IRQHandler(void);
static void RTC_IRQHandler(void);
static UINT32 __get_PRIMASK(void) { return primask; }
static void __disable_irq(void) { primask=1; }
static void __enable_irq(void) {
    primask=0;
    if(stop_dispatch) {
        assert(clock_restored); stop_dispatch=0;
        scb.ICSR=39; EXTI9_5_IRQHandler();
        scb.ICSR=57; RTCAlarm_IRQHandler();
        scb.ICSR=19; RTC_IRQHandler(); scb.ICSR=0;
        nvic.ISPR[0]=nvic.ISPR[1]=0;
    }
}
static int EXTI_GetITStatus(UINT32 bit) { return (exti.PR & exti.IMR & bit)!=0; }
static void EXTI_ClearITPendingBit(UINT32 bit) { exti.PR &= ~bit; }
static int RTC_GetITStatus(UINT16 bit) { return (rtc.CRL & rtc.CRH & bit)!=0; }
static void RTC_ClearITPendingBit(UINT16 bit) { rtc.CRL &= ~bit; }
static UINT8 RTC_WaitForLastTaskSafe(void) { return 1; }
static UINT32 RTC_GetCounter(void) { return 25; }
static void Fault_ResetOrHold(UINT16 reason) { fault_reason=reason; }
static void Sci1_CommonUpper_IRQHandler(void) { ++uart_dispatch[0]; }
static void Sci2_CommonUpper_IRQHandler(void) { ++uart_dispatch[1]; }
static void Sci3_CommonUpper_IRQHandler(void) { ++uart_dispatch[2]; }
static int TIM_GetITStatus(int timer,int flag) { return timer_pending!=0; }
static void TIM_ClearITPendingBit(int timer,int flag) { timer_pending=0; }
static void SysTime_Post10msTick(void) { ++post_ticks; }
typedef struct { int unused; } CanRxMsg;
static UINT32 CAN_MessagePending(void *p,int fifo) { return can_pending; }
static void CAN_Receive(void *p,int fifo,CanRxMsg *msg) { --can_pending; }
static void can_handle_rx_msg(CanRxMsg *msg) { ++can_handled; }
static void RCC_APB1PeriphClockCmd(int peripheral,int enable) {}
static void TIM_Cmd(int timer,int enable) {}
static void LowPower_ClearWakeupPending(void) { exti.PR=0; }
static void PWR_EnterSTOPMode(int regulator,int entry) {
    assert(primask==1);
#if PROJECT_CFG_IRQ_DEBUG_ENABLE
    assert(sys_time.irq.stop.active && sys_time.irq.stop.entries==1);
#endif
    exti.PR=EXTI_Line7|EXTI_Line9|EXTI_Line17; exti.IMR=exti.PR;
    rtc.CRL=RTC_IT_SEC|RTC_IT_ALR|RTC_FLAG_RTOFF; rtc.CRH=RTC_IT_SEC|RTC_IT_ALR;
    nvic.ISPR[0]=(1U<<3)|(1U<<23); nvic.ISER[0]=nvic.ISPR[0];
    nvic.ISPR[1]=(1U<<9)|(1U<<6); nvic.ISER[1]=(1U<<9);
    gpio_a.IDR=0x200; gpio_b.IDR=0x80; stop_dispatch=1;
}
static void cpu_frequency_conf(void) {
    assert(primask==1);
#if PROJECT_CFG_IRQ_DEBUG_ENABLE
    assert(sys_time.irq.stop.returns==1 && sys_time.irq.stop.exti_pr==exti.PR);
#endif
    clock_restored=1;
}
/* REAL_SOURCE */
int main(void) {
    unsigned bit;
    scb.ICSR=39; exti.IMR=exti.PR=EXTI_Line5|EXTI_Line6|EXTI_Line7|EXTI_Line9;
    EXTI9_5_IRQHandler(); assert(!exti.PR && g_irq_t==soc_key);
    scb.ICSR=56; exti.IMR=exti.PR=EXTI_Line12|EXTI_Line13;
    EXTI15_10_IRQHandler(); assert(!exti.PR);
    scb.ICSR=22; exti.IMR=exti.PR=EXTI_Line0; EXTI0_IRQHandler();
    scb.ICSR=24; exti.IMR=exti.PR=EXTI_Line2; EXTI2_IRQHandler();
    scb.ICSR=25; exti.IMR=exti.PR=EXTI_Line3; EXTI3_IRQHandler(); assert(g_irq_t==uart2_irq);
    EXTI3_IRQHandler(); /* Spurious vector entry, no second line event. */
    scb.ICSR=53; USART1_IRQHandler(); scb.ICSR=54; USART2_IRQHandler();
    scb.ICSR=55; USART3_IRQHandler(); assert(uart_dispatch[0]==1 && uart_dispatch[1]==1 && uart_dispatch[2]==1);
    scb.ICSR=45; timer_pending=1; TIM3_IRQHandler(); TIM3_IRQHandler(); assert(post_ticks==1);
    scb.ICSR=36; can.RF0R=0x13; can_pending=3; USB_LP_CAN1_RX0_IRQHandler();
    assert(can_handled==3 && sys_time.can_rcv_cnt==3);
    NMI_Handler(); SVC_Handler(); DebugMon_Handler(); PendSV_Handler();
    scb.ICSR=15; SysTick_Handler(); HardFault_Handler(); assert(fault_reason==FAULT_REASON_HARD);
    MemManage_Handler(); assert(fault_reason==FAULT_REASON_MEM);
    BusFault_Handler(); assert(fault_reason==FAULT_REASON_BUS);
    UsageFault_Handler(); assert(fault_reason==FAULT_REASON_USAGE);
    scb.ICSR=42; IrqDebug_RecordUnhandledVector();
#if PROJECT_CFG_IRQ_DEBUG_ENABLE
    assert(sys_time.irq.exti.irq9_5==1 && sys_time.irq.exti.line5==1 && sys_time.irq.exti.line6==1);
    assert(sys_time.irq.exti.line7==1 && sys_time.irq.exti.line9==1 && sys_time.irq.exti.line12==1 && sys_time.irq.exti.line13==1);
    assert(sys_time.irq.exti.irq3==2 && sys_time.irq.exti.line3==1);
    assert(sys_time.irq.uart1.irq==1 && sys_time.irq.uart2.irq==1 && sys_time.irq.uart3.irq==1);
    assert(sys_time.irq.tim3.irq==2 && sys_time.irq.tim3.update==1);
    assert(sys_time.irq.can.rx0_irq==1 && sys_time.irq.can.rx0_frames==3 && sys_time.irq.can.last_rf0r==0x13);
    assert(sys_time.irq.core.nmi==1 && sys_time.irq.core.hardfault==1 && sys_time.irq.core.memmanage==1);
    assert(sys_time.irq.core.busfault==1 && sys_time.irq.core.usagefault==1 && sys_time.irq.core.svc==1);
    assert(sys_time.irq.core.debugmon==1 && sys_time.irq.core.pendsv==1 && sys_time.irq.core.systick==1);
    assert(sys_time.irq.unhandled==1 && sys_time.irq.last_unhandled_vector==42);
    /* Counter wrap and existing PRIMASK are preserved. */
    sys_time.irq.exti.line0=UINT32_MAX; primask=1;
    IrqDebug_Count(&sys_time.irq.exti.line0,0); assert(primask==1 && !sys_time.irq.exti.line0); primask=0;
    /* Both bitmap words and every hardware vector position stay bounded. */
    IrqDebug_BeginStop();
    for(bit=16;bit<80;++bit) IrqDebug_Count(&sys_time.irq.unhandled,bit);
    assert(sys_time.irq.stop.serviced_nvic[0]==UINT32_MAX && sys_time.irq.stop.serviced_nvic[1]==UINT32_MAX);
    IrqDebug_Count(&sys_time.irq.unhandled,500);
#endif
    memset(&sys_time,0,sizeof(sys_time)); scb.ICSR=0; Sys_StopMode();
    assert(!primask && !stop_dispatch && !exti.PR && !(rtc.CRL & (RTC_IT_SEC|RTC_IT_ALR)));
#if PROJECT_CFG_IRQ_DEBUG_ENABLE
    assert(sys_time.irq.stop.entries==1 && sys_time.irq.stop.returns==1 && !sys_time.irq.stop.active);
    assert(sys_time.irq.stop.first_vector==39 && sys_time.irq.stop.last_vector==19);
    assert(sys_time.irq.stop.serviced_nvic[0]==((1U<<3)|(1U<<23)) && sys_time.irq.stop.serviced_nvic[1]==(1U<<9));
    assert(sys_time.irq.stop.pending_nvic[1]==((1U<<9)|(1U<<6)) && sys_time.irq.stop.enabled_pending_nvic[1]==(1U<<9));
    assert(sys_time.irq.stop.exti_pr==(EXTI_Line7|EXTI_Line9|EXTI_Line17));
    assert(sys_time.irq.stop.rtc_crl==(RTC_IT_SEC|RTC_IT_ALR|RTC_FLAG_RTOFF));
    assert(sys_time.irq.stop.gpioa_idr==0x200 && sys_time.irq.stop.gpiob_idr==0x80);
    assert(sys_time.irq.rtc.irq==1 && sys_time.irq.rtc.alarm_irq==1 && sys_time.irq.rtc.second==1);
    assert(sys_time.irq.rtc.alarm_flag==1 && sys_time.irq.exti.line17==1 && s_rtc.wake);
    scb.ICSR=45; timer_pending=1; TIM3_IRQHandler();
    assert(sys_time.irq.stop.first_vector==39 && sys_time.irq.stop.last_vector==19);
#endif
    printf("PASS IRQ observation mode %d: all vectors/lines, RTC flags, UART dispatch, TIM3, CAN, faults, STOP pre-clear capture and clock order; structure %zu bytes\n",PROJECT_CFG_IRQ_DEBUG_ENABLE,sizeof(IRQ_DEBUG));
    return 0;
}
