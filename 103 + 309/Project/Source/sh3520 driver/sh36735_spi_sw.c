#include "sh36735_spi.h"

static inline void sck_high(void)
{
	GPIO_SetBits(GPIO_SCLK_SPI, PIN_SCLK_SPI);
}

static inline void sck_low(void)
{
	GPIO_ResetBits(GPIO_SCLK_SPI, PIN_SCLK_SPI);
}

static inline void mosi_high(void)
{
	GPIO_SetBits(GPIO_MOSI_SPI, PIN_MOSI_SPI);
}

static inline void mosi_low(void)
{
	GPIO_ResetBits(GPIO_MOSI_SPI, PIN_MOSI_SPI);
}

static inline uint8_t miso_read(void)
{
	return GPIO_ReadInputDataBit(GPIO_MISO_SPI, PIN_MISO_SPI) ? 1u : 0u;
}

void sh36735_spi_sw_init(void)
{
	sh_cs_high();
	sck_high();
	mosi_high();
}

uint8_t sh36735_spi_xfer(uint8_t tx)
{
	uint8_t i;
	uint8_t rx = 0;

	for (i = 0; i < 8u; ++i)
	{
		if ((tx & 0x80u) != 0u)
		{
			mosi_high();
		}
		else
		{
			mosi_low();
		}
		tx <<= 1;

		sck_low();
		sh_delay_us(1);

		sck_high();
		sh_delay_us(1);

		rx <<= 1;
		rx |= miso_read();
	}

	sck_high();
	return rx;
}
