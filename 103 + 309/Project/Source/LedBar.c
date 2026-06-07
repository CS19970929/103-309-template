#include "main.h"
#include "DebugWatch.h"

#define LEDBAR_STARTUP_DISPLAY_10MS 1000u
#define LEDBAR_SLEEP_SOC_MAGIC 0x5A00u
#define LEDBAR_SLEEP_SOC_MAGIC_MASK 0xFF00u
#define LEDBAR_SLEEP_SOC_VALUE_MASK 0x00FFu
#define LEDBAR_SLEEP_SOC_REG BKP_DR4
#define LEDBAR_SLEEP_SOC_INV_REG BKP_DR5

#define LEDBAR_LED_ON Bit_SET
#define LEDBAR_LED_OFF Bit_RESET

#define LEDBAR_ICON_CHARGE_MASK  (1u << 0)
#define LEDBAR_ICON_PERCENT_MASK (1u << 1)

typedef struct
{
    GPIO_TypeDef *port;
    uint16_t pin;
    uint8_t on_threshold;
} LedBarLed;

typedef struct LEDBAR_RUNTIME_TAG
{
    uint8_t initialized;
    uint8_t sleep;
    uint8_t blank;
    uint8_t number;
    uint8_t indicator_mask;
    uint16_t soc_display_10ms;
    uint8_t startup_display_armed;
    uint8_t key_active;
    uint8_t main_sw_closed;
    uint8_t main_sw_sleep_handled;
} LedBarRuntime;

static const LedBarLed s_ledbar_leds[LEDBAR_SOC_LED_COUNT] =
{
    {GPIO_SOC_LED_25, PIN_SOC_LED_25, 1u},
    {GPIO_SOC_LED_50, PIN_SOC_LED_50, 26u},
    {GPIO_SOC_LED_75, PIN_SOC_LED_75, 51u},
    {GPIO_SOC_LED_100, PIN_SOC_LED_100, 76u},
};

static LedBarRuntime s_ledbar;

extern void low_power_log_and_commit_sleep(uint8_t sleep_mode);

#if DEBUG_WATCH_ENABLED
void LedBar_DebugWatchBind(DEBUG_WATCH_ROOT *watch)
{
    watch->runtime.ledbar = &s_ledbar;
}
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

static uint16_t LedBar_GetAllLedPins(void)
{
    return (uint16_t)(PIN_SOC_LED_25 |
                      PIN_SOC_LED_50 |
                      PIN_SOC_LED_75 |
                      PIN_SOC_LED_100);
}

static void LedBar_ConfigureKeyInput(void)
{
    GPIO_InitTypeDef gpio_init;

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA | RCC_APB2Periph_AFIO, ENABLE);
    gpio_init.GPIO_Mode = GPIO_Mode_IPU;
    gpio_init.GPIO_Speed = GPIO_Speed_2MHz;

    gpio_init.GPIO_Pin = PIN_SOC_KEY;
    GPIO_Init(GPIO_SOC_KEY, &gpio_init);

    gpio_init.GPIO_Pin = PIN_MAIN_SW;
    GPIO_Init(GPIO_MAIN_SW, &gpio_init);
}

static void LedBar_ConfigureLedsOutput(void)
{
    GPIO_InitTypeDef gpio_init;
    uint16_t pins = LedBar_GetAllLedPins();

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE);
    GPIO_ResetBits(GPIOA, pins);
    gpio_init.GPIO_Pin = pins;
    gpio_init.GPIO_Mode = GPIO_Mode_Out_PP;
    gpio_init.GPIO_Speed = GPIO_Speed_2MHz;
    GPIO_Init(GPIOA, &gpio_init);
}

static void LedBar_ConfigureLedsAnalog(void)
{
    GPIO_InitTypeDef gpio_init;
    uint16_t pins = LedBar_GetAllLedPins();

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE);
    GPIO_ResetBits(GPIOA, pins);
    gpio_init.GPIO_Pin = pins;
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

static void LedBar_RefreshOutput(void)
{
    if ((s_ledbar.sleep != 0u) || (s_ledbar.blank != 0u))
    {
        LedBar_OutputOff();
        return;
    }

    LedBar_ConfigureLedsOutput();
    LedBar_OutputSoc(s_ledbar.number);
}

static uint8_t LedBar_ReadSocKeyPressed(void)
{
    return (uint8_t)(GPIO_ReadInputDataBit(GPIO_SOC_KEY, PIN_SOC_KEY) == Bit_RESET);
}

static uint8_t LedBar_ReadMainSwitchClosed(void)
{
    return (uint8_t)(GPIO_ReadInputDataBit(GPIO_MAIN_SW, PIN_MAIN_SW) == Bit_RESET);
}

static uint8_t LedBar_IsChargeActive(void)
{
    return MosStartup_Is5vChargeActive();
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

static void LedBar_ServiceMainSwitch(void)
{
    uint8_t closed = LedBar_ReadMainSwitchClosed();

    s_ledbar.main_sw_closed = closed;
    if (closed != 0u)
    {
        s_ledbar.main_sw_sleep_handled = 0u;
        return;
    }

    if (LedBar_IsChargeActive() != 0u)
    {
        s_ledbar.main_sw_sleep_handled = 0u;
        return;
    }

    if (s_ledbar.main_sw_sleep_handled != 0u)
    {
        return;
    }

    s_ledbar.main_sw_sleep_handled = 1u;
    LedBar_SaveSleepSoc();
    LedBar_SetSleep(1u);
    low_power_log_and_commit_sleep(DEEP_MODE);
}

static void LedBar_ServiceSocKey(void)
{
    uint8_t pressed;
    uint8_t was_pressed;

    if (g_st_SysTimeFlag.bits.b1Sys10msFlag == 0u)
    {
        return;
    }

    pressed = LedBar_ReadSocKeyPressed();
    was_pressed = s_ledbar.key_active;
    s_ledbar.key_active = pressed;

    if ((was_pressed == 0u) && (pressed != 0u))
    {
        LedBar_RequestSocDisplayWindow();
    }

    if (s_ledbar.soc_display_10ms != 0u)
    {
        s_ledbar.soc_display_10ms--;
    }
}

static uint8_t LedBar_IsDisplayRequested(void)
{
    return (uint8_t)(s_ledbar.soc_display_10ms != 0u);
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
    s_ledbar.soc_display_10ms = 0u;
    s_ledbar.startup_display_armed = 0u;
    s_ledbar.main_sw_sleep_handled = 0u;
    LedBar_GpioInitForDisplay();
    s_ledbar.key_active = LedBar_ReadSocKeyPressed();
    s_ledbar.main_sw_closed = LedBar_ReadMainSwitchClosed();
    LedBar_OutputOff();
    LedBar_GpioPrepareForStop();
    s_ledbar.initialized = 1u;
}

static void LedBar_Clear(void)
{
    LedBar_EnsureInit();

    s_ledbar.blank = 1u;
    LedBar_RefreshOutput();
    LedBar_GpioPrepareForStop();
}

void LedBar_SetSleep(uint8_t enable)
{
    LedBar_EnsureInit();

    enable = (enable != 0u) ? 1u : 0u;
    if (s_ledbar.sleep == enable)
    {
        return;
    }

    s_ledbar.sleep = enable;
    if (enable != 0u)
    {
        s_ledbar.blank = 1u;
        LedBar_OutputOff();
        LedBar_GpioPrepareForStop();
        return;
    }

    LedBar_RefreshOutput();
}

void LedBar_SaveSleepSoc(void)
{
    uint16_t value = (uint16_t)(LEDBAR_SLEEP_SOC_MAGIC | LedBar_GetRuntimeSoc());

    LedBar_EnableBackupAccess();
    BKP_WriteBackupRegister(LEDBAR_SLEEP_SOC_REG, value);
    BKP_WriteBackupRegister(LEDBAR_SLEEP_SOC_INV_REG, (uint16_t)(~value));
}

static uint8_t LedBar_LoadSleepSoc(void)
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

    s_ledbar.sleep = 1u;
    s_ledbar.blank = 1u;
    LedBar_OutputOff();
    LedBar_GpioPrepareForStop();
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

    return (uint8_t)((s_ledbar.soc_display_10ms != 0u) && (s_ledbar.blank == 0u));
}

void APP_LedBar(void)
{
    uint8_t display_value;

    LedBar_EnsureInit();
    LedBar_ServiceMainSwitch();
    if ((s_ledbar.main_sw_closed == 0u) &&
        (s_ledbar.main_sw_sleep_handled != 0u))
    {
        return;
    }
    LedBar_ServiceSocKey();
    LedBar_ServiceStartupDisplayWindow();

    if (LedBar_IsDisplayRequested() == 0u)
    {
        if (s_ledbar.blank == 0u)
        {
            LedBar_Clear();
        }
        return;
    }

    if (s_ledbar.sleep != 0u)
    {
        LedBar_SetSleep(0u);
    }

    display_value = LedBar_GetRuntimeSoc();
    if ((s_ledbar.number != display_value) || (s_ledbar.blank != 0u))
    {
        s_ledbar.number = display_value;
        s_ledbar.indicator_mask = LEDBAR_ICON_PERCENT_MASK;
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
    if (frame_len)    *frame_len    = (s_ledbar.blank == 0u) ? LEDBAR_SOC_LED_COUNT : 0u;
    if (scan_idx)     *scan_idx     = 0u;
    if (key_active)   *key_active   = s_ledbar.key_active;
    if (charge_icon)  *charge_icon  = 0u;
    if (percent_icon) *percent_icon = (s_ledbar.indicator_mask & LEDBAR_ICON_PERCENT_MASK) ? 1u : 0u;
}
#endif
