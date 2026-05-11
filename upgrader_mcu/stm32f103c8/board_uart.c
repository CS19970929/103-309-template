#include "board_uart.h"

#include "board_config.h"

#include "misc.h"
#include "stm32f10x_gpio.h"
#include "stm32f10x_rcc.h"
#include "stm32f10x_usart.h"

extern uint32_t Board_GetTickMs(void);

static volatile uint8_t s_rx_buffer[UPG_BOARD_UART_RX_BUFFER_SIZE];
static volatile uint16_t s_rx_head;
static volatile uint16_t s_rx_tail;

static uint16_t BoardUart_NextIndex(uint16_t value)
{
    value++;
    if (value >= UPG_BOARD_UART_RX_BUFFER_SIZE)
    {
        value = 0U;
    }
    return value;
}

void BoardUart_Init(void)
{
    GPIO_InitTypeDef gpio;
    USART_InitTypeDef usart;
    NVIC_InitTypeDef nvic;

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_AFIO | RCC_APB2Periph_GPIOA | RCC_APB2Periph_USART1, ENABLE);

    gpio.GPIO_Pin = GPIO_Pin_9;
    gpio.GPIO_Mode = GPIO_Mode_AF_PP;
    gpio.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOA, &gpio);

    gpio.GPIO_Pin = GPIO_Pin_10;
    gpio.GPIO_Mode = GPIO_Mode_IPU;
    gpio.GPIO_Speed = GPIO_Speed_2MHz;
    GPIO_Init(GPIOA, &gpio);

    USART_DeInit(USART1);
    USART_StructInit(&usart);
    usart.USART_BaudRate = UPG_BOARD_UART_BAUD;
    usart.USART_WordLength = USART_WordLength_8b;
    usart.USART_StopBits = USART_StopBits_1;
    usart.USART_Parity = USART_Parity_No;
    usart.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
    usart.USART_Mode = USART_Mode_Rx | USART_Mode_Tx;
    USART_Init(USART1, &usart);

    nvic.NVIC_IRQChannel = USART1_IRQn;
    nvic.NVIC_IRQChannelPreemptionPriority = 2U;
    nvic.NVIC_IRQChannelSubPriority = 0U;
    nvic.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&nvic);

    USART_ITConfig(USART1, USART_IT_RXNE, ENABLE);
    USART_Cmd(USART1, ENABLE);
}

uint16_t BoardUart_Read(uint8_t *data, uint16_t max_len)
{
    uint16_t count;

    count = 0U;
    while ((count < max_len) && (s_rx_tail != s_rx_head))
    {
        data[count] = s_rx_buffer[s_rx_tail];
        s_rx_tail = BoardUart_NextIndex(s_rx_tail);
        count++;
    }
    return count;
}

int BoardUart_Write(const uint8_t *data, uint16_t len, uint32_t now_ms)
{
    uint16_t index;
    uint32_t start_ms;

    (void)now_ms;
    for (index = 0U; index < len; index++)
    {
        start_ms = Board_GetTickMs();
        while (USART_GetFlagStatus(USART1, USART_FLAG_TXE) == RESET)
        {
            if ((uint32_t)(Board_GetTickMs() - start_ms) > UPG_BOARD_UART_TX_TIMEOUT_MS)
            {
                return -1;
            }
        }
        USART_SendData(USART1, data[index]);
    }

    start_ms = Board_GetTickMs();
    while (USART_GetFlagStatus(USART1, USART_FLAG_TC) == RESET)
    {
        if ((uint32_t)(Board_GetTickMs() - start_ms) > UPG_BOARD_UART_TX_TIMEOUT_MS)
        {
            return -1;
        }
    }
    return 0;
}

void BoardUart_IrqHandler(void)
{
    uint16_t next;
    uint8_t value;

    if (USART_GetITStatus(USART1, USART_IT_RXNE) != RESET)
    {
        value = (uint8_t)(USART_ReceiveData(USART1) & 0xFFU);
        next = BoardUart_NextIndex(s_rx_head);
        if (next != s_rx_tail)
        {
            s_rx_buffer[s_rx_head] = value;
            s_rx_head = next;
        }
    }

    if ((USART_GetFlagStatus(USART1, USART_FLAG_ORE) != RESET) ||
        (USART_GetFlagStatus(USART1, USART_FLAG_NE) != RESET) ||
        (USART_GetFlagStatus(USART1, USART_FLAG_FE) != RESET))
    {
        (void)USART1->SR;
        (void)USART1->DR;
    }
}
