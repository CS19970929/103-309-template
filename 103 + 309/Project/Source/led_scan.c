#include "main.h"
#include "led_scan.h"

/* ── Route table: (low_pin, high_pin) for each Charlieplexing segment ── */
static const LedScanRoute s_ledscan_routes[LED_SCAN_ROUTE_COUNT] =
{
    {3u, 2u},  /* HUNDREDS_1_UPPER */
    {3u, 1u},  /* HUNDREDS_1_LOWER */
    {2u, 1u},  /* TENS_A */
    {1u, 2u},  /* TENS_B */
    {2u, 3u},  /* TENS_C */
    {1u, 3u},  /* TENS_D */
    {1u, 4u},  /* TENS_E */
    {2u, 4u},  /* TENS_F */
    {3u, 4u},  /* TENS_G */
    {1u, 0u},  /* ONES_A */
    {0u, 1u},  /* ONES_B */
    {2u, 0u},  /* ONES_C */
    {0u, 2u},  /* ONES_D */
    {3u, 0u},  /* ONES_E */
    {0u, 3u},  /* ONES_F */
    {0u, 4u},  /* ONES_G */
    {4u, 2u},  /* ICON_CHARGE */
    {4u, 1u},  /* ICON_PERCENT */
};

/* ── Pin definitions ── */
typedef struct
{
    GPIO_TypeDef *port;
    uint16_t      pin;
} LedScanPinDef;

static const LedScanPinDef s_ledscan_pins[LED_SCAN_PIN_COUNT] =
{
    {LED_SCAN_GPIO_P1, LED_SCAN_PIN_P1},
    {LED_SCAN_GPIO_P3, LED_SCAN_PIN_P3},
    {LED_SCAN_GPIO_P2, LED_SCAN_PIN_P2},
    {LED_SCAN_GPIO_P4, LED_SCAN_PIN_P4},
    {LED_SCAN_GPIO_P5, LED_SCAN_PIN_P5},
};

/* ── Internal scan state ── */
static uint8_t s_scan_timer_initialized = 0u;
static uint8_t s_scan_timer_enabled     = 0u;

/* ── Helpers ── */

static uint8_t LedScan_GetPinIndex(uint16_t pin)
{
    uint8_t index = 0u;

    if (pin == 0u)
    {
        return 0u;
    }

    while ((((pin >> index) & 0x1u) == 0u) && (index < 15u))
    {
        index++;
    }

    return index;
}

static void LedScan_PinModeF1(GPIO_TypeDef *port, uint32_t pin_index, uint32_t mode_bits)
{
    volatile uint32_t *config_reg;
    uint32_t shift;

    if (pin_index < 8u)
    {
        config_reg = &port->CRL;
        shift = pin_index * 4u;
    }
    else
    {
        config_reg = &port->CRH;
        shift = (pin_index - 8u) * 4u;
    }

    *config_reg &= ~(0xFu << shift);
    *config_reg |= (mode_bits << shift);
}

static void LedScan_PinToInput(uint8_t pin_id)
{
    GPIO_TypeDef *port = s_ledscan_pins[pin_id].port;
    uint32_t pin_index = (uint32_t)LedScan_GetPinIndex(s_ledscan_pins[pin_id].pin);

    LedScan_PinModeF1(port, pin_index, 0x4u);
}

static void LedScan_PinToOutput(uint8_t pin_id, BitAction level)
{
    GPIO_TypeDef *port = s_ledscan_pins[pin_id].port;
    uint16_t pin = s_ledscan_pins[pin_id].pin;
    uint32_t pin_index = (uint32_t)LedScan_GetPinIndex(pin);

    if (level != Bit_RESET)
    {
        port->BSRR = pin;
    }
    else
    {
        port->BRR = pin;
    }

    LedScan_PinModeF1(port, pin_index, 0x2u);
}

static void LedScan_AllPinsHiZ(void)
{
    uint8_t pin_id;

    for (pin_id = 0u; pin_id < LED_SCAN_PIN_COUNT; ++pin_id)
    {
        LedScan_PinToInput(pin_id);
    }
}

static void LedScan_AllPinsOutputLow(void)
{
    uint8_t pin_id;

    LedScan_AllPinsHiZ();
    for (pin_id = 0u; pin_id < LED_SCAN_PIN_COUNT; ++pin_id)
    {
        LedScan_PinToOutput(pin_id, Bit_RESET);
    }
}

static uint16_t LedScan_GetTimerPrescalerFor100kHz(void)
{
    uint32_t div = SystemCoreClock / 100000U;

    if (div == 0U)
    {
        div = 1U;
    }
    if (div > 0x10000U)
    {
        div = 0x10000U;
    }

    return (uint16_t)(div - 1U);
}

/* ── Public API ── */

void LedScan_Init(void)
{
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_AFIO |
                               RCC_APB2Periph_GPIOA |
                               RCC_APB2Periph_GPIOB,
                           ENABLE);
    LedScan_AllPinsHiZ();
}

void LedScan_OutputRoute(uint8_t route_id)
{
    const LedScanRoute *route;

    LedScan_AllPinsHiZ();
    if (route_id >= (uint8_t)LED_SCAN_ROUTE_COUNT)
    {
        return;
    }

    route = &s_ledscan_routes[route_id];
    LedScan_PinToOutput(route->high_pin, Bit_SET);
    LedScan_PinToOutput(route->low_pin, Bit_RESET);
}

void LedScan_OutputOff(void)
{
    LedScan_AllPinsHiZ();
}

void LedScan_StartTimer(void)
{
    TIM_TimeBaseInitTypeDef timer_init;
    NVIC_InitTypeDef nvic_init;

    LedScan_Init();

    if (s_scan_timer_initialized != 0u)
    {
        return;
    }

    RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM4, ENABLE);
    TIM_Cmd(TIM4, DISABLE);

    timer_init.TIM_Prescaler = LedScan_GetTimerPrescalerFor100kHz();
    timer_init.TIM_Period = PROJECT_CFG_LEDBAR_SCAN_TIMER_100KHZ_TICKS - 1U;
    timer_init.TIM_ClockDivision = TIM_CKD_DIV1;
    timer_init.TIM_CounterMode = TIM_CounterMode_Up;
    timer_init.TIM_RepetitionCounter = 0x00;
    TIM_TimeBaseInit(TIM4, &timer_init);
    TIM_ClearITPendingBit(TIM4, TIM_IT_Update);
    TIM_ITConfig(TIM4, TIM_IT_Update, ENABLE);

    nvic_init.NVIC_IRQChannel = TIM4_IRQn;
    nvic_init.NVIC_IRQChannelPreemptionPriority = 1;
    nvic_init.NVIC_IRQChannelSubPriority = 3;
    nvic_init.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&nvic_init);

    s_scan_timer_initialized = 1u;

    TIM_ITConfig(TIM4, TIM_IT_Update, ENABLE);
    NVIC_EnableIRQ(TIM4_IRQn);
    if (s_scan_timer_enabled == 0u)
    {
        TIM_SetCounter(TIM4, 0U);
        TIM_ClearITPendingBit(TIM4, TIM_IT_Update);
        TIM_Cmd(TIM4, ENABLE);
        s_scan_timer_enabled = 1u;
    }
}

void LedScan_StopTimer(void)
{
    if (s_scan_timer_initialized != 0u)
    {
        TIM_Cmd(TIM4, DISABLE);
        TIM_ITConfig(TIM4, TIM_IT_Update, DISABLE);
        TIM_ClearITPendingBit(TIM4, TIM_IT_Update);
        NVIC_DisableIRQ(TIM4_IRQn);
        NVIC_ClearPendingIRQ(TIM4_IRQn);
        RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM4, DISABLE);
        s_scan_timer_initialized = 0u;
    }
    s_scan_timer_enabled = 0u;
}

void LedScan_PrepareForStop(void)
{
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA |
                               RCC_APB2Periph_GPIOB,
                           ENABLE);
    LedScan_AllPinsOutputLow();
}