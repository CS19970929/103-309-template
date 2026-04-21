#include "display_74hc595.h"

/*
 * 你只需要替换下面这几个底层函数
 * 例如用 STM32 标准库 / HAL / 直接寄存器都可以
 */
static void disp595_hw_set_data(uint8_t level)
{
    (void)level;
    /* TODO: 替换成你的 DATA 引脚输出 */
}

static void disp595_hw_set_clk(uint8_t level)
{
    (void)level;
    /* TODO: 替换成你的 CLK 引脚输出 */
}

static void disp595_hw_set_latch(uint8_t level)
{
    (void)level;
    /* TODO: 替换成你的 LATCH 引脚输出 */
}

static void disp595_hw_delay_small(void)
{
    volatile uint8_t i;
    for (i = 0; i < 8; i++)
    {
        __asm volatile ("nop");
    }
}

void disp595_init(void)
{
    disp595_hw_set_data(0);
    disp595_hw_set_clk(0);
    disp595_hw_set_latch(0);
    disp595_clear_all();
}

void disp595_write_byte(uint8_t data)
{
    uint8_t i;

    for (i = 0; i < 8; i++)
    {
        if (data & 0x80u)
        {
            disp595_hw_set_data(1);
        }
        else
        {
            disp595_hw_set_data(0);
        }

        disp595_hw_delay_small();
        disp595_hw_set_clk(1);
        disp595_hw_delay_small();
        disp595_hw_set_clk(0);

        data <<= 1;
    }

    disp595_hw_set_latch(1);
    disp595_hw_delay_small();
    disp595_hw_set_latch(0);
}

void disp595_clear_all(void)
{
    disp595_write_byte(0x00u);
}
