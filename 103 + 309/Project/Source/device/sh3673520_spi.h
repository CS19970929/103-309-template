#pragma once
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "../common/sh_spi_port.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * SH3673520 SPI protocol (from provided doc screenshots)
 * - SPI Mode3: CPOL=1, CPHA=1
 * - Write: [0x01][RegAddr][Len][Data0..DataLen-1][CRC8] -> then read ACK byte (0xA5 OK, 0xFF FAIL)
 * - Read : [0xFF][0x02][RegAddr][Len] -> then read DataLen bytes + CRC8
 * - CRC8: poly 0x07, init 0x00; read includes 0xFF in CRC input.
 */

typedef struct
{
    sh_spi_port_t port;

    /* Timing guards (microseconds). Keep short to avoid SPI reset (tSPIRST) per doc. */
    uint16_t byte_gap_us;   /* gap between bytes while CS low */
    uint16_t cs_setup_us;   /* delay after CS low */
    uint16_t cs_hold_us;    /* delay before CS high */
} sh3673520_spi_t;

void sh3673520_spi_init(sh3673520_spi_t *s, sh_spi_port_t port);

bool sh3673520_spi_write(sh3673520_spi_t *s, uint8_t reg, const uint8_t *data, uint8_t len);
bool sh3673520_spi_write_u8(sh3673520_spi_t *s, uint8_t reg, uint8_t val);

bool sh3673520_spi_read(sh3673520_spi_t *s, uint8_t reg, uint8_t *out, uint8_t len);
bool sh3673520_spi_read_u8(sh3673520_spi_t *s, uint8_t reg, uint8_t *val);

/* Convenience: read big-endian u16 (high byte at reg, low at reg+1) */
bool sh3673520_spi_read_be_u16(sh3673520_spi_t *s, uint8_t reg_hi, uint16_t *out);

#ifdef __cplusplus
}
#endif
