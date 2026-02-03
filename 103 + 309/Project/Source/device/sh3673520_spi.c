#include "sh3673520_spi.h"
#include "../common/sh_crc8.h"

#define SH_CMD_WRITE   0x01
#define SH_CMD_READ    0x02
#define SH_READ_PREFIX 0xFF

#define SH_ACK_OK      0xA5
#define SH_ACK_FAIL    0xFF

static inline void dly(const sh3673520_spi_t *s, uint32_t us)
{
    if (s->port.delay_us) {
        s->port.delay_us(s->port.user, us);
    }
}

static inline uint8_t xfer(const sh3673520_spi_t *s, uint8_t b)
{
    return s->port.txrx(s->port.user, b);
}

static void begin(const sh3673520_spi_t *s)
{
    s->port.cs_low(s->port.user);
    dly(s, s->cs_setup_us);
}

static void end(const sh3673520_spi_t *s)
{
    dly(s, s->cs_hold_us);
    s->port.cs_high(s->port.user);
}

void sh3673520_spi_init(sh3673520_spi_t *s, sh_spi_port_t port)
{
    s->port = port;
    /* 保守默认：避免 CS 拉低后/字节间隔过长触发芯片 SPI reset 检测 */
    s->byte_gap_us = 2;
    s->cs_setup_us = 2;
    s->cs_hold_us  = 2;
}

bool sh3673520_spi_write(sh3673520_spi_t *s, uint8_t reg, const uint8_t *data, uint8_t len)
{
    if (len == 0 || len > 0x3F || data == NULL) {
        return false;
    }

    uint8_t crc = 0x00;

    begin(s);

    /* CMD */
    crc = sh_crc8_update(crc, SH_CMD_WRITE);
    (void)xfer(s, SH_CMD_WRITE); dly(s, s->byte_gap_us);

    /* Reg */
    crc = sh_crc8_update(crc, reg);
    (void)xfer(s, reg); dly(s, s->byte_gap_us);

    /* Len */
    crc = sh_crc8_update(crc, len);
    (void)xfer(s, len); dly(s, s->byte_gap_us);

    /* Data */
    for (uint8_t i = 0; i < len; i++) {
        crc = sh_crc8_update(crc, data[i]);
        (void)xfer(s, data[i]);
        dly(s, s->byte_gap_us);
    }

    /* CRC */
    (void)xfer(s, crc);
    dly(s, s->byte_gap_us);

    /* Read ACK (chip drives SDO) */
    uint8_t ack = xfer(s, 0xFF);

    end(s);

    return (ack == SH_ACK_OK);
}

bool sh3673520_spi_write_u8(sh3673520_spi_t *s, uint8_t reg, uint8_t val)
{
    return sh3673520_spi_write(s, reg, &val, 1);
}

bool sh3673520_spi_read(sh3673520_spi_t *s, uint8_t reg, uint8_t *out, uint8_t len)
{
    if (len == 0 || len > 0x3F || out == NULL) {
        return false;
    }

    uint8_t crc = 0x00;

    begin(s);

    /* Prefix 0xFF participates in CRC for READ per doc */
    // crc = sh_crc8_update(crc, SH_READ_PREFIX);
    // (void)xfer(s, SH_READ_PREFIX); dly(s, s->byte_gap_us);

    /* CMD */
    crc = sh_crc8_update(crc, SH_CMD_READ);
    (void)xfer(s, SH_CMD_READ); dly(s, s->byte_gap_us);

    /* Reg */
    crc = sh_crc8_update(crc, reg);
    (void)xfer(s, reg); dly(s, s->byte_gap_us);

    /* Len */
    crc = sh_crc8_update(crc, len);
    (void)xfer(s, len); dly(s, s->byte_gap_us);

    xfer(s, 0xFF); dly(s, s->byte_gap_us);
    /* Read data */
    for (uint8_t i = 0; i < len; i++) {
        uint8_t b = xfer(s, 0xFF);
        out[i] = b;
        crc = sh_crc8_update(crc, b);
        dly(s, s->byte_gap_us);
    }

    /* Read CRC */
    uint8_t crc_rx = xfer(s, 0xFF);

    end(s);

    return (crc_rx == crc);
}

bool sh3673520_spi_read_u8(sh3673520_spi_t *s, uint8_t reg, uint8_t *val)
{
    return sh3673520_spi_read(s, reg, val, 1);
}

bool sh3673520_spi_read_be_u16(sh3673520_spi_t *s, uint8_t reg_hi, uint16_t *out)
{
    uint8_t buf[2] = {0};
    if (!out) return false;

    /* IMPORTANT: must read from high-byte address and read 2 bytes continuously */
    if (!sh3673520_spi_read(s, reg_hi, buf, 2)) return false;

    *out = ((uint16_t)buf[0] << 8) | buf[1];
    return true;
}
