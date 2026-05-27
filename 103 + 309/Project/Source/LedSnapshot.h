#ifndef LED_SNAPSHOT_H
#define LED_SNAPSHOT_H

#include "stm32f10x.h"

#define LED_SNAPSHOT_FLAG_ALARM     ((UINT8)0x01)
#define LED_SNAPSHOT_FLAG_POWER_ON  ((UINT8)0x02)

void LedSnapshot_Save(UINT8 soc, UINT8 flags, UINT8 power_on);
UINT8 LedSnapshot_Load(UINT8 *soc, UINT8 *flags, UINT8 *power_on);
void LedSnapshot_SaveRuntime(void);

#endif
