// #include "conf.h"
#include "main.h"
#include "AppInit.h"

Time_T sys_time = {
    .time_enter_rtc = 10,
    .power_on = false,
};

#define CONF_APB2_GPIO_CLOCKS (RCC_APB2Periph_GPIOA | \
                               RCC_APB2Periph_GPIOB | \
                               RCC_APB2Periph_GPIOC | \
                               RCC_APB2Periph_GPIOD | \
                               RCC_APB2Periph_GPIOE)
#define CONF_APB2_IO_CLOCKS (RCC_APB2Periph_AFIO | CONF_APB2_GPIO_CLOCKS)
#define CONF_APB2_WAKEUP_CLOCKS (RCC_APB2Periph_AFIO | \
                                 RCC_APB2Periph_GPIOA | \
                                 RCC_APB2Periph_GPIOB | \
                                 RCC_APB2Periph_GPIOC)

static void Conf_InitGpioMode(GPIO_TypeDef *gpio, uint16_t pin, GPIOMode_TypeDef mode)
{
    GPIO_InitTypeDef GPIO_InitStructure;

    GPIO_InitStructure.GPIO_Pin = pin;
    GPIO_InitStructure.GPIO_Mode = mode;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_2MHz;
    GPIO_Init(gpio, &GPIO_InitStructure);
}

static void Conf_InitWakeupInputExti(GPIO_TypeDef *gpio,
                                     uint16_t pin,
                                     uint8_t port_source,
                                     uint8_t pin_source,
                                     uint32_t exti_line,
                                     EXTITrigger_TypeDef trigger,
                                     uint8_t irq_channel)
{
    EXTI_InitTypeDef EXTI_InitStruct;
    NVIC_InitTypeDef NVIC_InitStructure;

    Conf_InitGpioMode(gpio, pin, GPIO_Mode_IN_FLOATING);
    GPIO_EXTILineConfig(port_source, pin_source);

    EXTI_InitStruct.EXTI_Line = exti_line;
    EXTI_InitStruct.EXTI_Mode = EXTI_Mode_Interrupt;
    EXTI_InitStruct.EXTI_Trigger = trigger;
    EXTI_InitStruct.EXTI_LineCmd = ENABLE;
    EXTI_Init(&EXTI_InitStruct);

    NVIC_InitStructure.NVIC_IRQChannel = irq_channel;
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 0x01;
    NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0x01;
    NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&NVIC_InitStructure);
}

static void LowPower_ConfigWakeupExti(uint32_t line, EXTITrigger_TypeDef trigger, FunctionalState cmd)
{
    EXTI_InitTypeDef EXTI_InitStruct;

    EXTI_InitStruct.EXTI_Line = line;
    EXTI_InitStruct.EXTI_Mode = EXTI_Mode_Interrupt;
    EXTI_InitStruct.EXTI_Trigger = trigger;
    EXTI_InitStruct.EXTI_LineCmd = cmd;
    EXTI_Init(&EXTI_InitStruct);
}

static void Conf_InitMainPowerRails(BitAction m_stb,
                                    BitAction ad_en,
                                    BitAction cmnt_en,
                                    BitAction adc_bus_en,
                                    BitAction dc_en,
                                    BitAction boost_en)
{
    GPIO_WriteBit(GPIO_M_STB, PIN_M_STB, m_stb);
    GPIO_WriteBit(GPIO_AD_EN, PIN_AD_EN, ad_en);
    GPIO_WriteBit(GPIO_CMNT_EN, PIN_CMNT_EN, cmnt_en);
    GPIO_WriteBit(GPIO_ADC_BUS_EN, PIN_ADC_BUS_EN, adc_bus_en);
    GPIO_WriteBit(GPIO_DC_EN, PIN_DC_EN, dc_en);
    GPIO_WriteBit(GPIO_2727_EN, PIN_2737_EN, boost_en);

    Conf_InitGpioMode(GPIO_M_STB, PIN_M_STB, GPIO_Mode_Out_PP);
    Conf_InitGpioMode(GPIO_AD_EN, PIN_AD_EN, GPIO_Mode_Out_PP);
    Conf_InitGpioMode(GPIO_CMNT_EN, PIN_CMNT_EN, GPIO_Mode_Out_PP);
    Conf_InitGpioMode(GPIO_ADC_BUS_EN, PIN_ADC_BUS_EN, GPIO_Mode_Out_PP);
    Conf_InitGpioMode(GPIO_DC_EN, PIN_DC_EN, GPIO_Mode_Out_PP);
    Conf_InitGpioMode(GPIO_2727_EN, PIN_2737_EN, GPIO_Mode_Out_PP);
}

static void Conf_InitRunSharedIo(void)
{
    Conf_InitGpioMode(GPIO_CHG_IN, PIN_CHG_IN, GPIO_Mode_IN_FLOATING);

    Conf_InitGpioMode(GPIO_MCC_C, PIN_MCC_C, GPIO_Mode_Out_PP);
    GPIO_WriteBit(GPIO_MCC_C, PIN_MCC_C, Bit_RESET);

    Conf_InitGpioMode(GPIO_MCU_WK, PIN_MCU_WK, GPIO_Mode_IN_FLOATING);
    Conf_InitGpioMode(GPIO_SW, PIN_SW, GPIO_Mode_IN_FLOATING);
    Conf_InitGpioMode(GPIO_RF_EN, PIN_RF_EN, GPIO_Mode_Out_PP);
    Conf_InitGpioMode(GPIOA, PIN_ADC_VBUS | PIN_ADC_CUR, GPIO_Mode_AIN);
    Conf_InitGpioMode(GPIOB, PIN_ADC_NMOS, GPIO_Mode_AIN);

    Conf_InitMainPowerRails(Bit_SET,
                            Bit_SET,
                            Bit_RESET,
                            Bit_SET,
                            Bit_SET,
                            Bit_SET);

    Conf_InitGpioMode(GPIO_DBG_LED, PIN_DBG_LED, GPIO_Mode_Out_PP);
}

static void Conf_PrepareStopEntry(void)
{
    LedBar_SetSleep(1u);
    ADC_StopForLowPower();
}

static void Conf_InitAllPortsAnalog(void)
{
    Conf_InitGpioMode(GPIOA, GPIO_Pin_All, GPIO_Mode_AIN);
    Conf_InitGpioMode(GPIOB, GPIO_Pin_All, GPIO_Mode_AIN);
    Conf_InitGpioMode(GPIOC, GPIO_Pin_All, GPIO_Mode_AIN);
    Conf_InitGpioMode(GPIOD, GPIO_Pin_All, GPIO_Mode_AIN);
    Conf_InitGpioMode(GPIOE, GPIO_Pin_All, GPIO_Mode_AIN);
}

void LowPower_ClearWakeupPending(void)
{
    EXTI_ClearITPendingBit(EXTI_Line0);
    EXTI_ClearITPendingBit(EXTI_Line9);
    EXTI_ClearITPendingBit(EXTI_Line12);
    EXTI_ClearITPendingBit(EXTI_Line13);

#if defined(UART1_WAKEUP_ENABLE)
    EXTI_ClearITPendingBit(EXTI_Line7);
#endif
#if defined(UART2_WAKEUP_ENABLE)
    EXTI_ClearITPendingBit(EXTI_Line3);
#endif

    NVIC_ClearPendingIRQ(EXTI0_IRQn);
    NVIC_ClearPendingIRQ(EXTI9_5_IRQn);
    NVIC_ClearPendingIRQ(EXTI15_10_IRQn);
#if defined(UART2_WAKEUP_ENABLE)
    NVIC_ClearPendingIRQ(EXTI3_IRQn);
#endif
}

void LowPower_DisableWakeupExti(void)
{
    LowPower_ConfigWakeupExti(EXTI_Line0, EXTI_Trigger_Falling, DISABLE);
    LowPower_ConfigWakeupExti(EXTI_Line9, EXTI_Trigger_Falling, DISABLE);
    LowPower_ConfigWakeupExti(EXTI_Line12, EXTI_Trigger_Rising, DISABLE);
    LowPower_ConfigWakeupExti(EXTI_Line13, EXTI_Trigger_Rising, DISABLE);

#if defined(UART1_WAKEUP_ENABLE)
    LowPower_ConfigWakeupExti(EXTI_Line7, EXTI_Trigger_Rising, DISABLE);
#endif
#if defined(UART2_WAKEUP_ENABLE)
    LowPower_ConfigWakeupExti(EXTI_Line3, EXTI_Trigger_Rising, DISABLE);
#endif

    LowPower_ClearWakeupPending();
}

void InitIO_rtc(void)
{
    RCC_APB2PeriphClockCmd(CONF_APB2_IO_CLOCKS, ENABLE);

    Conf_InitRunSharedIo();
}

void InitIO(void)
{
    RCC_APB2PeriphClockCmd(CONF_APB2_IO_CLOCKS, ENABLE);

    {
        // GPIO_InitStructure.GPIO_Pin = PIN_AFE1_ALM | PIN_AFE1_MODE | PIN_AFE1_SHIP;
        // GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
        // GPIO_InitStructure.GPIO_Speed = GPIO_Speed_2MHz; // IO口速度为2MHz
        // GPIO_Init(GPIOA, &GPIO_InitStructure);

        Conf_InitGpioMode(GPIOB, PIN_AFE1_PRO_EN | PIN_AFE1_CTL, GPIO_Mode_Out_PP);
        MCUO_AFE_CTLC = 0;
    }

    Conf_InitRunSharedIo();
}

void InitWakeUp_Base(void)
{
    RCC_APB2PeriphClockCmd(CONF_APB2_WAKEUP_CLOCKS, ENABLE);

    jtag_disableAndConfIO();
#if 1
    Conf_InitWakeupInputExti(GPIO_CHG_IN,
                             PIN_CHG_IN,
                             GPIO_PortSourceGPIOA,
                             GPIO_PinSource0,
                             EXTI_Line0,
                             EXTI_Trigger_Falling,
                             EXTI0_IRQn);
    Conf_InitWakeupInputExti(GPIO_SW,
                             PIN_SW,
                             GPIO_PortSourceGPIOA,
                             GPIO_PinSource9,
                             EXTI_Line9,
                             EXTI_Trigger_Falling,
                             EXTI9_5_IRQn);
#endif
}

void InitWakeUp_NormalMode(void)
{
    InitWakeUp_Base();

    {
#ifdef UART1_WAKEUP_ENABLE
        Conf_InitWakeupInputExti(GPIO_SCI1_RX,
                                 PIN_SCI1_RX,
                                 GPIO_PortSourceGPIOB,
                                 GPIO_PinSource7,
                                 EXTI_Line7,
                                 EXTI_Trigger_Rising,
                                 EXTI9_5_IRQn);
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
    Conf_InitWakeupInputExti(GPIO_CHG_IN,
                             PIN_CHG_IN,
                             GPIO_PortSourceGPIOA,
                             GPIO_PinSource0,
                             EXTI_Line0,
                             EXTI_Trigger_Falling,
                             EXTI0_IRQn);
    Conf_InitWakeupInputExti(GPIO_INT_WK_CMNT,
                             PIN_INT_WK_CMNT,
                             GPIO_PortSourceGPIOB,
                             GPIO_PinSource12,
                             EXTI_Line12,
                             EXTI_Trigger_Rising,
                             EXTI15_10_IRQn);
    Conf_InitWakeupInputExti(GPIO_MCU_WK,
                             PIN_MCU_WK,
                             GPIO_PortSourceGPIOB,
                             GPIO_PinSource13,
                             EXTI_Line13,
                             EXTI_Trigger_Rising,
                             EXTI15_10_IRQn);
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
    RCC_APB2PeriphClockCmd(CONF_APB2_GPIO_CLOCKS, ENABLE);

    Conf_PrepareStopEntry();
    Conf_InitAllPortsAnalog();
    Conf_InitMainPowerRails(Bit_RESET,
                            Bit_RESET,
                            Bit_SET,
                            Bit_RESET,
                            Bit_RESET,
                            Bit_RESET);

    LedBar_PrepareForStop();
}

void IOstatus_RTCMode(void)
{
    RCC_APB2PeriphClockCmd(CONF_APB2_GPIO_CLOCKS, ENABLE);

    Conf_PrepareStopEntry();

    // GPIO_InitStructure.GPIO_Pin = GPIO_Pin_All;
    // GPIO_InitStructure.GPIO_Pin = GPIO_Pin_All & (~PIN_DC_EN) & (~PIN_2737_EN);
    Conf_InitGpioMode(GPIOA, GPIO_Pin_All & (~PIN_2737_EN), GPIO_Mode_AIN);

    // GPIO_InitStructure.GPIO_Pin = GPIO_Pin_All;
    Conf_InitGpioMode(GPIOB, GPIO_Pin_All & (~GPIO_Pin_14), GPIO_Mode_AIN);
#if 1
    Conf_InitGpioMode(GPIOC, GPIO_Pin_All, GPIO_Mode_AIN);
#else
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_All;
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_All & (~GPIO_Pin_4);
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AIN;
    GPIO_Init(GPIOC, &GPIO_InitStructure);
#endif

    Conf_InitGpioMode(GPIOD, GPIO_Pin_All, GPIO_Mode_AIN);
    Conf_InitGpioMode(GPIOE, GPIO_Pin_All, GPIO_Mode_AIN);

    GPIO_WriteBit(GPIO_DC_EN, PIN_DC_EN, Bit_RESET);
    Conf_InitGpioMode(GPIO_DC_EN, PIN_DC_EN, GPIO_Mode_Out_PP);

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
    LowPower_ClearWakeupPending();
    PWR_EnterSTOPMode(PWR_Regulator_LowPower, PWR_STOPEntry_WFI);

    cpu_frequency_conf();
}

void InitRtcWakeupCheck(void)
{
    InitRunAfterStopWakeup();
}

void InitRunAfterStopWakeup(void)
{
    is_wakeup = true;

    InitDelay();
    RTC_RestoreRunInterrupts();
    // InitIO();
    InitIO_rtc();

    ADC_StopForLowPower();
    InitADC();

#ifdef __FUNC__LED__
    APP_LedBar();
    set_LED_state(LED_BAR_NORMAL, 4);
#endif // DEBUG

    USART_DeInit(USART1);
    USART_DeInit(USART2);

    AppInit_InitSci();
    InitCan();
    InitTimer();

    sys_time.wakeup_rtc = is_rtc_wakekup ? true : false;
    /* Wakeup EXTI is configured only when entering STOP. Keeping it armed in
       run mode can leave stale pending bits for the next low-power cycle. */

    initAFE1_IIC();
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
