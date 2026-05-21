#ifndef CAN_GATEWAY_H
#define CAN_GATEWAY_H

#include <stdint.h>

void CanGateway_Init(void);
void CanGateway_Poll(void);
uint8_t CanGateway_ReadRegs(uint8_t node_id, uint16_t address, uint16_t count);
uint8_t CanGateway_WriteReg(uint8_t node_id, uint16_t address, uint16_t value);
uint8_t CanGateway_RequestBootloader(uint8_t node_id);

#endif
