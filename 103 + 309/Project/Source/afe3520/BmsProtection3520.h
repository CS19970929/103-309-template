#ifndef BMS_PROTECTION_3520_H
#define BMS_PROTECTION_3520_H

#include "afe3520/Afe3520.h"
#include "SH367309_DataDeal.h"

#define BMS3520_PROTECTION_PERIOD_MS        200U
#define BMS3520_REVERSE_CURRENT_A10         10U
#define BMS3520_HW_RECOVERY_STABLE_TICKS    25U
#define BMS3520_SW_RECOVERY_STABLE_TICKS    5U

typedef struct
{
    uint32_t chargeBlocks;
    uint32_t dischargeBlocks;
    uint32_t globalBlocks;
    uint32_t activeAll;
    uint32_t latchedHardware;
    uint16_t recoveryCounter;
    uint8_t requestedCharge;
    uint8_t requestedDischarge;
    uint8_t actualCharge;
    uint8_t actualDischarge;
    uint8_t configValid;
} BMS3520_PROTECTION_STATUS;

void Bms3520_ProtectionInit(void);
void Bms3520_ProtectionService(void);
void Bms3520_RequestMos(GPIO_Type type, uint8_t on);
uint8_t Bms3520_BuildAfeConfig(AFE3520_REG_CONFIG *cfg);
uint8_t Bms3520_ApplyAndVerifyAfeConfig(void);
const BMS3520_PROTECTION_STATUS *Bms3520_GetProtectionStatus(void);
uint32_t Bms3520_GetBlockMask(void);
void Bms3520_SetSystemBlock(uint8_t blocked);

#endif /* BMS_PROTECTION_3520_H */
