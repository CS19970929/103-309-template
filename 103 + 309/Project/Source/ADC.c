#include "main.h"

__IO UINT16 g_u16ADCValFilter[ADC_NUM]; // 这个位数不能改

INT32 g_u32ADCValFilter2[ADC_NUM]; // ADC数据缓存2，问题解决了，原来是UINT32，在计算过程出错了！
                                   // 不能改UINT32
INT32 g_i32ADCResult[ADC_NUM];     // ADC结果保存
static UINT32 s_u32AnlogCalLast10msTick = 0U;
#define TYPEC_CUR_ZERO_CONFIRM_CNT ((UINT8)3)

UINT16 g_u16IoutOffsetAD;
UINT16 g_u16TypeCOutCurrent_mA;
UINT16 g_u16TypeCOutCurrent_A10;
UINT16 g_u16TypeCOutOffsetAD;
UINT16 g_u16TypeCOutStableAD;
UINT16 g_u16TypeCOutDelta_mV;
UINT16 g_u16VbcStableAD;
UINT16 g_u16VbcAdc_mV;
UINT32 g_u32Vbat_mV;
UINT16 gu16_BusCurr_CHG; // legacy mirror, A*10
UINT16 gu16_BusCurr_DSG; // legacy mirror, A*10

// 12位，4096最大
const UINT16 iSheldTemp_10K[LENGTH_TBLTEMP_PORT_10K] = {
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

// 030和103设置一致
void InitADC_DMA(void)
{
    DMA_InitTypeDef DMA_InitStruct;
    // NVIC_InitTypeDef  		 NVIC_InitStructure;
    RCC_AHBPeriphClockCmd(RCC_AHBPeriph_DMA1, ENABLE); // 开启DMA1外设时钟，用于读取ADC1

    /*
    //DMA中断配置
    NVIC_InitStructure.NVIC_IRQChannel = DMA1_Channel1_IRQn;       //选择DMA1通道中断
    NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;                //中断使能
    NVIC_InitStructure.NVIC_IRQChannelPriority = 0;                //优先级设为0
    NVIC_Init(&NVIC_InitStructure);
    DMA_ITConfig(DMA1_Channel1, DMA_IT_TC, ENABLE);		//使能DMA中断，后面看看是否需要，个人感觉不需要
    */

    // DMA初始化
    // 按道理SYSCFG_CFGR1的ADC_DMA_RMP位置为0，ADC才和Channel1连在一起，而1时和Channel2连在一起(ADC要么Channel1要么2)
    // 而这个东西我没配置过，reset值为0，所以不用管
    DMA_DeInit(DMA1_Channel1);                                               // 选择频道
    DMA_StructInit(&DMA_InitStruct);                                         // 初始化DMA结构体
    DMA_InitStruct.DMA_PeripheralBaseAddr = (UINT32)(&(ADC1->DR));           // 配置外设地址
    DMA_InitStruct.DMA_MemoryBaseAddr = (UINT32)(&g_u16ADCValFilter[0]);     // 设置内存映射地址
    DMA_InitStruct.DMA_DIR = DMA_DIR_PeripheralSRC;                          // 数据传输方向，0：从外设读。1：从存储器读
    DMA_InitStruct.DMA_BufferSize = AD_Used_amount;                          // 传输次数，DMA缓存数组大小设置
    DMA_InitStruct.DMA_PeripheralInc = DMA_PeripheralInc_Disable;            // 外设地址不变，这个不太懂是哪个外设地址
    DMA_InitStruct.DMA_MemoryInc = DMA_MemoryInc_Enable;                     // 内存地址增加
    DMA_InitStruct.DMA_PeripheralDataSize = DMA_PeripheralDataSize_HalfWord; // 外设半字传输16位
    DMA_InitStruct.DMA_MemoryDataSize = DMA_MemoryDataSize_HalfWord;         // 内存半字传输16位
    DMA_InitStruct.DMA_Mode = DMA_Mode_Circular;                             // 循环模式
    DMA_InitStruct.DMA_Priority = DMA_Priority_High;                         // 高优先级，当使用一个DMA通道时，优先级设置不影响
    DMA_InitStruct.DMA_M2M = DMA_M2M_Disable;                                // 非内存到内存传输
    DMA_Init(DMA1_Channel1, &DMA_InitStruct);
    DMA_Cmd(DMA1_Channel1, ENABLE);
}

// 初始化PC5，则会出现485串口2没法通讯的情况，所以不初始化PC5，实际上又不影响采样
// 这是个BUG，后续观察
void InitADC_GPIO(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;
    // RCC_APB2PeriphClockCmd(RCC_APB2Periph_AFIO,ENABLE);

    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AIN;

    GPIO_InitStructure.GPIO_Pin = PIN_ADC_VBUS | PIN_ADC_CUR;
    GPIO_Init(GPIOA, &GPIO_InitStructure);

    GPIO_InitStructure.GPIO_Pin = PIN_ADC_NMOS;
    GPIO_Init(GPIOB, &GPIO_InitStructure);

#if 0 // 无效
	GPIO_PinRemapConfig(GPIO_PartialRemap1_TIM2, ENABLE);		//部分重映射1——CH2/PB3
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_3;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_10MHz;
	GPIO_Init(GPIOB, &GPIO_InitStructure);
#endif
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
void InitADC_ADC1(void)
{
    ADC_InitTypeDef ADC_InitStruct;
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_ADC1, ENABLE); // 开启ADC1外设时钟

    // ADC初始化
    ADC_DeInit(ADC1);                // ADC恢复默认设置
    ADC_StructInit(&ADC_InitStruct); // 初始化ADC结构体

    ADC_InitStruct.ADC_Mode = ADC_Mode_Independent;                    // 独立模式
    ADC_InitStruct.ADC_ScanConvMode = ENABLE;                          // 扫描模式
    ADC_InitStruct.ADC_ContinuousConvMode = DISABLE;                   // 连续转换模式
    ADC_InitStruct.ADC_ExternalTrigConv = ADC_ExternalTrigConv_T2_CC2; // T2_CC2触发
    ADC_InitStruct.ADC_DataAlign = ADC_DataAlign_Right;                // 转换结果右对齐
    ADC_InitStruct.ADC_NbrOfChannel = AD_Used_amount;                  // 转换通道个数
    ADC_Init(ADC1, &ADC_InitStruct);                                   // 初始化ADC

    RCC_ADCCLKConfig(RCC_PCLK2_Div8); // 配置ADC时钟PCLK2的8分频，即9MHz

    ADC_RegularChannelConfig(ADC1, ADC_Channel_9, 1, ADC_SampleTime_239Cycles5); // PB1: GPIO_ADC_NMOS
    ADC_RegularChannelConfig(ADC1, ADC_Channel_2, 2, ADC_SampleTime_55Cycles5);  // PA2: GPIO_ADC_CUR
    ADC_RegularChannelConfig(ADC1, ADC_Channel_1, 3, ADC_SampleTime_239Cycles5); // PA1: GPIO_ADC_VBUS

    ADC_Cmd(ADC1, ENABLE);    // 开启ADC，并开始转换
    ADC_DMACmd(ADC1, ENABLE); // 使能ADC DMA 请求

    ADC_ResetCalibration(ADC1); // 初始化ADC 校准寄存器，果然要放在以上两句话后面才行
    while (ADC_GetResetCalibrationStatus(ADC1))
        ;                       // 等待校准寄存器初始化完成
    ADC_StartCalibration(ADC1); // ADC开始校准
    while (ADC_GetCalibrationStatus(ADC1))
        ; // 等待校准完成

    // ADC_SoftwareStartConvCmd(ADC1, ENABLE);		//由于没有采用外部触发，所以使用软件触发ADC转换
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

static UINT8 ADC_IsTypeCZeroSample(UINT32 ad_value)
{
    return (UINT8)(ad_value <= (UINT32)AD_CurZeroDeadband);
}

static void ADC_ClearTypeCOutCurrent(void)
{
    g_u16TypeCOutCurrent_mA = 0;
    g_u16TypeCOutCurrent_A10 = 0;
    g_u16TypeCOutDelta_mV = 0;
    g_u32ADCValFilter2[ADC_CURR] = 0;
    g_i32ADCResult[ADC_CURR] = 0;
    gu16_BusCurr_CHG = 0;
    gu16_BusCurr_DSG = 0;
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
    return g_u32Vbat_mV;
}

void ADC_Current_Smooth(void)
{
    static UINT8 su8_ADcnt = 0;
    static UINT8 su8_ZeroCnt = 0;
    UINT32 u32TypeCAdAvg = 0;

    g_u16TypeCOutOffsetAD = 0;
    g_u16IoutOffsetAD = 0;

    if (ADC_IsTypeCZeroSample((UINT32)g_u16ADCValFilter[ADC_CUR_AMP]))
    {
        if (++su8_ZeroCnt >= TYPEC_CUR_ZERO_CONFIRM_CNT)
        {
            su8_ZeroCnt = TYPEC_CUR_ZERO_CONFIRM_CNT;
            su8_ADcnt = 0;
            g_u32ADCValFilter2[ADC_CUR_AMP] = 0;
            g_u16TypeCOutStableAD = 0;
            ADC_ClearTypeCOutCurrent();
            return;
        }
    }
    else
    {
        su8_ZeroCnt = 0;
    }

    if (su8_ADcnt++ < AD_CalNum_Cur)
    {
        g_u32ADCValFilter2[ADC_CUR_AMP] += g_u16ADCValFilter[ADC_CUR_AMP];
    }
    else
    {
        su8_ADcnt = 0;
        u32TypeCAdAvg = g_u32ADCValFilter2[ADC_CUR_AMP] >> AD_CalNum_Cur_2;
        g_u32ADCValFilter2[ADC_CUR_AMP] = 0;
        g_u16TypeCOutStableAD = ADC_LimitU16(u32TypeCAdAvg);

        if (u32TypeCAdAvg <= (UINT32)AD_CurZeroDeadband)
        {
            ADC_ClearTypeCOutCurrent();
            return;
        }

        g_u16TypeCOutDelta_mV = ADC_TypeCAdToMilliVolt(u32TypeCAdAvg);
        g_u16TypeCOutCurrent_mA = ADC_TypeCDeltaMvToMilliAmp(g_u16TypeCOutDelta_mV);
        g_u16TypeCOutCurrent_A10 = (UINT16)(((UINT32)g_u16TypeCOutCurrent_mA + 50U) / 100U);
        g_u32ADCValFilter2[ADC_CURR] = g_u16TypeCOutCurrent_A10;
        g_i32ADCResult[ADC_CURR] = g_u16TypeCOutCurrent_A10;
        gu16_BusCurr_CHG = 0;
        gu16_BusCurr_DSG = g_u16TypeCOutCurrent_A10;
    }
}

void ADC_TTC(void)
{
    INT32 t_i32temp = 0;

    //-------------MOS1温度(+40)-------------
    t_i32temp = (INT32)g_u16ADCValFilter[ADC_TEMP_MOS1]; // 读取AD值
    t_i32temp = GetEndValue(iSheldTemp_10K, (UINT16)LENGTH_TBLTEMP_PORT_10K, (UINT16)t_i32temp);
    g_u32ADCValFilter2[ADC_TEMP_MOS1] = (((t_i32temp << 10) - g_u32ADCValFilter2[ADC_TEMP_MOS1]) >> 3) + g_u32ADCValFilter2[ADC_TEMP_MOS1];
    g_i32ADCResult[ADC_TEMP_MOS1] = (UINT16)((g_u32ADCValFilter2[ADC_TEMP_MOS1] + 512) >> 10);
}

void ADC_Vbc(void)
{
    static UINT8 s8ADcnt = 0;
    UINT32 u32AdAvg = 0;
    UINT32 u32VbatCalc_mV = 0;

    if (s8ADcnt++ < AD_CalNum)
    {
        g_u32ADCValFilter2[ADC_VBC] += (UINT32)g_u16ADCValFilter[ADC_VBC];
    }
    else
    {
        s8ADcnt = 0;
        u32AdAvg = (UINT32)g_u32ADCValFilter2[ADC_VBC] >> AD_CalNum_2;
        g_u32ADCValFilter2[ADC_VBC] = 0;

        g_u16VbcStableAD = ADC_LimitU16(u32AdAvg);
        g_u16VbcAdc_mV = ADC_VbcAdToMilliVolt(u32AdAvg);
        u32VbatCalc_mV = ADC_VbcAdcMvToBatteryMv(g_u16VbcAdc_mV);

        g_i32ADCResult[ADC_VBC] = (((INT32)u32VbatCalc_mV - g_i32ADCResult[ADC_VBC]) >> 3)
                                 + g_i32ADCResult[ADC_VBC];
        if (g_i32ADCResult[ADC_VBC] < 0)
        {
            g_i32ADCResult[ADC_VBC] = 0;
        }
        g_u32Vbat_mV = (UINT32)g_i32ADCResult[ADC_VBC];
    }
}

// VDDA和VSSA为AD采样专门供电，VREF+和VREF-为AD采样的参加电压，不需要再配置了(所以会发现没相关语句配置)
// 关于那个表，因为为12位分辨率，所以最大输入为4096。
void InitADC(void)
{
    UINT8 i;

    for (i = 0; i < ADC_NUM; i++)
    {
        g_u16ADCValFilter[i] = 0;
        g_i32ADCResult[i] = 0;
        g_u32ADCValFilter2[i] = 0;
    }

    ADC_ClearTypeCOutCurrent();
    g_u16TypeCOutStableAD = 0;
    g_u16VbcStableAD = 0;
    g_u16VbcAdc_mV = 0;
    g_u32Vbat_mV = 0;
    ADC_ResetAnlogCalSchedule();

    InitADC_GPIO();
    InitADC_TIMER();
    InitADC_DMA();
    InitADC_ADC1();
}

// 延时类型初始化是不需要return的
void App_AnlogCal(void)
{
    UINT32 u32Now10msTick;
    UINT32 u32Elapsed10msTick;

    u32Now10msTick = SysTime_Get10msTickCount();
    u32Elapsed10msTick = u32Now10msTick - s_u32AnlogCalLast10msTick;
    if (0U == u32Elapsed10msTick)
    {
        return;
    }
    s_u32AnlogCalLast10msTick = u32Now10msTick;

    while (u32Elapsed10msTick > 0U)
    {
        ADC_TTC();
        ADC_Vbc();
        ADC_Current_Smooth();
        u32Elapsed10msTick--;
    }
    g_u16TypeCOutCurrent_mA = sys_time.typc_curr;
    g_stCellInfoReport.u16VCell[31] = g_u16TypeCOutCurrent_mA;
    g_u16TypeCOutCurrent_A10 = (UINT16)(((UINT32)g_u16TypeCOutCurrent_mA + 50U) / 100U);
}
