#include "main.h"
#include "AppInit.h"
#include "Flash64KAppTest.h"

UINT8 SeriesNum = 10;

static void AppInit_InitDevice(void)
{
	SystemInit();

#if (defined _DEBUG_CODE)
	InitDelay();
	InitIO();
#else
	InitDelay();
	IsSleepStartUp();

	jtag_disableAndConfIO();

	InitNVIC();
	InitIO();
	AppInit_InitSci();
#ifdef FLASH_BOOT_PRINT_ENABLE
	StorageFlash_PrintBootCheck();
#endif
#ifdef FLASH64K_APP_QUICK_TEST_ENABLE
	StorageFlash_RunAppQuickTest();
#endif
	InitE2PROM();
	InitAFE1();
	InitCan();
	InitADC();

	InitData_SOC();

	InitTimer();
	__enable_irq();

#ifdef ELOG_OUTPUT_ENABLE
	elogInit();
	log_w("debug serial log enabled profile=%u", (unsigned int)PROJECT_CFG_BUILD_PROFILE);
#endif
	log_w("init over");

	EnableLowPowerDebug();

#ifdef wdog_enable
	Init_IWDG();
#endif

#endif
}

static void AppInit_InitRuntimeState(void)
{
	InitSystemMonitorData_EEPROM();
	g_u32CS_Res_AFE = ((UINT32)OtherElement.u16Sys_CS_Res_Num * 1000) / OtherElement.u16Sys_CS_Res;

	SystemRuntime_MarkBootReady();
	SystemRuntime_SetProjectVersion(1U);
	LogRecord_RequestStartup();
}

void AppInit_Boot(void)
{
	AppInit_InitDevice();
	AppInit_InitRuntimeState();
	Init_RTC();
	LP_Init();
	/* RTC_WKTimeConfig(); */
}
