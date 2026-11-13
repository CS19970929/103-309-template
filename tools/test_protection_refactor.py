"""Run real ADC/protection C against host hardware stubs; outputs stay in user temp."""
import os
from pathlib import Path
import subprocess
import tempfile
import argparse
import re

ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / '103 + 309/Project/Source'


def function(text, signature):
    start = text.index(signature)
    brace = text.index('{', start)
    while ';' in text[start:brace]:  # Skip forward declarations.
        start = text.index(signature, start + len(signature))
        brace = text.index('{', start)
    depth = 0
    for pos in range(brace, len(text)):
        depth += (text[pos] == '{') - (text[pos] == '}')
        if depth == 0:
            return text[start:pos + 1] + '\n'
    raise ValueError(signature)


ADC_STUBS = r'''
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
typedef uint8_t UINT8; typedef uint16_t UINT16; typedef uint32_t UINT32;
typedef int32_t INT32;
typedef struct { int unused; } GPIO_TypeDef;
typedef struct { int GPIO_Mode, GPIO_Pin; } GPIO_InitTypeDef;
typedef struct { int ADC_ExternalTrigConv, ADC_NbrOfChannel; } ADC_InitTypeDef;
static GPIO_TypeDef gpio_a, gpio_b;
#define GPIOA (&gpio_a)
#define GPIOB (&gpio_b)
#define GPIO_Pin_1 2
#define RCC_APB2Periph_GPIOA 4
#define RCC_APB2Periph_GPIOB 1
#define RCC_APB2Periph_ADC1 2
#define ADC_Channel_1 1
#define ADC_Channel_9 9
#define ADC1 1
#define ADC_FLAG_EOC 1
#define GPIO_Mode_AIN 1
#define RCC_PCLK2_Div8 8
#define ADC_ExternalTrigConv_None 0
#define ADC_SampleTime_239Cycles5 239
#define ENABLE 1
#define DISABLE 0
#define RESET 0
#define ERROR_ADC 1
#define ERROR_REMOVE_ADC 0
static UINT32 tick;
static int eoc, calibration_stuck, selected_channel, conversions, adc_enabled, adc_clock;
static UINT16 conversion;
static UINT32 SysTime_Get10msTickCount(void) { return tick; }
static void RCC_APB2PeriphClockCmd(int p,int en) { if(p==RCC_APB2Periph_ADC1) adc_clock=en; }
static void ADC_Cmd(int p,int en) { adc_enabled=en; }
static void ADC_DeInit(int p) { eoc=0; }
static void GPIO_StructInit(GPIO_InitTypeDef *p) { memset(p,0,sizeof(*p)); }
static void GPIO_Init(GPIO_TypeDef *p,GPIO_InitTypeDef *cfg) {
    assert(p==GPIOA && cfg->GPIO_Pin==GPIO_Pin_1 && cfg->GPIO_Mode==GPIO_Mode_AIN);
}
static void RCC_ADCCLKConfig(int p) {}
static void ADC_StructInit(ADC_InitTypeDef *p) { memset(p,0,sizeof(*p)); }
static void ADC_Init(int p,ADC_InitTypeDef *cfg) { assert(cfg->ADC_NbrOfChannel==1); }
static void __delay_us(UINT32 us) { assert(us>=1); }
static void ADC_ResetCalibration(int p) {}
static int ADC_GetResetCalibrationStatus(int p) { return calibration_stuck; }
static void ADC_StartCalibration(int p) {}
static int ADC_GetCalibrationStatus(int p) { return calibration_stuck; }
static void System_ERROR_UserCallback(int p) {}
static void ADC_ClearFlag(int p,int flag) { eoc=0; }
static void ADC_SoftwareStartConvCmd(int p,int en) { assert(adc_clock && adc_enabled); ++conversions; }
static void ADC_RegularChannelConfig(int p,int ch,int rank,int sample) { selected_channel=ch; }
static int ADC_GetFlagStatus(int p,int flag) { return eoc; }
static UINT16 ADC_GetConversionValue(int p) { eoc=0; return conversion; }
'''

ADC_TESTS = r'''
static void complete(UINT16 raw) { conversion=raw; eoc=1; ADC_Task(); }
static void start_sweep(void) { tick+=ADC_SAMPLE_PERIOD_TICKS; ADC_Task(); }
static void group(UINT16 raw) { int i; complete(0); for(i=0;i<8;++i) complete(raw); }
int main(void) {
    UINT16 raw=99; int i, before;
    InitADC();
    assert(!ADC_TakeSample(ADC_INPUT_MOS,&raw));
    for(i=0;i<100;++i) ADC_Task();
    assert(conversions==0 && !ADC_TakeSample(ADC_INPUT_MOS,&raw));
    start_sweep(); assert(selected_channel==1); /* Board MOS NTC is on PA1. */
    /* Merely advancing time without an EOC cannot produce temperature. */
    tick+=ADC_CONVERSION_TIMEOUT_TICKS; ADC_Task();
    assert(ADC_TakeSample(ADC_INPUT_MOS,&raw)==ADC_SAMPLE_INVALID && raw==99);
    start_sweep(); complete(0); /* Real startup conversion is discarded. */
    complete(1800); complete(2200);
    for(i=0;i<5;++i) complete(2000);
    assert(!ADC_TakeSample(ADC_INPUT_MOS,&raw));
    complete(2000);
    assert(ADC_TakeSample(ADC_INPUT_MOS,&raw) && raw==2000);
    assert(!ADC_TakeSample(ADC_INPUT_MOS,&raw));
    assert(!ADC_TakeSample((enum ADC_INPUT)-1,&raw));
    assert(!ADC_TakeSample(ADC_INPUT_MOS,0));
    start_sweep(); complete(2000); complete(0);
    assert(ADC_TakeSample(ADC_INPUT_MOS,&raw)==ADC_SAMPLE_INVALID && raw==2000);
    start_sweep(); group(2050);
    assert(ADC_TakeSample(ADC_INPUT_MOS,&raw) && raw==2050);
    start_sweep(); complete(2050); complete(2050);
    ADC_StopForLowPower(); before=conversions;
    tick+=100000; for(i=0;i<100;++i) ADC_Task();
    assert(!adc_enabled && !adc_clock && conversions==before);
    assert(!ADC_TakeSample(ADC_INPUT_MOS,&raw));
    /* Full RUN restore starts a new group, including across tick wrap. */
    tick=UINT32_MAX-1; InitADC(); start_sweep(); group(2100);
    assert(ADC_TakeSample(ADC_INPUT_MOS,&raw) && raw==2100);
    calibration_stuck=1; InitADC(); tick+=100; ADC_Task();
    assert(!adc_enabled && !adc_clock && !ADC_TakeSample(ADC_INPUT_MOS,&raw));
    puts("PASS ADC: EOC, exact group, filtering, timeout, rails, STOP/wake, wrap, calibration failure");
    return 0;
}
'''

FAULT_STUBS = r'''
static void SOC_PauseCalibration(void) {}
static struct {
    union MDLCHGFAULT_REG unMdlFault_Second, unMdlFault_Third;
    UINT16 u16Ichg, u16IDischg;
    UINT16 u16Temperature[10];
} g_stCellInfoReport;
static struct {
    struct { UINT16 curValue; } u16IchgOcp_First, u16IdsgOcp_First;
} AFE_Parameters_RS485_Struction;
static struct {
    union {
        UINT8 all;
        struct { UINT8 IDLE:1, SLEEP:1, ENWDT:1, CADCON:1, CHGMOS:1, DSGMOS:1, PCHMOS:1, OCRC:1; } bits;
    } REG_MTP_CONF;
    struct { struct { UINT8 SC; } bits; } REG_BSTATUS1;
} SH367309_Reg_Store;
static UINT8 hardware_conf=0x38, AFE_PARAM_WRITE_Flag;
static int reads, writes, fail_read, fail_write, bad_verify;
#define MTP_CONF 0x40
#define ERROR_AFE1 2
typedef enum { GPIO_PreCHG, GPIO_CHG, GPIO_DSG } GPIO_Type;
static int MTPRead(int addr,int len,UINT8 *p) {
    ++reads; if(reads==fail_read) return 0;
    *p=hardware_conf; if(bad_verify && writes) *p^=0x10;
    return 1;
}
static int MTPWrite(int addr,int len,UINT8 *p) {
    ++writes; if(fail_write) return 0; hardware_conf=*p; return 1;
}
UINT8 SH367309_SetMosPermits(UINT8 charge_on,UINT8 discharge_on);
static UINT16 g_u16CalibCoefK[1]={1024};
static int16_t g_i16CalibCoefB[1];
#define MDL_TEMP_MOS1 0
#define MOS_TEMP1 9
static UINT8 sample_status;
static UINT16 sample_raw;
UINT8 ADC_TakeSample(enum ADC_INPUT input, UINT16 *raw) {
    UINT8 status=sample_status; sample_status=ADC_SAMPLE_NONE;
    if(status==ADC_SAMPLE_READY) *raw=sample_raw;
    return status;
}
'''

ADC_MEASUREMENT_TESTS = r'''
static void sample_pins(UINT16 pa1_raw, UINT16 pb1_raw) {
    int i;
    tick+=ADC_SAMPLE_PERIOD_TICKS; ADC_Task();
    for(i=0;i<9;++i) {
        conversion=selected_channel==1 ? pa1_raw : pb1_raw;
        eoc=1; ADC_Task(); DataLoad_MosTemperature();
    }
}
int main(void) {
    int i;
    reset_case(); InitADC();
    sample_pins(2048,2048);
    assert(g_stCellInfoReport.u16Temperature[MOS_TEMP1]==650);
    sample_pins(1509,2048); /* Heat the connected PA1 probe from 25 C to 40 C. */
    assert(g_stCellInfoReport.u16Temperature[MOS_TEMP1]==800);
    sample_pins(1509,400); /* PB1 is unrelated to MOS temperature. */
    assert(g_stCellInfoReport.u16Temperature[MOS_TEMP1]==800);
    sample_pins(0,400); /* Invalid input retains display but cannot trip heat. */
    assert(g_stCellInfoReport.u16Temperature[MOS_TEMP1]==800);
    assert(!g_stCellInfoReport.unMdlFault_Third.bits.b1TmosOtp);
    tick+=ADC_SAMPLE_PERIOD_TICKS; ADC_Task();
    conversion=400; eoc=1; tick+=ADC_CONVERSION_TIMEOUT_TICKS;
    ADC_Task(); DataLoad_MosTemperature(); /* A delayed EOC is not fresh heat. */
    assert(g_stCellInfoReport.u16Temperature[MOS_TEMP1]==800);
    assert(!g_stCellInfoReport.unMdlFault_Third.bits.b1TmosOtp);
    ADC_StopForLowPower(); Fault_PauseMeasurements(); tick+=100000;
    InitADC(); ADC_Task(); DataLoad_MosTemperature();
    assert(!g_stCellInfoReport.unMdlFault_Third.bits.b1TmosOtp);
    sample_pins(2048,400);
    assert(g_stCellInfoReport.u16Temperature[MOS_TEMP1]==650);
    for(i=0;i<11;++i) sample_pins(400,2048);
    assert(g_stCellInfoReport.unMdlFault_Third.bits.b1TmosOtp);
    for(i=0;i<11;++i) sample_pins(2048,400);
    assert(!g_stCellInfoReport.unMdlFault_Third.bits.b1TmosOtp);
    puts("PASS PA1 MOS: probe heating -> ADC -> NTC -> report -> heat latch/recovery, invalid/late input, RUN restore");
    return 0;
}
'''

FAULT_TESTS = r'''
static void reset_case(void) {
    memset(&g_stCellInfoReport,0,sizeof(g_stCellInfoReport));
    memset(&SH367309_Reg_Store,0,sizeof(SH367309_Reg_Store));
    FaultPoint_Third2=0; reads=writes=fail_read=fail_write=bad_verify=0;
    AFE_PARAM_WRITE_Flag=0; hardware_conf=0x38;
    AFE_Parameters_RS485_Struction.u16IchgOcp_First.curValue=120;
    AFE_Parameters_RS485_Struction.u16IdsgOcp_First.curValue=150;
    PRT_E2ROMParas.u16TmosOTp_Third=1350;
    PRT_E2ROMParas.u16TmosOTp_Rcv=1200;
    PRT_E2ROMParas.u16TmosOTp_Filter=100;
    s_charge_recovery_ticks=s_discharge_recovery_ticks=0;
    s_host_mos_off=0; tick=0; Fault_PauseMeasurements();
}
int main(void) {
    unsigned n; int i;
    /* A host close stays requested while reverse current temporarily conducts. */
    for(n=1;n<=2;++n) {
        reset_case();
        assert(Fault_SetHostMosOff((UINT8)n));
        assert(((hardware_conf>>4)&3)==(3U^n));
        if(n==1) g_stCellInfoReport.u16IDischg=9;
        else g_stCellInfoReport.u16Ichg=9;
        Fault_UpdateCurrentProtection(1);
        assert(Fault_ApplyMosPermits(1,1) && ((hardware_conf>>4)&3)==(3U^n));
        if(n==1) g_stCellInfoReport.u16IDischg=10;
        else g_stCellInfoReport.u16Ichg=10;
        Fault_UpdateCurrentProtection(1);
        assert(Fault_ApplyMosPermits(1,1) && (hardware_conf&0x30)==0x30);
        assert(s_host_mos_off==n);
        g_stCellInfoReport.u16Ichg=g_stCellInfoReport.u16IDischg=0;
        Fault_UpdateCurrentProtection(1);
        assert(Fault_ApplyMosPermits(1,1) && ((hardware_conf>>4)&3)==(3U^n));
        assert(s_host_mos_off==n);
        if(n==1) g_stCellInfoReport.u16IDischg=10;
        else g_stCellInfoReport.u16Ichg=10;
        Fault_UpdateCurrentProtection(1);
        AFE_PARAM_WRITE_Flag=1;
        assert(!Fault_ApplyMosPermits(1,1) && ((hardware_conf>>4)&3)==(3U^n));
        AFE_PARAM_WRITE_Flag=0; fail_write=1;
        assert(!Fault_ApplyMosPermits(1,1) && s_host_mos_off==n);
        fail_write=0;
        assert(Fault_ApplyMosPermits(1,1) && (hardware_conf&0x30)==0x30);
        tick+=51; /* Stale current cannot hold the temporary release. */
        assert(Fault_ApplyMosPermits(1,1) && ((hardware_conf>>4)&3)==(3U^n));
        tick=UINT32_MAX-10; Fault_PauseMeasurements(); Fault_UpdateCurrentProtection(1);
        tick+=20;
        assert(Fault_IsCurrentSampleFresh());
        assert(Fault_ApplyMosPermits(1,1) && (hardware_conf&0x30)==0x30);
        Fault_UpdateCurrentProtection(0);
        assert(!Fault_IsCurrentSampleFresh());
        assert(Fault_ApplyMosPermits(1,1) && ((hardware_conf>>4)&3)==(3U^n));
        Fault_UpdateCurrentProtection(1);
        assert(Fault_ApplyMosPermits(1,1) && (hardware_conf&0x30)==0x30);
        Fault_PauseMeasurements();
        assert(!Fault_IsCurrentSampleFresh());
        assert(Fault_ApplyMosPermits(1,1) && ((hardware_conf>>4)&3)==(3U^n));
        assert(s_host_mos_off==n);
    }
    /* Every combination of AFE directional faults, both software latches,
       MOS heat, host requests, caller requests and reverse current. */
    for(n=0;n<131072;++n) {
        unsigned charge_fault, discharge_fault, reverse_charge, reverse_discharge, heat;
        unsigned host=(n>>15)&3, requested;
        reset_case();
        g_stCellInfoReport.unMdlFault_Third.bits.b1CellOvp=(n>>0)&1;
        g_stCellInfoReport.unMdlFault_Third.bits.b1IchgOcp=(n>>1)&1;
        g_stCellInfoReport.unMdlFault_Third.bits.b1CellChgOtp=(n>>2)&1;
        g_stCellInfoReport.unMdlFault_Third.bits.b1CellChgUtp=(n>>3)&1;
        g_stCellInfoReport.unMdlFault_Third.bits.b1CellUvp=(n>>4)&1;
        g_stCellInfoReport.unMdlFault_Third.bits.b1IdischgOcp=(n>>5)&1;
        g_stCellInfoReport.unMdlFault_Third.bits.b1CellDischgOtp=(n>>6)&1;
        g_stCellInfoReport.unMdlFault_Third.bits.b1CellDischgUtp=(n>>7)&1;
        SH367309_Reg_Store.REG_BSTATUS1.bits.SC=(n>>8)&1;
        g_stCellInfoReport.unMdlFault_Second.bits.b1IchgOcp=(n>>9)&1;
        g_stCellInfoReport.unMdlFault_Second.bits.b1IdischgOcp=(n>>10)&1;
        heat=(n>>11)&1; g_stCellInfoReport.unMdlFault_Third.bits.b1TmosOtp=heat;
        reverse_charge=(n>>12)&1; reverse_discharge=(n>>13)&1;
        g_stCellInfoReport.u16Ichg=(UINT16)(reverse_charge*10);
        g_stCellInfoReport.u16IDischg=(UINT16)(reverse_discharge*10);
        Fault_UpdateCurrentProtection((n>>14)&1);
        if(!(n&0x4000)) reverse_charge=reverse_discharge=0;
        assert(Fault_SetHostMosOff((UINT8)host));
        for(requested=0;requested<4;++requested) {
            unsigned common=heat || host==3 || requested==0;
            charge_fault=((n&15)!=0)||((n>>9)&1)||(host&1)||!(requested&1);
            discharge_fault=((n&0x1f0)!=0)||((n>>10)&1)||(host&2)||!(requested&2);
            assert(Fault_ApplyMosPermits((UINT8)(requested&1),(UINT8)((requested>>1)&1)));
            assert(((hardware_conf>>4)&1)==!(common || (charge_fault && !reverse_discharge)));
            assert(((hardware_conf>>5)&1)==!(common || (discharge_fault && !reverse_charge)));
            assert(s_host_mos_off==host);
        }
    }
    for(n=0;n<4;++n) {
        reset_case(); g_stCellInfoReport.unMdlFault_Third.bits.b1TmosOtp=1;
        assert(Fault_SetHostMosOff((UINT8)n) && !(hardware_conf&0x30));
        assert(Fault_SetHostMosOff(0) && !(hardware_conf&0x30));
        g_stCellInfoReport.unMdlFault_Third.bits.b1TmosOtp=0;
        assert(Fault_SetHostMosOff((UINT8)n));
        assert(((hardware_conf>>4)&1)==!(n&1));
        assert(((hardware_conf>>5)&1)==!(n&2));
        Fault_PauseMeasurements(); assert(s_host_mos_off==n);
    }
    reset_case(); fail_write=1; assert(!Fault_SetHostMosOff(3) && s_host_mos_off==3);
    fail_write=0; assert(Fault_ApplyMosPermits(1,1) && !(hardware_conf&0x30));
    assert(!Fault_SetHostMosOff(4) && s_host_mos_off==3);
    reset_case(); g_stCellInfoReport.u16Ichg=120;
    for(i=0;i<5;++i) { tick+=20; Fault_UpdateCurrentProtection(1); }
    assert(FaultPoint_Third2==1 && Fault_record_Third2[0]==IchgOcp_Second);
    /* Hardware output may be off while CONF still grants permission. */
    assert(Fault_ApplyMosPermits(1,1) && !(hardware_conf&0x10) && writes==1);
    g_stCellInfoReport.u16Ichg=0;
    for(i=0;i<1000;++i) Fault_UpdateCurrentProtection(1); /* queued tasks, same tick */
    assert(g_stCellInfoReport.unMdlFault_Second.bits.b1IchgOcp);
    for(i=0;i<149;++i) { tick+=20; Fault_UpdateCurrentProtection(1); }
    assert(g_stCellInfoReport.unMdlFault_Second.bits.b1IchgOcp);
    tick+=20; Fault_UpdateCurrentProtection(1);
    assert(!g_stCellInfoReport.unMdlFault_Second.bits.b1IchgOcp);
    reset_case(); g_stCellInfoReport.u16IDischg=150; Fault_UpdateCurrentProtection(1);
    g_stCellInfoReport.u16IDischg=0;
    tick=100000; Fault_UpdateCurrentProtection(0); Fault_UpdateCurrentProtection(1);
    assert(s_discharge_recovery_ticks==0);
    tick=UINT32_MAX-10; Fault_PauseMeasurements(); Fault_UpdateCurrentProtection(1);
    tick+=20; Fault_UpdateCurrentProtection(1); assert(s_discharge_recovery_ticks==20);
    Fault_PauseMeasurements(); assert(g_stCellInfoReport.unMdlFault_Second.bits.b1IdischgOcp);
    reset_case(); fail_read=1; assert(!SH367309_SetMosPermits(0,0) && writes==0);
    reset_case(); fail_write=1; assert(!SH367309_SetMosPermits(0,0));
    assert(SH367309_Reg_Store.REG_MTP_CONF.all==0);
    reset_case(); fail_read=2; assert(!SH367309_SetMosPermits(0,0));
    reset_case(); bad_verify=1; assert(!SH367309_SetMosPermits(0,0));
    bad_verify=0; assert(SH367309_SetMosPermits(0,0));
    reset_case(); hardware_conf=0x08; AFE_PARAM_WRITE_Flag=1;
    assert(!SH367309_SetMosPermits(1,1) && hardware_conf==0x08);
    AFE_PARAM_WRITE_Flag=0; assert(SH367309_SetMosPermits(1,1) && hardware_conf==0x38);
    reset_case(); hardware_conf=0x08; SH367309_Reg_Store.REG_MTP_CONF.all=0x38;
    AFE_Sleep(); assert(hardware_conf==0x0a); /* A stale permit shadow cannot reopen either MOS. */
    reset_case();
    for(i=0;i<1000;++i) Fault_UpdateMosTemperature(1,1350);
    assert(!g_stCellInfoReport.unMdlFault_Third.bits.b1TmosOtp);
    for(i=0;i<9;++i) { tick+=10; Fault_UpdateMosTemperature(1,1350); }
    assert(!g_stCellInfoReport.unMdlFault_Third.bits.b1TmosOtp);
    tick+=10; Fault_UpdateMosTemperature(1,1350);
    assert(g_stCellInfoReport.unMdlFault_Third.bits.b1TmosOtp && (hardware_conf&0x30)==0);
    assert(FaultPoint_Third2==1 && Fault_record_Third2[0]==MosOTp_Third);
    for(i=0;i<100;++i) { tick+=10; Fault_UpdateMosTemperature(1,1349); }
    assert(g_stCellInfoReport.unMdlFault_Third.bits.b1TmosOtp && FaultPoint_Third2==1);
    Fault_UpdateMosTemperature(0,0); Fault_PauseMeasurements(); tick=0;
    assert(g_stCellInfoReport.unMdlFault_Third.bits.b1TmosOtp);
    for(i=0;i<10;++i) { tick+=10; Fault_UpdateMosTemperature(1,1200); }
    assert(g_stCellInfoReport.unMdlFault_Third.bits.b1TmosOtp);
    tick+=10; Fault_UpdateMosTemperature(1,1200);
    assert(!g_stCellInfoReport.unMdlFault_Third.bits.b1TmosOtp && (hardware_conf&0x30)==0x30);
    /* Interrupted or old samples must start a fresh confirmation interval. */
    reset_case(); Fault_UpdateMosTemperature(1,1350);
    for(i=0;i<9;++i) { tick+=10; Fault_UpdateMosTemperature(1,1350); }
    Fault_UpdateMosTemperature(0,0); tick+=10; Fault_UpdateMosTemperature(1,1350);
    assert(!g_stCellInfoReport.unMdlFault_Third.bits.b1TmosOtp);
    tick+=1000; Fault_UpdateMosTemperature(1,1350);
    assert(!g_stCellInfoReport.unMdlFault_Third.bits.b1TmosOtp && s_mos_confirm_ticks==0);
    Fault_ResetMosConfirmation();
    tick+=10; Fault_UpdateMosTemperature(1,1350); assert(s_mos_confirm_ticks==0);
    tick=UINT32_MAX-50; Fault_ResetMosConfirmation(); Fault_UpdateMosTemperature(1,1350);
    for(i=0;i<10;++i) { tick+=10; Fault_UpdateMosTemperature(1,1350); }
    assert(g_stCellInfoReport.unMdlFault_Third.bits.b1TmosOtp);
    PRT_E2ROMParas.u16TmosOTp_Rcv=1350; assert(!Fault_MosParametersValid(&PRT_E2ROMParas));
    assert(!Fault_MosParametersValid(0));
    reset_case(); PRT_E2ROMParas.u16TmosOTp_Third=2000;
    assert(!Fault_MosParametersValid(&PRT_E2ROMParas));
    Fault_UpdateMosTemperature(1,1350);
    for(i=0;i<10;++i) { tick+=10; Fault_UpdateMosTemperature(1,1350); }
    assert(g_stCellInfoReport.unMdlFault_Third.bits.b1TmosOtp);
    assert(PRT_E2ROMParas.u16TmosOTp_Third==2000); /* No storage rewrite. */
    reset_case(); sample_status=ADC_SAMPLE_NONE;
    DataLoad_MosTemperature(); assert(!g_stCellInfoReport.unMdlFault_Third.bits.b1TmosOtp);
    sample_status=ADC_SAMPLE_READY; sample_raw=2048;
    DataLoad_MosTemperature(); assert(g_stCellInfoReport.u16Temperature[MOS_TEMP1]==650);
    sample_status=ADC_SAMPLE_READY; sample_raw=0;
    DataLoad_MosTemperature(); assert(g_stCellInfoReport.u16Temperature[MOS_TEMP1]==650);
    assert(s_mos_confirm_ticks==FAULT_MOS_CONFIRM_IDLE);
    for(i=0;i<12;++i) {
        sample_status=ADC_SAMPLE_READY; sample_raw=400; tick+=10; DataLoad_MosTemperature();
    }
    assert(g_stCellInfoReport.unMdlFault_Third.bits.b1TmosOtp);
    sample_status=ADC_SAMPLE_INVALID; DataLoad_MosTemperature();
    assert(g_stCellInfoReport.unMdlFault_Third.bits.b1TmosOtp);
    test_parameter_transaction();
    puts("PASS protection: 524288 permit combinations, host reverse flow/zero/freshness/STOP, independent latches, CONF retry");
    puts("PASS MOS: trigger/recovery hysteresis, invalid/old samples, STOP, wrap, parameter fallback, real NTC path");
    return 0;
}
'''

PARAMETER_STUBS = r'''
#define SNum 16
#define E2P_PARA_NUM_PROTECT (sizeof(struct PRT_E2ROM_PARAS)/sizeof(UINT16))
#define RS485_ACK_NEG 1
#define RS485_ERROR_DATA_INVALID 2
#define RS485_ERROR_CMD_INVALID 3
struct RS485MSG { UINT16 u16Buffer[160]; UINT8 AckType, ErrorType; };
static int save_calls, save_fails;
static UINT8 EEPROM_SaveRWParametersToFlash(UINT8 cycle_count_written) { ++save_calls; return (UINT8)!save_fails; }
static void InitData_SOC(void) {}
'''

PARAMETER_TESTS = r'''
static void parameter_request(struct RS485MSG *s, UINT16 value) {
    memset(s,0,sizeof(*s)); s->u16Buffer[5]=1; s->u16Buffer[6]=2;
    s->u16Buffer[7]=value>>8; s->u16Buffer[8]=value&255;
}
static void test_parameter_transaction(void) {
    struct RS485MSG request;
    struct PRT_E2ROM_PARAS before;
    const struct PRT_E2ROM_PARAS defaults=E2P_PROTECT_DEFAULT_PRT;
    UINT16 trip_address=RS485_CMD_ADDR_TMOS_OTP_FIRST+2;
    PRT_E2ROMParas=defaults; before=PRT_E2ROMParas;
    g_stCellInfoReport.unMdlFault_Third.bits.b1TmosOtp=1;
    save_calls=save_fails=0; s_mos_confirm_ticks=55;
    parameter_request(&request,1200); /* Equals recovery: reject before Flash. */
    Sci_WrRegs_0x10_Protect(trip_address,&request);
    assert(request.ErrorType==RS485_ERROR_DATA_INVALID && save_calls==0);
    assert(memcmp(&before,&PRT_E2ROMParas,sizeof(before))==0 && s_mos_confirm_ticks==55);
    parameter_request(&request,1451); /* Outside the NTC characterized range. */
    Sci_WrRegs_0x10_Protect(trip_address,&request);
    assert(request.ErrorType==RS485_ERROR_DATA_INVALID && save_calls==0);
    parameter_request(&request,1400); save_fails=1;
    Sci_WrRegs_0x10_Protect(trip_address,&request);
    assert(request.ErrorType==RS485_ERROR_CMD_INVALID && save_calls==1);
    assert(memcmp(&before,&PRT_E2ROMParas,sizeof(before))==0);
    parameter_request(&request,1400); save_fails=0;
    Sci_WrRegs_0x10_Protect(trip_address,&request);
    assert(request.AckType==0 && save_calls==2 && s_mos_confirm_ticks==FAULT_MOS_CONFIRM_IDLE);
    before.u16TmosOTp_Third=1400;
    assert(memcmp(&before,&PRT_E2ROMParas,sizeof(before))==0);
    assert(g_stCellInfoReport.unMdlFault_Third.bits.b1TmosOtp);
    parameter_request(&request,1300); request.u16Buffer[6]=0;
    Sci_WrRegs_0x10_Protect(trip_address,&request);
    assert(request.ErrorType==RS485_ERROR_CMD_INVALID && save_calls==2);
    puts("PASS MOS parameters: cross-field validation, no invalid Flash writes, rollback, hot update retains fault");
}
'''

AFE_STUBS = r'''
#include <stdbool.h>
#define LIFEPO 1
#define AFE_OCC1 120
#define AFE_OCC2 120
#define AFE_ODC1 150
#define AFE_ODC2 150
#define CBC_Cur_DSG 1000
#define CBC_DelayT 256
#define RS485_ACK_NEG 1
#define RS485_ERROR_CMD_INVALID 2
#define RS485_ERROR_DATA_INVALID 3
#define ERROR_AFE1 2
#define Feed_IWatchDog ((void)0)
struct RS485MSG { UINT16 u16Buffer[160]; UINT8 AckType, ErrorType; };
static struct { UINT16 TR_ResRef; } SH367309_Reg_Store;
static struct { UINT16 u16Sys_CS_Res_Num, u16Sys_CS_Res; } OtherElement={2,2};
static UINT32 g_u32CS_Res_AFE;
static UINT8 SeriesNum=16, ucMTPBuffer[26], hardware_rom[25];
static int MCUO_AFE_VPRO, ctl, resets, startup_calls, calibration_calls;
static int tr_fail, ready_fail, rom_write_fail, permit_fail, heat_latched;
static int rom_reads, rom_read_fail, saved, save_fail, rom_writes, permits;
static int MTPRead(int addr,int len,UINT8 *p) {
    if(addr==0x19) { if(tr_fail) return 0; *p=64; return 1; }
    assert(addr==0 && len==25);
    if(++rom_reads==rom_read_fail) return 0;
    memcpy(p,hardware_rom,25); return 1;
}
static int MTPWriteROM(UINT8 addr,int len,UINT8 *p) {
    ++rom_writes; if(rom_write_fail) return 0; hardware_rom[addr]=*p; return 1;
}
static UINT8 StorageFlash_SaveAfeData(UINT16 *p,UINT16 len) { ++saved; return (UINT8)!save_fail; }
static void Delay1ms(int ms) {}
static void AFE_Reset(void) { ++resets; }
static UINT8 AFE_IsReady(void) { return (UINT8)ready_fail; }
static void close_ctlc(void) { ctl=0; }
static void open_ctlc(void) { ctl=1; }
static void initAFE1_IIC(void) {}
static void AfeCurrent_StartupZeroCal(void) { ++calibration_calls; }
static UINT8 App_SH367309_Monitor(void) { return 1U; }
'''

AFE_TESTS = r'''
static void reset_afe(void) {
    const AFE_Parameters_RS485_Typedef defaults=AFE_PARAMETERS_RS485_STRUCTION_DEFAULT;
    AFE_Parameters_RS485_Struction=defaults;
    OtherElement.u16Sys_CS_Res_Num=OtherElement.u16Sys_CS_Res=2;
    memset(hardware_rom,0,sizeof(hardware_rom));
    AFE_PARAM_WRITE_Flag=1; tr_fail=ready_fail=rom_write_fail=permit_fail=heat_latched=0;
    rom_reads=rom_read_fail=rom_writes=saved=save_fail=resets=startup_calls=calibration_calls=permits=0;
    ctl=1;
}
int main(void) {
    struct RS485MSG request;
    AFE_ROM_PARAMETERS_TypeDef previous;
    UINT16 old;
    unsigned value;
    reset_afe(); assert(sizeof(AFE_ROM_PARAMETERS_Struction)==26);
    assert(Refresh_Parameters());
    assert(s_sh_afe_ocd1t[AFE_ROM_PARAMETERS_Struction.m0CH_0DH.OCD1T]==600);
    assert(s_sh_afe_occt_ocd2t[AFE_ROM_PARAMETERS_Struction.m0EH_0FH.OCCT]==100);
    for(value=0;value<=500000;value+=10) {
        int index=Choose_Right_Value(value,s_sh_afe_ocd1t);
        assert(index>=0 && index<16);
        assert(s_sh_afe_ocd1t[index]>=value || index==15);
        if(index) assert(s_sh_afe_ocd1t[index-1]<value);
    }
    AFE_Parameters_RS485_Struction.u16IdsgOcp_Filter_Second.curValue=6554;
    assert(Refresh_Parameters());
    assert(AFE_ROM_PARAMETERS_Struction.m0CH_0DH.OCD1T==15); /* No 16-bit wrap to 4 ms. */
    previous=AFE_ROM_PARAMETERS_Struction; tr_fail=1;
    assert(!Refresh_Parameters() && memcmp(&previous,&AFE_ROM_PARAMETERS_Struction,26)==0);
    tr_fail=0; OtherElement.u16Sys_CS_Res=0; assert(!Refresh_Parameters());
    OtherElement.u16Sys_CS_Res=65535; OtherElement.u16Sys_CS_Res_Num=1;
    assert(!Refresh_Parameters());
    reset_afe(); AFE_Parameters_RS485_Struction.u16TChgOTp.curValue=2000;
    assert(!Refresh_Parameters());
    App_SH367309(); assert(AFE_PARAM_WRITE_Flag && rom_writes==0 && resets==0);
    reset_afe(); memset(&request,0,sizeof(request));
    request.u16Buffer[2]=0x24; request.u16Buffer[3]=14; /* Charge high-temperature register. */
    request.u16Buffer[5]=1; request.u16Buffer[6]=2;
    request.u16Buffer[7]=2000>>8; request.u16Buffer[8]=2000&255;
    old=AFE_Parameters_RS485_Struction.u16TChgOTp.curValue;
    assert(Sci_WrRegs_0x10_AFE_Parameters(0,&request));
    assert(request.ErrorType==RS485_ERROR_DATA_INVALID && saved==0);
    assert(AFE_Parameters_RS485_Struction.u16TChgOTp.curValue==old);
    request.u16Buffer[5]=0; request.u16Buffer[6]=0;
    Sci_WrRegs_0x10_AFE_Parameters(0,&request);
    assert(request.ErrorType==RS485_ERROR_CMD_INVALID && saved==0);
    request.u16Buffer[5]=1; request.u16Buffer[6]=1;
    Sci_WrRegs_0x10_AFE_Parameters(0,&request);
    assert(request.ErrorType==RS485_ERROR_CMD_INVALID && saved==0);
    reset_afe(); rom_write_fail=1; App_SH367309();
    assert(AFE_PARAM_WRITE_Flag && ctl==0 && startup_calls==0);
    /* Successful ROM writes but failed readback: equal-profile retry must reopen
       only after readiness and the MOS permit have both been confirmed. */
    reset_afe(); rom_read_fail=3; App_SH367309();
    assert(AFE_PARAM_WRITE_Flag && ctl==0 && startup_calls==0);
    rom_read_fail=0; heat_latched=1; App_SH367309();
    assert(!AFE_PARAM_WRITE_Flag && ctl==1 && startup_calls==1 && permits==0);
    reset_afe(); ready_fail=1; InitAFE1(); assert(ctl==0 && permits==0);
    reset_afe(); permit_fail=1; InitAFE1(); assert(ctl==0 && permits==0);
    reset_afe(); heat_latched=1; InitAFE1(); assert(ctl==1 && permits==0);
    puts("PASS AFE: 600 ms OCD1, wide timing sweep, NTC bounds, TR/ROM/readback failures, retry and startup gates");
    return 0;
}
'''

RTC_STUBS = r'''
#define USART1 1
#define USART2 2
static int adc_on, adc_starts, order, afe_temperatures;
static struct { int wakeup_rtc; } sys_time;
static void InitDelay(void) { assert(order==0); order=1; }
static void RTC_RestoreRunInterrupts(void) { assert(order==1); order=2; }
static void InitIO_rtc(void) { assert(order==2); order=3; }
static void ADC_StopForLowPower(void) { adc_on=0; }
static void USART_DeInit(int port) { assert(!adc_on && order==3); }
static void InitTimer(void) { assert(order==3); tick=0; order=4; }
static void InitUSART_CommonUpper(void) { assert(order==4); order=5; }
static void InitCan(void) { assert(order==5); order=6; }
static void InitADC(void) { assert(order==6 && tick==0); adc_on=1; ++adc_starts; order=7; }
static int RTC_IsStopWakeup(void) { return 1; }
static void initAFE1_IIC(void) { assert(order==7); order=8; }
static UINT8 UpdateVoltageFromBqMaximo(void) { return 0; }
static void DataLoad_CellVolt(void) {}
static void DataLoad_CellVoltMaxMinFind(void) {}
static void DataLoad_Temperature(void) { assert(!adc_on); ++afe_temperatures; }
static void DataLoad_TemperatureMaxMinFind(void) {}
'''

RTC_TESTS = r'''
int main(void) {
    int i;
    ADC_StopForLowPower();
    for(i=0;i<100;++i) assert(RtcSleep_AfePortUpdateRtcData());
    assert(!adc_on && adc_starts==0 && afe_temperatures==100);
    tick=100000;
    InitRunAfterStopWakeup();
    assert(adc_on && adc_starts==1 && order==8);
    puts("PASS RTC: periodic AFE checks leave ADC off; full RUN restores GPIO/tick before ADC");
    return 0;
}
'''


def run_c(name, code, output, env, compiler):
    path = output / (name + '.c')
    path.write_text(code, encoding='utf-8')
    exe = output / (name + '.exe')
    result = subprocess.run([str(compiler), '/nologo', '/W3', '/utf-8', '/TC', '/std:c11',
                             str(path), '/Fe:' + str(exe), '/Fo:' + str(output / (name + '.obj'))],
                            cwd=output, env=env, capture_output=True, text=True, errors='replace')
    if result.returncode:
        raise RuntimeError(result.stdout + result.stderr)
    subprocess.run([str(exe)], cwd=output, check=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--committed-measurement', action='store_true',
                        help='Use committed DataDeal.c while the worktree has a manual temperature injection')
    args = parser.parse_args()
    temp_root = Path(os.environ['LOCALAPPDATA']) / 'CodexTemp/103-309-copy'
    temp_root.mkdir(parents=True, exist_ok=True)
    output = Path(tempfile.mkdtemp(prefix='protection-host-', dir=temp_root))
    vs = Path(r'C:\Program Files\Microsoft Visual Studio\2022\Community')
    setup = output / 'compiler-env.bat'
    setup.write_text('@echo off\ncall "' + str(vs / 'Common7/Tools/VsDevCmd.bat') +
                     '" -no_logo -arch=x64 -host_arch=x64 >nul\nset\n')
    env = os.environ.copy()
    for line in subprocess.check_output(['cmd.exe', '/d', '/c', str(setup)],
                                        text=True, errors='replace').splitlines():
        key, sep, value = line.partition('=')
        if sep and key:
            env[key] = value
    compiler = sorted((vs / 'VC/Tools/MSVC').glob('*/bin/Hostx64/x64/cl.exe'))[-1]
    adc = (SOURCE / 'ADC.c').read_text(encoding='utf-8').replace('#include "main.h"', '')
    # Read the real board pin definitions, rather than repeating them in a stub.
    gpio = '\n'.join(line for line in (SOURCE / 'conf/conf_gpio.h').read_text(encoding='utf-8').splitlines()
                     if re.match(r'#define\s+(GPIO_ADC_NMOS|PIN_ADC_NMOS)\b', line)) + '\n'
    adc_stubs = ADC_STUBS + gpio
    run_c('adc', adc_stubs + (SOURCE / 'ADC.h').read_text() + adc + ADC_TESTS, output, env, compiler)
    upper = (SOURCE / 'Sci_Upper.h').read_text(encoding='utf-8', errors='replace')
    # Use only the real protocol fault bit layout, not the rest of the report.
    bits = upper[upper.index('struct MDLCHGFAULT_BITS'):
                 upper.index('};', upper.index('union  MDLCHGFAULT_REG')) + 2]
    driver = (SOURCE / 'SH367309_Func.c').read_text(encoding='utf-8')
    driver = ''.join(function(driver, sig) for sig in (
        'static UINT8 SH367309_WriteConfBits(', 'UINT8 SH367309_SetMosPermits(',
        'static void AFE_SetLowPowerBit(', 'void AFE_Sleep('))
    fault = (SOURCE / 'Fault.c').read_text(encoding='utf-8').replace('#include "main.h"', '')
    conversion = function((SOURCE / 'PubFunc.c').read_text(encoding='utf-8'), 'UINT16 GetEndValue(')
    measurement = (SOURCE / 'DataDeal.c').read_text(encoding='utf-8')
    if args.committed_measurement:
        measurement = subprocess.check_output(
            ['git','show','HEAD:103 + 309/Project/Source/DataDeal.c'], cwd=ROOT).decode('utf-8')
        print('Measurement fixture: committed DataDeal.c; worktree injection is excluded explicitly')
    elif re.search(r'(?m)^[ \t]*temperature\s*=\s*sys_time\.test_mos_temp\s*;', measurement):
        raise SystemExit('Worktree overrides real MOS temperature with sys_time.test_mos_temp. '
                         'Use --committed-measurement to test the committed NTC path without changing it.')
    conversion += function(measurement, 'void DataLoad_MosTemperature(')
    sci = (SOURCE / 'Sci_Upper.c').read_text(encoding='utf-8')
    parameters = 'enum {\n' + upper[upper.index('RS485_CMD_ADDR_VCELL_OVP_FIRST ='):
                                   upper.index('RS485_CMD_ADDR_VDELTA_OP_FIRST,')] + '\n};\n'
    parameters += PARAMETER_STUBS
    parameters += ''.join(function(sci, sig) for sig in (
        'static void Sci_SetWrError(', 'static UINT16 Sci_GetWrRegNum(',
        'static UINT16 Sci_GetWrValue(', 'static UINT8 Sci_WrRegsByteCountValid(',
        'static UINT8 Sci_RangeFits(', 'static UINT8 Sci_RangeOverlaps(',
        'static void Sci_CopyWords(', 'static UINT8 Sci_WrValuesInRange(',
        'static void Sci_WriteWordsFromRequest(', 'static void Sci_ApplyProtectSideEffects(',
        'void Sci_WrRegs_0x10_Protect('))
    run_c('protection', ADC_STUBS + (SOURCE / 'ADC.h').read_text() +
          (SOURCE / 'Fault.h').read_text(encoding='utf-8') + bits +
          FAULT_STUBS + driver + fault + conversion + parameters + PARAMETER_TESTS + FAULT_TESTS,
          output, env, compiler)
    real_adc_stubs = FAULT_STUBS.replace(function(FAULT_STUBS, 'UINT8 ADC_TakeSample(').rstrip(), '')
    run_c('mos_measurement', adc_stubs + (SOURCE / 'ADC.h').read_text() + adc +
          (SOURCE / 'Fault.h').read_text(encoding='utf-8') + bits + real_adc_stubs + driver +
          fault + conversion + function(FAULT_TESTS, 'static void reset_case(') + ADC_MEASUREMENT_TESTS,
          output, env, compiler)
    afe = (SOURCE / 'SH367309_DataDeal.c').read_text(encoding='utf-8')
    afe_prefix = afe[afe.index('int AFE_PARAM_WRITE_Flag'):afe.index('static void AFE_CopyCurValues')]
    afe_source = ''.join(function(afe, sig) for sig in (
        'static void AFE_CopyCurValues(', 'static void AFE_RestoreCurValues(',
        'static UINT8 AFE_SaveCurValuesToFlash(', 'bool Write_Parameters(',
        'bool SH367309_UpdataAfeConfig(', 'UINT8 Sci_WrRegs_0x10_AFE_Parameters('))
    afe_header = (SOURCE / 'SH367309_DataDeal.h').read_text(encoding='utf-8').replace('#include "conf.h"', '')
    ntc = (SOURCE / 'SH367309_Func.c').read_text(encoding='utf-8')
    ntc = ntc[ntc.index('const UINT16 iSheldTemp_10K_NTC'):ntc.index('// 前26个')]
    startup = function((SOURCE / 'I2C_AFE1.c').read_text(encoding='utf-8'), 'void InitAFE1(void)')
    app = function((SOURCE / 'SH367309_Func.c').read_text(encoding='utf-8'), 'UINT8 App_SH367309(void)')
    permit_stub = '''UINT8 MosStartup_ApplyInitialState(void) {
        ++startup_calls; if(permit_fail || AFE_PARAM_WRITE_Flag) return 0;
        permits=heat_latched ? 0 : 3; return 1;
    }\n'''
    run_c('afe', ADC_STUBS + AFE_STUBS + afe_header +
          'UINT8 MosStartup_ApplyInitialState(void);\n' + ntc + afe_prefix + afe_source +
          permit_stub + startup + app + AFE_TESTS, output, env, compiler)
    rtc = function((SOURCE / 'rtc_sleep_afe_sh367309.c').read_text(encoding='utf-8'),
                   'UINT8 RtcSleep_AfePortUpdateRtcData(')
    restore = function((SOURCE / 'conf/conf.c').read_text(encoding='utf-8'),
                       'void InitRunAfterStopWakeup(')
    run_c('rtc', ADC_STUBS + RTC_STUBS + rtc + restore + RTC_TESTS, output, env, compiler)
    print(f'Host evidence only; files: {output}')


if __name__ == '__main__':
    main()
