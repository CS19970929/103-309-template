#include <stdint.h>

void HAL_GPIO_WritePin(int port, int pin, int value);

void SPI1_IRQHandler(void)
{
}

void Board_LedSet(uint8_t on)
{
    HAL_GPIO_WritePin(0, 1, on);
}
