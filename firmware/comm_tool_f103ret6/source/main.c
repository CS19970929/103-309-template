#include "board.h"
#include "board_uart.h"
#include "ct_app.h"
#include "ct_protocol.h"

static CtProtocolParser s_parser;
static CtFrame s_frame;

int main(void)
{
    uint8_t byte;

    Board_Init();
    CtProtocol_Init(&s_parser);
    CtApp_Init();

    while (1)
    {
        while (BoardUart_ReadByte(&byte))
        {
            if (CtProtocol_Feed(&s_parser, byte, &s_frame) != 0u)
            {
                CtApp_HandleFrame(&s_frame);
            }
        }
        CtApp_Poll();
        Board_Poll();
    }
}
