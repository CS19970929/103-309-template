#include "can_gateway.h"
#include "board_can.h"
#include "comm_tool_config.h"

static uint8_t s_gateway_seq;

void CanGateway_Init(void)
{
    s_gateway_seq = 0u;
}

void CanGateway_Poll(void)
{
    BoardCanFrame frame;

    while (BoardCan_Receive(&frame) != 0u) {
    }
}

uint8_t CanGateway_ReadRegs(uint8_t node_id, uint16_t address, uint16_t count)
{
    BoardCanFrame frame;

    frame.id = COMM_TOOL_CAN_APP_REQ_BASE | node_id;
    frame.is_extended = 1u;
    frame.dlc = 8u;
    frame.data[0] = 0x03u;
    frame.data[1] = s_gateway_seq++;
    frame.data[2] = (uint8_t)(address >> 8u);
    frame.data[3] = (uint8_t)address;
    frame.data[4] = (uint8_t)(count >> 8u);
    frame.data[5] = (uint8_t)count;
    frame.data[6] = 0u;
    frame.data[7] = 0u;
    return BoardCan_Send(&frame);
}

uint8_t CanGateway_WriteReg(uint8_t node_id, uint16_t address, uint16_t value)
{
    BoardCanFrame frame;

    frame.id = COMM_TOOL_CAN_APP_REQ_BASE | node_id;
    frame.is_extended = 1u;
    frame.dlc = 8u;
    frame.data[0] = 0x06u;
    frame.data[1] = s_gateway_seq++;
    frame.data[2] = (uint8_t)(address >> 8u);
    frame.data[3] = (uint8_t)address;
    frame.data[4] = (uint8_t)(value >> 8u);
    frame.data[5] = (uint8_t)value;
    frame.data[6] = 0u;
    frame.data[7] = 0u;
    return BoardCan_Send(&frame);
}

uint8_t CanGateway_RequestBootloader(uint8_t node_id)
{
    BoardCanFrame frame;

    frame.id = COMM_TOOL_CAN_APP_REQ_BASE | node_id;
    frame.is_extended = 1u;
    frame.dlc = 8u;
    frame.data[0] = 0x55u;
    frame.data[1] = s_gateway_seq++;
    frame.data[2] = 0xC3u;
    frame.data[3] = 0x3Cu;
    frame.data[4] = node_id;
    frame.data[5] = 0u;
    frame.data[6] = 0u;
    frame.data[7] = 0u;
    return BoardCan_Send(&frame);
}
