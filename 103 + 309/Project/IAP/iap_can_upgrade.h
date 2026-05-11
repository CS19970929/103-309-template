#ifndef IAP_CAN_UPGRADE_H
#define IAP_CAN_UPGRADE_H

#include <stdint.h>

void IapCan_Init(void);
void IapCan_Task(void);
uint8_t IapCan_IsUpgradeActive(void);

#endif
