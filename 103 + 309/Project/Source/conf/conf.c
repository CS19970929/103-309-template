// #include "conf.h"
#include "main.h"

Time_T sys_time = {
    .time_enter_rtc = 60,
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

static GPIO_TypeDef * const s_gpio_ports[] = { GPIOA, GPIOB, GPIOC, GPIOD, GPIOE };
static const uint8_t s_gpio_port_sources[] = {
    GPIO_PortSourceGPIOA, GPIO_PortSourceGPIOB, GPIO_PortSourceGPIOC,
    GPIO_PortSourceGPIOD, GPIO_PortSourceGPIOE
};

static void Conf_InitGpioMode(GPIO_TypeDef *gpio, uint16_t pin,
                              GPIOMode_TypeDef mode, GPIOSpeed_TypeDef speed)
{
    GPIO_InitTypeDef config;

    config.GPIO_Pin = pin;
    config.GPIO_Mode = mode;
    config.GPIO_Speed = speed;
    GPIO_Init(gpio, &config);
}

void Board_EnableIrq(const BoardIrqConfig *config)
{
    NVIC_InitTypeDef nvic;

    nvic.NVIC_IRQChannel = config->channel;
    nvic.NVIC_IRQChannelPreemptionPriority = config->preemption_priority;
    nvic.NVIC_IRQChannelSubPriority = config->sub_priority;
    nvic.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&nvic);
}

void Board_InitIoGroup(BoardIoGroup group)
{
    uint32_t i;

    RCC_APB2PeriphClockCmd(CONF_APB2_GPIO_CLOCKS, ENABLE);
    /* Preload ALL power latches before enabling any output in the group. */
    for (i = 0U; i < BOARD_IO_COUNT; ++i) {
        const BoardIoConfig *io = &g_board_io[i];
        if (io->init_group == group &&
            (io->initial_level == BOARD_LOW_BEFORE_MODE ||
             io->initial_level == BOARD_HIGH_BEFORE_MODE)) {
            GPIO_WriteBit(io->gpio, BOARD_IO_PIN(i),
                io->initial_level == BOARD_HIGH_BEFORE_MODE ? Bit_SET : Bit_RESET);
        }
    }
    for (i = 0U; i < BOARD_IO_COUNT; ++i) {
        const BoardIoConfig *io = &g_board_io[i];
        if (io->init_group == group) {
            Conf_InitGpioMode(io->gpio, BOARD_IO_PIN(i), io->mode, io->speed);
        }
    }
    /* AFE CTL and software I2C retain their existing mode-before-level order. */
    for (i = 0U; i < sizeof(s_gpio_ports) / sizeof(s_gpio_ports[0]); ++i) {
        uint32_t id;
        uint16_t high_pins = 0U;
        uint16_t low_pins = 0U;
        for (id = 0U; id < BOARD_IO_COUNT; ++id) {
            const BoardIoConfig *io = &g_board_io[id];
            if (io->init_group == group && io->gpio == s_gpio_ports[i]) {
                if (io->initial_level == BOARD_HIGH_AFTER_MODE) high_pins |= BOARD_IO_PIN(id);
                if (io->initial_level == BOARD_LOW_AFTER_MODE) low_pins |= BOARD_IO_PIN(id);
            }
        }
        if (high_pins != 0U) GPIO_SetBits(s_gpio_ports[i], high_pins);
        if (low_pins != 0U) GPIO_ResetBits(s_gpio_ports[i], low_pins);
    }
}

static uint32_t Conf_WakeupLine(BoardWakeupId id)
{
    return BOARD_IO_PIN(g_board_wakeup[id].io);
}

static void Conf_InitWakeupInputExti(const BoardWakeupConfig *config)
{
    const BoardIoConfig *io = &g_board_io[config->io];
    EXTI_InitTypeDef exti;
    uint32_t i;

    /* AFIO source is selected from the same port as the GPIO row. */
    for (i = 0U; i < sizeof(s_gpio_ports) / sizeof(s_gpio_ports[0]); ++i) {
        if (s_gpio_ports[i] == io->gpio) {
            Conf_InitGpioMode(io->gpio, BOARD_IO_PIN(config->io), io->mode, io->speed);
            GPIO_EXTILineConfig(s_gpio_port_sources[i], io->pin_source);
            exti.EXTI_Line = BOARD_IO_PIN(config->io);
            exti.EXTI_Mode = EXTI_Mode_Interrupt;
            exti.EXTI_Trigger = config->trigger;
            exti.EXTI_LineCmd = ENABLE;
            EXTI_Init(&exti);
            Board_EnableIrq(&config->irq);
            return;
        }
    }
}

static void LowPower_ConfigWakeupExti(uint32_t line, EXTITrigger_TypeDef trigger, FunctionalState cmd)
{
    EXTI_InitTypeDef exti;

    exti.EXTI_Line = line;
    exti.EXTI_Mode = EXTI_Mode_Interrupt;
    exti.EXTI_Trigger = trigger;
    exti.EXTI_LineCmd = cmd;
    EXTI_Init(&exti);
}

static void Conf_SetPortsAnalog(uint8_t keep_mask)
{
    uint32_t port;
    uint32_t i;

    for (port = 0U; port < sizeof(s_gpio_ports) / sizeof(s_gpio_ports[0]); ++port) {
        uint16_t pins = GPIO_Pin_All;
        for (i = 0U; i < BOARD_IO_COUNT; ++i) {
            if (g_board_io[i].gpio == s_gpio_ports[port] &&
                (g_board_io[i].stop_keep & keep_mask) != 0U) {
                pins &= (uint16_t)~BOARD_IO_PIN(i);
            }
        }
        if (pins != 0U) Conf_InitGpioMode(s_gpio_ports[port], pins, GPIO_Mode_AIN, GPIO_Speed_2MHz);
    }
}

static void Conf_PrepareStopEntry(void)
{
    Sci_StopForLowPower();
    Fault_PauseMeasurements();
    ADC_StopForLowPower();
}

void LowPower_ClearWakeupPending(void)
{
    EXTI_ClearITPendingBit(Conf_WakeupLine(BOARD_WAKE_UART2));
    EXTI_ClearITPendingBit(Conf_WakeupLine(BOARD_WAKE_CHARGER));
    EXTI_ClearITPendingBit(Conf_WakeupLine(BOARD_WAKE_SWITCH));
    EXTI_ClearITPendingBit(Conf_WakeupLine(BOARD_WAKE_CMNT));
    EXTI_ClearITPendingBit(EXTI_Line13);
#if defined(UART1_WAKEUP_ENABLE)
    EXTI_ClearITPendingBit(Conf_WakeupLine(BOARD_WAKE_UART1));
#endif
    NVIC_ClearPendingIRQ(EXTI0_IRQn);
    NVIC_ClearPendingIRQ(EXTI3_IRQn);
    NVIC_ClearPendingIRQ(EXTI9_5_IRQn);
    NVIC_ClearPendingIRQ(EXTI15_10_IRQn);
}

void LowPower_DisableWakeupExti(void)
{
    LowPower_DisableUartWakeup();
    LowPower_ConfigWakeupExti(Conf_WakeupLine(BOARD_WAKE_CHARGER), EXTI_Trigger_Falling, DISABLE);
    LowPower_ConfigWakeupExti(Conf_WakeupLine(BOARD_WAKE_SWITCH), EXTI_Trigger_Falling, DISABLE);
    LowPower_ConfigWakeupExti(Conf_WakeupLine(BOARD_WAKE_CMNT), EXTI_Trigger_Rising, DISABLE);
    LowPower_ConfigWakeupExti(EXTI_Line13, EXTI_Trigger_Rising, DISABLE);
    LowPower_ConfigWakeupExti(EXTI_Line17, EXTI_Trigger_Rising, DISABLE);
    LowPower_ClearWakeupPending();
}

void LowPower_DisableUartWakeup(void)
{
    LowPower_ConfigWakeupExti(Conf_WakeupLine(BOARD_WAKE_UART1), EXTI_Trigger_Falling, DISABLE);
    LowPower_ConfigWakeupExti(Conf_WakeupLine(BOARD_WAKE_UART2), EXTI_Trigger_Falling, DISABLE);
    EXTI_ClearITPendingBit(Conf_WakeupLine(BOARD_WAKE_UART1));
    EXTI_ClearITPendingBit(Conf_WakeupLine(BOARD_WAKE_UART2));
}

static void Conf_InitUartWakeup(void)
{
#if defined(UART1_WAKEUP_ENABLE) && defined(_COMMOM_UPPER_SCI1)
    Conf_InitWakeupInputExti(&g_board_wakeup[BOARD_WAKE_UART1]);
#endif
#if defined(RS485_WAKEUP_ENABLE) && defined(_COMMOM_UPPER_SCI2)
    Conf_InitWakeupInputExti(&g_board_wakeup[BOARD_WAKE_UART2]);
#endif
}

void InitIO_rtc(void)
{
    RCC_APB2PeriphClockCmd(CONF_APB2_IO_CLOCKS, ENABLE);
    Board_InitIoGroup(BOARD_INIT_RUN_SHARED);
    Board_InitIoGroup(BOARD_INIT_POWER);
}

void InitIO(void)
{
    RCC_APB2PeriphClockCmd(CONF_APB2_IO_CLOCKS, ENABLE);
    Board_InitIoGroup(BOARD_INIT_AFE_CONTROL);
    Board_InitIoGroup(BOARD_INIT_RUN_SHARED);
    Board_InitIoGroup(BOARD_INIT_POWER);
    Board_InitIoGroup(BOARD_INIT_DEBUG_LED);
}

void InitWakeUp_Base(void)
{
    RCC_APB2PeriphClockCmd(CONF_APB2_WAKEUP_CLOCKS, ENABLE);
    jtag_disableAndConfIO();
    Conf_InitWakeupInputExti(&g_board_wakeup[BOARD_WAKE_CHARGER]);
    Conf_InitWakeupInputExti(&g_board_wakeup[BOARD_WAKE_SWITCH]);
}

void InitWakeUp_RTCMode(void)
{
    RCC_APB2PeriphClockCmd(CONF_APB2_WAKEUP_CLOCKS, ENABLE);
    jtag_disableAndConfIO();
    Conf_InitWakeupInputExti(&g_board_wakeup[BOARD_WAKE_SWITCH]);
    Conf_InitUartWakeup();
    Conf_InitWakeupInputExti(&g_board_wakeup[BOARD_WAKE_CMNT]);
}

void InitWakeUp_DeepMode(void)
{
    InitWakeUp_Base();
}

void IOstatus_Base(void)
{
    RCC_APB2PeriphClockCmd(CONF_APB2_GPIO_CLOCKS, ENABLE);
    Conf_SetPortsAnalog(BOARD_KEEP_NONE);
    Conf_PrepareStopEntry();
}

void IOstatus_RTCMode(void)
{
    RCC_APB2PeriphClockCmd(CONF_APB2_GPIO_CLOCKS, ENABLE);
    Conf_SetPortsAnalog(g_stLowPowerRtcStatus.mode == NORMAL_MODE ?
        BOARD_KEEP_NORMAL : BOARD_KEEP_HICCUP);
    Conf_PrepareStopEntry();
}

void IOstatus_DeepMode(void)
{
    IOstatus_Base();
}

void Sys_StopMode(void)
{
    UINT32 primask = __get_PRIMASK();
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_PWR, ENABLE);
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM3, ENABLE);
    TIM_Cmd(TIM3, DISABLE);
    TIM_ClearITPendingBit(TIM3, TIM_IT_Update);
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM3, DISABLE);
    LowPower_ClearWakeupPending();

    __disable_irq();
    /* A communication IRQ handled during STOP preparation must not be lost
     * when pending bits are cleared. New masked IRQs still wake WFI normally. */
    if (!g_stLowPowerRtcStatus.comm_wakeup) {
        PWR_EnterSTOPMode(PWR_Regulator_LowPower, PWR_STOPEntry_WFI);
    }

    cpu_frequency_conf();
    if (primask == 0U) __enable_irq();
}

void InitRunAfterStopWakeup(void)
{
    InitDelay();
    RTC_RestoreRunInterrupts();
    InitIO_rtc();

    ADC_StopForLowPower();

    USART_DeInit(USART1);
    USART_DeInit(USART2);

    InitTimer();
    InitUSART_CommonUpper();
    InitCan();
    InitADC();

    /* Wakeup EXTI is configured only when entering STOP. Keeping it armed in
       run mode can leave stale pending bits for the next low-power cycle. */

    initAFE1_IIC();
}
