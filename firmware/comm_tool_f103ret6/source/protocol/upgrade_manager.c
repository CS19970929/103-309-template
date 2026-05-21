#include "upgrade_manager.h"
#include "can_gateway.h"
#include "flash_store.h"

static UpgradeState s_state;
static uint8_t s_node_id;
static uint16_t s_error;

void UpgradeManager_Init(void)
{
    s_state = UPGRADE_STATE_IDLE;
    s_node_id = 0u;
    s_error = 0u;
}

uint8_t UpgradeManager_Start(uint8_t node_id)
{
    if (s_state != UPGRADE_STATE_IDLE) {
        return 0u;
    }
    if (!FlashStore_IsValid()) {
        s_error = 0x0008u;
        return 0u;
    }

    s_node_id = node_id;
    s_error = 0u;
    if (!CanGateway_RequestBootloader(s_node_id)) {
        s_error = 0x0007u;
        return 0u;
    }
    s_state = UPGRADE_STATE_WAIT_APP_BOOT;
    return 1u;
}

void UpgradeManager_Abort(void)
{
    s_state = UPGRADE_STATE_IDLE;
    s_error = 0u;
}

void UpgradeManager_Poll(void)
{
}

UpgradeState UpgradeManager_GetState(void)
{
    return s_state;
}

uint16_t UpgradeManager_GetError(void)
{
    return s_error;
}
