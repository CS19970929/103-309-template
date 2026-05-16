#include "stm32f10x.h"
#include "Can_HDX.h"
#include "FactoryAging.h"
#include "LedBar.h"
#include "LowPowerSleep.h"
#include "SocEnhance.h"

void LowPowerSleep_SaveCoreState(void)
{
	Can_PrepareSleep();
	SOC_SaveSnapshotBeforeSleep();
	FactoryAging_SaveProgressBeforeSleep();
}

void LowPowerSleep_SaveResetState(void)
{
	LowPowerSleep_SaveCoreState();
	LedBar_SaveSleepSoc();
}
