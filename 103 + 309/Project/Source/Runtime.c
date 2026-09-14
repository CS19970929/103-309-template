#include "main.h"
#include "Runtime.h"
#include "SerialProtocolMux.h"

UINT8 SeriesNum = PROJECT_FIXED_SERIES_NUM;

static void Runtime_ForceFixedSeries(void)
{
	SeriesNum = PROJECT_FIXED_SERIES_NUM;
	OtherElement.u16Sys_SeriesNum = PROJECT_FIXED_SERIES_NUM;
}

void Runtime_Boot(void)
{
	Init_RTC();

	InitDelay();
	SleepDeal_HandleBootSleepStartup();

	jtag_disableAndConfIO();

	InitNVIC();
	InitIO();
	InitE2PROM();

	/* C073 产品固定 7S，不接受历史 Flash 中其它串数。 */
	Runtime_ForceFixedSeries();
	InitAFE1();
	InitCan();
	InitADC();
	InitData_SOC();
	InitProID();

	/* USART1 / USART2 同时支持原 Modbus 与 LX V0.9。 */
	SerialProtocolMux_Init();

	InitTimer();
	__enable_irq();

	InitSystemMonitorData_EEPROM();
	g_u32CS_Res_AFE = ((UINT32)OtherElement.u16Sys_CS_Res_Num * 1000) / OtherElement.u16Sys_CS_Res;

	SystemRuntime_MarkBootReady();
	SystemRuntime_SetProjectVersion(1U);

	EnableLowPowerDebug();
	LogRecord_RequestStartup();
	Init_IWDG();
}

void Runtime_RunOnce(void)
{
	/* 双保险：任何旧参数或通信写入都不能把本产品改成非 7S。 */
	Runtime_ForceFixedSeries();
	SysTime_LatchTaskFlags();

	App_AFEGet();
	SerialProtocolMux_Process();
	Runtime_ForceFixedSeries();
	App_AnlogCal();

	rtc_sleep();

	App_Can();
	App_FlashUpdate();

	App_LogRecord();

	Feed_IWatchDog;

	// __WFI();
}
