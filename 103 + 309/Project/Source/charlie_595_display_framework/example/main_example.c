#include "display_charlie.h"

/*
 * 示例：
 * - 先初始化
 * - 在1ms定时中断或主循环节拍中调用 display_scan_task_1ms()
 * - 周期性更新显示值
 */

static uint16_t s_ms = 0u;
static uint8_t s_soc = 0u;

void app_init(void)
{
    display_init();
    display_set_value(0, 0, 1);
}

void app_1ms_task(void)
{
    display_scan_task_1ms();

    s_ms++;
    if (s_ms >= 500u)
    {
        s_ms = 0u;

        s_soc++;
        if (s_soc > 100u)
        {
            s_soc = 0u;
        }

        display_set_value(s_soc, (s_soc & 0x01u), 1u);
    }
}
