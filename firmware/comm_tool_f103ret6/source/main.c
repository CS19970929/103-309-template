#include "comm_tool_config.h"
#include "board.h"
#include "board_can.h"
#include "board_uart.h"
#include "flash_store.h"
#include "serial_proto.h"
#include "can_gateway.h"
#include "upgrade_manager.h"

int main(void)
{
    Board_Init();
    BoardUart_Init(COMM_TOOL_UART_BAUDRATE);
    BoardCan_Init(COMM_TOOL_CAN_BITRATE);
    FlashStore_Init();
    SerialProto_Init();
    CanGateway_Init();
    UpgradeManager_Init();

    for (;;) {
        SerialProto_Poll();
        CanGateway_Poll();
        UpgradeManager_Poll();
        Board_KickWatchdog();
    }
}
