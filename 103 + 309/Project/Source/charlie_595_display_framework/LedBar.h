#ifndef LEDBAR_H
#define LEDBAR_H

//#include "conf_gpio.h"

#define LEDBAR_PIN_COUNT 5U

typedef enum _LEDBAR_COMMAND {
    LED_BAR_STARTUP = 0,
    LED_BAR_NORMAL,
    LED_BAR_CHG,
    LED_BAR_DSG,
    LED_BAR_FAULT,
} LEDBAR_COMMAND;

typedef enum
{
    LEDBAR_SEG_1A = 0,
    LEDBAR_SEG_1B,
    LEDBAR_SEG_1C,
    LEDBAR_SEG_1D,
    LEDBAR_SEG_1E,
    LEDBAR_SEG_1F,
    LEDBAR_SEG_1G,
    LEDBAR_SEG_2A,
    LEDBAR_SEG_2B,
    LEDBAR_SEG_2C,
    LEDBAR_SEG_2D,
    LEDBAR_SEG_2E,
    LEDBAR_SEG_2F,
    LEDBAR_SEG_2G,
    LEDBAR_SEG_H1,
    LEDBAR_SEG_H2,
    LEDBAR_SEG_H3,
    LEDBAR_SEG_H4,
    LEDBAR_SEG_COUNT
} LedBarSegmentId;

#define LEDBAR_H1_MASK (1U << 0)
#define LEDBAR_H2_MASK (1U << 1)
#define LEDBAR_H3_MASK (1U << 2)
#define LEDBAR_H4_MASK (1U << 3)

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

void LedBar_Init(void);
void LedBar_Scan1ms(void);
void LedBar_SetNumber(UINT8 value);
void LedBar_SetIndicators(UINT8 indicator_mask);
void LedBar_SetIndicatorState(UINT8 indicator_mask, UINT8 enable);
void LedBar_Clear(void);
void APP_LedBar(void);

#endif
