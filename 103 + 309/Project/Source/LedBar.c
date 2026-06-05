#include "main.h"
#include "DebugWatch.h"
#include "IrqDebug.h"
#include <string.h>

#define LEDBAR_FRAME_ROUTE_COUNT ((uint8_t)LEDBAR_ROUTE_COUNT)
#define LEDBAR_SCAN_TIMER_100KHZ_TICKS 50u
#define LEDBAR_KEY_LONG_PRESS_10MS 50u

#define LEDBAR_GPIOA_CRL_MASK ((0xFUL << 16u) | (0xFUL << 20u) | (0xFUL << 24u))
#define LEDBAR_GPIOB_CRH_MASK ((0xFUL << 8u) | (0xFUL << 12u))
#define LEDBAR_TRANSITION_OFF_GHOST_COST 8u
#define LEDBAR_TRANSITION_ON_GHOST_COST 1u
#define LEDBAR_TRANSITION_NO_SHARED_PIN_COST 2u
#define LEDBAR_TRANSITION_MAX_COST 0xFFFFu
#define LEDBAR_ORDER_IMPROVE_MAX_PASSES LEDBAR_FRAME_ROUTE_COUNT

#define LEDBAR_SLEEP_SOC_MAGIC 0x5A00u
#define LEDBAR_SLEEP_SOC_MAGIC_MASK 0xFF00u
#define LEDBAR_SLEEP_SOC_VALUE_MASK 0x00FFu
#define LEDBAR_SLEEP_SOC_REG BKP_DR4
#define LEDBAR_SLEEP_SOC_INV_REG BKP_DR5

#define LEDBAR_DIGIT_BIT_A (1u << 0)
#define LEDBAR_DIGIT_BIT_B (1u << 1)
#define LEDBAR_DIGIT_BIT_C (1u << 2)
#define LEDBAR_DIGIT_BIT_D (1u << 3)
#define LEDBAR_DIGIT_BIT_E (1u << 4)
#define LEDBAR_DIGIT_BIT_F (1u << 5)
#define LEDBAR_DIGIT_BIT_G (1u << 6)

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

typedef struct
{
    uint8_t low_pin;
    uint8_t high_pin;
} LedBarRoute;

typedef struct
{
    GPIO_TypeDef *port;
    uint16_t pin;
} LedBarPinDef;

typedef struct
{
    uint8_t routes[LEDBAR_FRAME_ROUTE_COUNT];
    uint8_t length;
} LedBarFrame;

typedef struct LEDBAR_RUNTIME_TAG
{
    uint8_t initialized;
    uint8_t sleep;
    uint8_t blank;
    uint8_t number;
    uint8_t indicator_mask;
    LedBarFrame frame;
    uint8_t scan_index;
    uint8_t scan_timer_initialized;
    uint8_t scan_timer_enabled;
    uint16_t soc_display_10ms;
    uint8_t startup_display_armed;
    uint32_t key_hold_10ms;
    uint32_t key_press_start_10ms;
    uint8_t key_long_handled;
    uint8_t key_wakeup_armed;
    uint8_t key_active;
    uint8_t mcu_wk_active;
} LedBarRuntime;

static const LedBarRoute s_ledbar_routes[LEDBAR_ROUTE_COUNT] =
{
    {3u, 2u},
    {3u, 1u},
    {2u, 1u},
    {1u, 2u},
    {2u, 3u},
    {1u, 3u},
    {1u, 4u},
    {2u, 4u},
    {3u, 4u},
    {1u, 0u},
    {0u, 1u},
    {2u, 0u},
    {0u, 2u},
    {3u, 0u},
    {0u, 3u},
    {0u, 4u},
    {4u, 2u},
    {4u, 1u},
};

static const LedBarPinDef s_ledbar_pins[LEDBAR_PIN_COUNT] =
{
    {LEDBAR_GPIO_P1, LEDBAR_PIN_P1},
    {LEDBAR_GPIO_P3, LEDBAR_PIN_P3},
    {LEDBAR_GPIO_P2, LEDBAR_PIN_P2},
    {LEDBAR_GPIO_P4, LEDBAR_PIN_P4},
    {LEDBAR_GPIO_P5, LEDBAR_PIN_P5},
};

static const uint8_t s_ledbar_digit_map[10] =
{
    LEDBAR_DIGIT_BIT_A | LEDBAR_DIGIT_BIT_B | LEDBAR_DIGIT_BIT_C |
        LEDBAR_DIGIT_BIT_D | LEDBAR_DIGIT_BIT_E | LEDBAR_DIGIT_BIT_F,
    LEDBAR_DIGIT_BIT_B | LEDBAR_DIGIT_BIT_C,
    LEDBAR_DIGIT_BIT_A | LEDBAR_DIGIT_BIT_B | LEDBAR_DIGIT_BIT_D |
        LEDBAR_DIGIT_BIT_E | LEDBAR_DIGIT_BIT_G,
    LEDBAR_DIGIT_BIT_A | LEDBAR_DIGIT_BIT_B | LEDBAR_DIGIT_BIT_C |
        LEDBAR_DIGIT_BIT_D | LEDBAR_DIGIT_BIT_G,
    LEDBAR_DIGIT_BIT_B | LEDBAR_DIGIT_BIT_C | LEDBAR_DIGIT_BIT_F |
        LEDBAR_DIGIT_BIT_G,
    LEDBAR_DIGIT_BIT_A | LEDBAR_DIGIT_BIT_C | LEDBAR_DIGIT_BIT_D |
        LEDBAR_DIGIT_BIT_F | LEDBAR_DIGIT_BIT_G,
    LEDBAR_DIGIT_BIT_A | LEDBAR_DIGIT_BIT_C | LEDBAR_DIGIT_BIT_D |
        LEDBAR_DIGIT_BIT_E | LEDBAR_DIGIT_BIT_F | LEDBAR_DIGIT_BIT_G,
    LEDBAR_DIGIT_BIT_A | LEDBAR_DIGIT_BIT_B | LEDBAR_DIGIT_BIT_C,
    LEDBAR_DIGIT_BIT_A | LEDBAR_DIGIT_BIT_B | LEDBAR_DIGIT_BIT_C |
        LEDBAR_DIGIT_BIT_D | LEDBAR_DIGIT_BIT_E | LEDBAR_DIGIT_BIT_F |
        LEDBAR_DIGIT_BIT_G,
    LEDBAR_DIGIT_BIT_A | LEDBAR_DIGIT_BIT_B | LEDBAR_DIGIT_BIT_C |
        LEDBAR_DIGIT_BIT_D | LEDBAR_DIGIT_BIT_F | LEDBAR_DIGIT_BIT_G,
};

static LedBarRuntime s_ledbar =
{
    0u,
    0u,
    1u,
    0u,
    LEDBAR_ICON_PERCENT_MASK,
    {{0u}, 0u},
    0u,
    0u,
    0u,
    0u,
    0u,
    0u,
    0u,
    0u,
    0u,
    0u,
    0u,
};

#if DEBUG_WATCH_ENABLED
void LedBar_DebugWatchBind(DEBUG_WATCH_ROOT *watch)
{
    watch->runtime.ledbar = &s_ledbar;
    watch->tables.ledbar_digit_map = s_ledbar_digit_map;
    watch->tables.ledbar_digit_map_count =
        (uint16_t)(sizeof(s_ledbar_digit_map) / sizeof(s_ledbar_digit_map[0]));
    watch->tables.ledbar_routes = s_ledbar_routes;
    watch->tables.ledbar_routes_count =
        (uint16_t)(sizeof(s_ledbar_routes) / sizeof(s_ledbar_routes[0]));
    watch->tables.ledbar_pins = s_ledbar_pins;
    watch->tables.ledbar_pins_count =
        (uint16_t)(sizeof(s_ledbar_pins) / sizeof(s_ledbar_pins[0]));
}
#endif

static void LedBar_StopScanTimer(void);
static void LedBar_RefreshOutput(void);
#ifdef _DI_SWITCH_longKEY_ONOFF
extern void low_power_log_and_commit_sleep(uint8_t sleep_mode);
#endif

static void LedBar_EnsureInit(void)
{
    if (s_ledbar.initialized == 0u)
    {
        LedBar_Init();
    }
}

static void LedBar_EnableBackupAccess(void)
{
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_PWR | RCC_APB1Periph_BKP, ENABLE);
    PWR_BackupAccessCmd(ENABLE);
}

static uint8_t LedBar_LimitSoc(uint16_t soc)
{
    if (soc > 100u)
    {
        soc = 100u;
    }
    return (uint8_t)soc;
}

static uint8_t LedBar_GetRuntimeSoc(void)
{
    return LedBar_LimitSoc(g_stCellInfoReport.SocElement.u16Soc);
}

static uint8_t LedBar_ReadMcuWakeRaw(void)
{
    return (uint8_t)(GPIO_ReadInputDataBit(GPIO_MCU_WK, PIN_MCU_WK) != Bit_RESET);
}

static uint8_t LedBar_IsDischargeMosOpen(void)
{
    return (uint8_t)(SH367309_Reg_Store.REG_BSTATUS3.bits.DSG_FET != 0u);
}

static uint8_t LedBar_ReadSwitchRaw(void)
{
    return (uint8_t)(GPIO_ReadInputDataBit(GPIO_SW, PIN_SW) == Bit_RESET);
}

static uint8_t LedBar_GetPinIndex(uint16_t pin)
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

static void LedBar_PinWrite(uint8_t pin_id, BitAction level)
{
    GPIO_TypeDef *port = s_ledbar_pins[pin_id].port;
    uint16_t pin = s_ledbar_pins[pin_id].pin;

    if (level != Bit_RESET)
    {
        port->BSRR = pin;
    }
    else
    {
        port->BRR = pin;
    }
}

static void LedBar_PinToOutput(uint8_t pin_id, BitAction level)
{
    GPIO_TypeDef *port = s_ledbar_pins[pin_id].port;
    uint16_t pin = s_ledbar_pins[pin_id].pin;
    uint32_t pin_index = (uint32_t)LedBar_GetPinIndex(pin);

    LedBar_PinWrite(pin_id, level);
    LedBar_PinModeF1(port, pin_index, 0x2u);
}

static void LedBar_PinToOutputMode(uint8_t pin_id)
{
    GPIO_TypeDef *port = s_ledbar_pins[pin_id].port;
    uint32_t pin_index = (uint32_t)LedBar_GetPinIndex(s_ledbar_pins[pin_id].pin);

    LedBar_PinModeF1(port, pin_index, 0x2u);
}

static void LedBar_AllPinsHiZ(void)
{
    GPIOA->CRL &= (uint32_t)(~LEDBAR_GPIOA_CRL_MASK);
    GPIOB->CRH &= (uint32_t)(~LEDBAR_GPIOB_CRH_MASK);
}

static void LedBar_AllPinsOutputLow(void)
{
    uint8_t pin_id;

    LedBar_AllPinsHiZ();
    for (pin_id = 0u; pin_id < LEDBAR_PIN_COUNT; ++pin_id)
    {
        LedBar_PinToOutput(pin_id, Bit_RESET);
    }
}

static void LedBar_GpioInitForDisplay(void)
{
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_AFIO |
                               RCC_APB2Periph_GPIOA |
                               RCC_APB2Periph_GPIOB,
                           ENABLE);
    LedBar_AllPinsHiZ();
}

static void LedBar_GpioPrepareForStop(void)
{
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA |
                               RCC_APB2Periph_GPIOB,
                           ENABLE);
    LedBar_AllPinsOutputLow();
}

static void LedBar_OutputRoute(uint8_t route_id)
{
    const LedBarRoute *route;

    LedBar_AllPinsHiZ();
    if (route_id >= (uint8_t)LEDBAR_ROUTE_COUNT)
    {
        return;
    }

    route = &s_ledbar_routes[route_id];
    LedBar_PinWrite(route->low_pin, Bit_RESET);
    LedBar_PinWrite(route->high_pin, Bit_SET);
    LedBar_PinToOutputMode(route->low_pin);
    LedBar_PinToOutputMode(route->high_pin);
}

static void LedBar_OutputOff(void)
{
    LedBar_AllPinsHiZ();
}

static UINT16 LedBar_GetTimerPrescalerFor100kHz(void)
{
    UINT32 div = SystemCoreClock / 100000U;

    if (div == 0U)
    {
        div = 1U;
    }
    if (div > 0x10000U)
    {
        div = 0x10000U;
    }

    return (UINT16)(div - 1U);
}

static void LedBar_ScanTimerInit(void)
{
    TIM_TimeBaseInitTypeDef timer_init;
    NVIC_InitTypeDef nvic_init;

    if (s_ledbar.scan_timer_initialized != 0u)
    {
        return;
    }

    RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM4, ENABLE);
    TIM_Cmd(TIM4, DISABLE);

    timer_init.TIM_Prescaler = LedBar_GetTimerPrescalerFor100kHz();
    timer_init.TIM_Period = LEDBAR_SCAN_TIMER_100KHZ_TICKS - 1U;
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

    s_ledbar.scan_timer_initialized = 1u;
}

static void LedBar_StartScanTimer(void)
{
    LedBar_GpioInitForDisplay();
    LedBar_ScanTimerInit();

    TIM_ITConfig(TIM4, TIM_IT_Update, ENABLE);
    NVIC_EnableIRQ(TIM4_IRQn);
    if (s_ledbar.scan_timer_enabled == 0u)
    {
        TIM_SetCounter(TIM4, 0U);
        TIM_ClearITPendingBit(TIM4, TIM_IT_Update);
        TIM_Cmd(TIM4, ENABLE);
        s_ledbar.scan_timer_enabled = 1u;
    }
}

static void LedBar_StopScanTimer(void)
{
    if (s_ledbar.scan_timer_initialized != 0u)
    {
        TIM_Cmd(TIM4, DISABLE);
        TIM_ITConfig(TIM4, TIM_IT_Update, DISABLE);
        TIM_ClearITPendingBit(TIM4, TIM_IT_Update);
        NVIC_DisableIRQ(TIM4_IRQn);
        NVIC_ClearPendingIRQ(TIM4_IRQn);
        RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM4, DISABLE);
        s_ledbar.scan_timer_initialized = 0u;
    }
    s_ledbar.scan_timer_enabled = 0u;
}

static void LedBar_AddDigitRoutes(uint32_t *target_mask,
                                  uint8_t digit,
                                  uint8_t route_a)
{
    uint8_t digit_mask = s_ledbar_digit_map[digit % 10u];
    uint8_t segment;

    for (segment = 0u; segment < 7u; ++segment)
    {
        if ((digit_mask & (uint8_t)(1u << segment)) != 0u)
        {
            *target_mask |= (1UL << (route_a + segment));
        }
    }
}

static uint32_t LedBar_BuildTargetMask(uint8_t value, uint8_t indicator_mask)
{
    uint8_t tens = (uint8_t)((value / 10u) % 10u);
    uint8_t ones = (uint8_t)(value % 10u);
    uint32_t target_mask = 0u;

    if (value >= 100u)
    {
        target_mask |= (1UL << LEDBAR_ROUTE_HUNDREDS_1_UPPER);
        target_mask |= (1UL << LEDBAR_ROUTE_HUNDREDS_1_LOWER);
    }

    if (value >= 10u)
    {
        LedBar_AddDigitRoutes(&target_mask, tens, LEDBAR_ROUTE_TENS_A);
    }

    LedBar_AddDigitRoutes(&target_mask, ones, LEDBAR_ROUTE_ONES_A);

    if ((indicator_mask & LEDBAR_ICON_CHARGE_MASK) != 0u)
    {
        target_mask |= (1UL << LEDBAR_ROUTE_ICON_CHARGE);
    }
    if ((indicator_mask & LEDBAR_ICON_PERCENT_MASK) != 0u)
    {
        target_mask |= (1UL << LEDBAR_ROUTE_ICON_PERCENT);
    }

    return target_mask;
}

static void LedBar_FrameClear(LedBarFrame *frame)
{
    memset(frame->routes, 0, sizeof(frame->routes));
    frame->length = 0u;
}

static void LedBar_FrameAddRoute(LedBarFrame *frame, uint8_t route_id)
{
    if (frame->length >= LEDBAR_FRAME_ROUTE_COUNT)
    {
        return;
    }
    frame->routes[frame->length] = route_id;
    frame->length++;
}

static uint8_t LedBar_FindRouteByPins(uint8_t low_pin, uint8_t high_pin)
{
    uint8_t route_id;

    for (route_id = 0u; route_id < (uint8_t)LEDBAR_ROUTE_COUNT; ++route_id)
    {
        if ((s_ledbar_routes[route_id].low_pin == low_pin) &&
            (s_ledbar_routes[route_id].high_pin == high_pin))
        {
            return route_id;
        }
    }

    return (uint8_t)LEDBAR_ROUTE_COUNT;
}

static uint16_t LedBar_TransitionCost(uint8_t prev_route_id,
                                      uint8_t next_route_id,
                                      uint32_t target_mask)
{
    const LedBarRoute *prev_route = &s_ledbar_routes[prev_route_id];
    const LedBarRoute *next_route = &s_ledbar_routes[next_route_id];
    uint8_t ghost_route;
    uint16_t cost = 0u;

    /*
     * Charlieplexing line changes can momentarily combine one pin from the
     * previous route with one pin from the next route. Prefer scan orders
     * that do not create an off-target route during that transition.
     */
    if ((prev_route->low_pin != next_route->low_pin) &&
        (prev_route->low_pin != next_route->high_pin))
    {
        ghost_route = LedBar_FindRouteByPins(prev_route->low_pin,
                                             next_route->high_pin);
        if ((ghost_route < (uint8_t)LEDBAR_ROUTE_COUNT) &&
            (ghost_route != next_route_id))
        {
            cost = (uint16_t)(cost +
                              (((target_mask & (1UL << ghost_route)) == 0u)
                                   ? LEDBAR_TRANSITION_OFF_GHOST_COST
                                   : LEDBAR_TRANSITION_ON_GHOST_COST));
        }
    }

    if ((prev_route->high_pin != next_route->low_pin) &&
        (prev_route->high_pin != next_route->high_pin))
    {
        ghost_route = LedBar_FindRouteByPins(next_route->low_pin,
                                             prev_route->high_pin);
        if ((ghost_route < (uint8_t)LEDBAR_ROUTE_COUNT) &&
            (ghost_route != prev_route_id))
        {
            cost = (uint16_t)(cost +
                              (((target_mask & (1UL << ghost_route)) == 0u)
                                   ? LEDBAR_TRANSITION_OFF_GHOST_COST
                                   : LEDBAR_TRANSITION_ON_GHOST_COST));
        }
    }

    if ((prev_route->low_pin != next_route->low_pin) &&
        (prev_route->low_pin != next_route->high_pin) &&
        (prev_route->high_pin != next_route->low_pin) &&
        (prev_route->high_pin != next_route->high_pin))
    {
        cost = (uint16_t)(cost + LEDBAR_TRANSITION_NO_SHARED_PIN_COST);
    }

    return cost;
}

static uint16_t LedBar_FrameTransitionCost(const LedBarFrame *frame,
                                           uint32_t target_mask)
{
    uint8_t index;
    uint8_t next_index;
    uint16_t cost = 0u;

    if (frame->length < 2u)
    {
        return 0u;
    }

    for (index = 0u; index < frame->length; ++index)
    {
        next_index = (uint8_t)(index + 1u);
        if (next_index >= frame->length)
        {
            next_index = 0u;
        }
        cost = (uint16_t)(cost +
                          LedBar_TransitionCost(frame->routes[index],
                                                frame->routes[next_index],
                                                target_mask));
    }

    return cost;
}

static void LedBar_BuildGreedyFrameFromStart(LedBarFrame *frame,
                                             uint32_t target_mask,
                                             uint8_t start_route)
{
    uint32_t remaining_mask = target_mask;
    uint8_t route_id;
    uint8_t prev_route;
    uint8_t best_route;
    uint16_t route_cost;
    uint16_t best_cost;

    LedBar_FrameClear(frame);
    LedBar_FrameAddRoute(frame, start_route);
    remaining_mask &= (uint32_t)(~(1UL << start_route));

    while (remaining_mask != 0u)
    {
        prev_route = frame->routes[frame->length - 1u];
        best_route = (uint8_t)LEDBAR_ROUTE_COUNT;
        best_cost = LEDBAR_TRANSITION_MAX_COST;

        for (route_id = 0u; route_id < (uint8_t)LEDBAR_ROUTE_COUNT; ++route_id)
        {
            if ((remaining_mask & (1UL << route_id)) == 0u)
            {
                continue;
            }

            route_cost = LedBar_TransitionCost(prev_route,
                                               route_id,
                                               target_mask);
            if ((best_route >= (uint8_t)LEDBAR_ROUTE_COUNT) ||
                (route_cost < best_cost))
            {
                best_route = route_id;
                best_cost = route_cost;
            }
        }

        if (best_route >= (uint8_t)LEDBAR_ROUTE_COUNT)
        {
            break;
        }
        LedBar_FrameAddRoute(frame, best_route);
        remaining_mask &= (uint32_t)(~(1UL << best_route));
    }
}

static void LedBar_SwapFrameRoutes(LedBarFrame *frame,
                                   uint8_t left_index,
                                   uint8_t right_index)
{
    uint8_t temp_route = frame->routes[left_index];

    frame->routes[left_index] = frame->routes[right_index];
    frame->routes[right_index] = temp_route;
}

static void LedBar_ImproveFrameOrder(LedBarFrame *frame, uint32_t target_mask)
{
    uint8_t pass;
    uint8_t left_index;
    uint8_t right_index;
    uint8_t improved;
    uint16_t best_cost;
    uint16_t candidate_cost;

    if (frame->length < 3u)
    {
        return;
    }

    best_cost = LedBar_FrameTransitionCost(frame, target_mask);
    for (pass = 0u; pass < LEDBAR_ORDER_IMPROVE_MAX_PASSES; ++pass)
    {
        improved = 0u;
        for (left_index = 0u; left_index < frame->length; ++left_index)
        {
            for (right_index = (uint8_t)(left_index + 1u);
                 right_index < frame->length;
                 ++right_index)
            {
                LedBar_SwapFrameRoutes(frame, left_index, right_index);
                candidate_cost = LedBar_FrameTransitionCost(frame,
                                                            target_mask);
                if (candidate_cost < best_cost)
                {
                    best_cost = candidate_cost;
                    improved = 1u;
                }
                else
                {
                    LedBar_SwapFrameRoutes(frame, left_index, right_index);
                }
            }
        }

        if (improved == 0u)
        {
            break;
        }
    }
}

static void LedBar_BuildFrameFromMask(LedBarFrame *frame, uint32_t target_mask)
{
    uint8_t route_id;
    uint8_t start_route;
    uint16_t best_cost = LEDBAR_TRANSITION_MAX_COST;
    uint16_t candidate_cost;
    LedBarFrame candidate_frame;
    LedBarFrame best_frame;

    LedBar_FrameClear(frame);
    for (route_id = 0u; route_id < (uint8_t)LEDBAR_ROUTE_COUNT; ++route_id)
    {
        if ((target_mask & (1UL << route_id)) != 0u)
        {
            break;
        }
    }
    if (route_id >= (uint8_t)LEDBAR_ROUTE_COUNT)
    {
        return;
    }

    LedBar_FrameClear(&best_frame);
    for (start_route = 0u; start_route < (uint8_t)LEDBAR_ROUTE_COUNT; ++start_route)
    {
        if ((target_mask & (1UL << start_route)) == 0u)
        {
            continue;
        }

        LedBar_BuildGreedyFrameFromStart(&candidate_frame,
                                         target_mask,
                                         start_route);
        candidate_cost = LedBar_FrameTransitionCost(&candidate_frame,
                                                    target_mask);
        if ((best_frame.length == 0u) || (candidate_cost < best_cost))
        {
            best_frame = candidate_frame;
            best_cost = candidate_cost;
        }
    }

    LedBar_ImproveFrameOrder(&best_frame, target_mask);
    *frame = best_frame;
}

static uint8_t LedBar_FrameEquals(const LedBarFrame *left, const LedBarFrame *right)
{
    uint8_t index;

    if (left->length != right->length)
    {
        return 0u;
    }

    for (index = 0u; index < left->length; ++index)
    {
        if (left->routes[index] != right->routes[index])
        {
            return 0u;
        }
    }

    return 1u;
}

static void LedBar_BuildCurrentFrame(LedBarFrame *frame)
{
    uint32_t target_mask;

    if ((s_ledbar.blank != 0u) || (s_ledbar.sleep != 0u))
    {
        LedBar_FrameClear(frame);
        return;
    }

    target_mask = LedBar_BuildTargetMask(s_ledbar.number,
                                         (uint8_t)(s_ledbar.indicator_mask &
                                                   (LEDBAR_ICON_CHARGE_MASK |
                                                    LEDBAR_ICON_PERCENT_MASK)));
    LedBar_BuildFrameFromMask(frame, target_mask);
}

static void LedBar_ApplyFrame(const LedBarFrame *frame)
{
    uint8_t same_frame = LedBar_FrameEquals(&s_ledbar.frame, frame);
    uint8_t scan_was_enabled = s_ledbar.scan_timer_enabled;

    if ((same_frame != 0u) &&
        (((frame->length == 0u) && (scan_was_enabled == 0u)) ||
         ((frame->length != 0u) && (scan_was_enabled != 0u))))
    {
        return;
    }

    if (scan_was_enabled != 0u)
    {
        TIM_ITConfig(TIM4, TIM_IT_Update, DISABLE);
        NVIC_DisableIRQ(TIM4_IRQn);
        TIM_ClearITPendingBit(TIM4, TIM_IT_Update);
    }

    s_ledbar.frame = *frame;
    s_ledbar.scan_index = 0u;

    if (s_ledbar.frame.length == 0u)
    {
        LedBar_StopScanTimer();
        LedBar_OutputOff();
        LedBar_GpioPrepareForStop();
        return;
    }

    if (scan_was_enabled == 0u)
    {
        LedBar_StartScanTimer();
    }

    LedBar_OutputRoute(s_ledbar.frame.routes[0]);
    s_ledbar.scan_index = (s_ledbar.frame.length > 1u) ? 1u : 0u;

    if (scan_was_enabled != 0u)
    {
        TIM_ITConfig(TIM4, TIM_IT_Update, ENABLE);
        NVIC_EnableIRQ(TIM4_IRQn);
    }
}

static void LedBar_RefreshOutput(void)
{
    LedBarFrame frame;

    LedBar_BuildCurrentFrame(&frame);
    LedBar_ApplyFrame(&frame);
}

static void LedBar_RequestSocDisplayWindow(void)
{
    if (s_ledbar.soc_display_10ms < LEDBAR_SOC_DISPLAY_10MS)
    {
        s_ledbar.soc_display_10ms = LEDBAR_SOC_DISPLAY_10MS;
    }
}

static void LedBar_RequestStartupDisplayWindow(void)
{
    if (s_ledbar.soc_display_10ms < LEDBAR_STARTUP_DISPLAY_10MS)
    {
        s_ledbar.soc_display_10ms = LEDBAR_STARTUP_DISPLAY_10MS;
    }
}

static void LedBar_ServiceStartupDisplayWindow(void)
{
    if (s_ledbar.startup_display_armed == 0u)
    {
        s_ledbar.startup_display_armed = 1u;
        LedBar_RequestStartupDisplayWindow();
    }
}

static void LedBar_ServiceMcuWake(void)
{
    uint8_t active = LedBar_ReadMcuWakeRaw();

    if ((s_ledbar.mcu_wk_active == 0u) && (active != 0u))
    {
        LedBar_RequestSocDisplayWindow();
    }
    s_ledbar.mcu_wk_active = active;
}

static uint8_t LedBar_IsDisplayRequested(void)
{
#if !LEDBAR_SLEEP_ENABLE
    return 1u;
#else
    if (s_ledbar.soc_display_10ms != 0u)
    {
        return 1u;
    }
    return 0u;
#endif
}

static void LedBar_ServiceSwitch(void)
{
    uint8_t pressed;
    uint8_t was_pressed;
    uint32_t now_10ms = SysTime_Get10msTickCount();

    if (g_st_SysTimeFlag.bits.b1Sys10msFlag == 0u)
    {
        return;
    }

    pressed = LedBar_ReadSwitchRaw();
    was_pressed = s_ledbar.key_active;
    s_ledbar.key_active = pressed;

    if ((was_pressed == 0u) && (pressed != 0u))
    {
        s_ledbar.key_wakeup_armed = 1u;
        LedBar_RequestSocDisplayWindow();
        s_ledbar.key_press_start_10ms = now_10ms;
        s_ledbar.key_hold_10ms = 0u;
        s_ledbar.key_long_handled = 0u;
    }

    if ((pressed != 0u) && (s_ledbar.key_wakeup_armed != 0u))
    {
        s_ledbar.key_hold_10ms = now_10ms - s_ledbar.key_press_start_10ms;

#ifdef _DI_SWITCH_longKEY_ONOFF
        if ((s_ledbar.key_hold_10ms >= LEDBAR_KEY_LONG_PRESS_10MS) &&
            (s_ledbar.key_long_handled == 0u))
        {
            s_ledbar.key_long_handled = 1u;
            // LedBar_SaveSleepSoc();
            // LowPower_Request(DEEP_MODE);
            // SleepDeal_Continue((UINT8)DEEP_MODE);
            low_power_log_and_commit_sleep(DEEP_MODE);
        }
#endif
    }
    else
    {
        s_ledbar.key_hold_10ms = 0u;
        s_ledbar.key_press_start_10ms = now_10ms;
        s_ledbar.key_long_handled = 0u;
        s_ledbar.key_wakeup_armed = 0u;
    }

    if (s_ledbar.soc_display_10ms != 0u)
    {
        s_ledbar.soc_display_10ms--;
    }
}

static uint8_t LedBar_IsFaultActive(void)
{
    if ((g_stCellInfoReport.unMdlFault_Third.all & 0x3FFBu) != 0u)
    {
        return 1u;
    }
    if (System_ERROR_UserCallback(ERROR_STATUS_TEMP_BREAK) != 0u)
    {
        return 1u;
    }
    if (System_ERROR_UserCallback(ERROR_STATUS_CBC_DSG) != 0u)
    {
        return 1u;
    }
    return 0u;
}

void LedBar_Init(void)
{
    if (s_ledbar.initialized != 0u)
    {
        return;
    }

    s_ledbar.sleep = 0u;
    s_ledbar.blank = 1u;
    s_ledbar.number = 0u;
    s_ledbar.indicator_mask = LEDBAR_ICON_PERCENT_MASK;
    LedBar_FrameClear(&s_ledbar.frame);
    s_ledbar.scan_index = 0u;
    s_ledbar.scan_timer_initialized = 0u;
    s_ledbar.scan_timer_enabled = 0u;
    s_ledbar.soc_display_10ms = 0u;
    s_ledbar.startup_display_armed = 0u;
    s_ledbar.key_hold_10ms = 0u;
    s_ledbar.key_press_start_10ms = 0u;
    s_ledbar.key_long_handled = 0u;
    s_ledbar.key_wakeup_armed = 0u;
    LedBar_GpioInitForDisplay();
    s_ledbar.key_active = LedBar_ReadSwitchRaw();
    s_ledbar.mcu_wk_active = LedBar_ReadMcuWakeRaw();
    LedBar_OutputOff();
    LedBar_GpioPrepareForStop();
    s_ledbar.initialized = 1u;
}

void LedBar_Clear(void)
{
    LedBar_EnsureInit();

    s_ledbar.blank = 1u;
    LedBar_RefreshOutput();
}

void LedBar_SetSleep(uint8_t enable)
{
    LedBar_EnsureInit();

#if !LEDBAR_SLEEP_ENABLE
    enable = 0u;
#else
    enable = (enable != 0u) ? 1u : 0u;
#endif

    if (s_ledbar.sleep == enable)
    {
        return;
    }

    s_ledbar.sleep = enable;
    LedBar_RefreshOutput();
    if (enable != 0u)
    {
        LedBar_GpioPrepareForStop();
    }
}

void LedBar_Wakeup(void)
{
    LedBar_SetSleep(0u);
}

void LedBar_SetNumber(uint8_t value)
{
    LedBar_EnsureInit();

    value = LedBar_LimitSoc(value);
    if ((s_ledbar.number == value) && (s_ledbar.blank == 0u))
    {
        return;
    }

    s_ledbar.number = value;
    s_ledbar.blank = 0u;
    LedBar_RefreshOutput();
}

void LedBar_SaveSleepSoc(void)
{
    uint16_t value = (uint16_t)(LEDBAR_SLEEP_SOC_MAGIC | LedBar_GetRuntimeSoc());

    LedBar_EnableBackupAccess();
    BKP_WriteBackupRegister(LEDBAR_SLEEP_SOC_REG, value);
    BKP_WriteBackupRegister(LEDBAR_SLEEP_SOC_INV_REG, (uint16_t)(~value));
}

uint8_t LedBar_LoadSleepSoc(void)
{
    uint16_t value;
    uint16_t value_inv;
    uint8_t soc;

    LedBar_EnableBackupAccess();
    value = BKP_ReadBackupRegister(LEDBAR_SLEEP_SOC_REG);
    value_inv = BKP_ReadBackupRegister(LEDBAR_SLEEP_SOC_INV_REG);

    if ((uint16_t)(value ^ value_inv) == 0xFFFFu)
    {
        if ((value & LEDBAR_SLEEP_SOC_MAGIC_MASK) == LEDBAR_SLEEP_SOC_MAGIC)
        {
            soc = (uint8_t)(value & LEDBAR_SLEEP_SOC_VALUE_MASK);
            if (soc <= 100u)
            {
                return soc;
            }
        }
    }

    return LedBar_GetRuntimeSoc();
}

void LedBar_ShowSleepSocPreview(void)
{
    LedBar_EnsureInit();

    s_ledbar.sleep = 0u;
    s_ledbar.blank = 0u;
    s_ledbar.number = LedBar_LoadSleepSoc();
    s_ledbar.indicator_mask = LEDBAR_ICON_PERCENT_MASK;
    LedBar_RefreshOutput();
}

void LedBar_RequestSocDisplay(void)
{
    LedBar_EnsureInit();

    LedBar_RequestSocDisplayWindow();
}

void LedBar_PrepareForStop(void)
{
    LedBar_EnsureInit();

#if LEDBAR_SLEEP_ENABLE
    s_ledbar.sleep = 1u;
    s_ledbar.blank = 1u;
    LedBar_RefreshOutput();
    LedBar_GpioPrepareForStop();
#else
    LedBar_Clear();
#endif
}

uint8_t LedBar_IsActiveForLowPower(void)
{
    if (s_ledbar.initialized == 0u)
    {
        return 0u;
    }

    if (s_ledbar.sleep != 0u)
    {
        return 0u;
    }

    if ((s_ledbar.soc_display_10ms != 0u) ||
        (s_ledbar.frame.length != 0u) ||
        (s_ledbar.scan_timer_enabled != 0u))
    {
        return 1u;
    }

    return 0u;
}

void LedBar_Scan1ms(void)
{
    if (s_ledbar.initialized == 0u)
    {
        return;
    }

    if ((s_ledbar.sleep != 0u) || (s_ledbar.frame.length == 0u))
    {
        LedBar_OutputOff();
        s_ledbar.scan_index = 0u;
        return;
    }

    if (s_ledbar.scan_index >= s_ledbar.frame.length)
    {
        s_ledbar.scan_index = 0u;
    }

    LedBar_OutputRoute(s_ledbar.frame.routes[s_ledbar.scan_index]);
    s_ledbar.scan_index++;
    if (s_ledbar.scan_index >= s_ledbar.frame.length)
    {
        s_ledbar.scan_index = 0u;
    }
}

void TIM4_IRQHandler(void)
{
    if (TIM_GetITStatus(TIM4, TIM_IT_Update) != RESET)
    {
        IrqDebug_CountFast((uint8_t)IRQDBG_TIM4_LEDBAR);
        TIM_ClearITPendingBit(TIM4, TIM_IT_Update);
        LedBar_Scan1ms();
    }
}

void APP_LedBar(void)
{
    uint8_t display_value;
    uint8_t indicator_mask = LEDBAR_ICON_PERCENT_MASK;
    uint8_t display_requested;
    uint8_t discharge_mos_open;

    LedBar_ServiceMcuWake();
    LedBar_ServiceSwitch();

    LedBar_ServiceStartupDisplayWindow();

    // display_requested = 1;
    display_requested = LedBar_IsDisplayRequested();
    if (display_requested == 0u)
    {
        if ((s_ledbar.blank == 0u) ||
            (s_ledbar.frame.length != 0u) ||
            (s_ledbar.scan_timer_enabled != 0u))
        {
            LedBar_Clear();
        }
        return;
    }

    if (s_ledbar.sleep != 0u)
    {
        LedBar_Wakeup();
    }

    if ((g_st_SysTimeFlag.bits.b1Sys100msFlag == 0u) &&
        (s_ledbar.blank == 0u))
    {
        return;
    }

    display_value = LedBar_GetRuntimeSoc();
    discharge_mos_open = LedBar_IsDischargeMosOpen();

    if (discharge_mos_open != 0u)
    {
        indicator_mask |= LEDBAR_ICON_CHARGE_MASK;
    }

    if (LedBar_IsFaultActive() != 0u)
    {
    }

    if ((s_ledbar.number != display_value) ||
        (s_ledbar.indicator_mask != indicator_mask) ||
        (s_ledbar.blank != 0u))
    {
        s_ledbar.number = display_value;
        s_ledbar.indicator_mask = indicator_mask;
        s_ledbar.blank = 0u;
        LedBar_RefreshOutput();
    }
}

#if PROJECT_CFG_DEBUG_MONITOR_ENABLE
void LedBar_GetDebugSnapshot(uint8_t *sleep, uint8_t *blank,
                             uint8_t *number, uint8_t *indicators,
                             uint16_t *disp_10ms, uint8_t *frame_len,
                             uint8_t *scan_idx, uint8_t *key_active,
                             uint8_t *charge_icon, uint8_t *percent_icon)
{
	LedBar_EnsureInit();
	if (sleep)        *sleep        = s_ledbar.sleep;
	if (blank)        *blank        = s_ledbar.blank;
	if (number)       *number       = s_ledbar.number;
	if (indicators)   *indicators   = s_ledbar.indicator_mask;
	if (disp_10ms)    *disp_10ms    = s_ledbar.soc_display_10ms;
	if (frame_len)    *frame_len    = s_ledbar.frame.length;
	if (scan_idx)     *scan_idx     = s_ledbar.scan_index;
	if (key_active)   *key_active   = s_ledbar.key_active;
	if (charge_icon)  *charge_icon  = (s_ledbar.indicator_mask & LEDBAR_ICON_CHARGE_MASK)  ? 1U : 0U;
	if (percent_icon) *percent_icon = (s_ledbar.indicator_mask & LEDBAR_ICON_PERCENT_MASK) ? 1U : 0U;
}
#endif
