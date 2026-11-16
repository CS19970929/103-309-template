#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <setjmp.h>
typedef uint8_t UINT8; typedef uint16_t UINT16; typedef uint32_t UINT32;
typedef int32_t INT32;
typedef int FLASH_Status;
#include "conf/Project_Config.h"
#include "Flash.h"
#include "SleepDeal.h"
#include "rtc_sleep.h"
#define LIFEPO
#define Feed_IWatchDog (++watchdog_feeds)
#define GPIO_CHG_IN 1
#define PIN_CHG_IN 1
#define Bit_SET 1
#define RCC_APB1Periph_PWR 1
#define RCC_APB1Periph_BKP 2
#define BKP_DR2 2
#define BKP_DR3 3
#define BKP_DR4 4
#define BKP_DR5 5
#define ENABLE 1
static int key=1,charger,adc_on,led,ctlc,one_second;
static int stop_calls,restore_calls,deep_calls,rtc_wake,comm_busy,can_busy;
static UINT32 tick,now_ms,release_at_ms,charge_at_ms,rtc_counter;
static UINT16 bkp[6];
static UINT8 sleep_save_ok=1U;
static unsigned sleep_save_calls, storage_errors, sleep_log_calls, afe_sleep_calls;
static UINT32 runtime_seconds, sleep_log_seconds;
static UINT16 expected_sleep_flag;
static int watchdog_feeds,reset_during_stop;
static jmp_buf reset_point;
static UINT8 comm_counter,u8FlashUpdateFlag;
static enum irqWakeup stop_source;
static int scripted_key,current_wakeup;
static int afe_uv,afe_status_ok=1,soc_now,zero_calls;
static int fake_storage_fault, fake_sensor_fault, fake_afe_healthy=1, fake_afe_fresh=1;
static int fake_full_pending, fake_uv_cells_valid, AFE_PARAM_WRITE_Flag;
static int rtc_init_ok=1, rtc_alarm_ok=1;
static unsigned soc_empty_targets;
#define ERROR_STATUS_AFE1 1
#define ERROR_EEPROM_STORE 2
#define ERROR_STATUS_EEPROM_STORE 3
#define ERROR_STATUS_EEPROM_COM 4
#define ERROR_STATUS_TEMP_BREAK 5
static UINT8 SystemError_IsActive(int code) {
    if(code==ERROR_STATUS_EEPROM_STORE) return (UINT8)fake_storage_fault;
    if(code==ERROR_STATUS_TEMP_BREAK) return (UINT8)fake_sensor_fault;
    return 0U;
}
static UINT8 AFE_IsHealthy(void) { return (UINT8)fake_afe_healthy; }
static UINT8 AFE_IsDataFresh(void) { return (UINT8)fake_afe_fresh; }
static void AFE_MonitorResumeAfterStop(void) {}
static void close_ctlc(void);
void SystemRuntime_PausePersistentFailureTimers(void);
static UINT8 System_ERROR_UserCallback(int command) {
    if(command==ERROR_EEPROM_STORE) { ++storage_errors; return 0U; }
    return (UINT8)!afe_status_ok;
}
static void SOC_RequestSetOnce(UINT8 value) { assert(value==0); soc_now=value; ++zero_calls; }
static void SOC_NotifyOverdischarge(void) { ++soc_empty_targets; soc_now=0; }
static UINT8 SOC_CellVoltagesValid(void) { return (UINT8)(fake_afe_fresh && fake_uv_cells_valid); }
static UINT8 SOC_HasPendingFull(void) { return (UINT8)fake_full_pending; }
#define MCUI_ENI_DI1 key
#define MCUO_DEBUG_LED1 led
#define MCUO_AFE_CTLC ctlc
static void close_ctlc(void) { ctlc=0; }
static struct { UINT16 time_enter_rtc; UINT32 rtc_sec_cnt; } sys_time={5,0};
static struct { UINT16 u16Sleep_Vlow,u16Sleep_TimeVlow,u16Sleep_TimeNormal,u16Sleep_VNormal; } OtherElement={2600,1,10080,3200};
static struct {
    UINT16 u16VCellMin,u16VCell[32],u16Ichg,u16IDischg;
    union { UINT16 all; struct { UINT16 b1CellUvp:1; } bits; } unMdlFault_Third,unMdlFault_Second;
} g_stCellInfoReport;
static struct {
    UINT8 u8_MTP_BALANCEH;
        struct { struct { UINT8 SC,UV,PF; } bits; } REG_BSTATUS1;
    struct { struct { UINT8 CHG_FET,DSG_FET; } bits; } REG_BSTATUS3;
} SH367309_Reg_Store;
static struct { struct { UINT16 curValue; } u16VcellUvp; } AFE_Parameters_RS485_Struction;
static UINT32 SysTime_Get10msTickCount(void) { return tick; }
static UINT32 SysTime_GetMicroseconds(void) { now_ms+=1; return now_ms*1000; }
static void __delay_ms(UINT16 ms) {
    assert(!adc_on); now_ms+=ms;
    if(release_at_ms && now_ms>=release_at_ms) key=1;
    if(charge_at_ms && now_ms>=charge_at_ms) charger=1;
}
static int GPIO_ReadInputDataBit(int gpio,int pin) { return charger; }
static void RCC_APB1PeriphClockCmd(int peripheral,int enable) {}
static void PWR_BackupAccessCmd(int enable) {}
static void BKP_WriteBackupRegister(int reg,UINT16 value) { bkp[reg]=value; }
static UINT16 BKP_ReadBackupRegister(int reg) { return bkp[reg]; }
static UINT8 LowPowerSleep_SaveResetState(void) { ++sleep_save_calls; return sleep_save_ok; }
static UINT8 LowPowerSleep_SaveCoreState(void) { return sleep_save_ok; }
static void InitAFE1_Sleep(int n) {}
static void AFE_Sleep(void) { ++afe_sleep_calls; }
static void fake_reset(void) { ++deep_calls; }
#define MCU_RESET() fake_reset()
static void IOstatus_DeepMode(void) { adc_on=0; }
static void InitWakeUp_DeepMode(void) {}
static void Sys_StopMode(void) {
    if(expected_sleep_flag) assert(BootFlag_Read()==expected_sleep_flag);
    if(reset_during_stop) longjmp(reset_point,1);
    ++stop_calls;
    if(scripted_key==3) {
        /* Held RTC and release IRQ stay asleep; a new press IRQ wakes. */
        key=(stop_calls==3) ? 1 : 0;
        g_irq_t=(stop_calls==2) ? NO_IRQ : soc_key;
    } else {
        if(scripted_key) key=(stop_calls==1) ? 1 : 0;
        g_irq_t=(scripted_key==2 && stop_calls>1) ? soc_key : stop_source;
    }
    if(g_stLowPowerRtcStatus.mode==HICCUP_MODE &&
       (g_irq_t==uart1_irq || g_irq_t==uart2_irq || g_irq_t==rs485_irq))
        g_stLowPowerRtcStatus.comm_wakeup=1U;
    ++rtc_counter;
    if(g_irq_t==NO_IRQ) rtc_wake=1;
}
static UINT16 RtcSleep_PortGetChargeCurrentMa(void) { return g_stCellInfoReport.u16Ichg; }
static UINT16 RtcSleep_PortGetDischargeCurrentMa(void) { return g_stCellInfoReport.u16IDischg; }
static UINT8 RtcSleep_PortIsMcuWakeActive(void) { return key==0; }
static UINT8 Sci_IsAnyPortBusy(void) { return (UINT8)comm_busy; }
static UINT8 Can_IsBusy(void) { return (UINT8)can_busy; }
static UINT8 RtcSleep_PortGetExternalCommCounter(void) { return SleepDeal_GetExternalCommCounter(); }
static UINT8 RTC_IsStopWakeup(void) { return (UINT8)rtc_wake; }
static UINT16 RtcSleep_PortGetCellMinMv(void) { return g_stCellInfoReport.u16VCellMin; }
static UINT16 RtcSleep_PortGetLowVoltageSleepMv(void) { return OtherElement.u16Sleep_Vlow; }
static UINT8 RtcSleep_PortIsOneSecondTick(void) { return (UINT8)one_second; }
static void LogRecord_RequestSleep(void) { ++sleep_log_calls; sleep_log_seconds=runtime_seconds; }
/* RESET_SLEEP_SOURCE */
static UINT8 RtcSleep_PortUpdateRtcData(void) { return 1; }
static UINT8 RtcSleep_AfePortUpdateRtcData(void) { return (UINT8)fake_afe_fresh; }
static UINT8 RtcSleep_PortHasCurrentWake(enum irqWakeup *source) {
    if(current_wakeup) { *source=current_wake; return 1; }
    return 0;
}
#define MTP_BALANCEH 1
static UINT8 MTPRead(int address,int count,UINT8 *data) {
    if(!afe_status_ok) return 0;
    SH367309_Reg_Store.REG_BSTATUS1.bits.UV=(UINT8)afe_uv; return 1;
}
static void SystemRuntime_SetMosStatus(UINT8 chg,UINT8 dsg) {}
static void Fault_ChangeToMCU(void) { g_stCellInfoReport.unMdlFault_Third.bits.b1CellUvp=(UINT16)afe_uv; }
/* AFE_WAKE_SOURCE */
static UINT8 RtcSleep_PortHasAfeWake(enum irqWakeup *source) { return RtcSleep_AfePortHasAfeWake(source); }
static UINT8 Init_RTC(void) { return (UINT8)rtc_init_ok; }
static void IOstatus_RTCMode(void) { adc_on=0; }
static void InitWakeUp_RTCMode(void) {}
static void InitWakeUp_Base(void) {}
static void LowPower_DisableUartWakeup(void) {}
static void LowPower_DisableWakeupExti(void) {}
static void InitTimer(void) { tick=0; }
static void InitUSART_CommonUpper(void) {}
static UINT8 SystemRuntime_IsDischargeMosOpen(void) { return 1; }
static void RTC_ClearStopWakeup(void) { rtc_wake=0; }
static UINT8 RTC_WKTimeConfig(void) { return (UINT8)rtc_alarm_ok; }
static UINT32 RTC_GetCounter(void) { return rtc_counter; }
static void RtcSleep_PortEnterStop(void) { Sys_StopMode(); }
static void initAFE1_IIC(void) {}
static UINT32 RtcSleep_PortGetLastWakeupSeconds(void) { return 1; }
static void RtcSleep_PortDisableStopWakeup(void) {}
static void RtcSleep_PortRestoreAfterStop(void) { ++restore_calls; InitTimer(); adc_on=1; }
static void RtcSleep_PortAddRuntimeSeconds(UINT32 seconds) { runtime_seconds+=seconds; }
/* SLEEP_SOURCE */
/* RTC_SOURCE */
/* ERROR_SLEEP_SOURCE */
static void reset_case(void) {
    memset(&s_sleep,0,sizeof(s_sleep));
    memset(bkp,0,sizeof(bkp));sleep_save_ok=1U;sleep_save_calls=storage_errors=0;
    sleep_log_calls=afe_sleep_calls=0;
    runtime_seconds=sleep_log_seconds=0;
    memset((void *)&g_stLowPowerRtcStatus,0,sizeof(g_stLowPowerRtcStatus));
    g_stLowPowerRtcStatus.mode=NO_SLEEP;
    memset(&g_stCellInfoReport,0,sizeof(g_stCellInfoReport));
    memset(&SH367309_Reg_Store,0,sizeof(SH367309_Reg_Store));
    g_stCellInfoReport.u16VCellMin=3300;
    AFE_Parameters_RS485_Struction.u16VcellUvp.curValue=2800;
    tick=now_ms=release_at_ms=charge_at_ms=rtc_counter=0;
    key=1; charger=adc_on=stop_calls=restore_calls=deep_calls=scripted_key=one_second=0;
    rtc_wake=comm_busy=can_busy=0; stop_source=soc_key;
    afe_uv=zero_calls=0; soc_empty_targets=0; afe_status_ok=1; soc_now=55;
    fake_storage_fault=fake_sensor_fault=fake_full_pending=fake_uv_cells_valid=AFE_PARAM_WRITE_Flag=0;
    fake_afe_healthy=fake_afe_fresh=1;
    rtc_init_ok=rtc_alarm_ok=1;
    s_persistent_error_tracking=0U;
    OtherElement.u16Sleep_Vlow=2600; OtherElement.u16Sleep_TimeVlow=1;
    OtherElement.u16Sleep_TimeNormal=10080;
    expected_sleep_flag=0;
    watchdog_feeds=reset_during_stop=current_wakeup=0;
    BootFlag_Clear();
}
static void test_sleep_save_diagnostic(void) {
    reset_case();g_stLowPowerRtcStatus.mode=DEEP_MODE;s_sleep.undervoltage_handled=1U;
    sleep_save_ok=0U;SleepDeal_Continue(DEEP_MODE);
    assert(sleep_save_calls==1 && deep_calls==1); /* Failure does not veto protective reset. */
    assert(BootFlag_Read()==FLASH_UNDERVOLTAGE_SLEEP_VALUE);
    assert(bkp[BKP_DR4]==SOC_SAVE_FAILED_VALUE && bkp[BKP_DR5]==(UINT16)(~SOC_SAVE_FAILED_VALUE));
    memset(&s_sleep,0,sizeof(s_sleep));storage_errors=0;charger=1;
    SleepDeal_HandleBootSleepStartup();
    assert(storage_errors==1 && bkp[BKP_DR4]==SOC_SAVE_FAILED_VALUE);
    assert(BootFlag_Read()==FLASH_SLEEP_CHARGER_WAKE_VALUE);
    storage_errors=0;SleepDeal_HandleBootSleepStartup();assert(storage_errors==1);
    bkp[BKP_DR5]^=1U;storage_errors=0;BootFlag_Clear();
    SleepDeal_HandleBootSleepStartup();assert(!storage_errors); /* Reject a torn marker. */
    sleep_save_ok=1U;SleepDeal_Continue(DEEP_MODE);
    assert(deep_calls==2 && !bkp[BKP_DR4] && bkp[BKP_DR5]==0xFFFFU);
    storage_errors=0;BootFlag_Clear();SleepDeal_HandleBootSleepStartup();assert(!storage_errors);
    puts("PASS DEEP save diagnostics: failure retains protective reset/reason, marker survives restart, charger wake, torn marker rejected, success clears");
}
int main(void) {
    int i;
    test_sleep_save_diagnostic();
#if PROJECT_CFG_SWITCH_MODE == PROJECT_SWITCH_MAINTAINED
    reset_case(); adc_on=1; key=0;
    for(tick=0;tick<1000;++tick) assert(!SleepDeal_TakeKeySleepRequest());
    key=1; assert(!SleepDeal_TakeKeySleepRequest());
    tick+=4; assert(!SleepDeal_TakeKeySleepRequest());
    key=0; assert(!SleepDeal_TakeKeySleepRequest()); /* Opening bounce is cancelled. */
    key=1; assert(!SleepDeal_TakeKeySleepRequest());
    tick+=4; assert(!SleepDeal_TakeKeySleepRequest()); ++tick;
    assert(SleepDeal_TakeKeySleepRequest());
    tick+=1000; assert(!SleepDeal_TakeKeySleepRequest());
    key=0; assert(!SleepDeal_TakeKeySleepRequest());
    key=1; tick=UINT32_MAX-2; assert(!SleepDeal_TakeKeySleepRequest());
    tick+=4; assert(!SleepDeal_TakeKeySleepRequest()); ++tick;
    assert(SleepDeal_TakeKeySleepRequest());
    /* The existing charger source can run with the switch open; removal
     * re-arms the NORMAL request rather than repeatedly waking and sleeping. */
    reset_case(); key=1; charger=1;
    for(tick=0;tick<1000;++tick) assert(!SleepDeal_TakeKeySleepRequest());
    charger=0; assert(!SleepDeal_TakeKeySleepRequest()); tick+=5;
    assert(SleepDeal_TakeKeySleepRequest());
    for(i=0;i<2;++i) {
        reset_case(); g_stLowPowerRtcStatus.mode=(UINT8)(i ? NORMAL_MODE : HICCUP_MODE);
        key=0; release_at_ms=40; assert(rtc_sleep_run_hiccup_cycle() && !adc_on);
        release_at_ms=0; key=0; now_ms=0;
        assert(!rtc_sleep_run_hiccup_cycle() && now_ms==50 && !adc_on);
    }
    reset_case(); key=1; assert(!SleepDeal_TakeKeySleepRequest()); tick=5;
    scripted_key=1; rtc_sleep();
    assert(stop_calls==2 && restore_calls==1 && g_stLowPowerRtcStatus.mode==NO_SLEEP);
    assert(!SleepDeal_TakeKeySleepRequest()); tick+=1000;
    assert(!SleepDeal_TakeKeySleepRequest()); /* Short stays RUN. */
    /* A switch that was already shorted before protective DEEP is not a
     * fresh wake: it must open, then close after reset. */
    reset_case(); key=0; SleepDeal_ResetKeyState(0U);
    assert(!SleepDeal_IsWakeupValid() && !now_ms);
    key=1; assert(!SleepDeal_IsWakeupValid()); key=0;
    assert(SleepDeal_IsWakeupValid() && now_ms==50);
    puts("PASS maintained: short stays RUN, 50 ms open NORMAL/closed wake, bounce/wrap, charger/remove, both STOP modes, protective DEEP release/reclose");
#else
    reset_case(); adc_on=1; key=0;
    assert(!SleepDeal_TakeKeySleepRequest());
    tick=299; for(i=0;i<1000;++i) assert(!SleepDeal_TakeKeySleepRequest());
    tick=300; assert(SleepDeal_TakeKeySleepRequest());
    tick+=1000; assert(!SleepDeal_TakeKeySleepRequest());
    key=1; assert(!SleepDeal_TakeKeySleepRequest());
    key=0; tick=UINT32_MAX-100; assert(!SleepDeal_TakeKeySleepRequest());
    tick+=299; assert(!SleepDeal_TakeKeySleepRequest()); ++tick; assert(SleepDeal_TakeKeySleepRequest());
    reset_case(); key=0; release_at_ms=990;
    assert(!SleepDeal_IsWakeupValid() && now_ms==990);
    release_at_ms=0; key=0; now_ms=0;
    assert(SleepDeal_IsWakeupValid() && now_ms==1000);
    tick=5000; assert(!SleepDeal_TakeKeySleepRequest());
    key=1; assert(!SleepDeal_TakeKeySleepRequest());
    key=0; assert(!SleepDeal_TakeKeySleepRequest()); tick+=300; assert(SleepDeal_TakeKeySleepRequest());
    /* NORMAL needs 1 s; a short candidate never restores ADC/RUN. */
    reset_case(); g_stLowPowerRtcStatus.mode=NORMAL_MODE;
    key=0; release_at_ms=990; assert(rtc_sleep_run_hiccup_cycle() && !adc_on && !restore_calls);
    release_at_ms=0; key=0; now_ms=0;
    assert(!rtc_sleep_run_hiccup_cycle() && now_ms==1000 && !adc_on);
    /* After NORMAL wake the original press remains blocked, even past 3 s.
     * Only releasing and holding again for a full 3 s requests NORMAL. */
    reset_case(); g_stLowPowerRtcStatus.mode=NORMAL_MODE;
    key=0; scripted_key=1; rtc_sleep();
    assert(stop_calls==2 && restore_calls==1 && now_ms==1000 && adc_on && !tick);
    for(tick=0;tick<=1000;++tick) assert(!SleepDeal_TakeKeySleepRequest());
    key=1; assert(!SleepDeal_TakeKeySleepRequest());
    key=0; assert(!SleepDeal_TakeKeySleepRequest());
    tick+=299; assert(!SleepDeal_TakeKeySleepRequest()); ++tick;
    assert(SleepDeal_TakeKeySleepRequest());
    /* HICCUP accepts a pressed-switch IRQ immediately, including an
     * already-held key state; a release IRQ alone stays asleep. */
    reset_case(); g_stLowPowerRtcStatus.mode=HICCUP_MODE; key=1;
    assert(rtc_sleep_run_hiccup_cycle() && !now_ms && !adc_on);
    key=0; SleepDeal_ResetKeyState(0U);
    assert(!rtc_sleep_run_hiccup_cycle() && !now_ms && !adc_on);
    /* An old key level cannot wake RTC, but a communication edge wakes directly. */
    reset_case(); g_stLowPowerRtcStatus.mode=HICCUP_MODE; key=0; stop_source=NO_IRQ;
    for(i=0;i<3;++i) assert(rtc_sleep_run_hiccup_cycle() && now_ms==0 && !adc_on);
    stop_source=uart1_irq;
    assert(!rtc_sleep_run_hiccup_cycle() && now_ms==0 && !adc_on);
    reset_case(); key=1; stop_source=uart1_irq; g_stLowPowerRtcStatus.mode=HICCUP_MODE;
    rtc_sleep();
    assert(stop_calls==1 && restore_calls==1 && g_stLowPowerRtcStatus.mode==NO_SLEEP && adc_on);
    assert(!g_stLowPowerRtcStatus.comm_wakeup);
    /* RUN timer resets on restore: arm the HICCUP key's 3 s afterwards.
     * On expiry this changes to NORMAL. Held RTC and release IRQ must stay
     * asleep, then the next press must pass NORMAL's 1 s confirmation. */
    reset_case(); g_stLowPowerRtcStatus.mode=HICCUP_MODE; key=0; tick=12345;
    rtc_sleep();
    assert(stop_calls==1 && restore_calls==1 && !tick && !now_ms && adc_on);
    assert(g_stLowPowerRtcStatus.mode==NO_SLEEP);
    tick=299; for(i=0;i<1000;++i) assert(!SleepDeal_TakeKeySleepRequest());
    rtc_sleep(); assert(stop_calls==1 && g_stLowPowerRtcStatus.mode==NO_SLEEP);
    tick=300; scripted_key=3; rtc_sleep();
    assert(stop_calls==4 && restore_calls==2 && !tick && now_ms==1000 && adc_on);
    assert(g_stLowPowerRtcStatus.mode==NO_SLEEP && runtime_seconds==1);
    tick=1000; assert(key==0 && !SleepDeal_TakeKeySleepRequest());
    /* Releasing before 3 s cancels the HICCUP wake timer. */
    reset_case(); g_stLowPowerRtcStatus.mode=HICCUP_MODE; key=0; rtc_sleep();
    tick=299; key=1; assert(!SleepDeal_TakeKeySleepRequest());
    tick=1000; key=0; assert(!SleepDeal_TakeKeySleepRequest());
    tick+=299; assert(!SleepDeal_TakeKeySleepRequest()); ++tick;
    assert(SleepDeal_TakeKeySleepRequest());
    /* Concurrent current classification retains the real key wake; a
     * current-only wake with an old held level must keep the release guard. */
    reset_case(); g_stLowPowerRtcStatus.mode=HICCUP_MODE; key=0; current_wakeup=1;
    rtc_sleep(); assert(g_irq_t==soc_key && !now_ms && restore_calls==1);
    tick=299; assert(!SleepDeal_TakeKeySleepRequest()); ++tick;
    assert(SleepDeal_TakeKeySleepRequest());
    reset_case(); g_stLowPowerRtcStatus.mode=HICCUP_MODE; key=0; current_wakeup=1; stop_source=NO_IRQ;
    rtc_sleep(); assert(g_irq_t==current_wake && !now_ms && restore_calls==1);
    tick=1000; assert(!SleepDeal_TakeKeySleepRequest());
    /* The resetting DEEP path shares the same release and 1 s qualification. */
    reset_case(); BootFlag_Write(FLASH_DEEP_SLEEP_VALUE); key=0; scripted_key=1;
    expected_sleep_flag=FLASH_DEEP_SLEEP_VALUE;
    SleepDeal_HandleBootSleepStartup(); assert(stop_calls==2 && now_ms==1000);
    assert(BootFlag_Read()==BOOT_FLAG_RESET_VALUE);
    assert(watchdog_feeds>=104);
    tick=1000; assert(!SleepDeal_TakeKeySleepRequest());
    reset_case(); charger=1; assert(SleepDeal_IsWakeupValid() && now_ms==0);
    assert(BootFlag_Read()==FLASH_SLEEP_CHARGER_WAKE_VALUE);
    puts("PASS key: NORMAL/DEEP need 1 s wake and release/re-press; HICCUP IRQ wakes immediately and arms 3 s hold after timer restore; RTC/release stay asleep; concurrent current/UART, bounce/wrap/charger");
#endif
    reset_case(); SleepDeal_UpdateOverdischarge(1,1);
    assert(soc_now==55 && zero_calls==0 && soc_empty_targets==0 && g_stLowPowerRtcStatus.mode==DEEP_MODE);
    SleepDeal_Continue(DEEP_MODE); assert(BootFlag_Read()==FLASH_UNDERVOLTAGE_SLEEP_VALUE);
    memset(&s_sleep,0,sizeof(s_sleep)); LowPower_Request(NO_SLEEP);
    scripted_key=1; key=0; expected_sleep_flag=FLASH_UNDERVOLTAGE_SLEEP_VALUE;
    SleepDeal_HandleBootSleepStartup();
    SleepDeal_UpdateOverdischarge(1,1);
    assert(zero_calls==0 && soc_empty_targets==0 && g_stLowPowerRtcStatus.mode==NO_SLEEP);
    key=1; g_stCellInfoReport.u16VCellMin=2200;
    g_stCellInfoReport.unMdlFault_Third.bits.b1CellUvp=1;
    for(i=0;i<59;++i) lp_update_sleep_request();
    assert(g_stLowPowerRtcStatus.mode==NO_SLEEP);
    lp_update_sleep_request();
    assert(g_stLowPowerRtcStatus.mode==DEEP_MODE);
    SleepDeal_UpdateOverdischarge(0,0); SleepDeal_UpdateOverdischarge(1,1);
    assert(zero_calls==0 && soc_empty_targets==0 && g_stLowPowerRtcStatus.mode==DEEP_MODE);
    for(i=0;i<2;++i) {
        reset_case(); g_stLowPowerRtcStatus.mode=(UINT8)(i ? NORMAL_MODE : HICCUP_MODE);
        key=1; stop_source=NO_IRQ; afe_uv=1;
        assert(!rtc_sleep_run_hiccup_cycle() && deep_calls==1 && zero_calls==0 && soc_empty_targets==0 && !adc_on && !restore_calls);
    }
    for(i=0;i<2;++i) {
        reset_case(); key=0; stop_source=soc_key; afe_uv=1;
        g_stLowPowerRtcStatus.mode=(UINT8)(i ? NORMAL_MODE : HICCUP_MODE);
        assert(!rtc_sleep_run_hiccup_cycle() && deep_calls==1 && zero_calls==0 && !adc_on && !restore_calls && !now_ms);
    }
    reset_case(); key=1; stop_source=NO_IRQ; rtc_wake=1;
    afe_uv=1; afe_status_ok=0; g_stLowPowerRtcStatus.mode=HICCUP_MODE;
    assert(!rtc_sleep_run_hiccup_cycle() && !deep_calls && !zero_calls);
    /* A UV with valid evidence may jump, but a stale frame or PF may not.
       Protection itself remains active in every case. */
    reset_case();g_stCellInfoReport.u16VCellMin=2700;
    SleepDeal_UpdateOverdischarge(1,0);
    assert(g_stLowPowerRtcStatus.mode==DEEP_MODE && soc_now==55 && !soc_empty_targets);
    LowPower_Request(NO_SLEEP);fake_uv_cells_valid=1;
    SleepDeal_UpdateOverdischarge(1,1);
    assert(g_stLowPowerRtcStatus.mode==NO_SLEEP && soc_now==0 && soc_empty_targets==1);
    reset_case();g_stCellInfoReport.u16VCellMin=2700;
    SH367309_Reg_Store.REG_BSTATUS1.bits.PF=1;
    SleepDeal_UpdateOverdischarge(1,1);
    assert(g_stLowPowerRtcStatus.mode==DEEP_MODE && soc_now==55 && !soc_empty_targets);
    reset_case();g_stLowPowerRtcStatus.mode=HICCUP_MODE;afe_uv=1;fake_uv_cells_valid=1;
    g_stCellInfoReport.u16VCellMin=2700;
    assert(!rtc_sleep_run_hiccup_cycle());

    assert(deep_calls==1);
    assert(soc_now==0);
    assert(soc_empty_targets==1);
    reset_case();fake_full_pending=1;fake_afe_healthy=fake_afe_fresh=1;
    for(i=0;i<100;i++) lp_update_sleep_request();
    assert(g_stLowPowerRtcStatus.mode==NO_SLEEP && g_stLowPowerRtcStatus.idle==0U);
    fake_full_pending=0;
    puts("PASS UV: new event preserves SOC and requests immediate DEEP; same UV after wake does not retrigger; forced low-voltage timer remains active; recovery re-arms, both RTC modes, invalid status");
    /* Both qualified key and charger wakes retain the handled UV state, but
     * neither may disable the configurable low-voltage sleep timer. */
    for(i=0;i<2;++i) {
        int seconds;
        reset_case(); BootFlag_Write(FLASH_UNDERVOLTAGE_SLEEP_VALUE);
        if(i) charger=1;
        else { key=0; scripted_key=1; }
        SleepDeal_HandleBootSleepStartup();
        SleepDeal_UpdateOverdischarge(1,1);
        assert(!zero_calls && g_stLowPowerRtcStatus.mode==NO_SLEEP);
        key=0; SleepDeal_ResetKeyState(0U);
        g_stCellInfoReport.unMdlFault_Third.bits.b1CellUvp=1;
        OtherElement.u16Sleep_Vlow=LOW_POWER_FORCE_DEEP_SLEEP_MV+100;
        OtherElement.u16Sleep_TimeVlow=2;
        g_stCellInfoReport.u16VCellMin=OtherElement.u16Sleep_Vlow+1;
        one_second=1;
        rtc_sleep(); assert(!g_stLowPowerRtcStatus.vlow && !g_stLowPowerRtcStatus.force);
        g_stCellInfoReport.u16VCellMin=OtherElement.u16Sleep_Vlow;
        g_stCellInfoReport.u16Ichg=LOW_POWER_DEEP_SLEEP_ICHG_LIMIT+1;
        rtc_sleep(); assert(!g_stLowPowerRtcStatus.vlow && !g_stLowPowerRtcStatus.force);
        g_stCellInfoReport.u16Ichg=0;
        one_second=0;
        for(seconds=0;seconds<120;++seconds) rtc_sleep();
        assert(g_stLowPowerRtcStatus.mode==NO_SLEEP && !g_stLowPowerRtcStatus.vlow);
        one_second=1;
        for(seconds=0;seconds<119;++seconds) {
            SleepDeal_UpdateOverdischarge(1,1);
            rtc_sleep();
            assert(g_stLowPowerRtcStatus.mode==NO_SLEEP && !deep_calls && !zero_calls);
        }
        assert(g_stLowPowerRtcStatus.vlow==119 && !g_stLowPowerRtcStatus.force);
        rtc_sleep();
        assert(g_stLowPowerRtcStatus.mode==DEEP_MODE && deep_calls==1 && !zero_calls);
    }
    /* The handled UV flag also must not veto the existing idle, key or host
     * requests. Only a new confirmed UV event has immediate priority. */
    reset_case(); BootFlag_Write(FLASH_UNDERVOLTAGE_SLEEP_VALUE); charger=1;
    SleepDeal_HandleBootSleepStartup(); charger=0; afe_uv=1;
    g_stCellInfoReport.unMdlFault_Third.bits.b1CellUvp=1;
    OtherElement.u16Sleep_TimeNormal=1;
    lp_update_sleep_request(); rtc_counter+=59; lp_update_sleep_request();
    assert(g_stLowPowerRtcStatus.mode==NO_SLEEP);
    ++rtc_counter; lp_update_sleep_request();
    assert(g_stLowPowerRtcStatus.mode==DEEP_MODE);
    LowPower_Request(NO_SLEEP); SleepDeal_ResetKeyState(0U);
#if PROJECT_CFG_SWITCH_MODE == PROJECT_SWITCH_PUSHBUTTON
    key=0;
#endif
    assert(!SleepDeal_TakeKeySleepRequest()); tick+=KEY_SLEEP_TICKS;
    assert(SleepDeal_TakeKeySleepRequest()); LowPower_Request(NORMAL_MODE);
    SleepDeal_UpdateOverdischarge(1,1);
    assert(g_stLowPowerRtcStatus.mode==NORMAL_MODE);
    LowPower_Request(DEEP_MODE); rtc_sleep();
    assert(g_stLowPowerRtcStatus.mode==DEEP_MODE && deep_calls==1 && !zero_calls);
    puts("PASS UV wake: key/charger wakes keep configured voltage/minute delay and second cadence; voltage/charge guards, idle DEEP, both switches and explicit DEEP requests remain effective");
    reset_case(); BootFlag_Write(FLASH_UNDERVOLTAGE_SLEEP_VALUE); reset_during_stop=1;
    if(setjmp(reset_point)==0) { SleepDeal_HandleBootSleepStartup(); assert(0); }
    assert(BootFlag_Read()==FLASH_UNDERVOLTAGE_SLEEP_VALUE);
    memset(&s_sleep,0,sizeof(s_sleep)); reset_during_stop=0; key=0; scripted_key=1;
    expected_sleep_flag=FLASH_UNDERVOLTAGE_SLEEP_VALUE;
    SleepDeal_HandleBootSleepStartup(); SleepDeal_UpdateOverdischarge(1,1);
    assert(!zero_calls && g_stLowPowerRtcStatus.mode==NO_SLEEP);
    assert(BootFlag_Read()==BOOT_FLAG_RESET_VALUE);
    reset_case(); BootFlag_Write(FLASH_UNDERVOLTAGE_SLEEP_VALUE); charger=1;
    SleepDeal_HandleBootSleepStartup();
    SleepDeal_UpdateOverdischarge(1,1);
    assert(!zero_calls && g_stLowPowerRtcStatus.mode==NO_SLEEP && BootFlag_Read()==FLASH_SLEEP_CHARGER_WAKE_VALUE);
    puts("PASS deep restart: sleep reason survives reset during STOP wait; configured key qualification required; watchdog feeds and charger reason preserved");
    reset_case(); assert(!SleepDeal_NormalIdleExpired(0));
    rtc_counter=604799; assert(!SleepDeal_NormalIdleExpired(0));
    ++rtc_counter; assert(SleepDeal_NormalIdleExpired(0));
    reset_case(); rtc_counter=UINT32_MAX-100; assert(!SleepDeal_NormalIdleExpired(0));
    rtc_counter+=604799; assert(!SleepDeal_NormalIdleExpired(0));
    ++rtc_counter; assert(SleepDeal_NormalIdleExpired(0));
    reset_case(); rtc_counter=1000; assert(!SleepDeal_NormalIdleExpired(0));
    rtc_counter=900; assert(!SleepDeal_NormalIdleExpired(0));
    rtc_counter+=604799; assert(!SleepDeal_NormalIdleExpired(0));
    ++rtc_counter; assert(SleepDeal_NormalIdleExpired(0));
    reset_case(); assert(!SleepDeal_NormalIdleExpired(0)); rtc_counter=604799;
    for(i=0;i<256;++i) SleepDeal_RecordExternalComm();
    ++rtc_counter; assert(!SleepDeal_NormalIdleExpired(0));
    rtc_counter+=604799; assert(!SleepDeal_NormalIdleExpired(0));
    ++rtc_counter; assert(SleepDeal_NormalIdleExpired(0));
    reset_case(); OtherElement.u16Sleep_TimeNormal=65000;
    assert(!SleepDeal_NormalIdleExpired(0)); rtc_counter=3900000-1;
    assert(!SleepDeal_NormalIdleExpired(0)); ++rtc_counter; assert(SleepDeal_NormalIdleExpired(0));
    reset_case(); OtherElement.u16Sleep_TimeNormal=1;
    assert(!SleepDeal_NormalIdleExpired(0)); rtc_counter=59; assert(!SleepDeal_NormalIdleExpired(0));
    ++rtc_counter; assert(SleepDeal_NormalIdleExpired(0));
    for(i=0;i<4;++i) {
        reset_case(); sys_time.time_enter_rtc=65535; lp_update_sleep_request();
        rtc_counter=604799;
        if(i==0) g_stCellInfoReport.u16Ichg=10;
        if(i==1) g_stCellInfoReport.u16IDischg=10;
        if(i==2) can_busy=1;
        if(i==3) comm_busy=1;
        lp_update_sleep_request(); ++rtc_counter;
        g_stCellInfoReport.u16Ichg=g_stCellInfoReport.u16IDischg=0; can_busy=comm_busy=0;
        lp_update_sleep_request(); assert(g_stLowPowerRtcStatus.mode==NO_SLEEP);
    }
    reset_case(); sys_time.time_enter_rtc=65535; OtherElement.u16Sleep_VNormal=65535;
    lp_update_sleep_request(); rtc_counter=604800; lp_update_sleep_request();
    assert(g_stLowPowerRtcStatus.mode==DEEP_MODE);
    rtc_sleep();
    assert(deep_calls==1 && sleep_save_calls==1 && sleep_log_calls==1 && afe_sleep_calls==1);
    assert(BootFlag_Read()==FLASH_DEEP_SLEEP_VALUE && !restore_calls && !zero_calls);
    reset_case(); assert(!SleepDeal_NormalIdleExpired(0)); rtc_counter=604799;
    g_stLowPowerRtcStatus.mode=HICCUP_MODE; stop_source=NO_IRQ; key=1; ctlc=1;
    g_stLowPowerRtcStatus.sleep=604799;
    assert(!rtc_sleep_run_hiccup_cycle() && g_stLowPowerRtcStatus.mode==DEEP_MODE && !adc_on);
    assert(deep_calls==1 && sleep_save_calls==1 && sleep_log_calls==1 && afe_sleep_calls==1);
    assert(runtime_seconds==604800 && sleep_log_seconds==604800);
    assert(BootFlag_Read()==FLASH_DEEP_SLEEP_VALUE && !restore_calls && !zero_calls);
    /* The outer HICCUP loop must not restore RUN or re-enable ADC on expiry. */
    reset_case(); OtherElement.u16Sleep_TimeNormal=1;
    assert(!SleepDeal_NormalIdleExpired(0)); rtc_counter=59;
    g_stLowPowerRtcStatus.mode=HICCUP_MODE; stop_source=NO_IRQ; ctlc=1;
    rtc_sleep();
    assert(g_stLowPowerRtcStatus.mode==DEEP_MODE && deep_calls==1 && !restore_calls && !adc_on);
    assert(sleep_save_calls==1 && sleep_log_calls==1 && afe_sleep_calls==1);
    /* Maintained-switch entry restarts idle timing; credit all STOP cycles. */
    assert(runtime_seconds==g_stLowPowerRtcStatus.sleep && runtime_seconds>0);
    assert(sleep_log_seconds==runtime_seconds);
    puts("PASS idle: exact 7 days across STOP, minute range, RTC wrap, 256-frame wrap, charge/discharge/UART/CAN reset, ignored VNormal, RUN/HICCUP commit DEEP without ADC or RUN restore");
    /* Persistent storage and sensor faults use a real 10-ms clock delta;
       recovery restarts the timeout without any 8-bit wraparound. */
    reset_case();
    fake_storage_fault=1;
    SystemRuntime_ObservePersistentFailures();
    tick=29999U;
    SystemRuntime_ObservePersistentFailures();
    assert(g_stLowPowerRtcStatus.mode==NO_SLEEP);
    fake_storage_fault=0;
    SystemRuntime_ObservePersistentFailures();
    fake_storage_fault=1; tick=40000U;
    SystemRuntime_ObservePersistentFailures();
    tick=69999U;
    SystemRuntime_ObservePersistentFailures();
    assert(g_stLowPowerRtcStatus.mode==NO_SLEEP);
    tick=70000U;
    SystemRuntime_ObservePersistentFailures();
    assert(g_stLowPowerRtcStatus.mode==DEEP_MODE);
    sleep_save_ok=0U; rtc_sleep();
    assert(deep_calls==1 && sleep_save_calls==1 && sleep_log_calls==1);
    assert(BootFlag_Read()==FLASH_DEEP_SLEEP_VALUE && bkp[BKP_DR4]==SOC_SAVE_FAILED_VALUE);

    /* RTC configuration failure must restore RUN without executing WFI. */
    reset_case(); g_stLowPowerRtcStatus.mode=NORMAL_MODE;
    rtc_alarm_ok=0; rtc_sleep();
    assert(!stop_calls && restore_calls==1 && g_stLowPowerRtcStatus.mode==NO_SLEEP);
    reset_case(); g_stLowPowerRtcStatus.mode=NORMAL_MODE;
    rtc_init_ok=0; rtc_sleep();
    assert(!stop_calls && !restore_calls && g_stLowPowerRtcStatus.mode==NO_SLEEP);
    puts("PASS error sleep: fault timeouts, recovery, failed snapshot and RTC STOP guards");
    return 0;
}
