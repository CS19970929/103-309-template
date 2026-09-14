#include "LxPowerProtocol.h"

#include <string.h>

#define LX_FRAME_HEAD_0                 0x5AU
#define LX_FRAME_HEAD_1                 0xA5U
#define LX_FRAME_END                    0xF0U
#define LX_DATA2_END                    0xFAU

#define LX_DD_HEAD                      0xDDU
#define LX_DD_READ                      0xA5U
#define LX_DD_WRITE                     0x5AU
#define LX_DD_END                       0x77U

#define LX_CMD_REQUEST_DATA1            0x04U
#define LX_CMD_REQUEST_DATA2            0x05U
#define LX_CMD_REQUEST_DATA3            0x03U
#define LX_CMD_RESPONSE_DATA1           0x01U
#define LX_CMD_RESPONSE_DATA3           0x03U

#define LX_DD_CMD_DATA4                 0xAAU
#define LX_DD_CMD_CLEAR_DATA4           0x01U
#define LX_DD_CMD_MOS                   0xE1U

static void Lx_ResetRx(LX_POWER_PROTOCOL_CONTEXT *ctx)
{
    ctx->rx_length = 0U;
    ctx->expected_length = 0U;
}

static void Lx_RestartWithByte(LX_POWER_PROTOCOL_CONTEXT *ctx, uint8_t data)
{
    Lx_ResetRx(ctx);
    if ((data == LX_FRAME_HEAD_0) || (data == LX_DD_HEAD))
    {
        ctx->rx_buffer[0] = data;
        ctx->rx_length = 1U;
    }
}

static void Lx_PutLe16(uint8_t *buffer, uint16_t *index, uint16_t value)
{
    buffer[(*index)++] = (uint8_t)(value & 0xFFU);
    buffer[(*index)++] = (uint8_t)(value >> 8);
}

static void Lx_PutBe16(uint8_t *buffer, uint16_t *index, uint16_t value)
{
    buffer[(*index)++] = (uint8_t)(value >> 8);
    buffer[(*index)++] = (uint8_t)(value & 0xFFU);
}

static uint8_t Lx_Xor(const uint8_t *data, uint16_t length)
{
    uint16_t i;
    uint8_t value = 0U;

    for (i = 0U; i < length; ++i)
    {
        value ^= data[i];
    }
    return value;
}

static uint16_t Lx_TwosComplement(const uint8_t *data, uint16_t length)
{
    uint16_t i;
    uint16_t sum = 0U;

    for (i = 0U; i < length; ++i)
    {
        sum = (uint16_t)(sum + data[i]);
    }
    return (uint16_t)(0U - sum);
}

static uint8_t Lx_MainRequestIsValid(const LX_POWER_PROTOCOL_CONTEXT *ctx)
{
    uint16_t length;
    uint16_t bcc_index;
    uint16_t end_index;

    if ((ctx->rx_length < 6U) ||
        (ctx->rx_buffer[0] != LX_FRAME_HEAD_0) ||
        (ctx->rx_buffer[1] != LX_FRAME_HEAD_1))
    {
        return 0U;
    }

    length = ctx->rx_buffer[3];
    if (ctx->rx_length != (uint16_t)(6U + length))
    {
        return 0U;
    }

    bcc_index = (uint16_t)(4U + length);
    end_index = (uint16_t)(5U + length);
    if (ctx->rx_buffer[end_index] != LX_FRAME_END)
    {
        return 0U;
    }

    return (Lx_Xor(&ctx->rx_buffer[2], (uint16_t)(2U + length)) ==
            ctx->rx_buffer[bcc_index]) ? 1U : 0U;
}

static uint8_t Lx_DdRequestIsValid(const LX_POWER_PROTOCOL_CONTEXT *ctx)
{
    uint16_t length;
    uint16_t checksum_index;
    uint16_t end_index;
    uint16_t received;
    uint16_t calculated;

    if ((ctx->rx_length < 7U) || (ctx->rx_buffer[0] != LX_DD_HEAD))
    {
        return 0U;
    }

    if ((ctx->rx_buffer[1] != LX_DD_READ) &&
        (ctx->rx_buffer[1] != LX_DD_WRITE))
    {
        return 0U;
    }

    length = ctx->rx_buffer[3];
    if (ctx->rx_length != (uint16_t)(7U + length))
    {
        return 0U;
    }

    checksum_index = (uint16_t)(4U + length);
    end_index = (uint16_t)(6U + length);
    if (ctx->rx_buffer[end_index] != LX_DD_END)
    {
        return 0U;
    }

    received = (uint16_t)(((uint16_t)ctx->rx_buffer[checksum_index] << 8) |
                          ctx->rx_buffer[checksum_index + 1U]);
    calculated = Lx_TwosComplement(&ctx->rx_buffer[2], (uint16_t)(2U + length));
    return (received == calculated) ? 1U : 0U;
}

static void Lx_BuildData1(LX_POWER_PROTOCOL_CONTEXT *ctx)
{
    LX_POWER_PROTOCOL_DATA1 data;
    uint16_t i;
    uint16_t index = 0U;

    if ((ctx->adapter == 0) || (ctx->adapter->get_data1 == 0))
    {
        return;
    }

    memset(&data, 0, sizeof(data));
    ctx->adapter->get_data1(&data);

    ctx->tx_buffer[index++] = LX_FRAME_HEAD_0;
    ctx->tx_buffer[index++] = LX_FRAME_HEAD_1;
    ctx->tx_buffer[index++] = LX_CMD_RESPONSE_DATA1;
    ctx->tx_buffer[index++] = LX_POWER_PROTOCOL_DATA1_LENGTH;

    for (i = 0U; i < LX_POWER_PROTOCOL_CELL_COUNT; ++i)
    {
        Lx_PutLe16(ctx->tx_buffer, &index, data.cell_mv[i]);
    }

    ctx->tx_buffer[index++] = data.temperature_raw;
    ctx->tx_buffer[index++] = (uint8_t)(data.balance_bits & 0x7FU);
    Lx_PutLe16(ctx->tx_buffer, &index, data.pack_voltage_raw);
    Lx_PutLe16(ctx->tx_buffer, &index, data.current_raw);
    ctx->tx_buffer[index++] = data.soc_percent;
    ctx->tx_buffer[index++] = data.fault_bits;
    Lx_PutLe16(ctx->tx_buffer, &index, data.cell_ov_mv);
    Lx_PutLe16(ctx->tx_buffer, &index, data.cell_uv_mv);
    ctx->tx_buffer[index++] = data.high_temperature_raw;
    Lx_PutLe16(ctx->tx_buffer, &index, data.charge_oc_raw);
    Lx_PutLe16(ctx->tx_buffer, &index, data.discharge_oc_raw);
    Lx_PutLe16(ctx->tx_buffer, &index, data.rated_capacity_raw);
    Lx_PutLe16(ctx->tx_buffer, &index, data.cell_ov_recovery_mv);
    Lx_PutLe16(ctx->tx_buffer, &index, data.cell_uv_recovery_mv);

    if (index != (uint16_t)(4U + LX_POWER_PROTOCOL_DATA1_LENGTH))
    {
        ctx->tx_length = 0U;
        return;
    }

    ctx->tx_buffer[index++] = Lx_Xor(&ctx->tx_buffer[2],
                                     (uint16_t)(2U + LX_POWER_PROTOCOL_DATA1_LENGTH));
    ctx->tx_buffer[index++] = LX_FRAME_END;
    ctx->tx_length = index;
}

static void Lx_BuildData2(LX_POWER_PROTOCOL_CONTEXT *ctx)
{
    LX_POWER_PROTOCOL_DATA2 data;
    uint16_t i;
    uint16_t index = 0U;
    uint16_t data_start;

    if ((ctx->adapter == 0) || (ctx->adapter->get_data2 == 0))
    {
        return;
    }

    memset(&data, 0, sizeof(data));
    ctx->adapter->get_data2(&data);

    ctx->tx_buffer[index++] = LX_POWER_PROTOCOL_VENDOR_ID_H;
    ctx->tx_buffer[index++] = LX_POWER_PROTOCOL_VENDOR_ID_L;
    ctx->tx_buffer[index++] = LX_POWER_PROTOCOL_DATA2_LENGTH;
    data_start = index;

    Lx_PutLe16(ctx->tx_buffer, &index, data.cycle_count);
    ctx->tx_buffer[index++] = data.software_version[0];
    ctx->tx_buffer[index++] = data.software_version[1];
    for (i = 0U; i < 11U; ++i)
    {
        ctx->tx_buffer[index++] = data.battery_id[i];
    }

    ctx->tx_buffer[index++] = Lx_Xor(&ctx->tx_buffer[data_start],
                                     LX_POWER_PROTOCOL_DATA2_LENGTH);
    ctx->tx_buffer[index++] = LX_DATA2_END;
    ctx->tx_length = index;
}

static void Lx_BuildData3(LX_POWER_PROTOCOL_CONTEXT *ctx)
{
    LX_POWER_PROTOCOL_DATA3 data;
    uint16_t i;
    uint16_t index = 0U;

    if ((ctx->adapter == 0) || (ctx->adapter->get_data3 == 0))
    {
        return;
    }

    memset(&data, 0, sizeof(data));
    ctx->adapter->get_data3(&data);

    ctx->tx_buffer[index++] = LX_FRAME_HEAD_0;
    ctx->tx_buffer[index++] = LX_FRAME_HEAD_1;
    ctx->tx_buffer[index++] = LX_CMD_RESPONSE_DATA3;
    ctx->tx_buffer[index++] = LX_POWER_PROTOCOL_DATA3_LENGTH;

    Lx_PutLe16(ctx->tx_buffer, &index, data.max_capacity_raw);
    Lx_PutLe16(ctx->tx_buffer, &index, data.charge_remaining_min);
    Lx_PutLe16(ctx->tx_buffer, &index, data.discharge_remaining_min);
    for (i = 0U; i < 10U; ++i)
    {
        ctx->tx_buffer[index++] = 0U;
    }

    ctx->tx_buffer[index++] = Lx_Xor(&ctx->tx_buffer[2],
                                     (uint16_t)(2U + LX_POWER_PROTOCOL_DATA3_LENGTH));
    ctx->tx_buffer[index++] = LX_FRAME_END;
    ctx->tx_length = index;
}

static void Lx_BuildData4(LX_POWER_PROTOCOL_CONTEXT *ctx)
{
    uint16_t counts[11];
    uint16_t i;
    uint16_t index = 0U;
    uint16_t checksum;

    if ((ctx->adapter == 0) || (ctx->adapter->get_data4_counts == 0))
    {
        return;
    }

    for (i = 0U; i < 11U; ++i)
    {
        counts[i] = 0U;
    }
    ctx->adapter->get_data4_counts(counts);

    ctx->tx_buffer[index++] = LX_DD_HEAD;
    ctx->tx_buffer[index++] = LX_DD_CMD_DATA4;
    ctx->tx_buffer[index++] = 0x00U;
    ctx->tx_buffer[index++] = LX_POWER_PROTOCOL_DATA4_LENGTH;

    for (i = 0U; i < 11U; ++i)
    {
        Lx_PutBe16(ctx->tx_buffer, &index, counts[i]);
    }

    /* V0.9 Command 4 response: checksum covers 00 16 + DATA only. */
    checksum = Lx_TwosComplement(&ctx->tx_buffer[2],
                                 (uint16_t)(2U + LX_POWER_PROTOCOL_DATA4_LENGTH));
    Lx_PutBe16(ctx->tx_buffer, &index, checksum);
    ctx->tx_buffer[index++] = LX_DD_END;
    ctx->tx_length = index;
}

static void Lx_BuildDdStatusResponse(LX_POWER_PROTOCOL_CONTEXT *ctx,
                                     uint8_t command,
                                     uint8_t status)
{
    uint16_t index = 0U;
    uint16_t checksum;

    ctx->tx_buffer[index++] = LX_DD_HEAD;
    ctx->tx_buffer[index++] = command;
    ctx->tx_buffer[index++] = status;
    ctx->tx_buffer[index++] = 0x00U;

    /* V0.9 Command 5/6 response checksum covers STATUS + LEN. */
    checksum = Lx_TwosComplement(&ctx->tx_buffer[2], 2U);
    Lx_PutBe16(ctx->tx_buffer, &index, checksum);
    ctx->tx_buffer[index++] = LX_DD_END;
    ctx->tx_length = index;
}

static void Lx_ProcessMainRequest(LX_POWER_PROTOCOL_CONTEXT *ctx)
{
    if (!Lx_MainRequestIsValid(ctx))
    {
        return;
    }

    if ((ctx->rx_buffer[3] != 1U) || (ctx->rx_buffer[4] != 0x01U))
    {
        return;
    }

    switch (ctx->rx_buffer[2])
    {
    case LX_CMD_REQUEST_DATA1:
        Lx_BuildData1(ctx);
        break;
    case LX_CMD_REQUEST_DATA2:
        Lx_BuildData2(ctx);
        break;
    case LX_CMD_REQUEST_DATA3:
        Lx_BuildData3(ctx);
        break;
    default:
        break;
    }
}

static void Lx_ProcessDdRequest(LX_POWER_PROTOCOL_CONTEXT *ctx)
{
    uint8_t status;

    if (!Lx_DdRequestIsValid(ctx))
    {
        return;
    }

    if ((ctx->rx_buffer[1] == LX_DD_READ) &&
        (ctx->rx_buffer[2] == LX_DD_CMD_DATA4) &&
        (ctx->rx_buffer[3] == 0U))
    {
        Lx_BuildData4(ctx);
        return;
    }

    if ((ctx->rx_buffer[1] == LX_DD_WRITE) &&
        (ctx->rx_buffer[2] == LX_DD_CMD_CLEAR_DATA4) &&
        (ctx->rx_buffer[3] == 2U) &&
        (ctx->rx_buffer[4] == 0x28U) &&
        (ctx->rx_buffer[5] == 0x28U))
    {
        status = 0x80U;
        if ((ctx->adapter != 0) && (ctx->adapter->clear_data4_counts != 0) &&
            ctx->adapter->clear_data4_counts())
        {
            status = 0x00U;
        }
        Lx_BuildDdStatusResponse(ctx, LX_DD_CMD_CLEAR_DATA4, status);
        return;
    }

    if ((ctx->rx_buffer[1] == LX_DD_WRITE) &&
        (ctx->rx_buffer[2] == LX_DD_CMD_MOS) &&
        (ctx->rx_buffer[3] == 2U) &&
        (ctx->rx_buffer[4] == 0x00U) &&
        (ctx->rx_buffer[5] <= 0x03U))
    {
        status = 0x80U;
        if ((ctx->adapter != 0) && (ctx->adapter->set_software_mos_mode != 0) &&
            ctx->adapter->set_software_mos_mode(ctx->rx_buffer[5]))
        {
            status = 0x00U;
        }
        Lx_BuildDdStatusResponse(ctx, LX_DD_CMD_MOS, status);
    }
}

void LxPowerProtocol_Init(LX_POWER_PROTOCOL_CONTEXT *ctx,
                          const LX_POWER_PROTOCOL_ADAPTER *adapter)
{
    if (ctx == 0)
    {
        return;
    }

    memset(ctx, 0, sizeof(*ctx));
    ctx->adapter = adapter;
}

void LxPowerProtocol_Reset(LX_POWER_PROTOCOL_CONTEXT *ctx)
{
    if (ctx == 0)
    {
        return;
    }

    Lx_ResetRx(ctx);
    ctx->tx_length = 0U;
}

uint8_t LxPowerProtocol_IsStartByte(uint8_t data)
{
    return (uint8_t)(((data == LX_FRAME_HEAD_0) || (data == LX_DD_HEAD)) ? 1U : 0U);
}

uint8_t LxPowerProtocol_Feed(LX_POWER_PROTOCOL_CONTEXT *ctx, uint8_t data)
{
    uint16_t length;
    uint16_t expected;

    if (ctx == 0)
    {
        return 0U;
    }

    length = ctx->rx_length;
    if (length == 0U)
    {
        Lx_RestartWithByte(ctx, data);
        return 0U;
    }

    if (length >= LX_POWER_PROTOCOL_RX_BUFFER_SIZE)
    {
        Lx_RestartWithByte(ctx, data);
        return 0U;
    }

    if (ctx->rx_buffer[0] == LX_FRAME_HEAD_0)
    {
        if ((length == 1U) && (data != LX_FRAME_HEAD_1))
        {
            Lx_RestartWithByte(ctx, data);
            return 0U;
        }

        ctx->rx_buffer[length++] = data;
        ctx->rx_length = length;
        if (length == 4U)
        {
            expected = (uint16_t)(6U + ctx->rx_buffer[3]);
            if (expected > LX_POWER_PROTOCOL_RX_BUFFER_SIZE)
            {
                Lx_ResetRx(ctx);
                return 0U;
            }
            ctx->expected_length = expected;
        }
    }
    else if (ctx->rx_buffer[0] == LX_DD_HEAD)
    {
        if ((length == 1U) &&
            (data != LX_DD_READ) &&
            (data != LX_DD_WRITE))
        {
            Lx_RestartWithByte(ctx, data);
            return 0U;
        }

        ctx->rx_buffer[length++] = data;
        ctx->rx_length = length;
        if (length == 4U)
        {
            expected = (uint16_t)(7U + ctx->rx_buffer[3]);
            if (expected > LX_POWER_PROTOCOL_RX_BUFFER_SIZE)
            {
                Lx_ResetRx(ctx);
                return 0U;
            }
            ctx->expected_length = expected;
        }
    }
    else
    {
        Lx_RestartWithByte(ctx, data);
        return 0U;
    }

    if ((ctx->expected_length != 0U) &&
        (ctx->rx_length == ctx->expected_length))
    {
        return 1U;
    }

    if ((ctx->expected_length != 0U) &&
        (ctx->rx_length > ctx->expected_length))
    {
        Lx_ResetRx(ctx);
    }

    return 0U;
}

void LxPowerProtocol_OnRxIdle(LX_POWER_PROTOCOL_CONTEXT *ctx)
{
    if ((ctx != 0) && (ctx->rx_length != 0U))
    {
        LxPowerProtocol_Reset(ctx);
    }
}

void LxPowerProtocol_Process(LX_POWER_PROTOCOL_CONTEXT *ctx)
{
    uint8_t first;

    if ((ctx == 0) || (ctx->rx_length == 0U))
    {
        return;
    }

    ctx->tx_length = 0U;
    first = ctx->rx_buffer[0];
    if (first == LX_FRAME_HEAD_0)
    {
        Lx_ProcessMainRequest(ctx);
    }
    else if (first == LX_DD_HEAD)
    {
        Lx_ProcessDdRequest(ctx);
    }
}

void LxPowerProtocol_Service(LX_POWER_PROTOCOL_CONTEXT *ctx)
{
    if ((ctx != 0) && (ctx->adapter != 0) && (ctx->adapter->service != 0))
    {
        ctx->adapter->service();
    }
}

uint8_t *LxPowerProtocol_GetTxBuffer(LX_POWER_PROTOCOL_CONTEXT *ctx)
{
    return (ctx != 0) ? ctx->tx_buffer : 0;
}

uint16_t LxPowerProtocol_GetTxLength(const LX_POWER_PROTOCOL_CONTEXT *ctx)
{
    return (ctx != 0) ? ctx->tx_length : 0U;
}

uint8_t LxPowerProtocol_IsBusy(const LX_POWER_PROTOCOL_CONTEXT *ctx)
{
    if (ctx == 0)
    {
        return 0U;
    }
    return (uint8_t)(((ctx->rx_length != 0U) || (ctx->tx_length != 0U)) ? 1U : 0U);
}
