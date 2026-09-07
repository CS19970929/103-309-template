#include "main.h"

Time_T sys_time = {
    .time_enter_rtc = 10,
    .power_on = false,
};

#define CONF_APB2_GPIO_CLOCKS (RCC_APB2Periph_GPIOA | RCC_APB2Periph_GPIOB)
#define CONF_APB2_IO_CLOCKS (RCC_APB2Periph_AFIO | CONF_APB2_GPIO_CLOCKS)
#define CONF_APB2_WAKEUP_CLOCKS CONF_APB2_IO_CLOCKS

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
                                    BitAction cmnt_en)
{
    GPIO_WriteBit(GPIO_M_STB, PIN_M_STB, m_stb);
    GPIO_WriteBit(GPIO_AD_EN, PIN_AD_EN, ad_en);
    GPIO_WriteBit(GPIO_CMNT_EN, PIN_CMNT_EN, cmnt_en);

    Conf_InitGpioMode(GPIO_M_STB, PIN_M_STB, GPIO_Mode_Out_PP);
    Conf_InitGpioMode(GPIO_AD_EN, PIN_AD_EN, GPIO_Mode_Out_PP);
    Conf_InitGpioMode(GPIO_CMNT_EN, PIN_CMNT_EN, GPIO_Mode_Out_PP);
}

static void Conf_InitRunSharedIo(void)
{
    /* Keep the reference-board IO defaults used by the working AFE build. */
    Conf_InitGpioMode(GPIO_DBG_LED, PIN_DBG_LED, GPIO_Mode_Out_PP);
    Conf_InitGpioMode(GPIO_KEY1, PIN_KEY1, GPIO_Mode_IN_FLOATING);
    Conf_InitGpioMode(GPIO_CS_SPI, PIN_CS_SPI, GPIO_Mode_Out_PP);
    GPIO_ResetBits(GPIO_M_CCC, PIN_M_CCC);
    Conf_InitGpioMode(GPIO_M_CCC, PIN_M_CCC, GPIO_Mode_Out_PP);
    Conf_InitGpioMode(GPIO_INT_WK_MCU, PIN_INT_WK_MCU, GPIO_Mode_IN_FLOATING);

    Conf_InitMainPowerRails(Bit_SET, Bit_SET, Bit_SET);
    GPIO_ResetBits(GPIO_M_CCC, PIN_M_CCC);
    GPIO_SetBits(GPIO_CS_SPI, PIN_CS_SPI);
}

static void Conf_InitAllPortsAnalog(void)
{
    Conf_InitGpioMode(GPIOA, GPIO_Pin_All, GPIO_Mode_AIN);
    Conf_InitGpioMode(GPIOB, GPIO_Pin_All, GPIO_Mode_AIN);
}

void LowPower_ClearWakeupPending(void)
{
    EXTI_ClearITPendingBit(EXTI_Line0 | EXTI_Line3 | EXTI_Line5 | EXTI_Line7 | EXTI_Line12);
    NVIC_ClearPendingIRQ(EXTI0_IRQn);
    NVIC_ClearPendingIRQ(EXTI3_IRQn);
    NVIC_ClearPendingIRQ(EXTI9_5_IRQn);
    NVIC_ClearPendingIRQ(EXTI15_10_IRQn);
}

void LowPower_DisableWakeupExti(void)
{
    LowPower_ConfigWakeupExti(EXTI_Line0 | EXTI_Line3 | EXTI_Line5 | EXTI_Line7 | EXTI_Line12 | EXTI_Line17,
                              EXTI_Trigger_Rising, DISABLE);
    LowPower_ClearWakeupPending();
}

void InitIO_rtc(void)
{
    RCC_APB2PeriphClockCmd(CONF_APB2_IO_CLOCKS, ENABLE);
    jtag_disableAndConfIO();
    Conf_InitGpioMode(GPIO_INT_WK_MCU, PIN_INT_WK_MCU, GPIO_Mode_IN_FLOATING);
    Conf_InitGpioMode(GPIO_KEY1, PIN_KEY1, GPIO_Mode_IN_FLOATING);
    Conf_InitGpioMode(GPIO_M_CCC, PIN_M_CCC, GPIO_Mode_Out_PP);
    Conf_InitGpioMode(GPIO_DBG_LED, PIN_DBG_LED, GPIO_Mode_Out_PP);
    Conf_InitMainPowerRails(Bit_SET, Bit_SET, Bit_SET);
}

void InitIO(void)
{
    RCC_APB2PeriphClockCmd(CONF_APB2_IO_CLOCKS, ENABLE);

    Conf_InitRunSharedIo();
}

void InitWakeUp_Base(void)
{
    RCC_APB2PeriphClockCmd(CONF_APB2_WAKEUP_CLOCKS, ENABLE);
    jtag_disableAndConfIO();
    LowPower_ClearWakeupPending();
    Conf_InitWakeupInputExti(GPIO_INT_WK_MCU, PIN_INT_WK_MCU,
                             GPIO_PortSourceGPIOA, GPIO_PinSource0, EXTI_Line0,
                             EXTI_Trigger_Rising, EXTI0_IRQn);
    Conf_InitWakeupInputExti(GPIO_KEY1, PIN_KEY1,
                             GPIO_PortSourceGPIOB, GPIO_PinSource5, EXTI_Line5,
                             EXTI_Trigger_Falling, EXTI9_5_IRQn);
}

void InitWakeUp_NormalMode(void)
{
    InitWakeUp_Base();
    Conf_InitWakeupInputExti(GPIO_INT_WK_CMNT, PIN_INT_WK_CMNT,
                             GPIO_PortSourceGPIOB, GPIO_PinSource12, EXTI_Line12,
                             EXTI_Trigger_Rising, EXTI15_10_IRQn);

#ifdef _COMMOM_UPPER_SCI1
    Conf_InitWakeupInputExti(GPIO_SCI1_RX, PIN_SCI1_RX,
                            GPIO_PortSourceGPIOB, GPIO_PinSource7, EXTI_Line7,
                            EXTI_Trigger_Falling, EXTI9_5_IRQn);
    Conf_InitGpioMode(GPIO_SCI1_RX, PIN_SCI1_RX, GPIO_Mode_IPU);
#endif
#ifdef _COMMOM_UPPER_SCI2
    Conf_InitWakeupInputExti(GPIO_SCI2_RX, PIN_SCI2_RX,
                            GPIO_PortSourceGPIOA, GPIO_PinSource3, EXTI_Line3,
                            EXTI_Trigger_Falling, EXTI3_IRQn);
    Conf_InitGpioMode(GPIO_SCI2_RX, PIN_SCI2_RX, GPIO_Mode_IPU);
#endif
}

void InitWakeUp_RTCMode(void)
{
    /* RTC timing is configured by the current RTC scheduler. */
    InitWakeUp_NormalMode();
}

void InitWakeUp_DeepMode(void)
{
    InitWakeUp_Base();
}

void IOstatus_Base(void)
{
    RCC_APB2PeriphClockCmd(CONF_APB2_GPIO_CLOCKS, ENABLE);
    Conf_InitAllPortsAnalog();
}

static void Conf_ParkRtcSpi(void)
{
    /* A deselected AFE must never see a floating CS between RTC inspections. */
    RCC_APB2PeriphClockCmd(CONF_APB2_IO_CLOCKS | RCC_APB2Periph_SPI1, ENABLE);
    SPI_Cmd(SPI1, DISABLE);
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_SPI1, DISABLE);
    GPIO_SetBits(GPIO_CS_SPI, PIN_CS_SPI);
    Conf_InitGpioMode(GPIO_CS_SPI, PIN_CS_SPI, GPIO_Mode_Out_PP);
    GPIO_SetBits(GPIO_SCLK_SPI, PIN_SCLK_SPI);
    GPIO_ResetBits(GPIO_MOSI_SPI, PIN_MOSI_SPI);
    Conf_InitGpioMode(GPIO_SCLK_SPI, PIN_SCLK_SPI, GPIO_Mode_Out_PP);
    Conf_InitGpioMode(GPIO_MOSI_SPI, PIN_MOSI_SPI, GPIO_Mode_Out_PP);
    Conf_InitGpioMode(GPIO_MISO_SPI, PIN_MISO_SPI, GPIO_Mode_AIN);
}

void IOstatus_RTCMode(void)
{
    /* F103 USART cannot receive in STOP. RX EXTI wakes the core; the first
       character is a wake preamble, not a guaranteed complete Modbus frame. */
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_USART1, ENABLE);
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_USART2, ENABLE);
    USART_Cmd(USART1, DISABLE);
    USART_Cmd(USART2, DISABLE);
    NVIC_DisableIRQ(USART1_IRQn);
    NVIC_DisableIRQ(USART2_IRQn);
    NVIC_ClearPendingIRQ(USART1_IRQn);
    NVIC_ClearPendingIRQ(USART2_IRQn);
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_USART1, DISABLE);
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_USART2, DISABLE);
    RCC_APB2PeriphClockCmd(CONF_APB2_GPIO_CLOCKS, ENABLE);
    Conf_InitGpioMode(GPIOA, GPIO_Pin_All, GPIO_Mode_AIN);
    /* Keep only M_CCC as in reference; no fictitious PRO_EN/BLE/LED rails. */
    Conf_InitGpioMode(GPIOB, GPIO_Pin_All & ~PIN_M_CCC, GPIO_Mode_AIN);
    Conf_InitMainPowerRails(Bit_RESET, Bit_RESET, Bit_RESET);
    Conf_ParkRtcSpi();
}

void IOstatus_NormalMode(void)
{
    IOstatus_Base();
}

void IOstatus_DeepMode(void)
{
    IOstatus_Base();
}

void Sys_StopMode(void)
{
    uint32_t primask = __get_PRIMASK();
    __disable_irq();
    if (g_irq_t != NO_IRQ ||
        GPIO_ReadInputDataBit(GPIO_KEY1, PIN_KEY1) == Bit_RESET ||
        GPIO_ReadInputDataBit(GPIO_INT_WK_MCU, PIN_INT_WK_MCU) == Bit_SET)
    {
        __set_PRIMASK(primask);
        return;
    }
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_PWR, ENABLE);
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM3, ENABLE);
    TIM_Cmd(TIM3, DISABLE);
    TIM_ClearITPendingBit(TIM3, TIM_IT_Update);
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM3, DISABLE);
    NVIC_ClearPendingIRQ(TIM3_IRQn);
    SysTick->CTRL = 0U;
    SCB->ICSR = SCB_ICSR_PENDSTCLR_Msk;
    EnableLowPowerDebug();
    if (g_stLowPowerRtcStatus.mode == HICCUP_MODE || g_stLowPowerRtcStatus.mode == NORMAL_MODE)
        Conf_ParkRtcSpi();
    /* Do not clear a just-arrived wake event. Pending IRQ wakes masked WFI. */
    __DSB();
    PWR_EnterSTOPMode(PWR_Regulator_LowPower, PWR_STOPEntry_WFI);
    __ISB();

    cpu_frequency_conf();
    __set_PRIMASK(primask);
}

void test_rtc_led_display(void)
{
    Conf_InitGpioMode(GPIO_DBG_LED, PIN_DBG_LED, GPIO_Mode_Out_PP);
    GPIO_ResetBits(GPIO_DBG_LED, PIN_DBG_LED);
}
void InitRunAfterStopWakeup(void)
{
    InitDelay();
    RTC_RestoreRunInterrupts();
    // InitIO();
    InitIO_rtc();

    /* SPL DeInit sequence for the fixed USART1 peripheral. */
    RCC_APB2PeriphResetCmd(RCC_APB2Periph_USART1, ENABLE);
    RCC_APB2PeriphResetCmd(RCC_APB2Periph_USART1, DISABLE);
    /* SPL DeInit sequence for the fixed USART2 peripheral. */
    RCC_APB1PeriphResetCmd(RCC_APB1Periph_USART2, ENABLE);
    RCC_APB1PeriphResetCmd(RCC_APB1Periph_USART2, DISABLE);

    InitUSART_CommonUpper();
    InitCan();
    InitTimer();

    if (g_irq_t == uart1_irq || g_irq_t == uart2_irq) SleepDeal_RecordExternalComm();
    sys_time.wakeup_rtc = (RTC_IsStopWakeup() != 0U) ? true : false;
    /* Wakeup EXTI is configured only when entering STOP. Keeping it armed in
       run mode can leave stale pending bits for the next low-power cycle. */

    Afe3520_RestorePort();
}
