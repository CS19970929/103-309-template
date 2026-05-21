#ifndef BOARD_CAN_H
#define BOARD_CAN_H

#include <stdint.h>

typedef struct {
    uint32_t id;
    uint8_t is_extended;
    uint8_t dlc;
    uint8_t data[8];
} BoardCanFrame;

void BoardCan_Init(uint32_t bitrate);
uint8_t BoardCan_Send(const BoardCanFrame *frame);
uint8_t BoardCan_Receive(BoardCanFrame *frame);

#endif
