#ifndef CT_UPGRADE_MANAGER_H
#define CT_UPGRADE_MANAGER_H

#include <stdint.h>

typedef struct
{
    uint8_t state;
    uint8_t percent;
    uint8_t last_error;
    uint8_t reserved;
    uint32_t written;
    uint32_t total;
    uint16_t expect_seq;
} CtUpgradeStatus;

void CtUpgrade_Init(void);
const CtUpgradeStatus *CtUpgrade_GetStatus(void);
int CtUpgrade_Start(uint8_t node);
void CtUpgrade_Abort(void);

#endif

