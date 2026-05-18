#include "bsp_spi_bus.h"
#include "main.h"

void bsp_InitSPIBus(void)
{
	GPIO_InitTypeDef gpio;

	RCC_APB2PeriphClockCmd(RCC_SCK | RCC_MOSI | RCC_MISO, ENABLE);

	gpio.GPIO_Mode = GPIO_Mode_Out_PP;
	gpio.GPIO_Speed = GPIO_Speed_2MHz;

	gpio.GPIO_Pin = PIN_SCK;
	GPIO_Init(PORT_SCK, &gpio);

	gpio.GPIO_Pin = PIN_MOSI;
	GPIO_Init(PORT_MOSI, &gpio);

	gpio.GPIO_Mode = GPIO_Mode_IN_FLOATING;
	gpio.GPIO_Pin = PIN_MISO;
	GPIO_Init(PORT_MISO, &gpio);
}
