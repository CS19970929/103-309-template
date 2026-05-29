#ifndef LEDBAR_H
#define LEDBAR_H

typedef enum _LEDBAR_COMMAND {
    LED_BAR_STARTUP = 0,
    LED_BAR_NORMAL,
    LED_BAR_CHG,
    LED_BAR_DSG,
    LED_BAR_SLEEP,
    LED_BAR_FAULT,
} LEDBAR_COMMAND;

typedef enum _LEDBAR_L1_COLOR {
    LEDBAR_L1_OFF = 0,
    LEDBAR_L1_GREEN,
    LEDBAR_L1_YELLOW,
} LEDBAR_L1_COLOR;

#define PORT_SOC_25             GPIOA
#define PIN_SOC_25              GPIO_Pin_6

#define PORT_SOC_Y               GPIOA
#define PIN_SOC_Y                GPIO_Pin_2

#define PORT_SOC_G               GPIOA
#define PIN_SOC_G                GPIO_Pin_3

#define PORT_SOC_50              GPIOA
#define PIN_SOC_50               GPIO_Pin_7

#define PORT_SOC_75              GPIOA
#define PIN_SOC_75               GPIO_Pin_5

#define PORT_SOC_100             GPIOB
#define PIN_SOC_100              GPIO_Pin_1

#define PORT_SOC_KEY             GPIOA
#define PIN_SOC_KEY              GPIO_Pin_9

#define MCUO_SOC_25              (PAout(6))
#define MCUO_SOC_Y               (PAout(2))
#define MCUO_SOC_G               (PAout(3))
#define MCUO_SOC_50              (PAout(7))
#define MCUO_SOC_75              (PAout(5))
#define MCUO_SOC_100             (PBout(1))
#define MCUI_SOC_KEY             (PAin(9))

extern LEDBAR_COMMAND LedBar_Command;

#ifndef LED_UI_TICK_MS
#define LED_UI_TICK_MS                 ((UINT16)10)
#endif
#ifndef LED_BOOT_CONFIRM_HOLD_MS
#define LED_BOOT_CONFIRM_HOLD_MS       ((UINT16)1000)
#endif
#ifndef LED_SHUTDOWN_CONFIRM_HOLD_MS
#define LED_SHUTDOWN_CONFIRM_HOLD_MS   ((UINT16)1000)
#endif
#ifndef LED_BOOT_PREVIEW_TIMEOUT_MS
#define LED_BOOT_PREVIEW_TIMEOUT_MS    ((UINT16)8000)
#endif
#ifndef LED_SHUTDOWN_CONFIRM_TIMEOUT_MS
#define LED_SHUTDOWN_CONFIRM_TIMEOUT_MS ((UINT16)8000)
#endif
#ifndef LED_ANIM_STEP_MS
#define LED_ANIM_STEP_MS               ((UINT16)200)
#endif
#ifndef LED_BLINK_PERIOD_MS
#define LED_BLINK_PERIOD_MS            ((UINT16)500)
#endif

#define LED_MS_TO_TICKS(ms)            ((UINT16)(((UINT32)(ms) + ((UINT32)LED_UI_TICK_MS - 1U)) / (UINT32)LED_UI_TICK_MS))

void LedBar_gpio_Init(void);
void LedBar_Init(void);
void LedBar_FastInit(void);
void LedBar_OutputOff(void);
void LedBar_ShowSocImmediate(UINT8 soc, UINT8 alarm, UINT8 blink_on);
void LedBar_ShowBootAnimationStep(UINT8 step);
void LedBar_ShowShutdownConfirmFrame(UINT8 blink_on);
void LedBar_ShowShutdownAnimationStep(UINT8 step);
void APP_LedBar(void);

#endif
