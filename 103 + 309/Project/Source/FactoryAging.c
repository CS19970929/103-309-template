#include "main.h"
#include "FactoryAging.h"

#define FACTORY_AGING_STATE_UNINIT  ((UINT8)0U)
#define FACTORY_AGING_STATE_RUNNING ((UINT8)1U)
#define FACTORY_AGING_STATE_DONE    ((UINT8)2U)
#define FACTORY_AGING_STATE_STOPPED ((UINT8)3U)
#define FACTORY_AGING_10MS_PER_SEC  ((UINT32)100U)
#define FACTORY_AGING_FLASH_SAVE_INTERVAL_SECONDS ((UINT32)7200U)
#define FACTORY_AGING_FLASH_SAVE_INTERVAL_10MS \
	(FACTORY_AGING_FLASH_SAVE_INTERVAL_SECONDS * FACTORY_AGING_10MS_PER_SEC)
#define FACTORY_AGING_BKP_SAVE_INTERVAL_10MS FACTORY_AGING_10MS_PER_SEC
#define FACTORY_AGING_FINISH_RETRY_10MS FACTORY_AGING_10MS_PER_SEC
#define FACTORY_AGING_DEFAULT_DURATION_10MS \
	((UINT32)PROJECT_CFG_FACTORY_AGING_DURATION_SECONDS * FACTORY_AGING_10MS_PER_SEC)
#define FACTORY_AGING_DURATION_10MS FactoryAging_GetDuration10ms()
#define FACTORY_AGING_DURATION_HOURS_RESET_VALUE ((UINT16)0xFFFFU)
#define FACTORY_AGING_MOS_MODE_UNKNOWN        ((UINT8)0xFFU)
#define FACTORY_AGING_MOS_MODE_FACTORY        ((UINT8)1U)
#define FACTORY_AGING_MOS_MODE_5V_CHARGE      ((UINT8)2U)
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
static UINT16 s_u16FactoryAgingDurationHours = 0U;
static UINT8 s_u8FactoryAgingBkpSaveValid = 0U;
static UINT8 s_u8FactoryAgingFlashSaveValid = 0U;
static UINT8 s_u8FactoryAgingMosMode = FACTORY_AGING_MOS_MODE_UNKNOWN;

static UINT8 FactoryAging_DurationHoursValid(UINT16 hours)
{
	return ((hours >= FACTORY_AGING_DURATION_HOURS_MIN) &&
			(hours <= FACTORY_AGING_DURATION_HOURS_MAX)) ? 1U : 0U;
}

static UINT32 FactoryAging_GetDuration10ms(void)
{
	if (FactoryAging_DurationHoursValid(s_u16FactoryAgingDurationHours) != 0U)
	{
		return (UINT32)s_u16FactoryAgingDurationHours * 3600U * FACTORY_AGING_10MS_PER_SEC;
	}
	return FACTORY_AGING_DEFAULT_DURATION_10MS;
}

static void FactoryAging_LoadDurationFromData(const STORAGE_FLASH_FACTORY_AGING_DATA *data)
{
	if ((data != 0) &&
		(FactoryAging_DurationHoursValid(data->u16DurationHours) != 0U))
	{
		s_u16FactoryAgingDurationHours = data->u16DurationHours;
	}
}

static UINT32 FactoryAging_ClampElapsed(UINT32 elapsed10ms)
{
	return (elapsed10ms > FACTORY_AGING_DURATION_10MS) ?
		FACTORY_AGING_DURATION_10MS : elapsed10ms;
}

static UINT16 FactoryAging_BkpCrc(UINT32 elapsed10ms)
{
	return (UINT16)((UINT16)(elapsed10ms & 0xFFFFU) ^
		(UINT16)((elapsed10ms >> 16) & 0xFFFFU) ^
		FACTORY_AGING_BKP_MAGIC ^ 0x5A5AU);
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

static UINT8 FactoryAging_LoadStoredProgress(UINT32 *elapsed10ms, UINT8 *done, UINT8 *stopped)
{
	STORAGE_FLASH_FACTORY_AGING_DATA data;
	UINT32 flash_elapsed = 0U;
	UINT32 bkp_elapsed = 0U;
	UINT8 has_progress = 0U;

	if ((elapsed10ms == 0) || (done == 0) || (stopped == 0))
	{
		return 0U;
	}

	*elapsed10ms = 0U;
	*done = 0U;
	*stopped = 0U;
	if (StorageFlash_LoadFactoryAgingData(&data) != 0U)
	{
		FactoryAging_LoadDurationFromData(&data);
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
		else if (data.u16State == FLASH_FACTORY_AGING_STATE_STOPPED)
		{
			flash_elapsed = FactoryAging_ClampElapsed(data.u32Elapsed10ms);
			s_u32FactoryAgingLastFlashSave10ms = flash_elapsed;
			s_u8FactoryAgingFlashSaveValid = 1U;
			*stopped = 1U;
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

UINT8 FactoryAging_ShouldStartOnBoot(void)
{
#if PROJECT_CFG_FACTORY_AGING_ENABLE
	UINT32 stored_elapsed = 0U;
	UINT8 done = 0U;
	UINT8 stopped = 0U;

	(void)FactoryAging_LoadStoredProgress(&stored_elapsed, &done, &stopped);
	return ((done == 0U) && (stopped == 0U)) ? 1U : 0U;
#else
	return 0U;
#endif
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
	data.u16DurationHours =
		(FactoryAging_DurationHoursValid(s_u16FactoryAgingDurationHours) != 0U) ?
			s_u16FactoryAgingDurationHours : FACTORY_AGING_DURATION_HOURS_RESET_VALUE;
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
	STORAGE_FLASH_FACTORY_AGING_DATA data;

	if ((StorageFlash_LoadFactoryAgingData(&data) != 0U) &&
		(data.u16State == FLASH_FACTORY_AGING_STATE_DONE))
	{
		return 1U;
	}

	s_u32FactoryAgingElapsed10ms = FACTORY_AGING_DURATION_10MS;
	return FactoryAging_SaveStoredProgress(FLASH_FACTORY_AGING_STATE_DONE, 1U, 1U);
}

static void FactoryAging_ResetMosCache(void)
{
	s_u8FactoryAgingMosMode = FACTORY_AGING_MOS_MODE_UNKNOWN;
}

static void FactoryAging_ApplyStoppedMos(void)
{
	enter_fac_mode(false);
	FactoryAging_ResetMosCache();
}

static void FactoryAging_ApplyRunningMos(void)
{
	UINT8 next_mode = (MosStartup_Is5vChargeActive() != 0U) ?
		FACTORY_AGING_MOS_MODE_5V_CHARGE : FACTORY_AGING_MOS_MODE_FACTORY;

	if (s_u8FactoryAgingMosMode == next_mode)
	{
		return;
	}

	enter_fac_mode(true);
	s_u8FactoryAgingMosMode = next_mode;
}

static UINT8 FactoryAging_EnterRunningFromHost(UINT32 now_tick)
{
	s_u8FactoryAgingState = FACTORY_AGING_STATE_RUNNING;
	s_u32FactoryAgingLastTick = now_tick;
	s_u32FactoryAgingNextFinishRetry10ms = 0U;
	FactoryAging_ResetMosCache();
	FactoryAging_ApplyRunningMos();
	return FactoryAging_SaveStoredProgress(FLASH_FACTORY_AGING_STATE_RUNNING, 1U, 1U);
}

static UINT8 FactoryAging_Finish(void)
{
	FactoryAging_ApplyStoppedMos();
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
	UINT8 stopped = 0U;

	(void)FactoryAging_LoadStoredProgress(&stored_elapsed, &done, &stopped);
	s_u32FactoryAgingElapsed10ms = FactoryAging_ClampElapsed(stored_elapsed);
	s_u32FactoryAgingLastTick = now_tick;
	s_u32FactoryAgingNextFinishRetry10ms = 0U;

	if (done != 0U)
	{
		FactoryAging_ApplyStoppedMos();
		s_u8FactoryAgingState = FACTORY_AGING_STATE_DONE;
		return;
	}

	if (stopped != 0U)
	{
		FactoryAging_ApplyStoppedMos();
		s_u8FactoryAgingState = FACTORY_AGING_STATE_STOPPED;
		return;
	}

	s_u8FactoryAgingState = FACTORY_AGING_STATE_RUNNING;
	if (s_u32FactoryAgingElapsed10ms >= FACTORY_AGING_DURATION_10MS)
	{
		(void)FactoryAging_Finish();
		return;
	}

	FactoryAging_ResetMosCache();
	FactoryAging_ApplyRunningMos();
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

static void FactoryAging_LoadRuntimeStateForHost(UINT32 now_tick)
{
	UINT32 stored_elapsed = 0U;
	UINT8 done = 0U;
	UINT8 stopped = 0U;

	if (s_u8FactoryAgingState != FACTORY_AGING_STATE_UNINIT)
	{
		return;
	}

	(void)FactoryAging_LoadStoredProgress(&stored_elapsed, &done, &stopped);
	s_u32FactoryAgingElapsed10ms = FactoryAging_ClampElapsed(stored_elapsed);
	s_u32FactoryAgingLastTick = now_tick;
	s_u32FactoryAgingNextFinishRetry10ms = 0U;

	if ((done != 0U) || (s_u32FactoryAgingElapsed10ms >= FACTORY_AGING_DURATION_10MS))
	{
		FactoryAging_ApplyStoppedMos();
		s_u8FactoryAgingState = FACTORY_AGING_STATE_DONE;
		return;
	}

	if (stopped != 0U)
	{
		FactoryAging_ApplyStoppedMos();
		s_u8FactoryAgingState = FACTORY_AGING_STATE_STOPPED;
		return;
	}

	s_u8FactoryAgingState = FACTORY_AGING_STATE_STOPPED;
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

UINT8 FactoryAging_GetState(void)
{
#if PROJECT_CFG_FACTORY_AGING_ENABLE
	UINT32 stored_elapsed = 0U;
	UINT8 done = 0U;
	UINT8 stopped = 0U;

	switch (s_u8FactoryAgingState)
	{
	case FACTORY_AGING_STATE_RUNNING:
		return FACTORY_AGING_PUBLIC_STATE_RUNNING;
	case FACTORY_AGING_STATE_DONE:
		return FACTORY_AGING_PUBLIC_STATE_DONE;
	case FACTORY_AGING_STATE_STOPPED:
		return FACTORY_AGING_PUBLIC_STATE_STOPPED;
	default:
		(void)FactoryAging_LoadStoredProgress(&stored_elapsed, &done, &stopped);
		if ((done != 0U) || (stored_elapsed >= FACTORY_AGING_DURATION_10MS))
		{
			return FACTORY_AGING_PUBLIC_STATE_DONE;
		}
		if (stopped != 0U)
		{
			return FACTORY_AGING_PUBLIC_STATE_STOPPED;
		}
		return FACTORY_AGING_PUBLIC_STATE_RUNNING;
	}
#else
	return FACTORY_AGING_PUBLIC_STATE_STOPPED;
#endif
}

UINT32 FactoryAging_GetRemainingSeconds(void)
{
#if PROJECT_CFG_FACTORY_AGING_ENABLE
	UINT32 elapsed10ms = s_u32FactoryAgingElapsed10ms;
	UINT32 remain10ms;
	UINT8 done = 0U;
	UINT8 stopped = 0U;

	if (s_u8FactoryAgingState == FACTORY_AGING_STATE_UNINIT)
	{
		(void)FactoryAging_LoadStoredProgress(&elapsed10ms, &done, &stopped);
		(void)stopped;
		if (done != 0U)
		{
			return 0U;
		}
	}

	elapsed10ms = FactoryAging_ClampElapsed(elapsed10ms);
	if (elapsed10ms >= FACTORY_AGING_DURATION_10MS)
	{
		return 0U;
	}

	remain10ms = FACTORY_AGING_DURATION_10MS - elapsed10ms;
	return (remain10ms + (FACTORY_AGING_10MS_PER_SEC - 1U)) / FACTORY_AGING_10MS_PER_SEC;
#else
	return 0U;
#endif
}

UINT8 FactoryAging_StartByHost(void)
{
#if PROJECT_CFG_FACTORY_AGING_ENABLE
	UINT32 now_tick = SysTime_Get10msTickCount();

	FactoryAging_LoadRuntimeStateForHost(now_tick);
	if (s_u8FactoryAgingState == FACTORY_AGING_STATE_DONE)
	{
		return 0U;
	}
	if (s_u32FactoryAgingElapsed10ms >= FACTORY_AGING_DURATION_10MS)
	{
		(void)FactoryAging_Finish();
		return 0U;
	}

	return FactoryAging_EnterRunningFromHost(now_tick);
#else
	return 0U;
#endif
}

UINT8 FactoryAging_StopByHost(void)
{
#if PROJECT_CFG_FACTORY_AGING_ENABLE
	UINT32 now_tick = SysTime_Get10msTickCount();

	FactoryAging_LoadRuntimeStateForHost(now_tick);
	if (s_u8FactoryAgingState == FACTORY_AGING_STATE_RUNNING)
	{
		FactoryAging_AddRunningTicks(now_tick);
		if (s_u32FactoryAgingElapsed10ms >= FACTORY_AGING_DURATION_10MS)
		{
			return FactoryAging_Finish();
		}
	}
	else if (s_u8FactoryAgingState == FACTORY_AGING_STATE_DONE)
	{
		return 1U;
	}

	FactoryAging_ApplyStoppedMos();
	s_u8FactoryAgingState = FACTORY_AGING_STATE_STOPPED;
	s_u32FactoryAgingNextFinishRetry10ms = 0U;
	return FactoryAging_SaveStoredProgress(FLASH_FACTORY_AGING_STATE_STOPPED, 1U, 1U);
#else
	return 0U;
#endif
}

UINT8 FactoryAging_ResetTimeByHost(void)
{
#if PROJECT_CFG_FACTORY_AGING_ENABLE
	UINT32 now_tick = SysTime_Get10msTickCount();

	FactoryAging_LoadRuntimeStateForHost(now_tick);

	s_u32FactoryAgingElapsed10ms = 0U;
	return FactoryAging_EnterRunningFromHost(now_tick);
#else
	return 0U;
#endif
}

UINT8 FactoryAging_SetDurationHoursByHost(UINT16 hours)
{
#if PROJECT_CFG_FACTORY_AGING_ENABLE
	UINT32 now_tick;

	if (FactoryAging_DurationHoursValid(hours) == 0U)
	{
		return 0U;
	}

	now_tick = SysTime_Get10msTickCount();
	FactoryAging_LoadRuntimeStateForHost(now_tick);

	s_u16FactoryAgingDurationHours = hours;
	s_u32FactoryAgingElapsed10ms = 0U;
	return FactoryAging_EnterRunningFromHost(now_tick);
#else
	(void)hours;
	return 0U;
#endif
}

void FactoryAging_Task(void)
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

	FactoryAging_ApplyRunningMos();
	(void)FactoryAging_SaveStoredProgress(FLASH_FACTORY_AGING_STATE_RUNNING, 0U, 0U);
#endif
}
