#include "LegacyModbusProtocol.h"

#include <string.h>

/* Sci_Upper.c 中已有、但历史头文件未公开的旧协议业务入口。 */
extern void CRC_verify(struct RS485MSG *s);
extern void Sci_Deal_ReadRegs_0x03(struct RS485MSG *s);
extern void Sci_Deal_WrReg_0x06(struct RS485MSG *s);
extern void Sci_Deal_WrRegs_0x10(struct RS485MSG *s);
extern void Sci_ACK_0x03(struct RS485MSG *s);
extern void Sci_ACK_0x06_0x10(struct RS485MSG *s);

void LegacyModbusProtocol_Reset(LEGACY_MODBUS_PROTOCOL_CONTEXT *ctx)
{
    struct RS485MSG *s;

    if (ctx == 0)
    {
        return;
    }

    s = &ctx->msg;
    s->ptr_no = 0U;
    s->csr = RS485_STA_IDLE;
    s->u16RdRegStartAddr = 0U;
    s->u16RdRegStartAddrActure = 0U;
    s->u16RdRegByteNum = 0U;
    s->AckLenth = 0U;
    s->AckType = RS485_ACK_POS;
    s->ErrorType = RS485_ERROR_NULL;
    s->enRs485CmdType = RS485_CMD_READ_REGS;
    s->u16Buffer[0] = 0U;
    s->u16Buffer[1] = 0U;
    s->u16Buffer[2] = 0U;
    s->u16Buffer[3] = 0U;
}

void LegacyModbusProtocol_Init(LEGACY_MODBUS_PROTOCOL_CONTEXT *ctx)
{
    if (ctx == 0)
    {
        return;
    }

    memset(ctx, 0, sizeof(*ctx));
    LegacyModbusProtocol_Reset(ctx);
}

uint8_t LegacyModbusProtocol_IsStartByte(uint8_t data)
{
    return (uint8_t)(((data == RS485_SLAVE_ADDR) ||
                      (data == RS485_BROADCAST_ADDR)) ? 1U : 0U);
}

uint8_t LegacyModbusProtocol_IsBusy(const LEGACY_MODBUS_PROTOCOL_CONTEXT *ctx)
{
    const struct RS485MSG *s;

    if (ctx == 0)
    {
        return 0U;
    }

    s = &ctx->msg;
    return (uint8_t)(((s->ptr_no != 0U) || (s->csr != RS485_STA_IDLE)) ? 1U : 0U);
}

uint8_t LegacyModbusProtocol_Feed(LEGACY_MODBUS_PROTOCOL_CONTEXT *ctx, uint8_t data)
{
    struct RS485MSG *s;
    UINT16 frame_end_index;

    if (ctx == 0)
    {
        return 0U;
    }

    s = &ctx->msg;
    if (s->ptr_no >= RS485_MAX_BUFFER_SIZE)
    {
        LegacyModbusProtocol_Reset(ctx);
    }

    s->u16Buffer[s->ptr_no] = data;

    if (s->ptr_no == 0U)
    {
        if (!LegacyModbusProtocol_IsStartByte(data))
        {
            LegacyModbusProtocol_Reset(ctx);
            return 0U;
        }
    }
    else if (s->ptr_no == 1U)
    {
        switch (data)
        {
        case RS485_CMD_READ_REGS:
            s->enRs485CmdType = RS485_CMD_READ_REGS;
            break;
        case RS485_CMD_WRITE_REG:
            s->enRs485CmdType = RS485_CMD_WRITE_REG;
            break;
        case RS485_CMD_WRITE_REGS:
            s->enRs485CmdType = RS485_CMD_WRITE_REGS;
            break;
        default:
            LegacyModbusProtocol_Reset(ctx);
            return 0U;
        }
    }
    else
    {
        switch (s->enRs485CmdType)
        {
        case RS485_CMD_READ_REGS:
        case RS485_CMD_WRITE_REG:
            if (s->ptr_no == 7U)
            {
                s->csr = RS485_STA_RX_COMPLETE;
                ++s->ptr_no;
                return 1U;
            }
            break;

        case RS485_CMD_WRITE_REGS:
            if (s->ptr_no == 6U)
            {
                frame_end_index = (UINT16)s->u16Buffer[6] + 8U;
                if (frame_end_index >= RS485_MAX_BUFFER_SIZE)
                {
                    LegacyModbusProtocol_Reset(ctx);
                    return 0U;
                }
            }
            if (s->ptr_no >= 7U)
            {
                frame_end_index = (UINT16)s->u16Buffer[6] + 8U;
                if (s->ptr_no == frame_end_index)
                {
                    s->csr = RS485_STA_RX_COMPLETE;
                    ++s->ptr_no;
                    return 1U;
                }
            }
            break;

        default:
            LegacyModbusProtocol_Reset(ctx);
            return 0U;
        }
    }

    ++s->ptr_no;
    if (s->ptr_no >= RS485_MAX_BUFFER_SIZE)
    {
        LegacyModbusProtocol_Reset(ctx);
    }
    return 0U;
}

static uint8_t LegacyModbusProtocol_FixedSeriesWriteIsValid(const struct RS485MSG *s)
{
    UINT16 start;
    UINT16 count;
    UINT16 offset;
    UINT16 data_index;
    UINT16 value;

    if (s->enRs485CmdType != RS485_CMD_WRITE_REGS)
    {
        return 1U;
    }

    start = (UINT16)(((UINT16)s->u16Buffer[2] << 8) | s->u16Buffer[3]);
    count = (UINT16)(((UINT16)s->u16Buffer[4] << 8) | s->u16Buffer[5]);

    if ((count == 0U) || (RS485_CMD_ADDR_SYS_SERIES_NUM < start))
    {
        return 1U;
    }

    offset = (UINT16)(RS485_CMD_ADDR_SYS_SERIES_NUM - start);
    if (offset >= count)
    {
        return 1U;
    }

    data_index = (UINT16)(7U + (offset << 1));
    if ((data_index + 1U) >= s->ptr_no)
    {
        return 0U;
    }

    value = (UINT16)(((UINT16)s->u16Buffer[data_index] << 8) |
                     s->u16Buffer[data_index + 1U]);
    return (uint8_t)((value == (UINT16)PROJECT_CFG_SERIES_NUM) ? 1U : 0U);
}

void LegacyModbusProtocol_Process(LEGACY_MODBUS_PROTOCOL_CONTEXT *ctx)
{
    struct RS485MSG *s;

    if (ctx == 0)
    {
        return;
    }

    s = &ctx->msg;
    s->AckType = RS485_ACK_POS;
    s->ErrorType = RS485_ERROR_NULL;

    /* 完全沿用旧实现的 CRC 错误应答行为。 */
    CRC_verify(s);
    if (s->AckType == RS485_ACK_POS)
    {
        switch (s->enRs485CmdType)
        {
        case RS485_CMD_READ_REGS:
            Sci_Deal_ReadRegs_0x03(s);
            break;

        case RS485_CMD_WRITE_REG:
            Sci_Deal_WrReg_0x06(s);
            break;

        case RS485_CMD_WRITE_REGS:
            if (!LegacyModbusProtocol_FixedSeriesWriteIsValid(s))
            {
                s->AckType = RS485_ACK_NEG;
                s->ErrorType = RS485_ERROR_DATA_INVALID;
            }
            else
            {
                Sci_Deal_WrRegs_0x10(s);
            }
            break;

        default:
            s->u16RdRegByteNum = 0U;
            s->AckType = RS485_ACK_NEG;
            s->ErrorType = RS485_ERROR_CMD_INVALID;
            break;
        }
    }

    switch (s->enRs485CmdType)
    {
    case RS485_CMD_READ_REGS:
        Sci_ACK_0x03(s);
        break;
    case RS485_CMD_WRITE_REG:
    case RS485_CMD_WRITE_REGS:
        Sci_ACK_0x06_0x10(s);
        break;
    default:
        LegacyModbusProtocol_Reset(ctx);
        break;
    }
}

void LegacyModbusProtocol_OnRxIdle(LEGACY_MODBUS_PROTOCOL_CONTEXT *ctx)
{
    if ((ctx != 0) &&
        (ctx->msg.ptr_no != 0U) &&
        (ctx->msg.csr == RS485_STA_IDLE))
    {
        LegacyModbusProtocol_Reset(ctx);
    }
}

uint8_t *LegacyModbusProtocol_GetTxBuffer(LEGACY_MODBUS_PROTOCOL_CONTEXT *ctx)
{
    return (ctx != 0) ? ctx->msg.u16Buffer : 0;
}

uint16_t LegacyModbusProtocol_GetTxLength(const LEGACY_MODBUS_PROTOCOL_CONTEXT *ctx)
{
    return (ctx != 0) ? ctx->msg.AckLenth : 0U;
}
