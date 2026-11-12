#ifndef LOW_POWER_SLEEP_H
#define LOW_POWER_SLEEP_H

#include <stdint.h>

/* At most two attempts; 0 reports an unsaved SOC, not permission to block DEEP. */
uint8_t LowPowerSleep_SaveCoreState(void);
uint8_t LowPowerSleep_SaveResetState(void);

#endif
