#ifndef FACTORY_AGING_H
#define FACTORY_AGING_H

#include "stm32f10x.h"

void FactoryAging_Task(void);
UINT8 FactoryAging_IsActive(void);
UINT8 FactoryAging_SaveProgressBeforeSleep(void);

#endif
