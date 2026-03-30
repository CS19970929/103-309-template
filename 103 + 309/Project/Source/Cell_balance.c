#include "main.h"

#define CB_BALANCE_REG_RETRY_MAX ((UINT8)3)
#define CB_BALANCE_REG_BYTES ((UINT8)3)

#define CB_BALANCE_REG_RETRY_MAX ((UINT8)3)
#define CB_BALANCE_CURRENT_LIMIT ((UINT16)10)
#define CB_BALANCE_REST_DELAY_S ((UINT16)(5 * 60))

enum BALANCE_STATE_E g_enBalanceState = BALANCE_ST_INIT;
enum CELL_BALANCE_STATUS_E g_enCellBalanceStatus[CELL_NUMS_MAX];
UINT8 g_u8CellBalanceFilterCnt[CELL_NUMS_MAX];

UINT16 g_u16CBnFLAG_ToUpper = 0;
UINT8 g_u8CBn_StatusFlag = 0;
UINT8 g_u8CBn_AFECloseFlag = 1;

static UINT16 s_u16BalanceCandidateMask = 0;
static UINT16 s_u16BalanceActiveMask = 0;

static UINT8 CB_GetCellCount(void)
{
	if (SeriesNum > CELL_NUMS_MAX)
	{
		return CELL_NUMS_MAX;
	}

	return SeriesNum;
}

static UINT16 CB_GetCellBit(UINT8 cell_index)
{
	if (cell_index >= CELL_NUMS_MAX)
	{
		return 0;
	}

	return (UINT16)(1u << cell_index);
}

static UINT8 CB_AfeReadBalanceMaskU24(uint32_t *balance_mask)
{
	UINT8 retry;
	UINT8 balance_regs[CB_BALANCE_REG_BYTES] = {0};

	if (NULL == balance_mask)
	{
		return 1;
	}

	for (retry = 0; retry < CB_BALANCE_REG_RETRY_MAX; ++retry)
	{
		if (sh36735_read_regs(AFE_BALANCEH, balance_regs, CB_BALANCE_REG_BYTES))
		{
			*balance_mask = ((uint32_t)balance_regs[0] << 16) | ((uint32_t)balance_regs[1] << 8) | (uint32_t)balance_regs[2];
			return 0;
		}
		Delay1ms(1);
	}

	*balance_mask = 0;
	return 1;
}

static UINT8 CB_AfeWriteBalanceMaskU24(UINT32 balance_mask)
{
	UINT8 retry;
	UINT8 balance_h = (UINT8)((balance_mask >> 16) & 0xFF);
	UINT8 balance_m = (UINT8)((balance_mask >> 8) & 0xFF);
	UINT8 balance_l = (UINT8)(balance_mask & 0xFF);

	// for (retry = 0; retry < CB_BALANCE_REG_RETRY_MAX; ++retry)
	// {
	// 	if (sh36735_write_reg_u8(AFE_BALANCEH, balance_h)
	// 		&& sh36735_write_reg_u8(AFE_BALANCEM, balance_m)
	// 		&& sh36735_write_reg_u8(AFE_BALANCEL, balance_l))
	// 	{
	// 		return 0;
	// 	}

	// 	Delay1ms(1);
	// }
	sh36735_write_reg_u8(AFE_BALANCEH, balance_h);
	sh36735_write_reg_u8(AFE_BALANCEM, balance_m);
	sh36735_write_reg_u8(AFE_BALANCEL, balance_l);

	return 1;
}

static void CB_UpdateDebugInfo(UINT16 active_mask)
{
	g_stCellInfoReport.u16BalanceFlag1 = active_mask;
	g_stCellInfoReport.u16BalanceFlag2 = 0;
	g_u16CBnFLAG_ToUpper = s_u16BalanceCandidateMask;
}

static UINT8 CB_ApplyBalanceMask(UINT16 active_mask)
{
	if (0 != CB_AfeWriteBalanceMaskU24((UINT32)active_mask))
	{
		return 1;
	}

	s_u16BalanceActiveMask = active_mask;
	g_u8CBn_StatusFlag = (active_mask != 0) ? 1 : 0;
	g_u8CBn_AFECloseFlag = (active_mask == 0) ? 1 : 0;
	CB_UpdateDebugInfo(active_mask);
	return 0;
}

static UINT8 CB_ApplyCandidateMask(enum CELL_BALANCE_FLAG_UPPER balance_flag)
{
	UINT16 apply_mask = 0;

	switch (balance_flag)
	{
	case CELL_BALANCE_ON_ODD:
		apply_mask = (UINT16)(s_u16BalanceCandidateMask & ODD_SELECT);
		break;

	case CELL_BALANCE_ON_EVEN:
		apply_mask = (UINT16)(s_u16BalanceCandidateMask & EVEN_SELECT);
		break;

	case CELL_BALANCE_COLSE:
	default:
		apply_mask = 0;
		break;
	}

	return CB_ApplyBalanceMask(apply_mask);
}

static UINT8 CB_IsBalanceAllowed(UINT8 onoff_ctrl)
{
	if (0 == onoff_ctrl)
	{
		return 0;
	}

	if ((g_stCellInfoReport.u16Ichg > CB_BALANCE_CURRENT_LIMIT) || (g_stCellInfoReport.u16IDischg > CB_BALANCE_CURRENT_LIMIT))
	{
		return 0;
	}

	if (g_stCellInfoReport.u16VCellMax < OtherElement.u16Balance_OpenVoltage)
	{
		return 0;
	}

	if (g_stCellInfoReport.u16VCellDelta < OtherElement.u16Balance_CloseWindow)
	{
		return 0;
	}

	return 1;
}

static void CB_RebuildCandidateMask(void)
{
	UINT8 i;
	UINT8 cell_count = CB_GetCellCount();
	UINT8 changed = 0;
	UINT16 vcell_min = g_stCellInfoReport.u16VCellMin;

	for (i = 0; i < cell_count; ++i)
	{
		UINT16 cell_bit = CB_GetCellBit(i);
		UINT16 cell_volt = g_stCellInfoReport.u16VCell[i];
		UINT8 need_balance = 0;

		if ((cell_volt >= OtherElement.u16Balance_OpenVoltage) && ((UINT16)(cell_volt - vcell_min) >= OtherElement.u16Balance_OpenWindow))
		{
			need_balance = 1;
		}

		if (CELL_BALANCE_STATUS_OFF == g_enCellBalanceStatus[i])
		{
			if (need_balance)
			{
				if ((++g_u8CellBalanceFilterCnt[i]) >= TIME_1000MS_2S)
				{
					g_u8CellBalanceFilterCnt[i] = 0;
					g_enCellBalanceStatus[i] = CELL_BALANCE_STATUS_ON_VOLT_DELTA;
					s_u16BalanceCandidateMask |= cell_bit;
					changed = 1;
				}
			}
			else if (g_u8CellBalanceFilterCnt[i] > 0)
			{
				--g_u8CellBalanceFilterCnt[i];
			}
		}
		else
		{
			if ((cell_volt < OtherElement.u16Balance_OpenVoltage) || ((UINT16)(cell_volt - vcell_min) < OtherElement.u16Balance_CloseWindow))
			{
				if ((++g_u8CellBalanceFilterCnt[i]) >= TIME_1000MS_2S)
				{
					g_u8CellBalanceFilterCnt[i] = 0;
					g_enCellBalanceStatus[i] = CELL_BALANCE_STATUS_OFF;
					s_u16BalanceCandidateMask &= (UINT16)(~cell_bit);
					changed = 1;
				}
			}
			else if (g_u8CellBalanceFilterCnt[i] > 0)
			{
				--g_u8CellBalanceFilterCnt[i];
			}
		}
	}

	for (; i < CELL_NUMS_MAX; ++i)
	{
		g_enCellBalanceStatus[i] = CELL_BALANCE_STATUS_OFF;
		g_u8CellBalanceFilterCnt[i] = 0;
	}

	if (changed)
	{
		g_u16CBnFLAG_ToUpper = s_u16BalanceCandidateMask;
	}
}

void CellBalance_DataInit(void)
{
	UINT8 i;

	for (i = 0; i < CELL_NUMS_MAX; ++i)
	{
		g_enCellBalanceStatus[i] = CELL_BALANCE_STATUS_OFF;
		g_u8CellBalanceFilterCnt[i] = 0;
	}

	s_u16BalanceCandidateMask = 0;
	s_u16BalanceActiveMask = 0;
	g_u16CBnFLAG_ToUpper = 0;
	(void)CB_ApplyBalanceMask(0);

	if (0 == SystemStatus.bits.b1Status_BnCloseIO)
	{
		g_enBalanceState = BALANCE_ST_MONITOR;
	}
}

void CellBalance_Monitor(UINT8 OnOFF_Ctrl)
{
	static UINT8 s_u8BnRecord = 0;
	static UINT8 s_u8CurrentSeen = 0;
	static UINT16 s_u16RestDelayCnt = 0;

	if ((g_stCellInfoReport.u16Ichg > CB_BALANCE_CURRENT_LIMIT) || (g_stCellInfoReport.u16IDischg > CB_BALANCE_CURRENT_LIMIT))
	{
		s_u8CurrentSeen = 1;
		s_u16RestDelayCnt = 0;
	}

	if (0 == CB_IsBalanceAllowed(OnOFF_Ctrl))
	{
		s_u16BalanceCandidateMask = 0;
		g_u16CBnFLAG_ToUpper = 0;

		if ((s_u16BalanceActiveMask != 0) || (0 != g_u8CBn_StatusFlag))
		{
			g_enBalanceState = BALANCE_ST_OFF;
		}
		else
		{
			CB_UpdateDebugInfo(0);
		}

		return;
	}

	if (s_u8CurrentSeen)
	{
		if (++s_u16RestDelayCnt < CB_BALANCE_REST_DELAY_S)
		{
			return;
		}

		s_u8CurrentSeen = 0;
		s_u16RestDelayCnt = 0;
	}

	CB_RebuildCandidateMask();

	if (s_u16BalanceCandidateMask != 0)
	{
		g_enBalanceState = BALANCE_ST_ODD_ON;

		if (0 == s_u8BnRecord)
		{
			System_ERROR_UserCallback(ERROR_BALANCED);
			s_u8BnRecord = 1;
		}
	}
	else
	{
		g_enBalanceState = BALANCE_ST_OFF;
		s_u8BnRecord = 0;
	}
}

void CellBalance_StateOddOn(UINT8 OnOFF_Ctrl)
{
	static UINT16 s_u16StateCnt = 0;

	if ((0 == OnOFF_Ctrl) || (0 == CB_IsBalanceAllowed(OnOFF_Ctrl)))
	{
		s_u16StateCnt = 0;
		g_enBalanceState = BALANCE_ST_OFF;
		return;
	}

	if (0 == (s_u16BalanceCandidateMask & ODD_SELECT))
	{
		s_u16StateCnt = 0;
		g_enBalanceState = BALANCE_ST_EVEN_ON;
		return;
	}

	if (0 == s_u16StateCnt)
	{
		if (0 != CB_ApplyCandidateMask(CELL_BALANCE_ON_ODD))
		{
			g_enBalanceState = BALANCE_ST_OFF;
			return;
		}
	}

	if (++s_u16StateCnt >= Balance_OpenT_ODD)
	{
		s_u16StateCnt = 0;
		g_enBalanceState = BALANCE_ST_EVEN_ON;
	}
}

void CellBalance_StateEvenOn(UINT8 OnOFF_Ctrl)
{
	static UINT16 s_u16StateCnt = 0;

	if ((0 == OnOFF_Ctrl) || (0 == CB_IsBalanceAllowed(OnOFF_Ctrl)))
	{
		s_u16StateCnt = 0;
		g_enBalanceState = BALANCE_ST_OFF;
		return;
	}

	if (0 == (s_u16BalanceCandidateMask & EVEN_SELECT))
	{
		s_u16StateCnt = 0;
		g_enBalanceState = BALANCE_ST_OFF;
		return;
	}

	if (0 == s_u16StateCnt)
	{
		if (0 != CB_ApplyCandidateMask(CELL_BALANCE_ON_EVEN))
		{
			g_enBalanceState = BALANCE_ST_OFF;
			return;
		}
	}

	if (++s_u16StateCnt >= Balance_OpenT_EVEN)
	{
		s_u16StateCnt = 0;
		g_enBalanceState = BALANCE_ST_OFF;
	}
}

void CellBalance_StateOFF(UINT8 OnOFF_Ctrl)
{
	static UINT16 s_u16StateCnt = 0;
	UINT16 off_delay_s = (UINT16)(Balance_OpenT_MOS + OtherElement.u16Sys_PreChg_Time);

	if (s_u16BalanceActiveMask != 0)
	{
		(void)CB_ApplyBalanceMask(0);
	}

	if (0 == off_delay_s)
	{
		s_u16StateCnt = 0;
		g_enBalanceState = BALANCE_ST_MONITOR;
		return;
	}

	if ((0 == OnOFF_Ctrl) || (0 == CB_IsBalanceAllowed(OnOFF_Ctrl)))
	{
		if (++s_u16StateCnt >= off_delay_s)
		{
			s_u16StateCnt = 0;
			g_enBalanceState = BALANCE_ST_MONITOR;
		}
		return;
	}

	if (++s_u16StateCnt >= off_delay_s)
	{
		s_u16StateCnt = 0;
		g_enBalanceState = BALANCE_ST_MONITOR;
	}
}

void App_CellBalance(void)
{
	uint8_t i;
	uint32_t balancebk = 0;
	static uint32_t uiBalanceChannel = 0;
	static uint8_t ucBalUpdateTimeCnt = 0;
	static uint8_t ucBalanceTimeCnt[20] = {0};

	if (0 == g_st_SysTimeFlag.bits.b1Sys1000msFlag2)
	{
		return;
	}

	if((g_stCellInfoReport.u16IDischg > 0) 
	|| (g_stCellInfoReport.u16VCellMin < OtherElement.u16Balance_OpenVoltage)
	|| (g_stCellInfoReport.u16VCellDelta < OtherElement.u16Balance_OpenWindow))
	{
		for(i=0; i<SeriesNum; i++)
		{
			ucBalanceTimeCnt[i] = 0;
		}
		balancebk = 0;
		g_stCellInfoReport.balance_status = 0;
		 for(i = 0; i < SeriesNum; i++)
    	{
        if (g_stCellInfoReport.balance_status & (1 << i)) {
			sys_time.bal_cell[i] = true;
        }
		else
			sys_time.bal_cell[i] = false;
    	}
	}
	else
	{
		uint16_t VC_min = g_stCellInfoReport.u16VCellMin;

		for (i = 0; i < SeriesNum; i++)
		{
			if ((g_stCellInfoReport.u16VCell[i] >= OtherElement.u16Balance_OpenVoltage) && (g_stCellInfoReport.u16VCell[i] >= OtherElement.u16Balance_OpenWindow + VC_min))
			{
				if (++ucBalanceTimeCnt[i] >= 3) // 该电芯满足平衡条件且超过延时阈值
				{
					ucBalanceTimeCnt[i] = 3;
					balancebk |= (1 << i);
				}
			}
			else // 电芯不满足平衡条件时立即停止该电芯的平衡
			{
				ucBalanceTimeCnt[i] = 0;
				balancebk &= ~(1 << i);
			}
		}
	}

	for (i = 0; i < SeriesNum; i++)
	{
		if (g_stCellInfoReport.balance_status & (1 << i))
		{
			sys_time.bal_cell[i] = true;
		}
		else
			sys_time.bal_cell[i] = false;

	}

	if ((uiBalanceChannel != balancebk) // 平衡状态改变时配置平衡，或平衡过程中定时写平衡寄存器
		|| ((uiBalanceChannel != 0) && (++ucBalUpdateTimeCnt >= (3))))
	{
		ucBalUpdateTimeCnt = 0;
		uiBalanceChannel = balancebk;
		g_stCellInfoReport.balance_status = uiBalanceChannel;

		CB_AfeWriteBalanceMaskU24(g_stCellInfoReport.balance_status);
	}
}

#if 0
void App_CellBalance(void)
{
	if (0 == g_st_SysTimeFlag.bits.b1Sys1000msFlag2)
	{
		return;
	}

	// 1 每一串均衡单独测试， 2、多串一起均衡，测试afe均衡逻辑 3、均衡31s，自动停止
	static uint8_t bal_state = 0;
	uint8_t i;
	uint32_t balancebk = 0;
	switch (bal_state)
	{
	case 0:
		if (sys_time.bal_channel != 0)
		{
			for (i = 0; i < SNum; i++)
			{
				if (sys_time.bal_cell[i])
				{
					balancebk |= (1 << i);
				}
			}

			if (balancebk != 0)
			{
				CB_AfeWriteBalanceMaskU24(balancebk);
				bal_state = 1;
				sys_time.bal_time = 0;
			}
		}
		break;
	case 1:
		// 超时、计时、35s
		sys_time.bal_time++;
		CB_AfeReadBalanceMaskU24(&g_stCellInfoReport.balance_status);
		if (sys_time.bal_time >= 100 || g_stCellInfoReport.balance_status == 0)
		{
			bal_state = 0;
			sys_time.bal_channel = 0;
			sys_time.bal_time = 0;
			g_stCellInfoReport.balance_status = 0;
		}
		break;

	default:
		break;
	}
}
#endif

#if 0

void App_CellBalance(void)
{
	if (0 == g_st_SysTimeFlag.bits.b1Sys1000msFlag2)
	{
		return;
	}

	switch (g_enBalanceState)
	{
	case BALANCE_ST_INIT:
		CellBalance_DataInit();
		break;

	case BALANCE_ST_MONITOR:
		CellBalance_Monitor(System_OnOFF_Func.bits.b1OnOFF_Balance);
		break;

	case BALANCE_ST_ODD_ON:
		CellBalance_StateOddOn(System_OnOFF_Func.bits.b1OnOFF_Balance);
		break;

	case BALANCE_ST_EVEN_ON:
		CellBalance_StateEvenOn(System_OnOFF_Func.bits.b1OnOFF_Balance);
		break;

	case BALANCE_ST_OFF:
		CellBalance_StateOFF(System_OnOFF_Func.bits.b1OnOFF_Balance);
		break;

	default:
		g_enBalanceState = BALANCE_ST_INIT;
		break;
	}
}

#endif