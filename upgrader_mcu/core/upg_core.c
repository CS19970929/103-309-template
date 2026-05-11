#include "upg_core.h"

#include "upg_crc16.h"
#include "upg_protocol.h"
#include "upg_utils.h"

#include <string.h>

#define UPG_CAP_CAN_OBJECT       ((uint32_t)0x00000001UL)
#define UPG_CAP_PARAM_RW         ((uint32_t)0x00000002UL)
#define UPG_CAP_IAP_UPGRADE      ((uint32_t)0x00000004UL)
#define UPG_CAP_RAW_CAN          ((uint32_t)0x00000008UL)
#define UPG_DEFAULT_TIMEOUT_MS   ((uint16_t)1000U)
#define UPG_MAX_TIMEOUT_MS       ((uint16_t)30000U)

static void UpgCore_HandleSerialFrame(const UpgSerialFrameView *frame, void *user);
static void UpgCore_HandleSerialError(uint8_t status, void *user);

static uint16_t UpgClampTimeout(uint16_t timeout_ms)
{
    if (timeout_ms == 0U)
    {
        return UPG_DEFAULT_TIMEOUT_MS;
    }
    if (timeout_ms > UPG_MAX_TIMEOUT_MS)
    {
        return UPG_MAX_TIMEOUT_MS;
    }
    return timeout_ms;
}

static uint8_t UpgSendSerial(UpgCore *ctx, uint8_t cmd, uint16_t seq, uint8_t flags, const uint8_t *payload, uint16_t len)
{
    uint16_t frame_len;

    if ((ctx == 0) || (ctx->hal.serial_tx == 0))
    {
        return 0U;
    }
    if (UpgSerial_Encode(cmd, seq, flags, payload, len, ctx->tx_buf, sizeof(ctx->tx_buf), &frame_len) == 0U)
    {
        return 0U;
    }
    return (ctx->hal.serial_tx(ctx->hal.user, ctx->tx_buf, frame_len) == 0) ? 1U : 0U;
}

static uint8_t UpgRespond(UpgCore *ctx, uint8_t cmd, uint16_t seq, uint8_t status, uint8_t detail, const uint8_t *data, uint16_t data_len)
{
    uint8_t payload[UPG_SERIAL_MAX_PAYLOAD];
    uint8_t flags;

    if ((uint16_t)(data_len + 2U) > UPG_SERIAL_MAX_PAYLOAD)
    {
        data_len = (uint16_t)(UPG_SERIAL_MAX_PAYLOAD - 2U);
    }
    payload[0] = status;
    payload[1] = detail;
    if ((data_len > 0U) && (data != 0))
    {
        memcpy(&payload[2], data, data_len);
    }
    flags = UPG_SERIAL_FLAG_ACK;
    if (status != UPG_STATUS_OK)
    {
        flags |= UPG_SERIAL_FLAG_ERROR;
    }
    return UpgSendSerial(ctx, cmd, seq, flags, payload, (uint16_t)(data_len + 2U));
}

static uint8_t UpgSendCan(UpgCore *ctx, const UpgCanFrame *frame)
{
    if ((ctx == 0) || (ctx->hal.can_tx == 0) || (frame == 0))
    {
        return 0U;
    }
    return (ctx->hal.can_tx(ctx->hal.user, frame) == 0) ? 1U : 0U;
}

static void UpgClearPending(UpgCore *ctx)
{
    memset(&ctx->pending, 0, sizeof(ctx->pending));
    ctx->pending.type = UPG_PENDING_NONE;
}

static uint8_t UpgStartPending(UpgCore *ctx, UpgPendingType type, uint8_t cmd, uint16_t seq, uint8_t index, uint8_t chd, uint16_t timeout_ms)
{
    if (ctx->pending.type != UPG_PENDING_NONE)
    {
        return 0U;
    }
    memset(&ctx->pending, 0, sizeof(ctx->pending));
    ctx->pending.type = type;
    ctx->pending.cmd = cmd;
    ctx->pending.seq = seq;
    ctx->pending.index = index;
    ctx->pending.chd = chd;
    ctx->pending.deadline_ms = ctx->now_ms + UpgClampTimeout(timeout_ms);
    return 1U;
}

static uint8_t UpgCanReadObject(UpgCore *ctx, uint8_t index, uint8_t chd)
{
    uint8_t data[8] = {0};
    UpgCanFrame frame;

    UpgFeidao_MakeFrame(&frame, ctx->host_node, ctx->device_node, FEIDAO_CTRL_READ, index, chd, data);
    return UpgSendCan(ctx, &frame);
}

static uint8_t UpgCanWriteObject(UpgCore *ctx, uint8_t index, uint8_t chd, const uint8_t data[8])
{
    UpgCanFrame frame;

    UpgFeidao_MakeFrame(&frame, ctx->host_node, ctx->device_node, FEIDAO_CTRL_WRITE, index, chd, data);
    return UpgSendCan(ctx, &frame);
}

static void UpgFinishPendingError(UpgCore *ctx, uint8_t status, uint8_t detail)
{
    uint8_t cmd;
    uint16_t seq;

    cmd = ctx->pending.cmd;
    seq = ctx->pending.seq;
    UpgClearPending(ctx);
    (void)UpgRespond(ctx, cmd, seq, status, detail, 0, 0U);
}

static void UpgSnapshot_Update(UpgCore *ctx, const UpgCanFrame *frame)
{
    UpgFeidaoId id;
    const uint8_t *data;

    if ((ctx == 0) || (frame == 0) || (frame->extended == 0U) || (frame->dlc != 8U))
    {
        return;
    }
    id = UpgFeidao_DecodeId(frame->id);
    if ((id.src != FEIDAO_NODE_BATTERY) ||
        (id.dst != FEIDAO_NODE_BROADCAST) ||
        (id.ctrl != FEIDAO_CTRL_WRITE) ||
        (id.index != 0x02U))
    {
        return;
    }

    data = frame->data;
    ctx->snapshot.last_update_ms = ctx->now_ms;
    if (id.chd == 0U)
    {
        ctx->snapshot.voltage_mv = UpgReadBe32(&data[0]);
        ctx->snapshot.current_ma = UpgReadBeS32(&data[4]);
        ctx->snapshot.valid_mask |= 0x00000001UL;
    }
    else if (id.chd == 2U)
    {
        ctx->snapshot.soc = data[1];
        ctx->snapshot.temp_c = (int8_t)data[2];
        ctx->snapshot.valid_mask |= 0x00000002UL;
    }
    else if (id.chd == 3U)
    {
        ctx->snapshot.soh = data[0];
        ctx->snapshot.cycles = UpgReadBe16(&data[1]);
        ctx->snapshot.valid_mask |= 0x00000004UL;
    }
    else if (id.chd == 4U)
    {
        ctx->snapshot.protocol_version = data[0];
        ctx->snapshot.software_version = data[1];
        ctx->snapshot.valid_mask |= 0x00000008UL;
    }
    else if (id.chd == 5U)
    {
        ctx->snapshot.work_status = data[0];
        ctx->snapshot.exception_status = data[1];
        ctx->snapshot.cap_full = UpgReadBe16(&data[2]);
        ctx->snapshot.cap_now = UpgReadBe16(&data[4]);
        ctx->snapshot.cap_design = UpgReadBe16(&data[6]);
        ctx->snapshot.valid_mask |= 0x00000010UL;
    }
    else
    {
        /* Other broadcast channels are ignored by the generic cache. */
    }
}

static void UpgRespondSnapshot(UpgCore *ctx, uint8_t cmd, uint16_t seq)
{
    uint8_t data[36];
    uint32_t age;

    memset(data, 0, sizeof(data));
    age = (ctx->snapshot.valid_mask == 0U) ? 0xFFFFFFFFUL : (ctx->now_ms - ctx->snapshot.last_update_ms);
    UpgWriteBe32(&data[0], ctx->snapshot.valid_mask);
    UpgWriteBe32(&data[4], age);
    UpgWriteBe32(&data[8], ctx->snapshot.voltage_mv);
    UpgWriteBe32(&data[12], (uint32_t)ctx->snapshot.current_ma);
    data[16] = ctx->snapshot.soc;
    data[17] = ctx->snapshot.soh;
    data[18] = (uint8_t)ctx->snapshot.temp_c;
    UpgWriteBe16(&data[19], ctx->snapshot.cycles);
    data[21] = ctx->snapshot.protocol_version;
    data[22] = ctx->snapshot.software_version;
    data[23] = ctx->snapshot.work_status;
    data[24] = ctx->snapshot.exception_status;
    UpgWriteBe16(&data[25], ctx->snapshot.cap_full);
    UpgWriteBe16(&data[27], ctx->snapshot.cap_now);
    UpgWriteBe16(&data[29], ctx->snapshot.cap_design);
    (void)UpgRespond(ctx, cmd, seq, UPG_STATUS_OK, 0U, data, sizeof(data));
}

static void UpgRespondUpgradeStatus(UpgCore *ctx, uint8_t cmd, uint16_t seq)
{
    uint8_t data[16];

    memset(data, 0, sizeof(data));
    data[0] = (uint8_t)ctx->upgrade.state;
    data[1] = ctx->upgrade.last_error;
    UpgWriteBe32(&data[2], ctx->upgrade.image_size);
    UpgWriteBe16(&data[6], ctx->upgrade.file_crc16);
    UpgWriteBe16(&data[8], ctx->upgrade.total_long_packets);
    UpgWriteBe16(&data[10], ctx->upgrade.acked_long_packets);
    UpgWriteBe16(&data[12], ctx->upgrade.current_long_index);
    UpgWriteBe16(&data[14], ctx->upgrade.current_received);
    (void)UpgRespond(ctx, cmd, seq, UPG_STATUS_OK, 0U, data, sizeof(data));
}

static void UpgHandleGetDeviceInfo(UpgCore *ctx, uint8_t cmd, uint16_t seq)
{
    uint8_t data[32];
    const char name[] = "STM32F103C8-UPG";

    memset(data, 0, sizeof(data));
    data[0] = UPG_VERSION;
    data[1] = 1U;
    UpgWriteBe16(&data[2], UPG_SERIAL_MAX_PAYLOAD);
    UpgWriteBe16(&data[4], UPG_LONG_PACKET_BYTES);
    UpgWriteBe32(&data[6], UPG_CAP_CAN_OBJECT | UPG_CAP_PARAM_RW | UPG_CAP_IAP_UPGRADE | UPG_CAP_RAW_CAN);
    UpgWriteBe32(&data[10], ctx->can_bitrate);
    data[14] = ctx->host_node;
    data[15] = ctx->device_node;
    memcpy(&data[16], name, sizeof(name) - 1U);
    (void)UpgRespond(ctx, cmd, seq, UPG_STATUS_OK, 0U, data, sizeof(data));
}

static void UpgHandleSetCanConfig(UpgCore *ctx, const UpgSerialFrameView *frame)
{
    uint32_t bitrate;
    uint8_t host;
    uint8_t device;

    if (frame->len < 6U)
    {
        (void)UpgRespond(ctx, frame->cmd, frame->seq, UPG_STATUS_BAD_PARAM, 0U, 0, 0U);
        return;
    }
    bitrate = UpgReadBe32(&frame->payload[0]);
    host = frame->payload[4];
    device = frame->payload[5];
    if ((host > 0x1FU) || (device > 0x1FU) || (bitrate == 0U))
    {
        (void)UpgRespond(ctx, frame->cmd, frame->seq, UPG_STATUS_BAD_PARAM, 0U, 0, 0U);
        return;
    }
    ctx->can_bitrate = bitrate;
    ctx->host_node = host;
    ctx->device_node = device;
    (void)UpgRespond(ctx, frame->cmd, frame->seq, UPG_STATUS_OK, 0U, 0, 0U);
}

static void UpgCommandReadObject(UpgCore *ctx, const UpgSerialFrameView *frame, UpgPendingType type)
{
    uint8_t index;
    uint8_t chd;
    uint16_t timeout_ms;

    if (frame->len < 4U)
    {
        (void)UpgRespond(ctx, frame->cmd, frame->seq, UPG_STATUS_BAD_PARAM, 0U, 0, 0U);
        return;
    }
    if (ctx->pending.type != UPG_PENDING_NONE)
    {
        (void)UpgRespond(ctx, frame->cmd, frame->seq, UPG_STATUS_BUSY, 0U, 0, 0U);
        return;
    }
    index = frame->payload[0];
    chd = frame->payload[1];
    timeout_ms = UpgReadBe16(&frame->payload[2]);
    if (UpgStartPending(ctx, type, frame->cmd, frame->seq, index, chd, timeout_ms) == 0U)
    {
        (void)UpgRespond(ctx, frame->cmd, frame->seq, UPG_STATUS_BUSY, 0U, 0, 0U);
        return;
    }
    if (UpgCanReadObject(ctx, index, chd) == 0U)
    {
        UpgFinishPendingError(ctx, UPG_STATUS_UNKNOWN, 0U);
    }
}

static void UpgCommandWriteObject(UpgCore *ctx, const UpgSerialFrameView *frame)
{
    uint8_t index;
    uint8_t chd;
    uint16_t timeout_ms;

    if (frame->len < 12U)
    {
        (void)UpgRespond(ctx, frame->cmd, frame->seq, UPG_STATUS_BAD_PARAM, 0U, 0, 0U);
        return;
    }
    if (ctx->pending.type != UPG_PENDING_NONE)
    {
        (void)UpgRespond(ctx, frame->cmd, frame->seq, UPG_STATUS_BUSY, 0U, 0, 0U);
        return;
    }
    index = frame->payload[0];
    chd = frame->payload[1];
    timeout_ms = UpgReadBe16(&frame->payload[2]);
    if (UpgStartPending(ctx, UPG_PENDING_OBJECT_WRITE, frame->cmd, frame->seq, index, chd, timeout_ms) == 0U)
    {
        (void)UpgRespond(ctx, frame->cmd, frame->seq, UPG_STATUS_BUSY, 0U, 0, 0U);
        return;
    }
    memcpy(ctx->pending.object_data, &frame->payload[4], 8U);
    if (UpgCanWriteObject(ctx, index, chd, ctx->pending.object_data) == 0U)
    {
        UpgFinishPendingError(ctx, UPG_STATUS_UNKNOWN, 0U);
    }
}

static void UpgCommandParamRead(UpgCore *ctx, const UpgSerialFrameView *frame)
{
    uint16_t param_id;
    uint16_t timeout_ms;
    const UpgParamDef *param;

    if (frame->len < 4U)
    {
        (void)UpgRespond(ctx, frame->cmd, frame->seq, UPG_STATUS_BAD_PARAM, 0U, 0, 0U);
        return;
    }
    if (ctx->pending.type != UPG_PENDING_NONE)
    {
        (void)UpgRespond(ctx, frame->cmd, frame->seq, UPG_STATUS_BUSY, 0U, 0, 0U);
        return;
    }
    param_id = UpgReadBe16(&frame->payload[0]);
    timeout_ms = UpgReadBe16(&frame->payload[2]);
    param = UpgParam_Find(param_id);
    if (param == 0)
    {
        (void)UpgRespond(ctx, frame->cmd, frame->seq, UPG_STATUS_BAD_PARAM, 1U, 0, 0U);
        return;
    }
    (void)UpgStartPending(ctx, UPG_PENDING_PARAM_READ_OBJECT, frame->cmd, frame->seq, param->can_index, param->can_chd, timeout_ms);
    ctx->pending.param = param;
    ctx->pending.param_id = param_id;
    if (UpgCanReadObject(ctx, param->can_index, param->can_chd) == 0U)
    {
        UpgFinishPendingError(ctx, UPG_STATUS_UNKNOWN, 0U);
    }
}

static void UpgCommandParamWrite(UpgCore *ctx, const UpgSerialFrameView *frame)
{
    uint16_t param_id;
    int32_t raw_value;
    uint8_t confirm;
    uint16_t timeout_ms;
    const UpgParamDef *param;

    if (frame->len < 9U)
    {
        (void)UpgRespond(ctx, frame->cmd, frame->seq, UPG_STATUS_BAD_PARAM, 0U, 0, 0U);
        return;
    }
    if (ctx->pending.type != UPG_PENDING_NONE)
    {
        (void)UpgRespond(ctx, frame->cmd, frame->seq, UPG_STATUS_BUSY, 0U, 0, 0U);
        return;
    }
    param_id = UpgReadBe16(&frame->payload[0]);
    raw_value = UpgReadBeS32(&frame->payload[2]);
    confirm = frame->payload[6];
    timeout_ms = UpgReadBe16(&frame->payload[7]);
    param = UpgParam_Find(param_id);
    if (param == 0)
    {
        (void)UpgRespond(ctx, frame->cmd, frame->seq, UPG_STATUS_BAD_PARAM, 1U, 0, 0U);
        return;
    }
    if ((param->writable == 0U) ||
        (raw_value < param->min_value) ||
        (raw_value > param->max_value) ||
        ((param->require_confirm != 0U) && (confirm != 0xA5U)))
    {
        (void)UpgRespond(ctx, frame->cmd, frame->seq, UPG_STATUS_BAD_PARAM, 2U, 0, 0U);
        return;
    }
    (void)UpgStartPending(ctx, UPG_PENDING_PARAM_WRITE_READ_OBJECT, frame->cmd, frame->seq, param->can_index, param->can_chd, timeout_ms);
    ctx->pending.param = param;
    ctx->pending.param_id = param_id;
    ctx->pending.write_value = raw_value;
    if (UpgCanReadObject(ctx, param->can_index, param->can_chd) == 0U)
    {
        UpgFinishPendingError(ctx, UPG_STATUS_UNKNOWN, 0U);
    }
}

static void UpgCommandEnterIap(UpgCore *ctx, const UpgSerialFrameView *frame)
{
    uint8_t data[8] = {0};
    uint16_t crc;
    uint16_t timeout_ms;
    UpgCanFrame can_frame;

    if (ctx->pending.type != UPG_PENDING_NONE)
    {
        (void)UpgRespond(ctx, frame->cmd, frame->seq, UPG_STATUS_BUSY, 0U, 0, 0U);
        return;
    }
    timeout_ms = (frame->len >= 2U) ? UpgReadBe16(&frame->payload[0]) : 2000U;
    data[0] = BMS_APP_CMD_MAGIC0;
    data[1] = BMS_APP_CMD_MAGIC1;
    data[2] = BMS_APP_CMD_ENTER_IAP;
    data[3] = BMS_APP_IAP_KEY0;
    data[4] = BMS_APP_IAP_KEY1;
    data[5] = 0x00U;
    crc = UpgCrc16_Calc(data, 6U);
    UpgWriteBe16(&data[6], crc);

    can_frame.id = BMS_APP_CAN_CMD_ID;
    can_frame.extended = 0U;
    can_frame.dlc = 8U;
    memcpy(can_frame.data, data, 8U);
    (void)UpgStartPending(ctx, UPG_PENDING_ENTER_IAP, frame->cmd, frame->seq, 0U, 0U, timeout_ms);
    if (UpgSendCan(ctx, &can_frame) == 0U)
    {
        UpgFinishPendingError(ctx, UPG_STATUS_UNKNOWN, 0U);
    }
}

static uint16_t UpgExpectedLongPackets(uint32_t image_size)
{
    uint32_t frames;
    uint32_t packets;

    if ((image_size == 0U) || (image_size > UPG_APP_MAX_SIZE))
    {
        return 0U;
    }
    frames = (image_size + 7U) / 8U;
    packets = (frames + UPG_LONG_PACKET_MAX_FRAMES - 1U) / UPG_LONG_PACKET_MAX_FRAMES;
    return (packets > 0xFFFFUL) ? 0U : (uint16_t)packets;
}

static void UpgCommandUpgradePrepare(UpgCore *ctx, const UpgSerialFrameView *frame)
{
    uint8_t data[8];
    uint32_t image_size;
    uint16_t file_crc;
    uint16_t total_packets;
    uint16_t expected_packets;
    uint16_t timeout_ms;

    if (frame->len < 10U)
    {
        (void)UpgRespond(ctx, frame->cmd, frame->seq, UPG_STATUS_BAD_PARAM, 0U, 0, 0U);
        return;
    }
    if (ctx->pending.type != UPG_PENDING_NONE)
    {
        (void)UpgRespond(ctx, frame->cmd, frame->seq, UPG_STATUS_BUSY, 0U, 0, 0U);
        return;
    }
    image_size = UpgReadBe32(&frame->payload[0]);
    file_crc = UpgReadBe16(&frame->payload[4]);
    total_packets = UpgReadBe16(&frame->payload[6]);
    timeout_ms = UpgReadBe16(&frame->payload[8]);
    expected_packets = UpgExpectedLongPackets(image_size);
    if ((expected_packets == 0U) || (expected_packets != total_packets))
    {
        (void)UpgRespond(ctx, frame->cmd, frame->seq, UPG_STATUS_IMAGE_INVALID, 0U, 0, 0U);
        return;
    }

    memset(&ctx->upgrade, 0, sizeof(ctx->upgrade));
    ctx->upgrade.state = UPG_UPGRADE_PREPARED;
    ctx->upgrade.image_size = image_size;
    ctx->upgrade.file_crc16 = file_crc;
    ctx->upgrade.total_long_packets = total_packets;

    UpgWriteBe16(&data[0], total_packets);
    UpgWriteBe16(&data[2], file_crc);
    UpgWriteBe32(&data[4], image_size);
    (void)UpgStartPending(ctx, UPG_PENDING_UPGRADE_PREPARE, frame->cmd, frame->seq, FEIDAO_UPGRADE_START_INDEX, FEIDAO_UPGRADE_START_ACK_CHD, timeout_ms);
    if (UpgCanWriteObject(ctx, FEIDAO_UPGRADE_START_INDEX, 0x00U, data) == 0U)
    {
        UpgFinishPendingError(ctx, UPG_STATUS_UNKNOWN, 0U);
    }
}

static void UpgCommandUpgradePacketData(UpgCore *ctx, const UpgSerialFrameView *frame)
{
    uint16_t chunk_index;
    uint16_t offset;
    uint16_t data_len;

    if (frame->len < 6U)
    {
        (void)UpgRespond(ctx, frame->cmd, frame->seq, UPG_STATUS_BAD_PARAM, 0U, 0, 0U);
        return;
    }
    if ((ctx->upgrade.state != UPG_UPGRADE_PREPARED) &&
        (ctx->upgrade.state != UPG_UPGRADE_TRANSFERRING) &&
        (ctx->upgrade.state != UPG_UPGRADE_WAIT_FINAL))
    {
        (void)UpgRespond(ctx, frame->cmd, frame->seq, UPG_STATUS_BUSY, 0U, 0, 0U);
        return;
    }
    chunk_index = UpgReadBe16(&frame->payload[0]);
    offset = UpgReadBe16(&frame->payload[2]);
    data_len = UpgReadBe16(&frame->payload[4]);
    if (((uint16_t)(frame->len - 6U) != data_len) ||
        (offset > UPG_LONG_PACKET_BYTES) ||
        (data_len > UPG_LONG_PACKET_BYTES) ||
        ((uint32_t)offset + (uint32_t)data_len > UPG_LONG_PACKET_BYTES))
    {
        (void)UpgRespond(ctx, frame->cmd, frame->seq, UPG_STATUS_BAD_PARAM, 0U, 0, 0U);
        return;
    }
    if ((offset == 0U) || (chunk_index != ctx->upgrade.current_long_index))
    {
        memset(ctx->upgrade.chunk, 0, sizeof(ctx->upgrade.chunk));
        ctx->upgrade.current_long_index = chunk_index;
        ctx->upgrade.current_received = 0U;
        ctx->upgrade.current_len = 0U;
    }
    memcpy(&ctx->upgrade.chunk[offset], &frame->payload[6], data_len);
    if ((uint16_t)(offset + data_len) > ctx->upgrade.current_received)
    {
        ctx->upgrade.current_received = (uint16_t)(offset + data_len);
    }
    (void)UpgRespond(ctx, frame->cmd, frame->seq, UPG_STATUS_OK, 0U, 0, 0U);
}

static uint8_t UpgSendUpgradeChunk(UpgCore *ctx, uint16_t chunk_index, uint16_t actual_len)
{
    uint8_t data[8];
    uint16_t frame_count;
    uint16_t padded_len;
    uint16_t seq;
    uint16_t crc;
    UpgCanFrame frame;

    if ((actual_len == 0U) || (actual_len > UPG_LONG_PACKET_BYTES))
    {
        return 0U;
    }
    frame_count = (uint16_t)((actual_len + 7U) / 8U);
    if ((frame_count == 0U) || (frame_count > UPG_LONG_PACKET_MAX_FRAMES))
    {
        return 0U;
    }
    padded_len = (uint16_t)(frame_count * 8U);
    if (padded_len > actual_len)
    {
        memset(&ctx->upgrade.chunk[actual_len], 0, (uint16_t)(padded_len - actual_len));
    }
    crc = UpgCrc16_Calc(ctx->upgrade.chunk, padded_len);

    UpgWriteBe16(&data[0], chunk_index);
    UpgWriteBe16(&data[2], frame_count);
    memset(&data[4], 0, 4U);
    UpgFeidao_MakeFrame(&frame, ctx->host_node, ctx->device_node, FEIDAO_CTRL_LONG_START, FEIDAO_UPGRADE_DATA_INDEX, FEIDAO_UPGRADE_CHUNK_CHD, data);
    if (UpgSendCan(ctx, &frame) == 0U)
    {
        return 0U;
    }

    for (seq = 0U; seq < frame_count; seq++)
    {
        UpgFeidao_MakeFrame(&frame, ctx->host_node, ctx->device_node, FEIDAO_CTRL_LONG_DATA, FEIDAO_UPGRADE_DATA_INDEX, (uint8_t)seq, &ctx->upgrade.chunk[(uint16_t)(seq * 8U)]);
        if (UpgSendCan(ctx, &frame) == 0U)
        {
            return 0U;
        }
    }

    UpgWriteBe16(&data[0], crc);
    memset(&data[2], 0, 6U);
    UpgFeidao_MakeFrame(&frame, ctx->host_node, ctx->device_node, FEIDAO_CTRL_LONG_END, FEIDAO_UPGRADE_DATA_INDEX, FEIDAO_UPGRADE_CHUNK_CHD, data);
    return UpgSendCan(ctx, &frame);
}

static void UpgCommandUpgradePacketCommit(UpgCore *ctx, const UpgSerialFrameView *frame)
{
    uint16_t chunk_index;
    uint16_t actual_len;
    uint16_t timeout_ms;

    if (frame->len < 6U)
    {
        (void)UpgRespond(ctx, frame->cmd, frame->seq, UPG_STATUS_BAD_PARAM, 0U, 0, 0U);
        return;
    }
    if (ctx->pending.type != UPG_PENDING_NONE)
    {
        (void)UpgRespond(ctx, frame->cmd, frame->seq, UPG_STATUS_BUSY, 0U, 0, 0U);
        return;
    }
    if ((ctx->upgrade.state != UPG_UPGRADE_PREPARED) &&
        (ctx->upgrade.state != UPG_UPGRADE_TRANSFERRING) &&
        (ctx->upgrade.state != UPG_UPGRADE_WAIT_FINAL))
    {
        (void)UpgRespond(ctx, frame->cmd, frame->seq, UPG_STATUS_BUSY, 1U, 0, 0U);
        return;
    }
    chunk_index = UpgReadBe16(&frame->payload[0]);
    actual_len = UpgReadBe16(&frame->payload[2]);
    timeout_ms = UpgReadBe16(&frame->payload[4]);
    if ((chunk_index != ctx->upgrade.current_long_index) ||
        (actual_len == 0U) ||
        (actual_len > ctx->upgrade.current_received))
    {
        (void)UpgRespond(ctx, frame->cmd, frame->seq, UPG_STATUS_BAD_PARAM, 0U, 0, 0U);
        return;
    }
    (void)UpgStartPending(ctx, UPG_PENDING_UPGRADE_COMMIT, frame->cmd, frame->seq, FEIDAO_UPGRADE_DATA_INDEX, FEIDAO_UPGRADE_CHUNK_CHD, timeout_ms);
    ctx->upgrade.state = UPG_UPGRADE_TRANSFERRING;
    if (UpgSendUpgradeChunk(ctx, chunk_index, actual_len) == 0U)
    {
        ctx->upgrade.last_error = UPG_STATUS_UNKNOWN;
        ctx->upgrade.state = UPG_UPGRADE_ERROR;
        UpgFinishPendingError(ctx, UPG_STATUS_UNKNOWN, 0U);
    }
}

static void UpgCommandUpgradeFinish(UpgCore *ctx, const UpgSerialFrameView *frame)
{
    uint16_t timeout_ms;

    if (ctx->upgrade.state == UPG_UPGRADE_DONE)
    {
        (void)UpgRespond(ctx, frame->cmd, frame->seq, UPG_STATUS_OK, 0U, 0, 0U);
        return;
    }
    if (ctx->pending.type != UPG_PENDING_NONE)
    {
        (void)UpgRespond(ctx, frame->cmd, frame->seq, UPG_STATUS_BUSY, 0U, 0, 0U);
        return;
    }
    timeout_ms = (frame->len >= 2U) ? UpgReadBe16(&frame->payload[0]) : 8000U;
    (void)UpgStartPending(ctx, UPG_PENDING_UPGRADE_FINISH, frame->cmd, frame->seq, FEIDAO_UPGRADE_DATA_INDEX, FEIDAO_UPGRADE_CHUNK_CHD, timeout_ms);
    ctx->upgrade.state = UPG_UPGRADE_WAIT_FINAL;
}

static void UpgCommandRawCanTx(UpgCore *ctx, const UpgSerialFrameView *frame)
{
    UpgCanFrame can_frame;
    uint8_t dlc;

    if (frame->len < 6U)
    {
        (void)UpgRespond(ctx, frame->cmd, frame->seq, UPG_STATUS_BAD_PARAM, 0U, 0, 0U);
        return;
    }
    can_frame.id = UpgReadBe32(&frame->payload[0]);
    can_frame.extended = frame->payload[4];
    dlc = frame->payload[5];
    if ((dlc > 8U) || (frame->len < (uint16_t)(6U + dlc)))
    {
        (void)UpgRespond(ctx, frame->cmd, frame->seq, UPG_STATUS_BAD_PARAM, 0U, 0, 0U);
        return;
    }
    can_frame.dlc = dlc;
    memset(can_frame.data, 0, 8U);
    memcpy(can_frame.data, &frame->payload[6], dlc);
    if (UpgSendCan(ctx, &can_frame) == 0U)
    {
        (void)UpgRespond(ctx, frame->cmd, frame->seq, UPG_STATUS_UNKNOWN, 0U, 0, 0U);
        return;
    }
    (void)UpgRespond(ctx, frame->cmd, frame->seq, UPG_STATUS_OK, 0U, 0, 0U);
}

static void UpgCore_HandleSerialFrame(const UpgSerialFrameView *frame, void *user)
{
    UpgCore *ctx = (UpgCore *)user;

    if ((ctx == 0) || (frame == 0))
    {
        return;
    }
    switch (frame->cmd)
    {
    case UPG_CMD_GET_DEVICE_INFO:
        UpgHandleGetDeviceInfo(ctx, frame->cmd, frame->seq);
        break;
    case UPG_CMD_SET_CAN_CONFIG:
        UpgHandleSetCanConfig(ctx, frame);
        break;
    case UPG_CMD_PING_BMS:
        {
            uint8_t payload[4];
            payload[0] = 0x02U;
            payload[1] = 0x04U;
            UpgWriteBe16(&payload[2], 1000U);
            UpgSerialFrameView read_frame = {UPG_CMD_PING_BMS, frame->seq, 0U, sizeof(payload), payload};
            UpgCommandReadObject(ctx, &read_frame, UPG_PENDING_OBJECT_READ);
        }
        break;
    case UPG_CMD_READ_BMS_SNAPSHOT:
        UpgRespondSnapshot(ctx, frame->cmd, frame->seq);
        break;
    case UPG_CMD_CAN_OBJECT_READ:
        UpgCommandReadObject(ctx, frame, UPG_PENDING_OBJECT_READ);
        break;
    case UPG_CMD_CAN_OBJECT_WRITE:
        UpgCommandWriteObject(ctx, frame);
        break;
    case UPG_CMD_PARAM_READ:
        UpgCommandParamRead(ctx, frame);
        break;
    case UPG_CMD_PARAM_WRITE:
        UpgCommandParamWrite(ctx, frame);
        break;
    case UPG_CMD_ENTER_BMS_IAP:
        UpgCommandEnterIap(ctx, frame);
        break;
    case UPG_CMD_UPGRADE_PREPARE:
        UpgCommandUpgradePrepare(ctx, frame);
        break;
    case UPG_CMD_UPGRADE_PACKET_DATA:
        UpgCommandUpgradePacketData(ctx, frame);
        break;
    case UPG_CMD_UPGRADE_PACKET_COMMIT:
        UpgCommandUpgradePacketCommit(ctx, frame);
        break;
    case UPG_CMD_UPGRADE_FINISH:
        UpgCommandUpgradeFinish(ctx, frame);
        break;
    case UPG_CMD_UPGRADE_ABORT:
        memset(&ctx->upgrade, 0, sizeof(ctx->upgrade));
        ctx->upgrade.state = UPG_UPGRADE_IDLE;
        UpgClearPending(ctx);
        (void)UpgRespond(ctx, frame->cmd, frame->seq, UPG_STATUS_OK, 0U, 0, 0U);
        break;
    case UPG_CMD_GET_UPGRADE_STATUS:
        UpgRespondUpgradeStatus(ctx, frame->cmd, frame->seq);
        break;
    case UPG_CMD_CAN_RAW_TX:
        UpgCommandRawCanTx(ctx, frame);
        break;
    case UPG_CMD_MCU_RESET:
        (void)UpgRespond(ctx, frame->cmd, frame->seq, UPG_STATUS_OK, 0U, 0, 0U);
        if (ctx->hal.reset != 0)
        {
            ctx->hal.reset(ctx->hal.user);
        }
        break;
    default:
        (void)UpgRespond(ctx, frame->cmd, frame->seq, UPG_STATUS_BAD_PARAM, 0x7FU, 0, 0U);
        break;
    }
}

static void UpgCore_HandleSerialError(uint8_t status, void *user)
{
    UpgCore *ctx = (UpgCore *)user;

    if (ctx != 0)
    {
        (void)UpgRespond(ctx, 0U, 0U, status, 0U, 0, 0U);
    }
}

static void UpgHandleObjectAck(UpgCore *ctx, const UpgCanFrame *frame)
{
    uint8_t data[16];
    int32_t raw;

    if (UpgFeidao_IsErrAckFor(frame, ctx->device_node, ctx->host_node, ctx->pending.index, ctx->pending.chd) != 0U)
    {
        UpgFinishPendingError(ctx, UPG_STATUS_BMS_ERROR, frame->data[0]);
        return;
    }
    if (UpgFeidao_IsAckFor(frame, ctx->device_node, ctx->host_node, ctx->pending.index, ctx->pending.chd) == 0U)
    {
        return;
    }

    if (ctx->pending.type == UPG_PENDING_OBJECT_READ)
    {
        data[0] = ctx->pending.index;
        data[1] = ctx->pending.chd;
        memcpy(&data[2], frame->data, 8U);
        (void)UpgRespond(ctx, ctx->pending.cmd, ctx->pending.seq, UPG_STATUS_OK, 0U, data, 10U);
        UpgClearPending(ctx);
    }
    else if (ctx->pending.type == UPG_PENDING_OBJECT_WRITE)
    {
        data[0] = ctx->pending.index;
        data[1] = ctx->pending.chd;
        (void)UpgRespond(ctx, ctx->pending.cmd, ctx->pending.seq, UPG_STATUS_OK, 0U, data, 2U);
        UpgClearPending(ctx);
    }
    else if (ctx->pending.type == UPG_PENDING_PARAM_READ_OBJECT)
    {
        if (UpgParam_ReadRaw(ctx->pending.param, frame->data, &raw) == 0U)
        {
            UpgFinishPendingError(ctx, UPG_STATUS_BAD_PARAM, 3U);
            return;
        }
        UpgWriteBe16(&data[0], ctx->pending.param_id);
        UpgWriteBeS32(&data[2], raw);
        memcpy(&data[6], frame->data, 8U);
        (void)UpgRespond(ctx, ctx->pending.cmd, ctx->pending.seq, UPG_STATUS_OK, 0U, data, 14U);
        UpgClearPending(ctx);
    }
    else if (ctx->pending.type == UPG_PENDING_PARAM_WRITE_READ_OBJECT)
    {
        memcpy(ctx->pending.object_data, frame->data, 8U);
        if (UpgParam_WriteRaw(ctx->pending.param, ctx->pending.write_value, ctx->pending.object_data) == 0U)
        {
            UpgFinishPendingError(ctx, UPG_STATUS_BAD_PARAM, 4U);
            return;
        }
        ctx->pending.type = UPG_PENDING_PARAM_WRITE_OBJECT;
        if (UpgCanWriteObject(ctx, ctx->pending.index, ctx->pending.chd, ctx->pending.object_data) == 0U)
        {
            UpgFinishPendingError(ctx, UPG_STATUS_UNKNOWN, 0U);
        }
    }
    else if (ctx->pending.type == UPG_PENDING_PARAM_WRITE_OBJECT)
    {
        UpgWriteBe16(&data[0], ctx->pending.param_id);
        UpgWriteBeS32(&data[2], ctx->pending.write_value);
        memcpy(&data[6], ctx->pending.object_data, 8U);
        (void)UpgRespond(ctx, ctx->pending.cmd, ctx->pending.seq, UPG_STATUS_OK, 0U, data, 14U);
        UpgClearPending(ctx);
    }
    else if (ctx->pending.type == UPG_PENDING_ENTER_IAP)
    {
        (void)UpgRespond(ctx, ctx->pending.cmd, ctx->pending.seq, UPG_STATUS_OK, 0U, 0, 0U);
        UpgClearPending(ctx);
    }
    else
    {
        /* Not an object-level pending operation. */
    }
}

static void UpgHandleUpgradeAck(UpgCore *ctx, const UpgCanFrame *frame)
{
    UpgFeidaoId id;
    uint8_t status;

    if ((frame->extended == 0U) || (frame->dlc != 8U))
    {
        return;
    }
    id = UpgFeidao_DecodeId(frame->id);
    if ((id.src != ctx->device_node) || (id.dst != ctx->host_node))
    {
        return;
    }

    if ((ctx->pending.type == UPG_PENDING_UPGRADE_PREPARE) &&
        (id.ctrl == FEIDAO_CTRL_ACK) &&
        (id.index == FEIDAO_UPGRADE_START_INDEX) &&
        (id.chd == FEIDAO_UPGRADE_START_ACK_CHD))
    {
        if (frame->data[0] != 1U)
        {
            ctx->upgrade.state = UPG_UPGRADE_ERROR;
            ctx->upgrade.last_error = frame->data[0];
            UpgFinishPendingError(ctx, UPG_STATUS_BMS_ERROR, frame->data[0]);
            return;
        }
        ctx->upgrade.state = UPG_UPGRADE_TRANSFERRING;
        (void)UpgRespond(ctx, ctx->pending.cmd, ctx->pending.seq, UPG_STATUS_OK, 0U, 0, 0U);
        UpgClearPending(ctx);
        return;
    }

    if ((id.ctrl == FEIDAO_CTRL_LONG_START) &&
        (id.index == FEIDAO_UPGRADE_DATA_INDEX) &&
        (id.chd == FEIDAO_UPGRADE_CHUNK_CHD))
    {
        status = frame->data[0];
        if (status == FEIDAO_UPGRADE_STATUS_CHUNK_OK)
        {
            if (ctx->pending.type == UPG_PENDING_UPGRADE_COMMIT)
            {
                ctx->upgrade.acked_long_packets++;
                ctx->upgrade.state = (ctx->upgrade.acked_long_packets >= ctx->upgrade.total_long_packets) ? UPG_UPGRADE_WAIT_FINAL : UPG_UPGRADE_TRANSFERRING;
                (void)UpgRespond(ctx, ctx->pending.cmd, ctx->pending.seq, UPG_STATUS_OK, 0U, 0, 0U);
                UpgClearPending(ctx);
            }
        }
        else if (status == FEIDAO_UPGRADE_STATUS_DONE)
        {
            ctx->upgrade.state = UPG_UPGRADE_DONE;
            if (ctx->pending.type == UPG_PENDING_UPGRADE_FINISH)
            {
                (void)UpgRespond(ctx, ctx->pending.cmd, ctx->pending.seq, UPG_STATUS_OK, 0U, 0, 0U);
                UpgClearPending(ctx);
            }
        }
        else
        {
            ctx->upgrade.state = UPG_UPGRADE_ERROR;
            ctx->upgrade.last_error = status;
            if ((ctx->pending.type == UPG_PENDING_UPGRADE_COMMIT) ||
                (ctx->pending.type == UPG_PENDING_UPGRADE_FINISH))
            {
                UpgFinishPendingError(ctx, UPG_STATUS_BMS_ERROR, status);
            }
        }
    }
}

static void UpgHandleEnterIapAck(UpgCore *ctx, const UpgCanFrame *frame)
{
    uint16_t expected_crc;
    uint16_t actual_crc;

    if ((ctx->pending.type != UPG_PENDING_ENTER_IAP) ||
        (frame->extended != 0U) ||
        (frame->id != BMS_APP_CAN_ACK_ID) ||
        (frame->dlc != 8U))
    {
        return;
    }

    if ((frame->data[0] != BMS_APP_ACK_MAGIC0) ||
        (frame->data[1] != BMS_APP_ACK_MAGIC1) ||
        (frame->data[2] != BMS_APP_CMD_ENTER_IAP))
    {
        return;
    }

    expected_crc = UpgReadBe16(&frame->data[6]);
    actual_crc = UpgCrc16_Calc(frame->data, 6U);
    if (expected_crc != actual_crc)
    {
        UpgFinishPendingError(ctx, UPG_STATUS_BMS_CRC_ERROR, 0U);
        return;
    }

    if (frame->data[3] != BMS_APP_STATUS_OK)
    {
        UpgFinishPendingError(ctx, UPG_STATUS_BMS_ERROR, frame->data[3]);
        return;
    }

    (void)UpgRespond(ctx, ctx->pending.cmd, ctx->pending.seq, UPG_STATUS_OK, 0U, &frame->data[4], 2U);
    UpgClearPending(ctx);
}

void UpgCore_Init(UpgCore *ctx, const UpgHal *hal)
{
    if (ctx == 0)
    {
        return;
    }
    memset(ctx, 0, sizeof(*ctx));
    if (hal != 0)
    {
        ctx->hal = *hal;
    }
    ctx->can_bitrate = 250000UL;
    ctx->host_node = FEIDAO_NODE_IOT;
    ctx->device_node = FEIDAO_NODE_BATTERY;
    ctx->pending.type = UPG_PENDING_NONE;
    ctx->upgrade.state = UPG_UPGRADE_IDLE;
    UpgSerial_InitParser(&ctx->serial_parser);
}

void UpgCore_SetNow(UpgCore *ctx, uint32_t now_ms)
{
    if (ctx != 0)
    {
        ctx->now_ms = now_ms;
    }
}

void UpgCore_Tick(UpgCore *ctx, uint32_t now_ms)
{
    if (ctx == 0)
    {
        return;
    }
    ctx->now_ms = now_ms;
    if ((ctx->pending.type != UPG_PENDING_NONE) &&
        ((int32_t)(ctx->now_ms - ctx->pending.deadline_ms) >= 0))
    {
        if ((ctx->pending.type == UPG_PENDING_UPGRADE_COMMIT) ||
            (ctx->pending.type == UPG_PENDING_UPGRADE_FINISH) ||
            (ctx->pending.type == UPG_PENDING_UPGRADE_PREPARE))
        {
            ctx->upgrade.state = UPG_UPGRADE_ERROR;
            ctx->upgrade.last_error = UPG_STATUS_CAN_TIMEOUT;
        }
        UpgFinishPendingError(ctx, UPG_STATUS_CAN_TIMEOUT, 0U);
    }
}

void UpgCore_OnSerialBytes(UpgCore *ctx, const uint8_t *data, uint16_t len)
{
    if (ctx == 0)
    {
        return;
    }
    UpgSerial_ParseBytes(&ctx->serial_parser, data, len, UpgCore_HandleSerialFrame, UpgCore_HandleSerialError, ctx);
}

void UpgCore_OnCanFrame(UpgCore *ctx, const UpgCanFrame *frame)
{
    if ((ctx == 0) || (frame == 0))
    {
        return;
    }
    UpgSnapshot_Update(ctx, frame);
    UpgHandleUpgradeAck(ctx, frame);
    UpgHandleEnterIapAck(ctx, frame);
    if (ctx->pending.type != UPG_PENDING_NONE)
    {
        UpgHandleObjectAck(ctx, frame);
    }
}
