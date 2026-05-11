#include "main.h"

__IO UINT16 g_u16ADCValFilter[ADC_NUM];
INT32 g_u32ADCValFilter2[ADC_NUM];
INT32 g_i32ADCResult[ADC_NUM];

static UINT32 s_u32AnlogCalLast10msTick = 0U;

UINT16 g_u16VbcStableAD;
UINT16 g_u16VbcAdc_mV;
UINT32 g_u32Vbat_mV;

const UINT16 iSheldTemp_10K[LENGTH_TBLTEMP_PORT_10K] = {
    3771, 100,
    3683, 150,
    3580, 200,
    3460, 250,
    3323, 300,
    3169, 350,
    3004, 400,
    2820, 450,
    2633, 500,
    2437, 550,
    2241, 600,
    2048, 650,
    1859, 700,
    1679, 750,
    1509, 800,
    1351, 850,
    1204, 900,
    1073, 950,
    953, 1000,
    845, 1050,
    749, 1100,
    664, 1150,
    588, 1200,
    522, 1250,
    463, 1300,
    411, 1350,
    366, 1400,
    326, 1450,
};

void InitADC_DMA(void)
{
    DMA_InitTypeDef dma_init;

    RCC_AHBPeriphClockCmd(RCC_AHBPeriph_DMA1, ENABLE);

    DMA_DeInit(DMA1_Channel1);
    DMA_StructInit(&dma_init);
    dma_init.DMA_PeripheralBaseAddr = (UINT32)(&(ADC1->DR));
    dma_init.DMA_MemoryBaseAddr = (UINT32)(&g_u16ADCValFilter[0]);
    dma_init.DMA_DIR = DMA_DIR_PeripheralSRC;
    dma_init.DMA_BufferSize = AD_Used_amount;
    dma_init.DMA_PeripheralInc = DMA_PeripheralInc_Disable;
    dma_init.DMA_MemoryInc = DMA_MemoryInc_Enable;
    dma_init.DMA_PeripheralDataSize = DMA_PeripheralDataSize_HalfWord;
    dma_init.DMA_MemoryDataSize = DMA_MemoryDataSize_HalfWord;
    dma_init.DMA_Mode = DMA_Mode_Circular;
    dma_init.DMA_Priority = DMA_Priority_High;
    dma_init.DMA_M2M = DMA_M2M_Disable;
    DMA_Init(DMA1_Channel1, &dma_init);
    DMA_Cmd(DMA1_Channel1, ENABLE);
}

void InitADC_GPIO(void)
{
    GPIO_InitTypeDef gpio_init;

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA | RCC_APB2Periph_GPIOB, ENABLE);
    gpio_init.GPIO_Mode = GPIO_Mode_AIN;

    gpio_init.GPIO_Pin = PIN_ADC_VBUS;
    GPIO_Init(GPIO_ADC_VBUS, &gpio_init);

    gpio_init.GPIO_Pin = PIN_ADC_NMOS;
    GPIO_Init(GPIO_ADC_NMOS, &gpio_init);
}

void InitADC_TIMER(void)
{
    TIM_TimeBaseInitTypeDef timer_base;
    TIM_OCInitTypeDef timer_oc;
    UINT32 timer_div;

    RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM2, ENABLE);
    TIM_Cmd(TIM2, DISABLE);

    timer_div = SystemCoreClock / 100000U;
    if (timer_div == 0U)
    {
        timer_div = 1U;
    }
    if (timer_div > 0x10000U)
    {
        timer_div = 0x10000U;
    }

    timer_base.TIM_Prescaler = (UINT16)(timer_div - 1U);
    timer_base.TIM_Period = 999U;
    timer_base.TIM_CounterMode = TIM_CounterMode_Up;
    timer_base.TIM_ClockDivision = TIM_CKD_DIV1;
    timer_base.TIM_RepetitionCounter = 0x00;
    TIM_TimeBaseInit(TIM2, &timer_base);

    timer_oc.TIM_OCMode = TIM_OCMode_PWM1;
    timer_oc.TIM_OutputState = TIM_OutputState_Enable;
    timer_oc.TIM_Pulse = 1U;
    timer_oc.TIM_OCPolarity = TIM_OCPolarity_Low;
    TIM_OC2Init(TIM2, &timer_oc);
    TIM_Cmd(TIM2, ENABLE);
}

void InitADC_ADC1(void)
{
    ADC_InitTypeDef adc_init;

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_ADC1, ENABLE);
    ADC_DeInit(ADC1);
    ADC_StructInit(&adc_init);

    adc_init.ADC_Mode = ADC_Mode_Independent;
    adc_init.ADC_ScanConvMode = ENABLE;
    adc_init.ADC_ContinuousConvMode = DISABLE;
    adc_init.ADC_ExternalTrigConv = ADC_ExternalTrigConv_T2_CC2;
    adc_init.ADC_DataAlign = ADC_DataAlign_Right;
    adc_init.ADC_NbrOfChannel = AD_Used_amount;
    ADC_Init(ADC1, &adc_init);

    RCC_ADCCLKConfig(RCC_PCLK2_Div8);

    ADC_RegularChannelConfig(ADC1, ADC_Channel_9, 1, ADC_SampleTime_239Cycles5); // PB1: NMOS temperature
    ADC_RegularChannelConfig(ADC1, ADC_Channel_1, 2, ADC_SampleTime_239Cycles5); // PA1: VBUS divider

    ADC_Cmd(ADC1, ENABLE);
    ADC_DMACmd(ADC1, ENABLE);

    ADC_ResetCalibration(ADC1);
    while (ADC_GetResetCalibrationStatus(ADC1))
        ;
    ADC_StartCalibration(ADC1);
    while (ADC_GetCalibrationStatus(ADC1))
        ;

    ADC_ExternalTrigConvCmd(ADC1, ENABLE);
}

void ADC_ResetAnlogCalSchedule(void)
{
    s_u32AnlogCalLast10msTick = 0U;
}

void ADC_StopForLowPower(void)
{
    RCC_AHBPeriphClockCmd(RCC_AHBPeriph_DMA1, ENABLE);
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM2, ENABLE);
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_ADC1, ENABLE);

    TIM_Cmd(TIM2, DISABLE);
    ADC_ExternalTrigConvCmd(ADC1, DISABLE);
    ADC_DMACmd(ADC1, DISABLE);
    ADC_Cmd(ADC1, DISABLE);
    DMA_Cmd(DMA1_Channel1, DISABLE);
    DMA_DeInit(DMA1_Channel1);
    ADC_DeInit(ADC1);

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_ADC1, DISABLE);
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM2, DISABLE);
    RCC_AHBPeriphClockCmd(RCC_AHBPeriph_DMA1, DISABLE);
}

static UINT16 ADC_LimitU16(UINT32 value)
{
    if (value > 0xFFFFU)
    {
        return 0xFFFFU;
    }

    return (UINT16)value;
}

static UINT16 ADC_VbcAdToMilliVolt(UINT32 ad_value)
{
    UINT32 adc_mV;

    adc_mV = (ad_value * (UINT32)VBC_ADC_VDDA_MV + 2048U) / 4096U;

    return ADC_LimitU16(adc_mV);
}

static UINT32 ADC_VbcAdcMvToBatteryMv(UINT16 adc_mV)
{
    UINT32 divider_top = (UINT32)VBC_DIVIDER_RTOP_KOHM;
    UINT32 divider_bottom = (UINT32)VBC_DIVIDER_RBOTTOM_KOHM;

    if (divider_bottom == 0U)
    {
        return 0U;
    }

    return ((UINT32)adc_mV * (divider_top + divider_bottom) + (divider_bottom / 2U)) / divider_bottom;
}

UINT32 ADC_GetVbatMilliVolt(void)
{
    return g_u32Vbat_mV;
}

void ADC_TTC(void)
{
    INT32 temp;

    temp = (INT32)g_u16ADCValFilter[ADC_TEMP_MOS1];
    temp = GetEndValue(iSheldTemp_10K, (UINT16)LENGTH_TBLTEMP_PORT_10K, (UINT16)temp);
    g_u32ADCValFilter2[ADC_TEMP_MOS1] = (((temp << 10) - g_u32ADCValFilter2[ADC_TEMP_MOS1]) >> 3) + g_u32ADCValFilter2[ADC_TEMP_MOS1];
    g_i32ADCResult[ADC_TEMP_MOS1] = (UINT16)((g_u32ADCValFilter2[ADC_TEMP_MOS1] + 512) >> 10);
}

void ADC_Vbc(void)
{
    static UINT8 s8ADcnt = 0;
    UINT32 ad_avg;
    UINT32 vbat_mV;

    if (s8ADcnt++ < AD_CalNum)
    {
        g_u32ADCValFilter2[ADC_VBC] += (UINT32)g_u16ADCValFilter[ADC_VBC];
    }
    else
    {
        s8ADcnt = 0;
        ad_avg = (UINT32)g_u32ADCValFilter2[ADC_VBC] >> AD_CalNum_2;
        g_u32ADCValFilter2[ADC_VBC] = 0;

        g_u16VbcStableAD = ADC_LimitU16(ad_avg);
        g_u16VbcAdc_mV = ADC_VbcAdToMilliVolt(ad_avg);
        vbat_mV = ADC_VbcAdcMvToBatteryMv(g_u16VbcAdc_mV);

        g_i32ADCResult[ADC_VBC] = (((INT32)vbat_mV - g_i32ADCResult[ADC_VBC]) >> 3) + g_i32ADCResult[ADC_VBC];
        if (g_i32ADCResult[ADC_VBC] < 0)
        {
            g_i32ADCResult[ADC_VBC] = 0;
        }
        g_u32Vbat_mV = (UINT32)g_i32ADCResult[ADC_VBC];
    }
}

void InitADC(void)
{
    UINT8 i;

    for (i = 0; i < ADC_NUM; i++)
    {
        g_u16ADCValFilter[i] = 0;
        g_i32ADCResult[i] = 0;
        g_u32ADCValFilter2[i] = 0;
    }

    g_u16VbcStableAD = 0;
    g_u16VbcAdc_mV = 0;
    g_u32Vbat_mV = 0;
    ADC_ResetAnlogCalSchedule();

    InitADC_GPIO();
    InitADC_TIMER();
    InitADC_DMA();
    InitADC_ADC1();
}

void App_AnlogCal(void)
{
    UINT32 now_10ms_tick;
    UINT32 elapsed_10ms_tick;

    now_10ms_tick = SysTime_Get10msTickCount();
    elapsed_10ms_tick = now_10ms_tick - s_u32AnlogCalLast10msTick;
    if (elapsed_10ms_tick == 0U)
    {
        return;
    }
    s_u32AnlogCalLast10msTick = now_10ms_tick;

    while (elapsed_10ms_tick > 0U)
    {
        ADC_TTC();
        ADC_Vbc();
        elapsed_10ms_tick--;
    }
}
