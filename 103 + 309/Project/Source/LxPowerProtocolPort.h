#ifndef LX_POWER_PROTOCOL_PORT_H
#define LX_POWER_PROTOCOL_PORT_H

#include "LxPowerProtocol.h"

/*
 * 当前 STM32F103 + SH367309 BMS 对 LX V0.9 的数据适配层。
 * 移植到其它 MCU/AFE 时，协议核心 LxPowerProtocol.c 不需要修改，
 * 只替换本 Port 层即可。
 */
extern const LX_POWER_PROTOCOL_ADAPTER g_lx_power_protocol_adapter;

#endif /* LX_POWER_PROTOCOL_PORT_H */
