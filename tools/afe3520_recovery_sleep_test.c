/* Compile selected production sleep functions with controllable board services.
 * The runner extracts the current function bodies, not copies of their logic. */
#include "afe3520_test_stubs/conf.h"
#include "afe3520/BmsProtection3520.h"
#include "afe3520/Afe3520Config.h"
#include "rtc_sleep.h"
#include "System_Monitor.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static AFE3520_SNAPSHOT snapshot;
static BMS3520_PROTECTION_STATUS protection;
static uint32_t blocks;
static unsigned dirty, charge, discharge, key, busy, flash_busy, ext_comm;
static unsigned sleep_ack, forced_on, commits, resets, flags, saves, fault_calls;
static unsigned rtc_valid, current_wake_test, afe_wake_test;
static uint16_t cell_min;
static struct { unsigned time_enter_rtc; } sys_time;
static struct { uint16_t u16Sleep_TimeVlow; } OtherElement;
static struct { struct { unsigned all; } unMdlFault_Third, unMdlFault_Second; } g_stCellInfoReport;
static unsigned u8FlashUpdateE2PROM, u8FlashUpdateFlag;
volatile struct LOW_POWER_RTC_STATUS g_stLowPowerRtcStatus;
volatile enum irqWakeup g_irq_t;
#define LOW_POWER_FORCE_DEEP_SLEEP_MV 2650U
#define LOW_POWER_FORCE_DEEP_SLEEP_SECONDS 60U
#define LOW_POWER_DEEP_SLEEP_ICHG_LIMIT 5U
#define FLASH_NORMAL_SLEEP_VALUE 1U
#define FLASH_DEEP_SLEEP_VALUE 2U
#define MCU_RESET() (++resets)
const AFE3520_SNAPSHOT *Afe3520_GetSnapshot(void) { return &snapshot; }
uint8_t Afe3520_ConfigDirty(void) { return dirty; }
const BMS3520_PROTECTION_STATUS *Bms3520_GetProtectionStatus(void) { return &protection; }
uint32_t Bms3520_GetBlockMask(void) { return blocks; }
void Bms3520_SetSystemBlock(uint8_t on) {
    if(on) { protection.mosFeedbackValid=1; protection.actualCharge=0; protection.actualDischarge=forced_on; }
}
void Bms3520_HandleCommFault(void) { ++fault_calls; snapshot.valid=0; }
AFE3520_RESULT Afe3520_EnterSleep(void) { return sleep_ack ? AFE3520_OK : AFE3520_ERR_ACK; }
static unsigned RtcSleep_PortGetChargeCurrentA10(void) { return charge; }
static unsigned RtcSleep_PortGetDischargeCurrentA10(void) { return discharge; }
static unsigned RtcSleep_PortIsMcuWakeActive(void) { return key; }
static unsigned RtcSleep_PortGetExternalCommCounter(void) { return ext_comm; }
static unsigned RtcSleep_PortGetCellMinMv(void) { return cell_min; }
static unsigned RtcSleep_PortGetLowVoltageSleepMv(void) { return 2800; }
static unsigned Sci_IsAnyPortBusy(void) { return busy; }
static unsigned Can_IsBusy(void) { return 0; }
static unsigned StorageFlash_IsBusy(void) { return flash_busy; }
static unsigned RTC_IsStopWakeup(void) { return 0; }
static void RtcSleep_PortCommitResetSleep(unsigned mode) { (void)mode; ++commits; }
static void LowPowerSleep_SaveResetState(void) { ++saves; }
static void BootFlag_Write(unsigned flag) { flags=flag; }
static unsigned RtcSleep_PortUpdateRtcData(void) { return rtc_valid; }
static unsigned RtcSleep_PortHasCurrentWake(enum irqWakeup *source) { *source=current_wake; return current_wake_test; }
static unsigned RtcSleep_PortHasAfeWake(enum irqWakeup *source) { *source=error_wake; return afe_wake_test; }
static volatile UINT8 error_count;
static volatile UINT8 *System_ErrorField(enum SYSTEM_ERROR_COMMAND code) { (void)code; return &error_count; }
static volatile UINT8 *System_ErrorCommandField(enum SYSTEM_ERROR_COMMAND code) { (void)code; return &error_count; }
static unsigned primask, stop_calls, clock_restores, inject_wake;
#define RCC_APB1Periph_PWR 1
#define RCC_APB1Periph_TIM3 2
#define TIM3 3
#define TIM_IT_Update 1
#define PWR_Regulator_LowPower 1
#define PWR_STOPEntry_WFI 1
static unsigned __get_PRIMASK(void) { return primask; }
static void __disable_irq(void) { primask=1; if(inject_wake) g_irq_t=soc_key; }
static void __set_PRIMASK(unsigned value) { primask=value; }
static void RCC_APB1PeriphClockCmd(int a,int b) { (void)a; (void)b; }
static void TIM_Cmd(int a,int b) { (void)a; (void)b; }
static void TIM_ClearITPendingBit(int a,int b) { (void)a; (void)b; }
static void PWR_EnterSTOPMode(int a,int b) { (void)a; (void)b; assert(primask==1); ++stop_calls; }
static void cpu_frequency_conf(void) { ++clock_restores; }
uint8_t GPIO_ReadInputDataBit(GPIO_TypeDef *port, uint16_t pin) { (void)port; return pin==PIN_KEY1 ? !key : 0; }
GPIO_TypeDef host_gpioa, host_gpiob;
#include "afe3520_sleep_functions.inc"
static void healthy(void) {
    memset(&snapshot,0,sizeof(snapshot)); snapshot.valid=1;
    memset(&protection,0,sizeof(protection)); protection.configValid=1;
    memset((void*)&g_stLowPowerRtcStatus,0,sizeof(g_stLowPowerRtcStatus));
    g_stLowPowerRtcStatus.mode=NO_SLEEP; g_irq_t=NO_IRQ;
    blocks=dirty=charge=discharge=key=busy=flash_busy=ext_comm=0;
    commits=resets=flags=saves=fault_calls=forced_on=0;
    u8FlashUpdateFlag=u8FlashUpdateE2PROM=0;
    sleep_ack=rtc_valid=1; current_wake_test=afe_wake_test=0;
    cell_min=3300; sys_time.time_enter_rtc=3; OtherElement.u16Sleep_TimeVlow=1;
}
int main(void) {
    unsigned i;
    healthy(); snapshot.valid=0; cell_min=0; LowPower_Request(DEEP_MODE);
    for(i=0;i<65;i++) lp_update_sleep_request();
    assert(g_stLowPowerRtcStatus.mode==NO_SLEEP && (LP_GetBlockReason()&LP_BLOCK_AFE));
    healthy(); blocks=AFE3520_BLOCK_GLOBAL_AFE_COMM; assert(LP_GetBlockReason()&LP_BLOCK_AFE);
    healthy(); dirty=1; assert(LP_GetBlockReason()&LP_BLOCK_AFE);
    puts("PASS: invalid samples, dirty configuration and recovering AFE cancel pending sleep");
    healthy(); cell_min=2400; u8FlashUpdateFlag=1;
    for(i=0;i<65;i++) lp_update_sleep_request();
    assert(g_stLowPowerRtcStatus.mode==NO_SLEEP);
    healthy(); cell_min=2400; flash_busy=1; lp_update_sleep_request(); assert(g_stLowPowerRtcStatus.mode==NO_SLEEP);
    healthy(); cell_min=2400; discharge=10; lp_update_sleep_request(); assert(g_stLowPowerRtcStatus.mode==NO_SLEEP);
    healthy(); cell_min=2400; key=1; lp_update_sleep_request(); assert(g_stLowPowerRtcStatus.mode==NO_SLEEP);
    puts("PASS: undervoltage cannot bypass upgrade, storage, discharge or active wake blockers");
#if AFE3520_CFG_WDT_ENABLE
    healthy(); for(i=0;i<65;i++) lp_update_sleep_request();
    assert(g_stLowPowerRtcStatus.mode==NO_SLEEP && (LP_GetBlockReason()&LP_BLOCK_AFE_WDT));
    SleepDeal_Continue(DEEP_MODE); assert(!resets && !flags);
    puts("PASS: enabled external WDT prevents STOP/reset-sleep");
#else
    healthy(); for(i=0;i<3;i++) lp_update_sleep_request(); assert(g_stLowPowerRtcStatus.mode==HICCUP_MODE);
    healthy(); cell_min=2400; for(i=0;i<60;i++) lp_update_sleep_request(); assert(g_stLowPowerRtcStatus.mode==DEEP_MODE);
    puts("PASS: valid idle/undervoltage retains normal sleep selection");
    healthy(); sleep_ack=0; SleepDeal_Continue(DEEP_MODE);
    assert(!resets && !flags && !saves && fault_calls && g_stLowPowerRtcStatus.mode==NO_SLEEP);
    healthy(); forced_on=1; SleepDeal_Continue(DEEP_MODE); assert(!resets && !flags && fault_calls);
    puts("PASS: sleep ACK failure or FET still on never commits boot flag or resets MCU");
    healthy(); SleepDeal_Continue(DEEP_MODE); assert(resets==1 && flags==FLASH_DEEP_SLEEP_VALUE && saves==1);
    healthy(); SleepDeal_Continue(NORMAL_MODE); assert(resets==1 && flags==FLASH_NORMAL_SLEEP_VALUE);
    puts("PASS: confirmed MOS-off/sleep ACK commits the requested mode");
#endif
    healthy(); g_stLowPowerRtcStatus.mode=HICCUP_MODE; afe_wake_test=1;
    assert(rtc_sleep_has_wakeup_exception() && g_irq_t==error_wake);
    healthy(); rtc_valid=0; assert(rtc_sleep_has_wakeup_exception() && g_irq_t==error_wake);
    puts("PASS: RTC sampling/protection errors retain the fault wake source");
    healthy(); primask=stop_calls=clock_restores=inject_wake=0;
    g_irq_t=soc_key; Sys_StopMode(); assert(!stop_calls && !primask);
    g_irq_t=NO_IRQ; inject_wake=1; Sys_StopMode(); assert(!stop_calls && !primask);
    g_irq_t=NO_IRQ; inject_wake=0; key=1; Sys_StopMode(); assert(!stop_calls);
    key=0; Sys_StopMode(); assert(stop_calls==1 && clock_restores==1 && !primask);
    primask=1; Sys_StopMode(); assert(primask==1);
    puts("PASS: STOP preserves pending/arriving wake events and prior interrupt mask");
    error_count=0;
    for(i=0;i<1024;i++) System_ERROR_UserCallback(ERROR_AFE1);
    assert(System_ERROR_UserCallback(ERROR_STATUS_AFE1)==255);
    System_ERROR_UserCallback(ERROR_REMOVE_AFE1);
    assert(System_ERROR_UserCallback(ERROR_STATUS_AFE1)==0);
    puts("PASS: persistent system error counters saturate instead of wrapping to healthy");
    return 0;
}
