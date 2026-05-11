#include "board_can.h"
#include "board_uart.h"
#include "upg_core.h"

#include "stm32f10x.h"

static volatile uint32_t s_tick_ms;
static UpgCore s_core;

uint32_t Board_GetTickMs(void)
{
    return s_tick_ms;
}

void SysTick_Handler(void)
{
    s_tick_ms++;
}

static int BoardHal_CanTx(void *user, const UpgCanFrame *frame)
{
    (void)user;
    return BoardCan_Write(frame, Board_GetTickMs());
}

static int BoardHal_SerialTx(void *user, const uint8_t *data, uint16_t len)
{
    (void)user;
    return BoardUart_Write(data, len, Board_GetTickMs());
}

static void BoardHal_Reset(void *user)
{
    uint32_t reset_at;

    (void)user;
    reset_at = Board_GetTickMs() + 20U;
    while ((int32_t)(Board_GetTickMs() - reset_at) < 0)
    {
    }
    NVIC_SystemReset();
}

int main(void)
{
    UpgHal hal;
    uint8_t serial_buf[64];
    uint16_t serial_len;
    UpgCanFrame can_frame;

    SystemInit();
    SystemCoreClockUpdate();
    (void)SysTick_Config(SystemCoreClock / 1000U);

    BoardUart_Init();
    BoardCan_Init();

    hal.can_tx = BoardHal_CanTx;
    hal.serial_tx = BoardHal_SerialTx;
    hal.reset = BoardHal_Reset;
    hal.user = 0;
    UpgCore_Init(&s_core, &hal);

    for (;;)
    {
        UpgCore_Tick(&s_core, Board_GetTickMs());

        serial_len = BoardUart_Read(serial_buf, sizeof(serial_buf));
        if (serial_len > 0U)
        {
            UpgCore_OnSerialBytes(&s_core, serial_buf, serial_len);
        }

        while (BoardCan_Read(&can_frame) != 0U)
        {
            UpgCore_OnCanFrame(&s_core, &can_frame);
        }
    }
}
