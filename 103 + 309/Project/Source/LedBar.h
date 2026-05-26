#ifndef LEDBAR_H
#define LEDBAR_H

#include <stdint.h>
#include "Project_Config.h"

#define LEDBAR_PIN_COUNT 5u

#define LEDBAR_SLEEP_ENABLE PROJECT_CFG_LEDBAR_SLEEP_ENABLE

#define LEDBAR_LONG_PRESS_GPIO_TOGGLE_TEST PROJECT_CFG_LEDBAR_LONG_PRESS_GPIO_TOGGLE_TEST
#define LEDBAR_TEST_ALWAYS_ON PROJECT_CFG_LEDBAR_TEST_ALWAYS_ON

/* GPIO Charlieplexing pins. LedBar.c owns the physical scan order. */
#define LEDBAR_GPIO_P1 GPIOB
#define LEDBAR_PIN_P1  GPIO_Pin_11
#define LEDBAR_GPIO_P2 GPIO_SPI1_NSS
#define LEDBAR_PIN_P2  PIN_SPI1_NSS
#define LEDBAR_GPIO_P3 GPIO_SPI1_SCK
#define LEDBAR_PIN_P3  PIN_SPI1_SCK
#define LEDBAR_GPIO_P4 GPIO_SPI_MOSI
#define LEDBAR_PIN_P4  PIN_SPI_MOSI
#define LEDBAR_GPIO_P5 GPIO_SEG_EN
#define LEDBAR_PIN_P5  PIN_SEG_EN

#define LEDBAR_ICON_CHARGE_MASK  (1u << 0)
#define LEDBAR_ICON_PERCENT_MASK (1u << 1)
#define LEDBAR_SINGLE_SEG_ID_MIN 0u
#define LEDBAR_SINGLE_SEG_ID_MAX 17u
#define LEDBAR_SOC_DISPLAY_10MS PROJECT_CFG_LEDBAR_SOC_DISPLAY_10MS
#define LEDBAR_STARTUP_DISPLAY_10MS PROJECT_CFG_LEDBAR_WAKEUP_DISPLAY_10MS
#define LEDBAR_WAKEUP_DISPLAY_10MS PROJECT_CFG_LEDBAR_WAKEUP_DISPLAY_10MS

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
void LedBar_SaveSleepSoc(void);
uint8_t LedBar_LoadSleepSoc(void);
void LedBar_ShowSleepSocPreview(void);
void LedBar_PrepareForStop(void);
uint8_t LedBar_IsActiveForLowPower(void);
void APP_LedBar(void);

#endif
