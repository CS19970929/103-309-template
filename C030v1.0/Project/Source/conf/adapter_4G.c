#include "main.h"
#include "adapter_4G.h"

void SendByte_4G(unsigned char data)
{
    // while ((USART3->SR & USART_FLAG_TXE) != USART_FLAG_TXE)
    //     ;
    // USART3->DR = data;

    while (!((USART1->SR) & (1 << 7)))
        ;              // 1<<6 也可以
    USART1->DR = data; // load data
}

void RecvByte_4G(void)
{
    unsigned char Res = 0;

    if ((USART1->SR & USART_IT_RXNE) != 0)
    {
        Res = USART1->DR;
        uart_receive_input(Res);
    }
}
