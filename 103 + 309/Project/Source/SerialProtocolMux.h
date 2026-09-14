#ifndef SERIAL_PROTOCOL_MUX_H
#define SERIAL_PROTOCOL_MUX_H

#include <stdint.h>

/*
 * USART1 / USART2 共用协议分发层。
 *
 * 支持：
 * 1. 原 CommonUpper / Modbus RTU 协议（0x00 / 0x01 开头）
 * 2. LX V0.9 主协议（0x5A 0xA5 ... 0xF0）
 * 3. LX V0.9 DD 指令协议（0xDD ... 0x77）
 *
 * UART ISR 只收发字节；完整帧解析和业务处理在主循环完成。
 */

void SerialProtocolMux_Init(void);
void SerialProtocolMux_Process(void);
void SerialProtocolMux_USART1_IRQHandler(void);
void SerialProtocolMux_USART2_IRQHandler(void);
uint8_t SerialProtocolMux_IsAnyPortBusy(void);

#endif /* SERIAL_PROTOCOL_MUX_H */
