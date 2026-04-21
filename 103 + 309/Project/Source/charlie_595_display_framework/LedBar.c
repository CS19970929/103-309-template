#include "main.h"

#define __STM32F1__

typedef struct
{
    GPIO_TypeDef *port;
    uint16_t pin;
} LedBarPinDef;

typedef struct
{
    uint8_t low_pin;
    uint8_t high_pin;
} LedBarSegmentRoute;

#define LEDBAR_DIGIT_BIT_A (1U << 0)
#define LEDBAR_DIGIT_BIT_B (1U << 1)
#define LEDBAR_DIGIT_BIT_C (1U << 2)
#define LEDBAR_DIGIT_BIT_D (1U << 3)
#define LEDBAR_DIGIT_BIT_E (1U << 4)
#define LEDBAR_DIGIT_BIT_F (1U << 5)
#define LEDBAR_DIGIT_BIT_G (1U << 6)
#define LEDBAR_ALL_SEGMENTS_OFF 0U
#define LEDBAR_INVALID_SCAN_INDEX 0xFFU

LEDBAR_COMMAND LedBar_Command = LED_BAR_STARTUP;

static const LedBarPinDef s_ledbar_pins[LEDBAR_PIN_COUNT] = {
    {LEDBAR_GPIO_P1, LEDBAR_PIN_P1},
    {LEDBAR_GPIO_P2, LEDBAR_PIN_P2},
    {LEDBAR_GPIO_P3, LEDBAR_PIN_P3},
    {LEDBAR_GPIO_P4, LEDBAR_PIN_P4},
    {LEDBAR_GPIO_P5, LEDBAR_PIN_P5},
};

static const LedBarSegmentRoute s_ledbar_routes[LEDBAR_SEG_COUNT] = {
    [LEDBAR_SEG_1A] = {2U, 1U},
    [LEDBAR_SEG_1B] = {1U, 2U},
    [LEDBAR_SEG_1C] = {2U, 3U},
    [LEDBAR_SEG_1D] = {1U, 3U},
    [LEDBAR_SEG_1E] = {1U, 4U},
    [LEDBAR_SEG_1F] = {2U, 4U},
    [LEDBAR_SEG_1G] = {3U, 4U},
    [LEDBAR_SEG_2A] = {1U, 0U},
    [LEDBAR_SEG_2B] = {0U, 1U},
    [LEDBAR_SEG_2C] = {2U, 0U},
    [LEDBAR_SEG_2D] = {0U, 2U},
    [LEDBAR_SEG_2E] = {3U, 0U},
    [LEDBAR_SEG_2F] = {0U, 3U},
    [LEDBAR_SEG_2G] = {0U, 4U},
    [LEDBAR_SEG_H1] = {3U, 2U},
    [LEDBAR_SEG_H2] = {3U, 1U},
    [LEDBAR_SEG_H3] = {4U, 2U},
    [LEDBAR_SEG_H4] = {4U, 1U},
};

static const uint8_t s_digit_segments[10] = {
    LEDBAR_DIGIT_BIT_A | LEDBAR_DIGIT_BIT_B | LEDBAR_DIGIT_BIT_C | LEDBAR_DIGIT_BIT_D | LEDBAR_DIGIT_BIT_E | LEDBAR_DIGIT_BIT_F,
    LEDBAR_DIGIT_BIT_B | LEDBAR_DIGIT_BIT_C,
    LEDBAR_DIGIT_BIT_A | LEDBAR_DIGIT_BIT_B | LEDBAR_DIGIT_BIT_D | LEDBAR_DIGIT_BIT_E | LEDBAR_DIGIT_BIT_G,
    LEDBAR_DIGIT_BIT_A | LEDBAR_DIGIT_BIT_B | LEDBAR_DIGIT_BIT_C | LEDBAR_DIGIT_BIT_D | LEDBAR_DIGIT_BIT_G,
    LEDBAR_DIGIT_BIT_B | LEDBAR_DIGIT_BIT_C | LEDBAR_DIGIT_BIT_F | LEDBAR_DIGIT_BIT_G,
    LEDBAR_DIGIT_BIT_A | LEDBAR_DIGIT_BIT_C | LEDBAR_DIGIT_BIT_D | LEDBAR_DIGIT_BIT_F | LEDBAR_DIGIT_BIT_G,
    LEDBAR_DIGIT_BIT_A | LEDBAR_DIGIT_BIT_C | LEDBAR_DIGIT_BIT_D | LEDBAR_DIGIT_BIT_E | LEDBAR_DIGIT_BIT_F | LEDBAR_DIGIT_BIT_G,
    LEDBAR_DIGIT_BIT_A | LEDBAR_DIGIT_BIT_B | LEDBAR_DIGIT_BIT_C,
    LEDBAR_DIGIT_BIT_A | LEDBAR_DIGIT_BIT_B | LEDBAR_DIGIT_BIT_C | LEDBAR_DIGIT_BIT_D | LEDBAR_DIGIT_BIT_E | LEDBAR_DIGIT_BIT_F | LEDBAR_DIGIT_BIT_G,
    LEDBAR_DIGIT_BIT_A | LEDBAR_DIGIT_BIT_B | LEDBAR_DIGIT_BIT_C | LEDBAR_DIGIT_BIT_D | LEDBAR_DIGIT_BIT_F | LEDBAR_DIGIT_BIT_G,
};

static volatile uint32_t s_ledbar_active_mask = 0U;
static volatile uint8_t s_ledbar_number = 0U;
static volatile uint8_t s_ledbar_indicator_mask = 0U;
static uint8_t s_ledbar_initialized = 0U;
static uint8_t s_ledbar_scan_index = LEDBAR_INVALID_SCAN_INDEX;

static uint8_t LedBar_GetPinIndex(uint16_t pin)
{
    uint8_t index = 0U;

    while (((pin >> index) & 0x1U) == 0U)
    {
        index++;
    }

    return index;
}

#if defined(__STM32F1__)
static void LedBar_PinModeF1(GPIO_TypeDef *port, uint32_t pin_index, uint32_t mode_bits)
{
    volatile uint32_t *config_reg;
    uint32_t shift;

    if (pin_index < 8U)
    {
        config_reg = &port->CRL;
        shift = pin_index * 4U;
    }
    else
    {
        config_reg = &port->CRH;
        shift = (pin_index - 8U) * 4U;
    }

    *config_reg &= ~(0xFU << shift);
    *config_reg |= (mode_bits << shift);
}
#endif

static void LedBar_PinToInput(uint8_t pin_id)
{
    GPIO_TypeDef *port = s_ledbar_pins[pin_id].port;
    uint32_t pin_index = (uint32_t)LedBar_GetPinIndex(s_ledbar_pins[pin_id].pin);

#if defined(__STM32F1__)
    LedBar_PinModeF1(port, pin_index, 0x4U);
#else
    {
        uint32_t shift = pin_index * 2U;
    port->MODER &= ~(0x3U << shift);
    port->PUPDR &= ~(0x3U << shift);
    }
#endif
}

static void LedBar_PinToOutput(uint8_t pin_id, BitAction level)
{
    GPIO_TypeDef *port = s_ledbar_pins[pin_id].port;
    uint16_t pin = s_ledbar_pins[pin_id].pin;
    uint32_t pin_index = (uint32_t)LedBar_GetPinIndex(pin);

    if (level != Bit_RESET)
    {
        port->BSRR = pin;
    }
    else
    {
        port->BRR = pin;
    }

#if defined(__STM32F1__)
    LedBar_PinModeF1(port, pin_index, 0x2U);
#else
    {
        uint32_t shift = pin_index * 2U;
    port->OTYPER &= ~((uint32_t)1U << pin_index);
    port->OSPEEDR &= ~(0x3U << shift);
    port->OSPEEDR |= (0x1U << shift);
    port->PUPDR &= ~(0x3U << shift);
    port->MODER &= ~(0x3U << shift);
    port->MODER |= (0x1U << shift);
    }
#endif
}

static void LedBar_AllPinsHiZ(void)
{
    uint8_t pin_id;

    for (pin_id = 0U; pin_id < LEDBAR_PIN_COUNT; ++pin_id)
    {
        LedBar_PinToInput(pin_id);
    }
}

static void LedBar_RebuildActiveMask(void)
{
    uint8_t tens;
    uint8_t ones;
    uint8_t show_hundreds;
    uint8_t show_tens;
    uint8_t digit_mask;
    uint32_t active_mask = 0U;

    tens = (uint8_t)(s_ledbar_number / 10U);
    ones = (uint8_t)(s_ledbar_number % 10U);
    show_hundreds = (uint8_t)(s_ledbar_number >= 100U);
    show_tens = (uint8_t)((s_ledbar_number >= 10U) || (show_hundreds != 0U));

    if (show_hundreds != 0U)
    {
        active_mask |= (1UL << LEDBAR_SEG_H1);
        active_mask |= (1UL << LEDBAR_SEG_H2);
    }

    if (show_tens != 0U)
    {
        digit_mask = s_digit_segments[tens];
        if ((digit_mask & LEDBAR_DIGIT_BIT_A) != 0U) { active_mask |= (1UL << LEDBAR_SEG_1A); }
        if ((digit_mask & LEDBAR_DIGIT_BIT_B) != 0U) { active_mask |= (1UL << LEDBAR_SEG_1B); }
        if ((digit_mask & LEDBAR_DIGIT_BIT_C) != 0U) { active_mask |= (1UL << LEDBAR_SEG_1C); }
        if ((digit_mask & LEDBAR_DIGIT_BIT_D) != 0U) { active_mask |= (1UL << LEDBAR_SEG_1D); }
        if ((digit_mask & LEDBAR_DIGIT_BIT_E) != 0U) { active_mask |= (1UL << LEDBAR_SEG_1E); }
        if ((digit_mask & LEDBAR_DIGIT_BIT_F) != 0U) { active_mask |= (1UL << LEDBAR_SEG_1F); }
        if ((digit_mask & LEDBAR_DIGIT_BIT_G) != 0U) { active_mask |= (1UL << LEDBAR_SEG_1G); }
    }

    digit_mask = s_digit_segments[ones];
    if ((digit_mask & LEDBAR_DIGIT_BIT_A) != 0U) { active_mask |= (1UL << LEDBAR_SEG_2A); }
    if ((digit_mask & LEDBAR_DIGIT_BIT_B) != 0U) { active_mask |= (1UL << LEDBAR_SEG_2B); }
    if ((digit_mask & LEDBAR_DIGIT_BIT_C) != 0U) { active_mask |= (1UL << LEDBAR_SEG_2C); }
    if ((digit_mask & LEDBAR_DIGIT_BIT_D) != 0U) { active_mask |= (1UL << LEDBAR_SEG_2D); }
    if ((digit_mask & LEDBAR_DIGIT_BIT_E) != 0U) { active_mask |= (1UL << LEDBAR_SEG_2E); }
    if ((digit_mask & LEDBAR_DIGIT_BIT_F) != 0U) { active_mask |= (1UL << LEDBAR_SEG_2F); }
    if ((digit_mask & LEDBAR_DIGIT_BIT_G) != 0U) { active_mask |= (1UL << LEDBAR_SEG_2G); }

    if ((s_ledbar_indicator_mask & LEDBAR_H1_MASK) != 0U) { active_mask |= (1UL << LEDBAR_SEG_H1); }
    if ((s_ledbar_indicator_mask & LEDBAR_H2_MASK) != 0U) { active_mask |= (1UL << LEDBAR_SEG_H2); }
    if ((s_ledbar_indicator_mask & LEDBAR_H3_MASK) != 0U) { active_mask |= (1UL << LEDBAR_SEG_H3); }
    if ((s_ledbar_indicator_mask & LEDBAR_H4_MASK) != 0U) { active_mask |= (1UL << LEDBAR_SEG_H4); }

    s_ledbar_active_mask = active_mask;
}

void LedBar_Init(void)
{
    GPIO_InitTypeDef gpio_init;

#if defined(__STM32F1__)
    gpio_init.GPIO_Pin = LEDBAR_PIN_P1 | LEDBAR_PIN_P2 | LEDBAR_PIN_P3 | LEDBAR_PIN_P4;
    gpio_init.GPIO_Mode = GPIO_Mode_IN_FLOATING;
    gpio_init.GPIO_Speed = GPIO_Speed_2MHz;
    GPIO_Init(LEDBAR_GPIO_P1, &gpio_init);

    gpio_init.GPIO_Pin = LEDBAR_PIN_P5;
    gpio_init.GPIO_Mode = GPIO_Mode_IN_FLOATING;
    gpio_init.GPIO_Speed = GPIO_Speed_2MHz;
    GPIO_Init(LEDBAR_GPIO_P5, &gpio_init);
#else
    gpio_init.GPIO_Pin = LEDBAR_PIN_P1 | LEDBAR_PIN_P2 | LEDBAR_PIN_P3 | LEDBAR_PIN_P4;
    gpio_init.GPIO_Mode = GPIO_Mode_IN;
    gpio_init.GPIO_PuPd = GPIO_PuPd_NOPULL;
    GPIO_Init(LEDBAR_GPIO_P1, &gpio_init);

    gpio_init.GPIO_Pin = LEDBAR_PIN_P5;
    gpio_init.GPIO_Mode = GPIO_Mode_IN;
    gpio_init.GPIO_PuPd = GPIO_PuPd_NOPULL;
    GPIO_Init(LEDBAR_GPIO_P5, &gpio_init);
#endif

    s_ledbar_number = 0U;
    s_ledbar_indicator_mask = 0U;
    s_ledbar_scan_index = LEDBAR_INVALID_SCAN_INDEX;
    s_ledbar_initialized = 1U;
    LedBar_Command = LED_BAR_NORMAL;
    LedBar_RebuildActiveMask();
}

void LedBar_Clear(void)
{
    s_ledbar_number = 0U;
    s_ledbar_indicator_mask = 0U;
    LedBar_RebuildActiveMask();
}

void LedBar_SetNumber(UINT8 value)
{
    if (value > 100U)
    {
        value = 100U;
    }

    s_ledbar_number = value;
    LedBar_RebuildActiveMask();
}

void LedBar_SetIndicators(UINT8 indicator_mask)
{
    s_ledbar_indicator_mask = (uint8_t)(indicator_mask & (LEDBAR_H1_MASK | LEDBAR_H2_MASK | LEDBAR_H3_MASK | LEDBAR_H4_MASK));
    LedBar_RebuildActiveMask();
}

void LedBar_SetIndicatorState(UINT8 indicator_mask, UINT8 enable)
{
    if (enable != 0U)
    {
        s_ledbar_indicator_mask |= (uint8_t)(indicator_mask & (LEDBAR_H1_MASK | LEDBAR_H2_MASK | LEDBAR_H3_MASK | LEDBAR_H4_MASK));
    }
    else
    {
        s_ledbar_indicator_mask &= (uint8_t)(~indicator_mask);
    }

    LedBar_RebuildActiveMask();
}

void LedBar_Scan1ms(void)
{
    uint8_t next_index;
    uint8_t count;
    LedBarSegmentRoute route;

    if (s_ledbar_initialized == 0U)
    {
        return;
    }

    LedBar_AllPinsHiZ();

    if (s_ledbar_active_mask == 0U)
    {
        s_ledbar_scan_index = LEDBAR_INVALID_SCAN_INDEX;
        return;
    }

    next_index = s_ledbar_scan_index;
    for (count = 0U; count < LEDBAR_SEG_COUNT; ++count)
    {
        next_index = (uint8_t)((next_index + 1U) % LEDBAR_SEG_COUNT);
        if ((s_ledbar_active_mask & (1UL << next_index)) != 0U)
        {
            s_ledbar_scan_index = next_index;
            route = s_ledbar_routes[next_index];
            LedBar_PinToOutput(route.low_pin, Bit_RESET);
            LedBar_PinToOutput(route.high_pin, Bit_SET);
            return;
        }
    }

    s_ledbar_scan_index = LEDBAR_INVALID_SCAN_INDEX;
}

void APP_LedBar(void)
{
    UINT8 display_value;
    UINT8 indicator_mask = 0U;

    if (SystemStatus.bits.b1StartUpBMS != 0U)
    {
        return;
    }

    if (s_ledbar_initialized == 0U)
    {
        LedBar_Init();
    }

    display_value = (UINT8)g_stCellInfoReport.SocElement.u16Soc;
    if (display_value > 100U)
    {
        display_value = 100U;
    }

    if (g_stCellInfoReport.u16Ichg != 0U)
    {
        indicator_mask |= LEDBAR_H1_MASK;
    }
    if (g_stCellInfoReport.u16IDischg != 0U)
    {
        indicator_mask |= LEDBAR_H2_MASK;
    }
    // if ((g_stCellInfoReport.unMdlFault_Third.all & 0x3FFBU) != 0U ||
    //     System_ERROR_UserCallback(ERROR_STATUS_TEMP_BREAK) ||
    //     System_ERROR_UserCallback(ERROR_STATUS_CBC_DSG))
    // {
    //     indicator_mask |= LEDBAR_H3_MASK;
    // }
    // if (BlueToothFlag != 0U)
    // {
    //     indicator_mask |= LEDBAR_H4_MASK;
    // }
    indicator_mask |= LEDBAR_H1_MASK;
    indicator_mask |= LEDBAR_H2_MASK;
    indicator_mask |= LEDBAR_H3_MASK;
    indicator_mask |= LEDBAR_H4_MASK;

    LedBar_SetNumber(display_value);
    LedBar_SetIndicators(indicator_mask);
}
