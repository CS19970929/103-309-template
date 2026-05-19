#include "SocEnhance.h"
#include "PubFunc.h"
#include "conf.h"
#include "EEPROM.h"
#include "DataDeal.h"

#define E2P_ADDR_SOC E2P_ADDR_E2POS_ENHANCE_SOC
#define E2P_ADDR_DSG_SOC_Int (E2P_ADDR_E2POS_ENHANCE_SOC + 2)
#define E2P_ADDR_CYCLE_TIMES (E2P_ADDR_E2POS_ENHANCE_SOC + 2 + 2)
// #define E2P_ADDR_CYCLE_TIMES	(E2P_ADDR_E2POS_ENHANCE_SOC + 2 + 2)

#define SOC_VIRTUAL_CURRENT_CHG (UINT16)2 // A*10，1和2都认为是0，带=号，0.2就开始算了
#define SOC_VIRTUAL_CURRENT_DSG (UINT16)2 // A*10，1和2都认为是0，这个不能为0的同时，把=号判断上去，不然就会卡在DSG那里计算出不来。

// #define CHG_CUR_1C							2100	//A*10恒流充电为1C，恒压充电为1C-0.1C(SOC=95%)，涓流充电也为0.1C
#define EEPROM_VALUE_POWEROFF_FLAG ((UINT16)0x5678)
#define EEPROM_VALUE_DATA_UPDATE_FLAG ((UINT16)0x9ABC)
#define EEPROM_VALUE_STORE_RESET ((UINT16)0xFFFF)

#define SOC_PERCENT_MAX ((UINT8)100)
#define SOC_DEFAULT_START_PERCENT ((UINT8)60)
#define SOC_DSG_CYCLE_PERCENT ((UINT8)80)
#define SOC_DEFAULT_CAPACITY_AH10 ((UINT16)250)
#define SOC_CELL_VOLTAGE_MIN_MV ((UINT16)2000)
#define SOC_CELL_VOLTAGE_MAX_MV ((UINT16)5000)
#define SOC_CELL_VOLTAGE_MAX_DELTA_MV ((UINT16)600)
#define SOC_CONTROL_PERIOD_MS ((UINT32)200)
#define SOC_CAP_UNIT_MA_MS ((UINT32)100000)
#define SOC_CURRENT_A10_TO_MA ((UINT32)100)

#ifndef SOC_SELF_CONSUMPTION_ENABLE
#define SOC_SELF_CONSUMPTION_ENABLE 1
#endif

#ifndef SOC_SELF_CONSUMPTION_CURRENT_MA
#ifdef SOC_SELF_CONSUME_CURRENT_MA
#define SOC_SELF_CONSUMPTION_CURRENT_MA SOC_SELF_CONSUME_CURRENT_MA
#else
#define SOC_SELF_CONSUMPTION_CURRENT_MA ((UINT16)15)
#endif
#endif
#ifndef SOC_SELF_CONSUME_CURRENT_MA
#define SOC_SELF_CONSUME_CURRENT_MA SOC_SELF_CONSUMPTION_CURRENT_MA
#endif

#define SOC_REST_OCV_START_DELAY_S ((UINT16)300)
#define SOC_REST_OCV_STEP_PERIOD_S ((UINT16)60)
#define SOC_REST_OCV_DEADBAND_PERCENT ((UINT8)5)
#define SOC_SECONDS_TO_200MS(sec) ((UINT16)(((UINT32)(sec) * 1000) / SOC_CONTROL_PERIOD_MS))
#define SOC_TERMINAL_CAL_L1_200MS SOC_SECONDS_TO_200MS(10)
#define SOC_TERMINAL_CAL_L2_200MS SOC_SECONDS_TO_200MS(8)
#define SOC_TERMINAL_CAL_L3_200MS SOC_SECONDS_TO_200MS(4)
#define SOC_TERMINAL_CAL_FORCE_200MS SOC_SECONDS_TO_200MS(2)
#define SOC_REST_OCV_START_DELAY_200MS SOC_SECONDS_TO_200MS(SOC_REST_OCV_START_DELAY_S)
#define SOC_REST_OCV_STEP_PERIOD_200MS SOC_SECONDS_TO_200MS(SOC_REST_OCV_STEP_PERIOD_S)
#define SOC_FULL_CALI_HOLD_200MS SOC_SECONDS_TO_200MS(5)
#define SOC_EMPTY_CALI_HOLD_200MS SOC_SECONDS_TO_200MS(10)
#define SOC_DIR_NONE ((UINT8)0)
#define SOC_DIR_CHG ((UINT8)1)
#define SOC_DIR_DSG ((UINT8)2)

#ifdef TERNARYLI
#define SOC_FULL_CELL_MIN_MV ((UINT16)4000)
#elif (defined(LIFEPO))
#define SOC_FULL_CELL_MIN_MV ((UINT16)3300)
#else
#define SOC_FULL_CELL_MIN_MV ((UINT16)4000)
#endif
// 充电可以提前充满，但是不能卡死
// #define _CAL_SLOW_DOWN_CHG

// typedef enum _CUR
//{
//	CurCHG = 0,
//	CurDSG
// } _Cur;

enum EEPROM_COMMAND
{
	EEPROM_DATA_REFRESH = 0,
	EEPROM_DATA_READ
};

struct SOC_CALCULATE_ELEMENT
{
	// InitSOC_IntEnhance赋值类型
	UINT32 u32CapFactory;	// 电池初始总容量(出厂容量)As*10 =        Ah*3600*10
	UINT32 u32CycleT_Limit; // 可循环次数
	// 以下置零
	UINT32 u32CapChange;	  // 电池容量变化	   As*10，叠加类型
	UINT8 u8OCV_Cali_Flag;	  // 开路电压法可使用标志
	UINT8 u8CHG_AHCalcu_Flag; // 充电安时积分可使用标志
	UINT8 u8DSG_AHCalcu_Flag; // 放电安时积分可使用标志

	// InitSOC_IntEnhance赋值，其后SOC_Update_StartUp再次赋值类型
	UINT8 u8SOC_Now;	   // 当前电池SOC     0—100 为相对容量百分比
	UINT32 u32CapNow;	   // 电池剩余总容量As*10
	UINT8 u8DSG_SOC_Int;   // 循环次数只算放电量，已放电量积累量百分比，90%算一个循环
	UINT32 u32Cycle_times; // 循环次数*100，本来只打算用用一个变量直接叠加去处理，但是太损耗EEPROM发现不行
	UINT32 u32CapFull;	   // 电池衰减后总容量As*10(SOH)，我的显示SOH要改一改，算错了

	// 运行过程长期修改类型
	UINT8 u8SOC_Old; // 初始SOC    0-100 为相对容量百分比
	// UINT8   u8a_BurnIn;         //老化因素α的修正系数，系数乘以100
	// UINT8   u8b_CapC;      		//电池容量修正因子δ，与充放电循环次数相关δ = f(Cycle_times)
	UINT8 u8_DataUpdateOK;	  // 更新记录
	UINT32 u32CapFull_Cal_As; // 长期运行，更新容量，As*10
};

struct SOC_ENHANCE_ELEMENT SOC_Enhance_Element;			   // 对外交互结构体,lib文件的桥梁
struct SOC_CALCULATE_ELEMENT SOC_Calculate_Element;		   // 内部计算结构体
struct SOC_CALCULATE_ELEMENT SOC_Calculate_Element_backup; // 内部计算结构体

#if SOC_SELF_CONSUMPTION_ENABLE
static UINT32 s_u32SelfConsumeMaMs = 0;
static UINT32 s_u32SelfConsumeCapChange = 0;
#endif

static void SOC_ResetSelfConsumeAccumulators(void)
{
#if SOC_SELF_CONSUMPTION_ENABLE
	s_u32SelfConsumeMaMs = 0;
	s_u32SelfConsumeCapChange = 0;
#endif
}

void SOC_DealEEPROM_Data(enum EEPROM_COMMAND Command);
UINT8 Get_OpenCircuit_Value(void);
UINT8 isCHG(void);
UINT8 isDSG(void);

static UINT8 SOC_ClampPercent(UINT32 percent)
{
	if (percent > SOC_PERCENT_MAX)
	{
		return SOC_PERCENT_MAX;
	}
	return (UINT8)percent;
}

static UINT8 SOC_IsCapacityValid(void)
{
	return (SOC_Calculate_Element.u32CapFactory > 0) ? 1 : 0;
}

static UINT8 SOC_IsCellVoltageValid(void)
{
	if ((SOC_Enhance_Element.u16_VCellMin < SOC_CELL_VOLTAGE_MIN_MV) || (SOC_Enhance_Element.u16_VCellMin > SOC_CELL_VOLTAGE_MAX_MV))
	{
		return 0;
	}
	if ((SOC_Enhance_Element.u16_VCellMax < SOC_CELL_VOLTAGE_MIN_MV) || (SOC_Enhance_Element.u16_VCellMax > SOC_CELL_VOLTAGE_MAX_MV))
	{
		return 0;
	}
	if (SOC_Enhance_Element.u16_VCellMax < SOC_Enhance_Element.u16_VCellMin)
	{
		return 0;
	}
	if (ModulusSub(SOC_Enhance_Element.u16_VCellMax, SOC_Enhance_Element.u16_VCellMin) >= SOC_CELL_VOLTAGE_MAX_DELTA_MV)
	{
		return 0;
	}
	return 1;
}

static void SOC_UpdateCapacityParam(void)
{
	SOC_Calculate_Element.u32CapFactory = (UINT32)SOC_Enhance_Element.u16_SOC_Ah * 3600;
	if (SOC_Calculate_Element.u32CapFactory == 0)
	{
		SOC_Calculate_Element.u32CapFactory = (UINT32)SOC_DEFAULT_CAPACITY_AH10 * 3600;
	}
	SOC_Calculate_Element.u32CycleT_Limit = (UINT32)SOC_Enhance_Element.u16_SOC_CycleT_Limit * 100;
}

static void SOC_SetSocAndCapacity(UINT8 soc)
{
	SOC_ResetSelfConsumeAccumulators();
	SOC_Calculate_Element.u8SOC_Now = SOC_ClampPercent(soc);
	if (SOC_IsCapacityValid())
	{
		SOC_Calculate_Element.u32CapNow = (UINT32)SOC_Calculate_Element.u8SOC_Now * SOC_Calculate_Element.u32CapFactory / 100;
	}
	else
	{
		SOC_Calculate_Element.u32CapNow = 0;
	}
}

static void SOC_NormalizeCapacity(void)
{
	SOC_Calculate_Element.u8SOC_Now = SOC_ClampPercent(SOC_Calculate_Element.u8SOC_Now);
	if (SOC_Calculate_Element.u32CapFull == 0 || SOC_Calculate_Element.u32CapFull > SOC_Calculate_Element.u32CapFactory)
	{
		SOC_Calculate_Element.u32CapFull = SOC_Calculate_Element.u32CapFactory;
	}
	if (SOC_Calculate_Element.u32CapNow > SOC_Calculate_Element.u32CapFactory)
	{
		SOC_Calculate_Element.u32CapNow = SOC_Calculate_Element.u32CapFactory;
	}
}

static void SOC_AddCapNow(UINT32 delta)
{
	if (!SOC_IsCapacityValid())
	{
		SOC_Calculate_Element.u32CapNow = 0;
		return;
	}
	if (SOC_Calculate_Element.u32CapNow >= SOC_Calculate_Element.u32CapFactory)
	{
		SOC_Calculate_Element.u32CapNow = SOC_Calculate_Element.u32CapFactory;
		return;
	}
	if (delta >= (SOC_Calculate_Element.u32CapFactory - SOC_Calculate_Element.u32CapNow))
	{
		SOC_Calculate_Element.u32CapNow = SOC_Calculate_Element.u32CapFactory;
	}
	else
	{
		SOC_Calculate_Element.u32CapNow += delta;
	}
}

static void SOC_SubCapNow(UINT32 delta)
{
	if (SOC_Calculate_Element.u32CapNow <= delta)
	{
		SOC_Calculate_Element.u32CapNow = 0;
	}
	else
	{
		SOC_Calculate_Element.u32CapNow -= delta;
	}
}

static void SOC_StepSocUp(void)
{
	if (SOC_Calculate_Element.u8SOC_Now < SOC_PERCENT_MAX)
	{
		++SOC_Calculate_Element.u8SOC_Now;
		SOC_AddCapNow(SOC_Calculate_Element.u32CapFactory / 100);
	}
}

static void SOC_StepSocDown(void)
{
	if (SOC_Calculate_Element.u8SOC_Now > 0)
	{
		--SOC_Calculate_Element.u8SOC_Now;
		SOC_SubCapNow(SOC_Calculate_Element.u32CapFactory / 100);
	}
}

static UINT32 SOC_CapChangeToPercent(UINT32 cap_change)
{
	if (!SOC_IsCapacityValid())
	{
		return 0;
	}
	return (UINT32)(((uint64_t)cap_change * 100) / SOC_Calculate_Element.u32CapFactory);
}

static UINT32 SOC_CapChangeRemainder(UINT32 cap_change)
{
	if (!SOC_IsCapacityValid())
	{
		return 0;
	}
	return (UINT32)(((((uint64_t)cap_change * 100) % SOC_Calculate_Element.u32CapFactory) + 50) / 100);
}

static UINT8 SOC_SelectStartupPercent(void)
{
	if (SOC_IsCellVoltageValid())
	{
		return SOC_ClampPercent(Get_OpenCircuit_Value());
	}
	return SOC_DEFAULT_START_PERCENT;
}

static UINT16 SOC_CapacityToAh100(UINT32 cap)
{
	UINT32 ah100;

	ah100 = cap / 360;
	if (ah100 > 0xFFFF)
	{
		return 0xFFFF;
	}
	return (UINT16)ah100;
}

static UINT16 SOC_CycleToU16(UINT32 cycle_times)
{
	UINT32 cycle;

	cycle = cycle_times / 100;
	if (cycle > 0xFFFF)
	{
		return 0xFFFF;
	}
	return (UINT16)cycle;
}

static void SOC_PassResultNow(void)
{
	UINT32 soh;

	SOC_NormalizeCapacity();
	SOC_Enhance_Element.u8_SOC = SOC_Calculate_Element.u8SOC_Now;
	if (!SOC_IsCapacityValid())
	{
		SOC_Enhance_Element.u8_SOH = 0;
	}
	else if (SOC_Calculate_Element.u32CapFull >= SOC_Calculate_Element.u32CapFactory)
	{
		SOC_Enhance_Element.u8_SOH = SOC_PERCENT_MAX;
	}
	else
	{
		soh = (UINT32)(((uint64_t)100 * SOC_Calculate_Element.u32CapFull) / SOC_Calculate_Element.u32CapFactory);
		SOC_Enhance_Element.u8_SOH = SOC_ClampPercent(soh);
	}
	SOC_Enhance_Element.u16_CapacityNow = SOC_CapacityToAh100(SOC_Calculate_Element.u32CapNow);
	SOC_Enhance_Element.u16_CapacityFull = SOC_CapacityToAh100(SOC_Calculate_Element.u32CapFull);
	SOC_Enhance_Element.u16_CapacityFactory = SOC_CapacityToAh100(SOC_Calculate_Element.u32CapFactory);
	SOC_Enhance_Element.u16_Cycle_times = SOC_CycleToU16(SOC_Calculate_Element.u32Cycle_times);
	SOC_Enhance_Element.u8_SOC_OCV_Cali = SOC_Calculate_Element.u8DSG_SOC_Int;
}
// 古瑞瓦特
const UINT16 SOC_Table_LiFePO[SOC_Size_LiFePO] = {
	3336,
	100,
	3332,
	90,
	3330,
	80,
	3327,
	75,
	3316,
	70,
	3301,
	65,
	3294,
	60,
	3291,
	55,
	3290,
	50,
	3288,
	45,
	3286,
	40,
	3279,
	35,
	3266,
	30,
	3254,
	25,
	3236,
	20,
	3212,
	15,
	3198,
	10,
	3112,
	5,
	2526,
	0,
	1000,
	0,
	1000,
	0,
};

// 单位为mV和SOC
const UINT16 SocTable_TernaryLi[SOC_Size_TernaryLi] = {
	4126,
	100,
	4066,
	95,
	4011,
	90,
	3955,
	85,
	3888,
	80,
	3837,
	75,
	3793,
	70,
	3756,
	65,
	3724,
	60,
	3699,
	55,
	3675,
	50,
	3658,
	45,
	3632,
	40,
	3605,
	35,
	3584,
	30,
	3557,
	25,
	3535,
	20,
	3497,
	15,
	3475,
	10,
	3371,
	5,
	3136,
	0,
};

// 单位为mV和SOC
const UINT16 SocTable_LiFePO2[SOC_Size_LiFePO2] = {
	3650,
	100,
	3600,
	98,
	3550,
	95,
	3500,
	92,
	3400,
	90,
	3350,
	87,
	3340,
	85,
	3335,
	82,
	3330,
	80,
	3325,
	78,
	3320,
	75,
	3300,
	70,
	3275,
	65,
	3250,
	60,
	3200,
	50,
	3150,
	45,
	3100,
	30,
	3000,
	20,
	2850,
	10,
	2750,
	5,
	2650,
	0,
};

void soc_factory_param_init_first(void)
{
	SOC_Enhance_Element.u16_SOC_Ah = OtherElement.u16Soc_Ah;
	SOC_Enhance_Element.u16_SOC_CycleT_Ever = OtherElement.u16Soc_Cycle_times;
	SOC_Enhance_Element.u16_SOC_CycleT_Limit = 5000;

	SOC_UpdateCapacityParam();
	SOC_Calculate_Element.u32Cycle_times = (UINT32)SOC_Enhance_Element.u16_SOC_CycleT_Ever * 100;
	SOC_Calculate_Element.u32CapFull = SOC_Calculate_Element.u32CapFactory;
	SOC_Calculate_Element.u32CapChange = 0;
	SOC_Calculate_Element.u8DSG_SOC_Int = 0;
	SOC_SetSocAndCapacity(SOC_DEFAULT_START_PERCENT);
	SOC_DealEEPROM_Data(EEPROM_DATA_REFRESH);
}

void soc_param_lib_init(void)
{

	// 外部获取的数据初始化
	SOC_UpdateCapacityParam();
	SOC_Calculate_Element.u32Cycle_times = (UINT32)SOC_Enhance_Element.u16_SOC_CycleT_Ever * 100;

	SOC_Calculate_Element.u32CapChange = 0;
	SOC_Calculate_Element.u8OCV_Cali_Flag = 0; // 第一次写置1出现了开机严重错误的问题
	SOC_Calculate_Element.u8CHG_AHCalcu_Flag = 0;
	SOC_Calculate_Element.u8DSG_AHCalcu_Flag = 0;

	SOC_Calculate_Element.u8SOC_Now = 0; // 以上均为0，因为模拟前端还没读回电压
	SOC_Calculate_Element.u32CapNow = 0;
	SOC_Calculate_Element.u8DSG_SOC_Int = 0;
	SOC_Calculate_Element.u32CapFull = 0;

	SOC_DealEEPROM_Data(EEPROM_DATA_READ);
	SOC_Enhance_Element.u16_SOC_InitOver = 1; // Soc初始化完毕
	SOC_PassResultNow();

	// extern void GetData_SOC(void);
	// GetData_SOC();
}

UINT8 Get_OpenCircuit_Value(void)
{
	UINT8 result = 0;

	if (!SOC_IsCellVoltageValid())
	{
		return SOC_ClampPercent(SOC_Calculate_Element.u8SOC_Now);
	}
	switch (SOC_Enhance_Element.u16_SOC_TableSelect)
	{
	case SOC_TABLE_TEST:
		result = GetEndValue(SOC_Enhance_Element.SOC_Table_CanSet, (UINT16)SOC_Size_TableCanSet, (UINT16)SOC_Enhance_Element.u16_VCellMin);
		break;
	case SOC_TABLE_LIFEPO:
		result = GetEndValue(SOC_Table_LiFePO, (UINT16)SOC_Size_LiFePO, (UINT16)SOC_Enhance_Element.u16_VCellMin);
		break;
	case SOC_TABLE_TERNARYLI:
		result = GetEndValue(SocTable_TernaryLi, (UINT16)SOC_Size_TernaryLi, (UINT16)SOC_Enhance_Element.u16_VCellMin);
		break;
	case SOC_TABLE_LIFEPO2:
		result = GetEndValue(SocTable_LiFePO2, (UINT16)SOC_Size_LiFePO2, (UINT16)SOC_Enhance_Element.u16_VCellMin);
		break;
	default:
		result = GetEndValue(SOC_Table_LiFePO, (UINT16)SOC_Size_TableCanSet, (UINT16)SOC_Enhance_Element.u16_VCellMin);
		break;
	}
	return SOC_ClampPercent(result);
}

// 末端校准
// 以锂智慧为范本
// 基于第一个末端SOC值总充不满，前提条件，校准后的电流值，宁愿偏大也不能偏小
void CorrectionTerminal_CV(enum _CUR CurrentType)
{
	static UINT16 su16_SocChgCal_L1_Tcnt = 0;
	static UINT16 su16_SocChgCal_L2_Tcnt = 0;
	static UINT16 su16_SocChgCal_L3_Tcnt = 0;
	static UINT16 su16_SocChgCal_L4_Tcnt = 0;

	static UINT16 su16_SocDsgCal_L1_Tcnt = 0;
	static UINT16 su16_SocDsgCal_L2_Tcnt = 0;
	static UINT16 su16_SocDsgCal_L3_Tcnt = 0;
	static UINT16 su16_SocDsgCal_L4_Tcnt = 0;
	if (!SOC_IsCellVoltageValid())
	{
		return;
	}

	switch (CurrentType)
	{
	case CurCHG:
		// SOC实际认为是100%的点，接近过充保护的时候
		// 本来想把内环校准值加上去的，但是想想这个系数不可控，算了算了，直接骗。
		// 以下这个点，假设我SOC相对不准，例如，大家都从0%开始计算，我最后算得SOC有90%(电流不准+板子本身功耗+时序有点误差)
		// 但实际已经满了，这个点一直没法处理。
		if (SOC_Enhance_Element.u16_VCellMax >= SOC_Enhance_Element.u16_SOC_100_Vol - 100 && SOC_Enhance_Element.u16_VCellMax < SOC_Enhance_Element.u16_SOC_100_Vol && SOC_Calculate_Element.u8SOC_Now < 95)
		{ // 和放电电流对应，第一段，必须拉到95%以内
			if (++su16_SocChgCal_L1_Tcnt >= SOC_TERMINAL_CAL_L1_200MS)
			{
				su16_SocChgCal_L1_Tcnt = 0;
				SOC_StepSocUp();
			}
		}
		else if (SOC_Enhance_Element.u16_VCellMax >= SOC_Enhance_Element.u16_SOC_100_Vol && SOC_Calculate_Element.u8SOC_Now < 100)
		{
			if (SOC_Calculate_Element.u8SOC_Now > 95)
			{
				if (++su16_SocChgCal_L2_Tcnt >= SOC_TERMINAL_CAL_L2_200MS)
				{
					su16_SocChgCal_L2_Tcnt = 0;
					SOC_StepSocUp();
				}
			}
			else
			{
				if (++su16_SocChgCal_L3_Tcnt >= SOC_TERMINAL_CAL_L3_200MS)
				{
					su16_SocChgCal_L3_Tcnt = 0;
					SOC_StepSocUp();
				}
			}
		}

		// 这是基于充电必须能达到100%的终极做法，2S + 1%
		if (SOC_Enhance_Element.u16_VCellMax >= SOC_Enhance_Element.u16_SOC_100_Vol + 50 && SOC_Calculate_Element.u8SOC_Now < 100)
		{
			if (++su16_SocChgCal_L4_Tcnt >= SOC_TERMINAL_CAL_FORCE_200MS)
			{
				su16_SocChgCal_L4_Tcnt = 0;
				SOC_StepSocUp();
			}
		}

#ifdef _CAL_SLOW_DOWN_CHG
		// 这里会出现回退的现象，就是末端，断开管子瞬间，电压下降200mV(类似)，此时SOC已经100%，
		// 但是由于电流计算是有权重的，变为0可能需要几秒，此时会回退到98，也即从100-98
		// 如果执行以上的几个情况，这个就不会执行，
		if (SOC_Calculate_Element.u8SOC_Now >= 98 && SOC_Enhance_Element.u16_VCellMax < SOC_Enhance_Element.u16_SOC_100_Vol)
		{
			// SOC_Calculate_Element.u8SOC_Now = 98;
			SOC_Calculate_Element.u8SOC_Now = SOC_Calculate_Element.u8SOC_Now; // SOC保持不变
			SOC_Calculate_Element.u32CapChange = 0;							   // 把这个累加量清零便可，还有这个漏洞，会回退1
			SOC_Calculate_Element.u32CapNow = (UINT32)SOC_Calculate_Element.u8SOC_Now * SOC_Calculate_Element.u32CapFactory / 100;
		}
#endif

		if (su16_SocDsgCal_L1_Tcnt)
			su16_SocDsgCal_L1_Tcnt = 0;
		if (su16_SocDsgCal_L2_Tcnt)
			su16_SocDsgCal_L2_Tcnt = 0;
		if (su16_SocDsgCal_L3_Tcnt)
			su16_SocDsgCal_L3_Tcnt = 0;
		if (su16_SocDsgCal_L4_Tcnt)
			su16_SocDsgCal_L4_Tcnt = 0;
		break;

	case CurDSG:
		if (SOC_Enhance_Element.u16_VCellMin <= SOC_Enhance_Element.u16_SOC_0_Vol + 100 && SOC_Enhance_Element.u16_VCellMin > SOC_Enhance_Element.u16_SOC_0_Vol && SOC_Calculate_Element.u8SOC_Now > 5)
		{
			if (++su16_SocDsgCal_L1_Tcnt >= SOC_TERMINAL_CAL_L1_200MS)
			{ // 第一级校准
				su16_SocDsgCal_L1_Tcnt = 0;
				SOC_StepSocDown();
			}
		}
		else if (SOC_Enhance_Element.u16_VCellMin <= SOC_Enhance_Element.u16_SOC_0_Vol && SOC_Calculate_Element.u8SOC_Now > 0)
		{ // 我也不知道为什么要5%，想想，直接0%，与下面两个行成闭循环
			if (SOC_Calculate_Element.u8SOC_Now < 5)
			{ // 第二级校准
				if (++su16_SocDsgCal_L2_Tcnt >= SOC_TERMINAL_CAL_L2_200MS)
				{										  // 电科大电流还是有一定的概率留下1%，从10改为8吧。
					su16_SocDsgCal_L2_Tcnt = 0;			  // 但是兼顾小电流能放久一些，不能改为6
					SOC_StepSocDown();
				}
			}
			else
			{ // 快没电了，还有很大的SOC
				if (++su16_SocDsgCal_L3_Tcnt >= SOC_TERMINAL_CAL_L3_200MS)
				{ // 第三级校准
					su16_SocDsgCal_L3_Tcnt = 0;
					SOC_StepSocDown();
				}
			}
		}

		// 这是基于放电必须能达到0%的终极做法，2S - 1%
		// 但实际上放电要求没充电高
		if (SOC_Enhance_Element.u16_VCellMin <= SOC_Enhance_Element.u16_SOC_0_Vol - 50 && SOC_Calculate_Element.u8SOC_Now > 0)
		{
			if (++su16_SocDsgCal_L4_Tcnt >= SOC_TERMINAL_CAL_FORCE_200MS)
			{
				su16_SocDsgCal_L4_Tcnt = 0;
				SOC_StepSocDown();
			}
		}

		if (SOC_Calculate_Element.u8SOC_Now <= 1 && SOC_Enhance_Element.u16_VCellMin > SOC_Enhance_Element.u16_SOC_0_Vol)
		{
			// SOC_Calculate_Element.u8SOC_Now = 2;
			SOC_Calculate_Element.u8SOC_Now = SOC_Calculate_Element.u8SOC_Now; // SOC保持不变
			SOC_Calculate_Element.u32CapChange = 0;							   // 把这个累加量清零便可，还有这个漏洞，会回退1
			SOC_Calculate_Element.u32CapNow = (UINT32)SOC_Calculate_Element.u8SOC_Now * SOC_Calculate_Element.u32CapFactory / 100;
		}

		if (su16_SocChgCal_L1_Tcnt)
			su16_SocChgCal_L1_Tcnt = 0;
		if (su16_SocChgCal_L2_Tcnt)
			su16_SocChgCal_L2_Tcnt = 0;
		if (su16_SocChgCal_L3_Tcnt)
			su16_SocChgCal_L3_Tcnt = 0;
		if (su16_SocChgCal_L4_Tcnt)
			su16_SocChgCal_L4_Tcnt = 0;
		break;

	default:
		break;
	}
}

// 末端大电流恒流充，调用的函数
// 本来打算合成一个函数，但是想想后续可能会有不同的策略，决定分开
// 多级保护，有个BUG，就是电压上涨太快，算不过来
void CorrectionTerminal_CC(enum _CUR CurrentType)
{
}

void Correction_Terminal(enum _CUR CurrentType)
{

	switch (CurrentType)
	{
	case CurCHG:
		switch (SOC_Enhance_Element.u8_LargeCurFlag_Chg)
		{
		case 0:
			CorrectionTerminal_CV(CurrentType);
			break;
		case 1:
			CorrectionTerminal_CC(CurrentType);
			break;
		default:
			break;
		}
		break;

	case CurDSG:
		switch (SOC_Enhance_Element.u8_LargeCurFlag_Dsg)
		{
		case 0:
			CorrectionTerminal_CV(CurrentType);
			break;
		case 1:
			CorrectionTerminal_CC(CurrentType);
			break;
		default:
			break;
		}
		break;

	default:
		break;
	}
}

static UINT8 s_u8SOC_CalcDirection = SOC_DIR_NONE;
static void SOC_SetCalcDirection(UINT8 direction)
{
	if (s_u8SOC_CalcDirection != direction)
	{
		s_u8SOC_CalcDirection = direction;
		SOC_Calculate_Element.u32CapChange = 0;
		SOC_Calculate_Element.u8CHG_AHCalcu_Flag = 0;
		SOC_Calculate_Element.u8DSG_AHCalcu_Flag = 0;
		SOC_ResetSelfConsumeAccumulators();
	}
}

static UINT32 SOC_CurrentMaToCapDelta(UINT32 current_ma, UINT32 *remain_ma_ms)
{
	uint64_t ma_ms;

	ma_ms = (uint64_t)current_ma * SOC_CONTROL_PERIOD_MS + *remain_ma_ms;
	*remain_ma_ms = (UINT32)(ma_ms % SOC_CAP_UNIT_MA_MS);
	return (UINT32)(ma_ms / SOC_CAP_UNIT_MA_MS);
}

static UINT32 SOC_CurrentA10ToCapDelta(UINT16 current_a10, UINT32 *remain_ma_ms)
{
	return SOC_CurrentMaToCapDelta((UINT32)current_a10 * SOC_CURRENT_A10_TO_MA, remain_ma_ms);
}

static void SOC_AccumulateDischargeCycle(UINT32 percent)
{
	UINT32 dsg_acc;
	if ((percent == 0) || (SOC_Calculate_Element.u8SOC_Now == 0))
	{
		return;
	}
	dsg_acc = (UINT32)SOC_Calculate_Element.u8DSG_SOC_Int + percent;
	SOC_Calculate_Element.u32Cycle_times += (dsg_acc / SOC_DSG_CYCLE_PERCENT) * 100;
	SOC_Calculate_Element.u8DSG_SOC_Int = (UINT8)(dsg_acc % SOC_DSG_CYCLE_PERCENT);
}

static void SOC_ApplyCapacityDelta(UINT32 delta, UINT8 is_charge)
{
	UINT32 C_change_per;
	if ((delta == 0) || !SOC_IsCapacityValid())
	{
		return;
	}
	SOC_Calculate_Element.u8SOC_Old = SOC_ClampPercent(SOC_Calculate_Element.u8SOC_Now);
	if ((UINT32)0xFFFFFFFF - SOC_Calculate_Element.u32CapChange < delta)
	{
		SOC_Calculate_Element.u32CapChange = (UINT32)0xFFFFFFFF;
	}
	else
	{
		SOC_Calculate_Element.u32CapChange += delta;
	}
	if (is_charge)
	{
		SOC_AddCapNow(delta);
	}
	else
	{
		SOC_SubCapNow(delta);
	}
	C_change_per = SOC_CapChangeToPercent(SOC_Calculate_Element.u32CapChange);
	if (C_change_per > 0)
	{
		if (is_charge)
		{
			if (C_change_per >= (UINT32)(SOC_PERCENT_MAX - SOC_Calculate_Element.u8SOC_Old))
			{
				SOC_Calculate_Element.u8SOC_Now = SOC_PERCENT_MAX;
			}
			else
			{
				SOC_Calculate_Element.u8SOC_Now = SOC_Calculate_Element.u8SOC_Old + (UINT8)C_change_per;
			}
		}
		else
		{
			if (C_change_per >= SOC_Calculate_Element.u8SOC_Old)
			{
				SOC_Calculate_Element.u8SOC_Now = 0;
			}
			else
			{
				SOC_Calculate_Element.u8SOC_Now = SOC_Calculate_Element.u8SOC_Old - (UINT8)C_change_per;
			}
			SOC_AccumulateDischargeCycle(C_change_per);
		}
	}
	SOC_Calculate_Element.u32CapChange = SOC_CapChangeRemainder(SOC_Calculate_Element.u32CapChange);
}

static void SOC_ApplySelfConsumeTick(void)
{
#if SOC_SELF_CONSUMPTION_ENABLE
	UINT32 delta;
	UINT32 C_change_per;

	if ((SOC_SELF_CONSUMPTION_CURRENT_MA == 0) || !SOC_IsCapacityValid() || (SOC_Calculate_Element.u8SOC_Now == 0))
	{
		SOC_ResetSelfConsumeAccumulators();
		return;
	}

	delta = SOC_CurrentMaToCapDelta((UINT32)SOC_SELF_CONSUMPTION_CURRENT_MA, &s_u32SelfConsumeMaMs);
	if (delta == 0)
	{
		return;
	}

	SOC_SubCapNow(delta);
	if (SOC_Calculate_Element.u32CapNow == 0)
	{
		SOC_Calculate_Element.u8SOC_Now = 0;
		SOC_ResetSelfConsumeAccumulators();
		return;
	}

	if ((UINT32)0xFFFFFFFF - s_u32SelfConsumeCapChange < delta)
	{
		s_u32SelfConsumeCapChange = (UINT32)0xFFFFFFFF;
	}
	else
	{
		s_u32SelfConsumeCapChange += delta;
	}

	C_change_per = SOC_CapChangeToPercent(s_u32SelfConsumeCapChange);
	if (C_change_per > 0)
	{
		if (C_change_per >= SOC_Calculate_Element.u8SOC_Now)
		{
			SOC_Calculate_Element.u8SOC_Now = 0;
		}
		else
		{
			SOC_Calculate_Element.u8SOC_Now -= (UINT8)C_change_per;
		}
		s_u32SelfConsumeCapChange = SOC_CapChangeRemainder(s_u32SelfConsumeCapChange);
	}
#endif
}

static void SOC_RestOcvCorrectionTick(UINT8 enable)
{
	static UINT16 s_u16RestTicks = 0;
	static UINT16 s_u16RestStepTicks = 0;
	UINT8 ocv_soc;

	if (!enable)
	{
		s_u16RestTicks = 0;
		s_u16RestStepTicks = 0;
		return;
	}
	if (!SOC_IsCellVoltageValid() || !SOC_IsCapacityValid())
	{
		s_u16RestTicks = 0;
		s_u16RestStepTicks = 0;
		return;
	}
	if (s_u16RestTicks < SOC_REST_OCV_START_DELAY_200MS)
	{
		++s_u16RestTicks;
		return;
	}

	if ((SOC_Enhance_Element.u16_VCellMax >= SOC_Enhance_Element.u16_SOC_100_Vol) && (SOC_Enhance_Element.u16_VCellMin >= SOC_FULL_CELL_MIN_MV))
	{
		SOC_SetSocAndCapacity(SOC_PERCENT_MAX);
		SOC_Calculate_Element.u32CapChange = 0;
		return;
	}
	if ((SOC_Enhance_Element.u16_VCellMin <= SOC_Enhance_Element.u16_SOC_0_Vol) && (SOC_Enhance_Element.u16_VCellMin >= SOC_CELL_VOLTAGE_MIN_MV))
	{
		SOC_SetSocAndCapacity(0);
		SOC_Calculate_Element.u32CapChange = 0;
		return;
	}

	if (++s_u16RestStepTicks < SOC_REST_OCV_STEP_PERIOD_200MS)
	{
		return;
	}
	s_u16RestStepTicks = 0;
	ocv_soc = SOC_ClampPercent(Get_OpenCircuit_Value());
	if ((UINT16)ocv_soc > ((UINT16)SOC_Calculate_Element.u8SOC_Now + SOC_REST_OCV_DEADBAND_PERCENT))
	{
		SOC_StepSocUp();
		SOC_Calculate_Element.u32CapChange = 0;
	}
	else if (((UINT16)SOC_Calculate_Element.u8SOC_Now > (UINT16)ocv_soc + SOC_REST_OCV_DEADBAND_PERCENT) && (SOC_Calculate_Element.u8SOC_Now > 0))
	{
		SOC_StepSocDown();
		SOC_Calculate_Element.u32CapChange = 0;
	}
}

static void SOC_Run200msCalculation(void)
{
	static UINT32 s_u32ChgMaMsRemain = 0;
	static UINT32 s_u32DsgMaMsRemain = 0;
	UINT32 cap_delta;

	if (isCHG())
	{
		SOC_SetCalcDirection(SOC_DIR_CHG);
		SOC_RestOcvCorrectionTick(0);
		s_u32DsgMaMsRemain = 0;
		Correction_Terminal(CurCHG);
		cap_delta = SOC_CurrentA10ToCapDelta(SOC_Enhance_Element.u16_Ichg, &s_u32ChgMaMsRemain);
		SOC_ApplyCapacityDelta(cap_delta, 1);
		if ((UINT32)0xFFFFFFFF - SOC_Calculate_Element.u32CapFull_Cal_As < cap_delta)
		{
			SOC_Calculate_Element.u32CapFull_Cal_As = (UINT32)0xFFFFFFFF;
		}
		else
		{
			SOC_Calculate_Element.u32CapFull_Cal_As += cap_delta;
		}
	}
	else if (isDSG())
	{
		SOC_SetCalcDirection(SOC_DIR_DSG);
		SOC_RestOcvCorrectionTick(0);
		s_u32ChgMaMsRemain = 0;
		Correction_Terminal(CurDSG);
		cap_delta = SOC_CurrentA10ToCapDelta(SOC_Enhance_Element.u16_Idsg, &s_u32DsgMaMsRemain);
		SOC_ApplyCapacityDelta(cap_delta, 0);
	}
	else
	{
		SOC_SetCalcDirection(SOC_DIR_NONE);
		s_u32ChgMaMsRemain = 0;
		s_u32DsgMaMsRemain = 0;
		SOC_RestOcvCorrectionTick(1);
		SOC_ApplySelfConsumeTick();
	}
}
void SOC_DealEEPROM_Data(enum EEPROM_COMMAND Command)
{
	UINT16 temp = 0;
	UINT16 cycle_limit = 0;

	switch (Command)
	{
	case EEPROM_DATA_REFRESH:
		WriteEEPROM_Word_NoZone(E2P_ADDR_SOC, SOC_Calculate_Element.u8SOC_Now);
		WriteEEPROM_Word_NoZone(E2P_ADDR_DSG_SOC_Int, SOC_Calculate_Element.u8DSG_SOC_Int);
		WriteEEPROM_Word_NoZone(E2P_ADDR_CYCLE_TIMES, SOC_CycleToU16(SOC_Calculate_Element.u32Cycle_times));
		break;
	case EEPROM_DATA_READ:
		temp = ReadEEPROM_Word_NoZone(E2P_ADDR_SOC);
		if (temp <= SOC_PERCENT_MAX)
		{
			SOC_Calculate_Element.u8SOC_Now = (UINT8)temp;
		}
		else
		{
			SOC_Calculate_Element.u8SOC_Now = SOC_SelectStartupPercent();
		}

		temp = ReadEEPROM_Word_NoZone(E2P_ADDR_DSG_SOC_Int);
		if (temp < SOC_DSG_CYCLE_PERCENT)
		{
			SOC_Calculate_Element.u8DSG_SOC_Int = (UINT8)temp;
		}
		else
		{
			SOC_Calculate_Element.u8DSG_SOC_Int = 0;
		}

		temp = ReadEEPROM_Word_NoZone(E2P_ADDR_CYCLE_TIMES);
		cycle_limit = (UINT16)(SOC_Calculate_Element.u32CycleT_Limit / 100);
		if (temp == EEPROM_VALUE_STORE_RESET)
		{
			temp = SOC_Enhance_Element.u16_SOC_CycleT_Ever;
		}
		if ((cycle_limit > 0) && (temp > cycle_limit))
		{
			temp = cycle_limit;
		}
		SOC_Calculate_Element.u32Cycle_times = (UINT32)temp * 100;
		SOC_Calculate_Element.u32CapFull = SOC_Calculate_Element.u32CapFactory;
		SOC_SetSocAndCapacity(SOC_Calculate_Element.u8SOC_Now);
		SOC_Calculate_Element_backup = SOC_Calculate_Element;
		break;
	default:
		break;
	}
}

void SOC_Update_StartUp(void)
{
	switch (SOC_Enhance_Element.u16_RefreshData_Flag)
	{
	case 1:
		SOC_SetSocAndCapacity(SOC_SelectStartupPercent());
		SOC_Calculate_Element.u32CapChange = 0;
		break;

	case 2: // SOC归零类型，改为循环次数归初始化
		SOC_Calculate_Element.u8DSG_SOC_Int = 0;
		SOC_Calculate_Element.u32CapChange = 0;
		SOC_UpdateCapacityParam();
		SOC_Calculate_Element.u32Cycle_times = (UINT32)SOC_Enhance_Element.u16_SOC_CycleT_Ever * 100;
		SOC_Calculate_Element.u32CapFull = SOC_Calculate_Element.u32CapFactory;
		SOC_SetSocAndCapacity(SOC_Calculate_Element.u8SOC_Now);
		break;

	case 3:
		SOC_SetSocAndCapacity(SOC_Enhance_Element.u8_SetSocOnce);
		SOC_Calculate_Element.u32CapChange = 0;
		break;

	default:
		SOC_SetSocAndCapacity(SOC_Calculate_Element.u8SOC_Now);
		break;
	}
	SOC_Calculate_Element.u8_DataUpdateOK = 1;
	SOC_SetCalcDirection(SOC_DIR_NONE);
}

/*
1，存SOC值，变化1%即存
2，循环次数下降数值，少1%即存
3，循环次数，多一个循环即存
4，关于这些EEPROM的数值的问题
   A，如果是第一次用这个EEPROM怎么处理？
   B，如果期间换电池了呢？
5，目前就这三个需要处理，后续关于运行期间掉电怎么处理后续再说，系数之类的一定要存的
*/
void SOC_EEPROM_Deal_Monitor(void)
{
	if (!SOC_Enhance_Element.u16_SOC_InitOver)
	{ // 初始化完才开始这个函数
		return;
	}

	if (SOC_Calculate_Element.u8SOC_Now != SOC_Calculate_Element_backup.u8SOC_Now)
	{
		SOC_Calculate_Element_backup.u8SOC_Now = SOC_Calculate_Element.u8SOC_Now;
		WriteEEPROM_Word_NoZone(E2P_ADDR_SOC, SOC_Calculate_Element.u8SOC_Now);
	}

	if (SOC_Calculate_Element.u8DSG_SOC_Int != SOC_Calculate_Element_backup.u8DSG_SOC_Int)
	{
		SOC_Calculate_Element_backup.u8DSG_SOC_Int = SOC_Calculate_Element.u8DSG_SOC_Int;
		WriteEEPROM_Word_NoZone(E2P_ADDR_DSG_SOC_Int, SOC_Calculate_Element.u8DSG_SOC_Int);
	}

	if (SOC_Calculate_Element.u32Cycle_times != SOC_Calculate_Element_backup.u32Cycle_times)
	{
		SOC_Calculate_Element_backup.u32Cycle_times = SOC_Calculate_Element.u32Cycle_times;
		WriteEEPROM_Word_NoZone(E2P_ADDR_CYCLE_TIMES, SOC_CycleToU16(SOC_Calculate_Element.u32Cycle_times));
	}
}

void SOC_RefreshData_Monitor(void)
{
	static UINT8 su8_DataRefreshFlag = 0;

	if (!SOC_Enhance_Element.u16_SOC_InitOver)
	{
		return;
	}

	switch (su8_DataRefreshFlag)
	{
	case 0:
		if (SOC_Enhance_Element.u16_RefreshData_Flag)
		{
			su8_DataRefreshFlag = 1;
			SOC_Update_StartUp();
		}
		break;

	case 1:
		if (SOC_Calculate_Element.u8_DataUpdateOK == 1)
		{											   // 3个地方初始化
			SOC_Calculate_Element.u8_DataUpdateOK = 0; // 这个思路留着。不改
			SOC_Enhance_Element.u16_RefreshData_Flag = 0;
			su8_DataRefreshFlag = 0;
		}
		break;
	default:
		break;
	}
}

void SOC_Result_Pass(void)
{
	static UINT8 su8_TimeCnt = 0;
	if (++su8_TimeCnt < 5)
	{
		return;
	}
	su8_TimeCnt = 0;

	SOC_PassResultNow();
}

void InitSOC_IntEnhance(void)
{
	// 外部获取的数据初始化
	SOC_UpdateCapacityParam();
	SOC_Calculate_Element.u32Cycle_times = (UINT32)SOC_Enhance_Element.u16_SOC_CycleT_Ever * 100;

	SOC_Calculate_Element.u32CapChange = 0;
	SOC_Calculate_Element.u8OCV_Cali_Flag = 0; // 第一次写置1出现了开机严重错误的问题
	SOC_Calculate_Element.u8CHG_AHCalcu_Flag = 0;
	SOC_Calculate_Element.u8DSG_AHCalcu_Flag = 0;

	SOC_Calculate_Element.u8SOC_Now = 0; // 以上均为0，因为模拟前端还没读回电压
	SOC_Calculate_Element.u32CapNow = 0;
	SOC_Calculate_Element.u8DSG_SOC_Int = 0;
	SOC_Calculate_Element.u32CapFull = 0;

	SOC_Enhance_Element.u16_SOC_InitOver = 0; // 对外标志位初始化
	SOC_SetCalcDirection(SOC_DIR_NONE);
}

UINT8 isCHG(void)
{
	// return g_stCellInfoReport.u16Ichg > SOC_VIRTUAL_CURRENT_CHG ? 1 : 0;
	return SOC_Enhance_Element.u16_Ichg >= SOC_VIRTUAL_CURRENT_CHG ? 1 : 0;
}

UINT8 isDSG(void)
{
	// return g_stCellInfoReport.u16IDischg > SOC_VIRTUAL_CURRENT_DSG ? 1 : 0;
	return SOC_Enhance_Element.u16_Idsg >= SOC_VIRTUAL_CURRENT_DSG ? 1 : 0;
}


void soc_cali(void)
{
	static uint8_t dsg_soc0_delay = 0;
	static uint8_t chg_soc100_delay = 0;
#ifdef _SOC_OCV_Fix2_func_
	SOC_OCV_Fix2();
#endif
	if (!SOC_IsCellVoltageValid())
	{
		dsg_soc0_delay = 0;
		chg_soc100_delay = 0;
		return;
	}
	if (isCHG())
	{
		dsg_soc0_delay = 0;
		if ((SOC_Enhance_Element.u16_VCellMax >= SOC_Enhance_Element.u16_SOC_100_Vol) && (SOC_Enhance_Element.u16_VCellMin >= SOC_FULL_CELL_MIN_MV))
		{
			if (++chg_soc100_delay >= SOC_FULL_CALI_HOLD_200MS)
			{
				chg_soc100_delay = SOC_FULL_CALI_HOLD_200MS;
				SOC_Calculate_Element.u8SOC_Now = SOC_PERCENT_MAX;
				SOC_Calculate_Element.u32CapNow = SOC_Calculate_Element.u32CapFull;
				SOC_Calculate_Element.u32CapChange = 0;
				SOC_ResetSelfConsumeAccumulators();
			}
		}
		else
		{
			chg_soc100_delay = 0;
		}
	}
	else if (isDSG())
	{
		chg_soc100_delay = 0;
		if ((SOC_Enhance_Element.u16_VCellMin <= SOC_Enhance_Element.u16_SOC_0_Vol) && (SOC_Enhance_Element.u16_VCellMin >= SOC_CELL_VOLTAGE_MIN_MV))
		{
			if (++dsg_soc0_delay >= SOC_EMPTY_CALI_HOLD_200MS)
			{
				dsg_soc0_delay = SOC_EMPTY_CALI_HOLD_200MS;
				SOC_SetSocAndCapacity(0);
				SOC_Calculate_Element.u32CapChange = 0;
			}
		}
		else
		{
			dsg_soc0_delay = 0;
		}
	}
	else
	{
		dsg_soc0_delay = 0;
		chg_soc100_delay = 0;
	}
}
/*
>>??>后记：
1，这个做法会出现一个问题，SOC加速，容量膨胀，然后静置之后，SOC保持不变，但是满电容量减少(因为满电容量是实打实计算的)。
   这样，剩余容量就突然减少了，会有分歧。如果此时SOC计算还没有100%(差距过大，当前满电容量太大)，直到40%这个样子，剩余容量会更少。
2，回到实际情况，用久衰减的电池，也会出现同样的情况，但是末端一定要小电流操作，使其充到100%。
   这样的话，当前容量虽然减少了，但是乘以100%，也差距不会太大。
3，结合1和2，容量最好不要显示，只显示SOC，SOH和出厂容量为妙。
*/
void SOC_IntEnhance_Ctrl(void)
{
	SOC_Run200msCalculation();
	soc_cali();
	// ???????????????????????????????????????
	SOC_EEPROM_Deal_Monitor();
	SOC_RefreshData_Monitor(); // ???????>>??????
	SOC_Result_Pass();
}
