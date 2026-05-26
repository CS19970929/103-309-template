#ifndef FACTORY_AGING_H
#define FACTORY_AGING_H

#include "stm32f10x.h"

#define FACTORY_AGING_PUBLIC_STATE_STOPPED ((UINT8)0U)
#define FACTORY_AGING_PUBLIC_STATE_RUNNING ((UINT8)1U)
#define FACTORY_AGING_PUBLIC_STATE_DONE    ((UINT8)2U)

void FactoryAging_Task(void);
UINT8 FactoryAging_IsActive(void);
UINT8 FactoryAging_GetState(void);
UINT32 FactoryAging_GetRemainingSeconds(void);
UINT8 FactoryAging_ShouldStartOnBoot(void);
UINT8 FactoryAging_SaveProgressBeforeSleep(void);
UINT8 FactoryAging_StartByHost(void);
UINT8 FactoryAging_StopByHost(void);
UINT8 FactoryAging_ResetTimeByHost(void);

#endif
