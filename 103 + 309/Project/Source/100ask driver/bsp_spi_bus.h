#ifndef __BSP_SPI_BUS_H
#define __BSP_SPI_BUS_H

#include "stm32f10x.h"

/* 例：GPIOA */
#define SPI_GPIO_PORT       GPIOA
#define SPI_SCK_PIN         GPIO_Pin_5
#define SPI_MISO_PIN        GPIO_Pin_6
#define SPI_MOSI_PIN        GPIO_Pin_7
#define SPI_CS_PIN          GPIO_Pin_4

/* 写 SCK/MOSI/CS */
#define SPI_SCK(level)      GPIO_WriteBit(SPI_GPIO_PORT, SPI_SCK_PIN,  (level)?Bit_SET:Bit_RESET)
#define SPI_MOSI(level)     GPIO_WriteBit(SPI_GPIO_PORT, SPI_MOSI_PIN, (level)?Bit_SET:Bit_RESET)
#define SPI_CS(level)       GPIO_WriteBit(SPI_GPIO_PORT, SPI_CS_PIN,   (level)?Bit_SET:Bit_RESET)

/* 读 MISO */
#define SPI_MISO()          (GPIO_ReadInputDataBit(SPI_GPIO_PORT, SPI_MISO_PIN) ? 1 : 0)

#endif
