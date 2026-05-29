#include "main.h"
#include "FactoryAging.h"
#include "LowPowerSleep.h"

void LowPowerSleep_SaveCoreState(void)
{
	Can_PrepareSleep();
	FactoryAging_SaveProgressBeforeSleep();
}

void LowPowerSleep_SaveResetState(void)
{
	LowPowerSleep_SaveCoreState();
	LedBar_SaveSleepSoc();
}
