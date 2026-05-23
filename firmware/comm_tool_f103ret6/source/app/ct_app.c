#include "ct_app.h"
#include "ct_board_port.h"
#include "ct_can_gateway.h"
#include "ct_config.h"
#include "ct_flash_store.h"
#include "ct_status.h"
#include "ct_upgrade_manager.h"
#include <string.h>

static uint8_t s_tx[10u + CT_UART_MAX_PAYLOAD + 2u];
static uint32_t s_can_bitrate = CT_CAN_DEFAULT_BITRATE;
static uint8_t s_node_id = CT_NODE_ID_DEFAULT;

static uint16_t rd16(const uint8_t *p)
{
    return (uint16_t)p[0] | ((uint16_t)p[1] << 8);
}

static uint32_t rd32(const uint8_t *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static void wr16(uint8_t *p, uint16_t value)
{
    p[0] = (uint8_t)value;
    p[1] = (uint8_t)(value >> 8);
}

static void wr32(uint8_t *p, uint32_t value)
{
    p[0] = (uint8_t)value;
    p[1] = (uint8_t)(value >> 8);
    p[2] = (uint8_t)(value >> 16);
    p[3] = (uint8_t)(value >> 24);
}

static void respond(const CtFrame *req, uint8_t status, const uint8_t *payload, uint16_t length)
{
    uint16_t frame_len;

    frame_len = CtProtocol_Encode(req->seq,
                                  req->cmd,
                                  status,
                                  payload,
                                  length,
                                  CT_UART_FLAG_ACK,
                                  s_tx,
                                  (uint16_t)sizeof(s_tx));
    if (frame_len != 0u)
    {
        CtBoard_UartWrite(s_tx, frame_len);
    }
}

void CtApp_Init(void)
{
    CtFlash_Init();
    CtUpgrade_Init();
}

static void handle_info(const CtFrame *req)
{
    uint8_t payload[20];

    payload[0] = CT_PROTOCOL_VERSION;
    payload[1] = CT_FW_VERSION_MAJOR;
    payload[2] = CT_FW_VERSION_MINOR;
    payload[3] = CT_FW_VERSION_PATCH;
    wr32(&payload[4], s_can_bitrate);
    wr32(&payload[8], CT_FW_CACHE_BASE);
    wr32(&payload[12], CT_FW_CACHE_SIZE);
    wr32(&payload[16], 0u);
    respond(req, CT_STATUS_OK, payload, (uint16_t)sizeof(payload));
}

static void handle_set_can(const CtFrame *req)
{
    if (req->length < 8u)
    {
        respond(req, CT_STATUS_BAD_PARAM, 0, 0u);
        return;
    }
    s_can_bitrate = rd32(&req->payload[0]);
    s_node_id = req->payload[4];
    if (!CtBoard_SetCanBitrate(s_can_bitrate))
    {
        respond(req, CT_STATUS_BAD_PARAM, 0, 0u);
        return;
    }
    respond(req, CT_STATUS_OK, 0, 0u);
}

static void handle_fw_begin(const CtFrame *req)
{
    uint32_t app_addr;
    uint32_t size;
    uint16_t crc16;
    uint32_t crc32;

    if (req->length < 14u)
    {
        respond(req, CT_STATUS_BAD_PARAM, 0, 0u);
        return;
    }
    app_addr = rd32(&req->payload[0]);
    size = rd32(&req->payload[4]);
    crc16 = rd16(&req->payload[8]);
    crc32 = rd32(&req->payload[10]);
    if (!CtFlash_Begin(app_addr, size, crc16, crc32))
    {
        respond(req, CT_STATUS_FLASH_ERROR, 0, 0u);
        return;
    }
    respond(req, CT_STATUS_OK, 0, 0u);
}

static void handle_fw_data(const CtFrame *req)
{
    uint32_t offset;

    if (req->length < 5u)
    {
        respond(req, CT_STATUS_BAD_PARAM, 0, 0u);
        return;
    }
    offset = rd32(&req->payload[0]);
    if (!CtFlash_Write(offset, &req->payload[4], (uint16_t)(req->length - 4u)))
    {
        respond(req, CT_STATUS_FLASH_ERROR, 0, 0u);
        return;
    }
    respond(req, CT_STATUS_OK, 0, 0u);
}

static void handle_fw_end(const CtFrame *req)
{
    uint32_t size;
    uint16_t crc16;
    uint32_t crc32;

    if (req->length < 10u)
    {
        respond(req, CT_STATUS_BAD_PARAM, 0, 0u);
        return;
    }
    size = rd32(&req->payload[0]);
    crc16 = rd16(&req->payload[4]);
    crc32 = rd32(&req->payload[6]);
    if (!CtFlash_End(size, crc16, crc32))
    {
        respond(req, CT_STATUS_FLASH_ERROR, 0, 0u);
        return;
    }
    respond(req, CT_STATUS_OK, 0, 0u);
}

static void handle_fw_info(const CtFrame *req)
{
    const CtFirmwareInfo *info;
    uint8_t payload[15];

    info = CtFlash_GetInfo();
    wr32(&payload[0], info->app_addr);
    wr32(&payload[4], info->size);
    wr16(&payload[8], info->crc16);
    wr32(&payload[10], info->crc32);
    payload[14] = (uint8_t)(info->valid != 0u);
    respond(req, CT_STATUS_OK, payload, (uint16_t)sizeof(payload));
}

static void handle_bms_read(const CtFrame *req)
{
    uint8_t soc;
    uint8_t soh;
    uint8_t payload[4];
    uint16_t addr;
    uint16_t count;

    if (req->length < 4u)
    {
        respond(req, CT_STATUS_BAD_PARAM, 0, 0u);
        return;
    }
    addr = rd16(&req->payload[0]);
    count = rd16(&req->payload[2]);
    if ((addr == 0xD000u) && (count >= 2u))
    {
        if (!CtCan_AppGetStatus(0u, &soc, &soh))
        {
            respond(req, CT_STATUS_CAN_TIMEOUT, 0, 0u);
            return;
        }
        wr16(&payload[0], soc);
        wr16(&payload[2], soh);
        respond(req, CT_STATUS_OK, payload, 4u);
        return;
    }
    respond(req, CT_STATUS_UNSUPPORTED, 0, 0u);
}

static void handle_enter_iap(const CtFrame *req)
{
    if (!CtCan_AppEnterIap(0u))
    {
        respond(req, CT_STATUS_CAN_TIMEOUT, 0, 0u);
        return;
    }
    respond(req, CT_STATUS_OK, 0, 0u);
}

static void handle_upgrade(const CtFrame *req)
{
    if (!CtUpgrade_Start(s_node_id))
    {
        respond(req, CT_STATUS_BMS_ERROR, 0, 0u);
        return;
    }
    respond(req, CT_STATUS_OK, 0, 0u);
}

static void handle_upgrade_status(const CtFrame *req)
{
    const CtUpgradeStatus *status;
    uint8_t payload[13];

    status = CtUpgrade_GetStatus();
    payload[0] = status->state;
    payload[1] = status->percent;
    payload[2] = status->last_error;
    wr32(&payload[3], status->written);
    wr32(&payload[7], status->total);
    wr16(&payload[11], status->expect_seq);
    respond(req, CT_STATUS_OK, payload, (uint16_t)sizeof(payload));
}

void CtApp_HandleFrame(const CtFrame *frame)
{
    if (frame == 0)
    {
        return;
    }

    switch (frame->cmd)
    {
    case CT_CMD_GET_INFO:
        handle_info(frame);
        break;
    case CT_CMD_SET_CAN:
        handle_set_can(frame);
        break;
    case CT_CMD_FW_BEGIN:
        handle_fw_begin(frame);
        break;
    case CT_CMD_FW_DATA:
        handle_fw_data(frame);
        break;
    case CT_CMD_FW_END:
        handle_fw_end(frame);
        break;
    case CT_CMD_FW_INFO:
        handle_fw_info(frame);
        break;
    case CT_CMD_BMS_READ:
        handle_bms_read(frame);
        break;
    case CT_CMD_BMS_WRITE:
        respond(frame, CT_STATUS_UNSUPPORTED, 0, 0u);
        break;
    case CT_CMD_ENTER_IAP:
        handle_enter_iap(frame);
        break;
    case CT_CMD_UPGRADE:
        handle_upgrade(frame);
        break;
    case CT_CMD_UPGRADE_STATUS:
        handle_upgrade_status(frame);
        break;
    case CT_CMD_UPGRADE_ABORT:
        CtUpgrade_Abort();
        respond(frame, CT_STATUS_OK, 0, 0u);
        break;
    default:
        respond(frame, CT_STATUS_UNSUPPORTED, 0, 0u);
        break;
    }
}
