#include "main.h"
#include <string.h>

#define LEDBAR_595_FRAME_PATTERN_COUNT 5u
#if LEDBAR_DRIVER_GPIO_CHARLIE
#define LEDBAR_FRAME_PATTERN_COUNT 18u
#else
#define LEDBAR_FRAME_PATTERN_COUNT LEDBAR_595_FRAME_PATTERN_COUNT
#endif

#define LEDBAR_SOC_DISPLAY_SNAP_ENABLE PROJECT_CFG_LEDBAR_SOC_DISPLAY_SNAP_ENABLE
#define LEDBAR_SOC_DISPLAY_SNAP_WINDOW PROJECT_CFG_LEDBAR_SOC_DISPLAY_SNAP_WINDOW
#define LEDBAR_SOC_DISPLAY_SNAP_MIN_EXTRA PROJECT_CFG_LEDBAR_SOC_DISPLAY_SNAP_MIN_EXTRA
#define LEDBAR_SOC_DISPLAY_SNAP_MIN_GAIN PROJECT_CFG_LEDBAR_SOC_DISPLAY_SNAP_MIN_GAIN

#define LEDBAR_SCAN_TIMER_100KHZ_TICKS PROJECT_CFG_LEDBAR_SCAN_TIMER_100KHZ_TICKS
#define LEDBAR_MCU_WK_ON_FILTER_10MS PROJECT_CFG_LEDBAR_MCU_WK_ON_FILTER_10MS
#define LEDBAR_MCU_WK_OFF_FILTER_10MS PROJECT_CFG_LEDBAR_MCU_WK_OFF_FILTER_10MS
#define LEDBAR_CHARGE_ON_FILTER_100MS PROJECT_CFG_LEDBAR_CHARGE_ON_FILTER_100MS
#define LEDBAR_CHARGE_OFF_FILTER_100MS PROJECT_CFG_LEDBAR_CHARGE_OFF_FILTER_100MS

typedef struct
{
    uint8_t low_pin;
    uint8_t high_pin;
} LedBarRoute;

#if LEDBAR_DRIVER_GPIO_CHARLIE
typedef struct
{
    GPIO_TypeDef *port;
    uint16_t pin;
} LedBarPinDef;
#endif

#if !LEDBAR_DRIVER_GPIO_CHARLIE
typedef struct
{
    uint32_t lit_mask;
} LedBarPattern;
#endif

typedef struct
{
    uint8_t patterns[LEDBAR_FRAME_PATTERN_COUNT];
    uint8_t length;
} LedBarFrameBuffer;

#if !LEDBAR_DRIVER_GPIO_CHARLIE
typedef enum
{
    LEDBAR_SCAN_STATE_OFF_PRE = 0,
    LEDBAR_SCAN_STATE_OUTPUT_TARGET,
    LEDBAR_SCAN_STATE_HOLD_TARGET,
    LEDBAR_SCAN_STATE_OFF_POST,
    LEDBAR_SCAN_STATE_NEXT_ITEM
} LedBarScanState;
#endif

typedef enum
{
    LEDBAR_ROUTE_HUNDREDS_1_UPPER = 0,
    LEDBAR_ROUTE_HUNDREDS_1_LOWER,
    LEDBAR_ROUTE_TENS_A,
    LEDBAR_ROUTE_TENS_B,
    LEDBAR_ROUTE_TENS_C,
    LEDBAR_ROUTE_TENS_D,
    LEDBAR_ROUTE_TENS_E,
    LEDBAR_ROUTE_TENS_F,
    LEDBAR_ROUTE_TENS_G,
    LEDBAR_ROUTE_ONES_A,
    LEDBAR_ROUTE_ONES_B,
    LEDBAR_ROUTE_ONES_C,
    LEDBAR_ROUTE_ONES_D,
    LEDBAR_ROUTE_ONES_E,
    LEDBAR_ROUTE_ONES_F,
    LEDBAR_ROUTE_ONES_G,
    LEDBAR_ROUTE_ICON_CHARGE,
    LEDBAR_ROUTE_ICON_PERCENT,
    LEDBAR_ROUTE_COUNT
} LedBarRouteId;

#define LEDBAR_DIGIT_BIT_A (1u << 0)
#define LEDBAR_DIGIT_BIT_B (1u << 1)
#define LEDBAR_DIGIT_BIT_C (1u << 2)
#define LEDBAR_DIGIT_BIT_D (1u << 3)
#define LEDBAR_DIGIT_BIT_E (1u << 4)
#define LEDBAR_DIGIT_BIT_F (1u << 5)
#define LEDBAR_DIGIT_BIT_G (1u << 6)
#define LEDBAR_PATTERN_MASK_MAX 30u
#define LEDBAR_SCAN_HOLD_TICKS 3u

#define LEDBAR_595_GPIO_DATA GPIO_LED595_DATA
#define LEDBAR_595_PIN_DATA PIN_LED595_DATA
#define LEDBAR_595_GPIO_CLK GPIO_LED595_CLK
#define LEDBAR_595_PIN_CLK PIN_LED595_CLK
#define LEDBAR_595_GPIO_LATCH GPIO_LED595_LATCH
#define LEDBAR_595_PIN_LATCH PIN_LED595_LATCH

#define LEDBAR_KEY_LONG_PRESS_10MS 300u /* 3s */
#define LEDBAR_SLEEP_SOC_MAGIC 0x5A00u
#define LEDBAR_SLEEP_SOC_MAGIC_MASK 0xFF00u
#define LEDBAR_SLEEP_SOC_VALUE_MASK 0x00FFu
#define LEDBAR_SLEEP_SOC_REG BKP_DR4
#define LEDBAR_SLEEP_SOC_INV_REG BKP_DR5

LEDBAR_COMMAND LedBar_Command = LED_BAR_STARTUP;

static const LedBarRoute s_ledbar_routes[LEDBAR_ROUTE_COUNT] =
    {
        {3u, 2u},
        {3u, 1u},
        {2u, 1u},
        {1u, 2u},
        {2u, 3u},
        {1u, 3u},
        {1u, 4u},
        {2u, 4u},
        {3u, 4u},
        {1u, 0u},
        {0u, 1u},
        {2u, 0u},
        {0u, 2u},
        {3u, 0u},
        {0u, 3u},
        {0u, 4u},
        {4u, 2u},
        {4u, 1u},
};

#if LEDBAR_DRIVER_GPIO_CHARLIE
// static const LedBarPinDef s_ledbar_gpio_pins[LEDBAR_PIN_COUNT] = {
//     {LEDBAR_GPIO_P5, LEDBAR_PIN_P5},
//     {LEDBAR_GPIO_P4, LEDBAR_PIN_P4},
//     {LEDBAR_GPIO_P2, LEDBAR_PIN_P2},
//     {LEDBAR_GPIO_P3, LEDBAR_PIN_P3},
//     {LEDBAR_GPIO_P1, LEDBAR_PIN_P1},
// };
static const LedBarPinDef s_ledbar_gpio_pins[LEDBAR_PIN_COUNT] = {
    {LEDBAR_GPIO_P1, LEDBAR_PIN_P1},
    {LEDBAR_GPIO_P3, LEDBAR_PIN_P3},
    {LEDBAR_GPIO_P2, LEDBAR_PIN_P2},
    {LEDBAR_GPIO_P4, LEDBAR_PIN_P4},
    {LEDBAR_GPIO_P5, LEDBAR_PIN_P5},
};

// static const LedBarPinDef s_ledbar_gpio_pins[LEDBAR_PIN_COUNT] =
// {
//     {LEDBAR_GPIO_P1, LEDBAR_PIN_P1},
//     {LEDBAR_GPIO_P2, LEDBAR_PIN_P2},
//     {LEDBAR_GPIO_P3, LEDBAR_PIN_P3},
//     {LEDBAR_GPIO_P4, LEDBAR_PIN_P4},
//     {LEDBAR_GPIO_P5, LEDBAR_PIN_P5},
// };
#endif

static const uint8_t s_ledbar_digit_map[10] =
    {
        LEDBAR_DIGIT_BIT_A | LEDBAR_DIGIT_BIT_B | LEDBAR_DIGIT_BIT_C | LEDBAR_DIGIT_BIT_D | LEDBAR_DIGIT_BIT_E | LEDBAR_DIGIT_BIT_F,
        LEDBAR_DIGIT_BIT_B | LEDBAR_DIGIT_BIT_C,
        LEDBAR_DIGIT_BIT_A | LEDBAR_DIGIT_BIT_B | LEDBAR_DIGIT_BIT_D | LEDBAR_DIGIT_BIT_E | LEDBAR_DIGIT_BIT_G,
        LEDBAR_DIGIT_BIT_A | LEDBAR_DIGIT_BIT_B | LEDBAR_DIGIT_BIT_C | LEDBAR_DIGIT_BIT_D | LEDBAR_DIGIT_BIT_G,
        LEDBAR_DIGIT_BIT_B | LEDBAR_DIGIT_BIT_C | LEDBAR_DIGIT_BIT_F | LEDBAR_DIGIT_BIT_G,
        LEDBAR_DIGIT_BIT_A | LEDBAR_DIGIT_BIT_C | LEDBAR_DIGIT_BIT_D | LEDBAR_DIGIT_BIT_F | LEDBAR_DIGIT_BIT_G,
        LEDBAR_DIGIT_BIT_A | LEDBAR_DIGIT_BIT_C | LEDBAR_DIGIT_BIT_D | LEDBAR_DIGIT_BIT_E | LEDBAR_DIGIT_BIT_F | LEDBAR_DIGIT_BIT_G,
        LEDBAR_DIGIT_BIT_A | LEDBAR_DIGIT_BIT_B | LEDBAR_DIGIT_BIT_C,
        LEDBAR_DIGIT_BIT_A | LEDBAR_DIGIT_BIT_B | LEDBAR_DIGIT_BIT_C | LEDBAR_DIGIT_BIT_D | LEDBAR_DIGIT_BIT_E | LEDBAR_DIGIT_BIT_F | LEDBAR_DIGIT_BIT_G,
        LEDBAR_DIGIT_BIT_A | LEDBAR_DIGIT_BIT_B | LEDBAR_DIGIT_BIT_C | LEDBAR_DIGIT_BIT_D | LEDBAR_DIGIT_BIT_F | LEDBAR_DIGIT_BIT_G,
};

#if !LEDBAR_DRIVER_GPIO_CHARLIE
/*
 * These tables are generated against the current schematic:
 * QA~QE drive D0~D4 directly and OE is always enabled.
 * Each row is the shortest 74HC595 pattern sequence that covers the target
 * display while minimizing unavoidable extra lit segments.
 */
static const uint8_t s_ledbar_soc_patterns[101][5] =
    {
        {1, 14, 0, 0, 0},    /* 000, extra=1 */
        {2, 3, 0, 0, 0},     /* 001, extra=3 */
        {1, 14, 30, 0, 0},   /* 002, extra=3 */
        {1, 14, 30, 0, 0},   /* 003, extra=3 */
        {26, 11, 0, 0, 0},   /* 004, extra=3 */
        {1, 14, 30, 0, 0},   /* 005, extra=3 */
        {1, 14, 30, 0, 0},   /* 006, extra=2 */
        {1, 2, 0, 0, 0},     /* 007, extra=3 */
        {1, 14, 30, 0, 0},   /* 008, extra=1 */
        {1, 14, 30, 0, 0},   /* 009, extra=2 */
        {12, 8, 1, 14, 0},   /* 010, extra=2 */
        {4, 10, 11, 0, 0},   /* 011, extra=5 */
        {5, 10, 30, 0, 0},   /* 012, extra=4 */
        {12, 9, 14, 30, 0},  /* 013, extra=3 */
        {4, 11, 30, 0, 0},   /* 014, extra=4 */
        {28, 9, 11, 0, 0},   /* 015, extra=3 */
        {28, 11, 1, 0, 0},   /* 016, extra=3 */
        {12, 9, 14, 0, 0},   /* 017, extra=4 */
        {12, 8, 1, 14, 30},  /* 018, extra=2 */
        {12, 9, 14, 30, 0},  /* 019, extra=2 */
        {2, 12, 17, 0, 0},   /* 020, extra=3 */
        {2, 29, 17, 0, 0},   /* 021, extra=4 */
        {2, 20, 29, 21, 0},  /* 022, extra=2 */
        {2, 20, 9, 0, 0},    /* 023, extra=3 */
        {10, 20, 8, 11, 0},  /* 024, extra=3 */
        {11, 20, 8, 9, 0},   /* 025, extra=2 */
        {3, 21, 28, 0, 0},   /* 026, extra=2 */
        {2, 29, 17, 0, 0},   /* 027, extra=3 */
        {2, 28, 17, 0, 0},   /* 028, extra=2 */
        {10, 20, 9, 0, 0},   /* 029, extra=2 */
        {10, 12, 17, 0, 0},  /* 030, extra=3 */
        {2, 21, 9, 0, 0},    /* 031, extra=5 */
        {10, 21, 28, 0, 0},  /* 032, extra=3 */
        {2, 20, 9, 0, 0},    /* 033, extra=3 */
        {10, 20, 8, 11, 0},  /* 034, extra=3 */
        {11, 20, 8, 9, 0},   /* 035, extra=2 */
        {11, 21, 28, 0, 0},  /* 036, extra=2 */
        {2, 21, 9, 0, 0},    /* 037, extra=4 */
        {10, 28, 17, 0, 0},  /* 038, extra=2 */
        {10, 20, 9, 0, 0},   /* 039, extra=2 */
        {12, 8, 17, 14, 0},  /* 040, extra=3 */
        {4, 11, 18, 0, 0},   /* 041, extra=6 */
        {20, 10, 16, 21, 0}, /* 042, extra=4 */
        {12, 9, 16, 14, 0},  /* 043, extra=4 */
        {20, 10, 27, 0, 0},  /* 044, extra=4 */
        {12, 9, 16, 15, 0},  /* 045, extra=3 */
        {28, 11, 17, 0, 0},  /* 046, extra=3 */
        {13, 11, 18, 0, 0},  /* 047, extra=5 */
        {20, 10, 17, 0, 0},  /* 048, extra=3 */
        {12, 9, 16, 14, 0},  /* 049, extra=3 */
        {19, 9, 14, 0, 0},   /* 050, extra=2 */
        {2, 8, 19, 0, 0},    /* 051, extra=3 */
        {2, 9, 19, 22, 0},   /* 052, extra=3 */
        {2, 9, 18, 22, 0},   /* 053, extra=2 */
        {11, 8, 18, 0, 0},   /* 054, extra=1 */
        {2, 9, 18, 30, 0},   /* 055, extra=2 */
        {11, 28, 17, 0, 0},  /* 056, extra=2 */
        {2, 9, 18, 0, 0},    /* 057, extra=2 */
        {2, 9, 19, 30, 0},   /* 058, extra=1 */
        {2, 9, 18, 30, 0},   /* 059, extra=1 */
        {10, 8, 17, 14, 0},  /* 060, extra=1 */
        {10, 8, 16, 11, 0},  /* 061, extra=2 */
        {10, 8, 17, 30, 0},  /* 062, extra=2 */
        {10, 9, 16, 30, 0},  /* 063, extra=1 */
        {10, 8, 16, 11, 0},  /* 064, extra=0 */
        {10, 9, 16, 30, 0},  /* 065, extra=1 */
        {11, 28, 17, 0, 0},  /* 066, extra=1 */
        {2, 9, 16, 0, 0},    /* 067, extra=2 */
        {10, 8, 17, 30, 0},  /* 068, extra=0 */
        {10, 9, 16, 30, 0},  /* 069, extra=0 */
        {10, 4, 1, 0, 0},    /* 070, extra=2 */
        {2, 4, 11, 0, 0},    /* 071, extra=4 */
        {10, 5, 30, 0, 0},   /* 072, extra=3 */
        {10, 28, 9, 0, 0},   /* 073, extra=3 */
        {10, 28, 11, 0, 0},  /* 074, extra=3 */
        {11, 28, 9, 0, 0},   /* 075, extra=2 */
        {11, 28, 1, 0, 0},   /* 076, extra=2 */
        {2, 13, 9, 0, 0},    /* 077, extra=3 */
        {10, 28, 1, 0, 0},   /* 078, extra=2 */
        {10, 28, 9, 0, 0},   /* 079, extra=2 */
        {10, 12, 17, 0, 0},  /* 080, extra=1 */
        {10, 29, 17, 0, 0},  /* 081, extra=3 */
        {10, 28, 17, 0, 0},  /* 082, extra=2 */
        {10, 28, 16, 9, 0},  /* 083, extra=1 */
        {10, 28, 16, 11, 0}, /* 084, extra=1 */
        {11, 28, 16, 9, 0},  /* 085, extra=0 */
        {11, 28, 17, 0, 0},  /* 086, extra=0 */
        {10, 29, 17, 0, 0},  /* 087, extra=2 */
        {10, 28, 17, 0, 0},  /* 088, extra=0 */
        {10, 28, 16, 9, 0},  /* 089, extra=0 */
        {10, 12, 17, 0, 0},  /* 090, extra=2 */
        {10, 29, 17, 0, 0},  /* 091, extra=4 */
        {10, 28, 17, 0, 0},  /* 092, extra=3 */
        {10, 28, 16, 9, 0},  /* 093, extra=2 */
        {10, 28, 16, 11, 0}, /* 094, extra=2 */
        {11, 28, 16, 9, 0},  /* 095, extra=1 */
        {11, 28, 17, 0, 0},  /* 096, extra=1 */
        {10, 29, 17, 0, 0},  /* 097, extra=3 */
        {10, 28, 17, 0, 0},  /* 098, extra=1 */
        {10, 28, 16, 9, 0},  /* 099, extra=1 */
        {2, 5, 25, 12, 0},   /* 100, extra=1 */
};

static const uint8_t s_ledbar_soc_charge_patterns[101][5] =
    {
        {1, 14, 0, 0, 0},    /* 000, extra=0 */
        {2, 3, 15, 0, 0},    /* 001, extra=3 */
        {1, 14, 30, 0, 0},   /* 002, extra=2 */
        {1, 14, 30, 0, 0},   /* 003, extra=2 */
        {14, 1, 30, 0, 0},   /* 004, extra=3 */
        {1, 14, 30, 0, 0},   /* 005, extra=2 */
        {1, 14, 30, 0, 0},   /* 006, extra=1 */
        {1, 14, 0, 0, 0},    /* 007, extra=3 */
        {1, 14, 30, 0, 0},   /* 008, extra=0 */
        {1, 14, 30, 0, 0},   /* 009, extra=1 */
        {12, 8, 1, 14, 0},   /* 010, extra=1 */
        {4, 10, 11, 0, 0},   /* 011, extra=4 */
        {5, 10, 30, 0, 0},   /* 012, extra=3 */
        {12, 9, 14, 30, 0},  /* 013, extra=2 */
        {4, 11, 30, 0, 0},   /* 014, extra=3 */
        {28, 9, 15, 0, 0},   /* 015, extra=2 */
        {28, 8, 1, 15, 0},   /* 016, extra=2 */
        {12, 9, 14, 0, 0},   /* 017, extra=3 */
        {12, 8, 1, 14, 30},  /* 018, extra=1 */
        {12, 9, 14, 30, 0},  /* 019, extra=1 */
        {2, 12, 17, 0, 0},   /* 020, extra=2 */
        {2, 13, 17, 0, 0},   /* 021, extra=4 */
        {2, 5, 13, 20, 0},   /* 022, extra=2 */
        {2, 4, 9, 20, 0},    /* 023, extra=3 */
        {10, 12, 16, 11, 0}, /* 024, extra=3 */
        {11, 12, 16, 9, 0},  /* 025, extra=2 */
        {3, 4, 28, 21, 0},   /* 026, extra=2 */
        {2, 13, 17, 0, 0},   /* 027, extra=3 */
        {2, 12, 16, 1, 0},   /* 028, extra=2 */
        {10, 4, 9, 20, 0},   /* 029, extra=2 */
        {10, 12, 17, 0, 0},  /* 030, extra=2 */
        {11, 13, 18, 0, 0},  /* 031, extra=4 */
        {2, 5, 8, 22, 0},    /* 032, extra=3 */
        {11, 13, 22, 0, 0},  /* 033, extra=2 */
        {11, 12, 18, 0, 0},  /* 034, extra=3 */
        {11, 12, 16, 9, 0},  /* 035, extra=2 */
        {11, 4, 28, 21, 0},  /* 036, extra=2 */
        {11, 13, 18, 0, 0},  /* 037, extra=3 */
        {3, 5, 8, 22, 0},    /* 038, extra=2 */
        {2, 12, 9, 18, 0},   /* 039, extra=2 */
        {12, 8, 17, 14, 0},  /* 040, extra=2 */
        {4, 11, 18, 0, 0},   /* 041, extra=5 */
        {21, 24, 14, 0, 0},  /* 042, extra=4 */
        {12, 9, 16, 14, 0},  /* 043, extra=3 */
        {12, 11, 18, 0, 0},  /* 044, extra=4 */
        {12, 9, 16, 15, 0},  /* 045, extra=2 */
        {12, 24, 17, 15, 0}, /* 046, extra=2 */
        {13, 11, 18, 0, 0},  /* 047, extra=4 */
        {12, 24, 17, 14, 0}, /* 048, extra=2 */
        {12, 9, 16, 14, 0},  /* 049, extra=2 */
        {19, 9, 14, 0, 0},   /* 050, extra=1 */
        {18, 9, 15, 0, 0},   /* 051, extra=3 */
        {3, 9, 18, 6, 0},    /* 052, extra=3 */
        {18, 9, 6, 0, 0},    /* 053, extra=2 */
        {11, 8, 18, 15, 0},  /* 054, extra=1 */
        {18, 9, 14, 0, 0},   /* 055, extra=2 */
        {3, 9, 18, 14, 0},   /* 056, extra=2 */
        {18, 9, 15, 0, 0},   /* 057, extra=2 */
        {3, 9, 18, 14, 0},   /* 058, extra=1 */
        {18, 9, 14, 0, 0},   /* 059, extra=1 */
        {10, 8, 17, 14, 0},  /* 060, extra=0 */
        {10, 8, 16, 11, 15}, /* 061, extra=2 */
        {10, 24, 17, 14, 0}, /* 062, extra=2 */
        {10, 9, 16, 14, 0},  /* 063, extra=1 */
        {10, 8, 16, 11, 15}, /* 064, extra=0 */
        {10, 9, 16, 14, 0},  /* 065, extra=1 */
        {10, 24, 17, 14, 0}, /* 066, extra=1 */
        {18, 25, 15, 0, 0},  /* 067, extra=2 */
        {10, 24, 17, 14, 0}, /* 068, extra=0 */
        {10, 9, 16, 14, 0},  /* 069, extra=0 */
        {10, 4, 1, 0, 0},    /* 070, extra=1 */
        {2, 4, 11, 0, 0},    /* 071, extra=3 */
        {10, 5, 30, 0, 0},   /* 072, extra=2 */
        {11, 13, 30, 0, 0},  /* 073, extra=2 */
        {11, 4, 30, 0, 0},   /* 074, extra=2 */
        {11, 13, 28, 0, 0},  /* 075, extra=2 */
        {11, 5, 30, 0, 0},   /* 076, extra=2 */
        {2, 13, 9, 0, 0},    /* 077, extra=2 */
        {11, 5, 30, 0, 0},   /* 078, extra=1 */
        {11, 13, 30, 0, 0},  /* 079, extra=1 */
        {10, 12, 17, 0, 0},  /* 080, extra=0 */
        {10, 13, 17, 0, 0},  /* 081, extra=3 */
        {10, 4, 24, 21, 0},  /* 082, extra=2 */
        {10, 12, 16, 9, 0},  /* 083, extra=1 */
        {10, 12, 16, 11, 0}, /* 084, extra=1 */
        {11, 12, 16, 9, 0},  /* 085, extra=0 */
        {11, 12, 16, 1, 0},  /* 086, extra=0 */
        {10, 13, 17, 0, 0},  /* 087, extra=2 */
        {10, 12, 16, 1, 0},  /* 088, extra=0 */
        {10, 12, 16, 9, 0},  /* 089, extra=0 */
        {10, 12, 17, 0, 0},  /* 090, extra=1 */
        {11, 13, 18, 0, 0},  /* 091, extra=3 */
        {3, 4, 9, 18, 0},    /* 092, extra=3 */
        {2, 4, 9, 18, 0},    /* 093, extra=2 */
        {11, 12, 18, 0, 0},  /* 094, extra=2 */
        {11, 12, 16, 9, 0},  /* 095, extra=1 */
        {11, 12, 16, 1, 0},  /* 096, extra=1 */
        {11, 13, 18, 0, 0},  /* 097, extra=2 */
        {3, 12, 9, 18, 0},   /* 098, extra=1 */
        {2, 12, 9, 18, 0},   /* 099, extra=1 */
        {2, 5, 25, 12, 0},   /* 100, extra=0 */
};

#if LEDBAR_SOC_DISPLAY_SNAP_ENABLE
static const uint8_t s_ledbar_soc_extra_score[101] =
    {
        1u, 3u, 3u, 3u, 3u, 3u, 2u, 3u, 1u, 2u,
        2u, 5u, 4u, 3u, 4u, 3u, 3u, 4u, 2u, 2u,
        3u, 4u, 2u, 3u, 3u, 2u, 2u, 3u, 2u, 2u,
        3u, 5u, 3u, 3u, 3u, 2u, 2u, 4u, 2u, 2u,
        3u, 6u, 4u, 4u, 4u, 3u, 3u, 5u, 3u, 3u,
        2u, 3u, 3u, 2u, 1u, 2u, 2u, 2u, 1u, 1u,
        1u, 2u, 2u, 1u, 0u, 1u, 1u, 2u, 0u, 0u,
        2u, 4u, 3u, 3u, 3u, 2u, 2u, 3u, 2u, 2u,
        1u, 3u, 2u, 1u, 1u, 0u, 0u, 2u, 0u, 0u,
        2u, 4u, 3u, 2u, 2u, 1u, 1u, 3u, 1u, 1u,
        1u};

static const uint8_t s_ledbar_soc_charge_extra_score[101] =
    {
        0u, 3u, 2u, 2u, 3u, 2u, 1u, 3u, 0u, 1u,
        1u, 4u, 3u, 2u, 3u, 2u, 2u, 3u, 1u, 1u,
        2u, 4u, 2u, 3u, 3u, 2u, 2u, 3u, 2u, 2u,
        2u, 4u, 3u, 2u, 3u, 2u, 2u, 3u, 2u, 2u,
        2u, 5u, 4u, 3u, 4u, 2u, 2u, 4u, 2u, 2u,
        1u, 3u, 3u, 2u, 1u, 2u, 2u, 2u, 1u, 1u,
        0u, 2u, 2u, 1u, 0u, 1u, 1u, 2u, 0u, 0u,
        1u, 3u, 2u, 2u, 2u, 2u, 2u, 2u, 1u, 1u,
        0u, 3u, 2u, 1u, 1u, 0u, 0u, 2u, 0u, 0u,
        1u, 3u, 3u, 2u, 2u, 1u, 1u, 2u, 1u, 1u,
        0u};
#endif
#endif

static volatile uint8_t s_ledbar_number = 0u;
static volatile uint8_t s_ledbar_indicator_mask = LEDBAR_ICON_PERCENT_MASK;
static uint8_t s_ledbar_initialized = 0u;
static uint8_t s_ledbar_force_blank = 0u;
static uint8_t s_ledbar_sleep = 0u;
static uint8_t s_ledbar_test_single_segment_enable = 0u;
static uint8_t s_ledbar_test_single_segment_id = 0u;
static LedBarFrameBuffer s_ledbar_frame_front;
static LedBarFrameBuffer s_ledbar_frame_back;
static uint8_t s_ledbar_frame_pending = 0u;
static uint8_t s_ledbar_scan_index = 0u;
#if !LEDBAR_DRIVER_GPIO_CHARLIE
static uint8_t s_ledbar_scan_hold_tick = 0u;
static LedBarScanState s_ledbar_scan_state = LEDBAR_SCAN_STATE_OFF_PRE;
static uint8_t s_ledbar_last_595_value = 0xFFu;
static LedBarPattern s_ledbar_patterns[LEDBAR_PATTERN_MASK_MAX + 1u];
#endif
static uint8_t s_ledbar_scan_timer_initialized = 0u;
static uint8_t s_ledbar_scan_timer_enabled = 0u;
static uint16_t s_ledbar_soc_display_10ms = 0u;
static uint32_t s_ledbar_key_hold_10ms = 0u;
static uint32_t s_ledbar_key_press_start_10ms = 0u;
static uint8_t s_ledbar_key_last_pressed = 0u;
static uint8_t s_ledbar_key_long_handled = 0u;
static uint8_t s_ledbar_mcu_wk_filter_initialized = 0u;
static uint8_t s_ledbar_mcu_wk_active = 0u;
static uint8_t s_ledbar_mcu_wk_on_10ms = 0u;
static uint8_t s_ledbar_mcu_wk_off_10ms = 0u;
static uint8_t s_ledbar_charge_filter_initialized = 0u;
static uint8_t s_ledbar_charge_active = 0u;
static uint8_t s_ledbar_charge_on_100ms = 0u;
static uint8_t s_ledbar_charge_off_100ms = 0u;

#if LEDBAR_DRIVER_GPIO_CHARLIE
static void LedBar_AllPinsHiZ(void);
static void LedBar_AllPinsOutputLow(void);
static void LedBar_OutputRoute(uint8_t route_id);
#endif
static void LedBar_CommitBackFrameIfPending(void);

static void LedBar_EnableBackupAccess(void)
{
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_PWR | RCC_APB1Periph_BKP, ENABLE);
    PWR_BackupAccessCmd(ENABLE);
}

static uint8_t LedBar_GetRuntimeSoc(void)
{
    uint16_t soc = g_stCellInfoReport.SocElement.u16Soc;

    if (soc > 100u)
    {
        soc = 100u;
    }

    return (uint8_t)soc;
}

static uint8_t LedBar_ReadMcuWakeRaw(void)
{
    return (uint8_t)(GPIO_ReadInputDataBit(GPIO_MCU_WK, PIN_MCU_WK) != Bit_RESET);
}

static void LedBar_ServiceMcuWakeFilter(void)
{
    uint8_t raw_active = LedBar_ReadMcuWakeRaw();

    if (s_ledbar_mcu_wk_filter_initialized == 0u)
    {
        s_ledbar_mcu_wk_filter_initialized = 1u;
        s_ledbar_mcu_wk_active = raw_active;
        s_ledbar_mcu_wk_on_10ms = (raw_active != 0u) ? LEDBAR_MCU_WK_ON_FILTER_10MS : 0u;
        s_ledbar_mcu_wk_off_10ms = (raw_active == 0u) ? LEDBAR_MCU_WK_OFF_FILTER_10MS : 0u;
        return;
    }

    if (g_st_SysTimeFlag.bits.b1Sys10msFlag == 0u)
    {
        return;
    }

    if (raw_active != 0u)
    {
        s_ledbar_mcu_wk_off_10ms = 0u;
        if (s_ledbar_mcu_wk_on_10ms < LEDBAR_MCU_WK_ON_FILTER_10MS)
        {
            s_ledbar_mcu_wk_on_10ms++;
        }
        if (s_ledbar_mcu_wk_on_10ms >= LEDBAR_MCU_WK_ON_FILTER_10MS)
        {
            s_ledbar_mcu_wk_active = 1u;
        }
    }
    else
    {
        s_ledbar_mcu_wk_on_10ms = 0u;
        if (s_ledbar_mcu_wk_off_10ms < LEDBAR_MCU_WK_OFF_FILTER_10MS)
        {
            s_ledbar_mcu_wk_off_10ms++;
        }
        if (s_ledbar_mcu_wk_off_10ms >= LEDBAR_MCU_WK_OFF_FILTER_10MS)
        {
            s_ledbar_mcu_wk_active = 0u;
        }
    }
}

static uint8_t LedBar_IsMcuWakeActive(void)
{
    return s_ledbar_mcu_wk_active;
}

static void LedBar_ServiceChargeDisplayFilter(uint8_t raw_active)
{
    if (s_ledbar_charge_filter_initialized == 0u)
    {
        s_ledbar_charge_filter_initialized = 1u;
        s_ledbar_charge_active = raw_active;
        s_ledbar_charge_on_100ms = (raw_active != 0u) ? LEDBAR_CHARGE_ON_FILTER_100MS : 0u;
        s_ledbar_charge_off_100ms = (raw_active == 0u) ? LEDBAR_CHARGE_OFF_FILTER_100MS : 0u;
        return;
    }

    if (g_st_SysTimeFlag.bits.b1Sys100msFlag == 0u)
    {
        return;
    }

    if (raw_active != 0u)
    {
        s_ledbar_charge_off_100ms = 0u;
        if (s_ledbar_charge_on_100ms < LEDBAR_CHARGE_ON_FILTER_100MS)
        {
            s_ledbar_charge_on_100ms++;
        }
        if (s_ledbar_charge_on_100ms >= LEDBAR_CHARGE_ON_FILTER_100MS)
        {
            s_ledbar_charge_active = 1u;
        }
    }
    else
    {
        s_ledbar_charge_on_100ms = 0u;
        if (s_ledbar_charge_off_100ms < LEDBAR_CHARGE_OFF_FILTER_100MS)
        {
            s_ledbar_charge_off_100ms++;
        }
        if (s_ledbar_charge_off_100ms >= LEDBAR_CHARGE_OFF_FILTER_100MS)
        {
            s_ledbar_charge_active = 0u;
        }
    }
}

static void LedBar_GpioInitForDisplay(void)
{
#if LEDBAR_DRIVER_GPIO_CHARLIE
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_AFIO |
                               RCC_APB2Periph_GPIOA |
                               RCC_APB2Periph_GPIOB,
                           ENABLE);
    LedBar_AllPinsHiZ();
#else
    GPIO_InitTypeDef gpio_init;

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_AFIO |
                               RCC_APB2Periph_GPIOA |
                               RCC_APB2Periph_GPIOB,
                           ENABLE);

    gpio_init.GPIO_Mode = GPIO_Mode_Out_PP;
    gpio_init.GPIO_Speed = GPIO_Speed_2MHz;

    gpio_init.GPIO_Pin = LEDBAR_595_PIN_DATA;
    GPIO_Init(LEDBAR_595_GPIO_DATA, &gpio_init);
    gpio_init.GPIO_Pin = LEDBAR_595_PIN_CLK;
    GPIO_Init(LEDBAR_595_GPIO_CLK, &gpio_init);
    gpio_init.GPIO_Pin = LEDBAR_595_PIN_LATCH;
    GPIO_Init(LEDBAR_595_GPIO_LATCH, &gpio_init);
    gpio_init.GPIO_Pin = PIN_SEG_EN;
    GPIO_Init(GPIO_SEG_EN, &gpio_init);

    GPIO_SetBits(GPIO_SEG_EN, PIN_SEG_EN);
#endif
}

static void LedBar_GpioPrepareForStop(void)
{
#if LEDBAR_DRIVER_GPIO_CHARLIE
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA |
                               RCC_APB2Periph_GPIOB,
                           ENABLE);
    LedBar_AllPinsOutputLow();
#else
    GPIO_InitTypeDef gpio_init;

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA |
                               RCC_APB2Periph_GPIOB,
                           ENABLE);

    GPIO_ResetBits(GPIO_SEG_EN, PIN_SEG_EN);

    gpio_init.GPIO_Mode = GPIO_Mode_AIN;
    gpio_init.GPIO_Speed = GPIO_Speed_2MHz;

    gpio_init.GPIO_Pin = LEDBAR_595_PIN_DATA;
    GPIO_Init(LEDBAR_595_GPIO_DATA, &gpio_init);
    gpio_init.GPIO_Pin = LEDBAR_595_PIN_CLK;
    GPIO_Init(LEDBAR_595_GPIO_CLK, &gpio_init);
    gpio_init.GPIO_Pin = LEDBAR_595_PIN_LATCH;
    GPIO_Init(LEDBAR_595_GPIO_LATCH, &gpio_init);
    gpio_init.GPIO_Pin = PIN_SEG_EN;
    GPIO_Init(GPIO_SEG_EN, &gpio_init);
#endif
}

static UINT16 LedBar_GetTimerPrescalerFor100kHz(void)
{
    UINT32 div = SystemCoreClock / 100000U;

    if (div == 0U)
    {
        div = 1U;
    }
    if (div > 0x10000U)
    {
        div = 0x10000U;
    }

    return (UINT16)(div - 1U);
}

static void LedBar_ScanTimerInit(void)
{
    TIM_TimeBaseInitTypeDef timer_init;
    NVIC_InitTypeDef nvic_init;

    if (s_ledbar_scan_timer_initialized != 0u)
    {
        return;
    }

    RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM4, ENABLE);
    TIM_Cmd(TIM4, DISABLE);

    timer_init.TIM_Prescaler = LedBar_GetTimerPrescalerFor100kHz();
    timer_init.TIM_Period = LEDBAR_SCAN_TIMER_100KHZ_TICKS - 1U;
    timer_init.TIM_ClockDivision = TIM_CKD_DIV1;
    timer_init.TIM_CounterMode = TIM_CounterMode_Up;
    timer_init.TIM_RepetitionCounter = 0x00;
    TIM_TimeBaseInit(TIM4, &timer_init);
    TIM_ClearITPendingBit(TIM4, TIM_IT_Update);
    TIM_ITConfig(TIM4, TIM_IT_Update, ENABLE);

    nvic_init.NVIC_IRQChannel = TIM4_IRQn;
    nvic_init.NVIC_IRQChannelPreemptionPriority = 1;
    nvic_init.NVIC_IRQChannelSubPriority = 3;
    nvic_init.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&nvic_init);

    s_ledbar_scan_timer_initialized = 1u;
}

static void LedBar_ScanTimerSetEnabled(uint8_t enable)
{
    enable = (enable != 0u) ? 1u : 0u;

    if (enable == 0u)
    {
        if (s_ledbar_scan_timer_initialized != 0u)
        {
            TIM_Cmd(TIM4, DISABLE);
            TIM_ITConfig(TIM4, TIM_IT_Update, DISABLE);
            TIM_ClearITPendingBit(TIM4, TIM_IT_Update);
            NVIC_DisableIRQ(TIM4_IRQn);
            NVIC_ClearPendingIRQ(TIM4_IRQn);
            RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM4, DISABLE);
            s_ledbar_scan_timer_initialized = 0u;
        }
        s_ledbar_scan_timer_enabled = 0u;
        return;
    }

    LedBar_GpioInitForDisplay();
    LedBar_ScanTimerInit();
    if (s_ledbar_scan_timer_enabled == 0u)
    {
        TIM_SetCounter(TIM4, 0U);
        TIM_ClearITPendingBit(TIM4, TIM_IT_Update);
        TIM_Cmd(TIM4, ENABLE);
        s_ledbar_scan_timer_enabled = 1u;
    }
}

#if !LEDBAR_DRIVER_GPIO_CHARLIE
static uint8_t LedBar_Popcount32(uint32_t value)
{
    uint8_t count = 0u;

    while (value != 0u)
    {
        value &= (value - 1u);
        count++;
    }

    return count;
}

static void LedBar_SetGpioLevel(GPIO_TypeDef *port, uint16_t pin, uint8_t level)
{
    if (level != 0u)
    {
        port->BSRR = pin;
    }
    else
    {
        port->BRR = pin;
    }
}
#endif

#if LEDBAR_DRIVER_GPIO_CHARLIE
static uint8_t LedBar_GetPinIndex(uint16_t pin)
{
    uint8_t index = 0u;

    while (((pin >> index) & 0x1u) == 0u)
    {
        index++;
    }

    return index;
}

static void LedBar_PinModeF1(GPIO_TypeDef *port, uint32_t pin_index, uint32_t mode_bits)
{
    volatile uint32_t *config_reg;
    uint32_t shift;

    if (pin_index < 8u)
    {
        config_reg = &port->CRL;
        shift = pin_index * 4u;
    }
    else
    {
        config_reg = &port->CRH;
        shift = (pin_index - 8u) * 4u;
    }

    *config_reg &= ~(0xFu << shift);
    *config_reg |= (mode_bits << shift);
}

static void LedBar_PinToInput(uint8_t pin_id)
{
    GPIO_TypeDef *port = s_ledbar_gpio_pins[pin_id].port;
    uint32_t pin_index = (uint32_t)LedBar_GetPinIndex(s_ledbar_gpio_pins[pin_id].pin);

    LedBar_PinModeF1(port, pin_index, 0x4u);
}

static void LedBar_PinToOutput(uint8_t pin_id, BitAction level)
{
    GPIO_TypeDef *port = s_ledbar_gpio_pins[pin_id].port;
    uint16_t pin = s_ledbar_gpio_pins[pin_id].pin;
    uint32_t pin_index = (uint32_t)LedBar_GetPinIndex(pin);

    if (level != Bit_RESET)
    {
        port->BSRR = pin;
    }
    else
    {
        port->BRR = pin;
    }

    LedBar_PinModeF1(port, pin_index, 0x2u);
}

static void LedBar_AllPinsHiZ(void)
{
    uint8_t pin_id;

    for (pin_id = 0u; pin_id < LEDBAR_PIN_COUNT; ++pin_id)
    {
        LedBar_PinToInput(pin_id);
    }
}

static void LedBar_AllPinsOutputLow(void)
{
    uint8_t pin_id;

    LedBar_AllPinsHiZ();
    for (pin_id = 0u; pin_id < LEDBAR_PIN_COUNT; ++pin_id)
    {
        LedBar_PinToOutput(pin_id, Bit_RESET);
    }
}

static void LedBar_OutputRoute(uint8_t route_id)
{
    const LedBarRoute *route;

    LedBar_AllPinsHiZ();
    if (route_id >= (uint8_t)LEDBAR_ROUTE_COUNT)
    {
        return;
    }

    route = &s_ledbar_routes[route_id];
    LedBar_PinToOutput(route->low_pin, Bit_RESET);
    LedBar_PinToOutput(route->high_pin, Bit_SET);
}

static void LedBar_OutputOff(void)
{
    LedBar_AllPinsHiZ();
}
#else
static void LedBar_595DelaySmall(void)
{
    __NOP();
    __NOP();
    __NOP();
    __NOP();
}

static void LedBar_595WriteByte(uint8_t data)
{
    uint8_t bit;

    for (bit = 0u; bit < 8u; ++bit)
    {
        LedBar_SetGpioLevel(LEDBAR_595_GPIO_DATA, LEDBAR_595_PIN_DATA, (uint8_t)((data & 0x80u) != 0u));
        LedBar_595DelaySmall();
        LedBar_SetGpioLevel(LEDBAR_595_GPIO_CLK, LEDBAR_595_PIN_CLK, 1u);
        LedBar_595DelaySmall();
        LedBar_SetGpioLevel(LEDBAR_595_GPIO_CLK, LEDBAR_595_PIN_CLK, 0u);
        data <<= 1;
    }

    LedBar_SetGpioLevel(LEDBAR_595_GPIO_LATCH, LEDBAR_595_PIN_LATCH, 1u);
    LedBar_595DelaySmall();
    LedBar_SetGpioLevel(LEDBAR_595_GPIO_LATCH, LEDBAR_595_PIN_LATCH, 0u);
}

static void LedBar_OutputPattern(uint8_t pattern_mask)
{
    pattern_mask = (uint8_t)(pattern_mask & 0x1Fu);

    if (pattern_mask != s_ledbar_last_595_value)
    {
        LedBar_595WriteByte(pattern_mask);
        s_ledbar_last_595_value = pattern_mask;
    }
}

static void LedBar_OutputOff(void)
{
    LedBar_OutputPattern(0u);
}
#endif

static void LedBar_ShowFrontFrameNow(void)
{
    LedBar_CommitBackFrameIfPending();

    if (s_ledbar_frame_front.length == 0u)
    {
        LedBar_OutputOff();
        LedBar_ScanTimerSetEnabled(0u);
        return;
    }

    LedBar_ScanTimerSetEnabled(1u);
#if LEDBAR_DRIVER_GPIO_CHARLIE
    s_ledbar_scan_index = 0u;
    LedBar_OutputRoute(s_ledbar_frame_front.patterns[0]);
    s_ledbar_scan_index = (s_ledbar_frame_front.length > 1u) ? 1u : 0u;
#else
    s_ledbar_scan_index = 0u;
    s_ledbar_scan_hold_tick = 0u;
    s_ledbar_scan_state = LEDBAR_SCAN_STATE_HOLD_TARGET;
    LedBar_OutputPattern(s_ledbar_frame_front.patterns[0]);
#endif
}

#if !LEDBAR_DRIVER_GPIO_CHARLIE
static void LedBar_InitPatternTable(void)
{
    uint8_t pattern_mask;
    uint8_t route_id;

    for (pattern_mask = 0u; pattern_mask <= LEDBAR_PATTERN_MASK_MAX; ++pattern_mask)
    {
        uint32_t lit_mask = 0u;

        for (route_id = 0u; route_id < (uint8_t)LEDBAR_ROUTE_COUNT; ++route_id)
        {
            const LedBarRoute *route = &s_ledbar_routes[route_id];
            uint8_t high_level = (uint8_t)((pattern_mask >> route->high_pin) & 0x1u);
            uint8_t low_level = (uint8_t)((pattern_mask >> route->low_pin) & 0x1u);

            if ((high_level != 0u) && (low_level == 0u))
            {
                lit_mask |= (1UL << route_id);
            }
        }

        s_ledbar_patterns[pattern_mask].lit_mask = lit_mask;
    }
}
#endif

static uint32_t LedBar_BuildTargetMask(uint8_t value, uint8_t indicator_mask)
{
    uint8_t show_hundreds = (uint8_t)(value >= 100u);
    uint8_t show_tens = (uint8_t)((value >= 10u) || (show_hundreds != 0u));
    uint8_t tens = (uint8_t)((value / 10u) % 10u);
    uint8_t ones = (uint8_t)(value % 10u);
    uint8_t digit_mask;
    uint32_t target_mask = 0u;

    if (show_hundreds != 0u)
    {
        target_mask |= (1UL << LEDBAR_ROUTE_HUNDREDS_1_UPPER);
        target_mask |= (1UL << LEDBAR_ROUTE_HUNDREDS_1_LOWER);
    }

    if (show_tens != 0u)
    {
        digit_mask = s_ledbar_digit_map[tens];
        if ((digit_mask & LEDBAR_DIGIT_BIT_A) != 0u)
        {
            target_mask |= (1UL << LEDBAR_ROUTE_TENS_A);
        }
        if ((digit_mask & LEDBAR_DIGIT_BIT_B) != 0u)
        {
            target_mask |= (1UL << LEDBAR_ROUTE_TENS_B);
        }
        if ((digit_mask & LEDBAR_DIGIT_BIT_C) != 0u)
        {
            target_mask |= (1UL << LEDBAR_ROUTE_TENS_C);
        }
        if ((digit_mask & LEDBAR_DIGIT_BIT_D) != 0u)
        {
            target_mask |= (1UL << LEDBAR_ROUTE_TENS_D);
        }
        if ((digit_mask & LEDBAR_DIGIT_BIT_E) != 0u)
        {
            target_mask |= (1UL << LEDBAR_ROUTE_TENS_E);
        }
        if ((digit_mask & LEDBAR_DIGIT_BIT_F) != 0u)
        {
            target_mask |= (1UL << LEDBAR_ROUTE_TENS_F);
        }
        if ((digit_mask & LEDBAR_DIGIT_BIT_G) != 0u)
        {
            target_mask |= (1UL << LEDBAR_ROUTE_TENS_G);
        }
    }

    digit_mask = s_ledbar_digit_map[ones];
    if ((digit_mask & LEDBAR_DIGIT_BIT_A) != 0u)
    {
        target_mask |= (1UL << LEDBAR_ROUTE_ONES_A);
    }
    if ((digit_mask & LEDBAR_DIGIT_BIT_B) != 0u)
    {
        target_mask |= (1UL << LEDBAR_ROUTE_ONES_B);
    }
    if ((digit_mask & LEDBAR_DIGIT_BIT_C) != 0u)
    {
        target_mask |= (1UL << LEDBAR_ROUTE_ONES_C);
    }
    if ((digit_mask & LEDBAR_DIGIT_BIT_D) != 0u)
    {
        target_mask |= (1UL << LEDBAR_ROUTE_ONES_D);
    }
    if ((digit_mask & LEDBAR_DIGIT_BIT_E) != 0u)
    {
        target_mask |= (1UL << LEDBAR_ROUTE_ONES_E);
    }
    if ((digit_mask & LEDBAR_DIGIT_BIT_F) != 0u)
    {
        target_mask |= (1UL << LEDBAR_ROUTE_ONES_F);
    }
    if ((digit_mask & LEDBAR_DIGIT_BIT_G) != 0u)
    {
        target_mask |= (1UL << LEDBAR_ROUTE_ONES_G);
    }

    if ((indicator_mask & LEDBAR_ICON_CHARGE_MASK) != 0u)
    {
        target_mask |= (1UL << LEDBAR_ROUTE_ICON_CHARGE);
    }
    if ((indicator_mask & LEDBAR_ICON_PERCENT_MASK) != 0u)
    {
        target_mask |= (1UL << LEDBAR_ROUTE_ICON_PERCENT);
    }

    return target_mask;
}

static void LedBar_ClearFrameBuffer(LedBarFrameBuffer *frame)
{
    memset(frame->patterns, 0, sizeof(frame->patterns));
    frame->length = 0u;
}

#if !LEDBAR_DRIVER_GPIO_CHARLIE
static void LedBar_CopyFramePatternsToBuffer(LedBarFrameBuffer *frame, const uint8_t *patterns)
{
    uint8_t index;

    LedBar_ClearFrameBuffer(frame);
    for (index = 0u; index < LEDBAR_595_FRAME_PATTERN_COUNT; ++index)
    {
        if (patterns[index] == 0u)
        {
            break;
        }

        frame->patterns[frame->length] = patterns[index];
        frame->length++;
    }
}
#endif

#if LEDBAR_DRIVER_GPIO_CHARLIE
static void LedBar_BuildRouteFrameToBuffer(LedBarFrameBuffer *frame, uint32_t target_mask)
{
    uint8_t route_id;

    LedBar_ClearFrameBuffer(frame);
    for (route_id = 0u; route_id < (uint8_t)LEDBAR_ROUTE_COUNT; ++route_id)
    {
        if ((target_mask & (1UL << route_id)) != 0u)
        {
            frame->patterns[frame->length] = route_id;
            frame->length++;
            if (frame->length >= LEDBAR_FRAME_PATTERN_COUNT)
            {
                break;
            }
        }
    }
}
#endif

#if !LEDBAR_DRIVER_GPIO_CHARLIE
static uint8_t LedBar_FindBestPatternForRoute(uint8_t route_id, uint32_t target_mask)
{
    uint8_t pattern_mask;
    uint8_t best_pattern = 0u;
    int16_t best_score = -32768;

    for (pattern_mask = 1u; pattern_mask <= LEDBAR_PATTERN_MASK_MAX; ++pattern_mask)
    {
        uint32_t lit_mask = s_ledbar_patterns[pattern_mask].lit_mask;
        uint8_t extra_count;
        uint8_t desired_count;
        int16_t score;

        if ((lit_mask & (1UL << route_id)) == 0u)
        {
            continue;
        }

        extra_count = LedBar_Popcount32(lit_mask & (~target_mask));
        desired_count = LedBar_Popcount32(lit_mask & target_mask);

        score = (int16_t)(desired_count * 2) - (int16_t)(extra_count * 6);
        if (extra_count == 0u)
        {
            score += 4;
        }

        if (score > best_score)
        {
            best_score = score;
            best_pattern = pattern_mask;
        }
    }

    return best_pattern;
}

static void LedBar_BuildGreedyFrameToBuffer(LedBarFrameBuffer *frame, uint32_t target_mask)
{
    uint32_t covered_mask = 0u;
    uint32_t remaining_mask = target_mask;

    LedBar_ClearFrameBuffer(frame);

    while ((remaining_mask != 0u) && (frame->length < LEDBAR_FRAME_PATTERN_COUNT))
    {
        uint8_t pattern_mask;
        uint8_t best_pattern = 0u;
        int16_t best_score = -32768;

        for (pattern_mask = 1u; pattern_mask <= LEDBAR_PATTERN_MASK_MAX; ++pattern_mask)
        {
            uint32_t lit_mask = s_ledbar_patterns[pattern_mask].lit_mask;
            uint32_t new_target_mask = lit_mask & remaining_mask;
            uint8_t new_target_count;
            uint8_t extra_count;
            uint8_t repeated_count;
            int16_t score;

            if (new_target_mask == 0u)
            {
                continue;
            }

            new_target_count = LedBar_Popcount32(new_target_mask);
            extra_count = LedBar_Popcount32(lit_mask & (~target_mask));
            repeated_count = LedBar_Popcount32(lit_mask & covered_mask);

            score = (int16_t)(new_target_count * 12) - (int16_t)(extra_count * 5) - (int16_t)repeated_count;
            if (extra_count == 0u)
            {
                score += 2;
            }
            if (new_target_count >= 3u)
            {
                score += 1;
            }

            if (score > best_score)
            {
                best_score = score;
                best_pattern = pattern_mask;
            }
        }

        if (best_pattern == 0u)
        {
            break;
        }

        frame->patterns[frame->length] = best_pattern;
        frame->length++;
        covered_mask |= (s_ledbar_patterns[best_pattern].lit_mask & target_mask);
        remaining_mask = target_mask & (~covered_mask);
    }

    while ((remaining_mask != 0u) && (frame->length < LEDBAR_FRAME_PATTERN_COUNT))
    {
        uint8_t route_id;
        uint8_t fallback_pattern = 0u;

        for (route_id = 0u; route_id < (uint8_t)LEDBAR_ROUTE_COUNT; ++route_id)
        {
            if ((remaining_mask & (1UL << route_id)) != 0u)
            {
                fallback_pattern = LedBar_FindBestPatternForRoute(route_id, target_mask);
                break;
            }
        }

        if (fallback_pattern == 0u)
        {
            break;
        }

        frame->patterns[frame->length] = fallback_pattern;
        frame->length++;
        covered_mask |= (s_ledbar_patterns[fallback_pattern].lit_mask & target_mask);
        remaining_mask = target_mask & (~covered_mask);
    }
}
#endif

static void LedBar_ResetScanState(void)
{
#if LEDBAR_DRIVER_GPIO_CHARLIE
    s_ledbar_scan_index = 0u;
#else
    s_ledbar_scan_hold_tick = 0u;
    s_ledbar_scan_state = LEDBAR_SCAN_STATE_OFF_PRE;
    if (s_ledbar_scan_index >= s_ledbar_frame_front.length)
    {
        s_ledbar_scan_index = 0u;
    }
#endif
}

static void LedBar_CommitBackFrame(void)
{
    memcpy(&s_ledbar_frame_front, &s_ledbar_frame_back, sizeof(s_ledbar_frame_front));
    s_ledbar_frame_pending = 0u;
    LedBar_ResetScanState();
}

static void LedBar_CommitBackFrameIfPending(void)
{
    if (s_ledbar_frame_pending != 0u)
    {
        LedBar_CommitBackFrame();
    }
}

#if !LEDBAR_DRIVER_GPIO_CHARLIE
#if LEDBAR_SOC_DISPLAY_SNAP_ENABLE
static uint8_t LedBar_GetDisplayExtraScore(uint8_t value, uint8_t indicator_mask)
{
    if (value > 100u)
    {
        value = 100u;
    }

    if (indicator_mask == (LEDBAR_ICON_PERCENT_MASK | LEDBAR_ICON_CHARGE_MASK))
    {
        return s_ledbar_soc_charge_extra_score[value];
    }

    if (indicator_mask == LEDBAR_ICON_PERCENT_MASK)
    {
        return s_ledbar_soc_extra_score[value];
    }

    return 0u;
}
#endif

static uint8_t LedBar_SelectDisplayValue(uint8_t value, uint8_t indicator_mask)
{
#if LEDBAR_SOC_DISPLAY_SNAP_ENABLE
    /* Only move severe ghosting values by a small range; keep normal values exact. */
    uint8_t best_value = value;
    uint8_t best_extra;
    uint8_t exact_extra;
    uint8_t delta;

    if ((indicator_mask != LEDBAR_ICON_PERCENT_MASK) &&
        (indicator_mask != (LEDBAR_ICON_PERCENT_MASK | LEDBAR_ICON_CHARGE_MASK)))
    {
        return value;
    }

    if ((value < 10u) || (value >= 100u))
    {
        return value;
    }

    exact_extra = LedBar_GetDisplayExtraScore(value, indicator_mask);
    if (exact_extra < LEDBAR_SOC_DISPLAY_SNAP_MIN_EXTRA)
    {
        return value;
    }

    best_extra = exact_extra;
    for (delta = 1u; delta <= LEDBAR_SOC_DISPLAY_SNAP_WINDOW; ++delta)
    {
        if (value >= delta)
        {
            uint8_t candidate = (uint8_t)(value - delta);
            uint8_t candidate_extra = LedBar_GetDisplayExtraScore(candidate, indicator_mask);

            if (((candidate_extra + LEDBAR_SOC_DISPLAY_SNAP_MIN_GAIN) <= exact_extra) &&
                (candidate_extra < best_extra))
            {
                best_extra = candidate_extra;
                best_value = candidate;
            }
        }

        if ((uint8_t)(value + delta) <= 100u)
        {
            uint8_t candidate = (uint8_t)(value + delta);
            uint8_t candidate_extra = LedBar_GetDisplayExtraScore(candidate, indicator_mask);

            if (((candidate_extra + LEDBAR_SOC_DISPLAY_SNAP_MIN_GAIN) <= exact_extra) &&
                (candidate_extra < best_extra))
            {
                best_extra = candidate_extra;
                best_value = candidate;
            }
        }
    }

    return best_value;
#else
    (void)indicator_mask;
    return value;
#endif
}
#endif

static void LedBar_RebuildFrame(void)
{
    uint8_t value = s_ledbar_number;
    uint8_t indicator_mask = (uint8_t)(s_ledbar_indicator_mask & (LEDBAR_ICON_CHARGE_MASK | LEDBAR_ICON_PERCENT_MASK));
#if LEDBAR_DRIVER_GPIO_CHARLIE
    uint32_t target_mask;
#endif

    if (value > 100u)
    {
        value = 100u;
    }

#if LEDBAR_DRIVER_GPIO_CHARLIE
    if ((s_ledbar_force_blank != 0u) || (s_ledbar_sleep != 0u))
    {
        target_mask = 0u;
    }
    else if (s_ledbar_test_single_segment_enable != 0u)
    {
        uint8_t route_id = (uint8_t)(s_ledbar_test_single_segment_id % (uint8_t)LEDBAR_ROUTE_COUNT);
        target_mask = (1UL << route_id);
    }
    else
    {
        target_mask = LedBar_BuildTargetMask(value, indicator_mask);
    }

    LedBar_BuildRouteFrameToBuffer(&s_ledbar_frame_back, target_mask);
    s_ledbar_frame_pending = 1u;
    return;
#else
    if ((s_ledbar_force_blank != 0u) || (s_ledbar_sleep != 0u))
    {
        LedBar_ClearFrameBuffer(&s_ledbar_frame_back);
        s_ledbar_frame_pending = 1u;
        return;
    }

    if (s_ledbar_test_single_segment_enable != 0u)
    {
        uint8_t route_id = (uint8_t)(s_ledbar_test_single_segment_id % (uint8_t)LEDBAR_ROUTE_COUNT);
        uint8_t pattern = LedBar_FindBestPatternForRoute(route_id, (1UL << route_id));
        LedBar_ClearFrameBuffer(&s_ledbar_frame_back);
        if (pattern != 0u)
        {
            s_ledbar_frame_back.patterns[0] = pattern;
            s_ledbar_frame_back.length = 1u;
        }
        s_ledbar_frame_pending = 1u;
        return;
    }

    value = LedBar_SelectDisplayValue(value, indicator_mask);

    if (indicator_mask == LEDBAR_ICON_PERCENT_MASK)
    {
        LedBar_CopyFramePatternsToBuffer(&s_ledbar_frame_back, s_ledbar_soc_patterns[value]);
        s_ledbar_frame_pending = 1u;
        return;
    }

    if (indicator_mask == (LEDBAR_ICON_PERCENT_MASK | LEDBAR_ICON_CHARGE_MASK))
    {
        LedBar_CopyFramePatternsToBuffer(&s_ledbar_frame_back, s_ledbar_soc_charge_patterns[value]);
        s_ledbar_frame_pending = 1u;
        return;
    }

    LedBar_BuildGreedyFrameToBuffer(&s_ledbar_frame_back, LedBar_BuildTargetMask(value, indicator_mask));
    s_ledbar_frame_pending = 1u;
#endif
}

void LedBar_Init(void)
{
    if (s_ledbar_initialized != 0u)
    {
        return;
    }

#if !LEDBAR_DRIVER_GPIO_CHARLIE
    LedBar_InitPatternTable();
#endif
    s_ledbar_number = 0u;
    s_ledbar_indicator_mask = LEDBAR_ICON_PERCENT_MASK;
    s_ledbar_force_blank = 1u;
    s_ledbar_sleep = 0u;
    s_ledbar_test_single_segment_enable = 0u;
    s_ledbar_test_single_segment_id = 0u;
    s_ledbar_soc_display_10ms = 0u;
    s_ledbar_key_hold_10ms = 0u;
    s_ledbar_key_press_start_10ms = 0u;
    s_ledbar_key_last_pressed = 0u;
    s_ledbar_key_long_handled = 0u;
    s_ledbar_mcu_wk_filter_initialized = 0u;
    s_ledbar_mcu_wk_active = 0u;
    s_ledbar_mcu_wk_on_10ms = 0u;
    s_ledbar_mcu_wk_off_10ms = 0u;
    s_ledbar_charge_filter_initialized = 0u;
    s_ledbar_charge_active = 0u;
    s_ledbar_charge_on_100ms = 0u;
    s_ledbar_charge_off_100ms = 0u;
    LedBar_ClearFrameBuffer(&s_ledbar_frame_front);
    LedBar_ClearFrameBuffer(&s_ledbar_frame_back);
    s_ledbar_frame_pending = 0u;
    s_ledbar_scan_index = 0u;
#if !LEDBAR_DRIVER_GPIO_CHARLIE
    s_ledbar_scan_hold_tick = 0u;
    s_ledbar_scan_state = LEDBAR_SCAN_STATE_OFF_PRE;
    s_ledbar_last_595_value = 0xFFu;
#endif
    LedBar_Command = LED_BAR_NORMAL;
    LedBar_RebuildFrame();
    LedBar_CommitBackFrameIfPending();
    LedBar_GpioInitForDisplay();
    LedBar_OutputOff();
    s_ledbar_initialized = 1u;
    LedBar_ScanTimerSetEnabled(0u);
    LedBar_GpioPrepareForStop();
}

void LedBar_Clear(void)
{
    if (s_ledbar_initialized == 0u)
    {
        LedBar_Init();
    }

    s_ledbar_force_blank = 1u;
    LedBar_RebuildFrame();
    LedBar_CommitBackFrameIfPending();
    LedBar_OutputOff();
    LedBar_ScanTimerSetEnabled(0u);
    LedBar_GpioPrepareForStop();
}

void LedBar_SetSleep(uint8_t enable)
{
    if (s_ledbar_initialized == 0u)
    {
        LedBar_Init();
    }

    enable = (enable != 0u) ? 1u : 0u;
#if !LEDBAR_SLEEP_ENABLE
    if (enable != 0u)
    {
        if (s_ledbar_sleep != 0u)
        {
            s_ledbar_sleep = 0u;
            LedBar_RebuildFrame();
            if (s_ledbar_force_blank == 0u)
            {
                LedBar_ShowFrontFrameNow();
            }
        }
        return;
    }
#endif
    if (s_ledbar_sleep == enable)
    {
        return;
    }

    s_ledbar_sleep = enable;
    LedBar_RebuildFrame();
    if (s_ledbar_sleep != 0u)
    {
        LedBar_ScanTimerSetEnabled(0u);
        LedBar_CommitBackFrameIfPending();
        LedBar_GpioInitForDisplay();
        LedBar_OutputOff();
        LedBar_GpioPrepareForStop();
    }
    else
    {
        if (s_ledbar_force_blank == 0u)
        {
            LedBar_ScanTimerSetEnabled(1u);
        }
    }
}

void LedBar_Wakeup(void)
{
    LedBar_SetSleep(0u);
}

void LedBar_EnableSingleSegmentTest(uint8_t enable)
{
    if (s_ledbar_initialized == 0u)
    {
        LedBar_Init();
    }

    enable = (enable != 0u) ? 1u : 0u;
    if (s_ledbar_test_single_segment_enable == enable)
    {
        return;
    }

    s_ledbar_test_single_segment_enable = enable;
    if (enable == 0u)
    {
        s_ledbar_test_single_segment_id = 0u;
    }
    s_ledbar_force_blank = 0u;
    LedBar_RebuildFrame();
    if (s_ledbar_sleep == 0u)
    {
        LedBar_ShowFrontFrameNow();
    }
}

void LedBar_SetSingleSegmentIndex(uint8_t segment_id)
{
    if (s_ledbar_initialized == 0u)
    {
        LedBar_Init();
    }

    if (segment_id > LEDBAR_SINGLE_SEG_ID_MAX)
    {
        segment_id = LEDBAR_SINGLE_SEG_ID_MAX;
    }
    if (s_ledbar_test_single_segment_id == segment_id)
    {
        return;
    }

    s_ledbar_test_single_segment_id = segment_id;
    if (s_ledbar_test_single_segment_enable != 0u)
    {
        LedBar_RebuildFrame();
        if (s_ledbar_sleep == 0u)
        {
            LedBar_ShowFrontFrameNow();
        }
    }
}

void LedBar_SetNumber(uint8_t value)
{
    if (s_ledbar_initialized == 0u)
    {
        LedBar_Init();
    }

    if (value > 100u)
    {
        value = 100u;
    }

    if ((s_ledbar_number == value) && (s_ledbar_force_blank == 0u))
    {
        return;
    }

    s_ledbar_number = value;
    s_ledbar_force_blank = 0u;
    LedBar_RebuildFrame();
    if (s_ledbar_sleep == 0u)
    {
        LedBar_ShowFrontFrameNow();
    }
}

void LedBar_SetIndicators(uint8_t indicator_mask)
{
    if (s_ledbar_initialized == 0u)
    {
        LedBar_Init();
    }

    indicator_mask = (uint8_t)(indicator_mask & (LEDBAR_ICON_CHARGE_MASK | LEDBAR_ICON_PERCENT_MASK));
    if ((s_ledbar_indicator_mask == indicator_mask) && (s_ledbar_force_blank == 0u))
    {
        return;
    }

    s_ledbar_indicator_mask = indicator_mask;
    s_ledbar_force_blank = 0u;
    LedBar_RebuildFrame();
    if (s_ledbar_sleep == 0u)
    {
        LedBar_ShowFrontFrameNow();
    }
}

void LedBar_SetIndicatorState(uint8_t indicator_mask, uint8_t enable)
{
    uint8_t old_indicator_mask;

    if (s_ledbar_initialized == 0u)
    {
        LedBar_Init();
    }

    indicator_mask = (uint8_t)(indicator_mask & (LEDBAR_ICON_CHARGE_MASK | LEDBAR_ICON_PERCENT_MASK));
    old_indicator_mask = s_ledbar_indicator_mask;

    if (enable != 0u)
    {
        s_ledbar_indicator_mask = (uint8_t)(s_ledbar_indicator_mask | indicator_mask);
    }
    else
    {
        s_ledbar_indicator_mask = (uint8_t)(s_ledbar_indicator_mask & (uint8_t)(~indicator_mask));
    }

    if ((s_ledbar_indicator_mask == old_indicator_mask) && (s_ledbar_force_blank == 0u))
    {
        return;
    }

    s_ledbar_force_blank = 0u;
    LedBar_RebuildFrame();
    if (s_ledbar_sleep == 0u)
    {
        LedBar_ShowFrontFrameNow();
    }
}

void LedBar_SaveSleepSoc(void)
{
    uint16_t value = (uint16_t)(LEDBAR_SLEEP_SOC_MAGIC | LedBar_GetRuntimeSoc());

    LedBar_EnableBackupAccess();
    BKP_WriteBackupRegister(LEDBAR_SLEEP_SOC_REG, value);
    BKP_WriteBackupRegister(LEDBAR_SLEEP_SOC_INV_REG, (uint16_t)(~value));
}

uint8_t LedBar_LoadSleepSoc(void)
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
    if (s_ledbar_initialized == 0u)
    {
        LedBar_Init();
    }

    s_ledbar_sleep = 0u;
    s_ledbar_force_blank = 0u;
    s_ledbar_number = LedBar_LoadSleepSoc();
    s_ledbar_indicator_mask = LEDBAR_ICON_PERCENT_MASK;
    LedBar_RebuildFrame();
    LedBar_ShowFrontFrameNow();
}

void LedBar_PrepareForStop(void)
{
    if (s_ledbar_initialized == 0u)
    {
        LedBar_Init();
    }

#if !LEDBAR_SLEEP_ENABLE
    LedBar_SetSleep(0u);
    return;
#else
    s_ledbar_sleep = 1u;
    s_ledbar_force_blank = 1u;
    LedBar_RebuildFrame();
    LedBar_CommitBackFrameIfPending();
    LedBar_ScanTimerSetEnabled(0u);
    LedBar_GpioInitForDisplay();
    LedBar_OutputOff();
    LedBar_GpioPrepareForStop();
#endif
}

void LedBar_Scan1ms(void)
{
    if (s_ledbar_initialized == 0u)
    {
        LedBar_Init();
    }

#if LEDBAR_DRIVER_GPIO_CHARLIE
    if (s_ledbar_sleep != 0u)
    {
        LedBar_OutputOff();
        LedBar_ResetScanState();
        return;
    }

    LedBar_CommitBackFrameIfPending();
    if (s_ledbar_frame_front.length == 0u)
    {
        LedBar_OutputOff();
        LedBar_ResetScanState();
        return;
    }

    if (s_ledbar_scan_index >= s_ledbar_frame_front.length)
    {
        s_ledbar_scan_index = 0u;
    }

    LedBar_OutputRoute(s_ledbar_frame_front.patterns[s_ledbar_scan_index]);
    s_ledbar_scan_index++;
    if (s_ledbar_scan_index >= s_ledbar_frame_front.length)
    {
        s_ledbar_scan_index = 0u;
    }
#else
    if (s_ledbar_sleep != 0u)
    {
        LedBar_OutputOff();
        LedBar_ResetScanState();
        return;
    }

    if (s_ledbar_frame_front.length == 0u)
    {
        LedBar_CommitBackFrameIfPending();
        if (s_ledbar_frame_front.length == 0u)
        {
            LedBar_OutputOff();
            LedBar_ResetScanState();
            return;
        }
    }

    switch (s_ledbar_scan_state)
    {
    case LEDBAR_SCAN_STATE_OFF_PRE:
        LedBar_OutputOff();
        s_ledbar_scan_state = LEDBAR_SCAN_STATE_OUTPUT_TARGET;
        break;

    case LEDBAR_SCAN_STATE_OUTPUT_TARGET:
        if (s_ledbar_scan_index >= s_ledbar_frame_front.length)
        {
            s_ledbar_scan_index = 0u;
        }
        LedBar_OutputPattern(s_ledbar_frame_front.patterns[s_ledbar_scan_index]);
        s_ledbar_scan_hold_tick = 0u;
        s_ledbar_scan_state = LEDBAR_SCAN_STATE_HOLD_TARGET;
        break;

    case LEDBAR_SCAN_STATE_HOLD_TARGET:
        s_ledbar_scan_hold_tick++;
        if (s_ledbar_scan_hold_tick >= LEDBAR_SCAN_HOLD_TICKS)
        {
            s_ledbar_scan_state = LEDBAR_SCAN_STATE_OFF_POST;
        }
        break;

    case LEDBAR_SCAN_STATE_OFF_POST:
        LedBar_OutputOff();
        s_ledbar_scan_state = LEDBAR_SCAN_STATE_NEXT_ITEM;
        break;

    case LEDBAR_SCAN_STATE_NEXT_ITEM:
    default:
        LedBar_CommitBackFrameIfPending();
        if (s_ledbar_frame_front.length == 0u)
        {
            LedBar_ResetScanState();
            break;
        }

        s_ledbar_scan_index++;
        if (s_ledbar_scan_index >= s_ledbar_frame_front.length)
        {
            s_ledbar_scan_index = 0u;
        }
        s_ledbar_scan_state = LEDBAR_SCAN_STATE_OFF_PRE;
        break;
    }
#endif
}

void TIM4_IRQHandler(void)
{
    if (TIM_GetITStatus(TIM4, TIM_IT_Update) != RESET)
    {
        TIM_ClearITPendingBit(TIM4, TIM_IT_Update);
        LedBar_Scan1ms();
    }
}

static uint8_t LedBar_IsSwitchPressed(void)
{
    return (uint8_t)(MCUI_ENI_DI1 == 0u);
}

static void LedBar_RequestSocDisplayWindow(void)
{
    s_ledbar_soc_display_10ms = LEDBAR_SOC_DISPLAY_10MS;
}

static uint8_t LedBar_IsSocDisplayRequested(void)
{
#if !LEDBAR_SLEEP_ENABLE
    return 1u;
#else
    if (LedBar_IsMcuWakeActive() != 0u)
    {
        return 1u;
    }

    if ((s_ledbar_soc_display_10ms != 0u) || (s_ledbar_key_last_pressed != 0u))
    {
        return 1u;
    }

    return 0u;
#endif
}

#if LEDBAR_LONG_PRESS_GPIO_TOGGLE_TEST
static void LedBar_ToggleLongPressTestOutputs(void)
{
    BitAction dc_state = (GPIO_ReadOutputDataBit(GPIO_DC_EN, PIN_DC_EN) == Bit_RESET) ? Bit_SET : Bit_RESET;
    BitAction en2727_state = (GPIO_ReadOutputDataBit(GPIO_2727_EN, PIN_2737_EN) == Bit_RESET) ? Bit_SET : Bit_RESET;

    GPIO_WriteBit(GPIO_DC_EN, PIN_DC_EN, dc_state);
    GPIO_WriteBit(GPIO_2727_EN, PIN_2737_EN, en2727_state);
}
#endif

static void LedBar_ServiceSwitch(void)
{
    uint8_t pressed = LedBar_IsSwitchPressed();
    uint32_t now_10ms = SysTime_Get10msTickCount();

    if ((pressed != 0u) && (s_ledbar_key_last_pressed == 0u))
    {
        LedBar_RequestSocDisplayWindow();
        s_ledbar_key_press_start_10ms = now_10ms;
        s_ledbar_key_hold_10ms = 0u;
    }
    s_ledbar_key_last_pressed = pressed;

    if (pressed != 0u)
    {
        s_ledbar_key_hold_10ms = now_10ms - s_ledbar_key_press_start_10ms;

#ifdef _DI_SWITCH_longKEY_ONOFF
        if ((s_ledbar_key_hold_10ms >= LEDBAR_KEY_LONG_PRESS_10MS) &&
            (s_ledbar_key_long_handled == 0u))
        {
            s_ledbar_key_long_handled = 1u;
#if LEDBAR_LONG_PRESS_GPIO_TOGGLE_TEST
            LedBar_ToggleLongPressTestOutputs();
#else
            LedBar_SaveSleepSoc();
            entersleep(DEEP_MODE);
            SleepDeal_Continue();
#endif
        }
#endif
    }
    else
    {
        s_ledbar_key_hold_10ms = 0u;
        s_ledbar_key_press_start_10ms = now_10ms;
        s_ledbar_key_long_handled = 0u;
        if (g_st_SysTimeFlag.bits.b1Sys10msFlag == 0u)
        {
            return;
        }
        if (s_ledbar_soc_display_10ms != 0u)
        {
            s_ledbar_soc_display_10ms--;
        }
    }
}

void APP_LedBar(void)
{
    uint8_t display_value;
    uint8_t indicator_mask = LEDBAR_ICON_PERCENT_MASK;
    uint8_t display_requested;
    uint8_t mcu_wk_active;
    uint8_t charge_raw_active;

    if (s_ledbar_initialized == 0u)
    {
        LedBar_Init();
    }

    LedBar_ServiceMcuWakeFilter();
    LedBar_ServiceSwitch();
    mcu_wk_active = LedBar_IsMcuWakeActive();

#if LEDBAR_SLEEP_ENABLE
    if (SystemStatus.bits.b1StartUpBMS != 0u)
    {
        LedBar_Command = LED_BAR_STARTUP;
        LedBar_SetSleep(1u);
        return;
    }

    if ((Sleep_Mode.bits.b1_ToSleepFlag != 0u) && (mcu_wk_active == 0u))
    {
        LedBar_SaveSleepSoc();
        LedBar_SetSleep(1u);
        return;
    }
#endif

    if (s_ledbar_test_single_segment_enable != 0u)
    {
        if (s_ledbar_sleep != 0u)
        {
            LedBar_Wakeup();
        }
        LedBar_Command = LED_BAR_NORMAL;
        return;
    }

    display_requested = LedBar_IsSocDisplayRequested();
    if (display_requested == 0u)
    {
        if ((s_ledbar_force_blank == 0u) ||
            (s_ledbar_frame_front.length != 0u) ||
            (s_ledbar_scan_timer_enabled != 0u))
        {
            LedBar_Clear();
        }
        return;
    }

    if (s_ledbar_sleep != 0u)
    {
        LedBar_Wakeup();
    }

    if ((g_st_SysTimeFlag.bits.b1Sys100msFlag == 0u) &&
        (s_ledbar_force_blank == 0u))
    {
        return;
    }

    display_value = (uint8_t)g_stCellInfoReport.SocElement.u16Soc;
    if (display_value > 100u)
    {
        display_value = 100u;
    }

    charge_raw_active = (uint8_t)(((g_stCellInfoReport.u16Ichg != 0u) || (mcu_wk_active != 0u)) ? 1u : 0u);
    LedBar_ServiceChargeDisplayFilter(charge_raw_active);
    if (s_ledbar_charge_active != 0u)
    {
        indicator_mask |= LEDBAR_ICON_CHARGE_MASK;
        LedBar_Command = LED_BAR_CHG;
    }
    else if (g_stCellInfoReport.u16IDischg != 0u)
    {
        LedBar_Command = LED_BAR_DSG;
    }
    else
    {
        LedBar_Command = LED_BAR_NORMAL;
    }

    if (((g_stCellInfoReport.unMdlFault_Third.all & 0x3FFBu) != 0u) ||
        (System_ERROR_UserCallback(ERROR_STATUS_TEMP_BREAK) != 0u) ||
        (System_ERROR_UserCallback(ERROR_STATUS_CBC_DSG) != 0u))
    {
        LedBar_Command = LED_BAR_FAULT;
    }

    if ((s_ledbar_number != display_value) ||
        (s_ledbar_indicator_mask != indicator_mask) ||
        (s_ledbar_force_blank != 0u))
    {
        s_ledbar_number = display_value;
        s_ledbar_indicator_mask = indicator_mask;
        s_ledbar_force_blank = 0u;
        LedBar_RebuildFrame();
        LedBar_ShowFrontFrameNow();
    }
}
