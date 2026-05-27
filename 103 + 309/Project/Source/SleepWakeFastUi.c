#include "main.h"
#include "LedSnapshot.h"
#include "PowerUi.h"
#include "SleepWakeFastUi.h"

#define FAST_UI_TIMEOUT_TICKS       ((UINT16)800)
#define FAST_UI_CONFIRM_TICKS       DELAYB10MS_3S
#define FAST_UI_BLINK_TICKS         DELAYB10MS_500MS
#define FAST_UI_ANIM_STEP_MS        ((UINT16)200)

static UINT8 SleepWakeFastUi_IsChargeWake(void);
static UINT8 SleepWakeFastUi_IsSocKeyPressed(void);
static UINT8 SleepWakeFastUi_ServiceSocKey(void);
static void SleepWakeFastUi_BootAnimation(void);

UINT8 SleepWakeFastUi_ServiceAfterStop(void)
{
    if (SleepWakeFastUi_IsChargeWake()) {
        PowerUi_ConfirmPowerOn();
        return SLEEP_WAKE_FAST_CHARGE;
    }

    if (SleepWakeFastUi_IsSocKeyPressed()) {
        return SleepWakeFastUi_ServiceSocKey();
    }

    return SLEEP_WAKE_FAST_NONE;
}

static UINT8 SleepWakeFastUi_IsChargeWake(void)
{
    if (GPIO_ReadInputDataBit(GPIO_INT_WK_MCU, PIN_INT_WK_MCU)) {
        return 1;
    }
    return 0;
}

static UINT8 SleepWakeFastUi_IsSocKeyPressed(void)
{
    return (MCUI_SOC_KEY == 0) ? 1 : 0;
}

static UINT8 SleepWakeFastUi_ServiceSocKey(void)
{
    UINT8 soc;
    UINT8 flags;
    UINT8 power_on;
    UINT8 blink_on;
    UINT16 timeout_ticks;
    UINT16 hold_ticks;
    UINT16 blink_ticks;

    soc = 0;
    flags = LED_SNAPSHOT_FLAG_ALARM;
    power_on = 0;
    blink_on = 1;
    timeout_ticks = FAST_UI_TIMEOUT_TICKS;
    hold_ticks = 0;
    blink_ticks = 0;

    LedSnapshot_Load(&soc, &flags, &power_on);
    LedBar_FastInit();
    LedBar_ShowSocImmediate(soc, (UINT8)(flags & LED_SNAPSHOT_FLAG_ALARM), blink_on);

    while (timeout_ticks > 0) {
        if (SleepWakeFastUi_IsChargeWake()) {
            PowerUi_ConfirmPowerOn();
            return SLEEP_WAKE_FAST_CHARGE;
        }

        if (SleepWakeFastUi_IsSocKeyPressed()) {
            if (hold_ticks < FAST_UI_CONFIRM_TICKS) {
                ++hold_ticks;
            }
            if (hold_ticks >= FAST_UI_CONFIRM_TICKS) {
                SleepWakeFastUi_BootAnimation();
                PowerUi_ConfirmPowerOn();
                return SLEEP_WAKE_FAST_BOOT;
            }
        } else {
            hold_ticks = 0;
        }

        if (++blink_ticks >= FAST_UI_BLINK_TICKS) {
            blink_ticks = 0;
            blink_on = blink_on ? 0 : 1;
            LedBar_ShowSocImmediate(soc, (UINT8)(flags & LED_SNAPSHOT_FLAG_ALARM), blink_on);
        }

        __delay_ms(10);
        --timeout_ticks;
    }

    LedBar_OutputOff();
    return SLEEP_WAKE_FAST_TIMEOUT;
}

static void SleepWakeFastUi_BootAnimation(void)
{
    UINT8 step;

    for (step = 1; step <= 5; ++step) {
        LedBar_ShowBootAnimationStep(step);
        __delay_ms(FAST_UI_ANIM_STEP_MS);
    }
}
