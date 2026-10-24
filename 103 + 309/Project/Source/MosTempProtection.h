#ifndef MOS_TEMP_PROTECTION_H
#define MOS_TEMP_PROTECTION_H

/* MOS NTC table and report encoding: (temperature_C + 40) * 10. */
#define MOS_TEMP_INVALID_CODE 110U /* -29C: open/invalid NTC sentinel */
#define MOS_TEMP_MAX_CODE     1450U /* +105C: ADC lookup upper bound */

/* Safety veto only. Does not write hardware or clear fault latches. */
UINT8 MosTempProtection_IsBlocked(void);

#endif /* MOS_TEMP_PROTECTION_H */
