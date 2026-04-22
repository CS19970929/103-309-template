#ifndef LEDBAR_H
#define LEDBAR_H

#include <stdint.h>

#define LEDBAR_PIN_COUNT 5u

typedef enum _LEDBAR_COMMAND
{
    LED_BAR_STARTUP = 0,
    LED_BAR_NORMAL,
    LED_BAR_CHG,
    LED_BAR_DSG,
    LED_BAR_FAULT,
} LEDBAR_COMMAND;

#define LEDBAR_ICON_CHARGE_MASK  (1u << 0)
#define LEDBAR_ICON_PERCENT_MASK (1u << 1)
#define LEDBAR_SINGLE_SEG_ID_MIN 0u
#define LEDBAR_SINGLE_SEG_ID_MAX 17u

void LedBar_Init(void);
void LedBar_Scan1ms(void);
void LedBar_SetNumber(uint8_t value);
void LedBar_SetIndicators(uint8_t indicator_mask);
void LedBar_SetIndicatorState(uint8_t indicator_mask, uint8_t enable);
void LedBar_Clear(void);
void LedBar_SetSleep(uint8_t enable);
void LedBar_Wakeup(void);
void LedBar_EnableSingleSegmentTest(uint8_t enable);
void LedBar_SetSingleSegmentIndex(uint8_t segment_id);
void APP_LedBar(void);

#endif
