#ifndef EEPROM_H
#define EEPROM_H

/* Counts shared by parameter serialization and the host register windows. */
#define E2P_PARA_NUM_PROTECT 65
#define E2P_PARA_NUM_RTC 12
#define E2P_PARA_NUM_OTHER_ELEMENT1 32
#define E2P_PARA_NUM_RESERVED_RW_PARAM 24

UINT8 InitE2PROM(void);
UINT8 EEPROM_SaveRWParametersToFlash(UINT8 cycle_count_written);

#endif /* EEPROM_H */
