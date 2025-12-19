// #include "conf.h"
#include "main.h"

Time_T sys_time;

// void GPIO_SetBits(GPIO_TypeDef * GPIOx, uint16_t GPIO_Pin);
// void GPIO_ResetBits(GPIO_TypeDef * GPIOx, uint16_t GPIO_Pin);
// GPIO_ResetBits(GPIOB, GPIO_Pin_15);
// GPIO_SetBits(GPIOB, GPIO_Pin_15);
void InitIO(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_AFIO, ENABLE);  // 使能IO复用功能模块时钟
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE); // 使能GPIOA时钟
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE); // 使能GPIOB时钟
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOC, ENABLE); // 使能GPIOC时钟
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOD, ENABLE); // 使能GPIOD时钟
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOE, ENABLE); // 使能GPIOE时钟

    {
        GPIO_InitStructure.GPIO_Pin = PIN_AFE1_ALM | PIN_AFE1_MODE | PIN_AFE1_SHIP;
        GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
        GPIO_InitStructure.GPIO_Speed = GPIO_Speed_2MHz; // IO口速度为2MHz
        GPIO_Init(GPIOA, &GPIO_InitStructure);

        GPIO_InitStructure.GPIO_Pin = PIN_AFE1_PRO_EN | PIN_AFE1_CTL;
        GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
        GPIO_InitStructure.GPIO_Speed = GPIO_Speed_2MHz; // IO口速度为2MHz
        GPIO_Init(GPIOB, &GPIO_InitStructure);
    }

    // PB15_LED1
    // GPIO_WriteBit(GPIOB, GPIO_Pin_15, Bit_RESET);
    // GPIO_WriteBit(GPIOB, GPIO_Pin_15, Bit_SET);
    GPIO_InitStructure.GPIO_Pin = PIN_DBG_LED;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP; // 推挽输出
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_2MHz; // IO口速度为2MHz
    GPIO_Init(GPIO_DBG_LED, &GPIO_InitStructure);
    // void GPIO_SetBits(GPIO_TypeDef * GPIOx, uint16_t GPIO_Pin);
    // void GPIO_ResetBits(GPIO_TypeDef * GPIOx, uint16_t GPIO_Pin);
    // GPIO_ResetBits(GPIOB, GPIO_Pin_15);
    // GPIO_SetBits(GPIOB, GPIO_Pin_15);

    GPIO_InitStructure.GPIO_Pin = PIN_KEY1;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;
    GPIO_Init(GPIO_KEY1, &GPIO_InitStructure);

    {
        //???这个函数没起作用
        // GPIO_WriteBit(GPIO_M_STB, PIN_M_STB, Bit_RESET);
        GPIO_WriteBit(GPIO_AD_EN, PIN_AD_EN, Bit_SET);
        // GPIO_WriteBit(GPIO_BLE_EN, PIN_BLE_EN, Bit_RESET);
        // GPIO_WriteBit(GPIO_SW_EN, PIN_SW_EN, Bit_RESET);

        GPIO_SetBits(GPIO_M_STB, PIN_M_STB);
        // GPIO_ResetBits(GPIO_AD_EN, PIN_AD_EN);
        GPIO_ResetBits(GPIO_BLE_EN, PIN_BLE_EN);
        GPIO_ResetBits(GPIO_CMNT_EN, PIN_CMNT_EN);
        GPIO_SetBits(GPIO_SW_EN, PIN_SW_EN);

        GPIO_InitStructure.GPIO_Pin = PIN_M_STB;
        GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP; // 推挽输出
        GPIO_InitStructure.GPIO_Speed = GPIO_Speed_2MHz; // IO口速度为2MHz
        GPIO_Init(GPIO_M_STB, &GPIO_InitStructure);

        GPIO_InitStructure.GPIO_Pin = PIN_AD_EN;
        GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP; // 推挽输出
        GPIO_InitStructure.GPIO_Speed = GPIO_Speed_2MHz; // IO口速度为2MHz
        GPIO_Init(GPIO_AD_EN, &GPIO_InitStructure);

        GPIO_InitStructure.GPIO_Pin = PIN_BLE_EN;
        GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP; // 推挽输出
        GPIO_InitStructure.GPIO_Speed = GPIO_Speed_2MHz; // IO口速度为2MHz
        GPIO_Init(GPIO_BLE_EN, &GPIO_InitStructure);

        GPIO_InitStructure.GPIO_Pin = PIN_SW_EN;
        GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP; // 推挽输出
        GPIO_InitStructure.GPIO_Speed = GPIO_Speed_2MHz; // IO口速度为2MHz
        GPIO_Init(GPIO_SW_EN, &GPIO_InitStructure);

        GPIO_InitStructure.GPIO_Pin = PIN_CMNT_EN;
        GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP; // 推挽输出
        GPIO_InitStructure.GPIO_Speed = GPIO_Speed_2MHz; // IO口速度为2MHz
        GPIO_Init(GPIO_CMNT_EN, &GPIO_InitStructure);
    }
        GPIO_WriteBit(GPIO_CMNT_EN, PIN_CMNT_EN, Bit_SET);
}

void InitWakeUp_Base(void)
{
    EXTI_InitTypeDef EXTI_InitStruct;
    NVIC_InitTypeDef NVIC_InitStructure;
    GPIO_InitTypeDef GPIO_InitStructure;

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE); // ??GPIOA??????????????
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE); // ??GPIOA??????????????
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOC, ENABLE); // ??GPIOA??????????????
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_AFIO, ENABLE);  // ???????????

    jtag_disableAndConfIO();
#if 1
    {
        GPIO_InitStructure.GPIO_Pin = PIN_INT_WK_MCU; // ?????GPIO??,PA0?????
        GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;
        GPIO_Init(GPIO_INT_WK_MCU, &GPIO_InitStructure);
        GPIO_EXTILineConfig(GPIO_PortSourceGPIOA, GPIO_PinSource0);
        EXTI_InitStruct.EXTI_Line = EXTI_Line0;
        EXTI_InitStruct.EXTI_Mode = EXTI_Mode_Interrupt;
        EXTI_InitStruct.EXTI_Trigger = EXTI_Trigger_Rising;
        EXTI_InitStruct.EXTI_LineCmd = ENABLE;
        EXTI_Init(&EXTI_InitStruct);
        NVIC_InitStructure.NVIC_IRQChannel = EXTI0_IRQn;
        NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 0x01;
        NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0x01;
        NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
        NVIC_Init(&NVIC_InitStructure);
    }

    {
        GPIO_InitStructure.GPIO_Pin = PIN_KEY1; // ?????GPIO??,PA0?????
        GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;
        GPIO_Init(GPIO_KEY1, &GPIO_InitStructure);
        GPIO_EXTILineConfig(GPIO_PortSourceGPIOA, GPIO_PinSource9);
        EXTI_InitStruct.EXTI_Line = EXTI_Line9;
        EXTI_InitStruct.EXTI_Mode = EXTI_Mode_Interrupt;
        EXTI_InitStruct.EXTI_Trigger = EXTI_Trigger_Falling;
        EXTI_InitStruct.EXTI_LineCmd = ENABLE;
        EXTI_Init(&EXTI_InitStruct);
        NVIC_InitStructure.NVIC_IRQChannel = EXTI9_5_IRQn;
        NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 0x01;
        NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0x01;
        NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
        NVIC_Init(&NVIC_InitStructure);
    }
#endif
}

void InitWakeUp_NormalMode	(void)
{
    EXTI_InitTypeDef EXTI_InitStruct;
    NVIC_InitTypeDef NVIC_InitStructure;
    GPIO_InitTypeDef GPIO_InitStructure;

    InitWakeUp_Base();

    {
        GPIO_InitStructure.GPIO_Pin = PIN_SCI1_RX; // ?????GPIO??,PA0?????
        GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;
        GPIO_Init(GPIO_SCI1_RX, &GPIO_InitStructure);
        GPIO_EXTILineConfig(GPIO_PortSourceGPIOB, GPIO_PinSource7);
        EXTI_InitStruct.EXTI_Line = EXTI_Line7;
        EXTI_InitStruct.EXTI_Mode = EXTI_Mode_Interrupt;
        EXTI_InitStruct.EXTI_Trigger = EXTI_Trigger_Rising;
        EXTI_InitStruct.EXTI_LineCmd = ENABLE;
        EXTI_Init(&EXTI_InitStruct);
        NVIC_InitStructure.NVIC_IRQChannel = EXTI9_5_IRQn;
        NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 0x01;
        NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0x01;
        NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
        NVIC_Init(&NVIC_InitStructure);

        // GPIO_InitStructure.GPIO_Pin = PIN_SCI2_RX; // ?????GPIO??,PA0?????
        // GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;
        // GPIO_Init(GPIO_SCI2_RX, &GPIO_InitStructure);
        // GPIO_EXTILineConfig(GPIO_PortSourceGPIOA, GPIO_PinSource3);
        // EXTI_InitStruct.EXTI_Line = EXTI_Line3;
        // EXTI_InitStruct.EXTI_Mode = EXTI_Mode_Interrupt;
        // EXTI_InitStruct.EXTI_Trigger = EXTI_Trigger_Rising;
        // EXTI_InitStruct.EXTI_LineCmd = ENABLE;
        // EXTI_Init(&EXTI_InitStruct);
        // NVIC_InitStructure.NVIC_IRQChannel = EXTI3_IRQn;
        // NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 0x01;
        // NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0x01;
        // NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
        // NVIC_Init(&NVIC_InitStructure);
    }
    {
        GPIO_InitStructure.GPIO_Pin = PIN_INT_WK_CMNT; // ?????GPIO??,PA0?????
        GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;
        GPIO_Init(GPIO_INT_WK_CMNT, &GPIO_InitStructure);

        GPIO_EXTILineConfig(GPIO_PortSourceGPIOB, GPIO_PinSource12);
        EXTI_InitStruct.EXTI_Line = EXTI_Line12;
        EXTI_InitStruct.EXTI_Mode = EXTI_Mode_Interrupt;
        EXTI_InitStruct.EXTI_Trigger = EXTI_Trigger_Rising;
        EXTI_InitStruct.EXTI_LineCmd = ENABLE;
        EXTI_Init(&EXTI_InitStruct);

        NVIC_InitStructure.NVIC_IRQChannel = EXTI15_10_IRQn;
        NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 0x01;
        NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0x01;
        NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
        NVIC_Init(&NVIC_InitStructure);
    }
}

void InitWakeUp_RTCMode(void)
{
    InitWakeUp_NormalMode(); // ???Base?????
    RTC_WKTimeConfig();
}

// ???standby?????PA0?wkup???
// ???????????????
void InitWakeUp_DeepMode(void)
{
    InitWakeUp_Base();
}

void IOstatus_Base(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE); // ??GPIOA??
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE); // ??GPIOB??
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOC, ENABLE); // ??GPIOC??
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOD, ENABLE); // ??GPIOD??
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOE, ENABLE); // ??GPIOE??

    ADC_DeInit(ADC1); // ????????????????

    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_All;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AIN;
    GPIO_Init(GPIOA, &GPIO_InitStructure);

    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_All;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AIN;
    GPIO_Init(GPIOB, &GPIO_InitStructure);

    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_All;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AIN;
    GPIO_Init(GPIOC, &GPIO_InitStructure);

    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_All;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AIN;
    GPIO_Init(GPIOD, &GPIO_InitStructure);

    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_All;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AIN;
    GPIO_Init(GPIOE, &GPIO_InitStructure);

    // __delay_ms(100);
}

void IOstatus_RTCMode(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE); // ??GPIOA??
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE); // ??GPIOB??
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOC, ENABLE); // ??GPIOC??
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOD, ENABLE); // ??GPIOD??
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOE, ENABLE); // ??GPIOE??

    ADC_DeInit(ADC1); // ????????????????

    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_All;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AIN;
    GPIO_Init(GPIOA, &GPIO_InitStructure);

#if 0
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_All ;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AIN;
    GPIO_Init(GPIOB, &GPIO_InitStructure);
#else
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_All;
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_All & (~GPIO_Pin_14);
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AIN;
    GPIO_Init(GPIOB, &GPIO_InitStructure);
#endif

#if 1
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_All;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AIN;
    GPIO_Init(GPIOC, &GPIO_InitStructure);
#else
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_All;
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_All & (~GPIO_Pin_4);
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AIN;
    GPIO_Init(GPIOC, &GPIO_InitStructure);
#endif

    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_All;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AIN;
    GPIO_Init(GPIOD, &GPIO_InitStructure);

    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_All;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AIN;
    GPIO_Init(GPIOE, &GPIO_InitStructure);

    // ??????
    // ???
#if 1
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_12;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_2MHz;
    GPIO_Init(GPIOC, &GPIO_InitStructure);
    // GPIO_ResetBits(GPIOC, GPIO_InitStructure.GPIO_Pin);
    MCUO_DRV_CMNT = 1;

    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_13;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_2MHz;
    GPIO_Init(GPIOC, &GPIO_InitStructure);
    // GPIO_ResetBits(GPIOC, GPIO_InitStructure.GPIO_Pin);
    MCUO_PWSV_CTR = 1;

    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_2;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_2MHz;
    GPIO_Init(GPIOD, &GPIO_InitStructure);
    // GPIO_ResetBits(GPIOD, GPIO_InitStructure.GPIO_Pin);
    MCUO_PWSV_STB = 0;
#endif

    __delay_ms(100);
}

void IOstatus_NormalMode(void)
{
    IOstatus_Base();
}

void IOstatus_DeepMode(void)
{
    IOstatus_Base();
}

void IORecover_RTCMode(void)
{
    MCU_RESET();
}

void IORecover_NormalMode(void)
{
    // TIM_Cmd(TIM3, ENABLE);	//??App_SleepTest()??
    MCU_RESET(); // ?????????????????????????????????????????
}

void IORecover_DeepMode(void)
{
    MCU_RESET();
}

void Sys_StopMode(void)
{
    // RCC_APB1PeriphClockCmd(RCC_APB1Periph_PWR, ENABLE);
    PWR_EnterSTOPMode(PWR_Regulator_LowPower, PWR_STOPEntry_WFI);

// ????????????????????
// ?????????????????????
#if (defined _HSE_8M_PLL_48M) || (defined _HSE_12M_PLL_48M)
    RCC_HSEConfig(RCC_HSE_ON); // ????????HSI
    while (RCC_GetFlagStatus(RCC_FLAG_HSERDY) == RESET)
        ;               // ?? HSE ????
    RCC_PLLCmd(ENABLE); // ?? PLL
    while (RCC_GetFlagStatus(RCC_FLAG_PLLRDY) == RESET)
        ;                                      // ?? PLL ????
    RCC_SYSCLKConfig(RCC_SYSCLKSource_PLLCLK); // ??PLL???????
    while (RCC_GetSYSCLKSource() != 0x08)
        ; // ??PLL?????????
#endif
}

#if 0
void Init(void)
{
#if 0
	RCC_AHBPeriphClockCmd(RCC_AHBPeriph_GPIOA, ENABLE); // 开启GPIOA的外设时钟
	RCC_AHBPeriphClockCmd(RCC_AHBPeriph_GPIOB, ENABLE); // 开启GPIOB的外设时钟
	RCC_AHBPeriphClockCmd(RCC_AHBPeriph_GPIOC, ENABLE); // 开启GPIOC的外设时钟
	RCC_AHBPeriphClockCmd(RCC_AHBPeriph_GPIOD, ENABLE); // 开启GPIOB的外设时钟
	RCC_AHBPeriphClockCmd(RCC_AHBPeriph_GPIOE, ENABLE); // 开启GPIOB的外设时钟
	RCC_AHBPeriphClockCmd(RCC_AHBPeriph_GPIOF, ENABLE); // 开启GPIOF的外设时钟

	InitSci();
#endif

#ifdef _DEBUG_
	// cpu_frequency_conf();
	InitIO();
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_AFIO, ENABLE);  // 使能IO复用功能模块时钟
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE); // 使能GPIOA时钟
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE); // 使能GPIOB时钟
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOC, ENABLE); // 使能GPIOC时钟
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOD, ENABLE); // 使能GPIOD时钟
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOE, ENABLE); // 使能GPIOE时钟

	InitSci();
#endif
	

	if (is_rtc_wakekup)
	{
	}
	else
	{
		cpu_frequency_conf();

		is_wakeup = true;

		InitDelay();
		InitIO();

		// InitSystemWakeUp();
		// {
		//     MCUO_PWSV_STB = 1;
		//     MCUO_PWSV_CTR = 1;
		//     // bug fixme 注意
		//     MCUO_AFE_SHIP = 0;
		//     MCUO_AFE_MODE = 0;
		// }

		// InitSci();
		// Init_ChargerLoad_Det();
		ADC_DeInit(ADC1);
		InitADC();
		//?????adc配置有什么影响
		// Init_ChargerLoad_Det();

#ifdef __FUNC__HEAT__
		InitHeat_Cool();
#endif
#ifdef __FUNC__LED__
		APP_LedBar();
		set_LED_state(LED_BAR_NORMAL, 4);
#endif // DEBUG
		InitTimer();
		// InitTimer_10ms_fault3();


        USART_DeInit(USART1);
        USART_DeInit(USART2);

        InitSci();

        InitCan();
	}

	initAFE_IIC();
	
	InitE2PROM();

	Init_ChargerLoad_Det();
}

#endif