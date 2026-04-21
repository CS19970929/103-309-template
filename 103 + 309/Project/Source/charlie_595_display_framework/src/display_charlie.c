#include "display_charlie.h"
#include "display_74hc595.h"
#include "display_segmap.h"
#include "display_hw_map.h"

static display_content_t s_content;

static void charlie_hw_pin_mode(charlie_pin_t pin, charlie_pin_mode_t mode)
{
    (void)pin;
    (void)mode;
    /* TODO:
     * mode == CHARLIE_PIN_MODE_HIZ : 配成输入/高阻
     * mode == CHARLIE_PIN_MODE_OUT : 配成推挽输出
     */
}

static void charlie_hw_pin_write(charlie_pin_t pin, uint8_t level)
{
    (void)pin;
    (void)level;
    /* TODO: 输出高低电平 */
}

static void charlie_all_hiz(void)
{
    uint8_t i;
    for (i = 0; i < (uint8_t)CHARLIE_PIN_MAX; i++)
    {
        charlie_hw_pin_mode((charlie_pin_t)i, CHARLIE_PIN_MODE_HIZ);
    }
}

static void display_enable_595_sel(uint8_t bit_index)
{
    uint8_t value = 0u;

    if (bit_index < 8u)
    {
        value = (uint8_t)(1u << bit_index);
    }

    disp595_write_byte(value);
}

static void display_disable_all_output(void)
{
    charlie_all_hiz();
    disp595_clear_all();
}

static void display_light_one_led(display_led_id_t led_id)
{
    const display_led_route_t* route;

    route = display_hw_get_led_route(led_id);
    if ((route == 0) || (route->valid == 0u))
    {
        display_disable_all_output();
        return;
    }

    /* 先全部关掉，避免串亮 */
    display_disable_all_output();

    /* 先选通 595 */
    display_enable_595_sel(route->sel_595_bit);

    /* 再配置 Charlie 两根线 */
    charlie_hw_pin_mode((charlie_pin_t)route->anode_pin, CHARLIE_PIN_MODE_OUT);
    charlie_hw_pin_mode((charlie_pin_t)route->cathode_pin, CHARLIE_PIN_MODE_OUT);

    charlie_hw_pin_write((charlie_pin_t)route->anode_pin, 1u);
    charlie_hw_pin_write((charlie_pin_t)route->cathode_pin, 0u);
}

static uint8_t display_should_turn_on_led(display_led_id_t led_id)
{
    uint8_t tens;
    uint8_t ones;
    uint8_t mask_tens;
    uint8_t mask_ones;

    tens = (uint8_t)((s_content.value / 10u) % 10u);
    ones = (uint8_t)(s_content.value % 10u);

    mask_tens = display_digit_to_7seg_mask(tens);
    mask_ones = display_digit_to_7seg_mask(ones);

    switch (led_id)
    {
        case DISP_LED_HUNDREDS_1:
            return (s_content.value >= 100u) ? 1u : 0u;

        case DISP_LED_TENS_A: return (mask_tens & (1u << 0)) ? 1u : 0u;
        case DISP_LED_TENS_B: return (mask_tens & (1u << 1)) ? 1u : 0u;
        case DISP_LED_TENS_C: return (mask_tens & (1u << 2)) ? 1u : 0u;
        case DISP_LED_TENS_D: return (mask_tens & (1u << 3)) ? 1u : 0u;
        case DISP_LED_TENS_E: return (mask_tens & (1u << 4)) ? 1u : 0u;
        case DISP_LED_TENS_F: return (mask_tens & (1u << 5)) ? 1u : 0u;
        case DISP_LED_TENS_G: return (mask_tens & (1u << 6)) ? 1u : 0u;

        case DISP_LED_ONES_A: return (mask_ones & (1u << 0)) ? 1u : 0u;
        case DISP_LED_ONES_B: return (mask_ones & (1u << 1)) ? 1u : 0u;
        case DISP_LED_ONES_C: return (mask_ones & (1u << 2)) ? 1u : 0u;
        case DISP_LED_ONES_D: return (mask_ones & (1u << 3)) ? 1u : 0u;
        case DISP_LED_ONES_E: return (mask_ones & (1u << 4)) ? 1u : 0u;
        case DISP_LED_ONES_F: return (mask_ones & (1u << 5)) ? 1u : 0u;
        case DISP_LED_ONES_G: return (mask_ones & (1u << 6)) ? 1u : 0u;

        case DISP_LED_ICON_CHARGE:
            return s_content.charge_on ? 1u : 0u;

        case DISP_LED_ICON_PERCENT:
            return s_content.percent_on ? 1u : 0u;

        default:
            return 0u;
    }
}

void display_init(void)
{
    s_content.value = 0u;
    s_content.charge_on = 0u;
    s_content.percent_on = 1u;

    disp595_init();
    display_disable_all_output();
}

void display_set_value(uint8_t value, uint8_t charge_on, uint8_t percent_on)
{
    if (value > 100u)
    {
        value = 100u;
    }

    s_content.value = value;
    s_content.charge_on = charge_on ? 1u : 0u;
    s_content.percent_on = percent_on ? 1u : 0u;
}

void display_scan_task_1ms(void)
{
    static uint8_t scan_index = 0u;
    uint8_t try_count = 0u;
    display_led_id_t led_id;

    /* 从当前索引开始，找到下一个需要点亮的LED */
    while (try_count < (uint8_t)DISP_LED_MAX)
    {
        led_id = (display_led_id_t)scan_index;

        scan_index++;
        if (scan_index >= (uint8_t)DISP_LED_MAX)
        {
            scan_index = 0u;
        }

        if (display_should_turn_on_led(led_id) != 0u)
        {
            display_light_one_led(led_id);
            return;
        }

        try_count++;
    }

    display_disable_all_output();
}
