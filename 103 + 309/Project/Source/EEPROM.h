#ifndef EEPROM_H
#define EEPROM_H

/* Persistent configuration is stored in internal Flash as one BMS_CONFIG.
 * The E2P_PARA_NUM_* constants remain protocol/runtime sizing constants; they
 * no longer describe separate EEPROM/Flash address regions. */
#define E2P_PARA_NUM_PROTECT                 65
#define E2P_PARA_NUM_RTC                     12
#define E2P_PARA_NUM_CALIB_K                 KB_NUM
#define E2P_PARA_NUM_CALIB_B                 KB_NUM
#define E2P_PARA_NUM_SOC_TABLE               SOC_TABLE_SIZE
#define E2P_PARA_NUM_COPPERLOSS              CompensateNUM
#define E2P_PARA_NUM_COPPERLOSS_NUM          CompensateNUM
#define E2P_PARA_NUM_FAULT_RECORD             (3*Record_len + 3 + Record_len*6)
#define E2P_PARA_NUM_OTHER_ELEMENT1           32
#define E2P_PARA_NUM_RESERVED_RW_PARAM        24

void InitE2PROM(void);
UINT8 EEPROM_SaveConfigToFlash(void);
/* Compatibility entry point; saves the same complete BMS_CONFIG. */
UINT8 EEPROM_SaveRWParametersToFlash(void);
UINT8 UpgradeParamPolicy_ApplyOnce(void);

#endif /* EEPROM_H */
