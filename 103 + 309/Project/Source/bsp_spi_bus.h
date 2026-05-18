#ifndef __BSP_SPI_BUS_H__
#define __BSP_SPI_BUS_H__

#include "stm32f10x.h"
#include "conf_gpio.h"

#define SOFT_SPI

#define PORT_SCK  GPIO_SCLK_SPI
#define PIN_SCK   PIN_SCLK_SPI
#define RCC_SCK   RCC_APB2Periph_GPIOA

#define PORT_MOSI GPIO_MOSI_SPI
#define PIN_MOSI  PIN_MOSI_SPI
#define RCC_MOSI  RCC_APB2Periph_GPIOA

#define PORT_MISO GPIO_MISO_SPI
#define PIN_MISO  PIN_MISO_SPI
#define RCC_MISO  RCC_APB2Periph_GPIOA

void bsp_InitSPIBus(void);

#endif
