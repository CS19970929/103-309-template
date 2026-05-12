#include "main.h"
#include "Flash64KAppTest.h"

UINT8 SeriesNum = 10;

const unsigned char SeriesSelect_AFE1[16][16] = {
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},	   // 1´®
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},	   // 2´®
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},	   // 3
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},	   // 4
	{0, 1, 2, 3, 4, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},	   // 5
	{0, 1, 2, 3, 4, 5, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},	   // 6
	{0, 1, 2, 3, 4, 5, 6, 0, 0, 0, 0, 0, 0, 0, 0, 0},	   // 7
	{0, 1, 2, 3, 4, 5, 6, 7, 0, 0, 0, 0, 0, 0, 0, 0},	   // 8
	{0, 1, 2, 3, 4, 5, 6, 7, 8, 0, 0, 0, 0, 0, 0, 0},	   // 9
	{0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 0, 0, 0, 0, 0, 0},	   // 10
	{0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 0, 0, 0, 0, 0},	   // 11
	{0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 0, 0, 0, 0},	   // 12
	{0, 1, 2, 3, 4, 5, 6, 7, 9, 9, 10, 11, 12, 0, 0, 0},   // 13
	{0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 0, 0},  // 14
	{0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 0}, // 15
	{0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15} // 16
};

void InitVar(void);
void InitDevice(void);
void InitSci(void);
void App_Sci(void);
void InitSystemWakeUp(void);
static UINT8 MainLoop_HasPendingWork(void);
static void MainLoop_EnterIdleSleep(void);
static void FactoryAging_Task(void);
static void Runtime_RunDebugOnce(void);
static void Runtime_RunOnce(void);

#define FACTORY_AGING_STATE_UNINIT  ((UINT8)0U)
#define FACTORY_AGING_STATE_RUNNING ((UINT8)1U)
#define FACTORY_AGING_STATE_DONE    ((UINT8)2U)
#define FACTORY_AGING_10MS_PER_SEC  ((UINT32)100U)
#define FACTORY_AGING_FLASH_SAVE_INTERVAL_SECONDS ((UINT32)7200U)
#define FACTORY_AGING_FLASH_SAVE_INTERVAL_10MS \
	(FACTORY_AGING_FLASH_SAVE_INTERVAL_SECONDS * FACTORY_AGING_10MS_PER_SEC)
#define FACTORY_AGING_BKP_SAVE_INTERVAL_10MS FACTORY_AGING_10MS_PER_SEC
#define FACTORY_AGING_FINISH_RETRY_10MS FACTORY_AGING_10MS_PER_SEC
#define FACTORY_AGING_DURATION_10MS \
	((UINT32)PROJECT_CFG_FACTORY_AGING_DURATION_SECONDS * FACTORY_AGING_10MS_PER_SEC)
#define FACTORY_AGING_BKP_MAGIC      ((UINT16)0xA91E)
#define FACTORY_AGING_BKP_MAGIC_REG  BKP_DR6
#define FACTORY_AGING_BKP_INV_REG    BKP_DR7
#define FACTORY_AGING_BKP_LO_REG     BKP_DR8
#define FACTORY_AGING_BKP_HI_REG     BKP_DR9
#define FACTORY_AGING_BKP_CRC_REG    BKP_DR10

static UINT8 s_u8FactoryAgingState = FACTORY_AGING_STATE_UNINIT;
static UINT32 s_u32FactoryAgingElapsed10ms = 0U;
static UINT32 s_u32FactoryAgingLastTick = 0U;
static UINT32 s_u32FactoryAgingLastBkpSave10ms = 0U;
static UINT32 s_u32FactoryAgingLastFlashSave10ms = 0U;
static UINT32 s_u32FactoryAgingNextFinishRetry10ms = 0U;
static UINT8 s_u8FactoryAgingBkpSaveValid = 0U;
static UINT8 s_u8FactoryAgingFlashSaveValid = 0U;

static UINT8 MainLoop_HasPendingWork(void)
{
	if (SysTime_HasPendingTaskFlags() != 0U)
	{
		return 1U;
	}

	if (Sci_IsAnyPortBusy() != 0U)
	{
		return 1U;
	}

	return 0U;
}

static void MainLoop_EnterIdleSleep(void)
{
	if (!System_OnOFF_Func.bits.b1OnOFF_Sleep)
	{
		return;
	}

	if (MainLoop_HasPendingWork())
	{
		return;
	}

	if (__get_PRIMASK() != 0U)
	{
		return;
	}

	/* 空闲等待只允许进入 Sleep，避免残留的 SLEEPDEEP 影响主循环。 */
	SCB->SCR &= (uint32_t)(~SCB_SCR_SLEEPDEEP_Msk);
	__DSB();
	__WFI();
	__ISB();
}

void open_chg_close_dsg(void)
{
	SH367309_Reg_Store.REG_MTP_CONF.bits.CADCON = 1; // å¯?¿½éšç–ŒADC
	SH367309_Reg_Store.REG_MTP_CONF.bits.CHGMOS = 1; // éå‘¯æ•¸MOSé¢ç›‡FEçº?ƒ¿æ¬¢éŽºÑƒåŸ?
	SH367309_Reg_Store.REG_MTP_CONF.bits.DSGMOS = 0; // éå‘¯æ•¸MOSé¢ç›‡FEçº?ƒ¿æ¬¢éŽºÑƒåŸ?
	MTPWrite(MTP_CONF, 1, &SH367309_Reg_Store.REG_MTP_CONF.all);
	GPIO_WriteBit(GPIO_MCC_C, PIN_MCC_C, Bit_SET);
}
void open_dsg_close_chg(void)
{
	SH367309_Reg_Store.REG_MTP_CONF.bits.CADCON = 1; // å¯?¿½éšç–ŒADC
	SH367309_Reg_Store.REG_MTP_CONF.bits.CHGMOS = 0; // éå‘¯æ•¸MOSé¢ç›‡FEçº?ƒ¿æ¬¢éŽºÑƒåŸ?
	SH367309_Reg_Store.REG_MTP_CONF.bits.DSGMOS = 1; // éå‘¯æ•¸MOSé¢ç›‡FEçº?ƒ¿æ¬¢éŽºÑƒåŸ?
	MTPWrite(MTP_CONF, 1, &SH367309_Reg_Store.REG_MTP_CONF.all);
	GPIO_WriteBit(GPIO_MCC_C, PIN_MCC_C, Bit_RESET);
}
void enter_fac_mode(bool on)
{
#if 1
	if (on)
	{
		SH367309_Reg_Store.REG_MTP_CONF.bits.CADCON = 1; // å¯?¿½éšç–ŒADC
		SH367309_Reg_Store.REG_MTP_CONF.bits.CHGMOS = 1; // éå‘¯æ•¸MOSé¢ç›‡FEçº?ƒ¿æ¬¢éŽºÑƒåŸ?
		SH367309_Reg_Store.REG_MTP_CONF.bits.DSGMOS = 1; // éå‘¯æ•¸MOSé¢ç›‡FEçº?ƒ¿æ¬¢éŽºÑƒåŸ?
		MTPWrite(MTP_CONF, 1, &SH367309_Reg_Store.REG_MTP_CONF.all);
		GPIO_WriteBit(GPIO_MCC_C, PIN_MCC_C, Bit_SET);
	}
	else
	{
		open_dsg_close_chg();
	}
#endif
}

static UINT32 FactoryAging_ClampElapsed(UINT32 elapsed10ms)
{
	return (elapsed10ms > FACTORY_AGING_DURATION_10MS) ?
		FACTORY_AGING_DURATION_10MS : elapsed10ms;
}

static UINT16 FactoryAging_BkpCrc(UINT32 elapsed10ms)
{
	UINT16 lo = (UINT16)(elapsed10ms & 0xFFFFU);
	UINT16 hi = (UINT16)((elapsed10ms >> 16) & 0xFFFFU);

	return (UINT16)(lo ^ hi ^ FACTORY_AGING_BKP_MAGIC ^ 0x5A5AU);
}

static void FactoryAging_EnableBkpAccess(void)
{
	RCC_APB1PeriphClockCmd(RCC_APB1Periph_PWR | RCC_APB1Periph_BKP, ENABLE);
	PWR_BackupAccessCmd(ENABLE);
}

static void FactoryAging_SaveBkp(UINT32 elapsed10ms)
{
	elapsed10ms = FactoryAging_ClampElapsed(elapsed10ms);
	FactoryAging_EnableBkpAccess();
	BKP_WriteBackupRegister(FACTORY_AGING_BKP_MAGIC_REG, FACTORY_AGING_BKP_MAGIC);
	BKP_WriteBackupRegister(FACTORY_AGING_BKP_INV_REG, (UINT16)(~FACTORY_AGING_BKP_MAGIC));
	BKP_WriteBackupRegister(FACTORY_AGING_BKP_LO_REG, (UINT16)(elapsed10ms & 0xFFFFU));
	BKP_WriteBackupRegister(FACTORY_AGING_BKP_HI_REG, (UINT16)((elapsed10ms >> 16) & 0xFFFFU));
	BKP_WriteBackupRegister(FACTORY_AGING_BKP_CRC_REG, FactoryAging_BkpCrc(elapsed10ms));
	s_u32FactoryAgingLastBkpSave10ms = elapsed10ms;
	s_u8FactoryAgingBkpSaveValid = 1U;
}

static UINT8 FactoryAging_LoadBkp(UINT32 *elapsed10ms)
{
	UINT16 magic;
	UINT16 inverse_magic;
	UINT16 lo;
	UINT16 hi;
	UINT32 elapsed;

	if (elapsed10ms == 0)
	{
		return 0U;
	}

	FactoryAging_EnableBkpAccess();
	magic = BKP_ReadBackupRegister(FACTORY_AGING_BKP_MAGIC_REG);
	inverse_magic = BKP_ReadBackupRegister(FACTORY_AGING_BKP_INV_REG);
	if ((magic != FACTORY_AGING_BKP_MAGIC) ||
		((UINT16)(magic ^ inverse_magic) != 0xFFFFU))
	{
		return 0U;
	}

	lo = BKP_ReadBackupRegister(FACTORY_AGING_BKP_LO_REG);
	hi = BKP_ReadBackupRegister(FACTORY_AGING_BKP_HI_REG);
	elapsed = ((UINT32)hi << 16) | lo;
	if (BKP_ReadBackupRegister(FACTORY_AGING_BKP_CRC_REG) != FactoryAging_BkpCrc(elapsed))
	{
		return 0U;
	}

	*elapsed10ms = FactoryAging_ClampElapsed(elapsed);
	return 1U;
}

static UINT8 FactoryAging_LoadStoredProgress(UINT32 *elapsed10ms, UINT8 *done)
{
	STORAGE_FLASH_FACTORY_AGING_DATA data;
	UINT32 flash_elapsed = 0U;
	UINT32 bkp_elapsed = 0U;
	UINT8 has_progress = 0U;

	if ((elapsed10ms == 0) || (done == 0))
	{
		return 0U;
	}

	*elapsed10ms = 0U;
	*done = 0U;
	if (StorageFlash_LoadFactoryAgingData(&data) != 0U)
	{
		if (data.u16State == FLASH_FACTORY_AGING_STATE_DONE)
		{
			*elapsed10ms = FACTORY_AGING_DURATION_10MS;
			*done = 1U;
			s_u32FactoryAgingLastFlashSave10ms = FACTORY_AGING_DURATION_10MS;
			s_u8FactoryAgingFlashSaveValid = 1U;
			return 1U;
		}
		if (data.u16State == FLASH_FACTORY_AGING_STATE_RUNNING)
		{
			flash_elapsed = FactoryAging_ClampElapsed(data.u32Elapsed10ms);
			s_u32FactoryAgingLastFlashSave10ms = flash_elapsed;
			s_u8FactoryAgingFlashSaveValid = 1U;
			has_progress = 1U;
		}
	}

	if (FactoryAging_LoadBkp(&bkp_elapsed) != 0U)
	{
		if (bkp_elapsed > flash_elapsed)
		{
			flash_elapsed = bkp_elapsed;
		}
		has_progress = 1U;
	}

	*elapsed10ms = FactoryAging_ClampElapsed(flash_elapsed);
	return has_progress;
}

static UINT8 FactoryAging_IsDoneStored(void)
{
	STORAGE_FLASH_FACTORY_AGING_DATA data;

	return ((StorageFlash_LoadFactoryAgingData(&data) != 0U) &&
			(data.u16State == FLASH_FACTORY_AGING_STATE_DONE)) ? 1U : 0U;
}

static UINT8 FactoryAging_SaveStoredProgress(UINT16 state, UINT8 force_flash, UINT8 force_bkp)
{
	STORAGE_FLASH_FACTORY_AGING_DATA data;
	UINT8 save_flash = force_flash;

	if ((force_bkp != 0U) ||
		(s_u8FactoryAgingBkpSaveValid == 0U) ||
		((s_u32FactoryAgingElapsed10ms - s_u32FactoryAgingLastBkpSave10ms) >= FACTORY_AGING_BKP_SAVE_INTERVAL_10MS))
	{
		FactoryAging_SaveBkp(s_u32FactoryAgingElapsed10ms);
	}

	if (save_flash == 0U)
	{
		if (s_u8FactoryAgingFlashSaveValid == 0U)
		{
			save_flash = 1U;
		}
		else if ((s_u32FactoryAgingElapsed10ms - s_u32FactoryAgingLastFlashSave10ms) >=
				 FACTORY_AGING_FLASH_SAVE_INTERVAL_10MS)
		{
			save_flash = 1U;
		}
	}

	if (save_flash == 0U)
	{
		return 1U;
	}

	memset(&data, 0, sizeof(data));
	data.u32Elapsed10ms = FactoryAging_ClampElapsed(s_u32FactoryAgingElapsed10ms);
	data.u16State = state;
	data.u16Reserved = 0xFFFFU;
	if (StorageFlash_SaveFactoryAgingData(&data) == 0U)
	{
		return 0U;
	}

	s_u32FactoryAgingLastFlashSave10ms = data.u32Elapsed10ms;
	s_u8FactoryAgingFlashSaveValid = 1U;
	return 1U;
}

static UINT8 FactoryAging_MarkDone(void)
{
	if (FactoryAging_IsDoneStored() != 0U)
	{
		return 1U;
	}

	s_u32FactoryAgingElapsed10ms = FACTORY_AGING_DURATION_10MS;
	return FactoryAging_SaveStoredProgress(FLASH_FACTORY_AGING_STATE_DONE, 1U, 1U);
}

static UINT8 FactoryAging_Finish(void)
{
	enter_fac_mode(false);
	if (FactoryAging_MarkDone() == 0U)
	{
		System_ERROR_UserCallback(ERROR_EEPROM_STORE);
		return 0U;
	}

	s_u8FactoryAgingState = FACTORY_AGING_STATE_DONE;
	s_u32FactoryAgingNextFinishRetry10ms = 0U;
	return 1U;
}

static void FactoryAging_Start(UINT32 now_tick)
{
	UINT32 stored_elapsed = 0U;
	UINT8 done = 0U;

	(void)FactoryAging_LoadStoredProgress(&stored_elapsed, &done);
	s_u32FactoryAgingElapsed10ms = FactoryAging_ClampElapsed(stored_elapsed);
	s_u32FactoryAgingLastTick = now_tick;
	s_u32FactoryAgingNextFinishRetry10ms = 0U;

	if (done != 0U)
	{
		enter_fac_mode(false);
		s_u8FactoryAgingState = FACTORY_AGING_STATE_DONE;
		return;
	}

	s_u8FactoryAgingState = FACTORY_AGING_STATE_RUNNING;
	if (s_u32FactoryAgingElapsed10ms >= FACTORY_AGING_DURATION_10MS)
	{
		(void)FactoryAging_Finish();
		return;
	}

	enter_fac_mode(true);
	(void)FactoryAging_SaveStoredProgress(FLASH_FACTORY_AGING_STATE_RUNNING, 0U, 1U);
}

static void FactoryAging_AddRunningTicks(UINT32 now_tick)
{
	UINT32 delta;

	if (now_tick >= s_u32FactoryAgingLastTick)
	{
		delta = now_tick - s_u32FactoryAgingLastTick;
	}
	else
	{
		/* TIM3 is reset after STOP wakeup; sleep time is not aging time. */
		delta = 0U;
	}

	s_u32FactoryAgingLastTick = now_tick;

	if ((delta == 0U) || (s_u32FactoryAgingElapsed10ms >= FACTORY_AGING_DURATION_10MS))
	{
		return;
	}

	if (delta >= (FACTORY_AGING_DURATION_10MS - s_u32FactoryAgingElapsed10ms))
	{
		s_u32FactoryAgingElapsed10ms = FACTORY_AGING_DURATION_10MS;
	}
	else
	{
		s_u32FactoryAgingElapsed10ms += delta;
	}
}

UINT8 FactoryAging_SaveProgressBeforeSleep(void)
{
#if PROJECT_CFG_FACTORY_AGING_ENABLE
	if (s_u8FactoryAgingState != FACTORY_AGING_STATE_RUNNING)
	{
		return 1U;
	}

	FactoryAging_AddRunningTicks(SysTime_Get10msTickCount());
	if (s_u32FactoryAgingElapsed10ms >= FACTORY_AGING_DURATION_10MS)
	{
		return FactoryAging_Finish();
	}

	return FactoryAging_SaveStoredProgress(FLASH_FACTORY_AGING_STATE_RUNNING, 0U, 1U);
#else
	return 1U;
#endif
}

UINT8 FactoryAging_IsActive(void)
{
#if PROJECT_CFG_FACTORY_AGING_ENABLE
	return (s_u8FactoryAgingState == FACTORY_AGING_STATE_RUNNING) ? 1U : 0U;
#else
	return 0U;
#endif
}

static void FactoryAging_Task(void)
{
#if PROJECT_CFG_FACTORY_AGING_ENABLE
	UINT32 now_tick = SysTime_Get10msTickCount();

	if (s_u8FactoryAgingState == FACTORY_AGING_STATE_UNINIT)
	{
		FactoryAging_Start(now_tick);
		return;
	}

	if (s_u8FactoryAgingState != FACTORY_AGING_STATE_RUNNING)
	{
		return;
	}

	FactoryAging_AddRunningTicks(now_tick);
	if (s_u32FactoryAgingElapsed10ms >= FACTORY_AGING_DURATION_10MS)
	{
		if ((s_u32FactoryAgingNextFinishRetry10ms == 0U) ||
			(now_tick >= s_u32FactoryAgingNextFinishRetry10ms))
		{
			if (FactoryAging_Finish() == 0U)
			{
				s_u32FactoryAgingNextFinishRetry10ms =
					now_tick + FACTORY_AGING_FINISH_RETRY_10MS;
			}
		}
		return;
	}

	(void)FactoryAging_SaveStoredProgress(FLASH_FACTORY_AGING_STATE_RUNNING, 0U, 0U);
#endif
}

static void Runtime_RunDebugOnce(void)
{
	App_AFEGet();
	App_Sci();
}

static void Runtime_RunOnce(void)
{
	SysTime_LatchTaskFlags();
	FactoryAging_Task();
	APP_LedBar();
	// App_WarnCtrl();
	App_AFEGet();

	App_Sci();
	App_AnlogCal();
	App_LowPowerProcess();
	App_Can();
	// App_SleepDeal(); // 关闭这个功能的话，在InitVar()中System_OnOFF_Func相关置零，或者直接屏蔽
	// App_SOC();
	StorageFlash_AppUseTest_Task();

#ifdef __FUNC__HEAT__
	App_Heat_Cool_Ctrl();
#endif

	App_FlashUpdate();
	App_LogRecord();
	App_ProID_Deal();
#ifdef wdog_enable
	Feed_IWatchDog;
#endif
	// MainLoop_EnterIdleSleep();
}

extern void new_todo_logi(void);

int main(void)
{
	InitDevice(); // ³õÊ¼»¯ÍâÉè
	InitVar();	  // ³õÊ¼»¯±äÁ¿
    Init_RTC();
    // RTC_WKTimeConfig();

	while (1)
	{
#if (defined _DEBUG_CODE)
		Runtime_RunDebugOnce();
#else
		Runtime_RunOnce();
#endif
	}
}

void InitDevice(void)
{
	SystemInit(); // HSEÄ¬ÈÏ±¶Æµµ½72MHz£¬Èç¹ûÃ»HSEÇÐ»ØHSIÔõÃ´´¦ÀíÄ¿Ç°»¹Ã»ÁË½â

#if (defined _DEBUG_CODE)
	InitDelay();
	InitIO();
#else
	InitDelay();
	IsSleepStartUp();

	jtag_disableAndConfIO();

	InitNVIC();
	InitIO();
	InitSci();
#ifdef ELOG_OUTPUT_ENABLE
	InitUSART_CommonUpper();
	elogInit();
#endif
	InitSystemWakeUp();
	StorageFlash_PrintBootCheck();
#ifdef FLASH64K_APP_QUICK_TEST_ENABLE
	StorageFlash_RunAppQuickTest();
#endif
	InitE2PROM(); // ¾ö¶¨°ÑÕâ¸ö·ÅÔÚÇ°Ãæ£¬ÓÅÏÈ¼¶Ìá¸ß£¬ÒòÎª¿Í»§´®¿Ú³õÊ¼»¯£¬ÓÐ¿ÉÄÜÒª¶ÁÆä×Ô¼ºµÄÊý¾Ý
	InitAFE1();
	InitCan();
	InitADC();
	InitSci();

#ifdef __FUNC__HEAT__
	InitHeat_Cool();
#endif
	// Init_ChargerLoad_Det();
	// InitMosRelay_DOx();
	InitData_SOC(); // ±ØÐë·ÅÔÚ¶ÁÍêeepromÊý¾ÝºóÃæ

	InitTimer();
	log_w("init over");

#ifdef _DEBUG_
	DBGMCU_Config(DBGMCU_STOP, ENABLE);
#endif

#ifdef wdog_enable
	Init_IWDG();
#endif // !1

#endif
}

void InitVar(void)
{
	// SystemMonitorResetData_EEPROM();							//Õâ¸öº¯ÊýµÄ³õÊ¼»¯Ä¬ÈÏÐèÇó¹¦ÄÜÐÞ¸ÄÁË£¬ÒªÐÞ¸ÄEEPROMµÄÉÏµç±êÖ¾Î»
	InitSystemMonitorData_EEPROM();
	g_u32CS_Res_AFE = ((UINT32)OtherElement.u16Sys_CS_Res_Num * 1000) / OtherElement.u16Sys_CS_Res;

	SystemStatus.bits.b1StartUpBMS = 0; // È¥µô¿ª»úÊ±Ðò
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
