#ifndef DISPLAY_TYPES_H
#define DISPLAY_TYPES_H

#include <stdint.h>

typedef enum
{
    DISP595_SEL_NONE = 0,
    DISP595_SEL_DIGIT_HUNDREDS = 1, /* 百分位“1” */
    DISP595_SEL_DIGIT_TENS     = 2, /* 十分位 */
    DISP595_SEL_DIGIT_ONES     = 3, /* 个位 */
    DISP595_SEL_ICON_CHARGE    = 4, /* 充电图标 */
    DISP595_SEL_ICON_PERCENT   = 5  /* 百分号图标 */
} display_sel_t;

typedef enum
{
    CHARLIE_PIN_0 = 0,
    CHARLIE_PIN_1,
    CHARLIE_PIN_2,
    CHARLIE_PIN_3,
    CHARLIE_PIN_4,
    CHARLIE_PIN_MAX
} charlie_pin_t;

typedef enum
{
    CHARLIE_PIN_MODE_HIZ = 0,
    CHARLIE_PIN_MODE_OUT
} charlie_pin_mode_t;

typedef enum
{
    DISP_LED_HUNDREDS_1 = 0,

    DISP_LED_TENS_A,
    DISP_LED_TENS_B,
    DISP_LED_TENS_C,
    DISP_LED_TENS_D,
    DISP_LED_TENS_E,
    DISP_LED_TENS_F,
    DISP_LED_TENS_G,

    DISP_LED_ONES_A,
    DISP_LED_ONES_B,
    DISP_LED_ONES_C,
    DISP_LED_ONES_D,
    DISP_LED_ONES_E,
    DISP_LED_ONES_F,
    DISP_LED_ONES_G,

    DISP_LED_ICON_CHARGE,
    DISP_LED_ICON_PERCENT,

    DISP_LED_MAX
} display_led_id_t;

typedef struct
{
    uint8_t sel_595_bit;
    uint8_t anode_pin;
    uint8_t cathode_pin;
    uint8_t valid;
} display_led_route_t;

typedef struct
{
    uint8_t value;       /* 0~100 */
    uint8_t charge_on;   /* 0/1 */
    uint8_t percent_on;  /* 0/1 */
} display_content_t;

#endif
