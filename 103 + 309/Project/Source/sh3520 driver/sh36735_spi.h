#ifndef __SH36735_SPI_H__
#define __SH36735_SPI_H__

#include <stdint.h>
#include <stdbool.h>

#include "sh36735_port.h"

#ifdef __cplusplus
extern "C" {
#endif

#define SH_SPI_CMD_WRITE_REG 0x01u
#define SH_SPI_CMD_READ_REG  0x02u
#define SH_SPI_CMD_SW_RESET  0x0Bu

#define SH_SPI_ACK_OK        0xA5u
#define SH_SPI_ACK_FAIL      0xFFu

void sh36735_spi_hw_init(uint32_t pclk_hz, uint32_t spi_hz);
void sh36735_spi_sw_init(void);
uint8_t sh36735_spi_xfer(uint8_t tx);

uint8_t sh36735_crc8(const uint8_t *buf, uint32_t len);
bool sh36735_write_reg_u8(uint8_t reg, uint8_t val);
bool sh36735_read_regs(uint8_t reg, uint8_t *buf, uint8_t n);
bool sh36735_sw_reset(void);

#ifdef __cplusplus
}
#endif

#endif
