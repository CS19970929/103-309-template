#ifndef BOARD_H
#define BOARD_H

#include <stdint.h>

void Board_Init(void);
void Board_KickWatchdog(void);
void Board_DelayMs(uint32_t delay_ms);
uint32_t Board_Millis(void);
void Board_SystickHook(void);

#endif
