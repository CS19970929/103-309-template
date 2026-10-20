"""Execute actual bounded ADC init and nonblocking RTC acquisition helpers."""
from test_current_calibration import function_source
from test_sleep import read
from test_low_power_policy import run_programs

STUBS = r'''
#include <stdint.h>
#include <stdio.h>
typedef struct { unsigned ADC_Mode,ADC_ScanConvMode,ADC_ContinuousConvMode,
 ADC_ExternalTrigConv,ADC_DataAlign,ADC_NbrOfChannel; } ADC_InitTypeDef;
#define ADC1 1
#define ADC_Mode_Independent 0
#define ENABLE 1
#define DISABLE 0
#define RESET 0
#define ADC_ExternalTrigConv_T2_CC2 0
#define ADC_DataAlign_Right 0
#define AD_Used_amount 1
#define RCC_APB2Periph_ADC1 1
#define RCC_PCLK2_Div8 0
#define ADC_Channel_1 1
#define ADC_SampleTime_55Cycles5 0
#define DMA1_FLAG_TC1 1
#define ADC_TEMP_MOS1 0
#define LENGTH_TBLTEMP_PORT_10K 56
#define ERROR_ADC 1
#define ADC_CAL_POLL_LIMIT 100000UL
#define ADC_BIAS_SETTLE_MS 5U
#define ADC_RTC_SAMPLE_COUNT 4U
static uint32_t s_adc_sample_start,s_adc_sample_sum,now,reset_polls,cal_polls;
static uint8_t s_adc_sample_count,s_adc_settled,stuck_reset,stuck_cal,tc,errors;
static uint16_t g_u16ADCValFilter[1], iSheldTemp_10K[56];
static int32_t g_i32ADCResult[1];
static void RCC_APB2PeriphClockCmd(int a,int b){(void)a;(void)b;}
static void ADC_DeInit(int a){(void)a;}
static void ADC_StructInit(ADC_InitTypeDef *p){(void)p;}
static void ADC_Init(int a,ADC_InitTypeDef *p){(void)a;(void)p;}
static void RCC_ADCCLKConfig(int a){(void)a;}
static void ADC_RegularChannelConfig(int a,int b,int c,int d){(void)a;(void)b;(void)c;(void)d;}
static void ADC_Cmd(int a,int b){(void)a;(void)b;}
static void ADC_DMACmd(int a,int b){(void)a;(void)b;}
static void ADC_ExternalTrigConvCmd(int a,int b){(void)a;(void)b;}
static void ADC_ResetCalibration(int a){(void)a;}
static void ADC_StartCalibration(int a){(void)a;}
static int ADC_GetResetCalibrationStatus(int a){(void)a; ++reset_polls; return stuck_reset;}
static int ADC_GetCalibrationStatus(int a){(void)a; ++cal_polls; return stuck_cal;}
static uint32_t System_GetUptimeMs(void){return now;}
static void DMA_ClearFlag(int a){(void)a;tc=0;}
static int DMA_GetFlagStatus(int a){(void)a;return tc;}
static int GetEndValue(uint16_t *a,int b,uint16_t c){(void)a;(void)b;return c;}
static void System_ERROR_UserCallback(int a){(void)a;++errors;}
'''

TESTS = r'''
int main(void){
    if(!ADC_InitHardwareBounded()) return 1;
    stuck_reset=1;reset_polls=0;
    if(ADC_InitHardwareBounded() || reset_polls!=ADC_CAL_POLL_LIMIT) return 2;
    stuck_reset=0;stuck_cal=1;cal_polls=0;
    if(ADC_InitHardwareBounded() || cal_polls!=ADC_CAL_POLL_LIMIT) return 3;
    now=4;tc=1;if(ADC_PollRtcSample()!=0 || !tc) return 4;
    now=5;if(ADC_PollRtcSample()!=0 || tc) return 5;
    for(unsigned i=0;i<4;i++){
        tc=1;g_u16ADCValFilter[0]=1000;
        if(ADC_PollRtcSample()!=(i==3?1:0)) return 6;
    }
    if(g_i32ADCResult[0]!=1000) return 7;
    tc=1;g_u16ADCValFilter[0]=4095;
    if(ADC_PollRtcSample()!=2 || !errors) return 8;
    s_adc_sample_start=UINT32_MAX-2;now=1;s_adc_settled=0;
    if(ADC_PollRtcSample()!=0 || s_adc_settled) return 9;
    now=2;if(ADC_PollRtcSample()!=0 || !s_adc_settled) return 10;
    puts("PASS: calibration failures bounded; bias settling, fresh samples, invalid ADC and tick wrap");return 0;
}
'''

if __name__ == "__main__":
    adc = read("ADC.c")
    run_programs({"adc": STUBS + function_source(adc, "ADC_InitHardwareBounded")
                  + function_source(adc, "ADC_PollRtcSample") + TESTS})
