#include "main.h"

static bool isforceClose(void);

enum RELAY_CTRL_STATUS RelayCtrl_Command = RELAY_PRE_DET;
enum MOS_CTRL_STATUS MOSCtrl_Command = MOS_PRE_DET;

volatile union Switch_OnOFF_Function Switch_OnOFF_Func;
UINT8 gu8_DsgFirstOpen_Flag = 0;
volatile ChargeCtrlDiag g_charge_ctrl_diag;

static void ChargeCtrl_ResetCounter(void);
static UINT16 ChargeCtrl_IncSat(UINT16 value, UINT16 limit);
static bool ChargeCtrl_IsDataValid(void);
static bool ChargeCtrl_HasChargeFault(void);
static bool ChargeCtrl_InFullVoltageZone(void);
static bool ChargeCtrl_InRechargeZone(void);
static void ChargeCtrl_UpdatePresence(void);
static UINT8 ChargeCtrl_Step(UINT8 protection_allow);

bool is_charger_online(void)
{
	// if (0 == GPIO_ReadInputDataBit(GPIO_CHG_DET, PIN_CHG_DET))
	// return true;
	if (1 == GPIO_ReadInputDataBit(GPIO_INT_WK_MCU, PIN_INT_WK_MCU))
		return true;

	return false;
}
bool is_load_online(void)
{
	if (0 == GPIO_ReadInputDataBit(GPIO_DSG_DET, PIN_DSG_DET))
		return true;

	return false;
}

// 长期更新数据
void RefreshData_Drivers(void)
{
	static UINT8 su8_OnOFF_Status = 0;

	// 需要不间断赋值的参数
	Driver_Element.Fault_Flag.all = g_stCellInfoReport.unMdlFault_Third.all;

#if defined(_SECOND_CURR_PROTECT_FUNC_)
	Driver_Element.Fault_Flag.bits.b1IdischgOcp |= g_stCellInfoReport.unMdlFault_Second.bits.b1IdischgOcp;
	Driver_Element.Fault_Flag.bits.b1IchgOcp |= g_stCellInfoReport.unMdlFault_Second.bits.b1IchgOcp;
	// Driver_Element.Fault_Flag.bits.b1IdischgOcp |= g_stCellInfoReport.unMdlFault_Second.bits.b1CellOvp;
	// Driver_Element.Fault_Flag.bits.b1IdischgOcp |= g_stCellInfoReport.unMdlFault_Second.bits.b1CellUvp;
#endif

	Driver_Element.u16_CurChg = g_stCellInfoReport.u16Ichg;
	Driver_Element.u16_CurDsg = g_stCellInfoReport.u16IDischg;

	// 信息交换区
	if (isforceClose())
	{
		Driver_Element.DriverForceExt.bits.b2_DriverOFF_Flag = FORCE_CLOSE_MODE; // CBC保护放到这里
	}
	else
	{
		Driver_Element.DriverForceExt.bits.b2_DriverOFF_Flag = FORCE_KEEP_MODE;
	}

	// 这个写法很巧妙，刚开始执行一个动作，如果执行了，转向下一个动作，不执行则继续等待。相互切换
	switch (su8_OnOFF_Status)
	{
	case 0:
		if (Driver_Element.u8_FuncOFF_Flag)
		{
			System_OnOFF_Func.bits.b1OnOFF_MOS_Relay = 0;

			if (Driver_Element.MosRelay_Status.bits.b1_FuncOFF_OV)
			{ // 具体是哪个出事了，留给后续使用
				// Sleep_Mode.bits.b1VcellOVP = 1;		//艾阳动力旧版(深山老林无电枪在线检测功能)使用了
				ChargerLoad_Func.bits.b1OFFDriver_Ovp = 1;
			}
			else if (Driver_Element.MosRelay_Status.bits.b1_FuncOFF_UV)
			{
				// Sleep_Mode.bits.b1VcellUVP = 1;		//艾阳动力使用了
				ChargerLoad_Func.bits.b1OFFDriver_Uvp = 1;
			}
			else if (Driver_Element.MosRelay_Status.bits.b1_FuncOFF_Ocp_Ichg)
			{
				ChargerLoad_Func.bits.b1OFFDriver_ChgOcp = 1;
			}
			else if (Driver_Element.MosRelay_Status.bits.b1_FuncOFF_Ocp_Idsg)
			{
				ChargerLoad_Func.bits.b1OFFDriver_DsgOcp = 1;
			}
			else if (Driver_Element.MosRelay_Status.bits.b1_FuncOFF_Ocp_Imain)
			{
				// ChargerLoad_Func.bits.b1OFFDriver_Ocp = 1;
				// 已被取消
			}
			else if (Driver_Element.MosRelay_Status.bits.b1_FuncOFF_Vdelta)
			{
				ChargerLoad_Func.bits.b1OFFDriver_Vdelta = 1;
			}

			su8_OnOFF_Status = 1;
		}
		break;

	case 1:
		if (System_OnOFF_Func.bits.b1OnOFF_MOS_Relay)
		{
			Driver_Element.u8_FuncOFF_Flag = 0; // 复原

			if (Driver_Element.MosRelay_Status.bits.b1_FuncOFF_OV)
			{ // 具体是哪个出事了，留给后续使用
				Driver_Element.MosRelay_Status.bits.b1_FuncOFF_OV = 0;
			}
			else if (Driver_Element.MosRelay_Status.bits.b1_FuncOFF_UV)
			{
				Driver_Element.MosRelay_Status.bits.b1_FuncOFF_UV = 0;
			}
			else if (Driver_Element.MosRelay_Status.bits.b1_FuncOFF_Ocp_Ichg)
			{
				Driver_Element.MosRelay_Status.bits.b1_FuncOFF_Ocp_Ichg = 0;
			}
			else if (Driver_Element.MosRelay_Status.bits.b1_FuncOFF_Ocp_Idsg)
			{
				Driver_Element.MosRelay_Status.bits.b1_FuncOFF_Ocp_Idsg = 0;
			}
			else if (Driver_Element.MosRelay_Status.bits.b1_FuncOFF_Ocp_Imain)
			{
				Driver_Element.MosRelay_Status.bits.b1_FuncOFF_Ocp_Imain = 0;
			}
			else if (Driver_Element.MosRelay_Status.bits.b1_FuncOFF_Vdelta)
			{
				Driver_Element.MosRelay_Status.bits.b1_FuncOFF_Vdelta = 0;
			}

			su8_OnOFF_Status = 0;
		}
		break;

	default:
		break;
	}
}

// 这个函数这样写对了吗
void GetData_Drivers(void)
{
	// SystemStatus.all = ((UINT32)(Driver_Element.MosRelay_Status.all & 0x00FE)) | (SystemStatus.all & 0xFFFFFF01);
}

void InitData_Drivers(void)
{
	Driver_Element.u16_PreChg_Time = OtherElement.u16Sys_PreChg_Time;

	Driver_Element.u16_PreChg_Duty = 10;
	Driver_Element.u16_PreChg_Period = 1;

	// Driver_Element.u16_VirCur_Chg = OtherElement.u16Sleep_VirCur_Chg;
	// Driver_Element.u16_VirCur_Dsg = OtherElement.u16Sleep_VirCur_Dsg;

	// 为了处理管子打开，有可能虚电流导致电池放空的现象(主接触器类型尤为明显
	// MOS带预的驱动(接触器类型没有这个机制)，改为写死2A，充电电流大于2A则退出预充机制，立刻打开放电管
	Driver_Element.u16_VirCur_Chg = 0;
	Driver_Element.u16_VirCur_Dsg = 0;

	// Driver_Element.u16_10msForceOpenT_Ovp = 3000; // 默认30s
	// Driver_Element.u16_10msForceOpenT_Uvp = 3000; // 默认30s

	Driver_Element.u8_DriverCtrl_Right = 1; // AFE控制

	g_charge_ctrl_diag.state = CHARGE_CTRL_WAIT_CHARGER;
	g_charge_ctrl_diag.presence = CHARGER_PRESENCE_UNKNOWN;
	g_charge_ctrl_diag.close_reason = CHARGE_CLOSE_NO_CHARGER;
	g_charge_ctrl_diag.charge_request = 0;
	g_charge_ctrl_diag.chg_det_low = 0;
	ChargeCtrl_ResetCounter();
}

void App_DI1_Switch(void)
{
#ifdef _DI_SWITCH_longKEY_ONOFF
	static UINT16 su16_AntiShake_Cnt2 = 0;

	if (0 == MCUI_ENI_DI1)
	{
		if (++su16_AntiShake_Cnt2 >= (5 * 3))
		{
			su16_AntiShake_Cnt2 = 0;
			entersleep(DEEP_MODE);
		}
	}
	else
	{
		su16_AntiShake_Cnt2 = 0;
	}

#endif // _DI_SWITCH_longKEY_ONOFF

#ifdef _DI_SWITCH_DSG_ONOFF
	static uint8_t su8_OnOFF_Flag = 0;
	static uint8_t su8_Repeat_Tcnt = 0;

	switch (su8_OnOFF_Flag)
	{
	case 0:
		if (MCUI_ENI_DI1)
		{
			SH367309_DriverMos_Ctrl(GPIO_DSG, 0);
			if (++su8_Repeat_Tcnt >= 5)
			{
				su8_Repeat_Tcnt = 0;
				su8_OnOFF_Flag = 1;
			}
		}
		else
		{
			su8_Repeat_Tcnt = 0;
			su8_OnOFF_Flag = 1;
		}
		break;

	case 1:
		if (!MCUI_ENI_DI1)
		{
			SH367309_DriverMos_Ctrl(GPIO_DSG, 1);
			if (++su8_Repeat_Tcnt >= 5)
			{
				su8_Repeat_Tcnt = 0;
				su8_OnOFF_Flag = 0;
			}
		}
		else
		{
			su8_Repeat_Tcnt = 0;
			su8_OnOFF_Flag = 0;
		}
		break;

	default:
		break;
	}
#endif

#ifdef _DI_SWITCH_SYS_ONOFF
	static UINT16 su16_AntiShake_Cnt1 = 0;

#if 0
	if (1 == MCUI_ENI_DI1 )
	{
		if (++su16_AntiShake_Cnt1 >= 270)
		{
			su16_AntiShake_Cnt1 = 0;
			entersleep(DEEP_MODE);
		}
	}
	else
	{
		if (su16_AntiShake_Cnt1)
		{
			--su16_AntiShake_Cnt1;
			return;
		}
	}
#else

	uint8_t state1 = 2;
	uint8_t state2 = 2;
	uint8_t state3 = 2;
	// if (MCUI_ENI_DI1 && MCUI_ENI_DI2 && MCUI_ENI_DI3)
	state1 = GPIO_ReadInputDataBit(GPIO_KEY1, PIN_KEY1);
	// state2 = GPIO_ReadInputDataBit(GPIOB, GPIO_Pin_3);
	// state3 = GPIO_ReadInputDataBit(GPIOA, GPIO_Pin_15);

	// if (state1 && state2 && state3)
	if (state1)
	{
		if (++su16_AntiShake_Cnt1 >= 10 * 2)
		{
			su16_AntiShake_Cnt1 = 0;
			entersleep(DEEP_MODE);

			BSP_Printf("switch deep sleep\n");
		}
	}
	else
	{
		su16_AntiShake_Cnt1 = 0;

		return;
	}

#if 0
	if (!MCUI_ENI_DI1 || !MCUI_ENI_DI2 || !MCUI_ENI_DI3)
	{
		return;
	}
	else
	{
	}
#endif

#endif

#endif
}
static void ChargeCtrl_ResetCounter(void)
{
	g_charge_ctrl_diag.det_low_cnt = 0;
	g_charge_ctrl_diag.det_high_cnt = 0;
	g_charge_ctrl_diag.no_charge_cnt = 0;
	g_charge_ctrl_diag.full_taper_cnt = 0;
	g_charge_ctrl_diag.full_voltage_cnt = 0;
	g_charge_ctrl_diag.recharge_cnt = 0;
}

static UINT16 ChargeCtrl_IncSat(UINT16 value, UINT16 limit)
{
	if (value < limit)
	{
		value++;
	}
	return value;
}

static bool ChargeCtrl_IsDataValid(void)
{
	if (SeriesNum != CHARGE_CTRL_TARGET_SERIES)
	{
		return false;
	}
	if ((g_stCellInfoReport.u16VCellMin < 1000u) ||
		(g_stCellInfoReport.u16VCellMax > 5000u) ||
		(g_stCellInfoReport.u16VCellMax < g_stCellInfoReport.u16VCellMin))
	{
		return false;
	}
	if (g_stCellInfoReport.u16VCellTotle == 0u)
	{
		return false;
	}
	return true;
}

static bool ChargeCtrl_HasChargeFault(void)
{
	if (Driver_Element.Fault_Flag.bits.b1CellOvp ||
		Driver_Element.Fault_Flag.bits.b1BatOvp ||
		Driver_Element.Fault_Flag.bits.b1PackOvp ||
		Driver_Element.Fault_Flag.bits.b1IchgOcp ||
		Driver_Element.Fault_Flag.bits.b1CellChgOtp ||
		Driver_Element.Fault_Flag.bits.b1CellChgUtp ||
		Driver_Element.Fault_Flag.bits.b1VcellDeltaBig ||
		Driver_Element.Fault_Flag.bits.b1TempDeltaBig ||
		Driver_Element.Fault_Flag.bits.b1TmosOtp)
	{
		return true;
	}

	if (is_AFE_COV || is_AFE_OCC || is_AFE_OTC || is_AFE_UTC)
	{
		return true;
	}
	return false;
}

static bool ChargeCtrl_InFullVoltageZone(void)
{
	return (g_stCellInfoReport.u16VCellTotle >= CHARGE_CTRL_FULL_PACK_CV) &&
		   (g_stCellInfoReport.u16VCellMax >= CHARGE_CTRL_FULL_CELL_MV);
}

static bool ChargeCtrl_InRechargeZone(void)
{
	return (g_stCellInfoReport.u16VCellTotle <= CHARGE_CTRL_RECHARGE_PACK_CV) &&
		   (g_stCellInfoReport.u16VCellMax <= CHARGE_CTRL_RECHARGE_CELL_MV);
}

static void ChargeCtrl_UpdatePresence(void)
{
	g_charge_ctrl_diag.chg_det_low = is_charger_online() ? 1u : 0u;
	if (g_charge_ctrl_diag.chg_det_low)
	{
		if (g_charge_ctrl_diag.presence == CHARGER_PRESENCE_ABSENT)
		{
			g_charge_ctrl_diag.presence = CHARGER_PRESENCE_UNKNOWN;
		}
		g_charge_ctrl_diag.det_low_cnt =
			ChargeCtrl_IncSat(g_charge_ctrl_diag.det_low_cnt, CHARGE_CTRL_DET_CONFIRM_CNT);
		g_charge_ctrl_diag.det_high_cnt = 0;
		if (g_charge_ctrl_diag.det_low_cnt >= CHARGE_CTRL_DET_CONFIRM_CNT)
		{
			g_charge_ctrl_diag.presence = CHARGER_PRESENCE_PRESENT;
		}
	}
	else
	{
		if (g_charge_ctrl_diag.presence == CHARGER_PRESENCE_PRESENT)
		{
			g_charge_ctrl_diag.presence = CHARGER_PRESENCE_UNKNOWN;
		}
		g_charge_ctrl_diag.det_high_cnt =
			ChargeCtrl_IncSat(g_charge_ctrl_diag.det_high_cnt, CHARGE_CTRL_PROBE_SAMPLE_CNT);
		g_charge_ctrl_diag.det_low_cnt = 0;
		if (g_charge_ctrl_diag.det_high_cnt >= CHARGE_CTRL_PROBE_SAMPLE_CNT)
		{
			g_charge_ctrl_diag.presence = CHARGER_PRESENCE_ABSENT;
		}
	}
}

static UINT8 ChargeCtrl_Step(UINT8 protection_allow)
{
	UINT8 charge_request = 0;

	if (!ChargeCtrl_IsDataValid())
	{
		g_charge_ctrl_diag.state = CHARGE_CTRL_FAULT_HOLD;
		g_charge_ctrl_diag.presence = CHARGER_PRESENCE_UNKNOWN;
		g_charge_ctrl_diag.close_reason = CHARGE_CLOSE_DATA_INVALID;
		ChargeCtrl_ResetCounter();
		return 0;
	}

	if ((!protection_allow) || ChargeCtrl_HasChargeFault())
	{
		g_charge_ctrl_diag.state = CHARGE_CTRL_FAULT_HOLD;
		g_charge_ctrl_diag.presence = CHARGER_PRESENCE_UNKNOWN;
		g_charge_ctrl_diag.close_reason = CHARGE_CLOSE_PROTECTION;
		ChargeCtrl_ResetCounter();
		return 0;
	}

	switch (g_charge_ctrl_diag.state)
	{
	case CHARGE_CTRL_WAIT_CHARGER:
		g_charge_ctrl_diag.close_reason = CHARGE_CLOSE_NO_CHARGER;
		ChargeCtrl_UpdatePresence();
		if (g_charge_ctrl_diag.presence == CHARGER_PRESENCE_PRESENT)
		{
			g_charge_ctrl_diag.state = CHARGE_CTRL_CHARGING;
			g_charge_ctrl_diag.close_reason = CHARGE_CLOSE_NONE;
			g_charge_ctrl_diag.no_charge_cnt = 0;
			charge_request = 1;
		}
		break;

	case CHARGE_CTRL_CHARGING:
		g_charge_ctrl_diag.presence = CHARGER_PRESENCE_UNKNOWN;
		g_charge_ctrl_diag.det_low_cnt = 0;
		g_charge_ctrl_diag.det_high_cnt = 0;
		g_charge_ctrl_diag.close_reason = CHARGE_CLOSE_NONE;
		charge_request = 1;

#if 0
		if (ChargeCtrl_InFullVoltageZone())
		{
			g_charge_ctrl_diag.full_voltage_cnt =
				ChargeCtrl_IncSat(g_charge_ctrl_diag.full_voltage_cnt,
								  CHARGE_CTRL_FULL_VOLT_CONFIRM_CNT);
			if ((g_stCellInfoReport.u16IDischg == 0u) &&
				(g_stCellInfoReport.u16Ichg <= CHARGE_CTRL_FULL_TAPER_CURRENT_A10))
			{
				g_charge_ctrl_diag.full_taper_cnt =
					ChargeCtrl_IncSat(g_charge_ctrl_diag.full_taper_cnt,
									  CHARGE_CTRL_FULL_TAPER_CONFIRM_CNT);
			}
			else
			{
				g_charge_ctrl_diag.full_taper_cnt = 0;
			}
		}
		else
		{
			g_charge_ctrl_diag.full_taper_cnt = 0;
			g_charge_ctrl_diag.full_voltage_cnt = 0;
		}

		if ((g_charge_ctrl_diag.full_taper_cnt >= CHARGE_CTRL_FULL_TAPER_CONFIRM_CNT) ||
			(g_charge_ctrl_diag.full_voltage_cnt >= CHARGE_CTRL_FULL_VOLT_CONFIRM_CNT))
		{
			g_charge_ctrl_diag.state = CHARGE_CTRL_FULL_HOLD;
			g_charge_ctrl_diag.presence = CHARGER_PRESENCE_UNKNOWN;
			g_charge_ctrl_diag.close_reason = CHARGE_CLOSE_FULL;
			g_charge_ctrl_diag.det_low_cnt = 0;
			g_charge_ctrl_diag.det_high_cnt = 0;
			g_charge_ctrl_diag.recharge_cnt = 0;
			charge_request = 0;
			break;
		}
#endif
		// 最好的逻辑是：1、充电电流没有，断开chgmos，检测充电器是否在线，在线即为充满，不在线代表充电器断开

		// if (g_stCellInfoReport.u16VCellTotle >= CHARGE_CTRL_PROBE_MAX_PACK_CV)
		// {
		// 	g_charge_ctrl_diag.no_charge_cnt = 0;
		// }
		// else if (g_stCellInfoReport.u16Ichg >= CHARGE_CTRL_CHARGE_EVIDENCE_A10)
		// {
		// 	g_charge_ctrl_diag.no_charge_cnt = 0;
		// }
		if (g_stCellInfoReport.u16Ichg >= CHARGE_CTRL_CHARGE_EVIDENCE_A10)
		{
			g_charge_ctrl_diag.no_charge_cnt = 0;
		}
		else
		{
			g_charge_ctrl_diag.no_charge_cnt =
				ChargeCtrl_IncSat(g_charge_ctrl_diag.no_charge_cnt, CHARGE_CTRL_PROBE_IDLE_CNT);
			if (g_charge_ctrl_diag.no_charge_cnt >= CHARGE_CTRL_PROBE_IDLE_CNT)
			{
				g_charge_ctrl_diag.state = CHARGE_CTRL_PROBE_OFF;
				g_charge_ctrl_diag.presence = CHARGER_PRESENCE_UNKNOWN;
				g_charge_ctrl_diag.close_reason = CHARGE_CLOSE_PROBE;
				g_charge_ctrl_diag.det_low_cnt = 0;
				g_charge_ctrl_diag.det_high_cnt = 0;
				g_charge_ctrl_diag.no_charge_cnt = 0;
				charge_request = 0;
			}
		}
		break;

	case CHARGE_CTRL_PROBE_OFF:
		g_charge_ctrl_diag.close_reason = CHARGE_CLOSE_PROBE;
		g_charge_ctrl_diag.det_high_cnt =
			ChargeCtrl_IncSat(g_charge_ctrl_diag.det_high_cnt, CHARGE_CTRL_PROBE_SETTLE_CNT);
		if (g_charge_ctrl_diag.det_high_cnt >= CHARGE_CTRL_PROBE_SETTLE_CNT)
		{
			g_charge_ctrl_diag.state = CHARGE_CTRL_PROBE_SAMPLE;
			g_charge_ctrl_diag.det_low_cnt = 0;
			g_charge_ctrl_diag.det_high_cnt = 0;
		}
		break;

	case CHARGE_CTRL_PROBE_SAMPLE:
		g_charge_ctrl_diag.close_reason = CHARGE_CLOSE_PROBE;
		ChargeCtrl_UpdatePresence();
		//无电流的三种情况
		//!!!逻辑上天生解决充电电流小于负载电流问题，目前直接锁死，充电器在线，断开充电，只有充电器重新断开才允许重新充电
		if (ChargeCtrl_InFullVoltageZone())
		{
			g_charge_ctrl_diag.state = CHARGE_CTRL_FULL_HOLD;
			g_charge_ctrl_diag.presence = CHARGER_PRESENCE_UNKNOWN;
			g_charge_ctrl_diag.close_reason = CHARGE_CLOSE_FULL;
			g_charge_ctrl_diag.det_low_cnt = 0;
			g_charge_ctrl_diag.det_high_cnt = 0;
			g_charge_ctrl_diag.recharge_cnt = 0;
			// charge_request = 0;
		}
		//需要充电电流小于负载电流时，允许充电的情况，还要按需求修改CHARGE_CTRL_PROBE_IDLE_CNT检测频率
	#if 0
		else if (g_charge_ctrl_diag.presence == CHARGER_PRESENCE_PRESENT)
		{
			g_charge_ctrl_diag.state = CHARGE_CTRL_CHARGING;
			g_charge_ctrl_diag.close_reason = CHARGE_CLOSE_NONE;
			// charge_request = 1;
		}
	#endif
		else if (g_charge_ctrl_diag.presence == CHARGER_PRESENCE_ABSENT)
		{
			g_charge_ctrl_diag.state = CHARGE_CTRL_WAIT_CHARGER;
			g_charge_ctrl_diag.close_reason = CHARGE_CLOSE_NO_CHARGER;
		}
		break;

	// 休眠重启了？
	case CHARGE_CTRL_FULL_HOLD:
		g_charge_ctrl_diag.close_reason = CHARGE_CLOSE_FULL;
		ChargeCtrl_UpdatePresence();
		if (ChargeCtrl_InRechargeZone() &&
			(g_charge_ctrl_diag.presence == CHARGER_PRESENCE_PRESENT))
		{
			g_charge_ctrl_diag.recharge_cnt =
				ChargeCtrl_IncSat(g_charge_ctrl_diag.recharge_cnt,
								  CHARGE_CTRL_RECHARGE_CONFIRM_CNT);
			if (g_charge_ctrl_diag.recharge_cnt >= CHARGE_CTRL_RECHARGE_CONFIRM_CNT)
			{
				g_charge_ctrl_diag.state = CHARGE_CTRL_CHARGING;
				g_charge_ctrl_diag.close_reason = CHARGE_CLOSE_NONE;
				g_charge_ctrl_diag.full_taper_cnt = 0;
				g_charge_ctrl_diag.full_voltage_cnt = 0;
				g_charge_ctrl_diag.recharge_cnt = 0;
				charge_request = 1;
			}
		}
		else
		{
			g_charge_ctrl_diag.recharge_cnt = 0;
		}
		break;

	case CHARGE_CTRL_FAULT_HOLD:
	default:
		g_charge_ctrl_diag.state = CHARGE_CTRL_WAIT_CHARGER;
		g_charge_ctrl_diag.presence = CHARGER_PRESENCE_UNKNOWN;
		g_charge_ctrl_diag.close_reason = CHARGE_CLOSE_NO_CHARGER;
		ChargeCtrl_ResetCounter();
		break;
	}

	return charge_request;
}

void ChargeCtrl_ForceOff(ChargeCloseReason reason)
{
	g_charge_ctrl_diag.state = CHARGE_CTRL_FAULT_HOLD;
	g_charge_ctrl_diag.presence = CHARGER_PRESENCE_UNKNOWN;
	g_charge_ctrl_diag.close_reason = reason;
	g_charge_ctrl_diag.charge_request = 0;
	g_charge_ctrl_diag.chg_det_low = 0;
	ChargeCtrl_ResetCounter();

	Driver_Element.MosRelay_Status.bits.b1Status_MOS_CHG = CLOSE_MODE;
	if (GPIO_ReadOutputDataBit(GPIO_M_CCC, PIN_M_CCC) != Bit_RESET)
	{
		sys_time.cnt_enter_chg_open++;
		SH367309_DriverMos_Ctrl(GPIO_CHG, CLOSE);
	}
}

enum system_status bms_status = S_STARTUP;

void Drivers_External_Ctrl(void)
{
	UINT8 protection_allow_chg;

	/*
	 * Keep the original BMS/load state machine responsible for discharge
	 * MOS control. The charger controller below is only an additional
	 * permission condition for charge MOS.
	 */
	protection_allow_chg =
		(UINT8)Driver_Element.MosRelay_Status.bits.b1Status_MOS_CHG;

	switch (bms_status)
	{
	case S_IDLE:
		Driver_Element.MosRelay_Status.bits.b1Status_MOS_DSG = CLOSE_MODE;
		Driver_Element.MosRelay_Status.bits.b1Status_MOS_CHG = CLOSE_MODE;

		if (is_load_online())
		{
			bms_status = S_DSG;
		}
		if (is_charger_online())
		{
			bms_status = S_CHG;
		}
		break;

	case S_STARTUP:
		Driver_Element.MosRelay_Status.bits.b1Status_MOS_DSG = CLOSE_MODE;
		Driver_Element.MosRelay_Status.bits.b1Status_MOS_CHG = CLOSE_MODE;

		if (is_load_online())
		{
			bms_status = S_DSG;
		}
		if (is_charger_online())
		{
			bms_status = S_CHG;
		}
		break;

	case S_DSG:
		Driver_Element.MosRelay_Status.bits.b1Status_MOS_CHG = CLOSE_MODE;
		if (!is_load_online())
		{
			bms_status = S_IDLE;
		}
		if (is_charger_online())
		{
			bms_status = S_CHG;
		}
		break;

	case S_CHG:
		if (!is_load_online())
		{
			Driver_Element.MosRelay_Status.bits.b1Status_MOS_DSG = CLOSE_MODE;
		}
		break;

	default:
		Driver_Element.MosRelay_Status.bits.b1Status_MOS_DSG = CLOSE_MODE;
		Driver_Element.MosRelay_Status.bits.b1Status_MOS_CHG = CLOSE_MODE;
		bms_status = S_STARTUP;
		break;
	}

	/*
	 * Drivers_Ctrl() has already calculated the protection result. The
	 * charger state machine may only remove that permission, never add it.
	 */
	g_charge_ctrl_diag.charge_request = ChargeCtrl_Step(protection_allow_chg);
	Driver_Element.MosRelay_Status.bits.b1Status_MOS_CHG =
		(DriversStatus)(protection_allow_chg && g_charge_ctrl_diag.charge_request);

	if (g_charge_ctrl_diag.charge_request)
	{
		bms_status = S_CHG;
	}
	else if ((bms_status == S_CHG) &&
			 (g_charge_ctrl_diag.state == CHARGE_CTRL_WAIT_CHARGER) &&
			 (g_charge_ctrl_diag.presence == CHARGER_PRESENCE_ABSENT))
	{
		bms_status = is_load_online() ? S_DSG : S_IDLE;
	}

	// todo 测试ctlc、芯片异常等情况对afe寄存器状态的影响
	// todo 状态变化时 会有问题？？？时序有影响，还会有其他问题吗
	if (Driver_Element.u8_DriverCtrl_Right)
	{
		if (SystemStatus.bits.b1Status_MOS_CHG != Driver_Element.MosRelay_Status.bits.b1Status_MOS_CHG)
		{
			// log_w();
			sys_time.cnt_enter_chg_open++;
			SH367309_DriverMos_Ctrl(GPIO_CHG, Driver_Element.MosRelay_Status.bits.b1Status_MOS_CHG);
		}
		if (SystemStatus.bits.b1Status_MOS_DSG != Driver_Element.MosRelay_Status.bits.b1Status_MOS_DSG)
		{
			// log_w();
			sys_time.cnt_enter_dsg_open++;
			SH367309_DriverMos_Ctrl(GPIO_DSG, Driver_Element.MosRelay_Status.bits.b1Status_MOS_DSG);
		}
	}
}
// void Drivers_External_Ctrl(void)
// {
// 	if (is_AFE_COV || is_AFE_CUV || is_AFE_OCC || is_AFE_ODC || is_AFE_OTC || is_AFE_UTC || is_AFE_OTD || is_AFE_UTD || IS_AFE_SC)
// 	{
// 		return;
// 	}
// #if 1
// 	if (Driver_Element.u8_DriverCtrl_Right)
// 	{
// 		if (SystemStatus.bits.b1Status_MOS_CHG != Driver_Element.MosRelay_Status.bits.b1Status_MOS_CHG)
// 		{
// 			// log_w();
// 			sys_time.cnt_enter_chg_open++;
// 			SH367309_DriverMos_Ctrl(GPIO_CHG, Driver_Element.MosRelay_Status.bits.b1Status_MOS_CHG);
// 		}
// 		if (SystemStatus.bits.b1Status_MOS_DSG != Driver_Element.MosRelay_Status.bits.b1Status_MOS_DSG)
// 		{
// 			// log_w();
// 			sys_time.cnt_enter_dsg_open++;
// 			SH367309_DriverMos_Ctrl(GPIO_DSG, Driver_Element.MosRelay_Status.bits.b1Status_MOS_DSG);
// 		}
// 	}
// #endif
// }

void InitMosRelay_DOx(void)
{
	InitData_Drivers();
}

bool isforceClose(void)
{
	if (SystemStatus.bits.b1Status_BnCloseIO)
	{
		log_w("close");
		return true;
	}
	else if (SystemStatus.bits.b1Status_HeatCloseIO)
	{
		log_w("close");
		return true;
	}
	else if (SystemStatus.bits.b1Status_CBCCloseIO)
	{
		log_w("close");
		return true;
	}
	else if (System_ErrFlag.u8ErrFlag_Com_AFE1)
	{
		log_w("close");
		return true;
	}
	else if (System_ErrFlag.u8ErrFlag_Com_AFE2)
	{
		log_w("close");
		return true;
	}
	else if (System_ErrFlag.u8ErrFlag_Com_EEPROM)
	{
		log_w("close");
		return true;
	}
	else if (System_ErrFlag.u8ErrFlag_Store_EEPROM)
	{
		log_w("close");
		return true;
	}
	else if (CBC_Element.u8CBC_CHG_ErrFlag)
	{
		log_w("close");
		return true;
	}
	else if (CBC_Element.u8CBC_DSG_ErrFlag)
	{
		log_w("close");
		return true;
	}
	else if (System_ERROR_UserCallback(ERROR_STATUS_TEMP_BREAK))
	{
		log_w("close");
		return true;
	}
	else
	{
		return false;
	}
}
void App_MOS_Relay_Ctrl(void)
{
	// if (0 == g_st_SysTimeFlag.bits.b1Sys10msFlag1)
	// {
	// 	return;
	// }

	App_DI1_Switch();
	RefreshData_Drivers();
	GetData_Drivers();

#if (defined _RELAY_SAME_DOOR_NO_PRECHG)
	Drivers_Ctrl(g_st_SysTimeFlag.bits.b1Sys10msFlag1, System_OnOFF_Func.bits.b1OnOFF_MOS_Relay, DRIVER_RELAY_SAME_DOOR_NO_PRECHG);
#elif (defined _RELAY_SAME_DOOR_HAVE_PRECHG)
	Drivers_Ctrl(g_st_SysTimeFlag.bits.b1Sys10msFlag1, System_OnOFF_Func.bits.b1OnOFF_MOS_Relay, DRIVER_RELAY_SAME_DOOR_HAVE_PRECHG);
#elif (defined _RELAY_DIFF_DOOR_NO_PRECHG)
	Drivers_Ctrl(g_st_SysTimeFlag.bits.b1Sys10msFlag1, System_OnOFF_Func.bits.b1OnOFF_MOS_Relay, DRIVER_RELAY_DIFF_DOOR_NO_PRECHG);
#elif (defined _RELAY_DIFF_DOOR_HAVE_PRECHG)
	Drivers_Ctrl(g_st_SysTimeFlag.bits.b1Sys10msFlag1, System_OnOFF_Func.bits.b1OnOFF_MOS_Relay, DRIVER_RELAY_DIFF_DOOR_HAVE_PRECHG);
#elif (defined _MOS_SAME_DOOR_NO_PRECHG)
	Drivers_Ctrl(System_OnOFF_Func.bits.b1OnOFF_MOS_Relay, DRIVER_MOS_SAME_DOOR_NO_PRECHG);
#elif (defined _MOS_SAME_DOOR_HAVE_PRECHG)
	Drivers_Ctrl(g_st_SysTimeFlag.bits.b1Sys10msFlag1, System_OnOFF_Func.bits.b1OnOFF_MOS_Relay, DRIVER_MOS_SAME_DOOR_HAVE_PRECHG);
#elif (defined _MOS_BOOTSTRAP_CIR)
	Drivers_Ctrl(g_st_SysTimeFlag.bits.b1Sys10msFlag1, System_OnOFF_Func.bits.b1OnOFF_MOS_Relay, DRIVER_MOS_BOOTSTRAP_CIR);
#endif

	Drivers_External_Ctrl();
}
