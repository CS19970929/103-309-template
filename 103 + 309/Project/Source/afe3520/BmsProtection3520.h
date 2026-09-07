#ifndef BMS_PROTECTION_3520_H
#define BMS_PROTECTION_3520_H

#include "afe3520/Afe3520.h"
#include "BmsParameters.h"
#include "afe3520/Afe3520Config.h"

#define BMS3520_PROTECTION_PERIOD_MS        200U
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
    uint8_t mosFeedbackValid;
} BMS3520_PROTECTION_STATUS;

typedef enum {
    BMS3520_CONFIG_INVALID = 0, /* rejected, no changes */
    BMS3520_CONFIG_PENDING,     /* accepted, bus/apply failed; MOS inhibited */
    BMS3520_CONFIG_VERIFIED     /* register readback passed; next service arbitrates */
} BMS3520_CONFIG_RESULT;
#define BMS3520_HW_REGISTER_BASE 0x2500U
#define BMS3520_HW_STORAGE_WORDS 24U
#define BMS3520_HW_READ_WORDS 32U
#define BMS3520_HW_SCHEMA_MAGIC 0x3520U
#define BMS3520_HW_SCHEMA_VERSION 1U
void Bms3520_EncodeHardware(const BMS3520_HARDWARE_CONFIG *cfg, uint16_t words[24]);
uint8_t Bms3520_DecodeHardware(const uint16_t words[24], BMS3520_HARDWARE_CONFIG *cfg);
uint8_t Bms3520_RestoreHardware(const uint16_t words[24]);
const BMS3520_HARDWARE_CONFIG *Bms3520_GetHardwareConfig(void);
uint8_t Bms3520_ValidateHardwareConfig(const BMS3520_HARDWARE_CONFIG *cfg);
BMS3520_CONFIG_RESULT Bms3520_SetHardwareConfig(const BMS3520_HARDWARE_CONFIG *cfg);
/* Mandatory for ALL three modes; do not remove this service to disable protection. */
void Bms3520_ProtectionInit(void);
void Bms3520_HandleCommFault(void);
void Bms3520_Service200ms(void);
void Bms3520_RequestMos(GPIO_Type type, uint8_t on);
uint8_t Bms3520_BuildAfeConfig(AFE3520_REG_CONFIG *cfg);
uint8_t Bms3520_ApplyAndVerifyAfeConfig(void);
const BMS3520_PROTECTION_STATUS *Bms3520_GetProtectionStatus(void);
uint32_t Bms3520_GetBlockMask(void);
void Bms3520_SetSystemBlock(uint8_t blocked);
void Bms3520_RestartSoftwareTimers(void);

#endif /* BMS_PROTECTION_3520_H */
