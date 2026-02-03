#include "sh3673520.h"

void sh3673520_init(sh3673520_t *d, sh3673520_spi_t spi)
{
    d->spi = spi;
    d->cadc_offset = 0;
    d->cadc_k = 1.0f;
}

bool sh3673520_wr_u8(sh3673520_t *d, uint8_t reg, uint8_t val)
{
    return sh3673520_spi_write_u8(&d->spi, reg, val);
}

bool sh3673520_rd_u8(sh3673520_t *d, uint8_t reg, uint8_t *val)
{
    return sh3673520_spi_read_u8(&d->spi, reg, val);
}

bool sh3673520_rd(sh3673520_t *d, uint8_t reg, uint8_t *buf, uint8_t len)
{
    return sh3673520_spi_read(&d->spi, reg, buf, len);
}

bool sh3673520_read_cadc_u16(sh3673520_t *d, uint16_t *code)
{
    return sh3673520_spi_read_be_u16(&d->spi, SH_REG_CADC_H, code);
}

bool sh3673520_read_current_a(sh3673520_t *d, float *cur_a)
{
    uint16_t code = 0;
    if (!cur_a) return false;
    if (!sh3673520_read_cadc_u16(d, &code)) return false;

    /* NOTE: CADC interpretation depends on your shunt + datasheet scale.
       Here we provide a configurable linear calibration hook. */
    float v = (float)((int32_t)code - (int32_t)d->cadc_offset);
    *cur_a = d->cadc_k * v;
    return true;
}

bool sh3673520_enter_ship(sh3673520_t *d)
{
    /* Doc: write 0x55 then 0xAA to SCONF1 */
    if (!sh3673520_wr_u8(d, SH_REG_SCONF1, 0x55)) return false;
    if (!sh3673520_wr_u8(d, SH_REG_SCONF1, 0xAA)) return false;
    return true;
}

bool sh3673520_exit_ship(sh3673520_t *d)
{
    /* Doc: MCU clears SCONF1 to exit */
    return sh3673520_wr_u8(d, SH_REG_SCONF1, 0x00);
}
