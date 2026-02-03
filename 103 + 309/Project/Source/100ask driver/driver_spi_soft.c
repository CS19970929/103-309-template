#include "driver_spi_soft.h"

/* 你工程里已有的 us 级延时函数：比如 us_timer_delay(x) */
extern void us_timer_delay(uint32_t us);

static spi_soft_cfg_t g_cfg;

/* ========= GPIO 适配 =========
 * 你需要在 bsp_spi_bus.h 里提供：
 *  SPI_SCK(level)
 *  SPI_MOSI(level)
 *  SPI_MISO()
 *  SPI_CS(level)
 *
 * level: 0/1
 */

static inline void DLY(uint16_t us)
{
    if (us) us_timer_delay(us);
}

static inline uint8_t cpol_level(spi_mode_t mode)
{
    return (mode == SPI_MODE2 || mode == SPI_MODE3) ? 1 : 0;
}

static inline uint8_t cpha_is_1(spi_mode_t mode)
{
    return (mode == SPI_MODE1 || mode == SPI_MODE3) ? 1 : 0;
}

void spi_soft_set_cfg(const spi_soft_cfg_t *cfg)
{
    if (!cfg) return;
    g_cfg = *cfg;
}

void spi_soft_init(const spi_soft_cfg_t *cfg)
{
    /* 这里不做 GPIO 初始化（因为你可能用标准库/自家 BSP 初始化）
     * 只做时序与空闲电平设置
     */
    spi_soft_set_cfg(cfg);

    /* CS 空闲高 */
    SPI_CS(1);

    /* SCK 空闲电平 = CPOL */
    SPI_SCK(cpol_level(g_cfg.mode));

    /* MOSI 默认拉低（可改成 0/1 都行，重要的是稳定） */
    SPI_MOSI(0);

    /* 给总线一个稳定时间 */
    DLY(g_cfg.t_cs_high_us);
}

void spi_soft_cs_low(void)
{
    SPI_CS(0);
    DLY(g_cfg.t_cs_setup_us);
}

void spi_soft_cs_high(void)
{
    DLY(g_cfg.t_cs_hold_us);
    SPI_CS(1);
    DLY(g_cfg.t_cs_high_us);
}

/*
 * 核心：按 CPOL/CPHA 产生时钟并采样
 *
 * 约定：
 *  - CPHA=0：第一个边沿采样（leading edge sample），第二个边沿改变数据
 *  - CPHA=1：第一个边沿改变数据，第二个边沿采样
 *
 * leading edge:
 *   CPOL=0 -> 0->1
 *   CPOL=1 -> 1->0
 */
uint8_t spi_soft_xfer8(uint8_t tx)
{
    uint8_t rx = 0;
    uint8_t cpol = cpol_level(g_cfg.mode);
    uint8_t cpha = cpha_is_1(g_cfg.mode);

    for (uint8_t i = 0; i < 8; i++) {
        uint8_t bit_tx;

        if (g_cfg.bit_order == SPI_BITORDER_MSB) {
            bit_tx = (tx & 0x80u) ? 1u : 0u;
            tx <<= 1;
        } else {
            bit_tx = (tx & 0x01u) ? 1u : 0u;
            tx >>= 1;
        }

        if (cpha == 0) {
            /* CPHA=0：先把数据放好，再打 leading edge 采样 */
            SPI_MOSI(bit_tx);
            if (g_cfg.t_bit_gap_us) DLY(g_cfg.t_bit_gap_us);

            /* leading edge */
            SPI_SCK(!cpol);
            DLY(g_cfg.t_half_delay_us);

            /* 采样 MISO */
            rx <<= 1;
            rx |= (SPI_MISO() ? 1u : 0u);

            /* trailing edge */
            SPI_SCK(cpol);
            DLY(g_cfg.t_half_delay_us);
        } else {
            /* CPHA=1：先打 leading edge（改变数据），再在 trailing edge 采样 */
            /* leading edge */
            SPI_SCK(!cpol);
            DLY(g_cfg.t_half_delay_us);

            SPI_MOSI(bit_tx);
            if (g_cfg.t_bit_gap_us) DLY(g_cfg.t_bit_gap_us);

            /* trailing edge */
            SPI_SCK(cpol);
            DLY(g_cfg.t_half_delay_us);

            rx <<= 1;
            rx |= (SPI_MISO() ? 1u : 0u);
        }
    }

    return rx;
}

void spi_soft_write8(uint8_t tx)
{
    (void)spi_soft_xfer8(tx);
}

uint8_t spi_soft_read8(void)
{
    return spi_soft_xfer8(0xFF);
}

void spi_soft_write(const uint8_t *buf, uint16_t len)
{
    if (!buf || !len) return;
    while (len--) {
        spi_soft_write8(*buf++);
    }
}

void spi_soft_read(uint8_t *buf, uint16_t len)
{
    if (!buf || !len) return;
    while (len--) {
        *buf++ = spi_soft_read8();
    }
}

void spi_soft_transfer(const uint8_t *tx, uint8_t *rx, uint16_t len)
{
    if (!len) return;

    for (uint16_t i = 0; i < len; i++) {
        uint8_t t = tx ? tx[i] : 0xFF;
        uint8_t r = spi_soft_xfer8(t);
        if (rx) rx[i] = r;
    }
}
