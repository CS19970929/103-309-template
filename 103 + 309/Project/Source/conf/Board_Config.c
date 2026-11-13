#include "conf.h"

/* 每项只描述一个引脚。初始化分组保持原有上电顺序，KEEP 不主动改输出锁存值。
 * stop_keep 只保留既有状态；其余引脚在 STOP 准备阶段转为模拟输入。
 * RF_EN、AD_EN 等沿用板上网络名，未确认的硬件用途不在这里推断。 */
const BoardIoConfig g_board_io[BOARD_IO_COUNT] = {
    [BOARD_IO_AFE_PRO] = {
        .gpio = GPIOB, .pin_source = 0U,
        .mode = GPIO_Mode_Out_PP, .speed = GPIO_Speed_2MHz,
        .initial_level = BOARD_LEVEL_KEEP,
        .init_group = BOARD_INIT_AFE_CONTROL, .stop_keep = BOARD_KEEP_RTC
    },
    [BOARD_IO_AFE_CTL] = {
        .gpio = GPIOB, .pin_source = 14U,
        .mode = GPIO_Mode_Out_PP, .speed = GPIO_Speed_2MHz,
        .initial_level = BOARD_LOW_AFTER_MODE,
        .init_group = BOARD_INIT_AFE_CONTROL, .stop_keep = BOARD_KEEP_RTC
    },
    [BOARD_IO_CHG_IN] = {
        .gpio = GPIOA, .pin_source = 0U,
        .mode = GPIO_Mode_IN_FLOATING, .speed = GPIO_Speed_2MHz,
        .initial_level = BOARD_LEVEL_KEEP,
        .init_group = BOARD_INIT_RUN_SHARED, .stop_keep = BOARD_KEEP_NONE
    },
    [BOARD_IO_SWITCH] = {
        .gpio = GPIOA, .pin_source = 9U,
        .mode = GPIO_Mode_IN_FLOATING, .speed = GPIO_Speed_2MHz,
        .initial_level = BOARD_LEVEL_KEEP,
        .init_group = BOARD_INIT_RUN_SHARED, .stop_keep = BOARD_KEEP_NONE
    },
    [BOARD_IO_RF_EN] = {
        .gpio = GPIOA, .pin_source = 7U,
        .mode = GPIO_Mode_Out_PP, .speed = GPIO_Speed_2MHz,
        .initial_level = BOARD_LEVEL_KEEP,
        .init_group = BOARD_INIT_RUN_SHARED, .stop_keep = BOARD_KEEP_NONE
    },
    [BOARD_IO_M_STB] = {
        .gpio = GPIOA, .pin_source = 15U,
        .mode = GPIO_Mode_Out_PP, .speed = GPIO_Speed_2MHz,
        .initial_level = BOARD_HIGH_BEFORE_MODE,
        .init_group = BOARD_INIT_POWER, .stop_keep = BOARD_KEEP_NONE
    },
    [BOARD_IO_AD_EN] = {
        .gpio = GPIOB, .pin_source = 3U,
        .mode = GPIO_Mode_Out_PP, .speed = GPIO_Speed_2MHz,
        .initial_level = BOARD_LOW_BEFORE_MODE,
        .init_group = BOARD_INIT_POWER, .stop_keep = BOARD_KEEP_NONE
    },
    [BOARD_IO_BLE_EN] = {
        .gpio = GPIOB, .pin_source = 4U,
        .mode = GPIO_Mode_Out_PP, .speed = GPIO_Speed_2MHz,
        .initial_level = BOARD_LOW_BEFORE_MODE,
        .init_group = BOARD_INIT_POWER, .stop_keep = BOARD_KEEP_HICCUP
    },
    [BOARD_IO_CMNT_EN] = {
        .gpio = GPIOA, .pin_source = 4U,
        .mode = GPIO_Mode_Out_PP, .speed = GPIO_Speed_2MHz,
        .initial_level = BOARD_LOW_BEFORE_MODE,
        .init_group = BOARD_INIT_POWER, .stop_keep = BOARD_KEEP_NONE
    },
    [BOARD_IO_SW_EN] = {
        .gpio = GPIOB, .pin_source = 5U,
        .mode = GPIO_Mode_Out_PP, .speed = GPIO_Speed_2MHz,
        .initial_level = BOARD_HIGH_BEFORE_MODE,
        .init_group = BOARD_INIT_POWER, .stop_keep = BOARD_KEEP_NONE
    },
    [BOARD_IO_DEBUG_LED] = {
        .gpio = GPIOB, .pin_source = 15U,
        .mode = GPIO_Mode_Out_PP, .speed = GPIO_Speed_2MHz,
        .initial_level = BOARD_LEVEL_KEEP,
        .init_group = BOARD_INIT_DEBUG_LED, .stop_keep = BOARD_KEEP_RTC
    },
    [BOARD_IO_UART1_TX] = {
        .gpio = GPIOB, .pin_source = 6U,
        .mode = GPIO_Mode_AF_PP, .speed = GPIO_Speed_10MHz,
        .initial_level = BOARD_LEVEL_KEEP,
        .init_group = BOARD_INIT_UART1, .stop_keep = BOARD_KEEP_NONE
    },
    [BOARD_IO_UART1_RX] = {
        .gpio = GPIOB, .pin_source = 7U,
        .mode = GPIO_Mode_IN_FLOATING, .speed = GPIO_Speed_10MHz,
        .initial_level = BOARD_LEVEL_KEEP,
        .init_group = BOARD_INIT_UART1, .stop_keep = BOARD_KEEP_NONE
    },
    [BOARD_IO_UART2_TX] = {
        .gpio = GPIOA, .pin_source = 2U,
        .mode = GPIO_Mode_AF_PP, .speed = GPIO_Speed_10MHz,
        .initial_level = BOARD_LEVEL_KEEP,
        .init_group = BOARD_INIT_UART2, .stop_keep = BOARD_KEEP_NONE
    },
    [BOARD_IO_UART2_RX] = {
        .gpio = GPIOA, .pin_source = 3U,
        .mode = GPIO_Mode_IN_FLOATING, .speed = GPIO_Speed_10MHz,
        .initial_level = BOARD_LEVEL_KEEP,
        .init_group = BOARD_INIT_UART2, .stop_keep = BOARD_KEEP_NONE
    },
    [BOARD_IO_CAN_RX] = {
        .gpio = GPIOA, .pin_source = 11U,
        .mode = GPIO_Mode_IPU, .speed = GPIO_Speed_2MHz,
        .initial_level = BOARD_LEVEL_KEEP,
        .init_group = BOARD_INIT_CAN, .stop_keep = BOARD_KEEP_NONE
    },
    [BOARD_IO_CAN_TX] = {
        .gpio = GPIOA, .pin_source = 12U,
        .mode = GPIO_Mode_AF_PP, .speed = GPIO_Speed_2MHz,
        .initial_level = BOARD_LEVEL_KEEP,
        .init_group = BOARD_INIT_CAN, .stop_keep = BOARD_KEEP_NONE
    },
    [BOARD_IO_CMNT_WAKE] = {
        .gpio = GPIOB, .pin_source = 12U,
        .mode = GPIO_Mode_IN_FLOATING, .speed = GPIO_Speed_2MHz,
        .initial_level = BOARD_LEVEL_KEEP,
        .init_group = BOARD_INIT_WAKE_ONLY, .stop_keep = BOARD_KEEP_NONE
    },
    [BOARD_IO_MOS_NTC] = {
        .gpio = GPIOA, .pin_source = 1U,
        .mode = GPIO_Mode_AIN, .speed = GPIO_Speed_2MHz,
        .initial_level = BOARD_LEVEL_KEEP,
        .init_group = BOARD_INIT_ADC, .stop_keep = BOARD_KEEP_NONE
    },
    [BOARD_IO_AFE_SCL] = {
        .gpio = BOARD_AFE_I2C_GPIO, .pin_source = BOARD_AFE_I2C_SCL_SOURCE,
        .mode = GPIO_Mode_Out_PP, .speed = GPIO_Speed_2MHz,
        .initial_level = BOARD_HIGH_AFTER_MODE,
        .init_group = BOARD_INIT_AFE_I2C, .stop_keep = BOARD_KEEP_NONE
    },
    [BOARD_IO_AFE_SDA] = {
        .gpio = BOARD_AFE_I2C_GPIO, .pin_source = BOARD_AFE_I2C_SDA_SOURCE,
        .mode = GPIO_Mode_Out_PP, .speed = GPIO_Speed_2MHz,
        .initial_level = BOARD_HIGH_AFTER_MODE,
        .init_group = BOARD_INIT_AFE_I2C, .stop_keep = BOARD_KEEP_NONE
    }
};

/* EXTI line and AFIO pin source come from the corresponding IO row. */
const BoardWakeupConfig g_board_wakeup[BOARD_WAKE_COUNT] = {
    [BOARD_WAKE_CHARGER] = { BOARD_IO_CHG_IN,    EXTI_Trigger_Rising,         { EXTI0_IRQn,     1U, 1U } },
    [BOARD_WAKE_SWITCH]  = { BOARD_IO_SWITCH,    EXTI_Trigger_Rising_Falling, { EXTI9_5_IRQn,   1U, 1U } },
    [BOARD_WAKE_UART1]   = { BOARD_IO_UART1_RX,  EXTI_Trigger_Falling,        { EXTI9_5_IRQn,   1U, 1U } },
    [BOARD_WAKE_UART2]   = { BOARD_IO_UART2_RX,  EXTI_Trigger_Falling,        { EXTI3_IRQn,     1U, 1U } },
    [BOARD_WAKE_CMNT]    = { BOARD_IO_CMNT_WAKE, EXTI_Trigger_Rising,         { EXTI15_10_IRQn, 1U, 1U } }
};

/* Baud rate is still owned by persisted parameters; both ports retain 8N1. */
const BoardUartConfig g_board_uart1 = {
    .usart = USART1, .clock = RCC_APB2Periph_USART1, .apb2 = 1U,
    .remap = GPIO_Remap_USART1, .io_group = BOARD_INIT_UART1,
    .irq = { USART1_IRQn, 1U, 3U }
};
const BoardUartConfig g_board_uart2 = {
    .usart = USART2, .clock = RCC_APB1Periph_USART2, .apb2 = 0U,
    .remap = 0U, .io_group = BOARD_INIT_UART2,
    .irq = { USART2_IRQn, 1U, 3U }
};

const BoardCanConfig g_board_can = {
    .init = {
        .CAN_Prescaler = 2U,
        .CAN_Mode = CAN_Mode_Normal,
        .CAN_SJW = CAN_SJW_1tq,
        .CAN_BS1 = CAN_BS1_5tq,
        .CAN_BS2 = CAN_BS2_2tq,
        .CAN_TTCM = DISABLE,
        .CAN_ABOM = ENABLE,
        .CAN_AWUM = DISABLE,
        .CAN_NART = ENABLE,
        .CAN_RFLM = DISABLE,
        .CAN_TXFP = DISABLE
    },
    .irq = { USB_LP_CAN1_RX0_IRQn, 1U, 1U },
    .power_on_level = Bit_RESET,
    .power_off_level = Bit_SET
};

const BoardAdcInputConfig g_board_adc_inputs[ADC_INPUT_COUNT] = {
    [ADC_INPUT_MOS] = { ADC_Channel_1, ADC_SampleTime_239Cycles5 }
};

/* TIM3: 100 kHz / (999 + 1) = the existing 10 ms system tick. */
const BoardTimerConfig g_board_timer3 = {
    .counter_hz = 100000U, .period = 999U,
    .irq = { TIM3_IRQn, 0U, 3U }
};

const BoardIwdgConfig g_board_iwdg = {
#ifdef __FUNC_RTC__
    .prescaler = IWDG_Prescaler_256, .reload = 0x0FFFU
#else
    .prescaler = IWDG_Prescaler_64, .reload = 800U
#endif
};
