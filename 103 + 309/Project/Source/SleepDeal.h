#ifndef SLEEPDEAL_H
#define SLEEPDEAL_H

#include "Project_Types.h"

extern UINT8 RTC_ExtComCnt;

void SleepDeal_Continue(UINT8 sleep_mode);
void BootFlag_Write(UINT16 flag);
UINT16 BootFlag_Read(void);
void BootFlag_Clear(void);
UINT8 SleepDeal_IsBootFromSleepStartup(void);
UINT8 SleepDeal_IsBootFromSleepChargerWakeup(void);
void IsSleepStartUp(void);

#endif	/* SLEEPDEAL_H */
