#include "main.h"
#include "Runtime.h"

UINT8 SeriesNum = 7;
static UINT8 s_boot_ready;

void Runtime_Boot(void)
{
	UINT32 reset_cause=RCC->CSR;
	UINT16 sleep_flag=BootFlag_Read();
	UINT8 attempt, storage_ready=0U;
	RCC_ClearFlag();
	/* Capture reset evidence before RTC setup can clear RCC/backup state. */
	StorageFlash_InitStatistics(reset_cause,sleep_flag);
	Init_RTC();

	InitDelay();
	SleepDeal_HandleBootSleepStartup();

	jtag_disableAndConfIO();

	InitNVIC();
	InitIO();
	/* A temporary programming failure can recover; damaged data never auto-erases. */
	for (attempt=0U; attempt<3U; ++attempt) {
		if (StorageFlash_InitParameters()) { storage_ready=1U; break; }
		__delay_ms(20U);
	}
	if (!storage_ready) {
		/* Keep the AFE uninitialized and the system outside normal MOS arbitration.
		 * Feed the watchdog in a deterministic fault loop for probe diagnostics. */
		ClearLowPowerDebug();
		Init_IWDG();
		for (;;) {
			Feed_IWatchDog;
			__delay_ms(100U);
		}
	}
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

	ClearLowPowerDebug();
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
    SystemRuntime_ObservePersistentFailures();
	if (g_stLowPowerRtcStatus.mode==DEEP_MODE) {
		rtc_sleep();
		return;
	}
	App_CommonUpper();

	rtc_sleep();

	App_Can();
	App_FlashUpdate();

	App_LogRecord();
	StorageFlash_StatisticsTask();

	Feed_IWatchDog;

}
