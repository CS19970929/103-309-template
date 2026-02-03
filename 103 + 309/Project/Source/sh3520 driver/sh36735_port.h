#ifndef __SH36735_PORT_H__
#define __SH36735_PORT_H__

/*
 * 这里是移植层：你可以把这些函数/宏改成你工程里现有的 BSP。
 * 下面给出 STM32F103 + Standard Peripheral Library 的可直接用的参考实现。
 */

#include "stm32f10x.h"
#include "stm32f10x_gpio.h"
#include "stm32f10x_rcc.h"
#include "stm32f10x_spi.h"

// ===================== GPIO / SPI 选择 =====================
// 默认：SPI1  (PA5=SCK, PA6=MISO, PA7=MOSI)，CS=PA4
#define SH_SPIx                SPI1
#define SH_SPIx_CLK            RCC_APB2Periph_SPI1

#define SH_SPI_GPIO_CLK        RCC_APB2Periph_GPIOA
#define SH_SPI_PORT            GPIOA
#define SH_SPI_SCK_PIN         GPIO_Pin_5
#define SH_SPI_MISO_PIN        GPIO_Pin_6
#define SH_SPI_MOSI_PIN        GPIO_Pin_7

#define SH_CS_GPIO_CLK         RCC_APB2Periph_GPIOA
#define SH_CS_PORT             GPIOA
#define SH_CS_PIN              GPIO_Pin_4

// 若你还接了 ALARM/RESET，可自行加定义
// #define SH_ALARM_PORT ...
// #define SH_RESET_PORT ...

// ===================== CS 控制 =====================
static inline void sh_cs_low(void)  { GPIO_ResetBits(SH_CS_PORT, SH_CS_PIN); }
static inline void sh_cs_high(void) { GPIO_SetBits(SH_CS_PORT, SH_CS_PIN);   }

// ===================== 简单 us 延时（忙等） =====================
// 72MHz 下粗略延时：每次循环约 3~4 cycle（受编译优化影响）
// 你工程里如果已有更准的 us 延时（例如 TIMx），请替换它。
static inline void sh_delay_us(uint32_t us)
{
    // 经验值：每次 while 大约 1/6 us @72MHz（O2 仅作粗略）
    // 为了稳定性，宁可慢一点：
    volatile uint32_t n = us * 12;
    while (n--) { __NOP(); }
}

#endif // __SH36735_PORT_H__
