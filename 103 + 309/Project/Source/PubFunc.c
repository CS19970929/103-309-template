#include "main.h"

/*=================================================================
 * FUNCTION: GetEndValue
 * PURPOSE : 查表
 * INPUT:    UINT16
 *
 * RETURN:   UINT16
 *
 * CALLS:    void
 *
 * CALLED BY:Adc_AnlogCal()
 *
 *=================================================================*/
UINT16 GetEndValue(const UINT16 *ptbl, UINT16 tblsize, UINT16 dat)
{
	UINT16 i, t_linenum;
	UINT32 x1 = 0, y1 = 0, x2 = 1, y2 = 1;
	const UINT16 *p;
	UINT16 t_tmp16a, t_tmp16b;
	INT32 t_tmp32a, t_tmp32b;
	UINT32 k, b;
	INT32 ret;
	p = ptbl;

	t_linenum = tblsize - 1;
	for (i = 0; i < tblsize - 2; i = i + 2)
	{
		t_tmp16a = p[i];
		t_tmp16b = p[i + 2];

		if (((dat >= t_tmp16a) && (dat <= t_tmp16b)) || ((dat <= t_tmp16a) && (dat >= t_tmp16b)))
		{
			x1 = t_tmp16a;
			x2 = t_tmp16b;
			y1 = p[i + 1];
			y2 = p[i + 3];
			break;
		}
	}

	if (i >= t_linenum - 1)
	{
		p = ptbl;
		t_tmp16a = p[0];
		t_tmp16b = p[tblsize - 2];

		if (t_tmp16a <= t_tmp16b)
		{
			if (dat >= t_tmp16b)
			{
				t_tmp16a = p[tblsize - 1];
			}
			else
			{
				t_tmp16a = p[1];
			}
		}
		else
		{
			if (dat >= t_tmp16a)
			{
				t_tmp16a = p[1];
			}
			else
			{
				t_tmp16a = p[tblsize - 1];
			}
		}
		return t_tmp16a;
	}
	else
	{
		if (x2 < x1)
		{
			ret = x2;
			x2 = x1;
			x1 = ret;
			ret = y2;
			y2 = y1;
			y1 = ret;
		}

		if (y2 >= y1)
		{
			t_tmp32a = y1 * x2;
			t_tmp32b = y2 * x1;
			ret = dat;
			k = y2 - y1;
			ret = ret * k;
			if (t_tmp32a >= t_tmp32b)
			{
				b = t_tmp32a - t_tmp32b;
				ret = ret + b;
			}
			else
			{
				b = t_tmp32b - t_tmp32a;
				ret = ret - b;
			}
			ret = ret / (x2 - x1);
		}
		else
		{
			t_tmp32a = y1 * x2;
			t_tmp32b = y2 * x1;
			ret = dat;
			k = y1 - y2;
			ret = ret * k;
			b = t_tmp32a - t_tmp32b;
			ret = b - ret;
			ret = ret / (x2 - x1);
		}
		return (ret & 0xffff);
	}
}

UINT16 Sci_CRC16RTU(UINT8 *pszBuf, UINT8 unLength)
{
	UINT16 CRCC = 0XFFFF;
	UINT32 CRC_count;

	for (CRC_count = 0; CRC_count < unLength; CRC_count++)
	{
		int i;

		CRCC = CRCC ^ *(pszBuf + CRC_count);

		for (i = 0; i < 8; i++)
		{
			if (CRCC & 1)
			{
				CRCC >>= 1;
				CRCC ^= 0xA001;
			}
			else
			{
				CRCC >>= 1;
			}
		}
	}

	return CRCC;
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
