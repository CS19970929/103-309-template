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
#define ENABLE 1
static int key=1,charger,adc_on,led,ctlc,one_second;
static int stop_calls,restore_calls,deep_calls,rtc_wake,comm_busy,can_busy,flash_busy;
static UINT32 tick,now_ms,release_at_ms,charge_at_ms,rtc_counter;
static UINT16 bkp[4];
static UINT16 expected_sleep_flag;
static int watchdog_feeds,reset_during_stop;
static jmp_buf reset_point;
static UINT8 comm_counter,u8FlashUpdateFlag;
static enum irqWakeup stop_source;
static int scripted_key,key_irq_during_verify,current_wakeup;
#if PROJECT_CFG_STORAGE_SELF_TEST_ENABLE
static UINT8 self_test_active;
static UINT8 FlashSelfTest_IsActive(void) { return self_test_active; }
#endif
static int afe_uv,afe_status_ok=1,soc_now,zero_calls;
#define ERROR_STATUS_AFE1 1
static UINT8 System_ERROR_UserCallback(int command) { return (UINT8)!afe_status_ok; }
static void SOC_RequestSetOnce(UINT8 value) { assert(value==0); soc_now=value; ++zero_calls; }
#define MCUI_ENI_DI1 key
#define MCUO_DEBUG_LED1 led
#define MCUO_AFE_CTLC ctlc
static struct { UINT16 time_enter_rtc; UINT32 rtc_sec_cnt; } sys_time={5,0};
static struct { UINT16 u16Sleep_Vlow,u16Sleep_TimeVlow,u16Sleep_TimeNormal,u16Sleep_VNormal; } OtherElement={2600,1,10080,3200};
static struct {
    UINT16 u16VCellMin,u16VCell[32],u16Ichg,u16IDischg;
    union { UINT16 all; struct { UINT16 b1CellUvp:1; } bits; } unMdlFault_Third,unMdlFault_Second;
} g_stCellInfoReport;
static struct {
    UINT8 u8_MTP_BALANCEH;
    struct { struct { UINT8 SC,UV; } bits; } REG_BSTATUS1;
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
static void LowPowerSleep_SaveResetState(void) {}
static void LowPowerSleep_SaveCoreState(void) {}
static void InitAFE1_Sleep(int n) {}
static void AFE_Sleep(void) {}
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
    ++rtc_counter;
    if(g_irq_t==NO_IRQ) rtc_wake=1;
}
static UINT16 RtcSleep_PortGetChargeCurrentMa(void) { return g_stCellInfoReport.u16Ichg; }
static UINT16 RtcSleep_PortGetDischargeCurrentMa(void) { return g_stCellInfoReport.u16IDischg; }
static UINT8 RtcSleep_PortIsMcuWakeActive(void) { return key==0; }
static UINT8 Sci_IsAnyPortBusy(void) { return (UINT8)comm_busy; }
static UINT8 Can_IsBusy(void) { return (UINT8)can_busy; }
static UINT8 StorageFlash_IsBusy(void) { return (UINT8)flash_busy; }
static UINT8 RtcSleep_PortGetExternalCommCounter(void) { return SleepDeal_GetExternalCommCounter(); }
static UINT8 RTC_IsStopWakeup(void) { return (UINT8)rtc_wake; }
static UINT16 RtcSleep_PortGetCellMinMv(void) { return g_stCellInfoReport.u16VCellMin; }
static UINT16 RtcSleep_PortGetLowVoltageSleepMv(void) { return OtherElement.u16Sleep_Vlow; }
static UINT8 RtcSleep_PortIsOneSecondTick(void) { return (UINT8)one_second; }
static void RtcSleep_PortCommitResetSleep(UINT8 mode) { assert(mode==DEEP_MODE); ++deep_calls; }
static UINT8 RtcSleep_PortUpdateRtcData(void) { return 1; }
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
static void Init_RTC(void) {}
static void IOstatus_RTCMode(void) { adc_on=0; }
static void InitWakeUp_RTCMode(void) {}
static void InitWakeUp_Base(void) {}
static void LowPower_DisableUartWakeup(void) {}
static void LowPower_DisableWakeupExti(void) {}
static void InitTimer(void) { tick=0; }
static void InitUSART_CommonUpper(void) {}
static UINT8 Sci_VerifyWakeFrames(void) {
    if(key_irq_during_verify) { g_irq_t=soc_key; key_irq_during_verify=0; }
    return 0;
}
static UINT8 SystemRuntime_IsDischargeMosOpen(void) { return 1; }
static void RTC_ClearStopWakeup(void) { rtc_wake=0; }
static void RTC_WKTimeConfig(void) {}
static UINT32 RTC_GetCounter(void) { return rtc_counter; }
static void RtcSleep_PortEnterStop(void) { Sys_StopMode(); }
static void initAFE1_IIC(void) {}
static UINT32 RtcSleep_PortGetLastWakeupSeconds(void) { return 1; }
static void RtcSleep_PortApplySocRtcRest(UINT32 seconds) {}
static void RtcSleep_PortDisableStopWakeup(void) {}
static void RtcSleep_PortRestoreAfterStop(void) { ++restore_calls; InitTimer(); adc_on=1; }
static void RtcSleep_PortAddRuntimeSeconds(UINT32 seconds) {}
/* SLEEP_SOURCE */
/* RTC_SOURCE */
static void reset_case(void) {
    memset(&s_sleep,0,sizeof(s_sleep));
    memset((void *)&g_stLowPowerRtcStatus,0,sizeof(g_stLowPowerRtcStatus));
    g_stLowPowerRtcStatus.mode=NO_SLEEP;
    memset(&g_stCellInfoReport,0,sizeof(g_stCellInfoReport));
    g_stCellInfoReport.u16VCellMin=3300;
    AFE_Parameters_RS485_Struction.u16VcellUvp.curValue=2800;
    tick=now_ms=release_at_ms=charge_at_ms=rtc_counter=0;
    key=1; charger=adc_on=stop_calls=restore_calls=deep_calls=scripted_key=one_second=0;
    rtc_wake=comm_busy=can_busy=flash_busy=0; stop_source=soc_key;
    afe_uv=zero_calls=0; afe_status_ok=1; soc_now=55;
    OtherElement.u16Sleep_Vlow=2600; OtherElement.u16Sleep_TimeVlow=1;
    OtherElement.u16Sleep_TimeNormal=10080;
    expected_sleep_flag=0;
    watchdog_feeds=reset_during_stop=key_irq_during_verify=current_wakeup=0;
    BootFlag_Clear();
#if PROJECT_CFG_STORAGE_SELF_TEST_ENABLE
    self_test_active=0U;
#endif
}
int main(void) {
    int i;
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
    /* HICCUP never qualifies an old low level on RTC or unrelated UART. */
    reset_case(); g_stLowPowerRtcStatus.mode=HICCUP_MODE; key=0; stop_source=NO_IRQ;
    for(i=0;i<3;++i) assert(rtc_sleep_run_hiccup_cycle() && now_ms==0 && !adc_on);
    stop_source=uart1_irq;
    assert(rtc_sleep_run_hiccup_cycle() && now_ms<=302 && !adc_on);
    /* A key IRQ arriving during UART qualification also bypasses the hold. */
    key_irq_during_verify=1; now_ms=0;
    assert(!rtc_sleep_run_hiccup_cycle() && now_ms<10 && !adc_on);
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
    assert(g_stLowPowerRtcStatus.mode==NO_SLEEP && g_stLowPowerRtcStatus.cycles==1);
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
    reset_case(); SleepDeal_UpdateOverdischarge(1);
    assert(soc_now==0 && zero_calls==1 && g_stLowPowerRtcStatus.mode==DEEP_MODE);
    SleepDeal_Continue(DEEP_MODE); assert(BootFlag_Read()==FLASH_UNDERVOLTAGE_SLEEP_VALUE);
    memset(&s_sleep,0,sizeof(s_sleep)); LowPower_Request(NO_SLEEP);
    scripted_key=1; key=0; expected_sleep_flag=FLASH_UNDERVOLTAGE_SLEEP_VALUE;
    SleepDeal_HandleBootSleepStartup();
    SleepDeal_UpdateOverdischarge(1);
    assert(zero_calls==1 && g_stLowPowerRtcStatus.mode==NO_SLEEP);
    key=1; g_stCellInfoReport.u16VCellMin=2200;
    g_stCellInfoReport.unMdlFault_Third.bits.b1CellUvp=1;
    for(i=0;i<59;++i) lp_update_sleep_request();
    assert(g_stLowPowerRtcStatus.mode==NO_SLEEP);
    lp_update_sleep_request();
    assert(g_stLowPowerRtcStatus.mode==DEEP_MODE);
    SleepDeal_UpdateOverdischarge(0); SleepDeal_UpdateOverdischarge(1);
    assert(zero_calls==2 && g_stLowPowerRtcStatus.mode==DEEP_MODE);
    for(i=0;i<2;++i) {
        reset_case(); g_stLowPowerRtcStatus.mode=(UINT8)(i ? NORMAL_MODE : HICCUP_MODE);
        key=1; stop_source=NO_IRQ; afe_uv=1;
        assert(!rtc_sleep_run_hiccup_cycle() && deep_calls==1 && zero_calls==1 && !adc_on && !restore_calls);
    }
    for(i=0;i<2;++i) {
        reset_case(); key=0; stop_source=soc_key; afe_uv=1;
        g_stLowPowerRtcStatus.mode=(UINT8)(i ? NORMAL_MODE : HICCUP_MODE);
        assert(!rtc_sleep_run_hiccup_cycle() && deep_calls==1 && zero_calls==1 && !adc_on && !restore_calls && !now_ms);
    }
    reset_case(); key=1; stop_source=NO_IRQ; rtc_wake=1;
    afe_uv=1; afe_status_ok=0; g_stLowPowerRtcStatus.mode=HICCUP_MODE;
    assert(!rtc_sleep_run_hiccup_cycle() && !deep_calls && !zero_calls);
    puts("PASS UV: new event zeros SOC and requests immediate DEEP; same UV after wake does not retrigger; forced low-voltage timer remains active; recovery re-arms, both RTC modes, invalid status");
    /* Both qualified key and charger wakes retain the handled UV state, but
     * neither may disable the configurable low-voltage sleep timer. */
    for(i=0;i<2;++i) {
        int seconds;
        reset_case(); BootFlag_Write(FLASH_UNDERVOLTAGE_SLEEP_VALUE);
        if(i) charger=1;
        else { key=0; scripted_key=1; }
        SleepDeal_HandleBootSleepStartup();
        SleepDeal_UpdateOverdischarge(1);
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
            SleepDeal_UpdateOverdischarge(1);
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
    assert(g_stLowPowerRtcStatus.mode==NORMAL_MODE);
    LowPower_Request(NO_SLEEP); SleepDeal_ResetKeyState(0U);
#if PROJECT_CFG_SWITCH_MODE == PROJECT_SWITCH_PUSHBUTTON
    key=0;
#endif
    assert(!SleepDeal_TakeKeySleepRequest()); tick+=KEY_SLEEP_TICKS;
    assert(SleepDeal_TakeKeySleepRequest()); LowPower_Request(NORMAL_MODE);
    SleepDeal_UpdateOverdischarge(1);
    assert(g_stLowPowerRtcStatus.mode==NORMAL_MODE);
    LowPower_Request(DEEP_MODE); rtc_sleep();
    assert(g_stLowPowerRtcStatus.mode==DEEP_MODE && deep_calls==1 && !zero_calls);
    puts("PASS UV wake: key/charger wakes keep configured voltage/minute delay and second cadence; voltage/charge guards, idle NORMAL, both switches and explicit DEEP requests remain effective");
    reset_case(); BootFlag_Write(FLASH_UNDERVOLTAGE_SLEEP_VALUE); reset_during_stop=1;
    if(setjmp(reset_point)==0) { SleepDeal_HandleBootSleepStartup(); assert(0); }
    assert(BootFlag_Read()==FLASH_UNDERVOLTAGE_SLEEP_VALUE);
    memset(&s_sleep,0,sizeof(s_sleep)); reset_during_stop=0; key=0; scripted_key=1;
    expected_sleep_flag=FLASH_UNDERVOLTAGE_SLEEP_VALUE;
    SleepDeal_HandleBootSleepStartup(); SleepDeal_UpdateOverdischarge(1);
    assert(!zero_calls && g_stLowPowerRtcStatus.mode==NO_SLEEP);
    assert(BootFlag_Read()==BOOT_FLAG_RESET_VALUE);
    reset_case(); BootFlag_Write(FLASH_UNDERVOLTAGE_SLEEP_VALUE); charger=1;
    SleepDeal_HandleBootSleepStartup();
    SleepDeal_UpdateOverdischarge(1);
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
    assert(g_stLowPowerRtcStatus.mode==NORMAL_MODE);
    reset_case(); assert(!SleepDeal_NormalIdleExpired(0)); rtc_counter=604799;
    g_stLowPowerRtcStatus.mode=HICCUP_MODE; stop_source=NO_IRQ; key=1; ctlc=1;
    assert(rtc_sleep_run_hiccup_cycle() && g_stLowPowerRtcStatus.mode==NORMAL_MODE && !ctlc && !adc_on);
    /* Promotion to NORMAL still restores CTLC after a later qualified wake. */
    reset_case(); assert(!SleepDeal_NormalIdleExpired(0)); rtc_counter=604799;
    g_stLowPowerRtcStatus.mode=HICCUP_MODE; stop_source=NO_IRQ; scripted_key=2; ctlc=1;
    rtc_sleep(); assert(ctlc==1 && restore_calls==1 && adc_on);
#if PROJECT_CFG_SWITCH_MODE == PROJECT_SWITCH_PUSHBUTTON
    assert(now_ms==1000); tick=1000; assert(!SleepDeal_TakeKeySleepRequest());
#endif
    puts("PASS idle: exact 7 days across STOP, minute range, RTC wrap, 256-frame wrap, charge/discharge/UART/CAN reset, ignored VNormal, HICCUP promotion without ADC, CTLC restore");
#if PROJECT_CFG_STORAGE_SELF_TEST_ENABLE
    reset_case(); self_test_active=1U; sys_time.time_enter_rtc=1;
    for(i=0;i<20;++i) lp_update_sleep_request();
    rtc_counter=604800; lp_update_sleep_request();
    assert(g_stLowPowerRtcStatus.mode==NO_SLEEP && !g_stLowPowerRtcStatus.idle &&
        (g_stLowPowerRtcStatus.block & LP_BLOCK_STORAGE_TEST));
    self_test_active=0U; lp_update_sleep_request(); assert(g_stLowPowerRtcStatus.mode==HICCUP_MODE);
    reset_case(); self_test_active=1U; key=0;
#if PROJECT_CFG_SWITCH_MODE == PROJECT_SWITCH_MAINTAINED
    key=1;
#endif
    assert(!SleepDeal_TakeKeySleepRequest()); tick=KEY_SLEEP_TICKS; scripted_key=1; rtc_sleep();
    assert(stop_calls==2 && restore_calls==1 && g_stLowPowerRtcStatus.mode==NO_SLEEP);
    reset_case(); self_test_active=1U; SleepDeal_UpdateOverdischarge(1U); rtc_sleep();
    assert(deep_calls==1 && g_stLowPowerRtcStatus.mode==DEEP_MODE);
    reset_case(); self_test_active=1U; g_stCellInfoReport.u16VCellMin=2200;
    for(i=0;i<60;++i) lp_update_sleep_request();
    assert(g_stLowPowerRtcStatus.mode==DEEP_MODE);
    puts("PASS self-test sleep: automatic HICCUP/7-day idle held; completion releases; both key modes and confirmed/timed DEEP retain priority");
#endif
    return 0;
}
