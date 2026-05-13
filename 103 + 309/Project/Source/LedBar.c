#include "main.h"

#define LEDBAR_KEY_LONG_PRESS_10MS 300u
#define LEDBAR_CHG_ANIMATION_PERIOD_10MS 20u
#define LEDBAR_POWER_ON_DISPLAY_10MS 1000u
#define LEDBAR_SLEEP_SOC_MAGIC 0x5A00u
#define LEDBAR_SLEEP_SOC_MAGIC_MASK 0xFF00u
#define LEDBAR_SLEEP_SOC_VALUE_MASK 0x00FFu
#define LEDBAR_SLEEP_SOC_REG BKP_DR4
#define LEDBAR_SLEEP_SOC_INV_REG BKP_DR5

#define LEDBAR_LED_ON Bit_SET
#define LEDBAR_LED_OFF Bit_RESET

typedef struct
{
    GPIO_TypeDef *port;
    uint16_t pin;
    uint8_t on_threshold;
} LedBarDiscreteLed;

LEDBAR_COMMAND LedBar_Command = LED_BAR_STARTUP;

static const LedBarDiscreteLed s_ledbar_leds[LEDBAR_SOC_LED_COUNT] =
{
    {GPIO_SOC_LED_25, PIN_SOC_LED_25, 1u},
    {GPIO_SOC_LED_50, PIN_SOC_LED_50, 26u},
    {GPIO_SOC_LED_75, PIN_SOC_LED_75, 51u},
    {GPIO_SOC_LED_100, PIN_SOC_LED_100, 76u},
};

static uint8_t s_ledbar_initialized = 0u;
static uint8_t s_ledbar_number = 0u;
static uint8_t s_ledbar_indicator_mask = LEDBAR_ICON_PERCENT_MASK;
static uint8_t s_ledbar_force_blank = 1u;
static uint8_t s_ledbar_sleep = 0u;
static uint8_t s_ledbar_test_single_segment_enable = 0u;
static uint8_t s_ledbar_test_single_segment_id = 0u;
static uint16_t s_ledbar_soc_display_10ms = 0u;
static uint16_t s_ledbar_key_hold_10ms = 0u;
static uint8_t s_ledbar_key_last_pressed = 0u;
static uint8_t s_ledbar_key_long_handled = 0u;
static uint16_t s_ledbar_power_on_display_10ms = 0u;
static uint8_t s_ledbar_wait_key_release_after_boot = 0u;
static uint8_t s_ledbar_main_switch_sleep_handled = 0u;
static uint8_t s_ledbar_charge_animation_enable = 0u;
static uint8_t s_ledbar_charge_animation_step = 0u;
static uint16_t s_ledbar_charge_animation_10ms = 0u;

static void LedBar_GpioInitForDisplay(void);
static void LedBar_GpioPrepareForStop(void);
static void LedBar_OutputOff(void);
static void LedBar_StopChargeAnimation(void);
static void LedBar_ApplyOutput(void);

static void LedBar_EnableBackupAccess(void)
{
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_PWR | RCC_APB1Periph_BKP, ENABLE);
    PWR_BackupAccessCmd(ENABLE);
}

static uint8_t LedBar_GetRuntimeSoc(void)
{
    uint16_t soc = g_stCellInfoReport.SocElement.u16Soc;

    if (soc > 100u)
    {
        soc = 100u;
    }

    return (uint8_t)soc;
}

static uint16_t LedBar_GetAllLedPins(void)
{
    return (uint16_t)(PIN_SOC_LED_25 | PIN_SOC_LED_50 | PIN_SOC_LED_75 | PIN_SOC_LED_100);
}

static void LedBar_ConfigureKeyInput(void)
{
    GPIO_InitTypeDef gpio_init;

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA | RCC_APB2Periph_AFIO, ENABLE);
    gpio_init.GPIO_Pin = PIN_SOC_KEY;
    gpio_init.GPIO_Mode = GPIO_Mode_IPU;
    GPIO_Init(GPIO_SOC_KEY, &gpio_init);

    gpio_init.GPIO_Pin = PIN_MAIN_SW;
    GPIO_Init(GPIO_MAIN_SW, &gpio_init);
}

static void LedBar_ConfigureLedsOutput(void)
{
    GPIO_InitTypeDef gpio_init;

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE);
    GPIO_WriteBit(GPIO_SOC_LED_25, PIN_SOC_LED_25, LEDBAR_LED_OFF);
    GPIO_WriteBit(GPIO_SOC_LED_50, PIN_SOC_LED_50, LEDBAR_LED_OFF);
    GPIO_WriteBit(GPIO_SOC_LED_75, PIN_SOC_LED_75, LEDBAR_LED_OFF);
    GPIO_WriteBit(GPIO_SOC_LED_100, PIN_SOC_LED_100, LEDBAR_LED_OFF);

    gpio_init.GPIO_Pin = LedBar_GetAllLedPins();
    gpio_init.GPIO_Mode = GPIO_Mode_Out_PP;
    gpio_init.GPIO_Speed = GPIO_Speed_2MHz;
    GPIO_Init(GPIOA, &gpio_init);
}

static void LedBar_ConfigureLedsAnalog(void)
{
    GPIO_InitTypeDef gpio_init;

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE);
    LedBar_OutputOff();
    gpio_init.GPIO_Pin = LedBar_GetAllLedPins();
    gpio_init.GPIO_Mode = GPIO_Mode_AIN;
    gpio_init.GPIO_Speed = GPIO_Speed_2MHz;
    GPIO_Init(GPIOA, &gpio_init);
}

static void LedBar_GpioInitForDisplay(void)
{
    LedBar_ConfigureKeyInput();
    LedBar_ConfigureLedsOutput();
}

static void LedBar_GpioPrepareForStop(void)
{
    LedBar_ConfigureLedsAnalog();
    LedBar_ConfigureKeyInput();
}

static void LedBar_WriteLed(uint8_t led_index, uint8_t on)
{
    BitAction level;

    if (led_index >= LEDBAR_SOC_LED_COUNT)
    {
        return;
    }

    level = (on != 0u) ? LEDBAR_LED_ON : LEDBAR_LED_OFF;
    GPIO_WriteBit(s_ledbar_leds[led_index].port, s_ledbar_leds[led_index].pin, level);
}

static void LedBar_OutputOff(void)
{
    uint8_t i;

    for (i = 0u; i < LEDBAR_SOC_LED_COUNT; ++i)
    {
        LedBar_WriteLed(i, 0u);
    }
}

static void LedBar_OutputSoc(uint8_t value)
{
    uint8_t i;

    if (value > 100u)
    {
        value = 100u;
    }

    for (i = 0u; i < LEDBAR_SOC_LED_COUNT; ++i)
    {
        LedBar_WriteLed(i, (uint8_t)(value >= s_ledbar_leds[i].on_threshold));
    }
}

static uint8_t LedBar_GetChargeSolidCount(uint8_t value)
{
    if (value >= 100u)
    {
        return LEDBAR_SOC_LED_COUNT;
    }
    if (value >= 75u)
    {
        return 3u;
    }
    if (value >= 50u)
    {
        return 2u;
    }
    if (value >= 25u)
    {
        return 1u;
    }

    return 0u;
}

static void LedBar_OutputChargeAnimation(void)
{
    uint8_t i;
    uint8_t solid_count = LedBar_GetChargeSolidCount(s_ledbar_number);
    uint8_t blink_on = (s_ledbar_charge_animation_step != 0u) ? 1u : 0u;

    for (i = 0u; i < LEDBAR_SOC_LED_COUNT; ++i)
    {
        if (i < solid_count)
        {
            LedBar_WriteLed(i, 1u);
        }
        else if ((i == solid_count) && (solid_count < LEDBAR_SOC_LED_COUNT))
        {
            LedBar_WriteLed(i, blink_on);
        }
        else
        {
            LedBar_WriteLed(i, 0u);
        }
    }
}

static void LedBar_ApplyOutput(void)
{
    if (s_ledbar_sleep != 0u)
    {
        LedBar_OutputOff();
        return;
    }

    LedBar_GpioInitForDisplay();

    if (s_ledbar_force_blank != 0u)
    {
        LedBar_OutputOff();
        return;
    }

    if (s_ledbar_charge_animation_enable != 0u)
    {
        LedBar_OutputChargeAnimation();
        return;
    }

    if (s_ledbar_test_single_segment_enable != 0u)
    {
        uint8_t i;
        for (i = 0u; i < LEDBAR_SOC_LED_COUNT; ++i)
        {
            LedBar_WriteLed(i, (uint8_t)(i == s_ledbar_test_single_segment_id));
        }
        return;
    }

    LedBar_OutputSoc(s_ledbar_number);
}

static uint8_t LedBar_IsSwitchPressed(void)
{
    uint8_t raw = GPIO_ReadInputDataBit(GPIO_SOC_KEY, PIN_SOC_KEY);

    return (uint8_t)(raw == Bit_RESET);
}

static uint8_t LedBar_IsMainSwitchClosed(void)
{
    uint8_t raw = GPIO_ReadInputDataBit(GPIO_MAIN_SW, PIN_MAIN_SW);

    return (uint8_t)(raw == Bit_RESET);
}

static void LedBar_RequestSocDisplayWindow(void)
{
    s_ledbar_soc_display_10ms = LEDBAR_SOC_DISPLAY_10MS;
}

static void LedBar_StopChargeAnimation(void)
{
    s_ledbar_charge_animation_enable = 0u;
    s_ledbar_charge_animation_step = 0u;
    s_ledbar_charge_animation_10ms = 0u;
}

static uint8_t LedBar_IsChargeActive(void)
{
    return (g_stCellInfoReport.u16Ichg != 0u) ? 1u : 0u;
}

static void LedBar_RunChargeAnimation(void)
{
    if (s_ledbar_charge_animation_enable == 0u)
    {
        s_ledbar_charge_animation_step = 1u;
        s_ledbar_charge_animation_10ms = 0u;
    }

    s_ledbar_charge_animation_enable = 1u;
    s_ledbar_force_blank = 0u;
    s_ledbar_number = LedBar_GetRuntimeSoc();
    LedBar_Command = LED_BAR_CHG;
    LedBar_SetSleep(0u);

    if (g_st_SysTimeFlag.bits.b1Sys10msFlag != 0u)
    {
        if (++s_ledbar_charge_animation_10ms >= LEDBAR_CHG_ANIMATION_PERIOD_10MS)
        {
            s_ledbar_charge_animation_10ms = 0u;
            s_ledbar_charge_animation_step ^= 1u;
        }
    }

    LedBar_ApplyOutput();
}

static uint8_t LedBar_IsSocDisplayRequested(void)
{
    if ((s_ledbar_soc_display_10ms != 0u) ||
        (s_ledbar_power_on_display_10ms != 0u) ||
        (s_ledbar_key_last_pressed != 0u) ||
        (s_ledbar_wait_key_release_after_boot != 0u))
    {
        return 1u;
    }

    return 0u;
}

static void LedBar_ServiceSwitch(void)
{
    uint8_t pressed = LedBar_IsSwitchPressed();
    uint8_t main_switch_closed = LedBar_IsMainSwitchClosed();

    if (main_switch_closed == 0u)
    {
        if (s_ledbar_main_switch_sleep_handled == 0u)
        {
            s_ledbar_main_switch_sleep_handled = 1u;
            LedBar_SaveSleepSoc();
            LedBar_SetSleep(1u);
            entersleep(DEEP_MODE);
            SleepDeal_Continue();
        }
        return;
    }

    s_ledbar_main_switch_sleep_handled = 0u;

    if ((pressed != 0u) && (s_ledbar_key_last_pressed == 0u))
    {
        LedBar_RequestSocDisplayWindow();
    }

    if (g_st_SysTimeFlag.bits.b1Sys10msFlag == 0u)
    {
        s_ledbar_key_last_pressed = pressed;
        return;
    }

    if (s_ledbar_power_on_display_10ms != 0u)
    {
        s_ledbar_power_on_display_10ms--;
    }

    if (s_ledbar_wait_key_release_after_boot != 0u)
    {
        s_ledbar_key_hold_10ms = 0u;
        s_ledbar_key_long_handled = 0u;
        if (pressed != 0u)
        {
            s_ledbar_key_last_pressed = pressed;
            return;
        }
        s_ledbar_wait_key_release_after_boot = 0u;
    }

    if (pressed != 0u)
    {
        if (s_ledbar_key_hold_10ms < LEDBAR_KEY_LONG_PRESS_10MS)
        {
            s_ledbar_key_hold_10ms++;
        }
        if ((s_ledbar_key_hold_10ms >= LEDBAR_KEY_LONG_PRESS_10MS) &&
            (s_ledbar_key_long_handled == 0u))
        {
            s_ledbar_key_long_handled = 1u;
            LedBar_SaveSleepSoc();
            entersleep(DEEP_MODE);
            SleepDeal_Continue();
        }
    }
    else
    {
        s_ledbar_key_hold_10ms = 0u;
        s_ledbar_key_long_handled = 0u;
        if (s_ledbar_soc_display_10ms != 0u)
        {
            s_ledbar_soc_display_10ms--;
        }
    }

    s_ledbar_key_last_pressed = pressed;
}

void LedBar_Init(void)
{
    if (s_ledbar_initialized != 0u)
    {
        return;
    }

    s_ledbar_number = 0u;
    s_ledbar_indicator_mask = LEDBAR_ICON_PERCENT_MASK;
    s_ledbar_force_blank = 1u;
    s_ledbar_sleep = 0u;
    s_ledbar_test_single_segment_enable = 0u;
    s_ledbar_test_single_segment_id = 0u;
    s_ledbar_soc_display_10ms = 0u;
    s_ledbar_key_hold_10ms = 0u;
    s_ledbar_key_last_pressed = 0u;
    s_ledbar_key_long_handled = 0u;
    s_ledbar_power_on_display_10ms = LEDBAR_POWER_ON_DISPLAY_10MS;
    s_ledbar_wait_key_release_after_boot = 0u;
    s_ledbar_main_switch_sleep_handled = 0u;
    LedBar_StopChargeAnimation();
    LedBar_Command = LED_BAR_NORMAL;

    LedBar_GpioInitForDisplay();
    s_ledbar_key_last_pressed = LedBar_IsSwitchPressed();
    s_ledbar_wait_key_release_after_boot = s_ledbar_key_last_pressed;
    LedBar_OutputOff();
    s_ledbar_initialized = 1u;
    LedBar_GpioPrepareForStop();
}

void LedBar_Clear(void)
{
    if (s_ledbar_initialized == 0u)
    {
        LedBar_Init();
    }

    LedBar_StopChargeAnimation();
    s_ledbar_force_blank = 1u;
    LedBar_OutputOff();
    LedBar_GpioPrepareForStop();
}

void LedBar_SetSleep(uint8_t enable)
{
    if (s_ledbar_initialized == 0u)
    {
        LedBar_Init();
    }

    enable = (enable != 0u) ? 1u : 0u;
    if (enable != 0u)
    {
        LedBar_StopChargeAnimation();
    }
    if (s_ledbar_sleep == enable)
    {
        return;
    }

    s_ledbar_sleep = enable;
    if (s_ledbar_sleep != 0u)
    {
        LedBar_OutputOff();
        LedBar_GpioPrepareForStop();
    }
    else
    {
        LedBar_ApplyOutput();
    }
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

    s_ledbar_test_single_segment_enable = (enable != 0u) ? 1u : 0u;
    if (s_ledbar_test_single_segment_enable == 0u)
    {
        s_ledbar_test_single_segment_id = 0u;
    }
    LedBar_StopChargeAnimation();
    s_ledbar_force_blank = 0u;
    LedBar_ApplyOutput();
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
    s_ledbar_test_single_segment_id = segment_id;
    if (s_ledbar_test_single_segment_enable != 0u)
    {
        LedBar_ApplyOutput();
    }
}

void LedBar_SetNumber(uint8_t value)
{
    if (s_ledbar_initialized == 0u)
    {
        LedBar_Init();
    }

    if (value > 100u)
    {
        value = 100u;
    }

    LedBar_StopChargeAnimation();
    s_ledbar_number = value;
    s_ledbar_force_blank = 0u;
    LedBar_ApplyOutput();
}

void LedBar_SetIndicators(uint8_t indicator_mask)
{
    if (s_ledbar_initialized == 0u)
    {
        LedBar_Init();
    }

    LedBar_StopChargeAnimation();
    s_ledbar_indicator_mask = (uint8_t)(indicator_mask & (LEDBAR_ICON_CHARGE_MASK | LEDBAR_ICON_PERCENT_MASK));
    s_ledbar_force_blank = 0u;
    LedBar_ApplyOutput();
}

void LedBar_SetIndicatorState(uint8_t indicator_mask, uint8_t enable)
{
    if (enable != 0u)
    {
        LedBar_SetIndicators((uint8_t)(s_ledbar_indicator_mask | indicator_mask));
    }
    else
    {
        LedBar_SetIndicators((uint8_t)(s_ledbar_indicator_mask & (uint8_t)(~indicator_mask)));
    }
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
    s_ledbar_force_blank = 0u;
    LedBar_StopChargeAnimation();
    s_ledbar_number = LedBar_LoadSleepSoc();
    s_ledbar_indicator_mask = LEDBAR_ICON_PERCENT_MASK;
    LedBar_ApplyOutput();
}

void LedBar_PrepareForStop(void)
{
    if (s_ledbar_initialized == 0u)
    {
        LedBar_Init();
    }

    s_ledbar_sleep = 1u;
    s_ledbar_force_blank = 1u;
    LedBar_StopChargeAnimation();
    LedBar_OutputOff();
    LedBar_GpioPrepareForStop();
}

void LedBar_Scan1ms(void)
{
}


void APP_LedBar(void)
{
    uint8_t display_value;
    uint8_t charge_active;

    if (s_ledbar_initialized == 0u)
    {
        LedBar_Init();
    }

    LedBar_ServiceSwitch();

    if (SystemStatus.bits.b1StartUpBMS != 0u)
    {
        LedBar_Command = LED_BAR_STARTUP;
        if (LedBar_IsSocDisplayRequested() == 0u)
        {
            LedBar_SetSleep(1u);
            return;
        }
    }

    if (Sleep_Mode.bits.b1_ToSleepFlag != 0u)
    {
        LedBar_SaveSleepSoc();
        LedBar_SetSleep(1u);
        return;
    }

    if (s_ledbar_test_single_segment_enable != 0u)
    {
        LedBar_Command = LED_BAR_NORMAL;
        LedBar_SetSleep(0u);
        LedBar_ApplyOutput();
        return;
    }

    charge_active = LedBar_IsChargeActive();
    if (charge_active != 0u)
    {
        LedBar_RunChargeAnimation();
        return;
    }

    LedBar_StopChargeAnimation();

    if (LedBar_IsSocDisplayRequested() == 0u)
    {
        if (s_ledbar_force_blank == 0u)
        {
            LedBar_Clear();
        }
        return;
    }

    display_value = LedBar_GetRuntimeSoc();
    LedBar_SetSleep(0u);

    if (((g_stCellInfoReport.unMdlFault_Third.all & 0x3FFBu) != 0u) ||
        (System_ERROR_UserCallback(ERROR_STATUS_TEMP_BREAK) != 0u) ||
        (System_ERROR_UserCallback(ERROR_STATUS_CBC_DSG) != 0u))
    {
        LedBar_Command = LED_BAR_FAULT;
    }
    else if (charge_active != 0u)
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

    LedBar_SetNumber(display_value);
}
