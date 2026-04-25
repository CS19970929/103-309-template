#include "main.h"
#include "Flash64KAppTest.h"

#ifdef FLASH64K_APP_QUICK_TEST_ENABLE

#define FLASH64K_SIZE_REG_ADDR          ((UINT32)0x1FFFF7E0)
#define FLASH64K_TEST_SOC_SLOT_A        FLASH_ADDR_STORAGE_SOC_SLOT_A
#define FLASH64K_TEST_SOC_SLOT_B        FLASH_ADDR_STORAGE_SOC_SLOT_B
#define FLASH64K_TEST_AFE_SLOT_A        FLASH_ADDR_STORAGE_AFE_SLOT_A
#define FLASH64K_TEST_AFE_SLOT_B        FLASH_ADDR_STORAGE_AFE_SLOT_B

#ifndef FLASH64K_APP_QUICK_TEST_CYCLES
#define FLASH64K_APP_QUICK_TEST_CYCLES  ((UINT16)96)
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

static UINT16 Flash64K_AppTest_ReadFlashSizeKb(void)
{
	return *((volatile UINT16 *)FLASH64K_SIZE_REG_ADDR);
}

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
	data->u16SocNow = (UINT16)(cycle % 101U);
	data->u16DsgSocInt = (UINT16)((100U - (cycle % 101U)) % 101U);
	data->u32CycleTimes = ((UINT32)0x5A5A0000U) | (UINT32)cycle;
}

static void Flash64K_AppTest_FillAfe(UINT16 *values, UINT16 cycle)
{
	UINT16 i;

	for (i = 0; i < FLASH_STORAGE_AFE_WORD_COUNT; ++i)
	{
		values[i] = (UINT16)(0xA500U ^ (UINT16)(cycle * 37U) ^ (UINT16)(i * 0x0111U));
	}
}

static UINT8 Flash64K_AppTest_SocEqual(const STORAGE_FLASH_SOC_DATA *a,
									   const STORAGE_FLASH_SOC_DATA *b)
{
	return (UINT8)((a->u16SocNow == b->u16SocNow) &&
				   (a->u16DsgSocInt == b->u16DsgSocInt) &&
				   (a->u32CycleTimes == b->u32CycleTimes));
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
	UINT16 flash_size_kb;
	UINT16 cycle;
	FLASH64K_APP_TEST_STATUS status;

	status = FLASH64K_APP_TEST_PASS;
	old_soc_valid = StorageFlash_LoadSocData(&old_soc);
	old_afe_valid = StorageFlash_LoadAfeData(old_afe, FLASH_STORAGE_AFE_WORD_COUNT);
	flash_size_kb = Flash64K_AppTest_ReadFlashSizeKb();

	printf("\r\n[FLASH64K_APP_TEST] quick storage test start\r\n");
	printf("[FLASH64K_APP_TEST] flash_size_reg=%uKB cycles=%u\r\n",
		   flash_size_kb,
		   (UINT16)FLASH64K_APP_QUICK_TEST_CYCLES);
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

	if (old_soc_valid)
	{
		if (!StorageFlash_SaveSocData(&old_soc))
		{
			status = FLASH64K_APP_TEST_RESTORE_SOC_FAIL;
		}
	}
	if (old_afe_valid)
	{
		if (!StorageFlash_SaveAfeData(old_afe, FLASH_STORAGE_AFE_WORD_COUNT))
		{
			status = FLASH64K_APP_TEST_RESTORE_AFE_FAIL;
		}
	}

	printf("[FLASH64K_APP_TEST] restore old_soc=%u old_afe=%u\r\n",
		   old_soc_valid,
		   old_afe_valid);
	printf("[FLASH64K_APP_TEST] finished result=%s cycle=%u\r\n",
		   Flash64K_AppTest_StatusText(status),
		   cycle);

	Flash64K_AppTest_Halt(status, cycle);
#endif
}
