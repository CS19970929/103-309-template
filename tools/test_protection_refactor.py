"""Run real ADC/protection C against host hardware stubs; outputs stay in user temp."""
import os
from pathlib import Path
import subprocess
import tempfile

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
#include <stdint.h>
#include <stdio.h>
#include <string.h>
typedef uint8_t UINT8; typedef uint16_t UINT16; typedef uint32_t UINT32;
typedef int32_t INT32;
typedef struct { int unused; } GPIO_TypeDef;
typedef struct { int GPIO_Mode, GPIO_Pin; } GPIO_InitTypeDef;
typedef struct { int ADC_ExternalTrigConv, ADC_NbrOfChannel; } ADC_InitTypeDef;
static GPIO_TypeDef gpio_b;
#define GPIO_ADC_NMOS (&gpio_b)
#define PIN_ADC_NMOS 2
#define RCC_APB2Periph_GPIOB 1
#define RCC_APB2Periph_ADC1 2
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
static void GPIO_Init(GPIO_TypeDef *p,GPIO_InitTypeDef *cfg) {}
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
    start_sweep(); assert(selected_channel==9);
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
    tick=0; Fault_PauseMeasurements();
}
int main(void) {
    unsigned n; int i;
    /* Every combination of AFE directional faults, both software latches,
       MOS heat and reverse current. Expected outcomes come from safety rules. */
    for(n=0;n<32768;++n) {
        unsigned charge_fault, discharge_fault, reverse_charge, reverse_discharge, heat;
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
        charge_fault=((n&15)!=0)||((n>>9)&1);
        discharge_fault=((n&0x1f0)!=0)||((n>>10)&1);
        if(!(n&0x4000)) reverse_charge=reverse_discharge=0;
        assert(Fault_ApplyMosPermits(1,1));
        assert(((hardware_conf>>4)&1)==!(heat || (charge_fault && !reverse_discharge)));
        assert(((hardware_conf>>5)&1)==!(heat || (discharge_fault && !reverse_charge)));
    }
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
    puts("PASS protection: 32768 combinations, independent latches, timed recovery, one record, CONF failures/retry");
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
static UINT8 EEPROM_SaveRWParametersToFlash(void) { ++save_calls; return (UINT8)!save_fails; }
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
    run_c('adc', ADC_STUBS + (SOURCE / 'ADC.h').read_text() + adc + ADC_TESTS, output, env, compiler)
    upper = (SOURCE / 'Sci_Upper.h').read_text(encoding='utf-8', errors='replace')
    # Use only the real protocol fault bit layout, not the rest of the report.
    bits = upper[upper.index('struct MDLCHGFAULT_BITS'):
                 upper.index('};', upper.index('union  MDLCHGFAULT_REG')) + 2]
    driver = (SOURCE / 'SH367309_Func.c').read_text(encoding='utf-8')
    driver = ''.join(function(driver, sig) for sig in (
        'static UINT8 SH367309_WriteConfBits(', 'UINT8 SH367309_SetMosPermits('))
    fault = (SOURCE / 'Fault.c').read_text(encoding='utf-8').replace('#include "main.h"', '')
    conversion = function((SOURCE / 'PubFunc.c').read_text(encoding='utf-8'), 'UINT16 GetEndValue(')
    conversion += function((SOURCE / 'DataDeal.c').read_text(encoding='utf-8'), 'void DataLoad_MosTemperature(')
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
    print(f'Host evidence only; files: {output}')


if __name__ == '__main__':
    main()
