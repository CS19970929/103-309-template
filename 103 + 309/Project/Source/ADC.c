#include "main.h"

__IO UINT16 g_u16ADCValFilter[ADC_NUM]; // 这个位数不能改

INT32 g_u32ADCValFilter2[ADC_NUM]; // ADC数据缓存2，问题解决了，原来是UINT32，在计算过程出错了！
								   // 不能改UINT32
INT32 g_i32ADCResult[ADC_NUM]; // ADC结果保存

UINT16 g_u16IoutOffsetAD;
UINT16 gu16_BusCurr_CHG; // A*10
UINT16 gu16_BusCurr_DSG; // A*10

// 12位，4096最大
const UINT16 iSheldTemp_10K[LENGTH_TBLTEMP_PORT_10K] = {
	// AD		(Temp+40)*10
	3771, 100, //-30
	3683, 150, //-25
	3580, 200, //-20
	3460, 250, //-15
	3323, 300, //-10
	3169, 350, //-5
	3004, 400, // 0
	2820, 450, // 5
	2633, 500, // 10
	2437, 550, // 15
	2241, 600, // 20
	2048, 650, // 25
	1859, 700, // 30
	1679, 750, // 35
	1509, 800, // 40
	1351, 850, // 45
	1204, 900, // 50
	1073, 950, // 55
	953, 1000, // 60
	845, 1050, // 65
	749, 1100, // 70
	664, 1150, // 75
	588, 1200, // 80
	522, 1250, // 85
	463, 1300, // 90
	411, 1350, // 95
	366, 1400, // 100
	326, 1450, // 105

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
	DMA_DeInit(DMA1_Channel1);												 // 选择频道
	DMA_StructInit(&DMA_InitStruct);										 // 初始化DMA结构体
	DMA_InitStruct.DMA_PeripheralBaseAddr = (UINT32)(&(ADC1->DR));			 // 配置外设地址
	DMA_InitStruct.DMA_MemoryBaseAddr = (UINT32)(&g_u16ADCValFilter[0]);	 // 设置内存映射地址
	DMA_InitStruct.DMA_DIR = DMA_DIR_PeripheralSRC;							 // 数据传输方向，0：从外设读。1：从存储器读
	DMA_InitStruct.DMA_BufferSize = AD_Used_amount;							 // 传输次数，DMA缓存数组大小设置
	DMA_InitStruct.DMA_PeripheralInc = DMA_PeripheralInc_Disable;			 // 外设地址不变，这个不太懂是哪个外设地址
	DMA_InitStruct.DMA_MemoryInc = DMA_MemoryInc_Enable;					 // 内存地址增加
	DMA_InitStruct.DMA_PeripheralDataSize = DMA_PeripheralDataSize_HalfWord; // 外设半字传输16位
	DMA_InitStruct.DMA_MemoryDataSize = DMA_MemoryDataSize_HalfWord;		 // 内存半字传输16位
	DMA_InitStruct.DMA_Mode = DMA_Mode_Circular;							 // 循环模式
	DMA_InitStruct.DMA_Priority = DMA_Priority_High;						 // 高优先级，当使用一个DMA通道时，优先级设置不影响
	DMA_InitStruct.DMA_M2M = DMA_M2M_Disable;								 // 非内存到内存传输
	DMA_Init(DMA1_Channel1, &DMA_InitStruct);
	DMA_Cmd(DMA1_Channel1, ENABLE);
}

// 初始化PC5，则会出现485串口2没法通讯的情况，所以不初始化PC5，实际上又不影响采样
// 这是个BUG，后续观察
void InitADC_GPIO(void)
{
	GPIO_InitTypeDef GPIO_InitStructure;
	// RCC_APB2PeriphClockCmd(RCC_APB2Periph_AFIO,ENABLE);

	GPIO_InitStructure.GPIO_Pin = PIN_AD_TTC_MOS1;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AIN;
	GPIO_Init(GPIO_AD_TTC_MOS1, &GPIO_InitStructure);

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
	RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM2, ENABLE);
	TIM_Cmd(TIM2, DISABLE); // 先停止TIM2时钟

    {
        RCC_ClocksTypeDef clocks;
        uint32_t timer_hz;
        RCC_GetClocksFreq(&clocks);
        timer_hz = clocks.PCLK1_Frequency;
        if ((RCC->CFGR & RCC_CFGR_PPRE1) != 0U) timer_hz *= 2U;
        TIM_TimeBaseStructure.TIM_Prescaler = (uint16_t)(timer_hz / 1000000U - 1U);
    }
	TIM_TimeBaseStructure.TIM_Period = 999; // 设置在下一个更新事件装入活动的自动重装载寄存器周期的值

	TIM_TimeBaseStructure.TIM_CounterMode = TIM_CounterMode_Up; // 向上计数
	TIM_TimeBaseStructure.TIM_ClockDivision = TIM_CKD_DIV1;		// 时钟分频
	TIM_TimeBaseStructure.TIM_RepetitionCounter = 0x00;			// 溢出指定(+1)次数后产生中断

	TIM_TimeBaseInit(TIM2, &TIM_TimeBaseStructure);
	TIM_OCInitStructure.TIM_OCMode = TIM_OCMode_PWM1;
	TIM_OCInitStructure.TIM_OutputState = TIM_OutputState_Enable;
	TIM_OCInitStructure.TIM_Pulse = 1;
	TIM_OCInitStructure.TIM_OCPolarity = TIM_OCPolarity_Low;

	TIM_OC2Init(TIM2, &TIM_OCInitStructure);
	TIM_Cmd(TIM2, ENABLE);
}

#define ADC_CAL_POLL_LIMIT 100000UL
#define ADC_BIAS_SETTLE_MS 5U
#define ADC_RTC_SAMPLE_COUNT 4U
static uint32_t s_adc_sample_start, s_adc_sample_sum;
static uint8_t s_adc_sample_count, s_adc_settled;

static uint8_t ADC_InitHardwareBounded(void)
{
    ADC_InitTypeDef config;
    uint32_t polls;
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_ADC1, ENABLE);
    ADC_DeInit(ADC1);
    ADC_StructInit(&config);
    config.ADC_Mode = ADC_Mode_Independent;
    config.ADC_ScanConvMode = ENABLE;
    config.ADC_ContinuousConvMode = DISABLE;
    config.ADC_ExternalTrigConv = ADC_ExternalTrigConv_T2_CC2;
    config.ADC_DataAlign = ADC_DataAlign_Right;
    config.ADC_NbrOfChannel = AD_Used_amount;
    ADC_Init(ADC1, &config);
    RCC_ADCCLKConfig(RCC_PCLK2_Div8);
    ADC_RegularChannelConfig(ADC1, ADC_Channel_1, 1, ADC_SampleTime_55Cycles5);
    ADC_Cmd(ADC1, ENABLE);
    ADC_DMACmd(ADC1, ENABLE);
    ADC_ResetCalibration(ADC1);
    polls = ADC_CAL_POLL_LIMIT;
    while (ADC_GetResetCalibrationStatus(ADC1))
        if (--polls == 0U) return 0U;
    ADC_StartCalibration(ADC1);
    polls = ADC_CAL_POLL_LIMIT;
    while (ADC_GetCalibrationStatus(ADC1))
        if (--polls == 0U) return 0U;
    ADC_ExternalTrigConvCmd(ADC1, ENABLE);
    return 1U;
}

void ADC_StopForRtc(void)
{
    TIM_Cmd(TIM2, DISABLE);
    ADC_ExternalTrigConvCmd(ADC1, DISABLE);
    ADC_DMACmd(ADC1, DISABLE);
    DMA_Cmd(DMA1_Channel1, DISABLE);
    ADC_Cmd(ADC1, DISABLE);
}

/* Legacy public entry point retained; all init paths now use a bounded wait. */
void InitADC_ADC1(void)
{
    if (!ADC_InitHardwareBounded())
    {
        ADC_StopForRtc();
        System_ERROR_UserCallback(ERROR_ADC);
    }
}

void ADC_Current_Smooth(void)
{
	static UINT8 su8_ADcnt = 0;
	INT32 t_i32temp = 0;
	UINT32 u32_ADC_CUR_AMP = 0;
	UINT32 u32_AD_VREF_AD = 0;

	if (su8_ADcnt++ < AD_CalNum_Cur)
	{
		g_u32ADCValFilter2[ADC_CUR_AMP] += g_u16ADCValFilter[ADC_CUR_AMP];
		g_u32ADCValFilter2[AD_VREF_AD] += g_u16ADCValFilter[AD_VREF_AD];
	}
	else
	{
		su8_ADcnt = 0;
		u32_ADC_CUR_AMP = g_u32ADCValFilter2[ADC_CUR_AMP] >> AD_CalNum_Cur_2;
		u32_AD_VREF_AD = g_u32ADCValFilter2[AD_VREF_AD] >> AD_CalNum_Cur_2;
		g_u32ADCValFilter2[ADC_CUR_AMP] = 0;
		g_u32ADCValFilter2[AD_VREF_AD] = 0;

		u32_ADC_CUR_AMP = u32_ADC_CUR_AMP * 151 / 100;
		if (u32_AD_VREF_AD > u32_ADC_CUR_AMP)
		{
			t_i32temp = ModulusSub(u32_AD_VREF_AD, u32_ADC_CUR_AMP) * 3300 / 4096;					  // mV
			t_i32temp = t_i32temp * OtherElement.u16Sys_CS_Res_Num / OtherElement.u16Sys_CS_Res >> 1; // A*10	//A*10/Gain = >>1
			g_u32ADCValFilter2[ADC_CURR] = ((t_i32temp - g_u32ADCValFilter2[ADC_CURR]) >> 3) + g_u32ADCValFilter2[ADC_CURR];
			gu16_BusCurr_DSG = (UINT16)g_u32ADCValFilter2[ADC_CURR];
			gu16_BusCurr_CHG = 0;
		}
		else
		{
			t_i32temp = ModulusSub(u32_AD_VREF_AD, u32_ADC_CUR_AMP) * 3300 / 4096;					  // mV
			t_i32temp = t_i32temp * OtherElement.u16Sys_CS_Res_Num / OtherElement.u16Sys_CS_Res >> 1; // A*10	//A*10/Gain = >>1
			g_u32ADCValFilter2[ADC_CURR] = ((t_i32temp - g_u32ADCValFilter2[ADC_CURR]) >> 3) + g_u32ADCValFilter2[ADC_CURR];
			gu16_BusCurr_DSG = 0;
			gu16_BusCurr_CHG = (UINT16)g_u32ADCValFilter2[ADC_CURR];
		}
	}
}

void ADC_TTC(void)
{
	INT32 t_i32temp = 0;
	//-------------TEMP1温度(+40)-------------
	t_i32temp = (INT32)g_u16ADCValFilter[ADC_TEMP_EV1]; // 读取AD值
	t_i32temp = GetEndValue(iSheldTemp_10K, (UINT16)LENGTH_TBLTEMP_PORT_10K, (UINT16)t_i32temp);
	g_u32ADCValFilter2[ADC_TEMP_EV1] = (((t_i32temp << 10) - g_u32ADCValFilter2[ADC_TEMP_EV1]) >> 3) + g_u32ADCValFilter2[ADC_TEMP_EV1];
	g_i32ADCResult[ADC_TEMP_EV1] = (UINT16)((g_u32ADCValFilter2[ADC_TEMP_EV1] + 512) >> 10);

	//-------------MOS1温度(+40)-------------
	t_i32temp = (INT32)g_u16ADCValFilter[ADC_TEMP_MOS1]; // 读取AD值
	t_i32temp = GetEndValue(iSheldTemp_10K, (UINT16)LENGTH_TBLTEMP_PORT_10K, (UINT16)t_i32temp);
	g_u32ADCValFilter2[ADC_TEMP_MOS1] = (((t_i32temp << 10) - g_u32ADCValFilter2[ADC_TEMP_MOS1]) >> 3) + g_u32ADCValFilter2[ADC_TEMP_MOS1];
	g_i32ADCResult[ADC_TEMP_MOS1] = (UINT16)((g_u32ADCValFilter2[ADC_TEMP_MOS1] + 512) >> 10);

	//-------------MOS2温度(+40)-------------
	t_i32temp = (INT32)g_u16ADCValFilter[ADC_TEMP_MOS2]; // 读取AD值
	t_i32temp = GetEndValue(iSheldTemp_10K, (UINT16)LENGTH_TBLTEMP_PORT_10K, (UINT16)t_i32temp);
	g_u32ADCValFilter2[ADC_TEMP_MOS2] = (((t_i32temp << 10) - g_u32ADCValFilter2[ADC_TEMP_MOS2]) >> 3) + g_u32ADCValFilter2[ADC_TEMP_MOS2];
	g_i32ADCResult[ADC_TEMP_MOS2] = (UINT16)((g_u32ADCValFilter2[ADC_TEMP_MOS2] + 512) >> 10);
}

void ADC_Vbc(void)
{
	static UINT8 s8ADcnt = 0;
	INT32 t_i32temp = 0;
	if (s8ADcnt++ < AD_CalNum)
	{
		g_u32ADCValFilter2[ADC_VBC] += (UINT32)g_u16ADCValFilter[ADC_VBC];
	}
	else
	{
		s8ADcnt = 0;
		t_i32temp = g_u32ADCValFilter2[ADC_VBC] >> AD_CalNum_2; // 读取AD值，mV
		g_u32ADCValFilter2[ADC_VBC] = 0;
		// t_i32temp *= 21;							// AD值转换为模拟量
		t_i32temp = ((t_i32temp * 825) >> 10) * Vbc_scale; // 12位，4096为基准
		t_i32temp = t_i32temp > 0 ? t_i32temp : 0;
		g_i32ADCResult[ADC_VBC] = ((t_i32temp - g_i32ADCResult[ADC_VBC]) >> 3) + g_i32ADCResult[ADC_VBC]; // mV
	}
}

// VDDA和VSSA为AD采样专门供电，VREF+和VREF-为AD采样的参加电压，不需要再配置了(所以会发现没相关语句配置)
// 关于那个表，因为为12位分辨率，所以最大输入为4096。
uint8_t ADC_TryInit(void)
{
    uint8_t i;
    ADC_StopForRtc();
    InitADC_GPIO();
    InitADC_DMA();
    if (!ADC_InitHardwareBounded())
    {
        ADC_StopForRtc();
        System_ERROR_UserCallback(ERROR_ADC);
        return 0U;
    }
    for (i = 0U; i < ADC_NUM; ++i)
    {
        g_u16ADCValFilter[i] = 0;
        g_i32ADCResult[i] = 0;
        g_u32ADCValFilter2[i] = 0;
    }
    DMA_ClearFlag(DMA1_FLAG_TC1);
    InitADC_TIMER();
    System_ERROR_UserCallback(ERROR_REMOVE_ADC);
    return 1U;
}

void InitADC(void)
{
    (void)ADC_TryInit();
}

uint8_t ADC_BeginRtcSample(void)
{
    GPIO_InitTypeDef gpio;
    /* HS-C073 AD-EN is active low; bias must settle before accepting data. */
    GPIO_ResetBits(GPIO_AD_EN, PIN_AD_EN);
    gpio.GPIO_Pin = PIN_AD_EN;
    gpio.GPIO_Mode = GPIO_Mode_Out_PP;
    gpio.GPIO_Speed = GPIO_Speed_2MHz;
    GPIO_Init(GPIO_AD_EN, &gpio);
    s_adc_sample_sum = 0U;
    s_adc_sample_count = s_adc_settled = 0U;
    if (!ADC_TryInit()) return 0U;
    s_adc_sample_start = System_GetUptimeMs();
    return 1U;
}

uint8_t ADC_PollRtcSample(void)
{
    uint16_t raw;
    if ((uint32_t)(System_GetUptimeMs() - s_adc_sample_start) < ADC_BIAS_SETTLE_MS)
        return 0U;
    if (!s_adc_settled)
    {
        /* Discard conversions started before the bias-settling deadline. */
        DMA_ClearFlag(DMA1_FLAG_TC1);
        s_adc_settled = 1U;
        return 0U;
    }
    if (DMA_GetFlagStatus(DMA1_FLAG_TC1) == RESET) return 0U;
    raw = g_u16ADCValFilter[ADC_TEMP_MOS1];
    DMA_ClearFlag(DMA1_FLAG_TC1);
    if (raw <= 5U || raw >= 4090U)
    {
        System_ERROR_UserCallback(ERROR_ADC);
        return 2U;
    }
    s_adc_sample_sum += raw;
    if (++s_adc_sample_count < ADC_RTC_SAMPLE_COUNT) return 0U;
    raw = (uint16_t)(s_adc_sample_sum / ADC_RTC_SAMPLE_COUNT);
    g_i32ADCResult[ADC_TEMP_MOS1] = GetEndValue(iSheldTemp_10K, LENGTH_TBLTEMP_PORT_10K, raw);
    return 1U;
}

// 延时类型初始化是不需要return的
void App_AnlogCal(void)
{
	if (STARTUP_CONT == System_FUNC_StartUp(SYSTEM_FUNC_STARTUP_ADC))
	{
		// return;
	}

	if (0 == g_st_SysTimeFlag.bits.b1Sys1msFlag)
	{
		return;
	}

	ADC_TTC();
}
