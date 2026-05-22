#include "ct_can_gateway.h"
#include "ct_config.h"
#include "ct_crc16.h"
#include <string.h>

static uint16_t rd_be16(const uint8_t *p)
{
    return ((uint16_t)p[0] << 8) | p[1];
}

static void wr_be16(uint8_t *p, uint16_t value)
{
    p[0] = (uint8_t)(value >> 8);
    p[1] = (uint8_t)value;
}

static void wr_be32(uint8_t *p, uint32_t value)
{
    p[0] = (uint8_t)(value >> 24);
    p[1] = (uint8_t)(value >> 16);
    p[2] = (uint8_t)(value >> 8);
    p[3] = (uint8_t)value;
}

static uint32_t app_req_id(uint8_t can_addr)
{
    return (((uint32_t)can_addr & 0x0Fu) << 7) | CT_CAN_APP_REQ_BASE;
}

static uint32_t app_ack_id(uint8_t can_addr)
{
    return (((uint32_t)can_addr & 0x0Fu) << 7) | CT_CAN_APP_ACK_BASE;
}

static int send_app_cmd(uint8_t can_addr, uint8_t cmd, uint8_t a0, uint8_t a1, uint8_t a2)
{
    CtCanFrame frame;
    uint16_t crc;

    memset(&frame, 0, sizeof(frame));
    frame.id = app_req_id(can_addr);
    frame.ide = 0u;
    frame.dlc = 8u;
    frame.data[0] = 0xA5u;
    frame.data[1] = 0x5Au;
    frame.data[2] = cmd;
    frame.data[3] = a0;
    frame.data[4] = a1;
    frame.data[5] = a2;
    crc = CtCrc16_Calc(frame.data, 6u);
    frame.data[6] = (uint8_t)(crc >> 8);
    frame.data[7] = (uint8_t)crc;
    return CtBoard_CanSend(&frame, 200u);
}

static int wait_app_ack(uint8_t can_addr, uint8_t cmd, uint8_t *v0, uint8_t *v1)
{
    CtCanFrame frame;
    uint16_t expect_crc;
    uint16_t actual_crc;

    if (!CtBoard_CanRecv(&frame, 1000u))
    {
        return 0;
    }
    if ((frame.ide != 0u) || (frame.id != app_ack_id(can_addr)) || (frame.dlc != 8u))
    {
        return 0;
    }
    if ((frame.data[0] != 0x5Au) || (frame.data[1] != 0xA5u) || (frame.data[2] != cmd))
    {
        return 0;
    }
    expect_crc = rd_be16(&frame.data[6]);
    actual_crc = CtCrc16_Calc(frame.data, 6u);
    if ((expect_crc != actual_crc) || (frame.data[3] != 0u))
    {
        return 0;
    }
    if (v0 != 0)
    {
        *v0 = frame.data[4];
    }
    if (v1 != 0)
    {
        *v1 = frame.data[5];
    }
    return 1;
}

int CtCan_AppGetStatus(uint8_t can_addr, uint8_t *soc, uint8_t *soh)
{
    uint8_t v0;
    uint8_t v1;

    if (!send_app_cmd(can_addr, CT_CAN_APP_GET_STATUS, 0u, 0u, 0u))
    {
        return 0;
    }
    if (!wait_app_ack(can_addr, CT_CAN_APP_GET_STATUS, &v0, &v1))
    {
        return 0;
    }
    if (soc != 0)
    {
        *soc = v0;
    }
    if (soh != 0)
    {
        *soh = v1;
    }
    return 1;
}

int CtCan_AppEnterIap(uint8_t can_addr)
{
    if (!send_app_cmd(can_addr, CT_CAN_APP_ENTER_IAP, 0xC3u, 0x3Cu, can_addr))
    {
        return 0;
    }
    return wait_app_ack(can_addr, CT_CAN_APP_ENTER_IAP, 0, 0);
}

static uint32_t iap_ctrl_id(uint8_t node)
{
    return CT_CAN_IAP_CTRL_BASE | node;
}

static uint32_t iap_ack_id(uint8_t node)
{
    return CT_CAN_IAP_ACK_BASE | node;
}

static uint32_t iap_data_id(uint8_t node, uint16_t seq)
{
    return CT_CAN_IAP_DATA_BASE | ((uint32_t)seq << 8) | node;
}

static int send_iap_ctrl(uint8_t node, const uint8_t data[8])
{
    CtCanFrame frame;

    memset(&frame, 0, sizeof(frame));
    frame.id = iap_ctrl_id(node);
    frame.ide = 1u;
    frame.dlc = 8u;
    memcpy(frame.data, data, 8u);
    return CtBoard_CanSend(&frame, 200u);
}

int CtCan_IapSendHello(uint8_t node)
{
    uint8_t data[8] = {CT_CAN_IAP_HELLO, CT_PROTOCOL_VERSION, 0, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
    data[2] = node;
    return send_iap_ctrl(node, data);
}

int CtCan_IapSendStart(uint8_t node, uint32_t size, uint16_t crc16)
{
    uint8_t data[8];
    data[0] = CT_CAN_IAP_START;
    data[1] = CT_PROTOCOL_VERSION;
    wr_be32(&data[2], size);
    wr_be16(&data[6], crc16);
    return send_iap_ctrl(node, data);
}

int CtCan_IapSendData(uint8_t node, uint16_t seq, const uint8_t data[8])
{
    CtCanFrame frame;

    memset(&frame, 0, sizeof(frame));
    frame.id = iap_data_id(node, seq);
    frame.ide = 1u;
    frame.dlc = 8u;
    memcpy(frame.data, data, 8u);
    return CtBoard_CanSend(&frame, 200u);
}

int CtCan_IapSendCommit(uint8_t node, uint16_t block_seq, uint16_t block_len, uint16_t block_crc)
{
    uint8_t data[8];
    data[0] = CT_CAN_IAP_COMMIT;
    wr_be16(&data[1], block_seq);
    wr_be16(&data[3], block_len);
    wr_be16(&data[5], block_crc);
    data[7] = 0xFFu;
    return send_iap_ctrl(node, data);
}

int CtCan_IapSendEnd(uint8_t node, uint16_t frame_count, uint16_t crc16)
{
    uint8_t data[8];
    data[0] = CT_CAN_IAP_END;
    wr_be16(&data[1], frame_count);
    wr_be16(&data[3], crc16);
    data[5] = 0xFFu;
    data[6] = 0xFFu;
    data[7] = 0xFFu;
    return send_iap_ctrl(node, data);
}

int CtCan_IapWaitAck(uint8_t node, uint8_t cmd, uint16_t *expect_seq, uint32_t timeout_ms)
{
    CtCanFrame frame;

    if (!CtBoard_CanRecv(&frame, timeout_ms))
    {
        return 0;
    }
    if ((frame.ide == 0u) || (frame.id != iap_ack_id(node)) || (frame.dlc < 6u))
    {
        return 0;
    }
    if ((frame.data[0] == CT_CAN_IAP_NACK) && (frame.data[1] == cmd))
    {
        return 0;
    }
    if ((frame.data[0] != CT_CAN_IAP_ACK) || (frame.data[1] != cmd) || (frame.data[2] != 0u))
    {
        return 0;
    }
    if (expect_seq != 0)
    {
        *expect_seq = rd_be16(&frame.data[3]);
    }
    return 1;
}

