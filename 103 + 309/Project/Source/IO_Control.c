#include "main.h"

static bool isforceClose(void);

enum RELAY_CTRL_STATUS RelayCtrl_Command = RELAY_PRE_DET;
enum MOS_CTRL_STATUS MOSCtrl_Command = MOS_PRE_DET;

volatile union Switch_OnOFF_Function Switch_OnOFF_Func;
UINT8 gu8_DsgFirstOpen_Flag = 0;

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
				ChargerLoad_Func.bits.b1OFFDriver_Ovp = 1;
			}
			else if (Driver_Element.MosRelay_Status.bits.b1_FuncOFF_UV)
			{
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
}

void App_DI1_Switch(void)
{
	return;
}

void Drivers_External_Ctrl(void)
{
#if 1
	if (Driver_Element.u8_DriverCtrl_Right)
	{
		if (SystemStatus.bits.b1Status_MOS_CHG != Driver_Element.MosRelay_Status.bits.b1Status_MOS_CHG)
		{
			//log_w();
			sys_time.cnt_enter_chg_open++;
			SH367309_DriverMos_Ctrl(GPIO_CHG, Driver_Element.MosRelay_Status.bits.b1Status_MOS_CHG);
		}
		if (SystemStatus.bits.b1Status_MOS_DSG != Driver_Element.MosRelay_Status.bits.b1Status_MOS_DSG)
		{
			//log_w();
			sys_time.cnt_enter_dsg_open++;
			SH367309_DriverMos_Ctrl(GPIO_DSG, Driver_Element.MosRelay_Status.bits.b1Status_MOS_DSG);
		}
	}
#endif
}

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
	// if (0 == g_st_SysTimeFlag.bits.b1Sys10msFlag)
	// {
	// 	return;
	// }

	App_DI1_Switch();
	RefreshData_Drivers();
	GetData_Drivers();

#if (defined _RELAY_SAME_DOOR_NO_PRECHG)
	Drivers_Ctrl(g_st_SysTimeFlag.bits.b1Sys10msFlag, System_OnOFF_Func.bits.b1OnOFF_MOS_Relay, DRIVER_RELAY_SAME_DOOR_NO_PRECHG);
#elif (defined _RELAY_SAME_DOOR_HAVE_PRECHG)
	Drivers_Ctrl(g_st_SysTimeFlag.bits.b1Sys10msFlag, System_OnOFF_Func.bits.b1OnOFF_MOS_Relay, DRIVER_RELAY_SAME_DOOR_HAVE_PRECHG);
#elif (defined _RELAY_DIFF_DOOR_NO_PRECHG)
	Drivers_Ctrl(g_st_SysTimeFlag.bits.b1Sys10msFlag, System_OnOFF_Func.bits.b1OnOFF_MOS_Relay, DRIVER_RELAY_DIFF_DOOR_NO_PRECHG);
#elif (defined _RELAY_DIFF_DOOR_HAVE_PRECHG)
	Drivers_Ctrl(g_st_SysTimeFlag.bits.b1Sys10msFlag, System_OnOFF_Func.bits.b1OnOFF_MOS_Relay, DRIVER_RELAY_DIFF_DOOR_HAVE_PRECHG);
#elif (defined _MOS_SAME_DOOR_NO_PRECHG)
	Drivers_Ctrl(System_OnOFF_Func.bits.b1OnOFF_MOS_Relay, DRIVER_MOS_SAME_DOOR_NO_PRECHG);
#elif (defined _MOS_SAME_DOOR_HAVE_PRECHG)
	Drivers_Ctrl(g_st_SysTimeFlag.bits.b1Sys10msFlag, System_OnOFF_Func.bits.b1OnOFF_MOS_Relay, DRIVER_MOS_SAME_DOOR_HAVE_PRECHG);
#elif (defined _MOS_BOOTSTRAP_CIR)
	Drivers_Ctrl(g_st_SysTimeFlag.bits.b1Sys10msFlag, System_OnOFF_Func.bits.b1OnOFF_MOS_Relay, DRIVER_MOS_BOOTSTRAP_CIR);
#endif

	Drivers_External_Ctrl();
}
