#include "main.h"
#include "AfeService.h"
#include "Flash64KAppTest.h"
#include "Project_AppTasks.h"
#include "Runtime.h"

UINT8 SeriesNum = 10;

const unsigned char SeriesSelect_AFE1[16][16] = {
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{0, 1, 2, 3, 4, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{0, 1, 2, 3, 4, 5, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{0, 1, 2, 3, 4, 5, 6, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{0, 1, 2, 3, 4, 5, 6, 7, 0, 0, 0, 0, 0, 0, 0, 0},
	{0, 1, 2, 3, 4, 5, 6, 7, 8, 0, 0, 0, 0, 0, 0, 0},
	{0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 0, 0, 0, 0, 0, 0},
	{0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 0, 0, 0, 0, 0},
	{0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 0, 0, 0, 0},
	{0, 1, 2, 3, 4, 5, 6, 7, 9, 9, 10, 11, 12, 0, 0, 0},
	{0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 0, 0},
	{0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 0},
	{0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15}
};

void InitVar(void);
void InitDevice(void);

int main(void)
{
	InitDevice();
	InitVar();
#if PROJECT_FEATURE_RTC_LOW_POWER
	Init_RTC();
#endif
	/* RTC_WKTimeConfig(); */

	while (1)
	{
		Runtime_RunOnce();
	}
}

void InitDevice(void)
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
#if PROJECT_FEATURE_RS485
	InitSci();
#endif
#ifdef ELOG_OUTPUT_ENABLE
	InitUSART_CommonUpper();
	elogInit();
#endif
	InitSystemWakeUp();
	StorageFlash_PrintBootCheck();
#ifdef FLASH64K_APP_QUICK_TEST_ENABLE
	StorageFlash_RunAppQuickTest();
#endif
	InitE2PROM();
#if PROJECT_FEATURE_AFE
	AfeService_Init();
#endif
#if PROJECT_FEATURE_CAN
	InitCan();
#endif
#if PROJECT_FEATURE_ANALOG_ADC
	InitADC();
#endif

#ifdef __FUNC__HEAT__
	InitHeat_Cool();
#endif
	/* Init_ChargerLoad_Det(); */
	/* InitMosRelay_DOx(); */
#if PROJECT_FEATURE_SOC
	InitData_SOC();
#endif

	InitTimer();
	log_w("init over");

#ifdef _DEBUG_
	DBGMCU_Config(DBGMCU_STOP, ENABLE);
#endif

#if defined(wdog_enable) && PROJECT_FEATURE_WATCHDOG
	Init_IWDG();
#endif

#endif
}

void InitVar(void)
{
	InitSystemMonitorData_EEPROM();
	g_u32CS_Res_AFE = ((UINT32)OtherElement.u16Sys_CS_Res_Num * 1000) / OtherElement.u16Sys_CS_Res;

	SystemStatus.bits.b1StartUpBMS = 0;
	SystemStatus.bits.b1Status_ToSleep = 1;

	SystemStatus.bits.b4Status_ProjectVer = 1;
	LogRecord_Flag.bits.Log_StartUp = 1;
}

void InitSystemWakeUp(void)
{
}

void InitSci(void)
{
	InitUSART_CommonUpper();
}

void App_Sci(void)
{
	App_CommonUpper();
}
