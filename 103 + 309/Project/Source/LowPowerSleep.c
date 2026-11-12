#include "main.h"
#include "LowPowerSleep.h"

void LowPowerSleep_SaveCoreState(void)
{
	Can_PrepareSleep();
    /* HICCUP retains RAM; do not wear Flash on every automatic idle STOP.
     * Manual NORMAL and reset/DEEP save the exact remaining capacity. */
    if (g_stLowPowerRtcStatus.mode != HICCUP_MODE)
        (void)SOC_SaveSnapshot();
	StorageFlash_FlushStatistics();
}

void LowPowerSleep_SaveResetState(void)
{
	LowPowerSleep_SaveCoreState();
}
