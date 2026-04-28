#include "main.h"
#include "Flash64KAppTest.h"

#define FLASH64K_SIZE_REG_ADDR          ((UINT32)0x1FFFF7E0)
#define FLASH64K_TEST_SOC_SLOT_A        FLASH_ADDR_STORAGE_SOC_SLOT_A
#define FLASH64K_TEST_SOC_SLOT_B        FLASH_ADDR_STORAGE_SOC_SLOT_B
#define FLASH64K_TEST_AFE_SLOT_A        FLASH_ADDR_STORAGE_AFE_SLOT_A
#define FLASH64K_TEST_AFE_SLOT_B        FLASH_ADDR_STORAGE_AFE_SLOT_B

#ifndef FLASH64K_APP_QUICK_TEST_CYCLES
#define FLASH64K_APP_QUICK_TEST_CYCLES  ((UINT16)96)
#endif

#ifndef FLASH64K_APP_USE_TEST_PRINT_PERIOD_SEC
#define FLASH64K_APP_USE_TEST_PRINT_PERIOD_SEC ((UINT16)10)
#endif

#ifndef FLASH64K_APP_USE_TEST_ACCEL_SOC_PERIOD_SEC
#define FLASH64K_APP_USE_TEST_ACCEL_SOC_PERIOD_SEC ((UINT16)1)
#endif

#ifndef FLASH64K_APP_USE_TEST_ACCEL_AFE_PERIOD_SEC
#define FLASH64K_APP_USE_TEST_ACCEL_AFE_PERIOD_SEC ((UINT16)30)
#endif

#if defined(FLASH64K_APP_QUICK_TEST_ENABLE) || defined(FLASH64K_APP_USE_TEST_ENABLE)

static UINT16 Flash64K_AppTest_ReadFlashSizeKb(void)
{
	return *((volatile UINT16 *)FLASH64K_SIZE_REG_ADDR);
}

static UINT8 Flash64K_AppTest_SocEqual(const STORAGE_FLASH_SOC_DATA *a,
									   const STORAGE_FLASH_SOC_DATA *b)
{
	return (UINT8)((a->u16FormatVersion == b->u16FormatVersion) &&
				   (a->u16SocNow == b->u16SocNow) &&
				   (a->u16DsgSocInt == b->u16DsgSocInt) &&
				   (a->u16MaxErrorPercent == b->u16MaxErrorPercent) &&
				   (a->u32CycleTimes == b->u32CycleTimes) &&
				   (a->u32CapNow == b->u32CapNow) &&
				   (a->u32CapFull == b->u32CapFull) &&
				   (a->u32LearnPassedAs10 == b->u32LearnPassedAs10) &&
				   (a->u16LearnAnchorSoc == b->u16LearnAnchorSoc) &&
				   (a->u16LearnState == b->u16LearnState) &&
				   (a->u16Flags == b->u16Flags));
}

static UINT8 Flash64K_AppTest_AfeEqual(const UINT16 *a, const UINT16 *b)
{
	UINT16 i;

	for (i = 0; i < FLASH_STORAGE_AFE_WORD_COUNT; ++i)
	{
		if (a[i] != b[i])
		{
			return 0;
		}
	}

	return 1;
}

#endif

typedef enum
{
	FLASH64K_APP_TEST_PASS = 0,
	FLASH64K_APP_TEST_SAVE_SOC_FAIL,
	FLASH64K_APP_TEST_LOAD_SOC_FAIL,
	FLASH64K_APP_TEST_VERIFY_SOC_FAIL,
	FLASH64K_APP_TEST_SAVE_AFE_FAIL,
	FLASH64K_APP_TEST_LOAD_AFE_FAIL,
	FLASH64K_APP_TEST_VERIFY_AFE_FAIL,
	FLASH64K_APP_TEST_RESTORE_SOC_FAIL,
	FLASH64K_APP_TEST_RESTORE_AFE_FAIL
} FLASH64K_APP_TEST_STATUS;

#ifdef FLASH64K_APP_QUICK_TEST_ENABLE

static void Flash64K_AppTest_DelayOneSecond(void)
{
	UINT8 i;

	for (i = 0; i < 5; ++i)
	{
		Delay1ms(200);
	}
}

static void Flash64K_AppTest_FillSoc(STORAGE_FLASH_SOC_DATA *data, UINT16 cycle)
{
	memset(data, 0, sizeof(*data));
	data->u16FormatVersion = FLASH_STORAGE_SOC_DATA_VERSION_V2;
	data->u16SocNow = (UINT16)(cycle % 101U);
	data->u16DsgSocInt = (UINT16)((100U - (cycle % 101U)) % 101U);
	data->u16MaxErrorPercent = (UINT16)(100U - (cycle % 10U));
	data->u32CycleTimes = ((UINT32)0x5A5A0000U) | (UINT32)cycle;
	data->u32CapFull = 972000U;
	data->u32CapNow = ((UINT32)data->u16SocNow * data->u32CapFull) / 100U;
	data->u32LearnPassedAs10 = (UINT32)cycle * 37U;
	data->u16LearnAnchorSoc = (UINT16)(cycle % 101U);
	data->u16LearnState = (UINT16)(cycle % 3U);
	data->u16Flags = (UINT16)(cycle & 0x00FFU);
}

static void Flash64K_AppTest_FillAfe(UINT16 *values, UINT16 cycle)
{
	UINT16 i;

	for (i = 0; i < FLASH_STORAGE_AFE_WORD_COUNT; ++i)
	{
		values[i] = (UINT16)(0xA500U ^ (UINT16)(cycle * 37U) ^ (UINT16)(i * 0x0111U));
	}
}

static const char *Flash64K_AppTest_StatusText(FLASH64K_APP_TEST_STATUS status)
{
	switch (status)
	{
	case FLASH64K_APP_TEST_PASS:
		return "PASS";
	case FLASH64K_APP_TEST_SAVE_SOC_FAIL:
		return "SAVE_SOC_FAIL";
	case FLASH64K_APP_TEST_LOAD_SOC_FAIL:
		return "LOAD_SOC_FAIL";
	case FLASH64K_APP_TEST_VERIFY_SOC_FAIL:
		return "VERIFY_SOC_FAIL";
	case FLASH64K_APP_TEST_SAVE_AFE_FAIL:
		return "SAVE_AFE_FAIL";
	case FLASH64K_APP_TEST_LOAD_AFE_FAIL:
		return "LOAD_AFE_FAIL";
	case FLASH64K_APP_TEST_VERIFY_AFE_FAIL:
		return "VERIFY_AFE_FAIL";
	case FLASH64K_APP_TEST_RESTORE_SOC_FAIL:
		return "RESTORE_SOC_FAIL";
	case FLASH64K_APP_TEST_RESTORE_AFE_FAIL:
		return "RESTORE_AFE_FAIL";
	default:
		return "UNKNOWN";
	}
}

static void Flash64K_AppTest_Halt(FLASH64K_APP_TEST_STATUS status, UINT16 cycle)
{
	while (1)
	{
		printf("[FLASH64K_APP_TEST] result=%s cycle=%u\r\n",
			   Flash64K_AppTest_StatusText(status),
			   cycle);
		Flash64K_AppTest_DelayOneSecond();
	}
}

#endif

void StorageFlash_RunAppQuickTest(void)
{
#ifdef FLASH64K_APP_QUICK_TEST_ENABLE
	STORAGE_FLASH_SOC_DATA old_soc;
	STORAGE_FLASH_SOC_DATA write_soc;
	STORAGE_FLASH_SOC_DATA read_soc;
	UINT16 old_afe[FLASH_STORAGE_AFE_WORD_COUNT];
	UINT16 write_afe[FLASH_STORAGE_AFE_WORD_COUNT];
	UINT16 read_afe[FLASH_STORAGE_AFE_WORD_COUNT];
	UINT8 old_soc_valid;
	UINT8 old_afe_valid;
	UINT8 restore_soc_ok = 1U;
	UINT8 restore_afe_ok = 1U;
	UINT16 flash_size_kb;
	UINT16 cycle;
	FLASH64K_APP_TEST_STATUS status;
	FLASH64K_APP_TEST_STATUS test_status;

	status = FLASH64K_APP_TEST_PASS;
	old_soc_valid = StorageFlash_LoadSocData(&old_soc);
	old_afe_valid = StorageFlash_LoadAfeData(old_afe, FLASH_STORAGE_AFE_WORD_COUNT);
	flash_size_kb = Flash64K_AppTest_ReadFlashSizeKb();

	printf("\r\n[FLASH64K_APP_TEST] quick storage test start\r\n");
	printf("[FLASH64K_APP_TEST] flash_size_reg=%uKB cycles=%u page=%lu\r\n",
		   flash_size_kb,
		   (UINT16)FLASH64K_APP_QUICK_TEST_CYCLES,
		   (unsigned long)FLASH_STORAGE_PAGE_SIZE);
	printf("[FLASH64K_APP_TEST] AFE: 0x%08lX 0x%08lX, SOC: 0x%08lX 0x%08lX\r\n",
		   (unsigned long)FLASH64K_TEST_AFE_SLOT_A,
		   (unsigned long)FLASH64K_TEST_AFE_SLOT_B,
		   (unsigned long)FLASH64K_TEST_SOC_SLOT_A,
		   (unsigned long)FLASH64K_TEST_SOC_SLOT_B);
	printf("[FLASH64K_APP_TEST] destructive: SOC/AFE slots will be rewritten\r\n");

	for (cycle = 0; cycle < (UINT16)FLASH64K_APP_QUICK_TEST_CYCLES; ++cycle)
	{
		Flash64K_AppTest_FillSoc(&write_soc, cycle);
		if (!StorageFlash_SaveSocData(&write_soc))
		{
			status = FLASH64K_APP_TEST_SAVE_SOC_FAIL;
			break;
		}
		memset(&read_soc, 0, sizeof(read_soc));
		if (!StorageFlash_LoadSocData(&read_soc))
		{
			status = FLASH64K_APP_TEST_LOAD_SOC_FAIL;
			break;
		}
		if (!Flash64K_AppTest_SocEqual(&write_soc, &read_soc))
		{
			status = FLASH64K_APP_TEST_VERIFY_SOC_FAIL;
			break;
		}

		Flash64K_AppTest_FillAfe(write_afe, cycle);
		if (!StorageFlash_SaveAfeData(write_afe, FLASH_STORAGE_AFE_WORD_COUNT))
		{
			status = FLASH64K_APP_TEST_SAVE_AFE_FAIL;
			break;
		}
		memset(read_afe, 0, sizeof(read_afe));
		if (!StorageFlash_LoadAfeData(read_afe, FLASH_STORAGE_AFE_WORD_COUNT))
		{
			status = FLASH64K_APP_TEST_LOAD_AFE_FAIL;
			break;
		}
		if (!Flash64K_AppTest_AfeEqual(write_afe, read_afe))
		{
			status = FLASH64K_APP_TEST_VERIFY_AFE_FAIL;
			break;
		}

		if (((cycle + 1U) % 8U) == 0U)
		{
			printf("[FLASH64K_APP_TEST] cycle %u/%u ok\r\n",
				   (UINT16)(cycle + 1U),
				   (UINT16)FLASH64K_APP_QUICK_TEST_CYCLES);
		}
	}

	test_status = status;
	if (old_soc_valid && !StorageFlash_SaveSocData(&old_soc))
	{
		restore_soc_ok = 0U;
	}
	if (old_afe_valid && !StorageFlash_SaveAfeData(old_afe, FLASH_STORAGE_AFE_WORD_COUNT))
	{
		restore_afe_ok = 0U;
	}

	if ((test_status == FLASH64K_APP_TEST_PASS) && (restore_soc_ok == 0U))
	{
		status = FLASH64K_APP_TEST_RESTORE_SOC_FAIL;
	}
	else if ((test_status == FLASH64K_APP_TEST_PASS) && (restore_afe_ok == 0U))
	{
		status = FLASH64K_APP_TEST_RESTORE_AFE_FAIL;
	}

	printf("[FLASH64K_APP_TEST] restore old_soc=%u old_afe=%u soc_ok=%u afe_ok=%u\r\n",
		   old_soc_valid,
		   old_afe_valid,
		   restore_soc_ok,
		   restore_afe_ok);
	printf("[FLASH64K_APP_TEST] finished test=%s restore_soc=%u restore_afe=%u final=%s cycle=%u\r\n",
		   Flash64K_AppTest_StatusText(test_status),
		   restore_soc_ok,
		   restore_afe_ok,
		   Flash64K_AppTest_StatusText(status),
		   cycle);

	Flash64K_AppTest_Halt(status, cycle);
#endif
}

#ifdef FLASH64K_APP_USE_TEST_ENABLE

typedef struct
{
	UINT8 started;
	UINT16 print_sec;
	UINT32 soc_save_ok;
	UINT32 soc_save_fail;
	UINT32 soc_verify_ok;
	UINT32 soc_verify_fail;
	UINT32 afe_save_ok;
	UINT32 afe_save_fail;
	UINT32 afe_verify_ok;
	UINT32 afe_verify_fail;
	UINT16 last_soc_now;
	UINT16 last_dsg_soc_int;
	UINT32 last_cycle_times;
	UINT16 last_afe_first;
	UINT16 last_afe_last;
	UINT16 accel_soc_sec;
	UINT16 accel_afe_sec;
	UINT16 accel_soc_value;
	UINT8 accel_soc_down;
	UINT32 accel_soc_write;
	UINT32 accel_afe_write;
	UINT32 accel_afe_skip;
} FLASH64K_APP_USE_TEST_STATS;

static FLASH64K_APP_USE_TEST_STATS s_flash64k_use_test;

static void Flash64K_AppUseTest_PrintStart(void)
{
	if (s_flash64k_use_test.started != 0U)
	{
		return;
	}

	s_flash64k_use_test.started = 1U;
	printf("\r\n[FLASH64K_USE_TEST] start\r\n");
	printf("[FLASH64K_USE_TEST] flash_size_reg=%uKB page=%lu print=%us\r\n",
		   Flash64K_AppTest_ReadFlashSizeKb(),
		   (unsigned long)FLASH_STORAGE_PAGE_SIZE,
		   (UINT16)FLASH64K_APP_USE_TEST_PRINT_PERIOD_SEC);
#ifdef FLASH64K_APP_USE_TEST_ACCEL_ENABLE
	printf("[FLASH64K_USE_TEST] accel on: soc=%us afe=%us\r\n",
		   (UINT16)FLASH64K_APP_USE_TEST_ACCEL_SOC_PERIOD_SEC,
		   (UINT16)FLASH64K_APP_USE_TEST_ACCEL_AFE_PERIOD_SEC);
#endif
	printf("[FLASH64K_USE_TEST] monitor real SOC/AFE save->load verify while app is running\r\n");
}

#ifdef FLASH64K_APP_USE_TEST_ACCEL_ENABLE

static void Flash64K_AppUseTest_MoveSocWave(void)
{
	if (s_flash64k_use_test.accel_soc_down != 0U)
	{
		if (s_flash64k_use_test.accel_soc_value > 0U)
		{
			s_flash64k_use_test.accel_soc_value--;
		}
		else
		{
			s_flash64k_use_test.accel_soc_down = 0U;
			s_flash64k_use_test.accel_soc_value++;
		}
	}
	else
	{
		if (s_flash64k_use_test.accel_soc_value < 100U)
		{
			s_flash64k_use_test.accel_soc_value++;
		}
		else
		{
			s_flash64k_use_test.accel_soc_down = 1U;
			s_flash64k_use_test.accel_soc_value--;
		}
	}
}

static void Flash64K_AppUseTest_AccelSocSave(void)
{
	STORAGE_FLASH_SOC_DATA data;

	memset(&data, 0, sizeof(data));
	data.u16FormatVersion = FLASH_STORAGE_SOC_DATA_VERSION_V2;
	data.u16SocNow = s_flash64k_use_test.accel_soc_value;
	data.u16DsgSocInt = s_flash64k_use_test.accel_soc_value;
	data.u16MaxErrorPercent = 100U;
	data.u32CycleTimes = s_flash64k_use_test.accel_soc_write;
	data.u32CapFull = 972000U;
	data.u32CapNow = ((UINT32)data.u16SocNow * data.u32CapFull) / 100U;

	s_flash64k_use_test.accel_soc_write++;
	(void)StorageFlash_SaveSocData(&data);
	Flash64K_AppUseTest_MoveSocWave();
}

static void Flash64K_AppUseTest_AccelAfeSave(void)
{
	UINT16 values[FLASH_STORAGE_AFE_WORD_COUNT];

	if (StorageFlash_LoadAfeData(values, FLASH_STORAGE_AFE_WORD_COUNT))
	{
		s_flash64k_use_test.accel_afe_write++;
		(void)StorageFlash_SaveAfeData(values, FLASH_STORAGE_AFE_WORD_COUNT);
	}
	else
	{
		s_flash64k_use_test.accel_afe_skip++;
	}
}

static void Flash64K_AppUseTest_AccelTask(void)
{
	if (++s_flash64k_use_test.accel_soc_sec >= (UINT16)FLASH64K_APP_USE_TEST_ACCEL_SOC_PERIOD_SEC)
	{
		s_flash64k_use_test.accel_soc_sec = 0U;
		Flash64K_AppUseTest_AccelSocSave();
	}

	if (++s_flash64k_use_test.accel_afe_sec >= (UINT16)FLASH64K_APP_USE_TEST_ACCEL_AFE_PERIOD_SEC)
	{
		s_flash64k_use_test.accel_afe_sec = 0U;
		Flash64K_AppUseTest_AccelAfeSave();
	}
}

#endif

#endif

void StorageFlash_AppUseTest_OnSocSaved(const STORAGE_FLASH_SOC_DATA *expect, UINT8 save_ok)
{
#ifdef FLASH64K_APP_USE_TEST_ENABLE
	STORAGE_FLASH_SOC_DATA actual;

	Flash64K_AppUseTest_PrintStart();
	if ((expect == 0) || (save_ok == 0U))
	{
		s_flash64k_use_test.soc_save_fail++;
		return;
	}

	s_flash64k_use_test.soc_save_ok++;
	if (!StorageFlash_LoadSocData(&actual))
	{
		s_flash64k_use_test.soc_verify_fail++;
		return;
	}

	if (!Flash64K_AppTest_SocEqual(expect, &actual))
	{
		s_flash64k_use_test.soc_verify_fail++;
		printf("[FLASH64K_USE_TEST] SOC verify fail exp=%u/%u/%lu got=%u/%u/%lu\r\n",
			   expect->u16SocNow,
			   expect->u16DsgSocInt,
			   (unsigned long)expect->u32CycleTimes,
			   actual.u16SocNow,
			   actual.u16DsgSocInt,
			   (unsigned long)actual.u32CycleTimes);
		return;
	}

	s_flash64k_use_test.soc_verify_ok++;
	s_flash64k_use_test.last_soc_now = actual.u16SocNow;
	s_flash64k_use_test.last_dsg_soc_int = actual.u16DsgSocInt;
	s_flash64k_use_test.last_cycle_times = actual.u32CycleTimes;
#else
	(void)expect;
	(void)save_ok;
#endif
}

void StorageFlash_AppUseTest_OnAfeSaved(const UINT16 *expect, UINT16 word_count, UINT8 save_ok)
{
#ifdef FLASH64K_APP_USE_TEST_ENABLE
	UINT16 actual[FLASH_STORAGE_AFE_WORD_COUNT];

	Flash64K_AppUseTest_PrintStart();
	if ((expect == 0) || (word_count != FLASH_STORAGE_AFE_WORD_COUNT) || (save_ok == 0U))
	{
		s_flash64k_use_test.afe_save_fail++;
		return;
	}

	s_flash64k_use_test.afe_save_ok++;
	if (!StorageFlash_LoadAfeData(actual, FLASH_STORAGE_AFE_WORD_COUNT))
	{
		s_flash64k_use_test.afe_verify_fail++;
		return;
	}

	if (!Flash64K_AppTest_AfeEqual(expect, actual))
	{
		s_flash64k_use_test.afe_verify_fail++;
		printf("[FLASH64K_USE_TEST] AFE verify fail exp0=0x%04X got0=0x%04X\r\n",
			   expect[0],
			   actual[0]);
		return;
	}

	s_flash64k_use_test.afe_verify_ok++;
	s_flash64k_use_test.last_afe_first = actual[0];
	s_flash64k_use_test.last_afe_last = actual[FLASH_STORAGE_AFE_WORD_COUNT - 1U];
#else
	(void)expect;
	(void)word_count;
	(void)save_ok;
#endif
}

void StorageFlash_AppUseTest_Task(void)
{
#ifdef FLASH64K_APP_USE_TEST_ENABLE
	Flash64K_AppUseTest_PrintStart();
	if (g_st_SysTimeFlag.bits.b1Sys1000msFlag == 0U)
	{
		return;
	}

#ifdef FLASH64K_APP_USE_TEST_ACCEL_ENABLE
	Flash64K_AppUseTest_AccelTask();
#endif

	if (++s_flash64k_use_test.print_sec < (UINT16)FLASH64K_APP_USE_TEST_PRINT_PERIOD_SEC)
	{
		return;
	}
	s_flash64k_use_test.print_sec = 0U;

	printf("[FLASH64K_USE_TEST] SOC save=%lu fail=%lu verify=%lu vfail=%lu last=%u/%u/%lu\r\n",
		   (unsigned long)s_flash64k_use_test.soc_save_ok,
		   (unsigned long)s_flash64k_use_test.soc_save_fail,
		   (unsigned long)s_flash64k_use_test.soc_verify_ok,
		   (unsigned long)s_flash64k_use_test.soc_verify_fail,
		   s_flash64k_use_test.last_soc_now,
		   s_flash64k_use_test.last_dsg_soc_int,
		   (unsigned long)s_flash64k_use_test.last_cycle_times);
	printf("[FLASH64K_USE_TEST] AFE save=%lu fail=%lu verify=%lu vfail=%lu last=0x%04X/0x%04X\r\n",
		   (unsigned long)s_flash64k_use_test.afe_save_ok,
		   (unsigned long)s_flash64k_use_test.afe_save_fail,
		   (unsigned long)s_flash64k_use_test.afe_verify_ok,
		   (unsigned long)s_flash64k_use_test.afe_verify_fail,
		   s_flash64k_use_test.last_afe_first,
		   s_flash64k_use_test.last_afe_last);
#ifdef FLASH64K_APP_USE_TEST_ACCEL_ENABLE
	printf("[FLASH64K_USE_TEST] accel soc_write=%lu afe_write=%lu afe_skip=%lu soc_now=%u\r\n",
		   (unsigned long)s_flash64k_use_test.accel_soc_write,
		   (unsigned long)s_flash64k_use_test.accel_afe_write,
		   (unsigned long)s_flash64k_use_test.accel_afe_skip,
		   s_flash64k_use_test.accel_soc_value);
#endif
#endif
}
