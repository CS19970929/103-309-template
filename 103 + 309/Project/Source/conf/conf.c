#include "main.h"

void InitSci(void);
void InitE2PROM_i2c(void);

Time_T sys_time = {
    .time_enter_rtc = 10,
    .power_on = false,
};

static void InitSocKeyInput(void)
{
    GPIO_InitTypeDef gpio_init;

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA | RCC_APB2Periph_AFIO, ENABLE);
    gpio_init.GPIO_Pin = PIN_SOC_KEY;
    gpio_init.GPIO_Mode = GPIO_Mode_IPU;
    GPIO_Init(GPIO_SOC_KEY, &gpio_init);

    gpio_init.GPIO_Pin = PIN_MAIN_SW;
    GPIO_Init(GPIO_MAIN_SW, &gpio_init);
}

static void InitSocLedOff(void)
{
    GPIO_InitTypeDef gpio_init;

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE);
    GPIO_ResetBits(GPIOA, PIN_SOC_LED_25 | PIN_SOC_LED_50 | PIN_SOC_LED_75 | PIN_SOC_LED_100);
    gpio_init.GPIO_Pin = PIN_SOC_LED_25 | PIN_SOC_LED_50 | PIN_SOC_LED_75 | PIN_SOC_LED_100;
    gpio_init.GPIO_Mode = GPIO_Mode_Out_PP;
    gpio_init.GPIO_Speed = GPIO_Speed_2MHz;
    GPIO_Init(GPIOA, &gpio_init);
}

static void InitD010CommonIO(void)
{
    GPIO_InitTypeDef gpio_init;

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_AFIO |
                               RCC_APB2Periph_GPIOA |
                               RCC_APB2Periph_GPIOB |
                               RCC_APB2Periph_GPIOC |
                               RCC_APB2Periph_GPIOD |
                               RCC_APB2Periph_GPIOE,
                           ENABLE);

    gpio_init.GPIO_Pin = PIN_CHG_IN;
    gpio_init.GPIO_Mode = GPIO_Mode_IN_FLOATING;
    GPIO_Init(GPIO_CHG_IN, &gpio_init);

    gpio_init.GPIO_Pin = PIN_INT_WK_CMNT;
    gpio_init.GPIO_Mode = GPIO_Mode_IN_FLOATING;
    GPIO_Init(GPIO_INT_WK_CMNT, &gpio_init);

    InitSocKeyInput();
    InitSocLedOff();

    GPIO_ResetBits(GPIO_MCC_C, PIN_MCC_C);
    gpio_init.GPIO_Pin = PIN_MCC_C;
    gpio_init.GPIO_Mode = GPIO_Mode_Out_PP;
    gpio_init.GPIO_Speed = GPIO_Speed_2MHz;
    GPIO_Init(GPIO_MCC_C, &gpio_init);

    GPIO_ResetBits(GPIO_RF_EN, PIN_RF_EN);
    gpio_init.GPIO_Pin = PIN_RF_EN;
    gpio_init.GPIO_Mode = GPIO_Mode_Out_PP;
    gpio_init.GPIO_Speed = GPIO_Speed_2MHz;
    GPIO_Init(GPIO_RF_EN, &gpio_init);

    GPIO_ResetBits(GPIO_DBG_LED, PIN_DBG_LED);
    gpio_init.GPIO_Pin = PIN_DBG_LED;
    gpio_init.GPIO_Mode = GPIO_Mode_Out_PP;
    gpio_init.GPIO_Speed = GPIO_Speed_2MHz;
    GPIO_Init(GPIO_DBG_LED, &gpio_init);

    gpio_init.GPIO_Pin = PIN_ADC_VBUS;
    gpio_init.GPIO_Mode = GPIO_Mode_AIN;
    GPIO_Init(GPIO_ADC_VBUS, &gpio_init);

    gpio_init.GPIO_Pin = PIN_ADC_NMOS;
    gpio_init.GPIO_Mode = GPIO_Mode_AIN;
    GPIO_Init(GPIO_ADC_NMOS, &gpio_init);

    GPIO_SetBits(GPIO_M_STB, PIN_M_STB);
    GPIO_SetBits(GPIO_AD_EN, PIN_AD_EN);
    GPIO_ResetBits(GPIO_CMNT_EN, PIN_CMNT_EN);
    GPIO_SetBits(GPIO_ADC_BUS_EN, PIN_ADC_BUS_EN);

    gpio_init.GPIO_Pin = PIN_M_STB;
    gpio_init.GPIO_Mode = GPIO_Mode_Out_PP;
    gpio_init.GPIO_Speed = GPIO_Speed_2MHz;
    GPIO_Init(GPIO_M_STB, &gpio_init);

    gpio_init.GPIO_Pin = PIN_AD_EN;
    gpio_init.GPIO_Mode = GPIO_Mode_Out_PP;
    gpio_init.GPIO_Speed = GPIO_Speed_2MHz;
    GPIO_Init(GPIO_AD_EN, &gpio_init);

    gpio_init.GPIO_Pin = PIN_CMNT_EN;
    gpio_init.GPIO_Mode = GPIO_Mode_Out_PP;
    gpio_init.GPIO_Speed = GPIO_Speed_2MHz;
    GPIO_Init(GPIO_CMNT_EN, &gpio_init);

    gpio_init.GPIO_Pin = PIN_ADC_BUS_EN;
    gpio_init.GPIO_Mode = GPIO_Mode_Out_PP;
    gpio_init.GPIO_Speed = GPIO_Speed_2MHz;
    GPIO_Init(GPIO_ADC_BUS_EN, &gpio_init);
}

void InitIO_rtc(void)
{
    InitD010CommonIO();
}

void InitIO(void)
{
    GPIO_InitTypeDef gpio_init;

    InitD010CommonIO();

    gpio_init.GPIO_Pin = PIN_AFE1_PRO_EN | PIN_AFE1_CTL;
    gpio_init.GPIO_Mode = GPIO_Mode_Out_PP;
    gpio_init.GPIO_Speed = GPIO_Speed_2MHz;
    GPIO_Init(GPIOB, &gpio_init);
    MCUO_AFE_CTLC = 0;
}

void InitWakeUp_Base(void)
{
    EXTI_InitTypeDef exti_init;
    NVIC_InitTypeDef nvic_init;
    GPIO_InitTypeDef gpio_init;

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA | RCC_APB2Periph_AFIO, ENABLE);

    jtag_disableAndConfIO();

    gpio_init.GPIO_Pin = PIN_CHG_IN;
    gpio_init.GPIO_Mode = GPIO_Mode_IN_FLOATING;
    GPIO_Init(GPIO_CHG_IN, &gpio_init);
    GPIO_EXTILineConfig(GPIO_PortSourceGPIOA, GPIO_PinSource0);
    exti_init.EXTI_Line = EXTI_Line0;
    exti_init.EXTI_Mode = EXTI_Mode_Interrupt;
    exti_init.EXTI_Trigger = EXTI_Trigger_Falling;
    exti_init.EXTI_LineCmd = ENABLE;
    EXTI_Init(&exti_init);
    nvic_init.NVIC_IRQChannel = EXTI0_IRQn;
    nvic_init.NVIC_IRQChannelPreemptionPriority = 0x01;
    nvic_init.NVIC_IRQChannelSubPriority = 0x01;
    nvic_init.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&nvic_init);

    InitSocKeyInput();
    GPIO_EXTILineConfig(SOC_KEY_EXTI_PORT_SOURCE, SOC_KEY_EXTI_PIN_SOURCE);
    exti_init.EXTI_Line = SOC_KEY_EXTI_LINE;
    exti_init.EXTI_Mode = EXTI_Mode_Interrupt;
    exti_init.EXTI_Trigger = EXTI_Trigger_Falling;
    exti_init.EXTI_LineCmd = ENABLE;
    EXTI_Init(&exti_init);
    nvic_init.NVIC_IRQChannel = SOC_KEY_EXTI_IRQn;
    nvic_init.NVIC_IRQChannelPreemptionPriority = 0x01;
    nvic_init.NVIC_IRQChannelSubPriority = 0x01;
    nvic_init.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&nvic_init);

    GPIO_EXTILineConfig(MAIN_SW_EXTI_PORT_SOURCE, MAIN_SW_EXTI_PIN_SOURCE);
    exti_init.EXTI_Line = MAIN_SW_EXTI_LINE;
    exti_init.EXTI_Mode = EXTI_Mode_Interrupt;
    exti_init.EXTI_Trigger = EXTI_Trigger_Falling;
    exti_init.EXTI_LineCmd = ENABLE;
    EXTI_Init(&exti_init);
    nvic_init.NVIC_IRQChannel = MAIN_SW_EXTI_IRQn;
    nvic_init.NVIC_IRQChannelPreemptionPriority = 0x01;
    nvic_init.NVIC_IRQChannelSubPriority = 0x01;
    nvic_init.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&nvic_init);
}

void InitWakeUp_NormalMode(void)
{
    EXTI_InitTypeDef exti_init;
    NVIC_InitTypeDef nvic_init;
    GPIO_InitTypeDef gpio_init;

    InitWakeUp_Base();

#ifdef UART1_WAKEUP_ENABLE
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB | RCC_APB2Periph_AFIO, ENABLE);
    gpio_init.GPIO_Pin = PIN_SCI1_RX;
    gpio_init.GPIO_Mode = GPIO_Mode_IN_FLOATING;
    GPIO_Init(GPIO_SCI1_RX, &gpio_init);
    GPIO_EXTILineConfig(GPIO_PortSourceGPIOB, GPIO_PinSource7);
    exti_init.EXTI_Line = EXTI_Line7;
    exti_init.EXTI_Mode = EXTI_Mode_Interrupt;
    exti_init.EXTI_Trigger = EXTI_Trigger_Rising;
    exti_init.EXTI_LineCmd = ENABLE;
    EXTI_Init(&exti_init);
    nvic_init.NVIC_IRQChannel = EXTI9_5_IRQn;
    nvic_init.NVIC_IRQChannelPreemptionPriority = 0x01;
    nvic_init.NVIC_IRQChannelSubPriority = 0x01;
    nvic_init.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&nvic_init);
#endif

#ifdef RS485_WAKEUP_ENABLE
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB | RCC_APB2Periph_AFIO, ENABLE);
    gpio_init.GPIO_Pin = PIN_INT_WK_CMNT;
    gpio_init.GPIO_Mode = GPIO_Mode_IN_FLOATING;
    GPIO_Init(GPIO_INT_WK_CMNT, &gpio_init);
    GPIO_EXTILineConfig(GPIO_PortSourceGPIOB, GPIO_PinSource12);
    exti_init.EXTI_Line = EXTI_Line12;
    exti_init.EXTI_Mode = EXTI_Mode_Interrupt;
    exti_init.EXTI_Trigger = EXTI_Trigger_Rising;
    exti_init.EXTI_LineCmd = ENABLE;
    EXTI_Init(&exti_init);
    nvic_init.NVIC_IRQChannel = EXTI15_10_IRQn;
    nvic_init.NVIC_IRQChannelPreemptionPriority = 0x01;
    nvic_init.NVIC_IRQChannelSubPriority = 0x01;
    nvic_init.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&nvic_init);
#endif
}

void InitWakeUp_RTCMode(void)
{
    InitWakeUp_NormalMode();
    RTC_WKTimeConfig();
}

void InitWakeUp_DeepMode(void)
{
    InitWakeUp_Base();
}

void IOstatus_Base(void)
{
    GPIO_InitTypeDef gpio_init;

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA |
                               RCC_APB2Periph_GPIOB |
                               RCC_APB2Periph_GPIOC |
                               RCC_APB2Periph_GPIOD |
                               RCC_APB2Periph_GPIOE,
                           ENABLE);

    LedBar_SetSleep(1u);
    ADC_StopForLowPower();

    gpio_init.GPIO_Pin = GPIO_Pin_All;
    gpio_init.GPIO_Mode = GPIO_Mode_AIN;
    GPIO_Init(GPIOA, &gpio_init);
    GPIO_Init(GPIOB, &gpio_init);
    GPIO_Init(GPIOC, &gpio_init);
    GPIO_Init(GPIOD, &gpio_init);
    GPIO_Init(GPIOE, &gpio_init);

    GPIO_ResetBits(GPIO_M_STB, PIN_M_STB);
    GPIO_ResetBits(GPIO_AD_EN, PIN_AD_EN);
    GPIO_SetBits(GPIO_CMNT_EN, PIN_CMNT_EN);
    GPIO_ResetBits(GPIO_ADC_BUS_EN, PIN_ADC_BUS_EN);

    gpio_init.GPIO_Pin = PIN_M_STB;
    gpio_init.GPIO_Mode = GPIO_Mode_Out_PP;
    gpio_init.GPIO_Speed = GPIO_Speed_2MHz;
    GPIO_Init(GPIO_M_STB, &gpio_init);

    gpio_init.GPIO_Pin = PIN_AD_EN;
    gpio_init.GPIO_Mode = GPIO_Mode_Out_PP;
    gpio_init.GPIO_Speed = GPIO_Speed_2MHz;
    GPIO_Init(GPIO_AD_EN, &gpio_init);

    gpio_init.GPIO_Pin = PIN_CMNT_EN;
    gpio_init.GPIO_Mode = GPIO_Mode_Out_PP;
    gpio_init.GPIO_Speed = GPIO_Speed_2MHz;
    GPIO_Init(GPIO_CMNT_EN, &gpio_init);

    gpio_init.GPIO_Pin = PIN_ADC_BUS_EN;
    gpio_init.GPIO_Mode = GPIO_Mode_Out_PP;
    gpio_init.GPIO_Speed = GPIO_Speed_2MHz;
    GPIO_Init(GPIO_ADC_BUS_EN, &gpio_init);

    InitSocKeyInput();
    LedBar_PrepareForStop();
}

void IOstatus_RTCMode(void)
{
    GPIO_InitTypeDef gpio_init;

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA |
                               RCC_APB2Periph_GPIOB |
                               RCC_APB2Periph_GPIOC |
                               RCC_APB2Periph_GPIOD |
                               RCC_APB2Periph_GPIOE,
                           ENABLE);

    LedBar_SetSleep(1u);
    ADC_StopForLowPower();

    gpio_init.GPIO_Pin = GPIO_Pin_All;
    gpio_init.GPIO_Mode = GPIO_Mode_AIN;
    GPIO_Init(GPIOA, &gpio_init);
    gpio_init.GPIO_Pin = GPIO_Pin_All & (~PIN_AFE1_CTL);
    gpio_init.GPIO_Mode = GPIO_Mode_AIN;
    GPIO_Init(GPIOB, &gpio_init);
    gpio_init.GPIO_Pin = GPIO_Pin_All;
    gpio_init.GPIO_Mode = GPIO_Mode_AIN;
    GPIO_Init(GPIOC, &gpio_init);
    GPIO_Init(GPIOD, &gpio_init);
    GPIO_Init(GPIOE, &gpio_init);

    GPIO_ResetBits(GPIO_M_STB, PIN_M_STB);
    GPIO_ResetBits(GPIO_AD_EN, PIN_AD_EN);
    GPIO_SetBits(GPIO_CMNT_EN, PIN_CMNT_EN);
    GPIO_ResetBits(GPIO_ADC_BUS_EN, PIN_ADC_BUS_EN);

    gpio_init.GPIO_Pin = PIN_M_STB;
    gpio_init.GPIO_Mode = GPIO_Mode_Out_PP;
    gpio_init.GPIO_Speed = GPIO_Speed_2MHz;
    GPIO_Init(GPIO_M_STB, &gpio_init);

    gpio_init.GPIO_Pin = PIN_AD_EN;
    gpio_init.GPIO_Mode = GPIO_Mode_Out_PP;
    gpio_init.GPIO_Speed = GPIO_Speed_2MHz;
    GPIO_Init(GPIO_AD_EN, &gpio_init);

    gpio_init.GPIO_Pin = PIN_CMNT_EN;
    gpio_init.GPIO_Mode = GPIO_Mode_Out_PP;
    gpio_init.GPIO_Speed = GPIO_Speed_2MHz;
    GPIO_Init(GPIO_CMNT_EN, &gpio_init);

    gpio_init.GPIO_Pin = PIN_ADC_BUS_EN;
    gpio_init.GPIO_Mode = GPIO_Mode_Out_PP;
    gpio_init.GPIO_Speed = GPIO_Speed_2MHz;
    GPIO_Init(GPIO_ADC_BUS_EN, &gpio_init);

    InitSocKeyInput();
    LedBar_PrepareForStop();
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
    MCU_RESET();
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
    InitIO_rtc();

    ADC_StopForLowPower();
    InitADC();

#ifdef __FUNC__HEAT__
    InitHeat_Cool();
#endif

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
