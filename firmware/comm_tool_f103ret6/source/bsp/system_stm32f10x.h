#ifndef SYSTEM_STM32F10X_H
#define SYSTEM_STM32F10X_H

#include <stdint.h>

extern uint32_t SystemCoreClock;

void SystemInit(void);
void SystemCoreClockUpdate(void);

#endif
