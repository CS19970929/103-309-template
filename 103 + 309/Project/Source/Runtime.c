#include "main.h"
#include "Runtime.h"

UINT8 SeriesNum = 7;
static UINT8 s_boot_ready;

void Runtime_Boot(void)
{
	Init_RTC();

	InitDelay();
	SleepDeal_HandleBootSleepStartup();

	jtag_disableAndConfIO();

	InitNVIC();
	InitIO();
	if (!InitE2PROM()) return;
	InitAFE1();
	InitCan();

	InitData_SOC();

	InitTimer();
	InitUSART_CommonUpper();
	InitADC();
	__enable_irq();


	InitSystemMonitorData_EEPROM();
	g_u32CS_Res_AFE = ((UINT32)OtherElement.u16Sys_CS_Res_Num * 1000) / OtherElement.u16Sys_CS_Res;

	SystemRuntime_MarkBootReady();
	SystemRuntime_SetProjectVersion(1U);
	InitProID();

	EnableLowPowerDebug();
	LogRecord_RequestStartup();
	Init_IWDG();
	s_boot_ready=1U;
}

void Runtime_RunOnce(void)
{
	if (!s_boot_ready) return;
	SysTime_LatchTaskFlags();

	ADC_Task();
	DataLoad_MosTemperature();
	App_AFEGet();
	App_CommonUpper();

	rtc_sleep();

	App_Can();
	App_FlashUpdate();

	App_LogRecord();

	Feed_IWatchDog;

	// __WFI();
}
