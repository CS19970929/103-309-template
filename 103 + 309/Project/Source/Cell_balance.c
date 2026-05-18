#include "main.h"

#define CB_BALANCE_REG_RETRY_MAX ((UINT8)3)
#define CB_BALANCE_REG_BYTES     ((UINT8)3)
#define CB_BALANCE_CELL_MAX      ((UINT8)20)
#define CB_BALANCE_FILTER_CNT    ((UINT8)3)
#define CB_BALANCE_REFRESH_CNT   ((UINT8)3)
#define CB_BALANCE_CURRENT_LIMIT ((UINT16)10)

enum BALANCE_STATE_E g_enBalanceState = BALANCE_ST_OFF;

UINT16 g_u16CBnFLAG_ToUpper = 0;
UINT8 g_u8CBn_StatusFlag = 0;
UINT8 g_u8CBn_AFECloseFlag = 1;

static UINT8 CB_GetCellCount(void)
{
	if (SeriesNum > CB_BALANCE_CELL_MAX)
	{
		return CB_BALANCE_CELL_MAX;
	}

	return SeriesNum;
}

static UINT32 CB_GetCellMask(UINT8 cell_index)
{
	if (cell_index >= CB_BALANCE_CELL_MAX)
	{
		return 0;
	}

	return ((UINT32)1u << cell_index);
}

static UINT8 CB_AfeWriteBalanceMaskU24(UINT32 balance_mask)
{
	UINT8 retry;
	UINT8 balance_h = (UINT8)((balance_mask >> 16) & 0xFFu);
	UINT8 balance_m = (UINT8)((balance_mask >> 8) & 0xFFu);
	UINT8 balance_l = (UINT8)(balance_mask & 0xFFu);

	for (retry = 0; retry < CB_BALANCE_REG_RETRY_MAX; ++retry)
	{
		if (sh36735_write_reg_u8(AFE_BALANCEH, balance_h)
			&& sh36735_write_reg_u8(AFE_BALANCEM, balance_m)
			&& sh36735_write_reg_u8(AFE_BALANCEL, balance_l))
		{
			System_ERROR_UserCallback(ERROR_REMOVE_SPI);
			return 0;
		}

		Delay1ms(1);
	}

	System_ERROR_UserCallback(ERROR_SPI);
	return 1;
}

static void CB_UpdateSoftwareStatus(UINT32 active_mask)
{
	UINT8 i;
	UINT8 cell_count = CB_GetCellCount();
	UINT32 valid_mask = 0;

	if (cell_count > 0)
	{
		valid_mask = ((UINT32)1u << cell_count) - 1u;
	}

	active_mask &= valid_mask;
	g_stCellInfoReport.balance_status = active_mask;
	g_stCellInfoReport.u16BalanceFlag1 = (UINT16)(active_mask & 0xFFFFu);
	g_stCellInfoReport.u16BalanceFlag2 = (UINT16)((active_mask >> 16) & 0xFFFFu);
	g_u16CBnFLAG_ToUpper = g_stCellInfoReport.u16BalanceFlag1;
	g_u8CBn_StatusFlag = (active_mask != 0u) ? 1u : 0u;
	g_u8CBn_AFECloseFlag = (active_mask == 0u) ? 1u : 0u;
	g_enBalanceState = (active_mask != 0u) ? BALANCE_ST_ODD_ON : BALANCE_ST_OFF;

	for (i = 0; i < CB_BALANCE_CELL_MAX; ++i)
	{
		sys_time.bal_cell[i] = ((i < cell_count) && ((active_mask & CB_GetCellMask(i)) != 0u)) ? true : false;
	}
}

static UINT8 CB_IsBalanceAllowed(void)
{
	if (0 == System_OnOFF_Func.bits.b1OnOFF_Balance)
	{
		return 0;
	}

	if ((g_stCellInfoReport.u16Ichg > CB_BALANCE_CURRENT_LIMIT)
		|| (g_stCellInfoReport.u16IDischg > CB_BALANCE_CURRENT_LIMIT))
	{
		return 0;
	}

	if (g_stCellInfoReport.u16VCellMin < OtherElement.u16Balance_OpenVoltage)
	{
		return 0;
	}

	if (g_stCellInfoReport.u16VCellDelta < OtherElement.u16Balance_OpenWindow)
	{
		return 0;
	}

	return 1;
}

static UINT32 CB_BuildTargetMask(UINT8 filter_cnt[])
{
	UINT8 i;
	UINT8 cell_count = CB_GetCellCount();
	UINT16 vcell_min = g_stCellInfoReport.u16VCellMin;
	UINT32 target_mask = 0;

	for (i = 0; i < cell_count; ++i)
	{
		if ((g_stCellInfoReport.u16VCell[i] >= OtherElement.u16Balance_OpenVoltage)
			&& (g_stCellInfoReport.u16VCell[i] >= (UINT16)(vcell_min + OtherElement.u16Balance_OpenWindow)))
		{
			if (filter_cnt[i] < CB_BALANCE_FILTER_CNT)
			{
				++filter_cnt[i];
			}

			if (filter_cnt[i] >= CB_BALANCE_FILTER_CNT)
			{
				target_mask |= CB_GetCellMask(i);
			}
		}
		else
		{
			filter_cnt[i] = 0;
		}
	}

	for (; i < CB_BALANCE_CELL_MAX; ++i)
	{
		filter_cnt[i] = 0;
	}

	return target_mask;
}

void App_CellBalance(void)
{
	UINT8 i;
	UINT32 target_mask;
	static UINT32 active_mask = 0;
	static UINT8 refresh_cnt = 0;
	static UINT8 filter_cnt[CB_BALANCE_CELL_MAX] = {0};

	if (0 == g_st_SysTimeFlag.bits.b1Sys1000msFlag2)
	{
		return;
	}

	if (0 == CB_IsBalanceAllowed())
	{
		for (i = 0; i < CB_BALANCE_CELL_MAX; ++i)
		{
			filter_cnt[i] = 0;
		}
		refresh_cnt = 0;
		target_mask = 0;
	}
	else
	{
		target_mask = CB_BuildTargetMask(filter_cnt);
	}

	if ((target_mask != active_mask)
		|| ((active_mask != 0u) && (++refresh_cnt >= CB_BALANCE_REFRESH_CNT)))
	{
		refresh_cnt = 0;
		if (0 == CB_AfeWriteBalanceMaskU24(target_mask))
		{
			active_mask = target_mask;
			CB_UpdateSoftwareStatus(active_mask);
		}
	}
	else
	{
		CB_UpdateSoftwareStatus(active_mask);
	}
}
