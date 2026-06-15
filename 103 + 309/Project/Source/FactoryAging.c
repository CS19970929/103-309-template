#include "main.h"
#include "FactoryAging.h"
#include "DebugWatch.h"

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
#define FACTORY_AGING_BKP_MAGIC      ((UINT16)0xA91E)
#define FACTORY_AGING_BKP_MAGIC_REG  BKP_DR6
#define FACTORY_AGING_BKP_LO_REG     BKP_DR7
#define FACTORY_AGING_BKP_HI_REG     BKP_DR8
#define FACTORY_AGING_BKP_CRC_REG    BKP_DR9

typedef struct FACTORY_AGING_RUNTIME_TAG
{
	UINT8 state;
	UINT32 elapsed10ms;
	UINT32 lastTick;
	UINT32 lastBkpSave10ms;
	UINT32 lastFlashSave10ms;
	UINT32 nextFinishRetry10ms;
	UINT16 durationHours;
	UINT8 bkpSaveValid;
	UINT8 flashSaveValid;
	UINT32 pendingSleep10ms;
} FactoryAgingRuntime;

static FactoryAgingRuntime s_factory_aging = {
	FACTORY_AGING_STATE_UNINIT,
	0U,
	0U,
	0U,
	0U,
	0U,
	0U,
	0U,
	0U,
	0U
};

#if DEBUG_WATCH_ENABLED
void FactoryAging_DebugWatchBind(DEBUG_WATCH_ROOT *watch)
{
	watch->runtime.factory_aging = &s_factory_aging;
}
#endif

static UINT8 FactoryAging_DurationHoursValid(UINT16 hours)
{
	return ((hours >= FACTORY_AGING_DURATION_HOURS_MIN) &&
			(hours <= FACTORY_AGING_DURATION_HOURS_MAX)) ? 1U : 0U;
}

static UINT32 FactoryAging_GetDuration10ms(void)
{
	if (FactoryAging_DurationHoursValid(s_factory_aging.durationHours) != 0U)
	{
		return (UINT32)s_factory_aging.durationHours * 3600U * FACTORY_AGING_10MS_PER_SEC;
	}
	return FACTORY_AGING_DEFAULT_DURATION_10MS;
}

static void FactoryAging_LoadDurationFromData(const STORAGE_FLASH_FACTORY_AGING_DATA *data)
{
	if ((data != 0) &&
		(FactoryAging_DurationHoursValid(data->u16DurationHours) != 0U))
	{
		s_factory_aging.durationHours = data->u16DurationHours;
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
	BKP_WriteBackupRegister(FACTORY_AGING_BKP_LO_REG, (UINT16)(elapsed10ms & 0xFFFFU));
	BKP_WriteBackupRegister(FACTORY_AGING_BKP_HI_REG, (UINT16)((elapsed10ms >> 16) & 0xFFFFU));
	BKP_WriteBackupRegister(FACTORY_AGING_BKP_CRC_REG, FactoryAging_BkpCrc(elapsed10ms));
	s_factory_aging.lastBkpSave10ms = elapsed10ms;
	s_factory_aging.bkpSaveValid = 1U;
}

static UINT8 FactoryAging_LoadBkp(UINT32 *elapsed10ms)
{
	UINT16 magic;
	UINT16 lo;
	UINT16 hi;
	UINT32 elapsed;

	if (elapsed10ms == 0)
	{
		return 0U;
	}

	FactoryAging_EnableBkpAccess();
	magic = BKP_ReadBackupRegister(FACTORY_AGING_BKP_MAGIC_REG);
	if (magic != FACTORY_AGING_BKP_MAGIC)
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
			s_factory_aging.lastFlashSave10ms = FACTORY_AGING_DURATION_10MS;
			s_factory_aging.flashSaveValid = 1U;
			return 1U;
		}
		if (data.u16State == FLASH_FACTORY_AGING_STATE_RUNNING)
		{
			flash_elapsed = FactoryAging_ClampElapsed(data.u32Elapsed10ms);
			s_factory_aging.lastFlashSave10ms = flash_elapsed;
			s_factory_aging.flashSaveValid = 1U;
			has_progress = 1U;
		}
		else if (data.u16State == FLASH_FACTORY_AGING_STATE_STOPPED)
		{
			flash_elapsed = FactoryAging_ClampElapsed(data.u32Elapsed10ms);
			s_factory_aging.lastFlashSave10ms = flash_elapsed;
			s_factory_aging.flashSaveValid = 1U;
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
	UINT32 stored_elapsed = 0U;
	UINT8 done = 0U;
	UINT8 stopped = 0U;

	(void)FactoryAging_LoadStoredProgress(&stored_elapsed, &done, &stopped);
	return ((done == 0U) && (stopped == 0U)) ? 1U : 0U;
}

static UINT8 FactoryAging_SaveStoredProgress(UINT16 state, UINT8 force_flash, UINT8 force_bkp)
{
	STORAGE_FLASH_FACTORY_AGING_DATA data;
	UINT8 save_flash = force_flash;

	if ((force_bkp != 0U) ||
		(s_factory_aging.bkpSaveValid == 0U) ||
		((s_factory_aging.elapsed10ms - s_factory_aging.lastBkpSave10ms) >= FACTORY_AGING_BKP_SAVE_INTERVAL_10MS))
	{
		FactoryAging_SaveBkp(s_factory_aging.elapsed10ms);
	}

	if (save_flash == 0U)
	{
		if (s_factory_aging.flashSaveValid == 0U)
		{
			save_flash = 1U;
		}
		else if ((s_factory_aging.elapsed10ms - s_factory_aging.lastFlashSave10ms) >=
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
	data.u32Elapsed10ms = FactoryAging_ClampElapsed(s_factory_aging.elapsed10ms);
	data.u16State = state;
	data.u16DurationHours =
		(FactoryAging_DurationHoursValid(s_factory_aging.durationHours) != 0U) ?
			s_factory_aging.durationHours : FACTORY_AGING_DURATION_HOURS_RESET_VALUE;
	if (StorageFlash_SaveFactoryAgingData(&data) == 0U)
	{
		return 0U;
	}

	s_factory_aging.lastFlashSave10ms = data.u32Elapsed10ms;
	s_factory_aging.flashSaveValid = 1U;
	return 1U;
}

static UINT8 FactoryAging_MarkDone(void)
{
	STORAGE_FLASH_FACTORY_AGING_DATA data;

	if ((StorageFlash_LoadFactoryAgingData(&data) != 0U) &&
		(data.u16State == FLASH_FACTORY_AGING_STATE_DONE))
	{
		s_factory_aging.elapsed10ms = FACTORY_AGING_DURATION_10MS;
		return 1U;
	}

	s_factory_aging.elapsed10ms = FACTORY_AGING_DURATION_10MS;
	return FactoryAging_SaveStoredProgress(FLASH_FACTORY_AGING_STATE_DONE, 1U, 1U);
}

static UINT8 FactoryAging_ResolveStoredState(UINT32 *elapsed, UINT8 *was_done, UINT8 *was_stopped)
{
	UINT32 stored = 0U;
	UINT8 done = 0U;
	UINT8 stopped = 0U;
	if (!FactoryAging_LoadStoredProgress(&stored, &done, &stopped)) return 0U;
	*elapsed = FactoryAging_ClampElapsed(stored);
	*was_done = done;
	*was_stopped = stopped;
	return 1U;
}

static void FactoryAging_ApplyStoppedMos(void)
{
	enter_fac_mode(false);
}

static void FactoryAging_ApplyRunningMos(void)
{
	enter_fac_mode(true);
}

static UINT8 FactoryAging_EnterRunningFromHost(UINT32 now_tick)
{
	s_factory_aging.state = FACTORY_AGING_STATE_RUNNING;
	s_factory_aging.lastTick = now_tick;
	s_factory_aging.nextFinishRetry10ms = 0U;
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

	s_factory_aging.state = FACTORY_AGING_STATE_DONE;
	s_factory_aging.nextFinishRetry10ms = 0U;
	return 1U;
}

static void FactoryAging_Start(UINT32 now_tick)
{
	UINT32 stored_elapsed = 0U;
	UINT8 done = 0U;
	UINT8 stopped = 0U;

	(void)FactoryAging_ResolveStoredState(&stored_elapsed, &done, &stopped);
	s_factory_aging.elapsed10ms = stored_elapsed;
	s_factory_aging.lastTick = now_tick;
	s_factory_aging.nextFinishRetry10ms = 0U;

	if (done != 0U)
	{
		FactoryAging_ApplyStoppedMos();
		s_factory_aging.state = FACTORY_AGING_STATE_DONE;
		return;
	}

	if (stopped != 0U)
	{
		FactoryAging_ApplyStoppedMos();
		s_factory_aging.state = FACTORY_AGING_STATE_STOPPED;
		return;
	}

	s_factory_aging.state = FACTORY_AGING_STATE_RUNNING;
	if (stored_elapsed >= FACTORY_AGING_DURATION_10MS)
	{
		(void)FactoryAging_Finish();
		return;
	}

	FactoryAging_ApplyRunningMos();
	(void)FactoryAging_SaveStoredProgress(FLASH_FACTORY_AGING_STATE_RUNNING, 0U, 1U);
}

static void FactoryAging_AddRunningTicks(UINT32 now_tick)
{
	UINT32 delta;

	if (now_tick >= s_factory_aging.lastTick)
	{
		delta = now_tick - s_factory_aging.lastTick;
	}
	else
	{
		/* TIM3 is reset after STOP wakeup; include pending sleep time. */
		delta = s_factory_aging.pendingSleep10ms + now_tick;
		s_factory_aging.pendingSleep10ms = 0U;
	}

	s_factory_aging.lastTick = now_tick;

	if ((delta == 0U) || (s_factory_aging.elapsed10ms >= FACTORY_AGING_DURATION_10MS))
	{
		return;
	}

	if (delta >= (FACTORY_AGING_DURATION_10MS - s_factory_aging.elapsed10ms))
	{
		s_factory_aging.elapsed10ms = FACTORY_AGING_DURATION_10MS;
	}
	else
	{
		s_factory_aging.elapsed10ms += delta;
	}
}

static void FactoryAging_LoadRuntimeStateForHost(UINT32 now_tick)
{
	UINT32 stored_elapsed = 0U;
	UINT8 done = 0U;
	UINT8 stopped = 0U;

	if (s_factory_aging.state != FACTORY_AGING_STATE_UNINIT)
	{
		return;
	}

	(void)FactoryAging_ResolveStoredState(&stored_elapsed, &done, &stopped);
	s_factory_aging.elapsed10ms = stored_elapsed;
	s_factory_aging.lastTick = now_tick;
	s_factory_aging.nextFinishRetry10ms = 0U;

	if ((done != 0U) || (stored_elapsed >= FACTORY_AGING_DURATION_10MS))
	{
		FactoryAging_ApplyStoppedMos();
		s_factory_aging.state = FACTORY_AGING_STATE_DONE;
		return;
	}

	if (stopped != 0U)
	{
		FactoryAging_ApplyStoppedMos();
		s_factory_aging.state = FACTORY_AGING_STATE_STOPPED;
		return;
	}

	s_factory_aging.state = FACTORY_AGING_STATE_STOPPED;
}

UINT8 FactoryAging_SaveProgressBeforeSleep(void)
{
	if (s_factory_aging.state != FACTORY_AGING_STATE_RUNNING)
	{
		return 1U;
	}

	FactoryAging_AddRunningTicks(SysTime_Get10msTickCount());
	if (s_factory_aging.elapsed10ms >= FACTORY_AGING_DURATION_10MS)
	{
		return FactoryAging_Finish();
	}

	return FactoryAging_SaveStoredProgress(FLASH_FACTORY_AGING_STATE_RUNNING, 0U, 1U);
}

UINT8 FactoryAging_IsActive(void)
{
	return (s_factory_aging.state == FACTORY_AGING_STATE_RUNNING) ? 1U : 0U;
}

void FactoryAging_ApplySleepTime(UINT32 seconds)
{
	s_factory_aging.pendingSleep10ms += seconds * FACTORY_AGING_10MS_PER_SEC;
}

void FactoryAging_SaveProgressQuick(void)
{
	UINT32 now_tick;

	if (s_factory_aging.state != FACTORY_AGING_STATE_RUNNING)
	{
		return;
	}

	now_tick = SysTime_Get10msTickCount();
	FactoryAging_AddRunningTicks(now_tick);
	if (s_factory_aging.elapsed10ms >= FACTORY_AGING_DURATION_10MS)
	{
		FactoryAging_Finish();
		return;
	}

	(void)FactoryAging_SaveStoredProgress(FLASH_FACTORY_AGING_STATE_RUNNING, 0U, 1U);
}

UINT8 FactoryAging_GetState(void)
{
	UINT32 stored_elapsed = 0U;
	UINT8 done = 0U;
	UINT8 stopped = 0U;

	switch (s_factory_aging.state)
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
}

UINT32 FactoryAging_GetRemainingSeconds(void)
{
	UINT32 elapsed10ms = s_factory_aging.elapsed10ms;
	UINT32 remain10ms;
	UINT8 done = 0U;
	UINT8 stopped = 0U;

	if (s_factory_aging.state == FACTORY_AGING_STATE_UNINIT)
	{
		(void)FactoryAging_LoadStoredProgress(&elapsed10ms, &done, &stopped);
		(void)stopped;
		if (done != 0U)
		{
			return 0U;
		}
	}
	if (s_factory_aging.state == FACTORY_AGING_STATE_DONE)
	{
		return 0U;
	}

	elapsed10ms = FactoryAging_ClampElapsed(elapsed10ms);
	if (elapsed10ms >= FACTORY_AGING_DURATION_10MS)
	{
		return 0U;
	}

	remain10ms = FACTORY_AGING_DURATION_10MS - elapsed10ms;
	return (remain10ms + (FACTORY_AGING_10MS_PER_SEC - 1U)) / FACTORY_AGING_10MS_PER_SEC;
}

UINT8 FactoryAging_StartByHost(void)
{
	UINT32 now_tick = SysTime_Get10msTickCount();

	FactoryAging_LoadRuntimeStateForHost(now_tick);
	if ((s_factory_aging.state == FACTORY_AGING_STATE_DONE) ||
		(s_factory_aging.elapsed10ms >= FACTORY_AGING_DURATION_10MS))
	{
		s_factory_aging.elapsed10ms = 0U;
	}

	return FactoryAging_EnterRunningFromHost(now_tick);
}

UINT8 FactoryAging_StopByHost(void)
{
	UINT32 now_tick = SysTime_Get10msTickCount();

	FactoryAging_LoadRuntimeStateForHost(now_tick);
	if (s_factory_aging.state == FACTORY_AGING_STATE_DONE)
	{
		return 1U;
	}

	if (s_factory_aging.state == FACTORY_AGING_STATE_RUNNING)
	{
		FactoryAging_AddRunningTicks(now_tick);
	}

	s_factory_aging.elapsed10ms = FACTORY_AGING_DURATION_10MS;
	return FactoryAging_Finish();
}

UINT8 FactoryAging_ResetTimeByHost(void)
{
	UINT32 now_tick = SysTime_Get10msTickCount();

	FactoryAging_LoadRuntimeStateForHost(now_tick);

	s_factory_aging.elapsed10ms = 0U;
	return FactoryAging_EnterRunningFromHost(now_tick);
}

UINT8 FactoryAging_SetDurationHoursByHost(UINT16 hours)
{
	UINT32 now_tick;

	if (FactoryAging_DurationHoursValid(hours) == 0U)
	{
		return 0U;
	}

	now_tick = SysTime_Get10msTickCount();
	FactoryAging_LoadRuntimeStateForHost(now_tick);

	s_factory_aging.durationHours = hours;
	s_factory_aging.elapsed10ms = 0U;
	return FactoryAging_EnterRunningFromHost(now_tick);
}

void FactoryAging_Task(void)
{
	UINT32 now_tick = SysTime_Get10msTickCount();

	if (s_factory_aging.state == FACTORY_AGING_STATE_UNINIT)
	{
		FactoryAging_Start(now_tick);
		return;
	}

	if (s_factory_aging.state != FACTORY_AGING_STATE_RUNNING)
	{
		return;
	}

	FactoryAging_AddRunningTicks(now_tick);
	if (s_factory_aging.elapsed10ms >= FACTORY_AGING_DURATION_10MS)
	{
		if ((s_factory_aging.nextFinishRetry10ms == 0U) ||
			(now_tick >= s_factory_aging.nextFinishRetry10ms))
		{
			if (FactoryAging_Finish() == 0U)
			{
				s_factory_aging.nextFinishRetry10ms =
					now_tick + FACTORY_AGING_FINISH_RETRY_10MS;
			}
		}
		return;
	}

	FactoryAging_ApplyRunningMos();
	(void)FactoryAging_SaveStoredProgress(FLASH_FACTORY_AGING_STATE_RUNNING, 0U, 0U);
}
