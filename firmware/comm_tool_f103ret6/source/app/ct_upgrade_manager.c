#include "ct_upgrade_manager.h"
#include "ct_can_gateway.h"
#include "ct_config.h"
#include "ct_crc16.h"
#include "ct_flash_store.h"
#include <string.h>

enum
{
    CT_UPGRADE_IDLE = 0u,
    CT_UPGRADE_RUNNING = 1u,
    CT_UPGRADE_DONE = 2u,
    CT_UPGRADE_ERROR = 3u,
    CT_UPGRADE_ABORTED = 4u
};

static CtUpgradeStatus s_status;
static uint8_t s_abort;

void CtUpgrade_Init(void)
{
    memset(&s_status, 0, sizeof(s_status));
    s_status.state = CT_UPGRADE_IDLE;
    s_abort = 0u;
}

const CtUpgradeStatus *CtUpgrade_GetStatus(void)
{
    return &s_status;
}

void CtUpgrade_Abort(void)
{
    s_abort = 1u;
    if (s_status.state == CT_UPGRADE_RUNNING)
    {
        s_status.state = CT_UPGRADE_ABORTED;
    }
}

static void set_error(uint8_t err)
{
    s_status.state = CT_UPGRADE_ERROR;
    s_status.last_error = err;
}

int CtUpgrade_Start(uint8_t node)
{
    const CtFirmwareInfo *info;
    uint8_t frame[8];
    uint8_t block[CT_IAP_BLOCK_BYTES];
    uint32_t offset;
    uint16_t seq;
    uint16_t block_seq;
    uint16_t frame_count;
    uint16_t chunk_len;
    uint16_t block_crc;
    uint8_t i;

    info = CtFlash_GetInfo();
    if ((info->valid == 0u) || (info->app_addr != CT_BMS_APP_BASE_ADDR) || (info->size == 0u))
    {
        set_error(1u);
        return 0;
    }

    memset(&s_status, 0, sizeof(s_status));
    s_status.state = CT_UPGRADE_RUNNING;
    s_status.total = info->size;
    s_abort = 0u;

    if (!CtCan_IapSendHello(node) || !CtCan_IapWaitAck(node, CT_CAN_IAP_HELLO, &s_status.expect_seq, 1000u))
    {
        set_error(2u);
        return 0;
    }
    if (!CtCan_IapSendStart(node, info->size, info->crc16) ||
        !CtCan_IapWaitAck(node, CT_CAN_IAP_START, &s_status.expect_seq, 2000u))
    {
        set_error(3u);
        return 0;
    }

    seq = 0u;
    block_seq = 0u;
    for (offset = 0u; offset < info->size; offset += CT_IAP_BLOCK_BYTES)
    {
        if (s_abort != 0u)
        {
            s_status.state = CT_UPGRADE_ABORTED;
            return 0;
        }

        chunk_len = (uint16_t)((info->size - offset) > CT_IAP_BLOCK_BYTES ? CT_IAP_BLOCK_BYTES : (info->size - offset));
        memset(block, 0xFF, sizeof(block));
        if (!CtFlash_Read(offset, block, chunk_len))
        {
            set_error(4u);
            return 0;
        }

        for (i = 0u; i < CT_IAP_BLOCK_FRAMES; ++i)
        {
            memcpy(frame, &block[(uint16_t)i * 8u], 8u);
            if (!CtCan_IapSendData(node, seq, frame))
            {
                set_error(5u);
                return 0;
            }
            seq++;
        }

        block_crc = CtCrc16_Calc(block, chunk_len);
        if (!CtCan_IapSendCommit(node, block_seq, chunk_len, block_crc) ||
            !CtCan_IapWaitAck(node, CT_CAN_IAP_COMMIT, &s_status.expect_seq, 2000u))
        {
            set_error(6u);
            return 0;
        }

        s_status.written = offset + chunk_len;
        s_status.percent = (uint8_t)((s_status.written * 100u) / s_status.total);
        block_seq++;
    }

    frame_count = (uint16_t)((info->size + 7u) / 8u);
    if (!CtCan_IapSendEnd(node, frame_count, info->crc16) ||
        !CtCan_IapWaitAck(node, CT_CAN_IAP_END, &s_status.expect_seq, 5000u))
    {
        set_error(7u);
        return 0;
    }

    s_status.percent = 100u;
    s_status.state = CT_UPGRADE_DONE;
    return 1;
}

