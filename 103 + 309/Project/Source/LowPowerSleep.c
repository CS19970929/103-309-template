#include "main.h"
#include "LowPowerSleep.h"

#define LOW_POWER_SOC_SAVE_ATTEMPTS 2U

static UINT8 LowPowerSleep_SaveState(UINT8 save_soc)
{
    UINT8 attempt, saved=1U;
    Can_PrepareSleep();
    if (save_soc) {
        saved=0U;
        /* One immediate retry; never wait indefinitely before protective sleep. */
        for (attempt=0U; attempt<LOW_POWER_SOC_SAVE_ATTEMPTS; ++attempt) {
            Feed_IWatchDog;
            if (SOC_SaveSnapshot()) { saved=1U; break; }
        }
        if (!saved) System_ERROR_UserCallback(ERROR_EEPROM_STORE);
    }
    StorageFlash_FlushStatistics();
    return saved;
}

UINT8 LowPowerSleep_SaveCoreState(void)
{
    /* HICCUP STOP retains RAM; NORMAL saves the exact remaining capacity. */
    return LowPowerSleep_SaveState((UINT8)(g_stLowPowerRtcStatus.mode!=HICCUP_MODE));
}

UINT8 LowPowerSleep_SaveResetState(void)
{
    /* A reset always loses RAM, even if its caller still has a STOP mode. */
    return LowPowerSleep_SaveState(1U);
}
