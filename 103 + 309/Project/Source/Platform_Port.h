#ifndef PLATFORM_PORT_H
#define PLATFORM_PORT_H

#include "Project_Types.h"
#include "stm32f10x.h"
#include "System_Init.h"

#ifndef PROJECT_PLATFORM_STM32F1_SPL
#define PROJECT_PLATFORM_STM32F1_SPL 1
#endif

BMS_INLINE void Platform_ResetMcu(void)
{
	NVIC_SystemReset();
}

BMS_INLINE void Platform_FeedWatchdog(void)
{
	Feed_IWatchDog;
}

BMS_INLINE UINT32 Platform_Get10msTick(void)
{
	return SysTime_Get10msTickCount();
}

BMS_INLINE void Platform_LatchTaskFlags(void)
{
	SysTime_LatchTaskFlags();
}

#endif
