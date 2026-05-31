#ifndef LEDBAR_H
#define LEDBAR_H

#include <stdint.h>
#include "Project_Config.h"
#include "led_scan.h"

/* ── Feature switches ── */
#define LEDBAR_SLEEP_ENABLE  PROJECT_CFG_LEDBAR_SLEEP_ENABLE
#define LEDBAR_TEST_ALWAYS_ON PROJECT_CFG_LEDBAR_TEST_ALWAYS_ON

/* ── Icon bit masks ── */
#define LEDBAR_ICON_CHARGE_MASK  (1u << 0)
#define LEDBAR_ICON_PERCENT_MASK (1u << 1)

/* ── Display timing (10 ms ticks) ── */
#define LEDBAR_SOC_DISPLAY_10MS     PROJECT_CFG_LEDBAR_SOC_DISPLAY_10MS
#define LEDBAR_STARTUP_DISPLAY_10MS PROJECT_CFG_LEDBAR_WAKEUP_DISPLAY_10MS

/* ── Backward-compat aliases for existing callers ── */
#define LEDBAR_PIN_COUNT     LED_SCAN_PIN_COUNT
#define LEDBAR_GPIO_P1       LED_SCAN_GPIO_P1
#define LEDBAR_PIN_P1        LED_SCAN_PIN_P1
#define LEDBAR_GPIO_P2       LED_SCAN_GPIO_P2
#define LEDBAR_PIN_P2        LED_SCAN_PIN_P2
#define LEDBAR_GPIO_P3       LED_SCAN_GPIO_P3
#define LEDBAR_PIN_P3        LED_SCAN_PIN_P3
#define LEDBAR_GPIO_P4       LED_SCAN_GPIO_P4
#define LEDBAR_PIN_P4        LED_SCAN_PIN_P4
#define LEDBAR_GPIO_P5       LED_SCAN_GPIO_P5
#define LEDBAR_PIN_P5        LED_SCAN_PIN_P5

#define LEDBAR_ROUTE_COUNT   LED_SCAN_ROUTE_COUNT
#define LEDBAR_ROUTE_HUNDREDS_1_UPPER LED_SCAN_ROUTE_HUNDREDS_1_UPPER
#define LEDBAR_ROUTE_HUNDREDS_1_LOWER LED_SCAN_ROUTE_HUNDREDS_1_LOWER
#define LEDBAR_ROUTE_TENS_A  LED_SCAN_ROUTE_TENS_A
#define LEDBAR_ROUTE_TENS_B  LED_SCAN_ROUTE_TENS_B
#define LEDBAR_ROUTE_TENS_C  LED_SCAN_ROUTE_TENS_C
#define LEDBAR_ROUTE_TENS_D  LED_SCAN_ROUTE_TENS_D
#define LEDBAR_ROUTE_TENS_E  LED_SCAN_ROUTE_TENS_E
#define LEDBAR_ROUTE_TENS_F  LED_SCAN_ROUTE_TENS_F
#define LEDBAR_ROUTE_TENS_G  LED_SCAN_ROUTE_TENS_G
#define LEDBAR_ROUTE_ONES_A  LED_SCAN_ROUTE_ONES_A
#define LEDBAR_ROUTE_ONES_B  LED_SCAN_ROUTE_ONES_B
#define LEDBAR_ROUTE_ONES_C  LED_SCAN_ROUTE_ONES_C
#define LEDBAR_ROUTE_ONES_D  LED_SCAN_ROUTE_ONES_D
#define LEDBAR_ROUTE_ONES_E  LED_SCAN_ROUTE_ONES_E
#define LEDBAR_ROUTE_ONES_F  LED_SCAN_ROUTE_ONES_F
#define LEDBAR_ROUTE_ONES_G  LED_SCAN_ROUTE_ONES_G
#define LEDBAR_ROUTE_ICON_CHARGE  LED_SCAN_ROUTE_ICON_CHARGE
#define LEDBAR_ROUTE_ICON_PERCENT LED_SCAN_ROUTE_ICON_PERCENT

/* ── Debug watch pointer ── */
#if PROJECT_CFG_DEBUG_WATCH_ENABLE
struct LedBarRuntime;  /* forward decl */
extern struct LedBarRuntime * const g_dbg_ledbar_runtime;
#endif

/* ── Public API ── */
void     LedBar_Init(void);
void     LedBar_SetNumber(uint8_t value);
void     LedBar_SetIndicators(uint8_t indicator_mask);
void     LedBar_SetIndicatorState(uint8_t indicator_mask, uint8_t enable);
void     LedBar_Clear(void);
void     LedBar_SetSleep(uint8_t enable);
void     LedBar_Wakeup(void);
void     LedBar_SaveSleepSoc(void);
uint8_t  LedBar_LoadSleepSoc(void);
void     LedBar_ShowSleepSocPreview(void);
void     LedBar_RequestSocDisplay(void);
void     LedBar_PrepareForStop(void);
uint8_t  LedBar_IsActiveForLowPower(void);
void     APP_LedBar(void);

#endif /* LEDBAR_H */