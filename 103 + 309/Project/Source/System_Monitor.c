#include "main.h"

volatile struct SYSTEM_ERROR System_ErrFlag;
volatile union System_OnOFF_Function System_OnOFF_Func;
volatile union System_OnOFF_Function System_OnOFF_Func_StartUpRec;
volatile union System_Status SystemStatus;
volatile union System_Function_StartUp System_Func_StartUp;

#define SYSTEM_ERROR_FIELD_INVALID ((UINT8)0xFFU)

static const UINT8 s_u8SystemErrorFieldOffset[ERROR_NUM + 1] = {
	SYSTEM_ERROR_FIELD_INVALID,
	0U,  1U,  2U,  3U,
	4U,  5U,  6U,  7U,
	8U,  9U,  10U, 11U,
	20U, 12U, 13U, 14U,
	15U, 16U, 17U, 21U,
	SYSTEM_ERROR_FIELD_INVALID,
	SYSTEM_ERROR_FIELD_INVALID,
	22U
};

static volatile UINT8 *System_ErrorField(enum SYSTEM_ERROR_COMMAND errorCode)
{
	UINT8 offset;

	if ((errorCode < ERROR_AFE1) || (errorCode > ERROR_NUM))
	{
		return 0;
	}

	offset = s_u8SystemErrorFieldOffset[(UINT8)errorCode];
	if (offset == SYSTEM_ERROR_FIELD_INVALID)
	{
		return 0;
	}

	return &(((volatile UINT8 *)&System_ErrFlag)[offset]);
}

void InitSystemMonitorData_EEPROM(void)
{
	// 系统功能控制类型，原则上，关闭功能不用管下面的，只需要这个置零就好
	System_OnOFF_Func.all = 0;
	System_OnOFF_Func.bits.b1OnOFF_Balance = 1; // 因为均衡还没处理好，全系列屏蔽
	System_OnOFF_Func.bits.b1OnOFF_BMS_Source = 1;
	System_OnOFF_Func.bits.b1OnOFF_MOS_Relay = 1; // 核心功能
	// System_OnOFF_Func.bits.b1OnOFF_Relay_Rec = 1;
	System_OnOFF_Func.bits.b1OnOFF_AFE1 = 1;
	// System_OnOFF_Func.bits.b1OnOFF_AFE2 = 1;
	System_OnOFF_Func.bits.b1OnOFF_Sleep = 1;
	System_OnOFF_Func.bits.b1OnOFF_SOC_Fixed = 0;

	// 系统开机时序控制类型
	System_Func_StartUp.all = 0; // 置1的原因是需要初始化的意思，意思是这个功能需要初始化，所以不用0
	System_Func_StartUp.bits.b1StartUpFlag_SOC = 1;
	System_Func_StartUp.bits.b1StartUpFlag_Balance = 1;
	System_Func_StartUp.bits.b1StartUpFlag_Protect = 1;
	System_Func_StartUp.bits.b1StartUpFlag_Relay = 1; // 没用继电器用MOS，这个值就不管就好，因为初始化建立是||，其中一个建立便可
	System_Func_StartUp.bits.b1StartUpFlag_MOS = 1;
	System_Func_StartUp.bits.b1StartUpFlag_ADC = 1;
	System_Func_StartUp.bits.b1StartUpFlag_CAN = 1;
	System_Func_StartUp.bits.b1StartUpFlag_BlueT = 1; //

	// 系统状态跟踪类型
	SystemStatus.all = 0;
	// SystemStatus.bits.b1Status_SysLimits = 0;		//默认无密码，不限制
	SystemStatus.bits.b1StartUpBMS = 1;			  // MOS或者接触器能打开意味着初始化完毕
	SystemStatus.bits.b1Status_Relay_PRE = CLOSE; // 默认是不打开，但是CLOSE不一定是低电平
	SystemStatus.bits.b1Status_Relay_CHG = CLOSE; // 全部写，方便上传给上位机，不需要选通再改
	SystemStatus.bits.b1Status_Relay_DSG = CLOSE;
	SystemStatus.bits.b1Status_Relay_MAIN = CLOSE;
	SystemStatus.bits.b1Status_MOS_PRE = CLOSE;
	SystemStatus.bits.b1Status_MOS_CHG = CLOSE;
	SystemStatus.bits.b1Status_MOS_DSG = CLOSE;

	// 总感觉，这个存在很麻烦，如果板子以前烧了代码，则这个关闭加热冷凝功能则没法处理。
	// 但是屏蔽了，假设均衡关掉，再次启动又打开很麻烦(但是基本都是要求均衡的)
	// 综上所述，先屏蔽观察一下
	// System_OnOFF_Func.all = (UINT32)ReadEEPROM_Word_NoZone(EEPROM_ADDR_SYS_FUNC_SELECT);
	// System_OnOFF_Func.all |= ((UINT32)ReadEEPROM_Word_NoZone(EEPROM_ADDR_SYS_FUNC_SELECT + 2)<<16);		//先扩大为32位再移位

	System_OnOFF_Func_StartUpRec.all = System_OnOFF_Func.all; // 如果某功能开机不打开，后续运行中途打开，则需要初始化，该位为记录位
	if (!System_OnOFF_Func.bits.b1OnOFF_Balance)
		System_Func_StartUp.bits.b1StartUpFlag_Balance = 0;
	if (!System_OnOFF_Func.bits.b1OnOFF_MOS_Relay)
	{ // 没打开MOS功能，则不会完成初始化，打开才能把SystemStatus.bits.b1StartUpBMS关掉同时打开MOS
		System_Func_StartUp.bits.b1StartUpFlag_MOS = 0;
		System_Func_StartUp.bits.b1StartUpFlag_Relay = 0;
	}

	// 系统错误控制类型
	// 这个没有
}


// 0:没错误，X：有错误
UINT8 System_ERROR_UserCallback(enum SYSTEM_ERROR_COMMAND errorCode)
{
	UINT8 result = 0;
	volatile UINT8 *flag;
	enum SYSTEM_ERROR_COMMAND baseError;

	if ((errorCode >= ERROR_AFE1) && (errorCode <= ERROR_NUM))
	{
		flag = System_ErrorField(errorCode);
		if (flag != 0)
		{
			if (errorCode == ERROR_TEMP_BREAK)
			{
				*flag = 1U;
			}
			else if (errorCode != ERROR_EEPROM_STORE)
			{
				++(*flag);
			}
		}
		return result;
	}

	if ((errorCode >= ERROR_REMOVE_AFE1) && (errorCode <= ERROR_REMOVE_TEMP_BREAK))
	{
		baseError = (enum SYSTEM_ERROR_COMMAND)((UINT16)ERROR_AFE1 +
												((UINT16)errorCode - (UINT16)ERROR_REMOVE_AFE1));
		flag = System_ErrorField(baseError);
		if (flag != 0)
		{
			*flag = 0U;
		}
		return result;
	}

	if ((errorCode >= ERROR_STATUS_AFE1) && (errorCode <= ERROR_STATUS_TEMP_BREAK))
	{
		baseError = (enum SYSTEM_ERROR_COMMAND)((UINT16)ERROR_AFE1 +
												((UINT16)errorCode - (UINT16)ERROR_STATUS_AFE1));
		flag = System_ErrorField(baseError);
		if (flag != 0)
		{
			result = *flag;
		}
	}

	return result;
}
