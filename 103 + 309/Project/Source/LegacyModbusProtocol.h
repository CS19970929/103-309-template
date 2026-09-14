#ifndef LEGACY_MODBUS_PROTOCOL_H
#define LEGACY_MODBUS_PROTOCOL_H

#include "main.h"

/*
 * 原 CommonUpper / Modbus RTU 帧解析兼容层。
 * 寄存器地址、读写权限、参数副作用仍复用 Sci_Upper.c 的原业务函数，
 * 本模块不复制 BMS 寄存器业务，只负责原协议的收帧和应答流程。
 */
typedef struct
{
    struct RS485MSG msg;
} LEGACY_MODBUS_PROTOCOL_CONTEXT;

void LegacyModbusProtocol_Init(LEGACY_MODBUS_PROTOCOL_CONTEXT *ctx);
void LegacyModbusProtocol_Reset(LEGACY_MODBUS_PROTOCOL_CONTEXT *ctx);
uint8_t LegacyModbusProtocol_IsStartByte(uint8_t data);
uint8_t LegacyModbusProtocol_Feed(LEGACY_MODBUS_PROTOCOL_CONTEXT *ctx, uint8_t data);
void LegacyModbusProtocol_OnRxIdle(LEGACY_MODBUS_PROTOCOL_CONTEXT *ctx);
void LegacyModbusProtocol_Process(LEGACY_MODBUS_PROTOCOL_CONTEXT *ctx);
uint8_t *LegacyModbusProtocol_GetTxBuffer(LEGACY_MODBUS_PROTOCOL_CONTEXT *ctx);
uint16_t LegacyModbusProtocol_GetTxLength(const LEGACY_MODBUS_PROTOCOL_CONTEXT *ctx);
uint8_t LegacyModbusProtocol_IsBusy(const LEGACY_MODBUS_PROTOCOL_CONTEXT *ctx);

#endif /* LEGACY_MODBUS_PROTOCOL_H */
