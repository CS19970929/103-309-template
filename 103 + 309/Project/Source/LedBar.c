#include "main.h"
#include "PowerUi.h"

#define LED_BOOT_CONFIRM_TICKS       LED_MS_TO_TICKS(LED_BOOT_CONFIRM_HOLD_MS)
#define LED_SHUTDOWN_CONFIRM_TICKS   LED_MS_TO_TICKS(LED_SHUTDOWN_CONFIRM_HOLD_MS)
#define LED_BOOT_PREVIEW_TICKS       LED_MS_TO_TICKS(LED_BOOT_PREVIEW_TIMEOUT_MS)
#define LED_SHUTDOWN_CONFIRM_TIMEOUT_TICKS LED_MS_TO_TICKS(LED_SHUTDOWN_CONFIRM_TIMEOUT_MS)
#define LED_UI_BLINK_TICKS           LED_MS_TO_TICKS(LED_BLINK_PERIOD_MS)
#define LED_UI_ANIM_STEP_TICKS       LED_MS_TO_TICKS(LED_ANIM_STEP_MS)

typedef enum _LED_UI_STATE {
    LED_UI_OFF_IDLE = 0,
    LED_UI_BOOT_PREVIEW,
    LED_UI_BOOT_ANIM,
    LED_UI_WORK,
    LED_UI_CHARGE,
    LED_UI_SHUTDOWN_CONFIRM,
    LED_UI_SHUTDOWN_ANIM,
} LED_UI_STATE;

typedef struct _LED_UI_RUNTIME {
    LED_UI_STATE state;
    UINT8 key_last;
    UINT8 blink_on;
    UINT8 anim_step;
    UINT16 hold_ticks;
    UINT16 timeout_ticks;
    UINT16 blink_ticks;
    UINT16 anim_ticks;
} LED_UI_RUNTIME;

LEDBAR_COMMAND LedBar_Command = LED_BAR_NORMAL;
static LED_UI_RUNTIME s_led_ui;

static void LedBar_GpioInitForDisplay(void);
static void LedBar_OutputFrame(LEDBAR_L1_COLOR l1, UINT8 l2, UINT8 l3, UINT8 l4, UINT8 l5);
static UINT8 LedBar_KeyPressed(void);
static UINT8 LedBar_KeyDownEdge(UINT8 pressed);
static UINT8 LedBar_IsChargeActive(void);
static UINT8 LedBar_IsAlarmActive(UINT8 soc);
static UINT8 LedBar_WorkLevel(UINT8 soc);
static UINT8 LedBar_ChargeStableLevel(UINT8 soc);
static void LedBar_ServiceBlink(void);
static void LedBar_RenderSoc(UINT8 soc, UINT8 alarm);
static void LedBar_RenderWork(void);
static void LedBar_RenderCharge(void);
static void LedBar_EnterState(LED_UI_STATE state);
static void LedBar_ServiceOffIdle(UINT8 key_down);
static void LedBar_ServiceBootPreview(UINT8 pressed);
static void LedBar_ServiceBootAnim(void);
static void LedBar_ServiceWork(UINT8 key_down);
static void LedBar_ServiceShutdownConfirm(UINT8 pressed);
static void LedBar_ServiceShutdownAnim(void);

void LedBar_gpio_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE);
    GPIO_InitStructure.GPIO_Pin = PIN_SOC_KEY;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPU;
    GPIO_Init(PORT_SOC_KEY, &GPIO_InitStructure);
}

void LedBar_FastInit(void)
{
    LedBar_GpioInitForDisplay();
}

void LedBar_Init(void)
{
    LedBar_GpioInitForDisplay();
    memset(&s_led_ui, 0, sizeof(s_led_ui));
    s_led_ui.blink_on = 1;
    s_led_ui.state = PowerUi_IsPowerOnConfirmed() ? LED_UI_WORK : LED_UI_OFF_IDLE;

    if (s_led_ui.state == LED_UI_WORK) {
        LedBar_RenderWork();
    } else {
        LedBar_OutputOff();
    }
}

void APP_LedBar(void)
{
    UINT8 pressed;
    UINT8 key_down;
    UINT8 charge_active;

    LedBar_ServiceBlink();

    pressed = LedBar_KeyPressed();
    key_down = LedBar_KeyDownEdge(pressed);
    charge_active = LedBar_IsChargeActive();

    if (charge_active) {
        if (!PowerUi_IsPowerOnConfirmed()) {
            PowerUi_ConfirmPowerOn();
        }
        if (s_led_ui.state != LED_UI_CHARGE) {
            LedBar_EnterState(LED_UI_CHARGE);
        }
    } else if (s_led_ui.state == LED_UI_CHARGE) {
        LedBar_EnterState(PowerUi_IsPowerOnConfirmed() ? LED_UI_WORK : LED_UI_OFF_IDLE);
    }

    switch (s_led_ui.state) {
    case LED_UI_OFF_IDLE:
        LedBar_ServiceOffIdle(key_down);
        break;
    case LED_UI_BOOT_PREVIEW:
        LedBar_ServiceBootPreview(pressed);
        break;
    case LED_UI_BOOT_ANIM:
        LedBar_ServiceBootAnim();
        break;
    case LED_UI_WORK:
        LedBar_ServiceWork(key_down);
        break;
    case LED_UI_CHARGE:
        LedBar_RenderCharge();
        break;
    case LED_UI_SHUTDOWN_CONFIRM:
        LedBar_ServiceShutdownConfirm(pressed);
        break;
    case LED_UI_SHUTDOWN_ANIM:
        LedBar_ServiceShutdownAnim();
        break;
    default:
        LedBar_EnterState(PowerUi_IsPowerOnConfirmed() ? LED_UI_WORK : LED_UI_OFF_IDLE);
        break;
    }
}

void LedBar_OutputOff(void)
{
    LedBar_OutputFrame(LEDBAR_L1_OFF, 0, 0, 0, 0);
}

void LedBar_ShowSocImmediate(UINT8 soc, UINT8 alarm, UINT8 blink_on)
{
    if (alarm || soc < 20) {
        LedBar_OutputFrame(blink_on ? LEDBAR_L1_YELLOW : LEDBAR_L1_OFF, 0, 0, 0, 0);
    } else {
        UINT8 level;
        level = LedBar_WorkLevel(soc);
        LedBar_OutputFrame(LEDBAR_L1_GREEN, level >= 2, level >= 3, level >= 4, level >= 5);
    }
}

void LedBar_ShowBootAnimationStep(UINT8 step)
{
    if (step > 5) {
        step = 5;
    }
    LedBar_OutputFrame(step >= 5 ? LEDBAR_L1_GREEN : LEDBAR_L1_OFF,
                       step >= 1,
                       step >= 2,
                       step >= 3,
                       step >= 4);
}

void LedBar_ShowShutdownConfirmFrame(UINT8 blink_on)
{
    LedBar_OutputFrame(LEDBAR_L1_OFF, blink_on, blink_on, blink_on, blink_on);
}

void LedBar_ShowShutdownAnimationStep(UINT8 step)
{
    if (step > 4) {
        step = 4;
    }
    LedBar_OutputFrame(LEDBAR_L1_OFF,
                       step < 4,
                       step < 3,
                       step < 2,
                       step < 1);
}

static void LedBar_GpioInitForDisplay(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA | RCC_APB2Periph_GPIOB, ENABLE);

    GPIO_InitStructure.GPIO_Pin = PIN_SOC_25 | PIN_SOC_Y | PIN_SOC_G | PIN_SOC_50 | PIN_SOC_75;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_2MHz;
    GPIO_Init(PORT_SOC_25, &GPIO_InitStructure);

    GPIO_InitStructure.GPIO_Pin = PIN_SOC_100;
    GPIO_Init(PORT_SOC_100, &GPIO_InitStructure);

    GPIO_InitStructure.GPIO_Pin = PIN_SOC_KEY;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPU;
    GPIO_Init(PORT_SOC_KEY, &GPIO_InitStructure);
}

static void LedBar_OutputFrame(LEDBAR_L1_COLOR l1, UINT8 l2, UINT8 l3, UINT8 l4, UINT8 l5)
{
    MCUO_SOC_Y = (l1 == LEDBAR_L1_YELLOW) ? 1 : 0;
    MCUO_SOC_G = (l1 == LEDBAR_L1_GREEN) ? 1 : 0;
    MCUO_SOC_25 = l2 ? 1 : 0;
    MCUO_SOC_50 = l3 ? 1 : 0;
    MCUO_SOC_75 = l4 ? 1 : 0;
    MCUO_SOC_100 = l5 ? 1 : 0;
}

static UINT8 LedBar_KeyPressed(void)
{
    return (MCUI_SOC_KEY == 0) ? 1 : 0;
}

static UINT8 LedBar_KeyDownEdge(UINT8 pressed)
{
    UINT8 edge;

    edge = (pressed && !s_led_ui.key_last) ? 1 : 0;
    s_led_ui.key_last = pressed;
    return edge;
}

static UINT8 LedBar_IsChargeActive(void)
{
    if (g_stCellInfoReport.u16Ichg > 0) {
        return 1;
    }

    if (GPIO_ReadInputDataBit(GPIO_INT_WK_MCU, PIN_INT_WK_MCU)) {
        return 1;
    }

    return 0;
}

static UINT8 LedBar_IsAlarmActive(UINT8 soc)
{
    if (soc < 20) {
        return 1;
    }
    if (g_stCellInfoReport.unMdlFault_Third.bits.b1CellUvp) {
        return 1;
    }
    if (g_stCellInfoReport.unMdlFault_Third.bits.b1BatUvp) {
        return 1;
    }
    if (g_stCellInfoReport.unMdlFault_Third.bits.b1SocLow) {
        return 1;
    }
    return 0;
}

static UINT8 LedBar_WorkLevel(UINT8 soc)
{
    if (soc >= 80) {
        return 5;
    }
    if (soc >= 60) {
        return 4;
    }
    if (soc >= 40) {
        return 3;
    }
    if (soc >= 20) {
        return 2;
    }
    return 0;
}

static UINT8 LedBar_ChargeStableLevel(UINT8 soc)
{
    if (soc >= 100) {
        return 5;
    }
    if (soc > 80) {
        return 4;
    }
    if (soc > 60) {
        return 3;
    }
    if (soc > 40) {
        return 2;
    }
    if (soc > 20) {
        return 1;
    }
    return 0;
}

static void LedBar_ServiceBlink(void)
{
    if (++s_led_ui.blink_ticks >= LED_UI_BLINK_TICKS) {
        s_led_ui.blink_ticks = 0;
        s_led_ui.blink_on = s_led_ui.blink_on ? 0 : 1;
    }
}

static void LedBar_RenderSoc(UINT8 soc, UINT8 alarm)
{
    LedBar_ShowSocImmediate(soc, alarm, s_led_ui.blink_on);
}

static void LedBar_RenderWork(void)
{
    UINT8 soc;
    UINT8 alarm;

    soc = (UINT8)g_stCellInfoReport.SocElement.u16Soc;
    alarm = LedBar_IsAlarmActive(soc);
    LedBar_RenderSoc(soc, alarm);
}

static void LedBar_RenderCharge(void)
{
    UINT8 soc;
    UINT8 level;
    UINT8 blink_index;
    UINT8 l1;
    UINT8 l2;
    UINT8 l3;
    UINT8 l4;
    UINT8 l5;

    soc = (UINT8)g_stCellInfoReport.SocElement.u16Soc;
    level = LedBar_ChargeStableLevel(soc);
    blink_index = (level < 5) ? (UINT8)(level + 1) : 0;

    l1 = (level >= 1) ? 1 : 0;
    l2 = (level >= 2) ? 1 : 0;
    l3 = (level >= 3) ? 1 : 0;
    l4 = (level >= 4) ? 1 : 0;
    l5 = (level >= 5) ? 1 : 0;

    if (blink_index == 1) {
        l1 = s_led_ui.blink_on;
    } else if (blink_index == 2) {
        l2 = s_led_ui.blink_on;
    } else if (blink_index == 3) {
        l3 = s_led_ui.blink_on;
    } else if (blink_index == 4) {
        l4 = s_led_ui.blink_on;
    } else if (blink_index == 5) {
        l5 = s_led_ui.blink_on;
    }

    LedBar_OutputFrame(l1 ? LEDBAR_L1_GREEN : LEDBAR_L1_OFF, l2, l3, l4, l5);
}

static void LedBar_EnterState(LED_UI_STATE state)
{
    s_led_ui.state = state;
    s_led_ui.hold_ticks = 0;
    s_led_ui.anim_ticks = 0;
    s_led_ui.anim_step = 0;

    if (state == LED_UI_OFF_IDLE) {
        LedBar_OutputOff();
    } else if (state == LED_UI_BOOT_PREVIEW) {
        s_led_ui.timeout_ticks = LED_BOOT_PREVIEW_TICKS;
        LedBar_RenderWork();
    } else if (state == LED_UI_BOOT_ANIM) {
        s_led_ui.anim_step = 1;
        LedBar_ShowBootAnimationStep(s_led_ui.anim_step);
    } else if (state == LED_UI_WORK) {
        LedBar_RenderWork();
    } else if (state == LED_UI_CHARGE) {
        LedBar_RenderCharge();
    } else if (state == LED_UI_SHUTDOWN_CONFIRM) {
        s_led_ui.timeout_ticks = LED_SHUTDOWN_CONFIRM_TIMEOUT_TICKS;
        LedBar_ShowShutdownConfirmFrame(s_led_ui.blink_on);
    } else if (state == LED_UI_SHUTDOWN_ANIM) {
        s_led_ui.anim_step = 0;
        LedBar_ShowShutdownAnimationStep(s_led_ui.anim_step);
    }
}

static void LedBar_ServiceOffIdle(UINT8 key_down)
{
    if (key_down) {
        LedBar_EnterState(LED_UI_BOOT_PREVIEW);
    } else {
        LedBar_OutputOff();
    }
}

static void LedBar_ServiceBootPreview(UINT8 pressed)
{
    LedBar_RenderWork();

    if (pressed) {
        if (s_led_ui.hold_ticks < LED_BOOT_CONFIRM_TICKS) {
            ++s_led_ui.hold_ticks;
        }
        if (s_led_ui.hold_ticks >= LED_BOOT_CONFIRM_TICKS) {
            LedBar_EnterState(LED_UI_BOOT_ANIM);
            return;
        }
    } else {
        s_led_ui.hold_ticks = 0;
    }

    if (s_led_ui.timeout_ticks > 0) {
        --s_led_ui.timeout_ticks;
    }
    if (s_led_ui.timeout_ticks == 0) {
        LedBar_EnterState(LED_UI_OFF_IDLE);
    }
}

static void LedBar_ServiceBootAnim(void)
{
    if (++s_led_ui.anim_ticks < LED_UI_ANIM_STEP_TICKS) {
        return;
    }

    s_led_ui.anim_ticks = 0;
    if (s_led_ui.anim_step < 5) {
        ++s_led_ui.anim_step;
        LedBar_ShowBootAnimationStep(s_led_ui.anim_step);
    } else {
        PowerUi_ConfirmPowerOn();
        LedBar_EnterState(LED_UI_WORK);
    }
}

static void LedBar_ServiceWork(UINT8 key_down)
{
    if (!PowerUi_IsPowerOnConfirmed()) {
        LedBar_EnterState(LED_UI_OFF_IDLE);
        return;
    }

    if (key_down) {
        LedBar_EnterState(LED_UI_SHUTDOWN_CONFIRM);
    } else {
        LedBar_RenderWork();
    }
}

static void LedBar_ServiceShutdownConfirm(UINT8 pressed)
{
    LedBar_ShowShutdownConfirmFrame(s_led_ui.blink_on);

    if (pressed) {
        if (s_led_ui.hold_ticks < LED_SHUTDOWN_CONFIRM_TICKS) {
            ++s_led_ui.hold_ticks;
        }
        if (s_led_ui.hold_ticks >= LED_SHUTDOWN_CONFIRM_TICKS) {
            LedBar_EnterState(LED_UI_SHUTDOWN_ANIM);
            return;
        }
    } else {
        s_led_ui.hold_ticks = 0;
    }

    if (s_led_ui.timeout_ticks > 0) {
        --s_led_ui.timeout_ticks;
    }
    if (s_led_ui.timeout_ticks == 0) {
        LedBar_EnterState(LED_UI_WORK);
    }
}

static void LedBar_ServiceShutdownAnim(void)
{
    if (++s_led_ui.anim_ticks < LED_UI_ANIM_STEP_TICKS) {
        return;
    }

    s_led_ui.anim_ticks = 0;
    if (s_led_ui.anim_step < 4) {
        ++s_led_ui.anim_step;
        LedBar_ShowShutdownAnimationStep(s_led_ui.anim_step);
    } else {
        LedBar_OutputOff();
        PowerUi_RequestShutdown();
        LedBar_EnterState(LED_UI_OFF_IDLE);
    }
}
