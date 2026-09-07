#include "main.h"

UINT16 Sci_CRC16RTU(UINT8 *pszBuf, UINT8 unLength)
{
    /* Same CRC-16/MODBUS polynomial, initial value and byte order as storage. */
    return StorageFlash_Crc16(pszBuf, unLength);
}

/*
 * Millisecond delay used by AFE setup/write sequences.
 *
 * Do not implement protocol timing with empty software loops: ARMCC5 -O2 may
 * legally collapse or remove them. System_Init.c provides a SysTick-backed
 * delay whose observable peripheral accesses keep the timing independent of
 * compiler optimization level.
 */
void Delay1ms(UINT8 delaycnt)
{
	if (delaycnt != 0U)
	{
		__delay_ms((UINT16)delaycnt);
	}
}

// 1:有温度断线，0：正常
// 有问题，接好温度线，重启BMS才能消除报错
// 添加自动复原功能，不需要重启
UINT8 Monitor_TempBreak(UINT16 *temp_AD)
{
	static UINT8 su8_Recover_Cnt = 0;
	static UINT8 su8_StartUp_Flag = 0;
	static UINT8 su8_Delay_Cnt = 0;
	UINT8 result = 0;

	if (sys_time.wakeup_rtc)
	{
		sys_time.wakeup_rtc = false;

		su8_StartUp_Flag = 0;
		su8_Delay_Cnt = 0;
	}

	switch (su8_StartUp_Flag)
	{
	case 0: // 刚开机，不能判断，因为查询AFE函数已经被分割，不能拿到数据，此时判断必为错
		if (++su8_Delay_Cnt >= 20)
		{
			su8_Delay_Cnt = 0;
			su8_StartUp_Flag = 1;
		}
		break;

	case 1:
		if (*temp_AD < 110)
		{
			++result;
			*temp_AD = 110; // 定死在-29摄氏度。以防上位机显示NA以为没问题
			System_ERROR_UserCallback(ERROR_TEMP_BREAK);
			su8_Recover_Cnt = 0;
		}
		else
		{
			if (System_ERROR_UserCallback(ERROR_STATUS_TEMP_BREAK))
			{
				if (++su8_Recover_Cnt >= 50)
				{ // 判断50次自动复原，约为200*50=10s
					su8_Recover_Cnt = 0;
					System_ERROR_UserCallback(ERROR_REMOVE_TEMP_BREAK);
				}
			}
		}
		break;

	default:
		su8_StartUp_Flag = 0;
		break;
	}

	return result;
}

void jtag_disableAndConfIO(void)
{
	/* Disable JTAG while retaining SWD, freeing PA15/PB3/PB4 for GPIO use. */
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA | RCC_APB2Periph_GPIOB, ENABLE);
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_AFIO, ENABLE);
	GPIO_PinRemapConfig(GPIO_Remap_SWJ_JTAGDisable, ENABLE);
}
