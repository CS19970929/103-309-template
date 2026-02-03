#include "sh36735_spi.h"
#include "main.h"

/*
 * 说明书：CPOL=1, CPHA=1
 * 空闲 SCK=1；数据在下降沿改变，在上升沿采样（标准 SPI Mode 3 语义）。
 *
 * 该软件 SPI 仅用于你硬件 SPI 跑不通时的“救命示波器模式”，方便你用 GPIO 抓波形定位问题。
 * 实际量产建议用硬件 SPI。
 */

#ifndef SH_SW_SPI_ENABLE
#define SH_SW_SPI_ENABLE 1
#endif

#if SH_SW_SPI_ENABLE

#define SH_SW_SCK_PORT

// 你可以在 sh36735_port.h 里补这些 GPIO 宏（或改成你工程现有的）
#ifndef SH_SW_SCK_PORT
# error "Define SH_SW_SCK_PORT/SH_SW_SCK_PIN etc in sh36735_port.h for software SPI."
#endif

#if 0
static inline void sck_high(void){ GPIO_SetBits(SH_SW_SCK_PORT, SH_SW_SCK_PIN); }
static inline void sck_low(void) { GPIO_ResetBits(SH_SW_SCK_PORT, SH_SW_SCK_PIN); }
static inline void mosi_high(void){ GPIO_SetBits(SH_SW_MOSI_PORT, SH_SW_MOSI_PIN); }
static inline void mosi_low(void) { GPIO_ResetBits(SH_SW_MOSI_PORT, SH_SW_MOSI_PIN); }
static inline uint8_t miso_read(void){ return GPIO_ReadInputDataBit(SH_SW_MISO_PORT, SH_SW_MISO_PIN) ? 1u : 0u; }
#endif
static inline void sck_high(void){ GPIO_SetBits(GPIO_SCLK_SPI, PIN_SCLK_SPI); }
static inline void sck_low(void) { GPIO_ResetBits(GPIO_SCLK_SPI, PIN_SCLK_SPI); }
static inline void mosi_high(void){ GPIO_SetBits(GPIO_MOSI_SPI, PIN_MOSI_SPI); }
static inline void mosi_low(void) { GPIO_ResetBits(GPIO_MOSI_SPI, PIN_MOSI_SPI); }
static inline uint8_t miso_read(void){ return GPIO_ReadInputDataBit(GPIO_MISO_SPI, PIN_MISO_SPI) ? 1u : 0u; }


void sh36735_spi_sw_init(void)
{
    // 留给你自己在 port.h 里做 GPIO 初始化
    // 注意：SCK 默认高电平（CPOL=1）
    sck_high();
    mosi_high();
}

uint8_t sh36735_spi_xfer(uint8_t tx)
{
    uint8_t rx = 0;

    for (int i = 0; i < 8; i++) {
        // MSB first
        if (tx & 0x80) mosi_high(); else mosi_low();
        tx <<= 1;

        // CPHA=1: 第一个沿（下降沿）让对端看到数据变化
        sck_low();
        sh_delay_us(1);

        // 第二个沿（上升沿）采样
        sck_high();
        sh_delay_us(1);

        rx <<= 1;
        rx |= miso_read();
    }

    // 回到空闲高
    sck_high();
    return rx;
}

#else

void sh36735_spi_sw_init(void) { /* not enabled */ }

#endif
