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
    assert(!ADC_TakeSample(ADC_INPUT_MOS,&raw));
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
    assert(!ADC_TakeSample(ADC_INPUT_MOS,&raw));
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
    print(f'Host evidence only; files: {output}')


if __name__ == '__main__':
    main()
