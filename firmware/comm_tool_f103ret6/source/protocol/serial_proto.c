#include "serial_proto.h"
#include "board_uart.h"
#include "can_gateway.h"
#include "comm_tool_config.h"
#include "crc16.h"
#include "flash_store.h"
#include "upgrade_manager.h"
#include <string.h>

#define SERIAL_SOF0 0xA5u
#define SERIAL_SOF1 0x5Au

#define CMD_GET_INFO            0x01u
#define CMD_SET_CONFIG          0x02u
#define CMD_BMS_READ_REGS       0x10u
#define CMD_BMS_WRITE_REG       0x11u
#define CMD_BMS_WRITE_REGS      0x12u
#define CMD_FW_BEGIN            0x20u
#define CMD_FW_DATA             0x21u
#define CMD_FW_END_VERIFY       0x22u
#define CMD_FW_INFO             0x23u
#define CMD_BMS_UPGRADE_START   0x30u
#define CMD_BMS_UPGRADE_STATUS  0x31u
#define CMD_BMS_UPGRADE_ABORT   0x32u

typedef enum {
    PARSE_WAIT_SOF0 = 0,
    PARSE_WAIT_SOF1,
    PARSE_HEADER,
    PARSE_PAYLOAD,
    PARSE_CRC0,
    PARSE_CRC1
} SerialParseState;

static SerialParseState s_state;
static uint8_t s_header[8];
static uint8_t s_payload[COMM_TOOL_SERIAL_MAX_PAYLOAD];
static uint16_t s_header_index;
static uint16_t s_payload_index;
static uint16_t s_payload_len;
static uint8_t s_cmd;
static uint16_t s_seq;
static uint8_t s_node_id;
static uint8_t s_crc_bytes[2];

static uint16_t ReadLe16(const uint8_t *data)
{
    return (uint16_t)data[0] | ((uint16_t)data[1] << 8u);
}

static uint32_t ReadLe32(const uint8_t *data)
{
    return (uint32_t)data[0] |
           ((uint32_t)data[1] << 8u) |
           ((uint32_t)data[2] << 16u) |
           ((uint32_t)data[3] << 24u);
}

static void WriteLe16(uint8_t *data, uint16_t value)
{
    data[0] = (uint8_t)value;
    data[1] = (uint8_t)(value >> 8u);
}

static void WriteLe32(uint8_t *data, uint32_t value)
{
    data[0] = (uint8_t)value;
    data[1] = (uint8_t)(value >> 8u);
    data[2] = (uint8_t)(value >> 16u);
    data[3] = (uint8_t)(value >> 24u);
}

static void SendResponse(uint8_t cmd, uint16_t seq, const uint8_t *payload, uint16_t length)
{
    uint8_t frame[COMM_TOOL_SERIAL_MAX_FRAME];
    uint16_t crc;
    uint16_t frame_len;

    if (length > COMM_TOOL_SERIAL_MAX_PAYLOAD) {
        return;
    }

    frame[0] = SERIAL_SOF0;
    frame[1] = SERIAL_SOF1;
    frame[2] = COMM_TOOL_PROTOCOL_VERSION;
    frame[3] = (uint8_t)(cmd | 0x80u);
    WriteLe16(&frame[4], seq);
    WriteLe16(&frame[6], length);
    if ((payload != 0) && (length > 0u)) {
        memcpy(&frame[8], payload, length);
    }

    frame_len = (uint16_t)(8u + length);
    crc = Crc16_Modbus(frame, frame_len);
    frame[frame_len] = (uint8_t)crc;
    frame[frame_len + 1u] = (uint8_t)(crc >> 8u);
    BoardUart_Send(frame, (uint16_t)(frame_len + 2u));
}

static void SendStatus(uint8_t cmd, uint16_t seq, uint8_t status)
{
    uint8_t payload[1];

    payload[0] = status;
    SendResponse(cmd, seq, payload, 1u);
}

static void HandleGetInfo(uint16_t seq)
{
    uint8_t payload[28];

    memset(payload, 0, sizeof(payload));
    payload[0] = CT_STATUS_OK;
    payload[1] = COMM_TOOL_PROTOCOL_VERSION;
    payload[2] = COMM_TOOL_FW_VERSION_MAJOR;
    payload[3] = COMM_TOOL_FW_VERSION_MINOR;
    WriteLe32(&payload[4], COMM_TOOL_FLASH_SIZE);
    WriteLe32(&payload[8], COMM_TOOL_BMS_FW_CACHE_BASE);
    WriteLe32(&payload[12], COMM_TOOL_BMS_FW_CACHE_SIZE);
    WriteLe32(&payload[16], COMM_TOOL_UPGRADE_INDEX_BASE);
    WriteLe32(&payload[20], COMM_TOOL_APP_BASE);
    WriteLe32(&payload[24], COMM_TOOL_APP_SIZE);
    SendResponse(CMD_GET_INFO, seq, payload, sizeof(payload));
}

static void HandleFwInfo(uint16_t seq)
{
    uint8_t payload[12];
    FlashStoreInfo info;

    FlashStore_GetInfo(&info);
    payload[0] = CT_STATUS_OK;
    payload[1] = (uint8_t)info.valid;
    payload[2] = 0u;
    payload[3] = 0u;
    WriteLe32(&payload[4], info.image_size);
    WriteLe16(&payload[8], info.image_crc16);
    WriteLe16(&payload[10], info.calc_crc16);
    SendResponse(CMD_FW_INFO, seq, payload, sizeof(payload));
}

static void HandleUpgradeStatus(uint16_t seq)
{
    uint8_t payload[5];

    payload[0] = CT_STATUS_OK;
    payload[1] = (uint8_t)UpgradeManager_GetState();
    WriteLe16(&payload[2], UpgradeManager_GetError());
    payload[4] = 0u;
    SendResponse(CMD_BMS_UPGRADE_STATUS, seq, payload, sizeof(payload));
}

static void ProcessCommand(uint8_t cmd, uint16_t seq, const uint8_t *payload, uint16_t length)
{
    uint32_t size;
    uint32_t offset;
    uint16_t crc;
    uint16_t address;
    uint16_t value;
    uint16_t count;
    uint8_t node_id;

    switch (cmd) {
    case CMD_GET_INFO:
        HandleGetInfo(seq);
        break;

    case CMD_SET_CONFIG:
        if (length >= 1u) {
            s_node_id = payload[0];
            SendStatus(cmd, seq, CT_STATUS_OK);
        } else {
            SendStatus(cmd, seq, CT_STATUS_LENGTH_ERROR);
        }
        break;

    case CMD_BMS_READ_REGS:
        if (length < 5u) {
            SendStatus(cmd, seq, CT_STATUS_LENGTH_ERROR);
            break;
        }
        node_id = payload[0];
        address = ReadLe16(&payload[1]);
        count = ReadLe16(&payload[3]);
        SendStatus(cmd, seq, CanGateway_ReadRegs(node_id, address, count) ? CT_STATUS_OK : CT_STATUS_CAN_ERROR);
        break;

    case CMD_BMS_WRITE_REG:
        if (length < 5u) {
            SendStatus(cmd, seq, CT_STATUS_LENGTH_ERROR);
            break;
        }
        node_id = payload[0];
        address = ReadLe16(&payload[1]);
        value = ReadLe16(&payload[3]);
        SendStatus(cmd, seq, CanGateway_WriteReg(node_id, address, value) ? CT_STATUS_OK : CT_STATUS_CAN_ERROR);
        break;

    case CMD_BMS_WRITE_REGS:
        SendStatus(cmd, seq, CT_STATUS_UNSUPPORTED);
        break;

    case CMD_FW_BEGIN:
        if (length < 6u) {
            SendStatus(cmd, seq, CT_STATUS_LENGTH_ERROR);
            break;
        }
        size = ReadLe32(&payload[0]);
        crc = ReadLe16(&payload[4]);
        SendStatus(cmd, seq, FlashStore_Begin(size, crc) ? CT_STATUS_OK : CT_STATUS_FLASH_ERROR);
        break;

    case CMD_FW_DATA:
        if (length < 5u) {
            SendStatus(cmd, seq, CT_STATUS_LENGTH_ERROR);
            break;
        }
        offset = ReadLe32(&payload[0]);
        SendStatus(cmd, seq, FlashStore_Write(offset, &payload[4], (uint16_t)(length - 4u)) ? CT_STATUS_OK : CT_STATUS_FLASH_ERROR);
        break;

    case CMD_FW_END_VERIFY:
        if (length < 6u) {
            SendStatus(cmd, seq, CT_STATUS_LENGTH_ERROR);
            break;
        }
        size = ReadLe32(&payload[0]);
        crc = ReadLe16(&payload[4]);
        SendStatus(cmd, seq, FlashStore_Finalize(size, crc) ? CT_STATUS_OK : CT_STATUS_FW_INVALID);
        break;

    case CMD_FW_INFO:
        HandleFwInfo(seq);
        break;

    case CMD_BMS_UPGRADE_START:
        node_id = (length >= 1u) ? payload[0] : s_node_id;
        SendStatus(cmd, seq, UpgradeManager_Start(node_id) ? CT_STATUS_OK : CT_STATUS_FW_INVALID);
        break;

    case CMD_BMS_UPGRADE_STATUS:
        HandleUpgradeStatus(seq);
        break;

    case CMD_BMS_UPGRADE_ABORT:
        UpgradeManager_Abort();
        SendStatus(cmd, seq, CT_STATUS_OK);
        break;

    default:
        SendStatus(cmd, seq, CT_STATUS_UNSUPPORTED);
        break;
    }
}

static void ResetParser(void)
{
    s_state = PARSE_WAIT_SOF0;
    s_header_index = 0u;
    s_payload_index = 0u;
    s_payload_len = 0u;
    s_cmd = 0u;
    s_seq = 0u;
}

void SerialProto_Init(void)
{
    s_node_id = COMM_TOOL_DEFAULT_NODE_ID;
    ResetParser();
}

void SerialProto_Poll(void)
{
    uint8_t value;
    uint8_t crc_payload[COMM_TOOL_SERIAL_MAX_FRAME];
    uint16_t crc_calc;
    uint16_t crc_recv;

    while (BoardUart_ReadByte(&value) != 0u) {
        switch (s_state) {
        case PARSE_WAIT_SOF0:
            if (value == SERIAL_SOF0) {
                s_header[0] = value;
                s_header_index = 1u;
                s_state = PARSE_WAIT_SOF1;
            }
            break;

        case PARSE_WAIT_SOF1:
            if (value == SERIAL_SOF1) {
                s_header[1] = value;
                s_header_index = 2u;
                s_state = PARSE_HEADER;
            } else {
                ResetParser();
            }
            break;

        case PARSE_HEADER:
            s_header[s_header_index++] = value;
            if (s_header_index >= 8u) {
                if (s_header[2] != COMM_TOOL_PROTOCOL_VERSION) {
                    ResetParser();
                    break;
                }
                s_cmd = s_header[3];
                s_seq = ReadLe16(&s_header[4]);
                s_payload_len = ReadLe16(&s_header[6]);
                if (s_payload_len > COMM_TOOL_SERIAL_MAX_PAYLOAD) {
                    ResetParser();
                    break;
                }
                s_payload_index = 0u;
                s_state = (s_payload_len == 0u) ? PARSE_CRC0 : PARSE_PAYLOAD;
            }
            break;

        case PARSE_PAYLOAD:
            s_payload[s_payload_index++] = value;
            if (s_payload_index >= s_payload_len) {
                s_state = PARSE_CRC0;
            }
            break;

        case PARSE_CRC0:
            s_crc_bytes[0] = value;
            s_state = PARSE_CRC1;
            break;

        case PARSE_CRC1:
            s_crc_bytes[1] = value;
            memcpy(crc_payload, s_header, 8u);
            if (s_payload_len > 0u) {
                memcpy(&crc_payload[8], s_payload, s_payload_len);
            }
            crc_calc = Crc16_Modbus(crc_payload, (uint16_t)(8u + s_payload_len));
            crc_recv = ReadLe16(s_crc_bytes);
            if (crc_calc == crc_recv) {
                ProcessCommand(s_cmd, s_seq, s_payload, s_payload_len);
            } else {
                SendStatus(s_cmd, s_seq, CT_STATUS_CRC_ERROR);
            }
            ResetParser();
            break;

        default:
            ResetParser();
            break;
        }
    }
}
