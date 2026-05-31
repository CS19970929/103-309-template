#ifndef LED_SCAN_H
#define LED_SCAN_H

#include <stdint.h>

/* ── Board-level GPIO pin mapping ── */
#define LED_SCAN_PIN_COUNT 5u

#define LED_SCAN_GPIO_P1 GPIOB
#define LED_SCAN_PIN_P1  GPIO_Pin_11
#define LED_SCAN_GPIO_P2 GPIO_SPI1_NSS
#define LED_SCAN_PIN_P2  PIN_SPI1_NSS
#define LED_SCAN_GPIO_P3 GPIO_SPI1_SCK
#define LED_SCAN_PIN_P3  PIN_SPI1_SCK
#define LED_SCAN_GPIO_P4 GPIO_SPI_MOSI
#define LED_SCAN_PIN_P4  PIN_SPI_MOSI
#define LED_SCAN_GPIO_P5 GPIO_SEG_EN
#define LED_SCAN_PIN_P5  PIN_SEG_EN

/* ── Charlieplexing route table ──
   Each route is a (low_pin, high_pin) pair.
   The scan outputs each active route in round-robin order. */
typedef enum
{
    LED_SCAN_ROUTE_HUNDREDS_1_UPPER = 0,
    LED_SCAN_ROUTE_HUNDREDS_1_LOWER,
    LED_SCAN_ROUTE_TENS_A,
    LED_SCAN_ROUTE_TENS_B,
    LED_SCAN_ROUTE_TENS_C,
    LED_SCAN_ROUTE_TENS_D,
    LED_SCAN_ROUTE_TENS_E,
    LED_SCAN_ROUTE_TENS_F,
    LED_SCAN_ROUTE_TENS_G,
    LED_SCAN_ROUTE_ONES_A,
    LED_SCAN_ROUTE_ONES_B,
    LED_SCAN_ROUTE_ONES_C,
    LED_SCAN_ROUTE_ONES_D,
    LED_SCAN_ROUTE_ONES_E,
    LED_SCAN_ROUTE_ONES_F,
    LED_SCAN_ROUTE_ONES_G,
    LED_SCAN_ROUTE_ICON_CHARGE,
    LED_SCAN_ROUTE_ICON_PERCENT,
    LED_SCAN_ROUTE_COUNT
} LedScanRouteId;

typedef struct
{
    uint8_t low_pin;
    uint8_t high_pin;
} LedScanRoute;

/* ── Public API ── */

/* One-time GPIO and TIM4 init. Call before any other function. */
void LedScan_Init(void);

/* Output a single route. All other pins go Hi-Z first. */
void LedScan_OutputRoute(uint8_t route_id);

/* All pins to Hi-Z (display off). */
void LedScan_OutputOff(void);

/* Configure TIM4 for the scan period and start updates. */
void LedScan_StartTimer(void);

/* Stop TIM4 and disable its clock. */
void LedScan_StopTimer(void);

/* Drive all pins output LOW for low-power preparation. */
void LedScan_PrepareForStop(void);

#endif /* LED_SCAN_H */