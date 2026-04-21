#include "display_74hc595.h"
#include "display_types.h"

/*
 * 可选调试文件。
 * 你可以把这里的两个函数并到正式模块里。
 * 用于现场快速确定映射关系。
 */

extern void display_debug_set_all_hiz(void);
extern void display_debug_set_pin_mode(charlie_pin_t pin, charlie_pin_mode_t mode);
extern void display_debug_set_pin_write(charlie_pin_t pin, uint8_t level);

void display_debug_light_route(uint8_t sel, uint8_t hi_pin, uint8_t lo_pin)
{
    display_debug_set_all_hiz();
    disp595_write_byte((uint8_t)(1u << sel));

    display_debug_set_pin_mode((charlie_pin_t)hi_pin, CHARLIE_PIN_MODE_OUT);
    display_debug_set_pin_mode((charlie_pin_t)lo_pin, CHARLIE_PIN_MODE_OUT);
    display_debug_set_pin_write((charlie_pin_t)hi_pin, 1u);
    display_debug_set_pin_write((charlie_pin_t)lo_pin, 0u);
}
