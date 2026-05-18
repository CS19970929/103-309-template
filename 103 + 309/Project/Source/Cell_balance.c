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

static UINT32 s_u32CB_ActiveMask = 0;
static UINT8 s_u8CB_RefreshCnt = 0;
static UINT8 s_u8CB_FilterCnt[CB_BALANCE_CELL_MAX] = {0};

static UINT16 CB_GetBalanceCloseWindow(void);

static UINT8 CB_GetCellCount(void)
{
	if (SeriesNum > CB_BALANCE_CELL_MAX)
	{
		return CB_BALANCE_CELL_MAX;
	}

	return SeriesNum;
}

static void CB_ResetFilter(void)
{
	UINT8 i;

	for (i = 0; i < CB_BALANCE_CELL_MAX; ++i)
	{
		s_u8CB_FilterCnt[i] = 0;
	}
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
	UINT8 cell_count = CB_GetCellCount();
	UINT16 balance_window = (s_u32CB_ActiveMask != 0u) ? CB_GetBalanceCloseWindow() : OtherElement.u16Balance_OpenWindow;

	if (0 == System_OnOFF_Func.bits.b1OnOFF_Balance)
	{
		return 0;
	}

	if (0 == cell_count)
	{
		return 0;
	}

	if ((g_stCellInfoReport.u16VCellMin < 1000u)
		|| (g_stCellInfoReport.u16VCellMin > 5000u))
	{
		return 0;
	}

	if (g_stCellInfoReport.unMdlFault_Third.all != 0u)
	{
		return 0;
	}

	if (System_ERROR_UserCallback(ERROR_STATUS_AFE1)
		|| System_ERROR_UserCallback(ERROR_STATUS_SPI)
		|| System_ERROR_UserCallback(ERROR_STATUS_CBC_DSG))
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

	if (g_stCellInfoReport.u16VCellDelta < balance_window)
	{
		return 0;
	}

	return 1;
}

static UINT16 CB_GetBalanceCloseWindow(void)
{
	UINT16 close_window = OtherElement.u16Balance_CloseWindow;

	if ((0u == close_window) || (close_window > OtherElement.u16Balance_OpenWindow))
	{
		close_window = OtherElement.u16Balance_OpenWindow;
	}

	return close_window;
}

static UINT32 CB_BuildTargetMask(UINT32 active_mask)
{
	UINT8 i;
	UINT8 cell_count = CB_GetCellCount();
	UINT16 vcell_min = g_stCellInfoReport.u16VCellMin;
	UINT16 close_window = CB_GetBalanceCloseWindow();
	UINT32 target_mask = 0;
	UINT32 cell_mask;
	UINT16 balance_window;

	for (i = 0; i < cell_count; ++i)
	{
		cell_mask = CB_GetCellMask(i);
		balance_window = ((active_mask & cell_mask) != 0u) ? close_window : OtherElement.u16Balance_OpenWindow;

		if ((g_stCellInfoReport.u16VCell[i] >= OtherElement.u16Balance_OpenVoltage)
			&& ((UINT32)g_stCellInfoReport.u16VCell[i] >= ((UINT32)vcell_min + balance_window)))
		{
			if ((active_mask & cell_mask) != 0u)
			{
				target_mask |= cell_mask;
			}
			else if (s_u8CB_FilterCnt[i] < CB_BALANCE_FILTER_CNT)
			{
				++s_u8CB_FilterCnt[i];
			}

			if (s_u8CB_FilterCnt[i] >= CB_BALANCE_FILTER_CNT)
			{
				target_mask |= cell_mask;
			}
		}
		else
		{
			s_u8CB_FilterCnt[i] = 0;
		}
	}

	for (; i < CB_BALANCE_CELL_MAX; ++i)
	{
		s_u8CB_FilterCnt[i] = 0;
	}

	return target_mask;
}

UINT8 CellBalance_ForceOff(void)
{
	UINT8 result;

	result = CB_AfeWriteBalanceMaskU24(0u);
	if (0 == result)
	{
		CB_ResetFilter();
		s_u8CB_RefreshCnt = 0;
		s_u32CB_ActiveMask = 0;
		CB_UpdateSoftwareStatus(0u);
	}

	return result;
}

void App_CellBalance(void)
{
	UINT32 target_mask;

	if (0 == g_st_SysTimeFlag.bits.b1Sys1000msFlag2)
	{
		return;
	}

	if (0 == CB_IsBalanceAllowed())
	{
		CB_ResetFilter();
		s_u8CB_RefreshCnt = 0;
		target_mask = 0;
	}
	else
	{
		target_mask = CB_BuildTargetMask(s_u32CB_ActiveMask);
	}

	if ((target_mask != s_u32CB_ActiveMask)
		|| ((s_u32CB_ActiveMask != 0u) && (++s_u8CB_RefreshCnt >= CB_BALANCE_REFRESH_CNT)))
	{
		s_u8CB_RefreshCnt = 0;
		if (0 == CB_AfeWriteBalanceMaskU24(target_mask))
		{
			s_u32CB_ActiveMask = target_mask;
			CB_UpdateSoftwareStatus(s_u32CB_ActiveMask);
		}
	}
	else
	{
		CB_UpdateSoftwareStatus(s_u32CB_ActiveMask);
	}
}
