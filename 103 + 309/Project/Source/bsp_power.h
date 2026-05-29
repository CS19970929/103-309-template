#ifndef BSP_POWER_H
#define BSP_POWER_H

#include <stdint.h>

void BSP_Power_PrepareStop(uint32_t rtc_cycle_count);
void BSP_Power_EnterStop(void);
void BSP_Power_DisableStopWakeup(void);
void BSP_Power_RestoreAfterStop(void);

#endif
