#include "board_can.h"
#include "stm32f10x_can.h"
#include "stm32f10x_gpio.h"
#include "stm32f10x_rcc.h"

void BoardCan_Init(uint32_t bitrate)
{
    GPIO_InitTypeDef gpio;
    CAN_InitTypeDef can;
    CAN_FilterInitTypeDef filter;

    (void)bitrate;

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA | RCC_APB2Periph_AFIO, ENABLE);
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_CAN1, ENABLE);

    GPIO_StructInit(&gpio);
    gpio.GPIO_Pin = GPIO_Pin_11;
    gpio.GPIO_Mode = GPIO_Mode_IPU;
    gpio.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOA, &gpio);

    gpio.GPIO_Pin = GPIO_Pin_12;
    gpio.GPIO_Mode = GPIO_Mode_AF_PP;
    GPIO_Init(GPIOA, &gpio);

    CAN_DeInit(CAN1);
    CAN_StructInit(&can);
    can.CAN_TTCM = DISABLE;
    can.CAN_ABOM = ENABLE;
    can.CAN_AWUM = ENABLE;
    can.CAN_NART = DISABLE;
    can.CAN_RFLM = DISABLE;
    can.CAN_TXFP = DISABLE;
    can.CAN_Mode = CAN_Mode_Normal;
    can.CAN_SJW = CAN_SJW_1tq;
    can.CAN_BS1 = CAN_BS1_13tq;
    can.CAN_BS2 = CAN_BS2_2tq;
    can.CAN_Prescaler = 9u;
    CAN_Init(CAN1, &can);

    filter.CAN_FilterNumber = 0u;
    filter.CAN_FilterMode = CAN_FilterMode_IdMask;
    filter.CAN_FilterScale = CAN_FilterScale_32bit;
    filter.CAN_FilterIdHigh = 0u;
    filter.CAN_FilterIdLow = 0u;
    filter.CAN_FilterMaskIdHigh = 0u;
    filter.CAN_FilterMaskIdLow = 0u;
    filter.CAN_FilterFIFOAssignment = CAN_FIFO0;
    filter.CAN_FilterActivation = ENABLE;
    CAN_FilterInit(&filter);
}

uint8_t BoardCan_Send(const BoardCanFrame *frame)
{
    CanTxMsg msg;
    uint8_t i;
    uint8_t mailbox;

    if (frame == 0) {
        return 0u;
    }

    msg.StdId = 0u;
    msg.ExtId = frame->id;
    msg.IDE = frame->is_extended ? CAN_ID_EXT : CAN_ID_STD;
    msg.RTR = CAN_RTR_DATA;
    msg.DLC = frame->dlc;
    for (i = 0u; i < 8u; i++) {
        msg.Data[i] = frame->data[i];
    }

    mailbox = CAN_Transmit(CAN1, &msg);
    return (mailbox != CAN_TxStatus_NoMailBox) ? 1u : 0u;
}

uint8_t BoardCan_Receive(BoardCanFrame *frame)
{
    CanRxMsg msg;
    uint8_t i;

    if ((frame == 0) || (CAN_MessagePending(CAN1, CAN_FIFO0) == 0u)) {
        return 0u;
    }

    CAN_Receive(CAN1, CAN_FIFO0, &msg);
    frame->is_extended = (msg.IDE == CAN_ID_EXT) ? 1u : 0u;
    frame->id = frame->is_extended ? msg.ExtId : msg.StdId;
    frame->dlc = msg.DLC;
    for (i = 0u; i < 8u; i++) {
        frame->data[i] = msg.Data[i];
    }
    return 1u;
}
