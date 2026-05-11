#ifndef BOARD_CAN_H
#define BOARD_CAN_H

#include "upg_feidao.h"

#include <stdint.h>

void BoardCan_Init(void);
int BoardCan_Write(const UpgCanFrame *frame, uint32_t now_ms);
uint8_t BoardCan_Read(UpgCanFrame *frame);

#endif
