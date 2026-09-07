#include "main.h"
#include "LowPowerSleep.h"
#include "FlashEndurance.h"

void LowPowerSleep_SaveCoreState(void)
{
	Can_PrepareSleep();
#if FLASH_ENDURANCE_TEST_ENABLE
    if (FlashTest_Active()) (void)FlashTest_Command(FLASH_TEST_BASE,FLASH_TEST_STOP);
#endif
	SOC_SaveSnapshotBeforeSleep();
}

void LowPowerSleep_SaveResetState(void)
{
	LowPowerSleep_SaveCoreState();
}
