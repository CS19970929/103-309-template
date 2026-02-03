#ifndef __SH36735_SPI_H__
#define __SH36735_SPI_H__

#include <stdint.h>
#include <stdbool.h>

#include "sh36735_port.h"

#ifdef __cplusplus
extern "C" {
#endif

// SPI CMD
#define SH_SPI_CMD_WRITE_REG   0x01
#define SH_SPI_CMD_READ_REG    0x02
#define SH_SPI_CMD_SW_RESET    0x0B

#define SH_SPI_ACK_OK          0xA5
#define SH_SPI_ACK_FAIL        0xFF

// 初始化硬件 SPI（Mode3, <=1MHz）
void sh36735_spi_hw_init(uint32_t pclk_hz, uint32_t spi_hz);

// 软件 SPI（bitbang）初始化（可选，若不用可不编译该文件）
void sh36735_spi_sw_init(void);

// 统一的 1byte 传输接口（硬件/软件 SPI 任选其一实现）
uint8_t sh36735_spi_xfer(uint8_t tx);

// CRC8 计算（poly=0x07 init=0x00）
uint8_t sh36735_crc8(const uint8_t *buf, uint32_t len);

// 写 1 字节寄存器（0x40~0x59），返回 true=ACK(0xA5)
bool sh36735_write_reg_u8(uint8_t reg, uint8_t val);

// 读 N 字节寄存器（0x40~0x99），返回 true=CRC OK
bool sh36735_read_regs(uint8_t reg, uint8_t *buf, uint8_t n);

// 软件复位（0x0B BB CC CRC8），返回 true=ACK(0xA5)
bool sh36735_sw_reset(void);

#ifdef __cplusplus
}
#endif

#endif // __SH36735_SPI_H__
