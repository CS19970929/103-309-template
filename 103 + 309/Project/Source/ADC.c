#include "main.h"

typedef struct
{
    __IO UINT16 raw[ADC_NUM];
    INT32 filt[ADC_NUM];
    INT32 result[ADC_NUM];
    UINT32 last;
    UINT32 vbat;
    UINT16 typec;
    UINT8 curCnt;
    UINT8 zeroCnt;
    UINT8 vbcCnt;
    UINT8 reserved;
} ADC_RUNTIME;

static ADC_RUNTIME s_adc;
#define TYPEC_CUR_ZERO_CONFIRM_CNT ((UINT8)3)
#define ADC_CALIBRATION_WAIT_LOOP ((UINT32)100000U)
#define ADC_ANALOG_CAL_MAX_CATCHUP_TICKS ((UINT32)10U)


// 12佝，4096��
static const UINT16 iSheldTemp_10K[LENGTH_TBLTEMP_PORT_10K] = {
    // AD		(Temp+40)*10
    3771,
    100, //-30
    3683,
    150, //-25
    3580,
    200, //-20
    3460,
    250, //-15
    3323,
    300, //-10
    3169,
    350, //-5
    3004,
    400, // 0
    2820,
    450, // 5
    2633,
    500, // 10
    2437,
    550, // 15
    2241,
    600, // 20
    2048,
    650, // 25
    1859,
    700, // 30
    1679,
    750, // 35
    1509,
    800, // 40
    1351,
    850, // 45
    1204,
    900, // 50
    1073,
    950, // 55
    953,
    1000, // 60
    845,
    1050, // 65
    749,
    1100, // 70
    664,
    1150, // 75
    588,
    1200, // 80
    522,
    1250, // 85
    463,
    1300, // 90
    411,
    1350, // 95
    366,
    1400, // 100
    326,
    1450, // 105

};

// 030�103设置��
void InitADC_DMA(void)
{
    DMA_InitTypeDef DMA_InitStruct;
    // NVIC_InitTypeDef  		 NVIC_InitStructure;
    RCC_AHBPeriphClockCmd(RCC_AHBPeriph_DMA1, ENABLE); // �坯DMA1外�时钟，用于读坖ADC1

    /*
    //DMA��酝置
    NVIC_InitStructure.NVIC_IRQChannel = DMA1_Channel1_IRQn;       //选择DMA1通靓��
    NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;                //��使能
    NVIC_InitStructure.NVIC_IRQChannelPriority = 0;                //优先级�为0
    NVIC_Init(&NVIC_InitStructure);
    DMA_ITConfig(DMA1_Channel1, DMA_IT_TC, ENABLE);		//使能DMA��，坎面看看是坦需覝，�人感觉丝��
    */

    // DMA初�化
    // 按靓睆SYSCFG_CFGR1的ADC_DMA_RMP佝置�0，ADC扝和Channel1连在�起，�1时和Channel2连在��(ADC覝么Channel1覝么2)
    // 而这�东西我没酝置过，reset值为0，所以丝用�
    DMA_DeInit(DMA1_Channel1);                                               // 选择频靓
    DMA_StructInit(&DMA_InitStruct);                                         // 初�化DMA结构�
    DMA_InitStruct.DMA_PeripheralBaseAddr = (UINT32)(&(ADC1->DR));           // 酝置外�地�
    DMA_InitStruct.DMA_MemoryBaseAddr = (UINT32)(&s_adc.raw[0]);     // 设置内存映射地址
    DMA_InitStruct.DMA_DIR = DMA_DIR_PeripheralSRC;                          // 数杮传输方坑�0：从外���1：从存储器�
    DMA_InitStruct.DMA_BufferSize = AD_Used_amount;                          // 传输次数，DMA缓存数组大尝设置
    DMA_InitStruct.DMA_PeripheralInc = DMA_PeripheralInc_Disable;            // 外�地�丝坘，这�丝太懂是��外�地�
    DMA_InitStruct.DMA_MemoryInc = DMA_MemoryInc_Enable;                     // 内存地址增加
    DMA_InitStruct.DMA_PeripheralDataSize = DMA_PeripheralDataSize_HalfWord; // 外�坊字传�16�
    DMA_InitStruct.DMA_MemoryDataSize = DMA_MemoryDataSize_HalfWord;         // 内存坊字传输16�
    DMA_InitStruct.DMA_Mode = DMA_Mode_Circular;                             // ��模弝
    DMA_InitStruct.DMA_Priority = DMA_Priority_High;                         // 高优先级，当使用�个DMA通靓时，优先级�置丝影�
    DMA_InitStruct.DMA_M2M = DMA_M2M_Disable;                                // 非内存到内存传输
    DMA_Init(DMA1_Channel1, &DMA_InitStruct);
    DMA_Cmd(DMA1_Channel1, ENABLE);
}

// 初�化PC5，则会出�485串坣2没法通�的情况，�以丝初�化PC5，实际上坈丝影哝采样
// 这是个BUG，坎�观察
void InitADC_GPIO(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;
    // RCC_APB2PeriphClockCmd(RCC_APB2Periph_AFIO,ENABLE);

    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AIN;

    GPIO_InitStructure.GPIO_Pin = PIN_ADC_VBUS | PIN_ADC_CUR;
    GPIO_Init(GPIOA, &GPIO_InitStructure);

    GPIO_InitStructure.GPIO_Pin = PIN_ADC_NMOS;
    GPIO_Init(GPIOB, &GPIO_InitStructure);
}

void InitADC_TIMER(void)
{
    TIM_TimeBaseInitTypeDef TIM_TimeBaseStructure;
    TIM_OCInitTypeDef TIM_OCInitStructure;
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

    TIM_TimeBaseStructure.TIM_Prescaler = (UINT16)(timer_div - 1U);
    TIM_TimeBaseStructure.TIM_Period = 999U;
    TIM_TimeBaseStructure.TIM_CounterMode = TIM_CounterMode_Up;
    TIM_TimeBaseStructure.TIM_ClockDivision = TIM_CKD_DIV1;
    TIM_TimeBaseStructure.TIM_RepetitionCounter = 0x00;

    TIM_TimeBaseInit(TIM2, &TIM_TimeBaseStructure);
    TIM_OCInitStructure.TIM_OCMode = TIM_OCMode_PWM1;
    TIM_OCInitStructure.TIM_OutputState = TIM_OutputState_Enable;
    TIM_OCInitStructure.TIM_Pulse = 1U;
    TIM_OCInitStructure.TIM_OCPolarity = TIM_OCPolarity_Low;

    TIM_OC2Init(TIM2, &TIM_OCInitStructure);
    TIM_Cmd(TIM2, ENABLE);
}
static UINT8 ADC_WaitResetCalibrationDone(void)
{
    UINT32 timeout = ADC_CALIBRATION_WAIT_LOOP;

    while (ADC_GetResetCalibrationStatus(ADC1) != RESET)
    {
        Feed_IWatchDog;
        if (timeout-- == 0U)
        {
            return 0U;
        }
    }

    return 1U;
}

static UINT8 ADC_WaitCalibrationDone(void)
{
    UINT32 timeout = ADC_CALIBRATION_WAIT_LOOP;

    while (ADC_GetCalibrationStatus(ADC1) != RESET)
    {
        Feed_IWatchDog;
        if (timeout-- == 0U)
        {
            return 0U;
        }
    }

    return 1U;
}

void InitADC_ADC1(void)
{
    ADC_InitTypeDef ADC_InitStruct;
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_ADC1, ENABLE); // �坯ADC1外�时�

    // ADC初�化
    ADC_DeInit(ADC1);                // ADC杢�默认�置
    ADC_StructInit(&ADC_InitStruct); // 初�化ADC结构�

    ADC_InitStruct.ADC_Mode = ADC_Mode_Independent;                    // �立模�
    ADC_InitStruct.ADC_ScanConvMode = ENABLE;                          // �杝模�
    ADC_InitStruct.ADC_ContinuousConvMode = DISABLE;                   // 连续�杢模�
    ADC_InitStruct.ADC_ExternalTrigConv = ADC_ExternalTrigConv_T2_CC2; // T2_CC2触坑
    ADC_InitStruct.ADC_DataAlign = ADC_DataAlign_Right;                // �杢结果坳对齝
    ADC_InitStruct.ADC_NbrOfChannel = AD_Used_amount;                  // �杢�靓��
    ADC_Init(ADC1, &ADC_InitStruct);                                   // 初�化ADC

    RCC_ADCCLKConfig(RCC_PCLK2_Div8); // 酝置ADC时钟PCLK2�8分�，�9MHz

    ADC_RegularChannelConfig(ADC1, ADC_Channel_9, 1, ADC_SampleTime_239Cycles5); // PB1: GPIO_ADC_NMOS
    ADC_RegularChannelConfig(ADC1, ADC_Channel_2, 2, ADC_SampleTime_55Cycles5);  // PA2: GPIO_ADC_CUR
    ADC_RegularChannelConfig(ADC1, ADC_Channel_1, 3, ADC_SampleTime_239Cycles5); // PA1: GPIO_ADC_VBUS

    ADC_Cmd(ADC1, ENABLE);    // �坯ADC，并�始转�
    ADC_DMACmd(ADC1, ENABLE); // 使能ADC DMA 请求

    ADC_ResetCalibration(ADC1); // 初�化ADC 校准寄存�，果然�放在以上两坥话坎面扝�
    if (!ADC_WaitResetCalibrationDone())
    {
        ADC_DMACmd(ADC1, DISABLE);
        ADC_Cmd(ADC1, DISABLE);
        System_ERROR_UserCallback(ERROR_ADC);
        return;
    } // 等待校准寄存器初始化完戝
    ADC_StartCalibration(ADC1); // ADC�始校�
    if (!ADC_WaitCalibrationDone())
    {
        ADC_DMACmd(ADC1, DISABLE);
        ADC_Cmd(ADC1, DISABLE);
        System_ERROR_UserCallback(ERROR_ADC);
        return;
    } // 等待校准完戝
    System_ERROR_UserCallback(ERROR_REMOVE_ADC);

    // ADC_SoftwareStartConvCmd(ADC1, ENABLE);		//由于没有采用外部触坑，所以使用软件触坑ADC��
    ADC_ExternalTrigConvCmd(ADC1, ENABLE);
}

void ADC_ResetAnlogCalSchedule(void)
{
    s_adc.last = SysTime_Get10msTickCount();
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

static UINT8 ADC_IsTypeCZeroSample(UINT32 ad_value)
{
    return (UINT8)(ad_value <= (UINT32)AD_CurZeroDeadband);
}

static void ADC_ClearTypeCOutCurrent(void)
{
    s_adc.typec = 0;
    s_adc.filt[ADC_CURR] = 0;
    s_adc.result[ADC_CURR] = 0;
}

static UINT16 ADC_TypeCAdToMilliVolt(UINT32 ad_value)
{
    UINT32 delta_mV;

    delta_mV = (ad_value * (UINT32)TYPEC_CUR_VDDA_MV + 2048U) / 4096U;

    return ADC_LimitU16(delta_mV);
}

static UINT16 ADC_TypeCDeltaMvToMilliAmp(UINT16 delta_mV)
{
    UINT32 current_mA;

    if (TYPEC_CUR_RSENSE_MOHM == 0U)
    {
        return 0;
    }

    current_mA = ((UINT32)delta_mV * 1000U + ((UINT32)TYPEC_CUR_RSENSE_MOHM / 2U)) / (UINT32)TYPEC_CUR_RSENSE_MOHM;

    return ADC_LimitU16(current_mA);
}

static UINT16 ADC_VbcAdToMilliVolt(UINT32 ad_value)
{
    UINT32 adc_mV;

    adc_mV = (ad_value * (UINT32)VBC_ADC_VDDA_MV + 2048U) / 4096U;

    return ADC_LimitU16(adc_mV);
}

static UINT32 ADC_VbcAdcMvToBatteryMv(UINT16 adc_mV)
{
    UINT32 divider_top;
    UINT32 divider_bottom;

    divider_top = (UINT32)VBC_DIVIDER_RTOP_KOHM;
    divider_bottom = (UINT32)VBC_DIVIDER_RBOTTOM_KOHM;
    if (divider_bottom == 0U)
    {
        return 0U;
    }

    return ((UINT32)adc_mV * (divider_top + divider_bottom) + (divider_bottom / 2U)) / divider_bottom;
}

UINT32 ADC_GetVbatMilliVolt(void)
{
    return s_adc.vbat;
}

UINT16 ADC_GetTypeCOutCurrentMilliAmp(void)
{
    if (sys_time.typec_curr_sim)
        return sys_time.typc_curr;

    return s_adc.typec;
}

INT32 ADC_GetResult(UINT8 index)
{
    if (index >= ADC_NUM)
    {
        return 0;
    }

    return s_adc.result[index];
}

UINT16 ADC_GetRaw(UINT8 index)
{
    if (index >= ADC_NUM)
    {
        return 0U;
    }

    return s_adc.raw[index];
}

void ADC_Current_Smooth(void)
{
    UINT32 u32TypeCAdAvg = 0;
    UINT16 typec_delta_mV = 0;
    UINT16 typec_current_A10 = 0;

    if (ADC_IsTypeCZeroSample((UINT32)s_adc.raw[ADC_CUR_AMP]))
    {
        if (++s_adc.zeroCnt >= TYPEC_CUR_ZERO_CONFIRM_CNT)
        {
            s_adc.zeroCnt = TYPEC_CUR_ZERO_CONFIRM_CNT;
            s_adc.curCnt = 0;
            s_adc.filt[ADC_CUR_AMP] = 0;
            ADC_ClearTypeCOutCurrent();
            return;
        }
    }
    else
    {
        s_adc.zeroCnt = 0;
    }

    if (s_adc.curCnt++ < AD_CalNum_Cur)
    {
        s_adc.filt[ADC_CUR_AMP] += s_adc.raw[ADC_CUR_AMP];
    }
    else
    {
        s_adc.curCnt = 0;
        u32TypeCAdAvg = s_adc.filt[ADC_CUR_AMP] >> AD_CalNum_Cur_2;
        s_adc.filt[ADC_CUR_AMP] = 0;

        if (u32TypeCAdAvg <= (UINT32)AD_CurZeroDeadband)
        {
            ADC_ClearTypeCOutCurrent();
            return;
        }

        typec_delta_mV = ADC_TypeCAdToMilliVolt(u32TypeCAdAvg);
        s_adc.typec = ADC_TypeCDeltaMvToMilliAmp(typec_delta_mV);
        typec_current_A10 = (UINT16)(((UINT32)s_adc.typec + 50U) / 100U);
        s_adc.filt[ADC_CURR] = typec_current_A10;
        s_adc.result[ADC_CURR] = typec_current_A10;
    }
}

void ADC_TTC(void)
{
    INT32 t_i32temp = 0;

    //-------------MOS1温度(+40)-------------
    t_i32temp = (INT32)s_adc.raw[ADC_TEMP_MOS1]; // 读坖AD�
    t_i32temp = GetEndValue(iSheldTemp_10K, (UINT16)LENGTH_TBLTEMP_PORT_10K, (UINT16)t_i32temp);
    s_adc.filt[ADC_TEMP_MOS1] = (((t_i32temp << 10) - s_adc.filt[ADC_TEMP_MOS1]) >> 3) + s_adc.filt[ADC_TEMP_MOS1];
    s_adc.result[ADC_TEMP_MOS1] = (UINT16)((s_adc.filt[ADC_TEMP_MOS1] + 512) >> 10);
}

void ADC_Vbc(void)
{
    UINT32 u32AdAvg = 0;
    UINT32 u32VbatCalc_mV = 0;

    if (s_adc.vbcCnt++ < AD_CalNum)
    {
        s_adc.filt[ADC_VBC] += (UINT32)s_adc.raw[ADC_VBC];
    }
    else
    {
        s_adc.vbcCnt = 0;
        u32AdAvg = (UINT32)s_adc.filt[ADC_VBC] >> AD_CalNum_2;
        s_adc.filt[ADC_VBC] = 0;

        u32VbatCalc_mV = ADC_VbcAdcMvToBatteryMv(ADC_VbcAdToMilliVolt(u32AdAvg));

        s_adc.result[ADC_VBC] = (((INT32)u32VbatCalc_mV - s_adc.result[ADC_VBC]) >> 3) + s_adc.result[ADC_VBC];
        if (s_adc.result[ADC_VBC] < 0)
        {
            s_adc.result[ADC_VBC] = 0;
        }
        s_adc.vbat = (UINT32)s_adc.result[ADC_VBC];
    }
}

// VDDA和VSSA为AD采样专门供电，VREF+和VREF-为AD采样的坂加电压，丝需覝冝酝置�(�以会坑现没相关�坥酝�)
// 关于那个�，因为为12佝分辨率，所以最大输入为4096�
void InitADC(void)
{
    UINT8 i;

    for (i = 0; i < ADC_NUM; i++)
    {
        s_adc.raw[i] = 0;
        s_adc.result[i] = 0;
        s_adc.filt[i] = 0;
    }

    ADC_ClearTypeCOutCurrent();
    s_adc.vbat = 0;
    ADC_ResetAnlogCalSchedule();

    InitADC_GPIO();
    InitADC_TIMER();
    InitADC_DMA();
    InitADC_ADC1();
}

void App_AnlogCal(void)
{
    UINT32 u32Now10msTick;
    UINT32 u32Elapsed10msTick;
    UINT32 u32Process10msTick;

    u32Now10msTick = SysTime_Get10msTickCount();
    u32Elapsed10msTick = u32Now10msTick - s_adc.last;
    if (0U == u32Elapsed10msTick)
    {
        return;
    }

    u32Process10msTick = u32Elapsed10msTick;
    if (u32Process10msTick > ADC_ANALOG_CAL_MAX_CATCHUP_TICKS)
    {
        u32Process10msTick = ADC_ANALOG_CAL_MAX_CATCHUP_TICKS;
    }
    s_adc.last += u32Process10msTick;

    while (u32Process10msTick > 0U)
    {
        ADC_TTC();
        ADC_Vbc();
        ADC_Current_Smooth();
        u32Process10msTick--;
    }
}
