#ifndef UPGRADE_MANAGER_H
#define UPGRADE_MANAGER_H

#include <stdint.h>

typedef enum {
    UPGRADE_STATE_IDLE = 0,
    UPGRADE_STATE_WAIT_APP_BOOT,
    UPGRADE_STATE_IAP_HANDSHAKE,
    UPGRADE_STATE_TRANSFER,
    UPGRADE_STATE_VERIFY,
    UPGRADE_STATE_DONE,
    UPGRADE_STATE_FAILED
} UpgradeState;

void UpgradeManager_Init(void);
uint8_t UpgradeManager_Start(uint8_t node_id);
void UpgradeManager_Abort(void);
void UpgradeManager_Poll(void);
UpgradeState UpgradeManager_GetState(void);
uint16_t UpgradeManager_GetError(void);

#endif
