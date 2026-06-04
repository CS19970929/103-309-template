#ifndef SLEEPDEAL_H
#define SLEEPDEAL_H

void SleepDeal_Continue(UINT8 sleep_mode);
void BootFlag_Write(UINT16 flag);
UINT16 BootFlag_Read(void);
void BootFlag_Clear(void);
void SleepDeal_RecordExternalComm(void);
UINT8 SleepDeal_GetExternalCommCounter(void);
UINT8 SleepDeal_IsBootFromSleepStartup(void);
UINT8 SleepDeal_IsBootFromSleepChargerWakeup(void);
void SleepDeal_HandleBootSleepStartup(void);

#endif	/* SLEEPDEAL_H */
