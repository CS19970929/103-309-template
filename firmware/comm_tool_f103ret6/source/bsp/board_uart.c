#include "board_uart.h"
#include "ct_board_port.h"
#include "stm32f10x_gpio.h"
#include "stm32f10x_rcc.h"
#include "stm32f10x_usart.h"
#include "misc.h"

#define BOARD_UART_RX_BUF_SIZE        1024u

static volatile uint16_t s_rx_head;
static volatile uint16_t s_rx_tail;
static uint8_t s_rx_buf[BOARD_UART_RX_BUF_SIZE];

static void rx_push(uint8_t byte)
{
    uint16_t next;

    next = (uint16_t)((s_rx_head + 1u) % BOARD_UART_RX_BUF_SIZE);
    if (next != s_rx_tail)
    {
        s_rx_buf[s_rx_head] = byte;
        s_rx_head = next;
    }
}

void BoardUart_Init(uint32_t baudrate)
{
    GPIO_InitTypeDef gpio;
    USART_InitTypeDef usart;
    NVIC_InitTypeDef nvic;

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_AFIO | RCC_APB2Periph_GPIOC, ENABLE);
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_USART3, ENABLE);

    GPIO_PinRemapConfig(GPIO_PartialRemap_USART3, ENABLE);

    GPIO_StructInit(&gpio);
    gpio.GPIO_Pin = GPIO_Pin_10;
    gpio.GPIO_Mode = GPIO_Mode_AF_PP;
    gpio.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOC, &gpio);

    gpio.GPIO_Pin = GPIO_Pin_11;
    gpio.GPIO_Mode = GPIO_Mode_IN_FLOATING;
    GPIO_Init(GPIOC, &gpio);

    USART_StructInit(&usart);
    usart.USART_BaudRate = baudrate;
    usart.USART_WordLength = USART_WordLength_8b;
    usart.USART_StopBits = USART_StopBits_1;
    usart.USART_Parity = USART_Parity_No;
    usart.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
    usart.USART_Mode = USART_Mode_Rx | USART_Mode_Tx;
    USART_Init(USART3, &usart);

    nvic.NVIC_IRQChannel = USART3_IRQn;
    nvic.NVIC_IRQChannelPreemptionPriority = 1u;
    nvic.NVIC_IRQChannelSubPriority = 0u;
    nvic.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&nvic);

    USART_ITConfig(USART3, USART_IT_RXNE, ENABLE);
    USART_Cmd(USART3, ENABLE);
}

int BoardUart_ReadByte(uint8_t *byte)
{
    if ((byte == 0) || (s_rx_head == s_rx_tail))
    {
        return 0;
    }

    *byte = s_rx_buf[s_rx_tail];
    s_rx_tail = (uint16_t)((s_rx_tail + 1u) % BOARD_UART_RX_BUF_SIZE);
    return 1;
}

int CtBoard_UartWrite(const uint8_t *data, uint16_t length)
{
    uint16_t i;

    if ((data == 0) || (length == 0u))
    {
        return 0;
    }

    for (i = 0u; i < length; ++i)
    {
        while (USART_GetFlagStatus(USART3, USART_FLAG_TXE) == RESET)
        {
        }
        USART_SendData(USART3, data[i]);
    }
    while (USART_GetFlagStatus(USART3, USART_FLAG_TC) == RESET)
    {
    }
    return 1;
}

void USART3_IRQHandler(void)
{
    if (USART_GetITStatus(USART3, USART_IT_RXNE) != RESET)
    {
        rx_push((uint8_t)USART_ReceiveData(USART3));
        USART_ClearITPendingBit(USART3, USART_IT_RXNE);
    }
    if (USART_GetFlagStatus(USART3, USART_FLAG_ORE) != RESET)
    {
        (void)USART3->SR;
        (void)USART3->DR;
    }
}
