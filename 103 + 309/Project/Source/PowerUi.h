#ifndef POWER_UI_H
#define POWER_UI_H

#include "stm32f10x.h"

void PowerUi_ConfirmPowerOn(void);
void PowerUi_RequestShutdown(void);
void PowerUi_ApplyInitialMosForce(void);
void PowerUi_ProcessRequests(void);
UINT8 PowerUi_IsPowerOnConfirmed(void);

#endif
