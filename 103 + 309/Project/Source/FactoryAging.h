#ifndef FACTORY_AGING_H
#define FACTORY_AGING_H

#include "stm32f10x.h"

typedef enum {
    FACTORY_AGING_STATE_UNINIT  = 0xFF,
    FACTORY_AGING_STATE_STOPPED = 3,
    FACTORY_AGING_STATE_RUNNING = 1,
    FACTORY_AGING_STATE_DONE    = 2
} FactoryAgingState;

#define FACTORY_AGING_PUBLIC_STATE_STOPPED ((UINT8)FACTORY_AGING_STATE_STOPPED)
#define FACTORY_AGING_PUBLIC_STATE_RUNNING ((UINT8)FACTORY_AGING_STATE_RUNNING)
#define FACTORY_AGING_PUBLIC_STATE_DONE    ((UINT8)FACTORY_AGING_STATE_DONE)
#define FACTORY_AGING_DURATION_HOURS_MIN   ((UINT16)1U)
#define FACTORY_AGING_DURATION_HOURS_MAX   ((UINT16)168U)

void FactoryAging_Task(void);
UINT8 FactoryAging_IsActive(void);
UINT8 FactoryAging_GetState(void);
UINT32 FactoryAging_GetRemainingSeconds(void);
UINT8 FactoryAging_ShouldStartOnBoot(void);
UINT8 FactoryAging_SaveProgressBeforeSleep(void);
UINT8 FactoryAging_StartByHost(void);
UINT8 FactoryAging_StopByHost(void);
UINT8 FactoryAging_ResetTimeByHost(void);
UINT8 FactoryAging_SetDurationHoursByHost(UINT16 hours);
void FactoryAging_ApplySleepTime(UINT32 seconds);
void FactoryAging_SaveProgressQuick(void);

#endif
