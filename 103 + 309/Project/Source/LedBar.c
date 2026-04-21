#include "main.h"

typedef struct
{
    GPIO_TypeDef *port;
    uint16_t pin;
} LedBarPinDef;

typedef struct
{
    uint8_t sel_595_bit;
    uint8_t low_pin;
    uint8_t high_pin;
} LedBarRoute;

typedef enum
{
    LEDBAR_ROUTE_HUNDREDS_1_UPPER = 0,
    LEDBAR_ROUTE_HUNDREDS_1_LOWER,

    LEDBAR_ROUTE_TENS_A,
    LEDBAR_ROUTE_TENS_B,
    LEDBAR_ROUTE_TENS_C,
    LEDBAR_ROUTE_TENS_D,
    LEDBAR_ROUTE_TENS_E,
    LEDBAR_ROUTE_TENS_F,
    LEDBAR_ROUTE_TENS_G,

    LEDBAR_ROUTE_ONES_A,
    LEDBAR_ROUTE_ONES_B,
    LEDBAR_ROUTE_ONES_C,
    LEDBAR_ROUTE_ONES_D,
    LEDBAR_ROUTE_ONES_E,
    LEDBAR_ROUTE_ONES_F,
    LEDBAR_ROUTE_ONES_G,

    LEDBAR_ROUTE_ICON_CHARGE,
    LEDBAR_ROUTE_ICON_PERCENT,

    LEDBAR_ROUTE_COUNT
} LedBarRouteId;

#define LEDBAR_DIGIT_BIT_A (1u << 0)
#define LEDBAR_DIGIT_BIT_B (1u << 1)
#define LEDBAR_DIGIT_BIT_C (1u << 2)
#define LEDBAR_DIGIT_BIT_D (1u << 3)
#define LEDBAR_DIGIT_BIT_E (1u << 4)
#define LEDBAR_DIGIT_BIT_F (1u << 5)
#define LEDBAR_DIGIT_BIT_G (1u << 6)

#define LEDBAR_595_SEL_HUNDREDS 0u
#define LEDBAR_595_SEL_TENS     1u
#define LEDBAR_595_SEL_ONES     2u
#define LEDBAR_595_SEL_CHARGE   3u
#define LEDBAR_595_SEL_PERCENT  4u

#define LEDBAR_GPIO_P1 GPIOC
#define LEDBAR_PIN_P1  GPIO_Pin_4
#define LEDBAR_GPIO_P2 GPIOC
#define LEDBAR_PIN_P2  GPIO_Pin_3
#define LEDBAR_GPIO_P3 GPIOC
#define LEDBAR_PIN_P3  GPIO_Pin_2
#define LEDBAR_GPIO_P4 GPIOC
#define LEDBAR_PIN_P4  GPIO_Pin_1
#define LEDBAR_GPIO_P5 GPIOC
#define LEDBAR_PIN_P5  GPIO_Pin_0

#define LEDBAR_595_GPIO_DATA  GPIO_LED595_DATA
#define LEDBAR_595_PIN_DATA   PIN_LED595_DATA
#define LEDBAR_595_GPIO_CLK   GPIO_LED595_CLK
#define LEDBAR_595_PIN_CLK    PIN_LED595_CLK
#define LEDBAR_595_GPIO_LATCH GPIO_LED595_LATCH
#define LEDBAR_595_PIN_LATCH  PIN_LED595_LATCH

#define LEDBAR_INVALID_SCAN_INDEX 0xFFu

LEDBAR_COMMAND LedBar_Command = LED_BAR_STARTUP;

static const LedBarPinDef s_ledbar_pins[LEDBAR_PIN_COUNT] =
{
    { LEDBAR_GPIO_P1, LEDBAR_PIN_P1 },
    { LEDBAR_GPIO_P2, LEDBAR_PIN_P2 },
    { LEDBAR_GPIO_P3, LEDBAR_PIN_P3 },
    { LEDBAR_GPIO_P4, LEDBAR_PIN_P4 },
    { LEDBAR_GPIO_P5, LEDBAR_PIN_P5 },
};

/*
 * Charlie 路由沿用 charlie_595_display_framework/LedBar.c 的实测结果：
 * - H1/H2：组成百位“1”
 * - H3：充电图标
 * - H4：百分号图标
 */
static const LedBarRoute s_ledbar_routes[LEDBAR_ROUTE_COUNT] =
{
    [LEDBAR_ROUTE_HUNDREDS_1_UPPER] = { LEDBAR_595_SEL_HUNDREDS, 3u, 2u },
    [LEDBAR_ROUTE_HUNDREDS_1_LOWER] = { LEDBAR_595_SEL_HUNDREDS, 3u, 1u },

    [LEDBAR_ROUTE_TENS_A] = { LEDBAR_595_SEL_TENS, 2u, 1u },
    [LEDBAR_ROUTE_TENS_B] = { LEDBAR_595_SEL_TENS, 1u, 2u },
    [LEDBAR_ROUTE_TENS_C] = { LEDBAR_595_SEL_TENS, 2u, 3u },
    [LEDBAR_ROUTE_TENS_D] = { LEDBAR_595_SEL_TENS, 1u, 3u },
    [LEDBAR_ROUTE_TENS_E] = { LEDBAR_595_SEL_TENS, 1u, 4u },
    [LEDBAR_ROUTE_TENS_F] = { LEDBAR_595_SEL_TENS, 2u, 4u },
    [LEDBAR_ROUTE_TENS_G] = { LEDBAR_595_SEL_TENS, 3u, 4u },

    [LEDBAR_ROUTE_ONES_A] = { LEDBAR_595_SEL_ONES, 1u, 0u },
    [LEDBAR_ROUTE_ONES_B] = { LEDBAR_595_SEL_ONES, 0u, 1u },
    [LEDBAR_ROUTE_ONES_C] = { LEDBAR_595_SEL_ONES, 2u, 0u },
    [LEDBAR_ROUTE_ONES_D] = { LEDBAR_595_SEL_ONES, 0u, 2u },
    [LEDBAR_ROUTE_ONES_E] = { LEDBAR_595_SEL_ONES, 3u, 0u },
    [LEDBAR_ROUTE_ONES_F] = { LEDBAR_595_SEL_ONES, 0u, 3u },
    [LEDBAR_ROUTE_ONES_G] = { LEDBAR_595_SEL_ONES, 0u, 4u },

    [LEDBAR_ROUTE_ICON_CHARGE] = { LEDBAR_595_SEL_CHARGE, 4u, 2u },
    [LEDBAR_ROUTE_ICON_PERCENT] = { LEDBAR_595_SEL_PERCENT, 4u, 1u },
};

static const uint8_t s_ledbar_digit_map[10] =
{
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

static volatile uint32_t s_ledbar_active_mask = 0u;
static volatile uint8_t s_ledbar_number = 0u;
static volatile uint8_t s_ledbar_indicator_mask = LEDBAR_ICON_PERCENT_MASK;
static uint8_t s_ledbar_initialized = 0u;
static uint8_t s_ledbar_scan_index = LEDBAR_INVALID_SCAN_INDEX;
static uint8_t s_ledbar_last_595_value = 0xFFu;

static uint8_t LedBar_GetPinIndex(uint16_t pin)
{
    uint8_t index = 0u;

    if (pin == 0u)
    {
        return 0u;
    }

    while (((pin >> index) & 0x1u) == 0u)
    {
        index++;
    }

    return index;
}

static void LedBar_SetGpioLevel(GPIO_TypeDef *port, uint16_t pin, uint8_t level)
{
    if (level != 0u)
    {
        port->BSRR = pin;
    }
    else
    {
        port->BRR = pin;
    }
}

static void LedBar_PinModeF1(GPIO_TypeDef *port, uint32_t pin_index, uint32_t mode_bits)
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

static void LedBar_PinToInput(uint8_t pin_id)
{
    GPIO_TypeDef *port = s_ledbar_pins[pin_id].port;
    uint32_t pin_index = (uint32_t)LedBar_GetPinIndex(s_ledbar_pins[pin_id].pin);

    LedBar_PinModeF1(port, pin_index, 0x4u);
}

static void LedBar_PinToOutput(uint8_t pin_id, BitAction level)
{
    GPIO_TypeDef *port = s_ledbar_pins[pin_id].port;
    uint16_t pin = s_ledbar_pins[pin_id].pin;
    uint32_t pin_index = (uint32_t)LedBar_GetPinIndex(pin);

    LedBar_SetGpioLevel(port, pin, (uint8_t)(level != Bit_RESET));
    LedBar_PinModeF1(port, pin_index, 0x2u);
}

static void LedBar_AllPinsHiZ(void)
{
    uint8_t pin_id;

    for (pin_id = 0u; pin_id < LEDBAR_PIN_COUNT; ++pin_id)
    {
        LedBar_PinToInput(pin_id);
    }
}

static void LedBar_595DelaySmall(void)
{
    __NOP();
    __NOP();
    __NOP();
    __NOP();
}

static void LedBar_595WriteByte(uint8_t data)
{
    uint8_t bit;

    for (bit = 0u; bit < 8u; ++bit)
    {
        LedBar_SetGpioLevel(LEDBAR_595_GPIO_DATA, LEDBAR_595_PIN_DATA, (uint8_t)((data & 0x80u) != 0u));
        LedBar_595DelaySmall();
        LedBar_SetGpioLevel(LEDBAR_595_GPIO_CLK, LEDBAR_595_PIN_CLK, 1u);
        LedBar_595DelaySmall();
        LedBar_SetGpioLevel(LEDBAR_595_GPIO_CLK, LEDBAR_595_PIN_CLK, 0u);
        data <<= 1;
    }

    LedBar_SetGpioLevel(LEDBAR_595_GPIO_LATCH, LEDBAR_595_PIN_LATCH, 1u);
    LedBar_595DelaySmall();
    LedBar_SetGpioLevel(LEDBAR_595_GPIO_LATCH, LEDBAR_595_PIN_LATCH, 0u);
}

static void LedBar_Select595(uint8_t bit_index)
{
    uint8_t value = 0u;

    if (bit_index < 8u)
    {
        value = (uint8_t)(1u << bit_index);
    }

    if (value != s_ledbar_last_595_value)
    {
        LedBar_595WriteByte(value);
        s_ledbar_last_595_value = value;
    }
}

static void LedBar_OutputOff(void)
{
    LedBar_AllPinsHiZ();
    LedBar_Select595(0xFFu);
}

static void LedBar_LightRoute(LedBarRouteId route_id)
{
    const LedBarRoute *route = &s_ledbar_routes[route_id];

    LedBar_AllPinsHiZ();
    LedBar_Select595(route->sel_595_bit);
    LedBar_PinToOutput(route->low_pin, Bit_RESET);
    LedBar_PinToOutput(route->high_pin, Bit_SET);
}

static void LedBar_RebuildActiveMask(void)
{
    uint8_t value = s_ledbar_number;
    uint8_t show_hundreds = (uint8_t)(value >= 100u);
    uint8_t show_tens = (uint8_t)((value >= 10u) || (show_hundreds != 0u));
    uint8_t tens = (uint8_t)((value / 10u) % 10u);
    uint8_t ones = (uint8_t)(value % 10u);
    uint8_t digit_mask;
    uint32_t active_mask = 0u;

    if (show_hundreds != 0u)
    {
        active_mask |= (1UL << LEDBAR_ROUTE_HUNDREDS_1_UPPER);
        active_mask |= (1UL << LEDBAR_ROUTE_HUNDREDS_1_LOWER);
    }

    if (show_tens != 0u)
    {
        digit_mask = s_ledbar_digit_map[tens];
        if ((digit_mask & LEDBAR_DIGIT_BIT_A) != 0u) { active_mask |= (1UL << LEDBAR_ROUTE_TENS_A); }
        if ((digit_mask & LEDBAR_DIGIT_BIT_B) != 0u) { active_mask |= (1UL << LEDBAR_ROUTE_TENS_B); }
        if ((digit_mask & LEDBAR_DIGIT_BIT_C) != 0u) { active_mask |= (1UL << LEDBAR_ROUTE_TENS_C); }
        if ((digit_mask & LEDBAR_DIGIT_BIT_D) != 0u) { active_mask |= (1UL << LEDBAR_ROUTE_TENS_D); }
        if ((digit_mask & LEDBAR_DIGIT_BIT_E) != 0u) { active_mask |= (1UL << LEDBAR_ROUTE_TENS_E); }
        if ((digit_mask & LEDBAR_DIGIT_BIT_F) != 0u) { active_mask |= (1UL << LEDBAR_ROUTE_TENS_F); }
        if ((digit_mask & LEDBAR_DIGIT_BIT_G) != 0u) { active_mask |= (1UL << LEDBAR_ROUTE_TENS_G); }
    }

    digit_mask = s_ledbar_digit_map[ones];
    if ((digit_mask & LEDBAR_DIGIT_BIT_A) != 0u) { active_mask |= (1UL << LEDBAR_ROUTE_ONES_A); }
    if ((digit_mask & LEDBAR_DIGIT_BIT_B) != 0u) { active_mask |= (1UL << LEDBAR_ROUTE_ONES_B); }
    if ((digit_mask & LEDBAR_DIGIT_BIT_C) != 0u) { active_mask |= (1UL << LEDBAR_ROUTE_ONES_C); }
    if ((digit_mask & LEDBAR_DIGIT_BIT_D) != 0u) { active_mask |= (1UL << LEDBAR_ROUTE_ONES_D); }
    if ((digit_mask & LEDBAR_DIGIT_BIT_E) != 0u) { active_mask |= (1UL << LEDBAR_ROUTE_ONES_E); }
    if ((digit_mask & LEDBAR_DIGIT_BIT_F) != 0u) { active_mask |= (1UL << LEDBAR_ROUTE_ONES_F); }
    if ((digit_mask & LEDBAR_DIGIT_BIT_G) != 0u) { active_mask |= (1UL << LEDBAR_ROUTE_ONES_G); }

    if ((s_ledbar_indicator_mask & LEDBAR_ICON_CHARGE_MASK) != 0u)
    {
        active_mask |= (1UL << LEDBAR_ROUTE_ICON_CHARGE);
    }
    if ((s_ledbar_indicator_mask & LEDBAR_ICON_PERCENT_MASK) != 0u)
    {
        active_mask |= (1UL << LEDBAR_ROUTE_ICON_PERCENT);
    }

    s_ledbar_active_mask = active_mask;
}

void LedBar_Init(void)
{
    GPIO_InitTypeDef gpio_init;

    gpio_init.GPIO_Pin = LEDBAR_PIN_P1 | LEDBAR_PIN_P2 | LEDBAR_PIN_P3 | LEDBAR_PIN_P4 | LEDBAR_PIN_P5;
    gpio_init.GPIO_Mode = GPIO_Mode_IN_FLOATING;
    gpio_init.GPIO_Speed = GPIO_Speed_2MHz;
    GPIO_Init(GPIOC, &gpio_init);

    s_ledbar_number = 0u;
    s_ledbar_indicator_mask = LEDBAR_ICON_PERCENT_MASK;
    s_ledbar_scan_index = LEDBAR_INVALID_SCAN_INDEX;
    s_ledbar_last_595_value = 0xFFu;
    s_ledbar_initialized = 1u;
    LedBar_Command = LED_BAR_NORMAL;
    LedBar_RebuildActiveMask();
    LedBar_OutputOff();
}

void LedBar_Clear(void)
{
    s_ledbar_number = 0u;
    s_ledbar_indicator_mask = 0u;
    LedBar_RebuildActiveMask();
}

void LedBar_SetNumber(uint8_t value)
{
    if (value > 100u)
    {
        value = 100u;
    }

    s_ledbar_number = value;
    LedBar_RebuildActiveMask();
}

void LedBar_SetIndicators(uint8_t indicator_mask)
{
    s_ledbar_indicator_mask = (uint8_t)(indicator_mask & (LEDBAR_ICON_CHARGE_MASK | LEDBAR_ICON_PERCENT_MASK));
    LedBar_RebuildActiveMask();
}

void LedBar_SetIndicatorState(uint8_t indicator_mask, uint8_t enable)
{
    indicator_mask = (uint8_t)(indicator_mask & (LEDBAR_ICON_CHARGE_MASK | LEDBAR_ICON_PERCENT_MASK));

    if (enable != 0u)
    {
        s_ledbar_indicator_mask |= indicator_mask;
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

    if (s_ledbar_initialized == 0u)
    {
        return;
    }

    if (s_ledbar_active_mask == 0u)
    {
        s_ledbar_scan_index = LEDBAR_INVALID_SCAN_INDEX;
        LedBar_OutputOff();
        return;
    }

    next_index = s_ledbar_scan_index;
    for (count = 0u; count < (uint8_t)LEDBAR_ROUTE_COUNT; ++count)
    {
        next_index = (uint8_t)((next_index + 1u) % (uint8_t)LEDBAR_ROUTE_COUNT);
        if ((s_ledbar_active_mask & (1UL << next_index)) != 0u)
        {
            s_ledbar_scan_index = next_index;
            LedBar_LightRoute((LedBarRouteId)next_index);
            return;
        }
    }

    s_ledbar_scan_index = LEDBAR_INVALID_SCAN_INDEX;
    LedBar_OutputOff();
}

void APP_LedBar(void)
{
    uint8_t display_value;
    uint8_t indicator_mask = LEDBAR_ICON_PERCENT_MASK;

    if (s_ledbar_initialized == 0u)
    {
        LedBar_Init();
    }

    if (SystemStatus.bits.b1StartUpBMS != 0u)
    {
        LedBar_Command = LED_BAR_STARTUP;
        LedBar_OutputOff();
        return;
    }

    if (g_st_SysTimeFlag.bits.b1Sys1msFlag != 0u)
    {
        LedBar_Scan1ms();
    }

    if (g_st_SysTimeFlag.bits.b1Sys100msFlag == 0u)
    {
        return;
    }

    display_value = (uint8_t)g_stCellInfoReport.SocElement.u16Soc;
    if (display_value > 100u)
    {
        display_value = 100u;
    }

    if (g_stCellInfoReport.u16Ichg != 0u)
    {
        indicator_mask |= LEDBAR_ICON_CHARGE_MASK;
        LedBar_Command = LED_BAR_CHG;
    }
    else if (g_stCellInfoReport.u16IDischg != 0u)
    {
        LedBar_Command = LED_BAR_DSG;
    }
    else
    {
        LedBar_Command = LED_BAR_NORMAL;
    }

    if (((g_stCellInfoReport.unMdlFault_Third.all & 0x3FFBu) != 0u) ||
        (System_ERROR_UserCallback(ERROR_STATUS_TEMP_BREAK) != 0u) ||
        (System_ERROR_UserCallback(ERROR_STATUS_CBC_DSG) != 0u))
    {
        LedBar_Command = LED_BAR_FAULT;
    }

    LedBar_SetNumber(display_value);
    LedBar_SetIndicators(indicator_mask);
}
