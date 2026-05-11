#ifndef BOARD_UART_H
#define BOARD_UART_H

#include <stdint.h>

void BoardUart_Init(void);
uint16_t BoardUart_Read(uint8_t *data, uint16_t max_len);
int BoardUart_Write(const uint8_t *data, uint16_t len, uint32_t now_ms);
void BoardUart_IrqHandler(void);

#endif
