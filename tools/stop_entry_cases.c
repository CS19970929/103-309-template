/* Exercise the real STOP entry around an already serviced communication IRQ. */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
typedef uint32_t UINT32;

#define RCC_APB1Periph_PWR 1
#define RCC_APB1Periph_TIM3 2
#define TIM3 3
#define TIM_IT_Update 1
#define ENABLE 1
#define DISABLE 0
#define PWR_Regulator_LowPower 1
#define PWR_STOPEntry_WFI 1
static volatile struct { uint8_t comm_wakeup; } g_stLowPowerRtcStatus;
static int irq_masked, inject_edge, stop_calls, clock_restores;
static void RCC_APB1PeriphClockCmd(int peripheral,int state) {}
static void TIM_Cmd(int timer,int state) {}
static void TIM_ClearITPendingBit(int timer,int flags) {}
static void LowPower_ClearWakeupPending(void) {
    if(inject_edge) g_stLowPowerRtcStatus.comm_wakeup=1U;
}
static UINT32 __get_PRIMASK(void) { return (UINT32)irq_masked; }
static void __disable_irq(void) { irq_masked=1; }
static void __enable_irq(void) { assert(clock_restores==1); irq_masked=0; }
static void PWR_EnterSTOPMode(int regulator,int entry) {
    assert(irq_masked && !g_stLowPowerRtcStatus.comm_wakeup); ++stop_calls;
}
static void cpu_frequency_conf(void) { assert(irq_masked); ++clock_restores; }
/* STOP_ENTRY_SOURCE */
int main(void) {
    int before_entry, during_prepare;
    for(before_entry=0;before_entry<=1;++before_entry) {
        for(during_prepare=0;during_prepare<=1;++during_prepare) {
            g_stLowPowerRtcStatus.comm_wakeup=(uint8_t)before_entry;
            inject_edge=during_prepare;
            irq_masked=stop_calls=clock_restores=0;
            Sys_StopMode();
            assert(stop_calls==!(before_entry || during_prepare));
            assert(clock_restores==1 && !irq_masked);
            assert(g_stLowPowerRtcStatus.comm_wakeup==(before_entry || during_prepare));
        }
    }
    puts("PASS STOP entry: serviced communication edges before/during preparation skip WFI; clock/IRQ restored; ordinary RTC STOP retained");
    return 0;
}
