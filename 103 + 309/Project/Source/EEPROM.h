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
UINT8 UpgradeParamPolicy_ApplyOnce(void);

/* Serialized configuration edit transaction. These APIs edit only the
 * EEPROM-owned candidate image; runtime parameters stay unchanged until the
 * caller applies its already-validated request after a successful commit. */
void EEPROM_ConfigEditBegin(void);
UINT8 EEPROM_ConfigEditSetAfeWord(UINT16 index, UINT16 value);
UINT8 EEPROM_ConfigEditSetProtectWord(UINT16 index, UINT16 value);
UINT8 EEPROM_ConfigEditSetCalibPair(UINT16 index, UINT16 k_value, INT16 b_value);
UINT8 EEPROM_ConfigEditSetOtherWord(UINT16 index, UINT16 value);
UINT8 EEPROM_ConfigEditCommit(void);

#endif /* EEPROM_H */
