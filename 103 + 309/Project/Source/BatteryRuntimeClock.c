#include "main.h"
#include "BatteryRuntimeClock.h"

#define BATTERY_RUNTIME_FLASH_SAVE_INTERVAL_SECONDS ((UINT32)600U)
#define BATTERY_RUNTIME_INVALID_SECONDS             ((UINT32)0xFFFFFFFFU)
#define BATTERY_RUNTIME_BKP_LO_REG                  BKP_DR13
#define BATTERY_RUNTIME_BKP_HI_REG                  BKP_DR14
#define BATTERY_RUNTIME_FLAG_SLEEP_PENDING          ((UINT16)0x0001U)

typedef struct BATTERY_RUNTIME_CLOCK_TAG
{
	UINT8 initialized;
	UINT8 flashValid;
	UINT8 sleepPending;
	UINT8 reserved;
	UINT32 runtimeSeconds;
	UINT32 sleepEntryEpochSeconds;
	UINT32 lastFlashSaveSeconds;
} BATTERY_RUNTIME_CLOCK;

static BATTERY_RUNTIME_CLOCK s_battery_runtime_clock;

static void BatteryRuntimeClock_EnableBkpAccess(void)
{
	RCC_APB1PeriphClockCmd(RCC_APB1Periph_PWR | RCC_APB1Periph_BKP, ENABLE);
	PWR_BackupAccessCmd(ENABLE);
}

static void BatteryRuntimeClock_SaveBkp(UINT32 runtime_seconds)
{
	BatteryRuntimeClock_EnableBkpAccess();
	BKP_WriteBackupRegister(BATTERY_RUNTIME_BKP_LO_REG,
							(UINT16)(runtime_seconds & 0xFFFFU));
	BKP_WriteBackupRegister(BATTERY_RUNTIME_BKP_HI_REG,
							(UINT16)((runtime_seconds >> 16) & 0xFFFFU));
}

static UINT8 BatteryRuntimeClock_LoadBkp(UINT32 *runtime_seconds)
{
	UINT16 lo;
	UINT16 hi;
	UINT32 value;

	if (runtime_seconds == 0)
	{
		return 0U;
	}

	BatteryRuntimeClock_EnableBkpAccess();
	lo = BKP_ReadBackupRegister(BATTERY_RUNTIME_BKP_LO_REG);
	hi = BKP_ReadBackupRegister(BATTERY_RUNTIME_BKP_HI_REG);
	value = ((UINT32)hi << 16) | lo;
	if (value == BATTERY_RUNTIME_INVALID_SECONDS)
	{
		return 0U;
	}

	*runtime_seconds = value;
	return 1U;
}

static UINT8 BatteryRuntimeClock_GetHardwareEpoch(UINT32 *epoch_seconds)
{
	struct RTC_ELEMENT time;

	if (epoch_seconds == 0)
	{
		return 0U;
	}
	if (RTC_GetCalendarTime(&time) == 0U)
	{
		return 0U;
	}

	return RTC_CalendarToEpochSeconds(&time, epoch_seconds);
}

static UINT8 BatteryRuntimeClock_SaveFlash(void)
{
	STORAGE_FLASH_RUNTIME_CLOCK_DATA data;

	memset(&data, 0, sizeof(data));
	data.u16FormatVersion = FLASH_STORAGE_RUNTIME_CLOCK_DATA_VERSION;
	data.u16Flags = s_battery_runtime_clock.sleepPending ?
		BATTERY_RUNTIME_FLAG_SLEEP_PENDING : 0U;
	data.u32RuntimeSeconds = s_battery_runtime_clock.runtimeSeconds;
	data.u32SleepEntryEpochSeconds =
		s_battery_runtime_clock.sleepEntryEpochSeconds;

	if (StorageFlash_SaveRuntimeClockData(&data) == 0U)
	{
		return 0U;
	}

	s_battery_runtime_clock.lastFlashSaveSeconds =
		s_battery_runtime_clock.runtimeSeconds;
	s_battery_runtime_clock.flashValid = 1U;
	return 1U;
}

static void BatteryRuntimeClock_SaveFlashIfNeeded(UINT8 force)
{
	UINT32 elapsed;

	if (force != 0U)
	{
		(void)BatteryRuntimeClock_SaveFlash();
		return;
	}
	if (s_battery_runtime_clock.flashValid == 0U)
	{
		(void)BatteryRuntimeClock_SaveFlash();
		return;
	}

	elapsed = s_battery_runtime_clock.runtimeSeconds -
		s_battery_runtime_clock.lastFlashSaveSeconds;
	if (elapsed >= BATTERY_RUNTIME_FLASH_SAVE_INTERVAL_SECONDS)
	{
		(void)BatteryRuntimeClock_SaveFlash();
	}
}

static void BatteryRuntimeClock_AddSecondsInternal(UINT32 seconds)
{
	if ((s_battery_runtime_clock.initialized == 0U) || (seconds == 0U))
	{
		return;
	}

	if (seconds >= (BATTERY_RUNTIME_INVALID_SECONDS -
				   s_battery_runtime_clock.runtimeSeconds))
	{
		s_battery_runtime_clock.runtimeSeconds = BATTERY_RUNTIME_INVALID_SECONDS - 1U;
	}
	else
	{
		s_battery_runtime_clock.runtimeSeconds += seconds;
	}
}

static void BatteryRuntimeClock_AddAndStore(UINT32 seconds)
{
	if ((s_battery_runtime_clock.initialized == 0U) || (seconds == 0U))
	{
		return;
	}

	BatteryRuntimeClock_AddSecondsInternal(seconds);
	BatteryRuntimeClock_SaveBkp(s_battery_runtime_clock.runtimeSeconds);
	BatteryRuntimeClock_SaveFlashIfNeeded(0U);
}

static void BatteryRuntimeClock_ApplyPendingSleepDelta(void)
{
	UINT32 current_epoch;
	UINT32 delta;

	if (s_battery_runtime_clock.sleepPending == 0U)
	{
		return;
	}

	if ((BatteryRuntimeClock_GetHardwareEpoch(&current_epoch) != 0U) &&
		(current_epoch >= s_battery_runtime_clock.sleepEntryEpochSeconds))
	{
		delta = current_epoch - s_battery_runtime_clock.sleepEntryEpochSeconds;
		BatteryRuntimeClock_AddSecondsInternal(delta);
	}

	s_battery_runtime_clock.sleepPending = 0U;
	s_battery_runtime_clock.sleepEntryEpochSeconds = 0U;
	BatteryRuntimeClock_SaveBkp(s_battery_runtime_clock.runtimeSeconds);
	BatteryRuntimeClock_SaveFlashIfNeeded(1U);
}

void BatteryRuntimeClock_Init(void)
{
	STORAGE_FLASH_RUNTIME_CLOCK_DATA data;
	UINT32 bkp_runtime = 0U;
	UINT8 has_bkp;

	memset(&s_battery_runtime_clock, 0, sizeof(s_battery_runtime_clock));
	has_bkp = BatteryRuntimeClock_LoadBkp(&bkp_runtime);

	if ((StorageFlash_LoadRuntimeClockData(&data) != 0U) &&
		(data.u16FormatVersion == FLASH_STORAGE_RUNTIME_CLOCK_DATA_VERSION))
	{
		s_battery_runtime_clock.runtimeSeconds = data.u32RuntimeSeconds;
		s_battery_runtime_clock.sleepPending =
			(UINT8)((data.u16Flags & BATTERY_RUNTIME_FLAG_SLEEP_PENDING) != 0U);
		s_battery_runtime_clock.sleepEntryEpochSeconds =
			data.u32SleepEntryEpochSeconds;
		s_battery_runtime_clock.lastFlashSaveSeconds = data.u32RuntimeSeconds;
		s_battery_runtime_clock.flashValid = 1U;
	}

	if ((has_bkp != 0U) &&
		(bkp_runtime > s_battery_runtime_clock.runtimeSeconds))
	{
		s_battery_runtime_clock.runtimeSeconds = bkp_runtime;
	}

	s_battery_runtime_clock.initialized = 1U;
	BatteryRuntimeClock_ApplyPendingSleepDelta();
	BatteryRuntimeClock_SaveBkp(s_battery_runtime_clock.runtimeSeconds);
	BatteryRuntimeClock_SaveFlashIfNeeded(s_battery_runtime_clock.flashValid == 0U);
}

void BatteryRuntimeClock_Task1s(void)
{
	if (g_st_SysTimeFlag.bits.b1Sys1000msFlag == 0U)
	{
		return;
	}

	BatteryRuntimeClock_AddAndStore(1U);
}

void BatteryRuntimeClock_AddSleepSeconds(UINT32 seconds)
{
	BatteryRuntimeClock_AddAndStore(seconds);
}

UINT8 BatteryRuntimeClock_MarkSleepEntry(void)
{
	UINT32 entry_epoch;

	if ((s_battery_runtime_clock.initialized == 0U) ||
		(BatteryRuntimeClock_GetHardwareEpoch(&entry_epoch) == 0U))
	{
		return 0U;
	}

	s_battery_runtime_clock.sleepEntryEpochSeconds = entry_epoch;
	s_battery_runtime_clock.sleepPending = 1U;
	BatteryRuntimeClock_SaveBkp(s_battery_runtime_clock.runtimeSeconds);
	BatteryRuntimeClock_SaveFlashIfNeeded(1U);
	return 1U;
}

UINT8 BatteryRuntimeClock_SetRtcTime(const struct RTC_ELEMENT *time)
{
	if ((s_battery_runtime_clock.initialized == 0U) ||
		(RTC_SetCounterFromCalendar(time) == 0U))
	{
		return 0U;
	}

	BatteryRuntimeClock_SaveFlashIfNeeded(1U);
	return 1U;
}

UINT8 BatteryRuntimeClock_GetRtcTime(struct RTC_ELEMENT *time)
{
	return RTC_GetCalendarTime(time);
}

UINT32 BatteryRuntimeClock_GetRuntimeSeconds(void)
{
	return s_battery_runtime_clock.runtimeSeconds;
}
