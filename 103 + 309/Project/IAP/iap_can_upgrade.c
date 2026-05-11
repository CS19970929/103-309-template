#include "iap_can_upgrade.h"

#include "iap_config.h"
#include "iap_crc16.h"
#include "iap_flash.h"

#include "misc.h"
#include "stm32f10x.h"
#include "stm32f10x_can.h"
#include "stm32f10x_gpio.h"
#include "stm32f10x_rcc.h"

#include <string.h>

extern uint32_t Iap_GetTickMs(void);

typedef struct
{
    uint8_t src;
    uint8_t dst;
    uint8_t ctrl;
    uint8_t index;
    uint8_t chd_index;
} IapFeidaoId;

typedef enum
{
    IAP_SESSION_IDLE = 0,
    IAP_SESSION_WAIT_CHUNK,
    IAP_SESSION_RECV_CHUNK,
    IAP_SESSION_ERROR,
    IAP_SESSION_DONE
} IapSessionState;

typedef struct
{
    uint8_t active;
    uint8_t flash_open;
    uint8_t host_node;
    uint8_t state;
    uint16_t total_long_packets;
    uint16_t packets_received;
    uint16_t expected_next_long_index;
    uint16_t current_long_index;
    uint16_t current_frame_count;
    uint16_t current_frames_received;
    uint16_t expected_file_crc;
    uint16_t running_file_crc;
    uint16_t current_chunk_crc;
    uint32_t image_size;
    uint32_t bytes_written;
    uint32_t reset_at_ms;
} IapUpgradeSession;

static IapUpgradeSession s_session;
static uint8_t s_chunk_buffer[IAP_LONG_PACKET_BYTES];

static uint16_t Iap_ReadBe16(const uint8_t *data)
{
    return (uint16_t)(((uint16_t)data[0] << 8U) | data[1]);
}

static uint32_t Iap_ReadBe32(const uint8_t *data)
{
    return (((uint32_t)data[0] << 24U) |
            ((uint32_t)data[1] << 16U) |
            ((uint32_t)data[2] << 8U) |
            data[3]);
}

static uint32_t Iap_BuildFeidaoId(uint8_t src, uint8_t dst, uint8_t ctrl, uint8_t index, uint8_t chd)
{
    return (((uint32_t)(src & 0x1FU) << 24U) |
            ((uint32_t)(dst & 0x1FU) << 19U) |
            ((uint32_t)(ctrl & 0x07U) << 16U) |
            ((uint32_t)index << 8U) |
            chd);
}

static void Iap_DecodeFeidaoId(uint32_t can_id, IapFeidaoId *decoded)
{
    decoded->src = (uint8_t)((can_id >> 24U) & 0x1FU);
    decoded->dst = (uint8_t)((can_id >> 19U) & 0x1FU);
    decoded->ctrl = (uint8_t)((can_id >> 16U) & 0x07U);
    decoded->index = (uint8_t)((can_id >> 8U) & 0xFFU);
    decoded->chd_index = (uint8_t)(can_id & 0xFFU);
}

static uint16_t Iap_ExpectedLongPackets(uint32_t image_size)
{
    uint32_t data_frames;
    uint32_t long_packets;

    data_frames = (image_size + 7U) / 8U;
    long_packets = (data_frames + IAP_LONG_PACKET_MAX_FRAMES - 1U) / IAP_LONG_PACKET_MAX_FRAMES;
    if (long_packets > 0xFFFFU)
    {
        return 0U;
    }
    return (uint16_t)long_packets;
}

static uint8_t Iap_DstMatches(uint8_t dst)
{
    if (dst == IAP_CAN_DEVICE_NODE)
    {
        return 1U;
    }
    if (dst == FEIDAO_NODE_BROADCAST)
    {
        return 1U;
    }
    return 0U;
}

static uint8_t IapCan_Send(uint32_t can_id, const uint8_t *data)
{
    CanTxMsg tx;
    uint8_t mailbox;
    uint32_t start_ms;
    uint8_t status;
    uint8_t index;

    tx.StdId = 0U;
    tx.ExtId = can_id;
    tx.IDE = CAN_ID_EXT;
    tx.RTR = CAN_RTR_DATA;
    tx.DLC = 8U;
    for (index = 0U; index < 8U; index++)
    {
        tx.Data[index] = data[index];
    }

    mailbox = CAN_Transmit(CAN1, &tx);
    if (mailbox > 2U)
    {
        return 0U;
    }

    start_ms = Iap_GetTickMs();
    do
    {
        status = CAN_TransmitStatus(CAN1, mailbox);
        if (status == CAN_TxStatus_Ok)
        {
            return 1U;
        }
        if (status == CAN_TxStatus_Failed)
        {
            break;
        }
    } while ((uint32_t)(Iap_GetTickMs() - start_ms) < 50U);

    CAN_CancelTransmit(CAN1, mailbox);
    return 0U;
}

static void IapCan_SendStartAck(uint8_t host_node, uint8_t ready)
{
    uint8_t data[8];
    uint8_t index;
    uint32_t can_id;

    data[0] = ready;
    for (index = 1U; index < 8U; index++)
    {
        data[index] = 0U;
    }

    can_id = Iap_BuildFeidaoId(
        IAP_CAN_DEVICE_NODE,
        host_node,
        FEIDAO_CTRL_ACK,
        FEIDAO_UPGRADE_START_INDEX,
        FEIDAO_UPGRADE_START_ACK_CHD);
    (void)IapCan_Send(can_id, data);
}

static void IapCan_SendChunkStatus(uint8_t status)
{
    uint8_t data[8];
    uint8_t index;
    uint32_t can_id;

    data[0] = status;
    for (index = 1U; index < 8U; index++)
    {
        data[index] = 0U;
    }

    can_id = Iap_BuildFeidaoId(
        IAP_CAN_DEVICE_NODE,
        s_session.host_node,
        FEIDAO_CTRL_LONG_START,
        FEIDAO_UPGRADE_DATA_INDEX,
        FEIDAO_UPGRADE_CHUNK_CHD);
    (void)IapCan_Send(can_id, data);
}

static void IapCan_ResetSession(void)
{
    if (s_session.flash_open != 0U)
    {
        IapFlash_EndUpgrade();
    }

    s_session.active = 0U;
    s_session.flash_open = 0U;
    s_session.host_node = IAP_CAN_HOST_NODE_DEFAULT;
    s_session.state = IAP_SESSION_IDLE;
    s_session.total_long_packets = 0U;
    s_session.packets_received = 0U;
    s_session.expected_next_long_index = 0U;
    s_session.current_long_index = 0U;
    s_session.current_frame_count = 0U;
    s_session.current_frames_received = 0U;
    s_session.expected_file_crc = 0U;
    s_session.running_file_crc = 0xFFFFU;
    s_session.current_chunk_crc = 0xFFFFU;
    s_session.image_size = 0U;
    s_session.bytes_written = 0U;
    s_session.reset_at_ms = 0U;
}

static void IapCan_AbortWithStatus(uint8_t status)
{
    if (s_session.flash_open != 0U)
    {
        IapFlash_EndUpgrade();
        s_session.flash_open = 0U;
    }
    IapCan_SendChunkStatus(status);
    s_session.active = 0U;
    s_session.state = IAP_SESSION_ERROR;
}

static void IapCan_HandleStart(const IapFeidaoId *id, const uint8_t *data)
{
    uint16_t total_long_packets;
    uint16_t expected_long_packets;
    uint16_t file_crc;
    uint32_t image_size;

    total_long_packets = Iap_ReadBe16(&data[0]);
    file_crc = Iap_ReadBe16(&data[2]);
    image_size = Iap_ReadBe32(&data[4]);
    expected_long_packets = Iap_ExpectedLongPackets(image_size);

    if ((IapFlash_IsImageSizeValid(image_size) == 0U) ||
        (total_long_packets == 0U) ||
        (expected_long_packets == 0U) ||
        (total_long_packets != expected_long_packets))
    {
        IapCan_SendStartAck(id->src, 0U);
        return;
    }

    IapCan_ResetSession();
    if (IapFlash_BeginUpgrade(image_size) == 0U)
    {
        IapCan_SendStartAck(id->src, 0U);
        return;
    }

    s_session.active = 1U;
    s_session.flash_open = 1U;
    s_session.host_node = id->src;
    s_session.state = IAP_SESSION_WAIT_CHUNK;
    s_session.total_long_packets = total_long_packets;
    s_session.expected_file_crc = file_crc;
    s_session.running_file_crc = 0xFFFFU;
    s_session.image_size = image_size;
    s_session.bytes_written = 0U;
    s_session.packets_received = 0U;
    s_session.expected_next_long_index = 0U;

    IapCan_SendStartAck(id->src, 1U);
}

static void IapCan_HandleChunkStart(const uint8_t *data)
{
    uint16_t long_index;
    uint16_t frame_count;

    if ((s_session.active == 0U) || (s_session.state != IAP_SESSION_WAIT_CHUNK))
    {
        IapCan_AbortWithStatus(IAP_STATUS_OTHER_ERROR);
        return;
    }

    long_index = Iap_ReadBe16(&data[0]);
    frame_count = Iap_ReadBe16(&data[2]);
    if ((frame_count == 0U) || (frame_count > IAP_LONG_PACKET_MAX_FRAMES))
    {
        IapCan_AbortWithStatus(IAP_STATUS_OTHER_ERROR);
        return;
    }

    if (s_session.packets_received == 0U)
    {
        if ((long_index != 0U) && (long_index != 1U))
        {
            IapCan_AbortWithStatus(IAP_STATUS_OTHER_ERROR);
            return;
        }
        s_session.expected_next_long_index = long_index;
    }
    if (long_index != s_session.expected_next_long_index)
    {
        IapCan_AbortWithStatus(IAP_STATUS_OTHER_ERROR);
        return;
    }

    s_session.current_long_index = long_index;
    s_session.current_frame_count = frame_count;
    s_session.current_frames_received = 0U;
    s_session.current_chunk_crc = 0xFFFFU;
    memset(s_chunk_buffer, 0, sizeof(s_chunk_buffer));
    s_session.state = IAP_SESSION_RECV_CHUNK;
}

static void IapCan_HandleChunkData(const IapFeidaoId *id, const uint8_t *data)
{
    uint8_t expected_seq;
    uint32_t offset;

    if ((s_session.active == 0U) || (s_session.state != IAP_SESSION_RECV_CHUNK))
    {
        IapCan_AbortWithStatus(IAP_STATUS_OTHER_ERROR);
        return;
    }
    if (s_session.current_frames_received >= s_session.current_frame_count)
    {
        IapCan_AbortWithStatus(IAP_STATUS_OTHER_ERROR);
        return;
    }

    expected_seq = (uint8_t)(s_session.current_frames_received & 0xFFU);
    if (id->chd_index != expected_seq)
    {
        IapCan_AbortWithStatus(IAP_STATUS_OTHER_ERROR);
        return;
    }

    offset = (uint32_t)s_session.current_frames_received * 8U;
    memcpy(&s_chunk_buffer[offset], data, 8U);
    s_session.current_chunk_crc = IapCrc16_Update(s_session.current_chunk_crc, data, 8U);
    s_session.current_frames_received++;
}

static void IapCan_FinishUpgrade(void)
{
    if ((s_session.bytes_written != s_session.image_size) ||
        (s_session.running_file_crc != s_session.expected_file_crc))
    {
        IapCan_AbortWithStatus(IAP_STATUS_FILE_CRC_ERROR);
        return;
    }

    if (s_session.flash_open != 0U)
    {
        IapFlash_EndUpgrade();
        s_session.flash_open = 0U;
    }

    if (IapFlash_IsAppVectorValid() == 0U)
    {
        IapCan_AbortWithStatus(IAP_STATUS_OTHER_ERROR);
        return;
    }

    if (IapFlash_ClearUpdateFlagPreservePage() == 0U)
    {
        IapCan_AbortWithStatus(IAP_STATUS_OTHER_ERROR);
        return;
    }

    IapCan_SendChunkStatus(IAP_STATUS_CHUNK_OK);
    IapCan_SendChunkStatus(IAP_STATUS_DONE);
    s_session.state = IAP_SESSION_DONE;
    s_session.reset_at_ms = Iap_GetTickMs() + IAP_FINAL_RESET_DELAY_MS;
}

static void IapCan_HandleChunkEnd(const uint8_t *data)
{
    uint16_t expected_crc;
    uint32_t chunk_data_len;
    uint32_t remaining;
    uint32_t write_len;

    if ((s_session.active == 0U) || (s_session.state != IAP_SESSION_RECV_CHUNK))
    {
        IapCan_AbortWithStatus(IAP_STATUS_OTHER_ERROR);
        return;
    }
    if (s_session.current_frames_received != s_session.current_frame_count)
    {
        IapCan_AbortWithStatus(IAP_STATUS_OTHER_ERROR);
        return;
    }

    expected_crc = Iap_ReadBe16(&data[0]);
    if (expected_crc != s_session.current_chunk_crc)
    {
        IapCan_AbortWithStatus(IAP_STATUS_CHUNK_CRC_ERROR);
        return;
    }

    chunk_data_len = (uint32_t)s_session.current_frame_count * 8U;
    remaining = s_session.image_size - s_session.bytes_written;
    write_len = (remaining >= chunk_data_len) ? chunk_data_len : remaining;
    if (write_len == 0U)
    {
        IapCan_AbortWithStatus(IAP_STATUS_OTHER_ERROR);
        return;
    }

    if (IapFlash_WriteBytes(IAP_APP_BASE + s_session.bytes_written, s_chunk_buffer, write_len) == 0U)
    {
        IapCan_AbortWithStatus(IAP_STATUS_OTHER_ERROR);
        return;
    }
    s_session.running_file_crc = IapCrc16_Update(s_session.running_file_crc, s_chunk_buffer, write_len);
    s_session.bytes_written += write_len;

    s_session.packets_received++;
    s_session.expected_next_long_index = (uint16_t)(s_session.current_long_index + 1U);
    s_session.state = IAP_SESSION_WAIT_CHUNK;

    if (s_session.packets_received >= s_session.total_long_packets)
    {
        IapCan_FinishUpgrade();
    }
    else
    {
        IapCan_SendChunkStatus(IAP_STATUS_CHUNK_OK);
    }
}

static void IapCan_ProcessMessage(const CanRxMsg *msg)
{
    IapFeidaoId id;

    if ((msg->IDE != CAN_ID_EXT) || (msg->DLC != 8U))
    {
        return;
    }

    Iap_DecodeFeidaoId(msg->ExtId, &id);
    if (Iap_DstMatches(id.dst) == 0U)
    {
        return;
    }

    if ((id.ctrl == FEIDAO_CTRL_WRITE) &&
        (id.index == FEIDAO_UPGRADE_START_INDEX) &&
        (id.chd_index == 0U))
    {
        IapCan_HandleStart(&id, msg->Data);
        return;
    }

    if ((s_session.active == 0U) || (id.src != s_session.host_node))
    {
        return;
    }

    if ((id.index != FEIDAO_UPGRADE_DATA_INDEX) ||
        (id.chd_index != FEIDAO_UPGRADE_CHUNK_CHD &&
         id.ctrl != FEIDAO_CTRL_LONG_DATA))
    {
        return;
    }

    if (id.ctrl == FEIDAO_CTRL_LONG_START)
    {
        IapCan_HandleChunkStart(msg->Data);
    }
    else if (id.ctrl == FEIDAO_CTRL_LONG_DATA)
    {
        IapCan_HandleChunkData(&id, msg->Data);
    }
    else if (id.ctrl == FEIDAO_CTRL_LONG_END)
    {
        IapCan_HandleChunkEnd(msg->Data);
    }
    else
    {
        /* Ignore unrelated frames. */
    }
}

void IapCan_Init(void)
{
    GPIO_InitTypeDef gpio;
    CAN_InitTypeDef can_init;
    CAN_FilterInitTypeDef filter;

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_AFIO | RCC_APB2Periph_GPIOA | RCC_APB2Periph_GPIOB, ENABLE);
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_CAN1, ENABLE);
    GPIO_PinRemapConfig(GPIO_Remap_SWJ_JTAGDisable, ENABLE);

    GPIO_WriteBit(GPIOB, GPIO_Pin_4, Bit_RESET);
    gpio.GPIO_Pin = GPIO_Pin_4;
    gpio.GPIO_Mode = GPIO_Mode_Out_PP;
    gpio.GPIO_Speed = GPIO_Speed_2MHz;
    GPIO_Init(GPIOB, &gpio);

    gpio.GPIO_Pin = GPIO_Pin_11;
    gpio.GPIO_Mode = GPIO_Mode_IPU;
    gpio.GPIO_Speed = GPIO_Speed_2MHz;
    GPIO_Init(GPIOA, &gpio);

    gpio.GPIO_Pin = GPIO_Pin_12;
    gpio.GPIO_Mode = GPIO_Mode_AF_PP;
    gpio.GPIO_Speed = GPIO_Speed_2MHz;
    GPIO_Init(GPIOA, &gpio);

    CAN_DeInit(CAN1);
    CAN_StructInit(&can_init);
    can_init.CAN_TTCM = DISABLE;
    can_init.CAN_ABOM = ENABLE;
    can_init.CAN_AWUM = DISABLE;
    can_init.CAN_NART = DISABLE;
    can_init.CAN_RFLM = DISABLE;
    can_init.CAN_TXFP = DISABLE;
    can_init.CAN_Mode = CAN_Mode_Normal;
    can_init.CAN_SJW = CAN_SJW_1tq;
    can_init.CAN_BS1 = CAN_BS1_5tq;
    can_init.CAN_BS2 = CAN_BS2_2tq;
    can_init.CAN_Prescaler = 4U;
    (void)CAN_Init(CAN1, &can_init);

    filter.CAN_FilterNumber = 0U;
    filter.CAN_FilterMode = CAN_FilterMode_IdMask;
    filter.CAN_FilterScale = CAN_FilterScale_32bit;
    filter.CAN_FilterIdHigh = 0U;
    filter.CAN_FilterIdLow = 0U;
    filter.CAN_FilterMaskIdHigh = 0U;
    filter.CAN_FilterMaskIdLow = 0U;
    filter.CAN_FilterFIFOAssignment = CAN_Filter_FIFO0;
    filter.CAN_FilterActivation = ENABLE;
    CAN_FilterInit(&filter);

    IapCan_ResetSession();
}

void IapCan_Task(void)
{
    CanRxMsg msg;

    if ((s_session.state == IAP_SESSION_DONE) &&
        (s_session.reset_at_ms != 0U) &&
        ((int32_t)(Iap_GetTickMs() - s_session.reset_at_ms) >= 0))
    {
        NVIC_SystemReset();
    }

    while (CAN_MessagePending(CAN1, CAN_FIFO0) != 0U)
    {
        CAN_Receive(CAN1, CAN_FIFO0, &msg);
        IapCan_ProcessMessage(&msg);
    }
}

uint8_t IapCan_IsUpgradeActive(void)
{
    if (s_session.active != 0U)
    {
        return 1U;
    }
    if (s_session.state == IAP_SESSION_DONE)
    {
        return 1U;
    }
    return 0U;
}
