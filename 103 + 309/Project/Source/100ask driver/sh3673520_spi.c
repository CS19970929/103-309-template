#include "sh3673520_spi.h"
#include "sh3673520_spi_cfg.h"
#include "driver_spi_soft.h"

/* 如果后续你确认 3520 有 CRC，这里再补 CRC8/CRC16 */
static uint8_t crc8_dummy(const uint8_t *p, uint16_t n)
{
    (void)p; (void)n;
    return 0;
}

void sh3673520_spi_init(void)
{
    spi_soft_cfg_t cfg = {
        .mode            = SH3673520_SPI_MODE,
        .bit_order       = SPI_BITORDER_MSB,
        .t_half_delay_us = SH3673520_SPI_HALF_US,
        .t_cs_setup_us   = SH3673520_CS_SETUP_US,
        .t_cs_hold_us    = SH3673520_CS_HOLD_US,
        .t_cs_high_us    = SH3673520_CS_HIGH_US,
        .t_bit_gap_us    = 0,
    };

    spi_soft_init(&cfg);
}

/* 生成命令字节：模板（按手册改） */
static void build_cmd(uint16_t reg, bool is_read, uint8_t *out, uint8_t *out_len)
{
#if (SH3673520_ADDR_BYTES == 1)
    uint8_t cmd = (uint8_t)(reg & 0x7F);

    if (is_read) {
        cmd |= (uint8_t)(SH3673520_CMD_READ << SH3673520_CMD_RW_BIT_POS);
    } else {
        cmd |= (uint8_t)(SH3673520_CMD_WRITE << SH3673520_CMD_RW_BIT_POS);
    }

    out[0] = cmd;
    *out_len = 1;

#elif (SH3673520_ADDR_BYTES == 2)
    /* 示例：两字节地址，不同芯片可能是 [cmdHi, cmdLo] 或者先发 R/W 再发 addr */
    uint8_t hi = (uint8_t)((reg >> 8) & 0xFF);
    uint8_t lo = (uint8_t)(reg & 0xFF);

    /* 这里给一个“最保守”的占位：直接发 hi/lo
     * 真实情况你必须按手册改：R/W 位在 hi 里还是单独 1Byte？
     */
    (void)is_read;
    out[0] = hi;
    out[1] = lo;
    *out_len = 2;
#else
#error "SH3673520_ADDR_BYTES must be 1 or 2"
#endif
}

bool sh3673520_write_reg(uint16_t reg, const uint8_t *buf, uint16_t len)
{
    if (!buf || !len) return false;

    uint8_t cmd[4];
    uint8_t cmd_len = 0;

    build_cmd(reg, false, cmd, &cmd_len);

    spi_soft_cs_low();

    /* 先发命令/地址 */
    spi_soft_write(cmd, cmd_len);

    /* 再发数据 */
    spi_soft_write(buf, len);

#if (SH3673520_HAS_CRC == 1)
    uint8_t c = crc8_dummy(cmd, cmd_len); /* 这里只是占位 */
    (void)c;
#endif

    spi_soft_cs_high();
    return true;
}

bool sh3673520_read_reg(uint16_t reg, uint8_t *buf, uint16_t len)
{
    if (!buf || !len) return false;

    uint8_t cmd[4];
    uint8_t cmd_len = 0;

    build_cmd(reg, true, cmd, &cmd_len);

    spi_soft_cs_low();

    /* 先发命令/地址 */
    spi_soft_write(cmd, cmd_len);

    /* 读前 dummy */
#if (SH3673520_READ_DUMMY_BYTES > 0)
    for (uint8_t i = 0; i < SH3673520_READ_DUMMY_BYTES; i++) {
        (void)spi_soft_xfer8(0xFF);
    }
#endif

    /* 读数据 */
    spi_soft_read(buf, len);

#if (SH3673520_HAS_CRC == 1)
    uint8_t c = spi_soft_read8();
    (void)c;
#endif

    spi_soft_cs_high();
    return true;
}

bool sh3673520_read_u8(uint16_t reg, uint8_t *val)
{
    return sh3673520_read_reg(reg, val, 1);
}

bool sh3673520_write_u8(uint16_t reg, uint8_t val)
{
    return sh3673520_write_reg(reg, &val, 1);
}

bool sh3673520_read_u16(uint16_t reg, uint16_t *val)
{
    uint8_t b[2];
    if (!val) return false;
    if (!sh3673520_read_reg(reg, b, 2)) return false;
    /* 默认：高字节在前（很多寄存器是 big-endian），如手册相反就调换 */
    *val = ((uint16_t)b[0] << 8) | b[1];
    return true;
}

bool sh3673520_write_u16(uint16_t reg, uint16_t val)
{
    uint8_t b[2];
    b[0] = (uint8_t)(val >> 8);
    b[1] = (uint8_t)(val & 0xFF);
    return sh3673520_write_reg(reg, b, 2);
}
