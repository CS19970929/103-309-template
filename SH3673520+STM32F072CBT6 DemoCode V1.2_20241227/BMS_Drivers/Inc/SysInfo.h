#ifndef SYSINFO_H_
#define SYSINFO_H_

#include "Head.h"

//#pragma pack()
//#pragma anon_unions

//typedef struct _SYSINFOR_ // data for up-computer monitor
//{
//	union
//	{
//		U16 usPackStatus;
//		struct
//		{
//			U16 bDSGMOS		:1;		// 放电MOS状态
//			U16 bCHGMOS		:1;		// 充电MOS状态
//			U16 bPDSGMOS	:1;		// 预放电MOS状态
//			U16 bDSGING		:1;		// 放电状态
//			U16 bCHGING		:1;		// 充电状态
//			U16 bPACKOV		:1;		// 总压过压保护
//			U16 bPACKUV		:1;		// 总压欠压保护
//			U16 bAFE_ERR	:1;		// AFE通讯是否异常
//			U16 bBAL		:1;		// 均衡状态
//            U16				:5;		// 预留(占位)
//			U16 bSLEEP		:1;		// AFE SLEEP模式
//			U16 bIDLE		:1;		// AFE IDLE模式
//		};
//	};
//	union
//	{
//		U16 usBatStatus;
//		struct
//		{
//			U16 bUTC		:1;		// 充电低温保护
//			U16 bOTC		:1;		// 充电高温保护
//			U16 bUTD		:1;		// 放电低温保护
//			U16 bOTD		:1;		// 放电高温保护
//			U16 bICOT		:1;		// 芯片内部高温保护
//			U16 bOV			:1;		// 单节过压保护
//			U16 bUV			:1;		// 单节欠压保护
//			U16 bOCC		:1;		// 充电过流保护
//			U16 bSC			:1;		// 硬件短路保护
//			U16 bOCD2		:1;		// 放电过流2保护
//			U16 bOCD1		:1;		// 放电过流1保护
//			U16 bCTO		:1;		// 断线状态
//			U16				:4;		// 预留(占位)
//		};
//	};
//  
//	union
//	{
//		U16 usPackConfig;
//		struct
//		{
//			U16 bPDSGMOS_EN	: 1;    // 是否支持预放电功能
//			U16 bBAL_EN		: 1;    // 是否支持均衡功能
//			U16 bCTO_EN		: 1;    // 是否支持断线功能
//			U16 Reserved	: 13;	// 预留(占位)
//		};
//	};
//    
//	union
//	{
//		struct
//		{
//			U16	ssVCell[20];		// 电芯电压
//            U32 uiVoltage1;			// 电芯总压(计算所有电芯电压之和)
//			U32 uiVoltage2;			// BAT+总压
//			S32	siCADCCurr;			// CADC计算后的平均电流
//			S32	siVADCCurr;			// VADC计算后的平均电流
//			U16	usTemp[4];			// TS1-4温度值
//			U16 usTempI;			// 内部温度值
//			U32 uiVCHGD;			// C+电压
//			U32 uiCTOChannel;		// 断线的对应位置
//			U32	uiBALChannel;		// 均衡的对应位置
//			U16	usSoftVersion;		// 软件版本
//			U16 usManufactureAccess;
//		};
//	};    
//} SYSINFOR;

//extern SYSINFOR Info;

#endif
