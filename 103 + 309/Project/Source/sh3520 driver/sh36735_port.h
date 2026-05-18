#ifndef __SH36735_PORT_H__
#define __SH36735_PORT_H__

#include "stm32f10x.h"
#include "stm32f10x_gpio.h"
#include "stm32f10x_rcc.h"
#include "stm32f10x_spi.h"
#include "conf_gpio.h"

#define SH_SPIx         SPI1
#define SH_SPIx_CLK     RCC_APB2Periph_SPI1
#define SH_SPI_GPIO_CLK RCC_APB2Periph_GPIOA
#define SH_SPI_PORT     GPIOA
#define SH_SPI_SCK_PIN  GPIO_Pin_5
#define SH_SPI_MISO_PIN GPIO_Pin_6
#define SH_SPI_MOSI_PIN GPIO_Pin_7

#define SH_CS_GPIO_CLK  RCC_APB2Periph_GPIOA
#define SH_CS_PORT      GPIO_CS_SPI
#define SH_CS_PIN       PIN_CS_SPI

static inline void sh_cs_low(void)
{
	GPIO_ResetBits(SH_CS_PORT, SH_CS_PIN);
}

static inline void sh_cs_high(void)
{
	GPIO_SetBits(SH_CS_PORT, SH_CS_PIN);
}

static inline void sh_delay_us(uint32_t us)
{
	volatile uint32_t n = us * 12u;

	while (n-- != 0u)
	{
		__NOP();
	}
}

#endif
