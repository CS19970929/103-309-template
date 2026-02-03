#ifndef SPI_BUS_SOFT_TEST_H
#define SPI_BUS_SOFT_TEST_H

#include "conf.h"

void softspi_mode3_gpio_init(void);
int ic_write_reg_u8(uint8_t reg_addr, uint8_t data);
int ic_read_reg(uint8_t reg_addr, uint8_t len, uint8_t *out);
int ic_soft_reset(void);


#endif