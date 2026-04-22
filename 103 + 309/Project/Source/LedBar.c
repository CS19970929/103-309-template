#include "main.h"
#include <string.h>

#define LEDBAR_FRAME_PATTERN_COUNT 5u

typedef struct
{
    uint8_t low_pin;
    uint8_t high_pin;
} LedBarRoute;

typedef struct
{
    uint32_t lit_mask;
} LedBarPattern;

typedef struct
{
    uint8_t patterns[LEDBAR_FRAME_PATTERN_COUNT];
    uint8_t length;
} LedBarFrameBuffer;

typedef enum
{
    LEDBAR_SCAN_STATE_OFF_PRE = 0,
    LEDBAR_SCAN_STATE_OUTPUT_TARGET,
    LEDBAR_SCAN_STATE_HOLD_TARGET,
    LEDBAR_SCAN_STATE_OFF_POST,
    LEDBAR_SCAN_STATE_NEXT_ITEM
} LedBarScanState;

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

#define LEDBAR_DIGIT_BIT_A         (1u << 0)
#define LEDBAR_DIGIT_BIT_B         (1u << 1)
#define LEDBAR_DIGIT_BIT_C         (1u << 2)
#define LEDBAR_DIGIT_BIT_D         (1u << 3)
#define LEDBAR_DIGIT_BIT_E         (1u << 4)
#define LEDBAR_DIGIT_BIT_F         (1u << 5)
#define LEDBAR_DIGIT_BIT_G         (1u << 6)
#define LEDBAR_PATTERN_MASK_MAX    30u
#define LEDBAR_SCAN_HOLD_TICKS     1u

#define LEDBAR_595_GPIO_DATA  GPIO_LED595_DATA
#define LEDBAR_595_PIN_DATA   PIN_LED595_DATA
#define LEDBAR_595_GPIO_CLK   GPIO_LED595_CLK
#define LEDBAR_595_PIN_CLK    PIN_LED595_CLK
#define LEDBAR_595_GPIO_LATCH GPIO_LED595_LATCH
#define LEDBAR_595_PIN_LATCH  PIN_LED595_LATCH

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

/*
 * These tables are generated against the current schematic:
 * QA~QE drive D0~D4 directly and OE is always enabled.
 * Each row is the shortest 74HC595 pattern sequence that covers the target
 * display while minimizing unavoidable extra lit segments.
 */
static const uint8_t s_ledbar_soc_patterns[101][5] =
{
    {1, 14, 0, 0, 0}, /* 000, extra=1 */
    {2, 3, 0, 0, 0}, /* 001, extra=3 */
    {1, 14, 30, 0, 0}, /* 002, extra=3 */
    {1, 14, 30, 0, 0}, /* 003, extra=3 */
    {26, 11, 0, 0, 0}, /* 004, extra=3 */
    {1, 14, 30, 0, 0}, /* 005, extra=3 */
    {1, 14, 30, 0, 0}, /* 006, extra=2 */
    {1, 2, 0, 0, 0}, /* 007, extra=3 */
    {1, 14, 30, 0, 0}, /* 008, extra=1 */
    {1, 14, 30, 0, 0}, /* 009, extra=2 */
    {12, 8, 1, 14, 0}, /* 010, extra=2 */
    {4, 10, 11, 0, 0}, /* 011, extra=5 */
    {5, 10, 30, 0, 0}, /* 012, extra=4 */
    {12, 9, 14, 30, 0}, /* 013, extra=3 */
    {4, 11, 30, 0, 0}, /* 014, extra=4 */
    {28, 9, 11, 0, 0}, /* 015, extra=3 */
    {28, 11, 1, 0, 0}, /* 016, extra=3 */
    {12, 9, 14, 0, 0}, /* 017, extra=4 */
    {12, 8, 1, 14, 30}, /* 018, extra=2 */
    {12, 9, 14, 30, 0}, /* 019, extra=2 */
    {2, 12, 17, 0, 0}, /* 020, extra=3 */
    {2, 29, 17, 0, 0}, /* 021, extra=4 */
    {2, 20, 29, 21, 0}, /* 022, extra=2 */
    {2, 20, 9, 0, 0}, /* 023, extra=3 */
    {10, 20, 8, 11, 0}, /* 024, extra=3 */
    {11, 20, 8, 9, 0}, /* 025, extra=2 */
    {3, 21, 28, 0, 0}, /* 026, extra=2 */
    {2, 29, 17, 0, 0}, /* 027, extra=3 */
    {2, 28, 17, 0, 0}, /* 028, extra=2 */
    {10, 20, 9, 0, 0}, /* 029, extra=2 */
    {10, 12, 17, 0, 0}, /* 030, extra=3 */
    {2, 21, 9, 0, 0}, /* 031, extra=5 */
    {10, 21, 28, 0, 0}, /* 032, extra=3 */
    {2, 20, 9, 0, 0}, /* 033, extra=3 */
    {10, 20, 8, 11, 0}, /* 034, extra=3 */
    {11, 20, 8, 9, 0}, /* 035, extra=2 */
    {11, 21, 28, 0, 0}, /* 036, extra=2 */
    {2, 21, 9, 0, 0}, /* 037, extra=4 */
    {10, 28, 17, 0, 0}, /* 038, extra=2 */
    {10, 20, 9, 0, 0}, /* 039, extra=2 */
    {12, 8, 17, 14, 0}, /* 040, extra=3 */
    {4, 11, 18, 0, 0}, /* 041, extra=6 */
    {20, 10, 16, 21, 0}, /* 042, extra=4 */
    {12, 9, 16, 14, 0}, /* 043, extra=4 */
    {20, 10, 27, 0, 0}, /* 044, extra=4 */
    {12, 9, 16, 15, 0}, /* 045, extra=3 */
    {28, 11, 17, 0, 0}, /* 046, extra=3 */
    {13, 11, 18, 0, 0}, /* 047, extra=5 */
    {20, 10, 17, 0, 0}, /* 048, extra=3 */
    {12, 9, 16, 14, 0}, /* 049, extra=3 */
    {19, 9, 14, 0, 0}, /* 050, extra=2 */
    {2, 8, 19, 0, 0}, /* 051, extra=3 */
    {2, 9, 19, 22, 0}, /* 052, extra=3 */
    {2, 9, 18, 22, 0}, /* 053, extra=2 */
    {11, 8, 18, 0, 0}, /* 054, extra=1 */
    {2, 9, 18, 30, 0}, /* 055, extra=2 */
    {11, 28, 17, 0, 0}, /* 056, extra=2 */
    {2, 9, 18, 0, 0}, /* 057, extra=2 */
    {2, 9, 19, 30, 0}, /* 058, extra=1 */
    {2, 9, 18, 30, 0}, /* 059, extra=1 */
    {10, 8, 17, 14, 0}, /* 060, extra=1 */
    {10, 8, 16, 11, 0}, /* 061, extra=2 */
    {10, 8, 17, 30, 0}, /* 062, extra=2 */
    {10, 9, 16, 30, 0}, /* 063, extra=1 */
    {10, 8, 16, 11, 0}, /* 064, extra=0 */
    {10, 9, 16, 30, 0}, /* 065, extra=1 */
    {11, 28, 17, 0, 0}, /* 066, extra=1 */
    {2, 9, 16, 0, 0}, /* 067, extra=2 */
    {10, 8, 17, 30, 0}, /* 068, extra=0 */
    {10, 9, 16, 30, 0}, /* 069, extra=0 */
    {10, 4, 1, 0, 0}, /* 070, extra=2 */
    {2, 4, 11, 0, 0}, /* 071, extra=4 */
    {10, 5, 30, 0, 0}, /* 072, extra=3 */
    {10, 28, 9, 0, 0}, /* 073, extra=3 */
    {10, 28, 11, 0, 0}, /* 074, extra=3 */
    {11, 28, 9, 0, 0}, /* 075, extra=2 */
    {11, 28, 1, 0, 0}, /* 076, extra=2 */
    {2, 13, 9, 0, 0}, /* 077, extra=3 */
    {10, 28, 1, 0, 0}, /* 078, extra=2 */
    {10, 28, 9, 0, 0}, /* 079, extra=2 */
    {10, 12, 17, 0, 0}, /* 080, extra=1 */
    {10, 29, 17, 0, 0}, /* 081, extra=3 */
    {10, 28, 17, 0, 0}, /* 082, extra=2 */
    {10, 28, 16, 9, 0}, /* 083, extra=1 */
    {10, 28, 16, 11, 0}, /* 084, extra=1 */
    {11, 28, 16, 9, 0}, /* 085, extra=0 */
    {11, 28, 17, 0, 0}, /* 086, extra=0 */
    {10, 29, 17, 0, 0}, /* 087, extra=2 */
    {10, 28, 17, 0, 0}, /* 088, extra=0 */
    {10, 28, 16, 9, 0}, /* 089, extra=0 */
    {10, 12, 17, 0, 0}, /* 090, extra=2 */
    {10, 29, 17, 0, 0}, /* 091, extra=4 */
    {10, 28, 17, 0, 0}, /* 092, extra=3 */
    {10, 28, 16, 9, 0}, /* 093, extra=2 */
    {10, 28, 16, 11, 0}, /* 094, extra=2 */
    {11, 28, 16, 9, 0}, /* 095, extra=1 */
    {11, 28, 17, 0, 0}, /* 096, extra=1 */
    {10, 29, 17, 0, 0}, /* 097, extra=3 */
    {10, 28, 17, 0, 0}, /* 098, extra=1 */
    {10, 28, 16, 9, 0}, /* 099, extra=1 */
    {2, 5, 25, 12, 0}, /* 100, extra=1 */
};

static const uint8_t s_ledbar_soc_charge_patterns[101][5] =
{
    {1, 14, 0, 0, 0}, /* 000, extra=0 */
    {2, 3, 15, 0, 0}, /* 001, extra=3 */
    {1, 14, 30, 0, 0}, /* 002, extra=2 */
    {1, 14, 30, 0, 0}, /* 003, extra=2 */
    {14, 1, 30, 0, 0}, /* 004, extra=3 */
    {1, 14, 30, 0, 0}, /* 005, extra=2 */
    {1, 14, 30, 0, 0}, /* 006, extra=1 */
    {1, 14, 0, 0, 0}, /* 007, extra=3 */
    {1, 14, 30, 0, 0}, /* 008, extra=0 */
    {1, 14, 30, 0, 0}, /* 009, extra=1 */
    {12, 8, 1, 14, 0}, /* 010, extra=1 */
    {4, 10, 11, 0, 0}, /* 011, extra=4 */
    {5, 10, 30, 0, 0}, /* 012, extra=3 */
    {12, 9, 14, 30, 0}, /* 013, extra=2 */
    {4, 11, 30, 0, 0}, /* 014, extra=3 */
    {28, 9, 15, 0, 0}, /* 015, extra=2 */
    {28, 8, 1, 15, 0}, /* 016, extra=2 */
    {12, 9, 14, 0, 0}, /* 017, extra=3 */
    {12, 8, 1, 14, 30}, /* 018, extra=1 */
    {12, 9, 14, 30, 0}, /* 019, extra=1 */
    {2, 12, 17, 0, 0}, /* 020, extra=2 */
    {2, 13, 17, 0, 0}, /* 021, extra=4 */
    {2, 5, 13, 20, 0}, /* 022, extra=2 */
    {2, 4, 9, 20, 0}, /* 023, extra=3 */
    {10, 12, 16, 11, 0}, /* 024, extra=3 */
    {11, 12, 16, 9, 0}, /* 025, extra=2 */
    {3, 4, 28, 21, 0}, /* 026, extra=2 */
    {2, 13, 17, 0, 0}, /* 027, extra=3 */
    {2, 12, 16, 1, 0}, /* 028, extra=2 */
    {10, 4, 9, 20, 0}, /* 029, extra=2 */
    {10, 12, 17, 0, 0}, /* 030, extra=2 */
    {11, 13, 18, 0, 0}, /* 031, extra=4 */
    {2, 5, 8, 22, 0}, /* 032, extra=3 */
    {11, 13, 22, 0, 0}, /* 033, extra=2 */
    {11, 12, 18, 0, 0}, /* 034, extra=3 */
    {11, 12, 16, 9, 0}, /* 035, extra=2 */
    {11, 4, 28, 21, 0}, /* 036, extra=2 */
    {11, 13, 18, 0, 0}, /* 037, extra=3 */
    {3, 5, 8, 22, 0}, /* 038, extra=2 */
    {2, 12, 9, 18, 0}, /* 039, extra=2 */
    {12, 8, 17, 14, 0}, /* 040, extra=2 */
    {4, 11, 18, 0, 0}, /* 041, extra=5 */
    {21, 24, 14, 0, 0}, /* 042, extra=4 */
    {12, 9, 16, 14, 0}, /* 043, extra=3 */
    {12, 11, 18, 0, 0}, /* 044, extra=4 */
    {12, 9, 16, 15, 0}, /* 045, extra=2 */
    {12, 24, 17, 15, 0}, /* 046, extra=2 */
    {13, 11, 18, 0, 0}, /* 047, extra=4 */
    {12, 24, 17, 14, 0}, /* 048, extra=2 */
    {12, 9, 16, 14, 0}, /* 049, extra=2 */
    {19, 9, 14, 0, 0}, /* 050, extra=1 */
    {18, 9, 15, 0, 0}, /* 051, extra=3 */
    {3, 9, 18, 6, 0}, /* 052, extra=3 */
    {18, 9, 6, 0, 0}, /* 053, extra=2 */
    {11, 8, 18, 15, 0}, /* 054, extra=1 */
    {18, 9, 14, 0, 0}, /* 055, extra=2 */
    {3, 9, 18, 14, 0}, /* 056, extra=2 */
    {18, 9, 15, 0, 0}, /* 057, extra=2 */
    {3, 9, 18, 14, 0}, /* 058, extra=1 */
    {18, 9, 14, 0, 0}, /* 059, extra=1 */
    {10, 8, 17, 14, 0}, /* 060, extra=0 */
    {10, 8, 16, 11, 15}, /* 061, extra=2 */
    {10, 24, 17, 14, 0}, /* 062, extra=2 */
    {10, 9, 16, 14, 0}, /* 063, extra=1 */
    {10, 8, 16, 11, 15}, /* 064, extra=0 */
    {10, 9, 16, 14, 0}, /* 065, extra=1 */
    {10, 24, 17, 14, 0}, /* 066, extra=1 */
    {18, 25, 15, 0, 0}, /* 067, extra=2 */
    {10, 24, 17, 14, 0}, /* 068, extra=0 */
    {10, 9, 16, 14, 0}, /* 069, extra=0 */
    {10, 4, 1, 0, 0}, /* 070, extra=1 */
    {2, 4, 11, 0, 0}, /* 071, extra=3 */
    {10, 5, 30, 0, 0}, /* 072, extra=2 */
    {11, 13, 30, 0, 0}, /* 073, extra=2 */
    {11, 4, 30, 0, 0}, /* 074, extra=2 */
    {11, 13, 28, 0, 0}, /* 075, extra=2 */
    {11, 5, 30, 0, 0}, /* 076, extra=2 */
    {2, 13, 9, 0, 0}, /* 077, extra=2 */
    {11, 5, 30, 0, 0}, /* 078, extra=1 */
    {11, 13, 30, 0, 0}, /* 079, extra=1 */
    {10, 12, 17, 0, 0}, /* 080, extra=0 */
    {10, 13, 17, 0, 0}, /* 081, extra=3 */
    {10, 4, 24, 21, 0}, /* 082, extra=2 */
    {10, 12, 16, 9, 0}, /* 083, extra=1 */
    {10, 12, 16, 11, 0}, /* 084, extra=1 */
    {11, 12, 16, 9, 0}, /* 085, extra=0 */
    {11, 12, 16, 1, 0}, /* 086, extra=0 */
    {10, 13, 17, 0, 0}, /* 087, extra=2 */
    {10, 12, 16, 1, 0}, /* 088, extra=0 */
    {10, 12, 16, 9, 0}, /* 089, extra=0 */
    {10, 12, 17, 0, 0}, /* 090, extra=1 */
    {11, 13, 18, 0, 0}, /* 091, extra=3 */
    {3, 4, 9, 18, 0}, /* 092, extra=3 */
    {2, 4, 9, 18, 0}, /* 093, extra=2 */
    {11, 12, 18, 0, 0}, /* 094, extra=2 */
    {11, 12, 16, 9, 0}, /* 095, extra=1 */
    {11, 12, 16, 1, 0}, /* 096, extra=1 */
    {11, 13, 18, 0, 0}, /* 097, extra=2 */
    {3, 12, 9, 18, 0}, /* 098, extra=1 */
    {2, 12, 9, 18, 0}, /* 099, extra=1 */
    {2, 5, 25, 12, 0}, /* 100, extra=0 */
};

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
static uint8_t s_ledbar_scan_hold_tick = 0u;
static LedBarScanState s_ledbar_scan_state = LEDBAR_SCAN_STATE_OFF_PRE;
static uint8_t s_ledbar_last_595_value = 0xFFu;
static LedBarPattern s_ledbar_patterns[LEDBAR_PATTERN_MASK_MAX + 1u];

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

/* 鐗╃悊灞傦細74HC595 寮曡剼鏃跺簭 */
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

/* 涓氬姟灞傦細SOC/鍥炬爣 -> 璺敱鎺╃爜 */
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
        if ((digit_mask & LEDBAR_DIGIT_BIT_A) != 0u) { target_mask |= (1UL << LEDBAR_ROUTE_TENS_A); }
        if ((digit_mask & LEDBAR_DIGIT_BIT_B) != 0u) { target_mask |= (1UL << LEDBAR_ROUTE_TENS_B); }
        if ((digit_mask & LEDBAR_DIGIT_BIT_C) != 0u) { target_mask |= (1UL << LEDBAR_ROUTE_TENS_C); }
        if ((digit_mask & LEDBAR_DIGIT_BIT_D) != 0u) { target_mask |= (1UL << LEDBAR_ROUTE_TENS_D); }
        if ((digit_mask & LEDBAR_DIGIT_BIT_E) != 0u) { target_mask |= (1UL << LEDBAR_ROUTE_TENS_E); }
        if ((digit_mask & LEDBAR_DIGIT_BIT_F) != 0u) { target_mask |= (1UL << LEDBAR_ROUTE_TENS_F); }
        if ((digit_mask & LEDBAR_DIGIT_BIT_G) != 0u) { target_mask |= (1UL << LEDBAR_ROUTE_TENS_G); }
    }

    digit_mask = s_ledbar_digit_map[ones];
    if ((digit_mask & LEDBAR_DIGIT_BIT_A) != 0u) { target_mask |= (1UL << LEDBAR_ROUTE_ONES_A); }
    if ((digit_mask & LEDBAR_DIGIT_BIT_B) != 0u) { target_mask |= (1UL << LEDBAR_ROUTE_ONES_B); }
    if ((digit_mask & LEDBAR_DIGIT_BIT_C) != 0u) { target_mask |= (1UL << LEDBAR_ROUTE_ONES_C); }
    if ((digit_mask & LEDBAR_DIGIT_BIT_D) != 0u) { target_mask |= (1UL << LEDBAR_ROUTE_ONES_D); }
    if ((digit_mask & LEDBAR_DIGIT_BIT_E) != 0u) { target_mask |= (1UL << LEDBAR_ROUTE_ONES_E); }
    if ((digit_mask & LEDBAR_DIGIT_BIT_F) != 0u) { target_mask |= (1UL << LEDBAR_ROUTE_ONES_F); }
    if ((digit_mask & LEDBAR_DIGIT_BIT_G) != 0u) { target_mask |= (1UL << LEDBAR_ROUTE_ONES_G); }

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

static void LedBar_CopyFramePatternsToBuffer(LedBarFrameBuffer *frame, const uint8_t *patterns)
{
    uint8_t index;

    LedBar_ClearFrameBuffer(frame);
    for (index = 0u; index < LEDBAR_FRAME_PATTERN_COUNT; ++index)
    {
        if (patterns[index] == 0u)
        {
            break;
        }

        frame->patterns[frame->length] = patterns[index];
        frame->length++;
    }
}

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

static void LedBar_ResetScanState(void)
{
    s_ledbar_scan_hold_tick = 0u;
    s_ledbar_scan_state = LEDBAR_SCAN_STATE_OFF_PRE;
    if (s_ledbar_scan_index >= s_ledbar_frame_front.length)
    {
        s_ledbar_scan_index = 0u;
    }
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

/* 涓氬姟灞傦細閲嶅缓 back buffer锛屾壂鎻忓眰鍦ㄥ畨鍏ㄧ偣鍒囨崲鍒?front buffer */
static void LedBar_RebuildFrame(void)
{
    uint8_t value = s_ledbar_number;
    uint8_t indicator_mask = (uint8_t)(s_ledbar_indicator_mask & (LEDBAR_ICON_CHARGE_MASK | LEDBAR_ICON_PERCENT_MASK));

    if (value > 100u)
    {
        value = 100u;
    }

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
}

void LedBar_Init(void)
{
    if (s_ledbar_initialized != 0u)
    {
        return;
    }

    LedBar_InitPatternTable();
    s_ledbar_number = 0u;
    s_ledbar_indicator_mask = LEDBAR_ICON_PERCENT_MASK;
    s_ledbar_force_blank = 0u;
    s_ledbar_sleep = 0u;
    s_ledbar_test_single_segment_enable = 0u;
    s_ledbar_test_single_segment_id = 0u;
    LedBar_ClearFrameBuffer(&s_ledbar_frame_front);
    LedBar_ClearFrameBuffer(&s_ledbar_frame_back);
    s_ledbar_frame_pending = 0u;
    s_ledbar_scan_index = 0u;
    s_ledbar_scan_hold_tick = 0u;
    s_ledbar_scan_state = LEDBAR_SCAN_STATE_OFF_PRE;
    s_ledbar_last_595_value = 0xFFu;
    LedBar_Command = LED_BAR_NORMAL;

    LedBar_RebuildFrame();
    LedBar_CommitBackFrameIfPending();
    LedBar_OutputOff();
    s_ledbar_initialized = 1u;
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
}

void LedBar_SetSleep(uint8_t enable)
{
    if (s_ledbar_initialized == 0u)
    {
        LedBar_Init();
    }

    enable = (enable != 0u) ? 1u : 0u;
    if (s_ledbar_sleep == enable)
    {
        return;
    }

    s_ledbar_sleep = enable;
    LedBar_RebuildFrame();
    if (s_ledbar_sleep != 0u)
    {
        LedBar_CommitBackFrameIfPending();
        LedBar_OutputOff();
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
}

void LedBar_Scan1ms(void)
{
    if (s_ledbar_initialized == 0u)
    {
        LedBar_Init();
    }

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
}

void APP_LedBar(void)
{
    uint8_t display_value;
    uint8_t indicator_mask = LEDBAR_ICON_PERCENT_MASK;

    if (s_ledbar_initialized == 0u)
    {
        LedBar_Init();
    }

    if (SystemStatus.bits.b1StartUpBMS != 0u)
    {
        LedBar_Command = LED_BAR_STARTUP;
        LedBar_SetSleep(1u);
    }
    else
    {
        /* TODO(确认): b1_ToSleepFlag 与真实睡眠动作时序需上板确认。当前仅用于显示熄屏联动。 */
        if (Sleep_Mode.bits.b1_ToSleepFlag != 0u)
        {
            LedBar_SetSleep(1u);
        }
        else if (s_ledbar_sleep != 0u)
        {
            LedBar_Wakeup();
        }
    }

    if (g_st_SysTimeFlag.bits.b1Sys1msFlag != 0u)
    {
        LedBar_Scan1ms();
    }

    if (s_ledbar_sleep != 0u)
    {
        return;
    }

    if (s_ledbar_test_single_segment_enable != 0u)
    {
        LedBar_Command = LED_BAR_NORMAL;
        return;
    }

    if (g_st_SysTimeFlag.bits.b1Sys100msFlag == 0u)
    {
        return;
    }

    display_value = (uint8_t)g_stCellInfoReport.SocElement.u16Soc;
    if (display_value > 100u)
    {
        display_value = 100u;
    }

    if (g_stCellInfoReport.u16Ichg != 0u)
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
    }
}
