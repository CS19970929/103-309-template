#pragma once
#include <stdint.h>
#include <stdbool.h>
#include "sh3673520_spi.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Minimal SH3673520 driver built from provided documents:
 * - SPI protocol + CRC8 + ACK/NACK
 * - Known RAM register window: 0x40..0x99 (doc)
 * - CADC register: 0x91/0x92 (doc)
 *
 * IMPORTANT:
 * This package intentionally keeps register map small and explicit.
 * Extend sh3673520_regs.h once you confirm full register table from datasheet.
 */

typedef struct
{
    sh3673520_spi_t spi;

    /* Calibration (optional): Current = k * (code - offset) */
    int16_t cadc_offset;
    float   cadc_k;
} sh3673520_t;

/* Known registers (from doc snippets) */
enum
{
    SH_REG_SCONF1 = 0x40,
    SH_REG_SCONF2 = 0x41,
    SH_REG_SCONF3 = 0x42,
    SH_REG_SCONF4 = 0x43,
    SH_REG_SCONF5 = 0x44,
    SH_REG_SCONF6 = 0x45,
    SH_REG_SCONF7 = 0x46,

    SH_REG_CADC_H = 0x91,
    SH_REG_CADC_L = 0x92,
};

/* Init: attach spi driver + optional timing */
void sh3673520_init(sh3673520_t *d, sh3673520_spi_t spi);

/* Raw register access */
bool sh3673520_wr_u8(sh3673520_t *d, uint8_t reg, uint8_t val);
bool sh3673520_rd_u8(sh3673520_t *d, uint8_t reg, uint8_t *val);
bool sh3673520_rd(sh3673520_t *d, uint8_t reg, uint8_t *buf, uint8_t len);

/* CADC current code (big-endian at 0x91/0x92) */
bool sh3673520_read_cadc_u16(sh3673520_t *d, uint16_t *code);
bool sh3673520_read_current_a(sh3673520_t *d, float *cur_a);

/* Low-power / ship operations mentioned in doc:
 * Enter condition includes DSGING/CHGING==0; driver doesn't force that,
 * it only emits the required write sequence.
 */
bool sh3673520_enter_ship(sh3673520_t *d);  /* write SCONF1: 0x55 then 0xAA */
bool sh3673520_exit_ship(sh3673520_t *d);   /* clear SCONF1 to 0 */

#ifdef __cplusplus
}
#endif
