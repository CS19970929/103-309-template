#include "upg_feidao.h"

#include <string.h>

uint32_t UpgFeidao_BuildId(uint8_t src, uint8_t dst, uint8_t ctrl, uint8_t index, uint8_t chd)
{
    return (((uint32_t)(src & 0x1FU) << 24U) |
            ((uint32_t)(dst & 0x1FU) << 19U) |
            ((uint32_t)(ctrl & 0x07U) << 16U) |
            ((uint32_t)index << 8U) |
            (uint32_t)chd);
}

UpgFeidaoId UpgFeidao_DecodeId(uint32_t id)
{
    UpgFeidaoId value;

    value.src = (uint8_t)((id >> 24U) & 0x1FU);
    value.dst = (uint8_t)((id >> 19U) & 0x1FU);
    value.ctrl = (uint8_t)((id >> 16U) & 0x07U);
    value.index = (uint8_t)((id >> 8U) & 0xFFU);
    value.chd = (uint8_t)(id & 0xFFU);
    return value;
}

void UpgFeidao_MakeFrame(UpgCanFrame *frame, uint8_t src, uint8_t dst, uint8_t ctrl, uint8_t index, uint8_t chd, const uint8_t data[8])
{
    frame->id = UpgFeidao_BuildId(src, dst, ctrl, index, chd);
    frame->extended = 1U;
    frame->dlc = 8U;
    if (data != 0)
    {
        memcpy(frame->data, data, 8U);
    }
    else
    {
        memset(frame->data, 0, 8U);
    }
}

uint8_t UpgFeidao_IsAckFor(const UpgCanFrame *frame, uint8_t src, uint8_t dst, uint8_t index, uint8_t chd)
{
    UpgFeidaoId id;

    if ((frame == 0) || (frame->extended == 0U) || (frame->dlc != 8U))
    {
        return 0U;
    }
    id = UpgFeidao_DecodeId(frame->id);
    return ((id.src == src) &&
            (id.dst == dst) &&
            (id.ctrl == FEIDAO_CTRL_ACK) &&
            (id.index == index) &&
            (id.chd == chd)) ? 1U : 0U;
}

uint8_t UpgFeidao_IsErrAckFor(const UpgCanFrame *frame, uint8_t src, uint8_t dst, uint8_t index, uint8_t chd)
{
    UpgFeidaoId id;

    if ((frame == 0) || (frame->extended == 0U) || (frame->dlc != 8U))
    {
        return 0U;
    }
    id = UpgFeidao_DecodeId(frame->id);
    return ((id.src == src) &&
            (id.dst == dst) &&
            (id.ctrl == FEIDAO_CTRL_ERR_ACK) &&
            (id.index == index) &&
            (id.chd == chd)) ? 1U : 0U;
}
