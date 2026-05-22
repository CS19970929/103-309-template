#ifndef CT_CAN_GATEWAY_H
#define CT_CAN_GATEWAY_H

#include <stdint.h>
#include "ct_board_port.h"

#define CT_CAN_APP_REQ_BASE             0x060u
#define CT_CAN_APP_ACK_BASE             0x061u
#define CT_CAN_APP_GET_STATUS           0x01u
#define CT_CAN_APP_ENTER_IAP            0x02u

#define CT_CAN_IAP_CTRL_BASE            0x14F8F000u
#define CT_CAN_IAP_ACK_BASE             0x14F8F100u
#define CT_CAN_IAP_DATA_BASE            0x14000000u

#define CT_CAN_IAP_HELLO                0x01u
#define CT_CAN_IAP_START                0x02u
#define CT_CAN_IAP_COMMIT               0x03u
#define CT_CAN_IAP_END                  0x04u
#define CT_CAN_IAP_ABORT                0x05u
#define CT_CAN_IAP_ACK                  0x79u
#define CT_CAN_IAP_NACK                 0x1Fu

int CtCan_AppGetStatus(uint8_t can_addr, uint8_t *soc, uint8_t *soh);
int CtCan_AppEnterIap(uint8_t can_addr);
int CtCan_IapSendHello(uint8_t node);
int CtCan_IapSendStart(uint8_t node, uint32_t size, uint16_t crc16);
int CtCan_IapSendData(uint8_t node, uint16_t seq, const uint8_t data[8]);
int CtCan_IapSendCommit(uint8_t node, uint16_t block_seq, uint16_t block_len, uint16_t block_crc);
int CtCan_IapSendEnd(uint8_t node, uint16_t frame_count, uint16_t crc16);
int CtCan_IapWaitAck(uint8_t node, uint8_t cmd, uint16_t *expect_seq, uint32_t timeout_ms);

#endif
