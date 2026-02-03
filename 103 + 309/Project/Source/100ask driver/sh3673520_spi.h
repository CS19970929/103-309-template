#ifndef __SH3673520_SPI_H
#define __SH3673520_SPI_H

#include <stdint.h>
#include <stdbool.h>

void sh3673520_spi_init(void);

/* 基础寄存器读写（按你的手册协议帧实现） */
bool sh3673520_read_reg(uint16_t reg, uint8_t *buf, uint16_t len);
bool sh3673520_write_reg(uint16_t reg, const uint8_t *buf, uint16_t len);

/* 便捷：读写 1 字节/2 字节 */
bool sh3673520_read_u8(uint16_t reg, uint8_t *val);
bool sh3673520_write_u8(uint16_t reg, uint8_t val);

bool sh3673520_read_u16(uint16_t reg, uint16_t *val);
bool sh3673520_write_u16(uint16_t reg, uint16_t val);

#endif /* __SH3673520_SPI_H */
