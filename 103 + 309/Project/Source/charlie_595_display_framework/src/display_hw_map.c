#include "display_hw_map.h"

/*
 * 映射表填写规则：
 * { 595位选bit, 阳极线, 阴极线, 有效标志 }
 *
 * 其中：
 * - 595位选bit：0~7，对应74HC595的Q0~Q7
 * - 阳极线 / 阴极线：0~4，对应5根Charlie线
 * - valid=1 表示有效
 *
 * !!! 下面这些是示例，不一定符合你的硬件 !!!
 * 你需要按你实际接线改掉
 */
static const display_led_route_t s_led_route_table[DISP_LED_MAX] =
{
    [DISP_LED_HUNDREDS_1] = {0, 0, 1, 1},

    [DISP_LED_TENS_A] = {1, 0, 1, 1},
    [DISP_LED_TENS_B] = {1, 0, 2, 1},
    [DISP_LED_TENS_C] = {1, 0, 3, 1},
    [DISP_LED_TENS_D] = {1, 0, 4, 1},
    [DISP_LED_TENS_E] = {1, 1, 2, 1},
    [DISP_LED_TENS_F] = {1, 1, 3, 1},
    [DISP_LED_TENS_G] = {1, 1, 4, 1},

    [DISP_LED_ONES_A] = {2, 0, 1, 1},
    [DISP_LED_ONES_B] = {2, 0, 2, 1},
    [DISP_LED_ONES_C] = {2, 0, 3, 1},
    [DISP_LED_ONES_D] = {2, 0, 4, 1},
    [DISP_LED_ONES_E] = {2, 2, 1, 1},
    [DISP_LED_ONES_F] = {2, 3, 1, 1},
    [DISP_LED_ONES_G] = {2, 4, 1, 1},

    [DISP_LED_ICON_CHARGE]  = {3, 2, 3, 1},
    [DISP_LED_ICON_PERCENT] = {4, 3, 4, 1},
};

const display_led_route_t* display_hw_get_led_route(display_led_id_t id)
{
    if ((uint32_t)id < (uint32_t)DISP_LED_MAX)
    {
        return &s_led_route_table[id];
    }

    return 0;
}
