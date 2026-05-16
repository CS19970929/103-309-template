#include "main.h"
#include <string.h>

#define LEDBAR_FRAME_ROUTE_COUNT ((uint8_t)LEDBAR_ROUTE_COUNT)
#define LEDBAR_SCAN_TIMER_100KHZ_TICKS PROJECT_CFG_LEDBAR_SCAN_TIMER_100KHZ_TICKS
#define LEDBAR_MCU_WK_ON_FILTER_10MS PROJECT_CFG_LEDBAR_MCU_WK_ON_FILTER_10MS
#define LEDBAR_MCU_WK_OFF_FILTER_10MS PROJECT_CFG_LEDBAR_MCU_WK_OFF_FILTER_10MS
#define LEDBAR_CHARGE_ON_FILTER_100MS PROJECT_CFG_LEDBAR_CHARGE_ON_FILTER_100MS
#define LEDBAR_CHARGE_OFF_FILTER_100MS PROJECT_CFG_LEDBAR_CHARGE_OFF_FILTER_100MS
#define LEDBAR_KEY_LONG_PRESS_10MS 300u

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

typedef struct
{
    uint8_t initialized;
    uint8_t sleep;
    uint8_t blank;
    uint8_t number;
    uint8_t indicator_mask;
    uint8_t test_single_segment_enable;
    uint8_t test_single_segment_id;
    LedBarFrame frame;
    uint8_t scan_index;
    uint8_t scan_timer_initialized;
    uint8_t scan_timer_enabled;
    uint16_t soc_display_10ms;
    uint8_t startup_display_armed;
    uint32_t key_hold_10ms;
    uint32_t key_press_start_10ms;
    uint8_t key_last_pressed;
    uint8_t key_long_handled;
    uint8_t mcu_wk_filter_initialized;
    uint8_t mcu_wk_active;
    uint8_t mcu_wk_on_10ms;
    uint8_t mcu_wk_off_10ms;
    uint8_t charge_filter_initialized;
    uint8_t charge_active;
    uint8_t charge_on_100ms;
    uint8_t charge_off_100ms;
} LedBarRuntime;

LEDBAR_COMMAND LedBar_Command = LED_BAR_STARTUP;

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

static LedBarRuntime s_ledbar_runtime =
{
    0u,
    0u,
    1u,
    0u,
    LEDBAR_ICON_PERCENT_MASK,
    0u,
    0u,
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
    0u,
    0u,
    0u,
    0u,
    0u,
    0u,
};

#if PROJECT_CFG_DEBUG_WATCH_ENABLE
LedBarRuntime * const g_dbg_ledbar_runtime = &s_ledbar_runtime;
#endif

#define s_ledbar_initialized (s_ledbar_runtime.initialized)
#define s_ledbar_sleep (s_ledbar_runtime.sleep)
#define s_ledbar_blank (s_ledbar_runtime.blank)
#define s_ledbar_number (s_ledbar_runtime.number)
#define s_ledbar_indicator_mask (s_ledbar_runtime.indicator_mask)
#define s_ledbar_test_single_segment_enable (s_ledbar_runtime.test_single_segment_enable)
#define s_ledbar_test_single_segment_id (s_ledbar_runtime.test_single_segment_id)
#define s_ledbar_frame (s_ledbar_runtime.frame)
#define s_ledbar_scan_index (s_ledbar_runtime.scan_index)
#define s_ledbar_scan_timer_initialized (s_ledbar_runtime.scan_timer_initialized)
#define s_ledbar_scan_timer_enabled (s_ledbar_runtime.scan_timer_enabled)
#define s_ledbar_soc_display_10ms (s_ledbar_runtime.soc_display_10ms)
#define s_ledbar_startup_display_armed (s_ledbar_runtime.startup_display_armed)
#define s_ledbar_key_hold_10ms (s_ledbar_runtime.key_hold_10ms)
#define s_ledbar_key_press_start_10ms (s_ledbar_runtime.key_press_start_10ms)
#define s_ledbar_key_last_pressed (s_ledbar_runtime.key_last_pressed)
#define s_ledbar_key_long_handled (s_ledbar_runtime.key_long_handled)
#define s_ledbar_mcu_wk_filter_initialized (s_ledbar_runtime.mcu_wk_filter_initialized)
#define s_ledbar_mcu_wk_active (s_ledbar_runtime.mcu_wk_active)
#define s_ledbar_mcu_wk_on_10ms (s_ledbar_runtime.mcu_wk_on_10ms)
#define s_ledbar_mcu_wk_off_10ms (s_ledbar_runtime.mcu_wk_off_10ms)
#define s_ledbar_charge_filter_initialized (s_ledbar_runtime.charge_filter_initialized)
#define s_ledbar_charge_active (s_ledbar_runtime.charge_active)
#define s_ledbar_charge_on_100ms (s_ledbar_runtime.charge_on_100ms)
#define s_ledbar_charge_off_100ms (s_ledbar_runtime.charge_off_100ms)

static void LedBar_StopScanTimer(void);
static void LedBar_RefreshOutput(void);

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

static uint8_t LedBar_ReadChargeRaw(void)
{
    return (uint8_t)(GPIO_ReadInputDataBit(GPIO_CHG_IN, PIN_CHG_IN) == Bit_RESET);
}

static uint8_t LedBar_IsDischargeMosOpen(void)
{
    return (uint8_t)(SystemStatus.bits.b1Status_MOS_DSG != 0u);
}

static uint8_t LedBar_IsSwitchPressed(void)
{
    return (uint8_t)(MCUI_ENI_DI1 == 0u);
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

    if (level != Bit_RESET)
    {
        port->BSRR = pin;
    }
    else
    {
        port->BRR = pin;
    }

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
    LedBar_PinToOutput(route->low_pin, Bit_RESET);
    LedBar_PinToOutput(route->high_pin, Bit_SET);
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

    if (s_ledbar_scan_timer_initialized != 0u)
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

    s_ledbar_scan_timer_initialized = 1u;
}

static void LedBar_StartScanTimer(void)
{
    LedBar_GpioInitForDisplay();
    LedBar_ScanTimerInit();

    TIM_ITConfig(TIM4, TIM_IT_Update, ENABLE);
    NVIC_EnableIRQ(TIM4_IRQn);
    if (s_ledbar_scan_timer_enabled == 0u)
    {
        TIM_SetCounter(TIM4, 0U);
        TIM_ClearITPendingBit(TIM4, TIM_IT_Update);
        TIM_Cmd(TIM4, ENABLE);
        s_ledbar_scan_timer_enabled = 1u;
    }
}

static void LedBar_StopScanTimer(void)
{
    if (s_ledbar_scan_timer_initialized != 0u)
    {
        TIM_Cmd(TIM4, DISABLE);
        TIM_ITConfig(TIM4, TIM_IT_Update, DISABLE);
        TIM_ClearITPendingBit(TIM4, TIM_IT_Update);
        NVIC_DisableIRQ(TIM4_IRQn);
        NVIC_ClearPendingIRQ(TIM4_IRQn);
        RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM4, DISABLE);
        s_ledbar_scan_timer_initialized = 0u;
    }
    s_ledbar_scan_timer_enabled = 0u;
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

static void LedBar_AddDigitRoutes(uint32_t *target_mask,
                                  uint8_t digit,
                                  uint8_t route_a)
{
    uint8_t digit_mask = s_ledbar_digit_map[digit % 10u];

    if ((digit_mask & LEDBAR_DIGIT_BIT_A) != 0u)
    {
        *target_mask |= (1UL << route_a);
    }
    if ((digit_mask & LEDBAR_DIGIT_BIT_B) != 0u)
    {
        *target_mask |= (1UL << (route_a + 1u));
    }
    if ((digit_mask & LEDBAR_DIGIT_BIT_C) != 0u)
    {
        *target_mask |= (1UL << (route_a + 2u));
    }
    if ((digit_mask & LEDBAR_DIGIT_BIT_D) != 0u)
    {
        *target_mask |= (1UL << (route_a + 3u));
    }
    if ((digit_mask & LEDBAR_DIGIT_BIT_E) != 0u)
    {
        *target_mask |= (1UL << (route_a + 4u));
    }
    if ((digit_mask & LEDBAR_DIGIT_BIT_F) != 0u)
    {
        *target_mask |= (1UL << (route_a + 5u));
    }
    if ((digit_mask & LEDBAR_DIGIT_BIT_G) != 0u)
    {
        *target_mask |= (1UL << (route_a + 6u));
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

static void LedBar_BuildFrameFromMask(LedBarFrame *frame, uint32_t target_mask)
{
    uint8_t route_id;

    LedBar_FrameClear(frame);
    for (route_id = 0u; route_id < (uint8_t)LEDBAR_ROUTE_COUNT; ++route_id)
    {
        if ((target_mask & (1UL << route_id)) != 0u)
        {
            LedBar_FrameAddRoute(frame, route_id);
        }
    }
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

    if ((s_ledbar_blank != 0u) || (s_ledbar_sleep != 0u))
    {
        LedBar_FrameClear(frame);
        return;
    }

    if (s_ledbar_test_single_segment_enable != 0u)
    {
        LedBar_FrameClear(frame);
        LedBar_FrameAddRoute(frame,
                             (uint8_t)(s_ledbar_test_single_segment_id %
                                       (uint8_t)LEDBAR_ROUTE_COUNT));
        return;
    }

    target_mask = LedBar_BuildTargetMask(s_ledbar_number,
                                         (uint8_t)(s_ledbar_indicator_mask &
                                                   (LEDBAR_ICON_CHARGE_MASK |
                                                    LEDBAR_ICON_PERCENT_MASK)));
    LedBar_BuildFrameFromMask(frame, target_mask);
}

static void LedBar_ApplyFrame(const LedBarFrame *frame)
{
    uint8_t same_frame = LedBar_FrameEquals(&s_ledbar_frame, frame);
    uint8_t scan_was_enabled = s_ledbar_scan_timer_enabled;

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

    s_ledbar_frame = *frame;
    s_ledbar_scan_index = 0u;

    if (s_ledbar_frame.length == 0u)
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

    LedBar_OutputRoute(s_ledbar_frame.routes[0]);
    s_ledbar_scan_index = (s_ledbar_frame.length > 1u) ? 1u : 0u;

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
    if (s_ledbar_soc_display_10ms < LEDBAR_SOC_DISPLAY_10MS)
    {
        s_ledbar_soc_display_10ms = LEDBAR_SOC_DISPLAY_10MS;
    }
}

static void LedBar_RequestStartupDisplayWindow(void)
{
    if (s_ledbar_soc_display_10ms < LEDBAR_STARTUP_DISPLAY_10MS)
    {
        s_ledbar_soc_display_10ms = LEDBAR_STARTUP_DISPLAY_10MS;
    }
}

static void LedBar_ServiceStartupDisplayWindow(void)
{
    if (s_ledbar_startup_display_armed == 0u)
    {
        s_ledbar_startup_display_armed = 1u;
        LedBar_RequestStartupDisplayWindow();
    }
}

static void LedBar_ServiceMcuWakeFilter(void)
{
    uint8_t raw_active = LedBar_ReadMcuWakeRaw();

    if (s_ledbar_mcu_wk_filter_initialized == 0u)
    {
        s_ledbar_mcu_wk_filter_initialized = 1u;
        s_ledbar_mcu_wk_active = raw_active;
        s_ledbar_mcu_wk_on_10ms = (raw_active != 0u) ? LEDBAR_MCU_WK_ON_FILTER_10MS : 0u;
        s_ledbar_mcu_wk_off_10ms = (raw_active == 0u) ? LEDBAR_MCU_WK_OFF_FILTER_10MS : 0u;
        return;
    }

    if (g_st_SysTimeFlag.bits.b1Sys10msFlag == 0u)
    {
        return;
    }

    if (raw_active != 0u)
    {
        s_ledbar_mcu_wk_off_10ms = 0u;
        if (s_ledbar_mcu_wk_on_10ms < LEDBAR_MCU_WK_ON_FILTER_10MS)
        {
            s_ledbar_mcu_wk_on_10ms++;
        }
        if (s_ledbar_mcu_wk_on_10ms >= LEDBAR_MCU_WK_ON_FILTER_10MS)
        {
            s_ledbar_mcu_wk_active = 1u;
        }
    }
    else
    {
        s_ledbar_mcu_wk_on_10ms = 0u;
        if (s_ledbar_mcu_wk_off_10ms < LEDBAR_MCU_WK_OFF_FILTER_10MS)
        {
            s_ledbar_mcu_wk_off_10ms++;
        }
        if (s_ledbar_mcu_wk_off_10ms >= LEDBAR_MCU_WK_OFF_FILTER_10MS)
        {
            s_ledbar_mcu_wk_active = 0u;
        }
    }
}

static uint8_t LedBar_IsMcuWakeActive(void)
{
    return s_ledbar_mcu_wk_active;
}

static void LedBar_ServiceChargeFilter(uint8_t raw_active)
{
    if (s_ledbar_charge_filter_initialized == 0u)
    {
        s_ledbar_charge_filter_initialized = 1u;
        s_ledbar_charge_active = 0u;
        s_ledbar_charge_on_100ms = 0u;
        s_ledbar_charge_off_100ms = LEDBAR_CHARGE_OFF_FILTER_100MS;
    }

    if (g_st_SysTimeFlag.bits.b1Sys100msFlag == 0u)
    {
        return;
    }

    if (raw_active != 0u)
    {
        s_ledbar_charge_off_100ms = 0u;
        if (s_ledbar_charge_on_100ms < LEDBAR_CHARGE_ON_FILTER_100MS)
        {
            s_ledbar_charge_on_100ms++;
        }
        if (s_ledbar_charge_on_100ms >= LEDBAR_CHARGE_ON_FILTER_100MS)
        {
            s_ledbar_charge_active = 1u;
        }
    }
    else
    {
        s_ledbar_charge_on_100ms = 0u;
        if (s_ledbar_charge_off_100ms < LEDBAR_CHARGE_OFF_FILTER_100MS)
        {
            s_ledbar_charge_off_100ms++;
        }
        if (s_ledbar_charge_off_100ms >= LEDBAR_CHARGE_OFF_FILTER_100MS)
        {
            s_ledbar_charge_active = 0u;
        }
    }
}

static uint8_t LedBar_IsDisplayRequested(void)
{
#if LEDBAR_TEST_ALWAYS_ON
    return 1u;
#elif !LEDBAR_SLEEP_ENABLE
    return 1u;
#else
    if (LedBar_IsMcuWakeActive() != 0u)
    {
        return 1u;
    }
    if ((s_ledbar_soc_display_10ms != 0u) || (s_ledbar_key_last_pressed != 0u))
    {
        return 1u;
    }
    return 0u;
#endif
}

static void LedBar_ServiceSwitch(void)
{
    uint8_t pressed = LedBar_IsSwitchPressed();
    uint32_t now_10ms = SysTime_Get10msTickCount();

    if ((pressed != 0u) && (s_ledbar_key_last_pressed == 0u))
    {
        LedBar_RequestSocDisplayWindow();
        s_ledbar_key_press_start_10ms = now_10ms;
        s_ledbar_key_hold_10ms = 0u;
    }
    s_ledbar_key_last_pressed = pressed;

    if (pressed != 0u)
    {
        s_ledbar_key_hold_10ms = now_10ms - s_ledbar_key_press_start_10ms;

#ifdef _DI_SWITCH_longKEY_ONOFF
        if ((s_ledbar_key_hold_10ms >= LEDBAR_KEY_LONG_PRESS_10MS) &&
            (s_ledbar_key_long_handled == 0u))
        {
            s_ledbar_key_long_handled = 1u;
#if LEDBAR_LONG_PRESS_GPIO_TOGGLE_TEST
            {
                BitAction dc_state = (GPIO_ReadOutputDataBit(GPIO_DC_EN, PIN_DC_EN) == Bit_RESET) ? Bit_SET : Bit_RESET;
                BitAction en2727_state = (GPIO_ReadOutputDataBit(GPIO_2727_EN, PIN_2737_EN) == Bit_RESET) ? Bit_SET : Bit_RESET;
                GPIO_WriteBit(GPIO_DC_EN, PIN_DC_EN, dc_state);
                GPIO_WriteBit(GPIO_2727_EN, PIN_2737_EN, en2727_state);
            }
#else
            LedBar_SaveSleepSoc();
            entersleep(DEEP_MODE);
            SleepDeal_Continue();
#endif
        }
#endif
    }
    else
    {
        s_ledbar_key_hold_10ms = 0u;
        s_ledbar_key_press_start_10ms = now_10ms;
        s_ledbar_key_long_handled = 0u;
        if (g_st_SysTimeFlag.bits.b1Sys10msFlag == 0u)
        {
            return;
        }
        if (s_ledbar_soc_display_10ms != 0u)
        {
            s_ledbar_soc_display_10ms--;
        }
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
    if (s_ledbar_initialized != 0u)
    {
        return;
    }

    s_ledbar_sleep = 0u;
    s_ledbar_blank = 1u;
    s_ledbar_number = 0u;
    s_ledbar_indicator_mask = LEDBAR_ICON_PERCENT_MASK;
    s_ledbar_test_single_segment_enable = 0u;
    s_ledbar_test_single_segment_id = 0u;
    LedBar_FrameClear(&s_ledbar_frame);
    s_ledbar_scan_index = 0u;
    s_ledbar_scan_timer_initialized = 0u;
    s_ledbar_scan_timer_enabled = 0u;
    s_ledbar_soc_display_10ms = 0u;
    s_ledbar_startup_display_armed = 0u;
    s_ledbar_key_hold_10ms = 0u;
    s_ledbar_key_press_start_10ms = 0u;
    s_ledbar_key_last_pressed = 0u;
    s_ledbar_key_long_handled = 0u;
    s_ledbar_mcu_wk_filter_initialized = 0u;
    s_ledbar_mcu_wk_active = 0u;
    s_ledbar_mcu_wk_on_10ms = 0u;
    s_ledbar_mcu_wk_off_10ms = 0u;
    s_ledbar_charge_filter_initialized = 0u;
    s_ledbar_charge_active = 0u;
    s_ledbar_charge_on_100ms = 0u;
    s_ledbar_charge_off_100ms = 0u;
    LedBar_Command = LED_BAR_NORMAL;

    LedBar_GpioInitForDisplay();
    LedBar_OutputOff();
    LedBar_GpioPrepareForStop();
    s_ledbar_initialized = 1u;
}

void LedBar_Clear(void)
{
    if (s_ledbar_initialized == 0u)
    {
        LedBar_Init();
    }

    s_ledbar_blank = 1u;
    LedBar_RefreshOutput();
}

void LedBar_SetSleep(uint8_t enable)
{
    if (s_ledbar_initialized == 0u)
    {
        LedBar_Init();
    }

#if !LEDBAR_SLEEP_ENABLE
    enable = 0u;
#else
    enable = (enable != 0u) ? 1u : 0u;
#endif

    if (s_ledbar_sleep == enable)
    {
        return;
    }

    s_ledbar_sleep = enable;
    LedBar_RefreshOutput();
}

void LedBar_Wakeup(void)
{
    LedBar_SetSleep(0u);
}

void LedBar_EnableSingleSegmentTest(uint8_t enable)
{
    if (s_ledbar_initialized == 0u)
    {
        LedBar_Init();
    }

    enable = (enable != 0u) ? 1u : 0u;
    if (s_ledbar_test_single_segment_enable == enable)
    {
        return;
    }

    s_ledbar_test_single_segment_enable = enable;
    if (enable == 0u)
    {
        s_ledbar_test_single_segment_id = 0u;
    }
    s_ledbar_blank = 0u;
    LedBar_RefreshOutput();
}

void LedBar_SetSingleSegmentIndex(uint8_t segment_id)
{
    if (s_ledbar_initialized == 0u)
    {
        LedBar_Init();
    }

    if (segment_id > LEDBAR_SINGLE_SEG_ID_MAX)
    {
        segment_id = LEDBAR_SINGLE_SEG_ID_MAX;
    }
    if (s_ledbar_test_single_segment_id == segment_id)
    {
        return;
    }

    s_ledbar_test_single_segment_id = segment_id;
    if (s_ledbar_test_single_segment_enable != 0u)
    {
        LedBar_RefreshOutput();
    }
}

void LedBar_SetNumber(uint8_t value)
{
    if (s_ledbar_initialized == 0u)
    {
        LedBar_Init();
    }

    value = LedBar_LimitSoc(value);
    if ((s_ledbar_number == value) && (s_ledbar_blank == 0u))
    {
        return;
    }

    s_ledbar_number = value;
    s_ledbar_blank = 0u;
    LedBar_RefreshOutput();
}

void LedBar_SetIndicators(uint8_t indicator_mask)
{
    if (s_ledbar_initialized == 0u)
    {
        LedBar_Init();
    }

    indicator_mask = (uint8_t)(indicator_mask &
                               (LEDBAR_ICON_CHARGE_MASK |
                                LEDBAR_ICON_PERCENT_MASK));
    if ((s_ledbar_indicator_mask == indicator_mask) && (s_ledbar_blank == 0u))
    {
        return;
    }

    s_ledbar_indicator_mask = indicator_mask;
    s_ledbar_blank = 0u;
    LedBar_RefreshOutput();
}

void LedBar_SetIndicatorState(uint8_t indicator_mask, uint8_t enable)
{
    uint8_t new_mask;

    if (s_ledbar_initialized == 0u)
    {
        LedBar_Init();
    }

    indicator_mask = (uint8_t)(indicator_mask &
                               (LEDBAR_ICON_CHARGE_MASK |
                                LEDBAR_ICON_PERCENT_MASK));
    if (enable != 0u)
    {
        new_mask = (uint8_t)(s_ledbar_indicator_mask | indicator_mask);
    }
    else
    {
        new_mask = (uint8_t)(s_ledbar_indicator_mask & (uint8_t)(~indicator_mask));
    }

    LedBar_SetIndicators(new_mask);
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
    if (s_ledbar_initialized == 0u)
    {
        LedBar_Init();
    }

    s_ledbar_sleep = 0u;
    s_ledbar_blank = 0u;
    s_ledbar_number = LedBar_LoadSleepSoc();
    s_ledbar_indicator_mask = LEDBAR_ICON_PERCENT_MASK;
    LedBar_RefreshOutput();
}

void LedBar_PrepareForStop(void)
{
    if (s_ledbar_initialized == 0u)
    {
        LedBar_Init();
    }

#if LEDBAR_SLEEP_ENABLE
    s_ledbar_sleep = 1u;
    s_ledbar_blank = 1u;
    LedBar_RefreshOutput();
#else
    LedBar_Clear();
#endif
}

void LedBar_Scan1ms(void)
{
    if (s_ledbar_initialized == 0u)
    {
        LedBar_Init();
    }

    if ((s_ledbar_sleep != 0u) || (s_ledbar_frame.length == 0u))
    {
        LedBar_OutputOff();
        s_ledbar_scan_index = 0u;
        return;
    }

    if (s_ledbar_scan_index >= s_ledbar_frame.length)
    {
        s_ledbar_scan_index = 0u;
    }

    LedBar_OutputRoute(s_ledbar_frame.routes[s_ledbar_scan_index]);
    s_ledbar_scan_index++;
    if (s_ledbar_scan_index >= s_ledbar_frame.length)
    {
        s_ledbar_scan_index = 0u;
    }
}

void TIM4_IRQHandler(void)
{
    if (TIM_GetITStatus(TIM4, TIM_IT_Update) != RESET)
    {
        TIM_ClearITPendingBit(TIM4, TIM_IT_Update);
        LedBar_Scan1ms();
    }
}

void APP_LedBar(void)
{
    uint8_t display_value;
    uint8_t indicator_mask = LEDBAR_ICON_PERCENT_MASK;
    uint8_t display_requested;
    uint8_t mcu_wk_active;
    uint8_t charge_raw_active;
    uint8_t discharge_mos_open;

    if (s_ledbar_initialized == 0u)
    {
        LedBar_Init();
    }

    LedBar_ServiceMcuWakeFilter();
    LedBar_ServiceSwitch();
    mcu_wk_active = LedBar_IsMcuWakeActive();

#if LEDBAR_SLEEP_ENABLE
    if (SystemStatus.bits.b1StartUpBMS != 0u)
    {
        LedBar_Command = LED_BAR_STARTUP;
        LedBar_SetSleep(1u);
        return;
    }

    if ((Sleep_Mode.bits.b1_ToSleepFlag != 0u) && (mcu_wk_active == 0u))
    {
        LedBar_SaveSleepSoc();
        LedBar_SetSleep(1u);
        return;
    }
#endif

    LedBar_ServiceStartupDisplayWindow();

    if (s_ledbar_test_single_segment_enable != 0u)
    {
        if (s_ledbar_sleep != 0u)
        {
            LedBar_Wakeup();
        }
        LedBar_Command = LED_BAR_NORMAL;
        return;
    }

    display_requested = LedBar_IsDisplayRequested();
    if (display_requested == 0u)
    {
        if ((s_ledbar_blank == 0u) ||
            (s_ledbar_frame.length != 0u) ||
            (s_ledbar_scan_timer_enabled != 0u))
        {
            LedBar_Clear();
        }
        return;
    }

    if (s_ledbar_sleep != 0u)
    {
        LedBar_Wakeup();
    }

    if ((g_st_SysTimeFlag.bits.b1Sys100msFlag == 0u) &&
        (s_ledbar_blank == 0u))
    {
        return;
    }

    display_value = LedBar_GetRuntimeSoc();
    charge_raw_active = LedBar_ReadChargeRaw();
    LedBar_ServiceChargeFilter(charge_raw_active);
    discharge_mos_open = LedBar_IsDischargeMosOpen();

    if (discharge_mos_open != 0u)
    {
        indicator_mask |= LEDBAR_ICON_CHARGE_MASK;
    }

    if (s_ledbar_charge_active != 0u)
    {
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

    if (LedBar_IsFaultActive() != 0u)
    {
        LedBar_Command = LED_BAR_FAULT;
    }

    if ((s_ledbar_number != display_value) ||
        (s_ledbar_indicator_mask != indicator_mask) ||
        (s_ledbar_blank != 0u))
    {
        s_ledbar_number = display_value;
        s_ledbar_indicator_mask = indicator_mask;
        s_ledbar_blank = 0u;
        LedBar_RefreshOutput();
    }
}
