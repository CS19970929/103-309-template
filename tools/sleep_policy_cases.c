#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
typedef uint8_t UINT8; typedef uint16_t UINT16; typedef uint32_t UINT32;
typedef int32_t INT32;
typedef int FLASH_Status;
#include "Flash.h"
#include "SleepDeal.h"
#include "rtc_sleep.h"
#define LIFEPO
#define Feed_IWatchDog ((void)0)
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
static UINT8 comm_counter,u8FlashUpdateFlag;
static enum irqWakeup stop_source;
static int scripted_key;
#define MCUI_ENI_DI1 key
#define MCUO_DEBUG_LED1 led
#define MCUO_AFE_CTLC ctlc
static struct { UINT16 time_enter_rtc; UINT32 rtc_sec_cnt; } sys_time={5,0};
static struct { UINT16 u16Sleep_Vlow,u16Sleep_TimeVlow,u16Sleep_TimeNormal; } OtherElement={2600,1,10080};
static struct {
    UINT16 u16VCellMin,u16VCell[32],u16Ichg,u16IDischg;
    union { UINT16 all; struct { UINT16 b1CellUvp:1; } bits; } unMdlFault_Third,unMdlFault_Second;
} g_stCellInfoReport;
static struct { struct { struct { UINT8 SC,UV; } bits; } REG_BSTATUS1; } SH367309_Reg_Store;
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
    ++stop_calls;
    if(scripted_key) key=(stop_calls==1) ? 1 : 0;
    g_irq_t=stop_source; ++rtc_counter;
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
static UINT8 RtcSleep_PortHasCurrentWake(enum irqWakeup *source) { return 0; }
static UINT8 RtcSleep_PortHasAfeWake(enum irqWakeup *source) { return 0; }
static void Init_RTC(void) {}
static void IOstatus_RTCMode(void) { adc_on=0; }
static void InitWakeUp_RTCMode(void) {}
static void InitWakeUp_Base(void) {}
static void LowPower_DisableUartWakeup(void) {}
static void InitTimer(void) {}
static void InitUSART_CommonUpper(void) {}
static UINT8 Sci_VerifyWakeFrames(void) { return 0; }
static UINT8 SystemRuntime_IsDischargeMosOpen(void) { return 1; }
static void RTC_ClearStopWakeup(void) { rtc_wake=0; }
static void RTC_WKTimeConfig(void) {}
static UINT32 RTC_GetCounter(void) { return rtc_counter; }
static void RtcSleep_PortEnterStop(void) { Sys_StopMode(); }
static void initAFE1_IIC(void) {}
static UINT32 RtcSleep_PortGetLastWakeupSeconds(void) { return 1; }
static void RtcSleep_PortApplySocRtcRest(UINT32 seconds) {}
static void RtcSleep_PortDisableStopWakeup(void) {}
static void RtcSleep_PortRestoreAfterStop(void) { ++restore_calls; adc_on=1; }
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
    BootFlag_Clear();
}
int main(void) {
    int i;
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
    /* Both STOP modes reject a short key candidate and qualify the full 1 s. */
    for(i=0;i<2;++i) {
        reset_case(); g_stLowPowerRtcStatus.mode=(UINT8)(i ? NORMAL_MODE : HICCUP_MODE);
        key=0; release_at_ms=990; assert(rtc_sleep_run_hiccup_cycle() && !adc_on);
        release_at_ms=0; key=0; now_ms=0;
        assert(!rtc_sleep_run_hiccup_cycle() && now_ms==1000 && !adc_on);
    }
    reset_case(); key=0; assert(!SleepDeal_TakeKeySleepRequest()); tick=300;
    scripted_key=1; rtc_sleep();
    assert(stop_calls==2 && restore_calls==1 && g_stLowPowerRtcStatus.mode==NO_SLEEP);
    tick+=1000; assert(!SleepDeal_TakeKeySleepRequest());
    /* The resetting DEEP path shares the same release and 1 s qualification. */
    reset_case(); BootFlag_Write(FLASH_DEEP_SLEEP_VALUE); key=0; scripted_key=1;
    SleepDeal_HandleBootSleepStartup(); assert(stop_calls==2 && now_ms==1000);
    tick=1000; assert(!SleepDeal_TakeKeySleepRequest());
    reset_case(); charger=1; assert(SleepDeal_IsWakeupValid() && now_ms==0);
    assert(BootFlag_Read()==FLASH_SLEEP_CHARGER_WAKE_VALUE);
    puts("PASS key: 3 s RUN hold, 1 s wake in all modes, release/re-press guard, no queued-tick acceleration, wrap, immediate sleep request and charger wake");
    return 0;
}
