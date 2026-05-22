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
#ifdef ELOG_OUTPUT_ENABLE
	InitUSART_CommonUpper();
	elogInit();
#endif
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
	log_w("init over");

#ifdef _DEBUG_
	DBGMCU_Config(DBGMCU_STOP, ENABLE);
#endif

#ifdef wdog_enable
	Init_IWDG();
#endif

#endif
}

static void AppInit_InitRuntimeState(void)
{
	InitSystemMonitorData_EEPROM();
	g_u32CS_Res_AFE = ((UINT32)OtherElement.u16Sys_CS_Res_Num * 1000) / OtherElement.u16Sys_CS_Res;

	SystemStatus.bits.b1StartUpBMS = 0;
	SystemStatus.bits.b1Status_ToSleep = 1;

	SystemStatus.bits.b4Status_ProjectVer = 1;
	LogRecord_Flag.bits.Log_StartUp = 1;
}

void AppInit_Boot(void)
{
	AppInit_InitDevice();
	AppInit_InitRuntimeState();
	Init_RTC();
	/* RTC_WKTimeConfig(); */
}
