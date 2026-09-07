"""Targeted EXTI wake regression. Run in a VS developer shell."""
import os
from pathlib import Path
import run_flash_fit_host_test as host
host.OUT=Path(os.environ['LOCALAPPDATA'])/'CodexTemp/afe-reference-align/stop-wakeup-tests'
host.OUT.mkdir(parents=True,exist_ok=True)
it=(host.ROOT/'103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x_it.c').read_bytes().decode('latin1').replace('\r\n','\n')
code=r'''
#define EXTI_Line3 (1U<<3)
#define EXTI_Line5 (1U<<5)
#define EXTI_Line7 (1U<<7)
#define RESET 0
struct {unsigned IMR,PR;} exti;
#define EXTI (&exti)
enum {NO_IRQ,rtc_alarm_irq,uart1_irq,uart2_irq,soc_key};
static unsigned g_irq_t;
static struct {unsigned cnt_bms1_keyirq;} sys_time;
static unsigned EXTI_GetITStatus(unsigned line){return (exti.IMR & exti.PR & line)!=0;}
static void EXTI_ClearITPendingBit(unsigned line){exti.PR &= ~line;}
'''
for name in ['EXTI3_IRQHandler','EXTI9_5_IRQHandler']:code+=host.function(it,name)
code+=r'''
int main(void){
 unsigned line,reason;
 for(line=3;line<=7;line+=4)for(reason=NO_IRQ;reason<=rtc_alarm_irq;reason++){
  exti.IMR=exti.PR=1U<<line;g_irq_t=reason;
  if(line==3)EXTI3_IRQHandler();else EXTI9_5_IRQHandler();
  assert(g_irq_t==(line==3?uart2_irq:uart1_irq));assert(!exti.PR && !exti.IMR);
  exti.PR=1U<<line; /* Remaining data edges cannot retrigger a masked IRQ. */
  assert(!EXTI_GetITStatus(1U<<line));
 }
 exti.IMR=exti.PR=EXTI_Line5|EXTI_Line7;g_irq_t=rtc_alarm_irq;EXTI9_5_IRQHandler();
 assert(!exti.PR && exti.IMR==EXTI_Line5 && sys_time.cnt_bms1_keyirq==1 && g_irq_t==soc_key);
 exti.IMR=exti.PR=EXTI_Line3;g_irq_t=soc_key;EXTI3_IRQHandler();assert(g_irq_t==soc_key);
 puts("PASS: USART1/2 wake reason, pending clear, one-shot RX, simultaneous RTC/key/UART events");return 0;
}
'''
print(host.run('stop_irq',code).decode())
conf=host.read('conf/conf.c');rtc=host.read('rtc_sleep.c')
assert 'EXTI_Line3 | EXTI_Line5 | EXTI_Line7' in conf
normal=host.function(conf,'InitWakeUp_NormalMode')
assert normal.count('EXTI_Trigger_Falling')==2 and normal.count('GPIO_Mode_IPU')==2
stop=host.function(conf,'Sys_StopMode')
assert stop.index('Conf_ParkRtcSpi')<stop.index('PWR_EnterSTOPMode')<stop.index('cpu_frequency_conf')
assert 'NVIC_ClearPendingIRQ(TIM3_IRQn)' in stop and 'EnableLowPowerDebug' in stop
cycle=host.function(rtc,'rtc_sleep_run_hiccup_cycle')
assert cycle.index('g_irq_t != NO_IRQ && g_irq_t != rtc_alarm_irq')<cycle.index('Afe3520_RestorePort')
print('PASS: dual falling-edge inputs, complete cleanup, SPI reparking and clock-before-ISR order')
