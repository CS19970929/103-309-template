#ifndef __DRIVER_SPI_SOFT_H
#define __DRIVER_SPI_SOFT_H

#include <stdint.h>
#include <stdbool.h>

/*
 * 你工程里已有的 GPIO 宏/函数，请在这里对接：
 *  - SPI_SCK(level)
 *  - SPI_MOSI(level)
 *  - SPI_MISO() -> 0/1
 *  - SPI_CS(level)  (可选：如果你的 CS 不是固定 W25，而是 AFE 的 CS)
 *
 * 建议你在 bsp_spi_bus.h 里提供这些宏。
 */
#include "bsp_spi_bus.h"

/* ============ SPI 配置 ============ */

typedef enum {
    SPI_BITORDER_MSB = 0,
    SPI_BITORDER_LSB = 1,
} spi_bitorder_t;

typedef enum {
    SPI_MODE0 = 0, /* CPOL=0, CPHA=0 */
    SPI_MODE1 = 1, /* CPOL=0, CPHA=1 */
    SPI_MODE2 = 2, /* CPOL=1, CPHA=0 */
    SPI_MODE3 = 3, /* CPOL=1, CPHA=1 */
} spi_mode_t;

typedef struct {
    spi_mode_t     mode;
    spi_bitorder_t bit_order;

    /* half-cycle delay: 决定 SPI 时钟速度（越大越慢，越稳） */
    uint16_t       t_half_delay_us;

    /* CS 前后留白（有些 AFE 要求严格） */
    uint16_t       t_cs_setup_us;   /* CS 拉低后，到第一个 SCK 前 */
    uint16_t       t_cs_hold_us;    /* 最后一个 SCK 后，到 CS 拉高前 */
    uint16_t       t_cs_high_us;    /* 两次事务之间 CS 高电平最小时间 */

    /* 可选：每个 bit 翻转后额外留白（一般不需要） */
    uint16_t       t_bit_gap_us;
} spi_soft_cfg_t;

/* ============ API ============ */

void     spi_soft_init(const spi_soft_cfg_t *cfg);
void     spi_soft_set_cfg(const spi_soft_cfg_t *cfg);

void     spi_soft_cs_low(void);
void     spi_soft_cs_high(void);

uint8_t  spi_soft_xfer8(uint8_t tx);
void     spi_soft_write8(uint8_t tx);
uint8_t  spi_soft_read8(void);

void     spi_soft_write(const uint8_t *buf, uint16_t len);
void     spi_soft_read(uint8_t *buf, uint16_t len);
void     spi_soft_transfer(const uint8_t *tx, uint8_t *rx, uint16_t len);

#endif /* __DRIVER_SPI_SOFT_H */
