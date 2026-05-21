#include "board_uart.h"
#include "stm32f10x_gpio.h"
#include "stm32f10x_rcc.h"
#include "stm32f10x_usart.h"
#include "misc.h"

#define BOARD_UART_RX_BUF_SIZE 512u

static volatile uint8_t s_rx_buf[BOARD_UART_RX_BUF_SIZE];
static volatile uint16_t s_rx_head;
static volatile uint16_t s_rx_tail;

static uint16_t BoardUart_Next(uint16_t value)
{
    value++;
    if (value >= BOARD_UART_RX_BUF_SIZE) {
        value = 0u;
    }
    return value;
}

void BoardUart_Init(uint32_t baudrate)
{
    GPIO_InitTypeDef gpio;
    USART_InitTypeDef usart;
    NVIC_InitTypeDef nvic;

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA | RCC_APB2Periph_USART1 | RCC_APB2Periph_AFIO, ENABLE);

    GPIO_StructInit(&gpio);
    gpio.GPIO_Pin = GPIO_Pin_9;
    gpio.GPIO_Mode = GPIO_Mode_AF_PP;
    gpio.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOA, &gpio);

    gpio.GPIO_Pin = GPIO_Pin_10;
    gpio.GPIO_Mode = GPIO_Mode_IN_FLOATING;
    GPIO_Init(GPIOA, &gpio);

    USART_StructInit(&usart);
    usart.USART_BaudRate = baudrate;
    usart.USART_WordLength = USART_WordLength_8b;
    usart.USART_StopBits = USART_StopBits_1;
    usart.USART_Parity = USART_Parity_No;
    usart.USART_Mode = USART_Mode_Rx | USART_Mode_Tx;
    usart.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
    USART_Init(USART1, &usart);
    USART_ITConfig(USART1, USART_IT_RXNE, ENABLE);
    USART_Cmd(USART1, ENABLE);

    nvic.NVIC_IRQChannel = USART1_IRQn;
    nvic.NVIC_IRQChannelPreemptionPriority = 1u;
    nvic.NVIC_IRQChannelSubPriority = 1u;
    nvic.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&nvic);
}

void BoardUart_SendByte(uint8_t value)
{
    while (USART_GetFlagStatus(USART1, USART_FLAG_TXE) == RESET) {
    }
    USART_SendData(USART1, value);
}

void BoardUart_Send(const uint8_t *data, uint16_t length)
{
    uint16_t i;

    for (i = 0u; i < length; i++) {
        BoardUart_SendByte(data[i]);
    }
}

uint8_t BoardUart_ReadByte(uint8_t *value)
{
    if (s_rx_head == s_rx_tail) {
        return 0u;
    }

    *value = s_rx_buf[s_rx_tail];
    s_rx_tail = BoardUart_Next(s_rx_tail);
    return 1u;
}

void BoardUart_IrqHandler(void)
{
    if (USART_GetITStatus(USART1, USART_IT_RXNE) != RESET) {
        uint8_t value = (uint8_t)USART_ReceiveData(USART1);
        uint16_t next = BoardUart_Next(s_rx_head);
        if (next != s_rx_tail) {
            s_rx_buf[s_rx_head] = value;
            s_rx_head = next;
        }
        USART_ClearITPendingBit(USART1, USART_IT_RXNE);
    }
}
