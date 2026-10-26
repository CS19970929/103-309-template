#include "main.h"
#include "LowPowerSleep.h"

void LowPowerSleep_SaveCoreState(void)
{
	Can_PrepareSleep();
	SOC_SaveSnapshotBeforeSleep();
	StorageFlash_FlushStatistics();
}

void LowPowerSleep_SaveResetState(void)
{
	LowPowerSleep_SaveCoreState();
}
