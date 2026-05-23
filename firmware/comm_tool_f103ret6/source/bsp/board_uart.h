#ifndef BOARD_UART_H
#define BOARD_UART_H

#include <stdint.h>

void BoardUart_Init(uint32_t baudrate);
int BoardUart_ReadByte(uint8_t *byte);

#endif
