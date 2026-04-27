// #include "conf.h"
#include "main.h"

void InitSci(void);
void InitE2PROM_i2c(void);

Time_T sys_time = {
    .time_enter_rtc = 10,
    .power_on = false,
};

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
        // GPIO_InitStructure.GPIO_Pin = PIN_AFE1_ALM | PIN_AFE1_MODE | PIN_AFE1_SHIP;
        // GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
        // GPIO_InitStructure.GPIO_Speed = GPIO_Speed_2MHz; // IO口速度为2MHz
        // GPIO_Init(GPIOA, &GPIO_InitStructure);

        GPIO_InitStructure.GPIO_Pin = PIN_AFE1_PRO_EN | PIN_AFE1_CTL;
        GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
        GPIO_InitStructure.GPIO_Speed = GPIO_Speed_2MHz; // IO口速度为2MHz
        GPIO_Init(GPIOB, &GPIO_InitStructure);
    }

    // GPIO_WriteBit(GPIOB, GPIO_Pin_15, Bit_RESET);
    // GPIO_WriteBit(GPIOB, GPIO_Pin_15, Bit_SET);
    // void GPIO_SetBits(GPIO_TypeDef * GPIOx, uint16_t GPIO_Pin);
    // void GPIO_ResetBits(GPIO_TypeDef * GPIOx, uint16_t GPIO_Pin);
    // GPIO_ResetBits(GPIOB, GPIO_Pin_15);
    // GPIO_SetBits(GPIOB, GPIO_Pin_15);
    GPIO_InitStructure.GPIO_Pin = PIN_CHG_IN;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;
    GPIO_Init(GPIO_CHG_IN, &GPIO_InitStructure);

    // todo GPIO_INT_WK_CMNT

    GPIO_InitStructure.GPIO_Pin = PIN_MCC_C;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP; // 推挽输出
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_2MHz; // IO口速度为2MHz
    GPIO_Init(GPIO_MCC_C, &GPIO_InitStructure);

    GPIO_InitStructure.GPIO_Pin = PIN_MCU_WK;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;
    GPIO_Init(GPIO_MCU_WK, &GPIO_InitStructure);

    GPIO_InitStructure.GPIO_Pin = PIN_SW;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;
    GPIO_Init(GPIO_SW, &GPIO_InitStructure);

    GPIO_WriteBit(GPIO_DC_EN, PIN_DC_EN, Bit_SET);
    GPIO_InitStructure.GPIO_Pin = PIN_DC_EN;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP; // 推挽输出
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_2MHz; // IO口速度为2MHz
    GPIO_Init(GPIO_DC_EN, &GPIO_InitStructure);

#if !LEDBAR_DRIVER_GPIO_CHARLIE
    GPIO_InitStructure.GPIO_Pin = PIN_DBG_LED;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP; // 推挽输出
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_2MHz; // IO口速度为2MHz
    GPIO_Init(GPIO_DBG_LED, &GPIO_InitStructure);
#endif

    // todo 确认spi配置
#if !LEDBAR_DRIVER_GPIO_CHARLIE
    GPIO_InitStructure.GPIO_Pin = PIN_SPI_MOSI;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP; // 推挽输出
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_2MHz; // IO口速度为2MHz
    GPIO_Init(GPIO_SPI_MOSI, &GPIO_InitStructure);

    GPIO_InitStructure.GPIO_Pin = PIN_SPI1_NSS;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP; // 推挽输出
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_2MHz; // IO口速度为2MHz
    GPIO_Init(GPIO_SPI1_NSS, &GPIO_InitStructure);

    GPIO_InitStructure.GPIO_Pin = PIN_SPI1_SCK;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP; // 推挽输出
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_2MHz; // IO口速度为2MHz
    GPIO_Init(GPIO_SPI1_SCK, &GPIO_InitStructure);
#else
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_2MHz;
    GPIO_InitStructure.GPIO_Pin = PIN_SPI1_NSS | PIN_SPI1_SCK | PIN_SPI_MOSI;
    GPIO_Init(GPIOA, &GPIO_InitStructure);
    GPIO_InitStructure.GPIO_Pin = PIN_DBG_LED | PIN_SEG_EN;
    GPIO_Init(GPIOB, &GPIO_InitStructure);
#endif

    GPIO_InitStructure.GPIO_Pin = PIN_RF_EN;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP; // 推挽输出
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_2MHz; // IO口速度为2MHz
    GPIO_Init(GPIO_RF_EN, &GPIO_InitStructure);

    GPIO_InitStructure.GPIO_Pin = PIN_ADC_VBUS | PIN_ADC_CUR;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AIN;
    GPIO_Init(GPIOA, &GPIO_InitStructure);

    GPIO_InitStructure.GPIO_Pin = PIN_ADC_NMOS;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AIN;
    GPIO_Init(GPIOB, &GPIO_InitStructure);

    GPIO_WriteBit(GPIO_2727_EN, PIN_2737_EN, Bit_SET);
    GPIO_InitStructure.GPIO_Pin = PIN_2737_EN;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP; // 推挽输出
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_2MHz; // IO口速度为2MHz
    GPIO_Init(GPIO_2727_EN, &GPIO_InitStructure);

    {
        //???这个函数没起作用
        // GPIO_WriteBit(GPIO_M_STB, PIN_M_STB, Bit_RESET);
        // GPIO_WriteBit(GPIO_AD_EN, PIN_AD_EN, Bit_RESET);
        // GPIO_WriteBit(GPIO_BLE_EN, PIN_BLE_EN, Bit_RESET);
        // GPIO_WriteBit(GPIO_SW_EN, PIN_SW_EN, Bit_RESET);
        GPIO_SetBits(GPIO_M_STB, PIN_M_STB);
        GPIO_SetBits(GPIO_AD_EN, PIN_AD_EN);
        GPIO_ResetBits(GPIO_CMNT_EN, PIN_CMNT_EN);
        GPIO_SetBits(GPIO_ADC_BUS_EN, PIN_ADC_BUS_EN);
#if !LEDBAR_DRIVER_GPIO_CHARLIE
        GPIO_SetBits(GPIO_SEG_EN, PIN_SEG_EN);
#endif

        GPIO_InitStructure.GPIO_Pin = PIN_M_STB;
        GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP; // 推挽输出
        GPIO_InitStructure.GPIO_Speed = GPIO_Speed_2MHz; // IO口速度为2MHz
        GPIO_Init(GPIO_M_STB, &GPIO_InitStructure);

        GPIO_InitStructure.GPIO_Pin = PIN_AD_EN;
        GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP; // 推挽输出
        GPIO_InitStructure.GPIO_Speed = GPIO_Speed_2MHz; // IO口速度为2MHz
        GPIO_Init(GPIO_AD_EN, &GPIO_InitStructure);

        GPIO_InitStructure.GPIO_Pin = PIN_CMNT_EN;
        GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP; // 推挽输出
        GPIO_InitStructure.GPIO_Speed = GPIO_Speed_2MHz; // IO口速度为2MHz
        GPIO_Init(GPIO_CMNT_EN, &GPIO_InitStructure);

        GPIO_InitStructure.GPIO_Pin = PIN_ADC_BUS_EN;
        GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP; // 推挽输出
        GPIO_InitStructure.GPIO_Speed = GPIO_Speed_2MHz; // IO口速度为2MHz
        GPIO_Init(GPIO_ADC_BUS_EN, &GPIO_InitStructure);

#if !LEDBAR_DRIVER_GPIO_CHARLIE
        GPIO_InitStructure.GPIO_Pin = PIN_SEG_EN;
        GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP; // 推挽输出
        GPIO_InitStructure.GPIO_Speed = GPIO_Speed_2MHz; // IO口速度为2MHz
        GPIO_Init(GPIO_SEG_EN, &GPIO_InitStructure);
#endif
    }
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
        GPIO_InitStructure.GPIO_Pin = PIN_CHG_IN; // ?????GPIO??,PA0?????
        GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;
        GPIO_Init(GPIO_CHG_IN, &GPIO_InitStructure);
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
        GPIO_InitStructure.GPIO_Pin = PIN_SW; // ?????GPIO??,PA0?????
        GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;
        GPIO_Init(GPIO_SW, &GPIO_InitStructure);
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

void InitWakeUp_NormalMode(void)
{
    EXTI_InitTypeDef EXTI_InitStruct;
    NVIC_InitTypeDef NVIC_InitStructure;
    GPIO_InitTypeDef GPIO_InitStructure;

    InitWakeUp_Base();

    {
#ifdef UART1_WAKEUP_ENABLE
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
#endif // UART1_WAKEUP_ENABLE

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

    LedBar_SetSleep(1u);
    ADC_StopForLowPower(); // stop ADC/TIM2/DMA before STOP

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

    {
        GPIO_WriteBit(GPIO_M_STB, PIN_M_STB, Bit_RESET);
        GPIO_WriteBit(GPIO_AD_EN, PIN_AD_EN, Bit_RESET);
        GPIO_WriteBit(GPIO_CMNT_EN, PIN_CMNT_EN, Bit_SET);
        GPIO_WriteBit(GPIO_ADC_BUS_EN, PIN_ADC_BUS_EN, Bit_RESET);
#if !LEDBAR_DRIVER_GPIO_CHARLIE
        GPIO_WriteBit(GPIO_SEG_EN, PIN_SEG_EN, Bit_RESET);
#endif

        GPIO_InitStructure.GPIO_Pin = PIN_M_STB;
        GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP; // 推挽输出
        GPIO_InitStructure.GPIO_Speed = GPIO_Speed_2MHz; // IO口速度为2MHz
        GPIO_Init(GPIO_M_STB, &GPIO_InitStructure);

        GPIO_InitStructure.GPIO_Pin = PIN_AD_EN;
        GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP; // 推挽输出
        GPIO_InitStructure.GPIO_Speed = GPIO_Speed_2MHz; // IO口速度为2MHz
        GPIO_Init(GPIO_AD_EN, &GPIO_InitStructure);

        GPIO_InitStructure.GPIO_Pin = PIN_CMNT_EN;
        GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP; // 推挽输出
        GPIO_InitStructure.GPIO_Speed = GPIO_Speed_2MHz; // IO口速度为2MHz
        GPIO_Init(GPIO_CMNT_EN, &GPIO_InitStructure);

        GPIO_InitStructure.GPIO_Pin = PIN_ADC_BUS_EN;
        GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP; // 推挽输出
        GPIO_InitStructure.GPIO_Speed = GPIO_Speed_2MHz; // IO口速度为2MHz
        GPIO_Init(GPIO_ADC_BUS_EN, &GPIO_InitStructure);

// #if !LEDBAR_DRIVER_GPIO_CHARLIE
//         GPIO_InitStructure.GPIO_Pin = PIN_SEG_EN;
//         GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP; // 推挽输出
//         GPIO_InitStructure.GPIO_Speed = GPIO_Speed_2MHz; // IO口速度为2MHz
//         GPIO_Init(GPIO_SEG_EN, &GPIO_InitStructure);
// #endif

        // GPIO_InitStructure.GPIO_Pin = PIN_DC_EN;
        // GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP; // 推挽输出
        // GPIO_InitStructure.GPIO_Speed = GPIO_Speed_2MHz; // IO口速度为2MHz
        // GPIO_Init(GPIO_DC_EN, &GPIO_InitStructure);

        // GPIO_InitStructure.GPIO_Pin = PIN_2737_EN;
        // GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP; // 推挽输出
        // GPIO_InitStructure.GPIO_Speed = GPIO_Speed_2MHz; // IO口速度为2MHz
        // GPIO_Init(GPIO_2727_EN, &GPIO_InitStructure);
    }
}

void IOstatus_RTCMode(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE); // ??GPIOA??
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE); // ??GPIOB??
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOC, ENABLE); // ??GPIOC??
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOD, ENABLE); // ??GPIOD??
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOE, ENABLE); // ??GPIOE??

    LedBar_SetSleep(1u);
    ADC_StopForLowPower(); // stop ADC/TIM2/DMA before STOP

    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_All & (~PIN_DC_EN) & (~PIN_2737_EN);
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AIN;
    GPIO_Init(GPIOA, &GPIO_InitStructure);

    // GPIO_InitStructure.GPIO_Pin = GPIO_Pin_All;
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_All & (~GPIO_Pin_14);
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AIN;
    GPIO_Init(GPIOB, &GPIO_InitStructure);
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
#if 0
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_12;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_2MHz;
    GPIO_Init(GPIOC, &GPIO_InitStructure);
    // GPIO_ResetBits(GPIOC, GPIO_InitStructure.GPIO_Pin);

    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_13;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_2MHz;
    GPIO_Init(GPIOC, &GPIO_InitStructure);
    // GPIO_ResetBits(GPIOC, GPIO_InitStructure.GPIO_Pin);

    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_2;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_2MHz;
    GPIO_Init(GPIOD, &GPIO_InitStructure);
    // GPIO_ResetBits(GPIOD, GPIO_InitStructure.GPIO_Pin);
#endif
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
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_PWR, ENABLE);
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM3, ENABLE);
    TIM_Cmd(TIM3, DISABLE);
    TIM_ClearITPendingBit(TIM3, TIM_IT_Update);
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM3, DISABLE);
    PWR_EnterSTOPMode(PWR_Regulator_LowPower, PWR_STOPEntry_WFI);

    cpu_frequency_conf();
}

void InitRtcWakeupCheck(void)
{
    InitDelay();

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_AFIO | RCC_APB2Periph_GPIOA | RCC_APB2Periph_GPIOB, ENABLE);
    InitSci();
    initAFE1_IIC();
    InitE2PROM_i2c();
}

void InitRunAfterStopWakeup(void)
{
    is_wakeup = true;

    InitDelay();
    InitIO();

    ADC_StopForLowPower();
    InitADC();

#ifdef __FUNC__HEAT__
    InitHeat_Cool();
#endif
#ifdef __FUNC__LED__
    APP_LedBar();
    set_LED_state(LED_BAR_NORMAL, 4);
#endif // DEBUG

    USART_DeInit(USART1);
    USART_DeInit(USART2);

    InitSci();
    InitCan();
    InitTimer();

    sys_time.wakeup_rtc = true;
    Init_ChargerLoad_Det();

    initAFE1_IIC();
    InitE2PROM_i2c();
}

void Init(void)
{
    if (is_rtc_wakekup)
    {
        InitRtcWakeupCheck();
    }
    else
    {
        InitRunAfterStopWakeup();
    }
}
