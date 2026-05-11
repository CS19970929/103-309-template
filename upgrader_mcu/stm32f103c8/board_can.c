#include "board_can.h"

#include "board_config.h"

#include "stm32f10x_can.h"
#include "stm32f10x_gpio.h"
#include "stm32f10x_rcc.h"

extern uint32_t Board_GetTickMs(void);

void BoardCan_Init(void)
{
    GPIO_InitTypeDef gpio;
    CAN_InitTypeDef can_init;
    CAN_FilterInitTypeDef filter;

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_AFIO | RCC_APB2Periph_GPIOA | UPG_BOARD_CAN_TRANSCEIVER_CLK, ENABLE);
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_CAN1, ENABLE);
    GPIO_PinRemapConfig(GPIO_Remap_SWJ_JTAGDisable, ENABLE);

    GPIO_WriteBit(UPG_BOARD_CAN_TRANSCEIVER_PORT, UPG_BOARD_CAN_TRANSCEIVER_PIN, UPG_BOARD_CAN_TRANSCEIVER_ON);
    gpio.GPIO_Pin = UPG_BOARD_CAN_TRANSCEIVER_PIN;
    gpio.GPIO_Mode = GPIO_Mode_Out_PP;
    gpio.GPIO_Speed = GPIO_Speed_2MHz;
    GPIO_Init(UPG_BOARD_CAN_TRANSCEIVER_PORT, &gpio);

    gpio.GPIO_Pin = GPIO_Pin_11;
    gpio.GPIO_Mode = GPIO_Mode_IPU;
    gpio.GPIO_Speed = GPIO_Speed_2MHz;
    GPIO_Init(GPIOA, &gpio);

    gpio.GPIO_Pin = GPIO_Pin_12;
    gpio.GPIO_Mode = GPIO_Mode_AF_PP;
    gpio.GPIO_Speed = GPIO_Speed_50MHz;
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
}

int BoardCan_Write(const UpgCanFrame *frame, uint32_t now_ms)
{
    CanTxMsg tx;
    uint8_t mailbox;
    uint8_t index;
    uint8_t status;

    tx.StdId = 0U;
    tx.ExtId = 0U;
    tx.IDE = (frame->extended != 0U) ? CAN_ID_EXT : CAN_ID_STD;
    if (tx.IDE == CAN_ID_EXT)
    {
        tx.ExtId = frame->id & 0x1FFFFFFFUL;
    }
    else
    {
        tx.StdId = (uint16_t)(frame->id & 0x7FFU);
    }
    tx.RTR = CAN_RTR_DATA;
    tx.DLC = (frame->dlc <= 8U) ? frame->dlc : 8U;
    for (index = 0U; index < tx.DLC; index++)
    {
        tx.Data[index] = frame->data[index];
    }
    for (; index < 8U; index++)
    {
        tx.Data[index] = 0U;
    }

    mailbox = CAN_Transmit(CAN1, &tx);
    if (mailbox > 2U)
    {
        return -1;
    }

    while ((uint32_t)(Board_GetTickMs() - now_ms) < UPG_BOARD_CAN_TX_TIMEOUT_MS)
    {
        status = CAN_TransmitStatus(CAN1, mailbox);
        if (status == CAN_TxStatus_Ok)
        {
            return 0;
        }
        if (status == CAN_TxStatus_Failed)
        {
            break;
        }
    }

    CAN_CancelTransmit(CAN1, mailbox);
    return -1;
}

uint8_t BoardCan_Read(UpgCanFrame *frame)
{
    CanRxMsg rx;
    uint8_t index;

    if (CAN_MessagePending(CAN1, CAN_FIFO0) == 0U)
    {
        return 0U;
    }

    CAN_Receive(CAN1, CAN_FIFO0, &rx);
    frame->extended = (rx.IDE == CAN_ID_EXT) ? 1U : 0U;
    frame->id = (frame->extended != 0U) ? rx.ExtId : rx.StdId;
    frame->dlc = (rx.DLC <= 8U) ? rx.DLC : 8U;
    for (index = 0U; index < frame->dlc; index++)
    {
        frame->data[index] = rx.Data[index];
    }
    for (; index < 8U; index++)
    {
        frame->data[index] = 0U;
    }
    return 1U;
}
