#include "board.h"
#include "stm32f10x.h"
#include "misc.h"

static volatile uint32_t s_board_ms;

void Board_Init(void)
{
    SystemInit();
    SystemCoreClockUpdate();
    NVIC_PriorityGroupConfig(NVIC_PriorityGroup_2);
    SysTick_Config(SystemCoreClock / 1000u);
}

void Board_KickWatchdog(void)
{
}

void Board_DelayMs(uint32_t delay_ms)
{
    uint32_t start = Board_Millis();
    while ((Board_Millis() - start) < delay_ms) {
    }
}

uint32_t Board_Millis(void)
{
    return s_board_ms;
}

void Board_SystickHook(void)
{
    s_board_ms++;
}
