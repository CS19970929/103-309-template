"""Run production sleep/RTC/Flash functions against fault-injecting hardware stubs.

Run: python -B tests/test_sleep.py (Windows Visual Studio C++ tools).
This is software regression evidence; it does not measure board power or prove wake IO.
"""

import os
from pathlib import Path
import re
import shutil
import subprocess
import tempfile

from test_current_calibration import function_source, msvc_environment

ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / "103 + 309/Project/Source"


def read(name):
    return (SOURCE / name).read_bytes().decode("latin1").replace("\r\n", "\n")


def clean(text):
    return re.sub(r"/\*.*?\*/|//[^\n]*", "", text, flags=re.DOTALL)


COMMON = r'''
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
typedef uint8_t UINT8;
typedef uint16_t UINT16;
typedef uint32_t UINT32;
#define RESET 0
#define ENABLE 1
#define DISABLE 0
#define Feed_IWatchDog ((void)0)
#define log_i(...) ((void)0)
#define log_w(...) ((void)0)
#define log_e(...) ((void)0)
#define log_a(...) ((void)0)
#define EXTI_Line0 (1u << 0)
#define EXTI_Line3 (1u << 3)
#define EXTI_Line7 (1u << 7)
#define EXTI_Line9 (1u << 9)
#define EXTI_Line12 (1u << 12)
#define EXTI_Line17 (1u << 17)
static struct { UINT32 IMR; } exti;
#define EXTI (&exti)
static void EXTI_ClearITPendingBit(UINT32 line) { (void)line; }
'''

POLICY_STUBS = r'''
void entersleep(enum _SLEEP_MODE mode);
#define _COMMOM_UPPER_SCI1
#define _COMMOM_UPPER_SCI2
#define SNum 16
#define AFE_TYPE 1
#define sh36xx 1
#define GPIOA 0
#define GPIOB 1
#define GPIO_INT_WK_MCU GPIOA
#define GPIO_INT_WK_CMNT GPIOB
#define GPIO_SCI1_RX GPIOB
#define GPIO_SCI2_RX GPIOA
#define GPIO_KEY1 0
#define GPIO_Pin_0 1
#define PIN_KEY1 512
#define PIN_INT_WK_MCU GPIO_Pin_0
#define PIN_INT_WK_CMNT 4096
#define PIN_SCI1_RX 128
#define PIN_SCI2_RX 8
#define USART1 1
#define USART2 2
#define USART_FLAG_TC 1
#define EXTI0_IRQn 0
#define EXTI3_IRQn 1
#define EXTI9_5_IRQn 2
#define EXTI15_10_IRQn 3
#define RS485_STA_IDLE 0
#define _DI_SWITCH_longKEY_ONOFF
#define MCUI_ENI_DI1 key_released
#define MTP_BALANCEH 0x26
#define MTP_BSTATUS1 0x28
#define FLASH_ADDR_SLEEP_FLAG 0x0801fc00
#define FLASH_NORMAL_SLEEP_VALUE 0x5555
#define FLASH_DEEP_SLEEP_VALUE 0xaaaa
#define FLASH_HICCUP_SLEEP_VALUE 0x7777
#define FLASH_SLEEP_RESET_VALUE 0xffff
#define FLASH_FLAG_EOP 1
#define FLASH_FLAG_PGERR 2
#define FLASH_FLAG_WRPRTERR 4
#define BMS_SLEEP 1
typedef enum { FLASH_COMPLETE, FLASH_ERROR_PG, FLASH_ERROR_WRP } FLASH_Status;
static struct { UINT16 u16Ichg, u16IDischg, u16VCellMin, u16VCell[16], u16BalanceFlag1; } g_stCellInfoReport;
static struct { UINT16 u16Sys_SeriesNum, u16Sleep_Vlow, u16Sleep_TimeVlow; } OtherElement;
static struct { UINT16 u16VcellUvp_Third, u16VcellOvp_Third; } PRT_E2ROMParas;
static struct { struct { UINT8 b1Status_Heat, b1Status_MOS_CHG, b1Status_MOS_DSG; } bits; } SystemStatus;
static struct { UINT8 all; } empty_status;
static struct { UINT8 u8_MTP_BALANCEH, u8_MTP_BALANCEL;
    struct { UINT8 all; } REG_BSTATUS1, REG_BSTATUS2;
    struct { struct { UINT8 CHG_FET, DSG_FET, L0V, PCHG_FET; } bits; } REG_BSTATUS3;
} SH367309_Reg_Store;
static struct { UINT32 time_enter_rtc; } sys_time;
static struct { UINT8 ptr_no, csr; } g_stCurrentMsgPtr_SCI1, g_stCurrentMsgPtr_SCI2;
static UINT8 gu8_TxEnable_SCI1, gu8_TxEnable_SCI2;
static volatile UINT8 gu8_1000msAccClock_Flag;
static UINT8 u8FlashUpdateFlag, u8FlashUpdateE2PROM, u8E2P_SocTable_WriteFlag;
static UINT8 u8E2P_CopperLoss_WriteFlag, u8E2P_KB_WriteFlag, gu8_Reset_EventRecord;
static UINT32 u32E2P_Pro_VolCur_WriteFlag, u32E2P_Pro_Temp_WriteFlag, u32E2P_Pro_Other_WriteFlag;
static UINT32 u32E2P_OtherElement1_WriteFlag, u32E2P_HeatCool_WriteFlag, u32E2P_RTC_Element_WriteFlag;
static struct { struct { UINT8 Log_Sleep; } bits; } LogRecord_Flag;
UINT32 su32_Interval_S_Tcnt;
volatile bool is_rtc_wakekup;
static UINT8 mtp_ok, balance, status_fault, fet_chg, fet_dsg, update_failed, charger, comm_wake, key_released;
static UINT8 serial_rx_high;
static UINT8 tc, rtc_ok, arm_ok, afe_ok, restore_calls, stop_calls, reset_calls;
static UINT8 rtc_alarm_pending;
static UINT32 primask_state;
static UINT8 arm_edge, stop_fault, stop_external, stop_spurious;
static UINT8 log_calls, erase_calls, program_calls, locked;
static UINT8 eeprom_failure, eeprom_calls;
static UINT32 rtc_counter, stop_elapsed;
static UINT16 flash_value;
static FLASH_Status erase_result, program_result;
static UINT8 corrupt_program;
static UINT16 FlashReadOneHalfWord(UINT32 a) { (void)a; return flash_value; }
static void FLASH_Unlock(void) { locked=0; }
static void FLASH_Lock(void) { locked=1; }
static void FLASH_ClearFlag(UINT32 f) { (void)f; }
static FLASH_Status FLASH_ErasePage(UINT32 a) {
    (void)a; ++erase_calls; if (erase_result==FLASH_COMPLETE) flash_value=0xffff; return erase_result;
}
static FLASH_Status FLASH_ProgramHalfWord(UINT32 a, UINT16 v) {
    (void)a; ++program_calls; if (program_result==FLASH_COMPLETE && !corrupt_program) flash_value=v; return program_result;
}
static UINT8 MTPRead(UINT8 address, UINT8 count, UINT8 *dest) {
    if (!mtp_ok) return 0;
    if (address==MTP_BALANCEH && count==2) { dest[0]=0; dest[1]=balance; }
    else {
        SH367309_Reg_Store.u8_MTP_BALANCEH=0;
        SH367309_Reg_Store.u8_MTP_BALANCEL=balance;
        SH367309_Reg_Store.REG_BSTATUS1.all=status_fault;
        SH367309_Reg_Store.REG_BSTATUS3.bits.CHG_FET=fet_chg;
        SH367309_Reg_Store.REG_BSTATUS3.bits.DSG_FET=fet_dsg;
    }
    return 1;
}
static void CellBalance_UpdateReport(UINT16 mask) { g_stCellInfoReport.u16BalanceFlag1=mask; }
static UINT8 GPIO_ReadInputDataBit(int port, int pin) {
    (void)port;
    if (pin==PIN_KEY1) return key_released;
    if (pin==PIN_SCI1_RX || pin==PIN_SCI2_RX) return serial_rx_high;
    if (pin==PIN_INT_WK_CMNT) return comm_wake;
    return charger;
}
static UINT8 USART_GetFlagStatus(int port, int flag) { (void)port; (void)flag; return tc; }
static void NVIC_ClearPendingIRQ(int irq) { (void)irq; }
static UINT8 Sci_CommonUpperHasPendingWork(void) {
    return gu8_TxEnable_SCI1 || gu8_TxEnable_SCI2 ||
        g_stCurrentMsgPtr_SCI1.ptr_no || g_stCurrentMsgPtr_SCI2.ptr_no ||
        g_stCurrentMsgPtr_SCI1.csr != RS485_STA_IDLE ||
        g_stCurrentMsgPtr_SCI2.csr != RS485_STA_IDLE || !tc;
}
static UINT8 Sci_CommonUpperPrepareStop(void) { return !Sci_CommonUpperHasPendingWork(); }
static UINT32 __get_PRIMASK(void) { return primask_state; }
static void __disable_irq(void) { primask_state=1; }
static void __set_PRIMASK(UINT32 value) { primask_state=value; }
static void __ISB(void) {}
static void DataLoad_Current(void) {}
static void DataLoad_CellVolt(void) {}
static void DataLoad_CellVoltMaxMinFind(void) {}
static void DataLoad_Temperature(void) {}
static void DataLoad_TemperatureMaxMinFind(void) {}
static UINT8 UpdateVoltageFromBqMaximo(void) { return update_failed; }
static UINT8 Init_RTC(void) { return rtc_ok; }
static UINT8 RTC_WKTimeConfig(void);
static UINT8 RTC_SynchronizeAfterStop(void) { return rtc_ok; }
static void RTC_LatchAlarmWake(void) {
    if (rtc_alarm_pending) { rtc_alarm_pending=0; is_rtc_wakekup=true; }
}
static UINT32 RTC_ReadCounterCoherent(void) { return rtc_counter; }
static void RTC_DisableStopAlarm(void) { is_rtc_wakekup=false; }
static void IOstatus_RTCMode(void) {}
static void InitWakeUp_RTCMode(void) {}
static void IOstatus_NormalMode(void) {}
static void InitWakeUp_NormalMode(void) {}
static void IOstatus_DeepMode(void) {}
static void InitWakeUp_DeepMode(void) {}
static void InitRunAfterStop(void) { ++restore_calls; }
static void initAFE1_IIC(void) {}
static void InitAFE1_Sleep(UINT8 mode) { (void)mode; }
static UINT8 AFE_Sleep(void) { return afe_ok; }
static void MCU_RESET(void) { ++reset_calls; }
static void LogEvent_Record(UINT8 v, UINT8 e, UINT32 *s) { (void)v; (void)e; (void)s; ++log_calls; }
#define E2P_ADDR_SOC 10
#define E2P_ADDR_DSG_SOC_Int 12
#define E2P_ADDR_CYCLE_TIMES 14
static struct { UINT8 u8SOC_Now, u8DSG_SOC_Int; UINT32 u32Cycle_times; }
    SOC_Calculate_Element, SOC_Calculate_Element_backup;
static UINT8 TakePendingFlag(volatile UINT8 *flag) {
    UINT8 pending = *flag;
    *flag = 0U;
    return pending;
}
static UINT16 saved_words[3];
static UINT8 WriteEEPROM_Word_NoZone(UINT16 a, UINT16 v) {
    ++eeprom_calls;
    if (eeprom_failure == eeprom_calls) return 1;
    saved_words[(a-10)/2]=v; return 0;
}
UINT8 SOC_SaveBeforeSleep(void);
UINT8 SleepDeal_CommitResetSleep(UINT8 mode);
static void Sys_StopMode(void);
'''

POLICY_TESTS = r'''
static UINT8 RTC_WKTimeConfig(void) { return arm_ok; }
static void Sys_StopMode(void) { ++stop_calls; }
uint32_t BoardLowPower_Lock(void) { return __get_PRIMASK(); }
void BoardLowPower_Unlock(uint32_t value) { __set_PRIMASK(value); }
void BoardLowPower_Init(void) {}
uint32_t BoardLowPower_NowMs(void) { return 0U; }
uint32_t BoardLowPower_RtcCounter(void) { return rtc_counter; }
void BoardLowPower_Snapshot(void) {}
uint32_t BoardLowPower_ActiveWake(void) {
    return (charger?LP_WAKE_PA0:0U)|(comm_wake?LP_WAKE_COMM:0U)|(!key_released?LP_WAKE_KEY:0U);
}
uint8_t BoardLowPower_Begin(void) { return 1U; }
uint8_t BoardLowPower_WaitQuiet(void) { return 1U; }
void BoardLowPower_ArmWake(void) {}
void BoardLowPower_PrepareNextStop(void) {}
void BoardLowPower_Stop(void) { ++stop_calls; LowPower_NotifyWake(LP_WAKE_RTC); }
uint8_t BoardLowPower_RtcInit(void) { return rtc_ok; }
uint8_t BoardLowPower_RtcArm(void) { return arm_ok; }
uint8_t BoardLowPower_CaptureWake(void) { return 1U; }
void BoardLowPower_BeginSample(void) {}
void BoardLowPower_Restore(void) { ++restore_calls; }
void BoardLowPower_AccountSeconds(uint32_t seconds) { su32_Interval_S_Tcnt+=seconds; }
static void defaults(void) {
    s_sleep_mode=NO_SLEEP; s_low_seconds=s_force_seconds=s_idle_seconds=s_last_comm=s_started=0;
    RTC_ExtComCnt=0; g_irq_t=NO_IRQ; is_rtc_wakekup=false; rtc_alarm_pending=0;
    mtp_ok=fet_chg=fet_dsg=key_released=tc=rtc_ok=arm_ok=afe_ok=serial_rx_high=1;
    charger=comm_wake=balance=status_fault=update_failed=arm_edge=stop_fault=stop_external=stop_spurious=0;
    restore_calls=stop_calls=reset_calls=erase_calls=program_calls=log_calls=0;
    primask_state=0;
    u8FlashUpdateFlag=u8FlashUpdateE2PROM=gu8_TxEnable_SCI1=gu8_TxEnable_SCI2=0;
    gu8_Reset_EventRecord=0; u32E2P_OtherElement1_WriteFlag=0;
    memset(&g_stCurrentMsgPtr_SCI1,0,sizeof(g_stCurrentMsgPtr_SCI1));
    memset(&g_stCurrentMsgPtr_SCI2,0,sizeof(g_stCurrentMsgPtr_SCI2));
    memset(&SH367309_Reg_Store,0,sizeof(SH367309_Reg_Store));
    memset(&g_stCellInfoReport,0,sizeof(g_stCellInfoReport));
    memset(&SystemStatus,0,sizeof(SystemStatus));
    OtherElement.u16Sys_SeriesNum=1; OtherElement.u16Sleep_Vlow=2800; OtherElement.u16Sleep_TimeVlow=2;
    g_stCellInfoReport.u16VCellMin=g_stCellInfoReport.u16VCell[0]=3300;
    PRT_E2ROMParas.u16VcellUvp_Third=2500; PRT_E2ROMParas.u16VcellOvp_Third=3650;
    sys_time.time_enter_rtc=30; rtc_counter=100; stop_elapsed=10; su32_Interval_S_Tcnt=0;
    flash_value=0xffff; erase_result=program_result=FLASH_COMPLETE; corrupt_program=0;
    eeprom_calls=eeprom_failure=0;
    SOC_Calculate_Element.u8SOC_Now=42; SOC_Calculate_Element.u8DSG_SOC_Int=17;
    SOC_Calculate_Element.u32Cycle_times=12345;
}

static void tick(void) { gu8_1000msAccClock_Flag=1; (void)LowPower_Poll(); }
static void ticks(int n) { while(n--) tick(); }
int main(void) {
    defaults(); LowPower_Init();
    rtc_sleep_record_wakeup(uart1_irq); rtc_sleep_record_wakeup(uart2_irq);
    assert(g_irq_t==uart1_irq);
    defaults(); ticks(30); assert(s_sleep_mode==NO_SLEEP);
    tick(); assert(s_sleep_mode==HICCUP_MODE && g_low_power_diag.state==LP_PREPARE);
    g_low_power_diag.state=LP_RUN;
    defaults(); g_stCellInfoReport.u16VCellMin=2600; ticks(59); assert(reset_calls==0);
    tick(); assert(reset_calls==1 && flash_value==FLASH_DEEP_SLEEP_VALUE);
    assert(saved_words[0]==42 && saved_words[1]==17 && saved_words[2]==123);
    defaults(); g_stCellInfoReport.u16VCellMin=2800; ticks(119); assert(reset_calls==0);
    tick(); assert(reset_calls==1);
    defaults(); g_stCellInfoReport.u16VCellMin=2600; g_stCellInfoReport.u16Ichg=1;
    ticks(59); assert(reset_calls==0);
    defaults(); entersleep(NORMAL_MODE); entersleep(HICCUP_MODE);
    gu8_TxEnable_SCI1=1; tick(); assert(reset_calls==0);
    gu8_TxEnable_SCI1=0; g_stCurrentMsgPtr_SCI2.ptr_no=1; tick(); assert(reset_calls==0);
    g_stCurrentMsgPtr_SCI2.ptr_no=0; tc=0; tick(); assert(reset_calls==0);
    tc=1; u32E2P_OtherElement1_WriteFlag=1; tick(); assert(reset_calls==0);
    u32E2P_OtherElement1_WriteFlag=0; tick(); assert(reset_calls==1);
    defaults(); entersleep(NORMAL_MODE); entersleep(DEEP_MODE); entersleep(NORMAL_MODE);
    tick(); assert(flash_value==FLASH_DEEP_SLEEP_VALUE);
    defaults(); for(int i=0;i<35;i++) { RTC_ExtComCnt +=256; tick(); }
    assert(stop_calls==0 && s_idle_seconds==0);
    defaults(); balance=1; ticks(35); assert(g_low_power_diag.block_mask&LP_BLOCK_BALANCE);
    defaults(); SystemStatus.bits.b1Status_Heat=1; ticks(35);
    assert(g_low_power_diag.block_mask&LP_BLOCK_HEAT);
    defaults(); mtp_ok=0; ticks(35); assert(g_low_power_diag.block_mask&LP_BLOCK_AFE);
    defaults(); fet_chg=0; assert(BmsSleep_SampleExit()&LP_EXIT_MOS);
    defaults(); mtp_ok=0; assert(BmsSleep_SampleExit()&LP_EXIT_AFE);
    defaults(); update_failed=1; assert(BmsSleep_SampleExit()&LP_EXIT_AFE);
    defaults(); g_stCellInfoReport.u16Ichg=1; assert(BmsSleep_SampleExit()&LP_EXIT_CURRENT);
    defaults(); OtherElement.u16Sys_SeriesNum=17; assert(BmsSleep_SampleExit()&LP_EXIT_CONFIG);
    defaults(); g_stCellInfoReport.u16VCell[0]=2400; assert(BmsSleep_SampleExit()&LP_EXIT_UV);
    defaults(); g_stCellInfoReport.u16VCell[0]=3700; assert(BmsSleep_SampleExit()&LP_EXIT_OV);
    defaults(); erase_result=FLASH_ERROR_WRP; assert(FlashWriteOneHalfWord(1,2)==FLASH_ERROR_WRP);
    assert(erase_calls==1 && program_calls==0 && locked);
    defaults(); program_result=FLASH_ERROR_PG; assert(FlashWriteOneHalfWord(1,2)==FLASH_ERROR_PG && locked);
    defaults(); corrupt_program=1; assert(FlashWriteOneHalfWord(1,2)==FLASH_ERROR_PG && locked);
    defaults(); eeprom_failure=2; entersleep(DEEP_MODE); tick(); assert(reset_calls==0 && erase_calls==0);
    defaults(); afe_ok=0; entersleep(DEEP_MODE); tick();
    assert(reset_calls==0 && log_calls==0 && flash_value==FLASH_SLEEP_RESET_VALUE && s_sleep_mode==NO_SLEEP);
    defaults(); flash_value=FLASH_NORMAL_SLEEP_VALUE; IsSleepStartUp();
    assert(flash_value==FLASH_SLEEP_RESET_VALUE && erase_calls==1 && reset_calls==1);
    defaults(); flash_value=FLASH_HICCUP_SLEEP_VALUE; IsSleepStartUp();
    assert(s_sleep_mode==HICCUP_MODE && !stop_calls);
    defaults(); flash_value=FLASH_DEEP_SLEEP_VALUE; erase_result=FLASH_ERROR_WRP;
    IsSleepStartUp(); assert(stop_calls==0 && reset_calls==0);
    defaults(); gu8_Reset_EventRecord=1; entersleep(DEEP_MODE); tick(); assert(reset_calls==0);
    defaults(); key_released=0;
    for(int i=0;i<14;i++) App_DI1_Switch();
    assert(s_sleep_mode==NO_SLEEP); App_DI1_Switch(); assert(s_sleep_mode==DEEP_MODE);
    puts("PASS: idle/low-voltage qualification, blockers, RTC policy, reset-sleep transactions, key debounce");
    return 0;
}
'''
RTC_STUBS = r'''
#define LP_WAKE_RTC 1U
static void LowPower_NotifyWake(uint32_t value) { (void)value; }
#define RTC_FLAG_RTOFF 0x20
#define RTC_FLAG_RSF 8
#define RTC_IT_ALR 2
#define RTC_IT_SEC 1
#define RCC_FLAG_LSERDY 1
#define RCC_FLAG_LSIRDY 2
#define RCC_LSE_ON 1
#define RCC_APB1Periph_PWR 1
#define RCC_APB1Periph_BKP 2
#define RCC_BDCR_RTCSEL 0x300
#define RCC_BDCR_RTCEN 0x8000
#define RCC_BDCR_RTCSEL_LSE 0x100
#define RCC_BDCR_RTCSEL_LSI 0x200
#define RCC_RTCCLKSource_LSE 0x100
#define RCC_RTCCLKSource_LSI 0x200
#define BKP_DR1 1
#define RTC_BKP_DATA 0xa5a5
#define LSE_FREQUENT 32767
#define TIME_ZOOM 28800
#define ALARM_TIME_SEC 10
#define RTCAlarm_IRQn 1
#define RTC_IRQn 2
#define EXTI_Mode_Interrupt 0
#define EXTI_Trigger_Rising 0
typedef struct { UINT32 EXTI_Line, EXTI_Mode, EXTI_Trigger, EXTI_LineCmd; } EXTI_InitTypeDef;
typedef struct { UINT8 NVIC_IRQChannel, NVIC_IRQChannelPreemptionPriority, NVIC_IRQChannelSubPriority, NVIC_IRQChannelCmd; } NVIC_InitTypeDef;
static struct { UINT16 CRL, CNTH, CNTL; } rtc_regs;
static struct { UINT32 BDCR; } rcc_regs;
#define RTC (&rtc_regs)
#define RCC (&rcc_regs)
static UINT8 lse_ok, lsi_ok, sync_ok, write_ok;
static UINT32 lse_start_calls, lsi_start_calls;
static UINT32 counter, alarm, prescaler, resets, counter_writes, polls, rtc_pending, exti_pending;
static UINT16 backup;
static UINT8 TimeDisplay;
static struct RTC_ELEMENT { int value; } Systmtime;
static UINT8 RCC_GetFlagStatus(UINT8 f) { ++polls; return f==RCC_FLAG_LSERDY? lse_ok : lsi_ok; }
static UINT8 RTC_GetFlagStatus(UINT16 f) {
    ++polls;
    rtc_regs.CNTH=(UINT16)(counter>>16); rtc_regs.CNTL=(UINT16)counter;
    return f==RTC_FLAG_RSF? sync_ok : write_ok;
}
static void RCC_LSEConfig(int v) { (void)v; ++lse_start_calls; }
static void RCC_LSICmd(int v) { (void)v; ++lsi_start_calls; }
static void RCC_APB1PeriphClockCmd(int p,int v) { (void)p; (void)v; }
static void PWR_BackupAccessCmd(int v) { (void)v; }
static void EXTI_Init(EXTI_InitTypeDef *p) { (void)p; }
static void NVIC_Init(NVIC_InitTypeDef *p) { (void)p; }
static void NVIC_ClearPendingIRQ(int n) { (void)n; }
static UINT16 BKP_ReadBackupRegister(int n) { (void)n; return backup; }
static void BKP_WriteBackupRegister(int n,UINT16 v) { (void)n; backup=v; }
static void BKP_DeInit(void) { ++resets; RCC->BDCR=0; backup=0; }
static void RCC_RTCCLKConfig(UINT32 v) { RCC->BDCR=(RCC->BDCR&~RCC_BDCR_RTCSEL)|v; }
static void RCC_RTCCLKCmd(int v) { if (v) RCC->BDCR|=RCC_BDCR_RTCEN; }
static void RTC_SetPrescaler(UINT32 v) { prescaler=v; }
static void RTC_SetCounter(UINT32 v) {
    ++counter_writes; counter=v;
    rtc_regs.CNTH=(UINT16)(v>>16); rtc_regs.CNTL=(UINT16)v;
}
static UINT32 Seccond_Cal(struct RTC_ELEMENT *p) { p->value=99; return 1546315200; }
static void RTC_ITConfig(int f,int v) { (void)f; (void)v; }
static void RTC_ClearITPendingBit(int f) { rtc_pending&=~f; }
static void RTC_SetAlarm(UINT32 v) { alarm=v; }
static UINT8 RTC_GetITStatus(int f) { return !!(rtc_pending & f); }
static UINT8 EXTI_GetITStatus(UINT32 f) { return !!(exti_pending & f); }
static void Get_RTC_Time(void) {}
'''

RTC_TESTS = r'''
int main(void) {
    const UINT32 expected_source = (BMS_RTC_CLOCK_SOURCE == BMS_RTC_CLOCK_LSE)
        ? RCC_BDCR_RTCSEL_LSE : RCC_BDCR_RTCSEL_LSI;
    const UINT32 other_source = (BMS_RTC_CLOCK_SOURCE == BMS_RTC_CLOCK_LSE)
        ? RCC_BDCR_RTCSEL_LSI : RCC_BDCR_RTCSEL_LSE;
    UINT32 previous_opposite_calls;

    lse_ok=lsi_ok=sync_ok=write_ok=1;
    assert(Init_RTC());
    assert(resets==1 && counter_writes==1);
    assert(prescaler==(BMS_RTC_CLOCK_SOURCE == BMS_RTC_CLOCK_LSE ? 32767U : 39999U));
    assert((RCC->BDCR & RCC_BDCR_RTCSEL)==expected_source);
    assert(Systmtime.value==0);
    if (BMS_RTC_CLOCK_SOURCE == BMS_RTC_CLOCK_LSE)
        assert(lse_start_calls==1 && lsi_start_calls==0);
    else
        assert(lsi_start_calls==1 && lse_start_calls==0);

    counter=1234;
    assert(Init_RTC()); assert(resets==1 && counter_writes==1 && counter==1234);
    assert(RTC_WKTimeConfig() && alarm==1234u+ALARM_TIME_SEC);
    counter=UINT32_MAX-4;
    assert(RTC_WKTimeConfig() && alarm==(UINT32)(counter+ALARM_TIME_SEC));

    /* Firmware changed clock selection while RTC backup values survived. */
    backup=RTC_BKP_DATA;
    RCC->BDCR=(RCC->BDCR & ~RCC_BDCR_RTCSEL)|other_source;
    counter=6789;
    assert(Init_RTC());
    assert(resets==2 && counter_writes==2 && counter==6789);
    assert((RCC->BDCR & RCC_BDCR_RTCSEL)==expected_source);

    /* Cold power-up selects only the configured source. */
    backup=0; lse_ok=lsi_ok=1;
    previous_opposite_calls=(BMS_RTC_CLOCK_SOURCE == BMS_RTC_CLOCK_LSE)
        ? lsi_start_calls : lse_start_calls;
    assert(Init_RTC()); assert(resets==3 && counter_writes==3);
    assert((BMS_RTC_CLOCK_SOURCE == BMS_RTC_CLOCK_LSE)
        ? lsi_start_calls==previous_opposite_calls
        : lse_start_calls==previous_opposite_calls);

    polls=0; write_ok=0;
    assert(!RTC_WKTimeConfig() && polls==RTC_WAIT_POLL_LIMIT);
    write_ok=1; sync_ok=0; polls=0;
    assert(!Init_RTC() && polls<=RTC_WAIT_POLL_LIMIT+1);
    sync_ok=1;
    if (BMS_RTC_CLOCK_SOURCE == BMS_RTC_CLOCK_LSE) lse_ok=0;
    else lsi_ok=0;
    polls=0; assert(!Init_RTC() && polls==RTC_WAIT_POLL_LIMIT);
    backup=0; polls=0;
    previous_opposite_calls=(BMS_RTC_CLOCK_SOURCE == BMS_RTC_CLOCK_LSE)
        ? lsi_start_calls : lse_start_calls;
    assert(!Init_RTC() && polls==RTC_WAIT_POLL_LIMIT);
    assert((BMS_RTC_CLOCK_SOURCE == BMS_RTC_CLOCK_LSE)
        ? lsi_start_calls==previous_opposite_calls
        : lse_start_calls==previous_opposite_calls);

    rtc_pending=RTC_IT_ALR; exti_pending=0; polls=0;
    RTCAlarm_IRQHandler(); assert(is_rtc_wakekup && rtc_cnt==1 && polls==0);
    RTCAlarm_IRQHandler(); assert(rtc_cnt==1);
    printf("PASS: RTC source %s, cold start, retained and migration, alarm, failures\n",
        BMS_RTC_CLOCK_SOURCE == BMS_RTC_CLOCK_LSE ? "LSE" : "LSI");
    return 0;
}
'''

STOP_STUBS = r'''
#define LP_FAULT_CLOCK 16U
static void LowPower_RecordFault(uint32_t value) { (void)value; }
#define RCC_CFGR_SWS_HSE 4
#define RCC_SYSCLKSource_HSE 1
#define RCC_HSE_ON 1
#define ERROR 0
#define RCC_APB1Periph_PWR 1
#define TIM3 3
#define TIM3_IRQn 3
#define TIM_IT_Update 1
#define PWR_Regulator_LowPower 1
#define PWR_STOPEntry_WFI 1
static UINT32 primask, stops, resets, updates, delay_inits, clock_polls;
static UINT8 clock_source=4, hse_ok=1, switch_ok=1;
volatile bool is_rtc_wakekup;
volatile enum irqWakeup g_irq_t=NO_IRQ;
static void RCC_APB1PeriphClockCmd(int p,int v) { (void)p; (void)v; }
static void TIM_Cmd(int t,int v) { (void)t; (void)v; }
static void TIM_ClearITPendingBit(int t,int v) { (void)t; (void)v; }
static void NVIC_ClearPendingIRQ(int n) { (void)n; }
static UINT32 __get_PRIMASK(void) { return primask; }
static void __disable_irq(void) { primask=1; }
static void __set_PRIMASK(UINT32 v) { primask=v; }
static void __DSB(void) {}
static void PWR_EnterSTOPMode(int r,int e) { (void)r; (void)e; assert(primask==1); ++stops; clock_source=0; }
static UINT8 RCC_GetSYSCLKSource(void) { ++clock_polls; return clock_source; }
static void RCC_HSEConfig(int s) { (void)s; }
static UINT8 RCC_WaitForHSEStartUp(void) { return hse_ok; }
static void RCC_SYSCLKConfig(int s) { (void)s; if (switch_ok) clock_source=4; }
static void SystemCoreClockUpdate(void) { assert(primask==1); ++updates; }
static void InitDelay(void) { assert(primask==1); ++delay_inits; }
static void MCU_RESET(void) { ++resets; }
'''

STOP_TESTS = r'''
int main(void) {
    Sys_StopMode(); assert(stops==1 && clock_source==4 && !primask && updates==1 && delay_inits==1);
    g_irq_t=bms_keyirq; Sys_StopMode(); assert(stops==1 && !primask);
    g_irq_t=NO_IRQ; is_rtc_wakekup=true; Sys_StopMode(); assert(stops==1 && !primask);
    is_rtc_wakekup=false; hse_ok=0; Sys_StopMode(); assert(stops==2 && resets==1);
    clock_source=4; hse_ok=1; switch_ok=0; clock_polls=0; Sys_StopMode();
    assert(resets==2 && clock_polls<=1000003);
    puts("PASS: masked WFI, latched wake prevents STOP, clock restored before IRQ release, HSE failures bounded");
    return 0;
}
'''


def main():
    base = Path(os.environ["LOCALAPPDATA"]) / "CodexTemp/103-309-template/sleep-tests"
    base.mkdir(parents=True, exist_ok=True)
    folder = Path(tempfile.mkdtemp(prefix="run-", dir=base))
    enums = read("rtc_sleep.h").split("enum irqWakeup", 1)[1].split("extern", 1)[0]
    policy = read("rtc_sleep.c").replace('#include "main.h"', "")
    core = read("low_power.c")
    commit = read("SleepDeal.c").replace('#include "main.h"', "")
    flash = function_source(read("Flash.c"), "FlashWriteOneHalfWord")
    soc = function_source(read("SocEnhance.c"), "SOC_SaveBeforeSleep")
    key = function_source(read("IO_Control.c"), "App_DI1_Switch")
    policy_stubs = POLICY_STUBS
    for name in ["FLASH_ADDR_SLEEP_FLAG", "FLASH_NORMAL_SLEEP_VALUE", "FLASH_DEEP_SLEEP_VALUE",
                 "FLASH_HICCUP_SLEEP_VALUE", "FLASH_SLEEP_RESET_VALUE"]:
        definition = re.search(rf"^#define {name}\s+[^\n]+", clean(read("Flash.h")), re.MULTILINE)[0]
        policy_stubs = re.sub(rf"^#define {name}\s+[^\n]+", lambda m: definition,
                              policy_stubs, flags=re.MULTILINE)
    rtc = read("RTC.c").split("/* Poll budgets", 1)[1]
    rtc = rtc[rtc.index("#define RTC_WAIT_POLL_LIMIT"):]
    rtc_stubs = re.sub(r"^#define ALARM_TIME_SEC[^\n]+",
                       re.search(r"^#define ALARM_TIME_SEC[^\n]+", read("RTC.h"), re.MULTILINE)[0],
                       RTC_STUBS, flags=re.MULTILINE)
    rtc_header = read("RTC.h")
    clock_defs = rtc_header.split("/* BMS_RTC_CLOCK_CONFIG_BEGIN */", 1)[1]
    clock_defs = clock_defs.split("/* BMS_RTC_CLOCK_CONFIG_END */", 1)[0]
    rtc_stubs = rtc_stubs.replace("#define RTC_FLAG_RTOFF", clock_defs + "\n#define RTC_FLAG_RTOFF", 1)
    programs = {
        "policy": COMMON + '#include "low_power_port.h"\n' + "enum irqWakeup" + enums
                  + policy_stubs + flash + soc + commit + policy + core + key + POLICY_TESTS,
        "rtc_lsi": COMMON + rtc_stubs + rtc + RTC_TESTS,
        "rtc_lse": COMMON + "#define BMS_RTC_CLOCK_SOURCE BMS_RTC_CLOCK_LSE\n"
                   + rtc_stubs + rtc + RTC_TESTS,
        "stop": COMMON + "enum irqWakeup" + enums + STOP_STUBS
                + function_source(read("conf/conf.c"), "RestoreRunClockAfterStop")
                + function_source(read("conf/conf.c"), "Sys_StopMode") + STOP_TESTS,
    }
    env = msvc_environment()
    for name, text in programs.items():
        source = folder / f"{name}.c"
        source.write_text(clean(text), encoding="ascii")
        result = subprocess.run([shutil.which("cl.exe", path=env["PATH"]), "/nologo", "/TC", "/W3", "/O2",
                                 f"/I{SOURCE}", str(source), f"/Fe{folder / (name + '.exe')}",
                                 f"/Fo{folder / (name + '.obj')}"],
                                env=env, cwd=folder, capture_output=True)
        log = (result.stdout + result.stderr).decode("gbk", errors="replace")
        (folder / f"{name}.log").write_text(log, encoding="utf-8")
        if result.returncode:
            raise RuntimeError(log)
        subprocess.run([folder / f"{name}.exe"], check=True, timeout=10)
    print(f"Temporary outputs: {folder}")


if __name__ == "__main__":
    main()
