#ifndef BOARD_UART_H
#define BOARD_UART_H

#include <stdint.h>

void BoardUart_Init(uint32_t baudrate);
void BoardUart_SendByte(uint8_t value);
void BoardUart_Send(const uint8_t *data, uint16_t length);
uint8_t BoardUart_ReadByte(uint8_t *value);
void BoardUart_IrqHandler(void);

#endif
