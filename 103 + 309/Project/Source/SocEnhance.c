#include "SocEnhance.h"
#include "PubFunc.h"
#include "conf.h"
#include "EEPROM.h"
#include "DataDeal.h"
#include "Flash.h"
#include "Sci_Upper.h"
#include "System_Monitor.h"
#include <string.h>

#ifndef STORAGE_FLASH_SOC_API_DECLARED
typedef struct
{
	UINT16 u16SocNow;
	UINT16 u16DsgSocInt;
	UINT32 u32CycleTimes;
} STORAGE_FLASH_SOC_DATA;

extern UINT8 StorageFlash_LoadSocData(STORAGE_FLASH_SOC_DATA *data);
extern UINT8 StorageFlash_SaveSocData(const STORAGE_FLASH_SOC_DATA *data);
#endif

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

// 充电可以提前充满，但是不能卡死
// #define _CAL_SLOW_DOWN_CHG

// typedef enum _CUR
//{
//	CurCHG = 0,
//	CurDSG
// } _Cur;

enum SOC_CALI_STATE
{
	// SOC_CALI_DATA_INIT = 0,
	// SOC_CALI_STARTUP,
	SOC_CALI_STATE_TRANSFER,
	SOC_CALI_CONT_CHG,
	SOC_CALI_CONT_DSG,
};

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

enum SOC_CALI_STATE SOC_Cali_Flag = SOC_CALI_STATE_TRANSFER; // 妈的，忘了这个？		SOC计算状态机，记得初始化

#define SOC_REST_BUCKET_1_SECONDS ((UINT32)600)
#define SOC_REST_BUCKET_2_SECONDS ((UINT32)1800)
#define SOC_REST_BUCKET_3_SECONDS ((UINT32)3600)
#define SOC_REST_BUCKET_4_SECONDS ((UINT32)21600)
#define SOC_REST_SECONDS_PER_TICK ((UINT32)5)
#define SOC_WEAK_CELL_GUARD_WINDOW_MV ((UINT16)120)
#define SOC_WEAK_CELL_CRITICAL_WINDOW_MV ((UINT16)30)

struct SOC_RUNTIME_CONTEXT
{
	UINT8 u8DisplaySoc;
	UINT8 u8DisplayReady;
	UINT8 u8RestBucketApplied;
	UINT8 u8Reserved;
	UINT32 u32RestTicks;
};

static struct SOC_RUNTIME_CONTEXT g_soc_runtime;

void SOC_DealEEPROM_Data(enum EEPROM_COMMAND Command);
static void SOC_ResetRuntimeContext(void);
static UINT8 SOC_IsVoltageValid(void);
static UINT32 SOC_GetCapBase(void);
static void SOC_SetSocValue(UINT8 soc_now, UINT8 clear_cap_change);
static void SOC_ApplySocNow(UINT8 soc_now);
static void SOC_ApplySocCorrection(UINT8 soc_now);
static UINT8 SOC_StepTowardTarget(UINT8 current_soc, UINT8 target_soc, UINT8 max_step);
static void SOC_PersistSnapshotIfChanged(void);
static UINT8 SOC_GetRestBucket(UINT32 rest_seconds);
static void SOC_ApplyRestCompensation(UINT32 rest_seconds);
static void SOC_UpdateRestMonitor(void);
static void SOC_ApplyWeakCellGuard(void);
static void SOC_UpdateDisplaySoc(void);
static void SOC_SyncOutputData(UINT8 force_display_follow);
UINT8 Get_OpenCircuit_Value(void);
UINT8 isCHG(void);
UINT8 isDSG(void);
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

static void SOC_ResetRuntimeContext(void)
{
	memset(&g_soc_runtime, 0, sizeof(g_soc_runtime));
}

static UINT8 SOC_IsVoltageValid(void)
{
	return (SOC_Enhance_Element.u16_VCellMin >= 2000U) ? 1U : 0U;
}

static UINT32 SOC_GetCapBase(void)
{
	if (SOC_Calculate_Element.u32CapFull != 0U)
	{
		return SOC_Calculate_Element.u32CapFull;
	}

	return SOC_Calculate_Element.u32CapFactory;
}

static void SOC_SetSocValue(UINT8 soc_now, UINT8 clear_cap_change)
{
	UINT32 cap_base;

	if (soc_now > 100U)
	{
		soc_now = 100U;
	}

	cap_base = SOC_GetCapBase();
	SOC_Calculate_Element.u8SOC_Now = soc_now;
	if (cap_base == 0U)
	{
		SOC_Calculate_Element.u32CapNow = 0U;
	}
	else
	{
		SOC_Calculate_Element.u32CapNow = (UINT32)soc_now * cap_base / 100U;
	}

	if (clear_cap_change)
	{
		SOC_Calculate_Element.u32CapChange = 0U;
	}
}

static void SOC_ApplySocNow(UINT8 soc_now)
{
	SOC_SetSocValue(soc_now, 1U);
}

static void SOC_ApplySocCorrection(UINT8 soc_now)
{
	SOC_SetSocValue(soc_now, 0U);
}

static UINT8 SOC_StepTowardTarget(UINT8 current_soc, UINT8 target_soc, UINT8 max_step)
{
	UINT8 step;

	if ((max_step == 0U) || (current_soc == target_soc))
	{
		return current_soc;
	}

	if (current_soc < target_soc)
	{
		step = (UINT8)(target_soc - current_soc);
		if (step > max_step)
		{
			step = max_step;
		}
		return (UINT8)(current_soc + step);
	}

	step = (UINT8)(current_soc - target_soc);
	if (step > max_step)
	{
		step = max_step;
	}
	return (UINT8)(current_soc - step);
}

static void SOC_PersistSnapshotIfChanged(void)
{
	if (!SOC_Enhance_Element.u16_SOC_InitOver)
	{
		return;
	}

	if ((SOC_Calculate_Element.u8SOC_Now != SOC_Calculate_Element_backup.u8SOC_Now) ||
		(SOC_Calculate_Element.u8DSG_SOC_Int != SOC_Calculate_Element_backup.u8DSG_SOC_Int) ||
		(SOC_Calculate_Element.u32Cycle_times != SOC_Calculate_Element_backup.u32Cycle_times))
	{
		SOC_Calculate_Element_backup.u8SOC_Now = SOC_Calculate_Element.u8SOC_Now;
		SOC_Calculate_Element_backup.u8DSG_SOC_Int = SOC_Calculate_Element.u8DSG_SOC_Int;
		SOC_Calculate_Element_backup.u32Cycle_times = SOC_Calculate_Element.u32Cycle_times;
		SOC_DealEEPROM_Data(EEPROM_DATA_REFRESH);
	}
}

static UINT8 SOC_GetRestBucket(UINT32 rest_seconds)
{
	if (rest_seconds < SOC_REST_BUCKET_1_SECONDS)
	{
		return 0U;
	}
	if (rest_seconds < SOC_REST_BUCKET_2_SECONDS)
	{
		return 1U;
	}
	if (rest_seconds < SOC_REST_BUCKET_3_SECONDS)
	{
		return 2U;
	}
	if (rest_seconds < SOC_REST_BUCKET_4_SECONDS)
	{
		return 3U;
	}

	return 4U;
}

static void SOC_ApplyRestCompensation(UINT32 rest_seconds)
{
	UINT8 bucket;
	UINT8 target_soc;
	UINT8 current_soc;
	UINT8 new_soc;
	UINT8 max_step = 0U;

	if (!SOC_Enhance_Element.u16_SOC_InitOver)
	{
		return;
	}

	if (!SOC_IsVoltageValid() || isCHG() || isDSG())
	{
		return;
	}

	bucket = SOC_GetRestBucket(rest_seconds);
	if ((bucket == 0U) || (bucket <= g_soc_runtime.u8RestBucketApplied))
	{
		return;
	}

	target_soc = Get_OpenCircuit_Value();
	current_soc = SOC_Calculate_Element.u8SOC_Now;
	if (target_soc < current_soc)
	{
		if (bucket >= 4U)
		{
			max_step = 3U;
		}
		else if (bucket >= 3U)
		{
			max_step = 2U;
		}
		else
		{
			max_step = 1U;
		}
	}
	else if ((target_soc > current_soc) && (current_soc >= 90U) && (bucket >= 3U))
	{
		max_step = (bucket >= 4U) ? 2U : 1U;
	}

	new_soc = SOC_StepTowardTarget(current_soc, target_soc, max_step);
	if (new_soc != current_soc)
	{
		SOC_ApplySocCorrection(new_soc);
	}
	g_soc_runtime.u8RestBucketApplied = bucket;
}

static void SOC_UpdateRestMonitor(void)
{
	UINT32 rest_seconds;

	if (!SOC_Enhance_Element.u16_SOC_InitOver)
	{
		return;
	}

	if (isCHG() || isDSG() || !SOC_IsVoltageValid())
	{
		g_soc_runtime.u32RestTicks = 0U;
		g_soc_runtime.u8RestBucketApplied = 0U;
		return;
	}

	if (g_soc_runtime.u32RestTicks < 0xFFFFFFFFU)
	{
		++g_soc_runtime.u32RestTicks;
	}

	rest_seconds = g_soc_runtime.u32RestTicks / SOC_REST_SECONDS_PER_TICK;
	SOC_ApplyRestCompensation(rest_seconds);
}

static void SOC_ApplyWeakCellGuard(void)
{
	UINT8 guard_soc = 100U;
	UINT8 target_soc;
	UINT8 current_soc;

	if (isCHG() || !SOC_IsVoltageValid())
	{
		return;
	}

	if (SOC_Enhance_Element.u16_VCellMin > (UINT16)(SOC_Enhance_Element.u16_SOC_0_Vol + SOC_WEAK_CELL_GUARD_WINDOW_MV))
	{
		return;
	}

	target_soc = Get_OpenCircuit_Value();
	if (target_soc < guard_soc)
	{
		guard_soc = target_soc;
	}

	if (SOC_Enhance_Element.u16_VCellMin <= SOC_Enhance_Element.u16_SOC_0_Vol)
	{
		guard_soc = 0U;
	}
	else if (SOC_Enhance_Element.u16_VCellMin <= (UINT16)(SOC_Enhance_Element.u16_SOC_0_Vol + SOC_WEAK_CELL_CRITICAL_WINDOW_MV))
	{
		if (guard_soc > 2U)
		{
			guard_soc = 2U;
		}
	}
	else if (SOC_Enhance_Element.u16_VCellMin <= (UINT16)(SOC_Enhance_Element.u16_SOC_0_Vol + 60U))
	{
		if (guard_soc > 4U)
		{
			guard_soc = 4U;
		}
	}
	else if (SOC_Enhance_Element.u16_VCellMin <= (UINT16)(SOC_Enhance_Element.u16_SOC_0_Vol + 90U))
	{
		if (guard_soc > 6U)
		{
			guard_soc = 6U;
		}
	}
	else
	{
		if (guard_soc > 8U)
		{
			guard_soc = 8U;
		}
	}

	current_soc = SOC_Calculate_Element.u8SOC_Now;
	if (current_soc <= guard_soc)
	{
		return;
	}

	/* Guard path may only lower one step per execution. */
	current_soc = SOC_StepTowardTarget(current_soc, guard_soc, 1U);
	SOC_ApplySocCorrection(current_soc);
}

static void SOC_UpdateDisplaySoc(void)
{
	UINT8 target_soc;

	target_soc = SOC_Calculate_Element.u8SOC_Now;

	if (!g_soc_runtime.u8DisplayReady)
	{
		g_soc_runtime.u8DisplaySoc = target_soc;
		g_soc_runtime.u8DisplayReady = 1U;
		return;
	}

	if (g_soc_runtime.u8DisplaySoc < target_soc)
	{
		++g_soc_runtime.u8DisplaySoc;
		if (g_soc_runtime.u8DisplaySoc > target_soc)
		{
			g_soc_runtime.u8DisplaySoc = target_soc;
		}
		return;
	}

	if (g_soc_runtime.u8DisplaySoc > target_soc)
	{
		if ((UINT8)(g_soc_runtime.u8DisplaySoc - target_soc) > 1U)
		{
			g_soc_runtime.u8DisplaySoc = (UINT8)(g_soc_runtime.u8DisplaySoc - 1U);
		}
		else
		{
			g_soc_runtime.u8DisplaySoc = target_soc;
		}
	}
}

static void SOC_SyncOutputData(UINT8 force_display_follow)
{
	if (force_display_follow)
	{
		g_soc_runtime.u8DisplaySoc = SOC_Calculate_Element.u8SOC_Now;
		g_soc_runtime.u8DisplayReady = 1U;
	}
	else
	{
		SOC_UpdateDisplaySoc();
	}

	SOC_Enhance_Element.u8_SOC = g_soc_runtime.u8DisplaySoc;
	if (SOC_Calculate_Element.u32CapFactory == 0U)
	{
		SOC_Enhance_Element.u8_SOH = 0U;
		SOC_Enhance_Element.u16_CapacityNow = 0U;
		SOC_Enhance_Element.u16_CapacityFull = 0U;
		SOC_Enhance_Element.u16_CapacityFactory = 0U;
	}
	else if (SOC_Calculate_Element.u32CapFull >= SOC_Calculate_Element.u32CapFactory)
	{
		SOC_Enhance_Element.u8_SOH = 100;
		SOC_Enhance_Element.u16_CapacityNow = SOC_Calculate_Element.u32CapNow * 1 / 360;
		SOC_Enhance_Element.u16_CapacityFull = SOC_Calculate_Element.u32CapFull * 1 / 360;
		SOC_Enhance_Element.u16_CapacityFactory = SOC_Calculate_Element.u32CapFactory * 1 / 360;
	}
	else
	{
		SOC_Enhance_Element.u8_SOH = (UINT8)((100 * SOC_Calculate_Element.u32CapFull / SOC_Calculate_Element.u32CapFactory) & 0xFF);
		SOC_Enhance_Element.u16_CapacityNow = SOC_Calculate_Element.u32CapNow * 1 / 360;
		SOC_Enhance_Element.u16_CapacityFull = SOC_Calculate_Element.u32CapFull * 1 / 360;
		SOC_Enhance_Element.u16_CapacityFactory = SOC_Calculate_Element.u32CapFactory * 1 / 360;
	}
	SOC_Enhance_Element.u16_Cycle_times = SOC_Calculate_Element.u32Cycle_times / 100;

	SOC_Enhance_Element.u8_SOC_OCV_Cali = SOC_Calculate_Element.u8DSG_SOC_Int; // 留着，自己知道

	g_stCellInfoReport.SocElement.u16Soc = SOC_Enhance_Element.u8_SOC;
	g_stCellInfoReport.SocElement.u16Soh = SOC_Enhance_Element.u8_SOH;
	g_stCellInfoReport.SocElement.u16CapacityNow = SOC_Enhance_Element.u16_CapacityNow;
	g_stCellInfoReport.SocElement.u16CapacityFull = SOC_Enhance_Element.u16_CapacityFull;
	g_stCellInfoReport.SocElement.u16CapacityFactory = SOC_Enhance_Element.u16_CapacityFactory;
	g_stCellInfoReport.SocElement.u16Cycle_times = SOC_Enhance_Element.u16_Cycle_times;

	if (System_OnOFF_Func.bits.b1OnOFF_SOC_Fixed)
	{
		g_stCellInfoReport.SocElement.u16Soc = 60U;
	}
	if (System_OnOFF_Func.bits.b1OnOFF_SOC_Zero)
	{
		g_stCellInfoReport.SocElement.u16Soc = 0U;
	}
}

void soc_factory_param_init_first(void)
{
	SOC_Enhance_Element.u16_SOC_CycleT_Ever = OtherElement.u16Soc_Cycle_times;

	SOC_Calculate_Element.u32CapFactory = (UINT32)SOC_Enhance_Element.u16_SOC_Ah * 3600; // 去掉*10;改单位这里进来的单位稍微修改一下便可，如此快捷
	SOC_Calculate_Element.u32Cycle_times = (UINT32)SOC_Enhance_Element.u16_SOC_CycleT_Ever * 100;
	SOC_Calculate_Element.u32CycleT_Limit = (UINT32)SOC_Enhance_Element.u16_SOC_CycleT_Limit * 100;

	SOC_Calculate_Element.u8SOC_Now = 60;
	SOC_Calculate_Element.u32CapFull = SOC_Calculate_Element.u32CapFactory;
	SOC_ResetRuntimeContext();
	SOC_SyncOutputData(1U);
	SOC_DealEEPROM_Data(EEPROM_DATA_REFRESH);
}

void soc_param_lib_init(void)
{

	// 外部获取的数据初始化
	SOC_Calculate_Element.u32CapFactory = (UINT32)SOC_Enhance_Element.u16_SOC_Ah * 3600; // 去掉*10;改单位这里进来的单位稍微修改一下便可，如此快捷
	SOC_Calculate_Element.u32Cycle_times = (UINT32)SOC_Enhance_Element.u16_SOC_CycleT_Ever * 100;
	SOC_Calculate_Element.u32CycleT_Limit = (UINT32)SOC_Enhance_Element.u16_SOC_CycleT_Limit * 100;

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
	SOC_ResetRuntimeContext();
	SOC_SyncOutputData(1U);

	// extern void GetData_SOC(void);
	// GetData_SOC();
}

UINT8 Get_OpenCircuit_Value(void)
{
	UINT8 result = 0;
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
	return result;
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
	switch (CurrentType)
	{
	case CurCHG:
		// SOC实际认为是100%的点，接近过充保护的时候
		// 本来想把内环校准值加上去的，但是想想这个系数不可控，算了算了，直接骗。
		// 以下这个点，假设我SOC相对不准，例如，大家都从0%开始计算，我最后算得SOC有90%(电流不准+板子本身功耗+时序有点误差)
		// 但实际已经满了，这个点一直没法处理。
		if (SOC_Enhance_Element.u16_VCellMax >= SOC_Enhance_Element.u16_SOC_100_Vol - 100 && SOC_Enhance_Element.u16_VCellMax < SOC_Enhance_Element.u16_SOC_100_Vol && SOC_Calculate_Element.u8SOC_Now < 95)
		{ // 和放电电流对应，第一段，必须拉到95%以内
			if (++su16_SocChgCal_L1_Tcnt >= 10)
			{
				su16_SocChgCal_L1_Tcnt = 0;
				SOC_ApplySocCorrection((UINT8)(SOC_Calculate_Element.u8SOC_Now + 1U));
			}
		}
		else if (SOC_Enhance_Element.u16_VCellMax >= SOC_Enhance_Element.u16_SOC_100_Vol && SOC_Calculate_Element.u8SOC_Now < 100)
		{
			if (SOC_Calculate_Element.u8SOC_Now > 95)
			{
				if (++su16_SocChgCal_L2_Tcnt >= 8)
				{
					su16_SocChgCal_L2_Tcnt = 0;
					SOC_ApplySocCorrection((UINT8)(SOC_Calculate_Element.u8SOC_Now + 1U));
				}
			}
			else
			{
				if (++su16_SocChgCal_L3_Tcnt >= 4)
				{
					su16_SocChgCal_L3_Tcnt = 0;
					SOC_ApplySocCorrection((UINT8)(SOC_Calculate_Element.u8SOC_Now + 1U));
				}
			}
		}

		// 这是基于充电必须能达到100%的终极做法，2S + 1%
		if (SOC_Enhance_Element.u16_VCellMax >= SOC_Enhance_Element.u16_SOC_100_Vol + 50 && SOC_Calculate_Element.u8SOC_Now < 100)
		{
			if (++su16_SocChgCal_L4_Tcnt >= 2)
			{
				su16_SocChgCal_L4_Tcnt = 0;
				SOC_ApplySocCorrection((UINT8)(SOC_Calculate_Element.u8SOC_Now + 1U));
			}
		}

#ifdef _CAL_SLOW_DOWN_CHG
		// 这里会出现回退的现象，就是末端，断开管子瞬间，电压下降200mV(类似)，此时SOC已经100%，
		// 但是由于电流计算是有权重的，变为0可能需要几秒，此时会回退到98，也即从100-98
		// 如果执行以上的几个情况，这个就不会执行，
		if (SOC_Calculate_Element.u8SOC_Now >= 98 && SOC_Enhance_Element.u16_VCellMax < SOC_Enhance_Element.u16_SOC_100_Vol)
		{
			SOC_ApplySocNow(SOC_Calculate_Element.u8SOC_Now);
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
			if (++su16_SocDsgCal_L1_Tcnt >= 10)
			{
				su16_SocDsgCal_L1_Tcnt = 0;
				SOC_ApplySocCorrection((UINT8)(SOC_Calculate_Element.u8SOC_Now - 1U));
			}
		}
		else if (SOC_Enhance_Element.u16_VCellMin <= SOC_Enhance_Element.u16_SOC_0_Vol && SOC_Calculate_Element.u8SOC_Now > 0)
		{ // 我也不知道为什么要5%，想想，直接0%，与下面两个行成闭循环
			if (SOC_Calculate_Element.u8SOC_Now < 5)
			{ // 第二级校准
				if (++su16_SocDsgCal_L2_Tcnt >= 8)
				{
					su16_SocDsgCal_L2_Tcnt = 0;
					SOC_ApplySocCorrection((UINT8)(SOC_Calculate_Element.u8SOC_Now - 1U));
				}
			}
			else
			{
				if (++su16_SocDsgCal_L3_Tcnt >= 4)
				{
					su16_SocDsgCal_L3_Tcnt = 0;
					SOC_ApplySocCorrection((UINT8)(SOC_Calculate_Element.u8SOC_Now - 1U));
				}
			}
		}

		// 这是基于放电必须能达到0%的终极做法，2S - 1%
		// 但实际上放电要求没充电高
		if (SOC_Enhance_Element.u16_VCellMin <= SOC_Enhance_Element.u16_SOC_0_Vol - 50 && SOC_Calculate_Element.u8SOC_Now > 0)
		{
			if (++su16_SocDsgCal_L4_Tcnt >= 2)
			{
				su16_SocDsgCal_L4_Tcnt = 0;
				SOC_ApplySocCorrection((UINT8)(SOC_Calculate_Element.u8SOC_Now - 1U));
			}
		}

		if (SOC_Calculate_Element.u8SOC_Now <= 1 && SOC_Enhance_Element.u16_VCellMin > SOC_Enhance_Element.u16_SOC_0_Vol)
		{
			SOC_ApplySocNow(SOC_Calculate_Element.u8SOC_Now);
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

void Correction_Terminal(enum _CUR CurrentType)
{
	/*
	 * 当前工程没有独立的 CC 末端策略实现。
	 * 为避免外部 flag 被置位后直接跳过末端校准，统一回落到 CV 策略。
	 */
	CorrectionTerminal_CV(CurrentType);
}

void SOC_Cont_AH_Int_CHG(void)
{
	UINT32 C_change_per;
	static UINT8 s_u8_CHG200msCnt = 0;
	static UINT8 s_u8_Transfer200msCnt = 0;
	if (SOC_Enhance_Element.u16_Ichg >= SOC_VIRTUAL_CURRENT_CHG)
	{
		// if(g_stCellInfoReport.u16Ichg > 0) {
		if (++s_u8_CHG200msCnt >= 5)
		{
			s_u8_CHG200msCnt = 0;
			SOC_Calculate_Element.u8CHG_AHCalcu_Flag = 1;
		}
		if (s_u8_Transfer200msCnt)
			s_u8_Transfer200msCnt = 0;
	}
	else
	{
		if (++s_u8_Transfer200msCnt >= 2)
		{ // 防止瞬间跳动问题
			s_u8_Transfer200msCnt = 0;
			s_u8_CHG200msCnt = 0;
			SOC_Cali_Flag = SOC_CALI_STATE_TRANSFER;
			return;
		}
		if (s_u8_CHG200msCnt > 0U)
		{
			--s_u8_CHG200msCnt;
		}
	}

#if 1 // 原来的计算方式着实太拖沓，下面的三句搞定，还清晰明了，例如，容量没到100%前，都是99%，到达那一瞬间才是100%
	  // 这个的效果和优化的没啥差别，基于放电没操作，这个也不改了吧。
	if (SOC_Calculate_Element.u8CHG_AHCalcu_Flag)
	{
		if (SOC_Calculate_Element.u32CapFactory == 0U)
		{
			SOC_Calculate_Element.u8CHG_AHCalcu_Flag = 0U;
			return;
		}

		Correction_Terminal(CurCHG);
		SOC_Calculate_Element.u8SOC_Old = SOC_Calculate_Element.u8SOC_Now;
		// SOC_Calculate_Element.u32CapChange += ((UINT32)SOC_Calculate_Element.u8n_CoulombicEff * SOC_Enhance_Element.u16_Ichg * 1+50)/100;	//As*10*100(库伦效率100)
		// SOC_Calculate_Element.u32CapNow += ((UINT32)SOC_Calculate_Element.u8n_CoulombicEff * SOC_Enhance_Element.u16_Ichg * 1+50)/100;  			//剩余容量实时跟踪
		SOC_Calculate_Element.u32CapChange += (UINT32)SOC_Enhance_Element.u16_Ichg * 1; // As*10*100(库伦效率100)
		SOC_Calculate_Element.u32CapNow += (UINT32)SOC_Enhance_Element.u16_Ichg * 1;	// 剩余容量实时跟踪

		if (SOC_Calculate_Element.u32CapNow > SOC_Calculate_Element.u32CapFactory)
			SOC_Calculate_Element.u32CapNow = SOC_Calculate_Element.u32CapFactory;
		C_change_per = SOC_Calculate_Element.u32CapChange * 100 / SOC_Calculate_Element.u32CapFactory;
		SOC_Calculate_Element.u8SOC_Now = SOC_Calculate_Element.u8SOC_Old + C_change_per;
		if (SOC_Calculate_Element.u8SOC_Now > 100)
			SOC_Calculate_Element.u8SOC_Now = 100;
		SOC_Calculate_Element.u32CapChange = (((SOC_Calculate_Element.u32CapChange * 100) % SOC_Calculate_Element.u32CapFactory) + 50) / 100;
		SOC_Calculate_Element.u8CHG_AHCalcu_Flag = 0;

		// 计算实际容量专用值。
		SOC_Calculate_Element.u32CapFull_Cal_As += (UINT32)SOC_Enhance_Element.u16_Ichg * 1;
	}
#endif
}

void SOC_Cont_AH_Int_DSG(void)
{
	UINT32 C_change_per;
	static UINT8 s_u8_DSG200msCnt = 0;
	static UINT8 s_u8_Transfer200msCnt = 0;
	if (SOC_Enhance_Element.u16_Idsg >= SOC_VIRTUAL_CURRENT_DSG)
	{
		if (++s_u8_DSG200msCnt >= 5)
		{
			s_u8_DSG200msCnt = 0;
			SOC_Calculate_Element.u8DSG_AHCalcu_Flag = 1;
		}
		if (s_u8_Transfer200msCnt)
			s_u8_Transfer200msCnt = 0;
	}
	else
	{
		if (++s_u8_Transfer200msCnt >= 2)
		{
			s_u8_Transfer200msCnt = 0;
			s_u8_DSG200msCnt = 0;
			SOC_Cali_Flag = SOC_CALI_STATE_TRANSFER;
			return;
		}
		if (s_u8_DSG200msCnt > 0U)
		{
			--s_u8_DSG200msCnt;
		}
	}

#if 1 // 这个计算方式还是妥一些，满减1%，SOC才显示99，客户体验会更好一些
	if (SOC_Calculate_Element.u8DSG_AHCalcu_Flag)
	{
		if (SOC_Calculate_Element.u32CapFactory == 0U)
		{
			SOC_Calculate_Element.u8DSG_AHCalcu_Flag = 0U;
			return;
		}

		Correction_Terminal(CurDSG);

		SOC_Calculate_Element.u8SOC_Old = SOC_Calculate_Element.u8SOC_Now;
		// SOC_Calculate_Element.u32CapChange += ((UINT32)SOC_Calculate_Element.u8n_CoulombicEff * SOC_Enhance_Element.u16_Idsg * 1 + 50)/100; //As*10*100(库伦效率100)
		// SOC_Calculate_Element.u32CapNow-= ((UINT32)SOC_Calculate_Element.u8n_CoulombicEff * SOC_Enhance_Element.u16_Idsg * 1 + 50)/100; 	//剩余容量实时跟踪
		SOC_Calculate_Element.u32CapChange += (UINT32)SOC_Enhance_Element.u16_Idsg * 1;
		if (SOC_Calculate_Element.u32CapNow > (UINT32)SOC_Enhance_Element.u16_Idsg)
		{
			SOC_Calculate_Element.u32CapNow -= (UINT32)SOC_Enhance_Element.u16_Idsg * 1;
		}
		else
		{
			SOC_Calculate_Element.u32CapNow = 0U;
		}
		C_change_per = SOC_Calculate_Element.u32CapChange * 100 / SOC_Calculate_Element.u32CapFactory;
		SOC_Calculate_Element.u8SOC_Now = SOC_Calculate_Element.u8SOC_Old - C_change_per;
		if (SOC_Calculate_Element.u8SOC_Now > 100)
			SOC_Calculate_Element.u8SOC_Now = 0;
		SOC_Calculate_Element.u32CapChange = (((SOC_Calculate_Element.u32CapChange * 100) % SOC_Calculate_Element.u32CapFactory) + 50) / 100; // 四舍五入，关键
		SOC_Calculate_Element.u8DSG_AHCalcu_Flag = 0;

		// 循环次数统计
		// 如果是SOC=0还在疯狂减的话，在校准期间会出现循环次数统计出错，特别是标称容量小，实际容量特别大的时候
		// 上面的也是一个BUG，通过循环次数暴露出来了。
		if (SOC_Calculate_Element.u8SOC_Now != 0)
		{
			SOC_Calculate_Element.u8DSG_SOC_Int += C_change_per;
			// SOC_Calculate_Element.u8DSG_SOC_Int += 1;
			if (SOC_Calculate_Element.u8DSG_SOC_Int >= 80)
			{
				SOC_Calculate_Element.u8DSG_SOC_Int = 0;
				SOC_Calculate_Element.u32Cycle_times += 100;
			}
		}
	}
#endif
}

void SOC_State_Transfer(void)
{
	static UINT8 s_u8SOC_State_CHG = 0;
	static UINT8 s_u8SOC_State_DSG = 0;
	static UINT8 s_u8SOC_State_OCV = 0;
	if (SOC_Enhance_Element.u16_Ichg >= SOC_VIRTUAL_CURRENT_CHG)
	{
		if (++s_u8SOC_State_CHG >= 3)
		{
			s_u8SOC_State_CHG = 0;
			SOC_Cali_Flag = SOC_CALI_CONT_CHG;
		}
		if (s_u8SOC_State_DSG)
			s_u8SOC_State_DSG = 0;
		if (s_u8SOC_State_OCV)
			s_u8SOC_State_OCV = 0;
	}
	else if (SOC_Enhance_Element.u16_Idsg >= SOC_VIRTUAL_CURRENT_DSG)
	{
		if (++s_u8SOC_State_DSG >= 3)
		{
			s_u8SOC_State_DSG = 0;
			SOC_Cali_Flag = SOC_CALI_CONT_DSG;
		}
		if (s_u8SOC_State_CHG)
			s_u8SOC_State_CHG = 0;
		if (s_u8SOC_State_OCV)
			s_u8SOC_State_OCV = 0;
	}
	else
	{
		if (++s_u8SOC_State_OCV >= 3)
		{
			s_u8SOC_State_OCV = 0;
		}
		if (s_u8SOC_State_CHG)
			s_u8SOC_State_CHG = 0;
		if (s_u8SOC_State_DSG)
			s_u8SOC_State_DSG = 0;
	}
}

void SOC_DealEEPROM_Data(enum EEPROM_COMMAND Command)
{
	STORAGE_FLASH_SOC_DATA flash_data;
	UINT8 valid = 0;

	switch (Command)
	{
	case EEPROM_DATA_REFRESH:
		flash_data.u16SocNow = SOC_Calculate_Element.u8SOC_Now;
		flash_data.u16DsgSocInt = SOC_Calculate_Element.u8DSG_SOC_Int;
		flash_data.u32CycleTimes = SOC_Calculate_Element.u32Cycle_times;
		StorageFlash_SaveSocData(&flash_data);
		break;
	case EEPROM_DATA_READ:
		valid = StorageFlash_LoadSocData(&flash_data);
		if (valid)
		{
			if ((flash_data.u16SocNow > 100) || (flash_data.u16DsgSocInt > 100))
			{
				valid = 0;
			}
		}

		if (!valid)
		{
			SOC_Calculate_Element.u8SOC_Now = 60;
			SOC_Calculate_Element.u8DSG_SOC_Int = 0;
			SOC_Calculate_Element.u32Cycle_times = (UINT32)SOC_Enhance_Element.u16_SOC_CycleT_Ever * 100;
			SOC_Calculate_Element.u32CapFull = SOC_Calculate_Element.u32CapFactory;
			SOC_Calculate_Element.u32CapNow = SOC_Calculate_Element.u8SOC_Now * SOC_Calculate_Element.u32CapFactory / 100;
			SOC_DealEEPROM_Data(EEPROM_DATA_REFRESH);
		}
		else
		{
			SOC_Calculate_Element.u8SOC_Now = (UINT8)flash_data.u16SocNow;
			SOC_Calculate_Element.u8DSG_SOC_Int = (UINT8)flash_data.u16DsgSocInt;
			SOC_Calculate_Element.u32Cycle_times = flash_data.u32CycleTimes;
			SOC_Calculate_Element.u32CapFull = (UINT32)SOC_Calculate_Element.u32CapFactory;
			SOC_Calculate_Element.u32CapNow = SOC_Calculate_Element.u8SOC_Now * SOC_Calculate_Element.u32CapFactory / 100;
		}

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
	{
		UINT8 target_soc;

		target_soc = Get_OpenCircuit_Value();
		if ((!isCHG()) && (target_soc > SOC_Calculate_Element.u8SOC_Now))
		{
			target_soc = SOC_Calculate_Element.u8SOC_Now;
		}
		SOC_Calculate_Element.u8SOC_Now = target_soc;
	}
		break;

	case 2: // SOC归零类型，改为循环次数归初始化
			// 添加容量初始化
		// SOC_Calculate_Element.u8SOC_Now = 0;
		SOC_Calculate_Element.u8DSG_SOC_Int = 0;
		SOC_Calculate_Element.u32CapFactory = (UINT32)SOC_Enhance_Element.u16_SOC_Ah * 3600;
		SOC_Calculate_Element.u32Cycle_times = (UINT32)SOC_Enhance_Element.u16_SOC_CycleT_Ever * 100;
		SOC_Calculate_Element.u32CycleT_Limit = (UINT32)SOC_Enhance_Element.u16_SOC_CycleT_Limit * 100;
		// 上面SOC_Calculate_Element.u32CapFactory已经初始化
		SOC_Calculate_Element.u32CapFull = SOC_Calculate_Element.u32CapFactory;
		break;

	case 3:
		SOC_Calculate_Element.u8SOC_Now = SOC_Enhance_Element.u8_SetSocOnce;
		break;

	default:
		break;
	}
	SOC_Calculate_Element.u8_DataUpdateOK = 1;
	SOC_ApplySocNow(SOC_Calculate_Element.u8SOC_Now);
	SOC_ResetRuntimeContext();
	SOC_SyncOutputData(1U);
	SOC_Cali_Flag = SOC_CALI_STATE_TRANSFER;
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
	SOC_PersistSnapshotIfChanged();
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
	SOC_SyncOutputData(0U);
}

void InitSOC_IntEnhance(void)
{
	// 外部获取的数据初始化
	SOC_Calculate_Element.u32CapFactory = (UINT32)SOC_Enhance_Element.u16_SOC_Ah * 3600; // 去掉*10;改单位这里进来的单位稍微修改一下便可，如此快捷
	SOC_Calculate_Element.u32Cycle_times = (UINT32)SOC_Enhance_Element.u16_SOC_CycleT_Ever * 100;
	SOC_Calculate_Element.u32CycleT_Limit = (UINT32)SOC_Enhance_Element.u16_SOC_CycleT_Limit * 100;

	SOC_Calculate_Element.u32CapChange = 0;
	SOC_Calculate_Element.u8OCV_Cali_Flag = 0; // 第一次写置1出现了开机严重错误的问题
	SOC_Calculate_Element.u8CHG_AHCalcu_Flag = 0;
	SOC_Calculate_Element.u8DSG_AHCalcu_Flag = 0;

	SOC_Calculate_Element.u8SOC_Now = 0; // 以上均为0，因为模拟前端还没读回电压
	SOC_Calculate_Element.u32CapNow = 0;
	SOC_Calculate_Element.u8DSG_SOC_Int = 0;
	SOC_Calculate_Element.u32CapFull = 0;

	SOC_Enhance_Element.u16_SOC_InitOver = 0; // 对外标志位初始化
	SOC_ResetRuntimeContext();
	SOC_Cali_Flag = SOC_CALI_STATE_TRANSFER;
}

UINT8 isCHG(void)
{
	// return g_stCellInfoReport.u16Ichg > SOC_VIRTUAL_CURRENT_CHG ? 1 : 0;
	return SOC_Enhance_Element.u16_Ichg > SOC_VIRTUAL_CURRENT_CHG ? 1 : 0;
}

UINT8 isDSG(void)
{
	// return g_stCellInfoReport.u16IDischg > SOC_VIRTUAL_CURRENT_DSG ? 1 : 0;
	return SOC_Enhance_Element.u16_Idsg > SOC_VIRTUAL_CURRENT_DSG ? 1 : 0;
}


static void SOC_ApplyVoltageCalibration(void)
{
	UINT16 total_soc100 = 0U;

#ifdef _SOC_OCV_Fix2_func_
	SOC_OCV_Fix2();
#endif

	#ifdef TERNARYLI
		total_soc100 = 4000U;
	#elif (defined(LIFEPO))
		total_soc100 = 3300U;
	#endif

	if (!isCHG())
	{
		return;
	}

	if ((SOC_Enhance_Element.u16_VCellMax >= SOC_Enhance_Element.u16_SOC_100_Vol) &&
		(SOC_Enhance_Element.u16_VCellMin >= total_soc100) &&
		(SOC_Calculate_Element.u8SOC_Now < 100U))
	{
		SOC_ApplySocNow(100U);
	}
}
/*
>>后记：
1，这个做法会出现一个问题，SOC加速，容量膨胀，然后静置之后，SOC保持不变，但是满电容量减少(因为满电容量是实打实计算的)。
   这样，剩余容量就突然减少了，会有分歧。如果此时SOC计算还没有100%(差距过大，当前满电容量太大)，直到40%这个样子，剩余容量会更少。
2，回到实际情况，用久衰减的电池，也会出现同样的情况，但是末端一定要小电流操作，使其充到100%。
   这样的话，当前容量虽然减少了，但是乘以100%，也差距不会太大。
3，结合1和2，容量最好不要显示，只显示SOC，SOH和出厂容量为妙。
*/
void SOC_IntEnhance_Ctrl(void)
{
	switch (SOC_Cali_Flag)
	{
	// case SOC_CALI_DATA_INIT:
	// 	InitSOC_IntEnhance();
	// 	break;
	// case SOC_CALI_STARTUP:
	// 	SOC_Update_StartUp();
	// 	break;
	case SOC_CALI_STATE_TRANSFER:
		SOC_State_Transfer();
		break;
	case SOC_CALI_CONT_CHG:
		SOC_Cont_AH_Int_CHG();
		break;
	case SOC_CALI_CONT_DSG:
		SOC_Cont_AH_Int_DSG();
		break;
	default:
		SOC_Cali_Flag = SOC_CALI_STATE_TRANSFER;
		break;
	}
	SOC_ApplyVoltageCalibration();
	SOC_UpdateRestMonitor();
	SOC_ApplyWeakCellGuard();

	// 参数刷新命令先落地，再决定是否持久化，减少手动校准后一拍延迟。
	SOC_RefreshData_Monitor();
	SOC_EEPROM_Deal_Monitor();
	SOC_Result_Pass();
}

void SOC_ApplyRtcRelaxationCompensation(UINT32 rest_seconds, UINT16 vcell_min, UINT16 vcell_max)
{
	if (!SOC_Enhance_Element.u16_SOC_InitOver)
	{
		return;
	}

	SOC_Enhance_Element.u16_VCellMin = vcell_min;
	SOC_Enhance_Element.u16_VCellMax = vcell_max;
	SOC_ApplyRestCompensation(rest_seconds);
	SOC_ApplyWeakCellGuard();
	SOC_PersistSnapshotIfChanged();
	SOC_SyncOutputData(1U);
}
