#ifndef __COMMMON_H
#define __COMMMON_H

#include "stm32f0xx_hal.h"
#include "stm32f0xx_it.h"
#include "stdbool.h"
#include "stdint.h"
#include "string.h"
#include "stdlib.h"
#include "SH36735xx.h"

typedef signed char         S8;
typedef signed short int    S16;
typedef signed int          S32;

typedef unsigned char       U8;
typedef unsigned short int  U16;
typedef unsigned int        U32;

typedef	bool                BOOL;

#define NTC_103AT_3435		0
#define NTC_103AT_3950		1

#define RT_TABLE			NTC_103AT_3950

#if RT_TABLE == NTC_103AT_3435
#define NTC103AT_ARRAY_LEN 	161
#define TEMP_UPPER_LIMIT	110 			// 110℃
#define TEMP_LOWER_LIMIT	-50				// -50℃
#elif RT_TABLE == NTC_103AT_3950
#define NTC103AT_ARRAY_LEN 	161
#define TEMP_UPPER_LIMIT	110 			// 110℃
#define TEMP_LOWER_LIMIT	-50				// -50℃
#else
	#error  "undefined RT_TABLE!"
#endif

#define DATAFLASH_BLOCK_SIZE    512

#pragma pack()
#if defined(__CC_ARM)
  #pragma anon_unions
#elif defined(__ICCARM__)
  #pragma language=extended
#elif defined(__GNUC__)
  /* anonymous unions are enabled by default */
#elif defined(__TMS470__)
/* anonymous unions are enabled by default */
#elif defined(__TASKING__)
  #pragma warning 586
#else
  #warning Not supported compiler type
#endif

typedef struct	_AFEREG_					   //data for AFE set in init
{
	U8	AFESCONF1;
	U8	AFESCONF2;
	U8	AFESCONF3;
	U8	AFESCONF4;
	U8	AFESCONF5; 
	U8	AFESCONF6;
	U8	AFESCONF7;
	U8	AFEOWV_ALARMH;
	U8	AFEALARML;
	U8	AFEOVT_OVH;
	U8	AFEOVL;
	U8	AFEUVT_UVH;
	U8	AFEUVL;
	U8	AFEOCD1V_OCD1T;
	U8	AFEOCD2V_OCD2T;
	U8	AFESCV_SCT; 
	U8	AFEOCCV_OCCT;
	U8	AFEOTC;
	U8	AFEOTD;
	U8	AFEUTC;
	U8	AFEUTD;
	U8	AFEBALANCEH;
	U8	AFEBALANCEM;
	U8	AFEBALANCEL;
	U8	AFEFLAG1;
	U8	AFEFLAG2;
	U8	AFEFLAG3;
	U8	AFEBSTATUS1;
	U8	AFEBSTATUS2;
	U8  AFEOWDH;
	U8  AFEOWDM;
	U8  AFEOWDL;
}AFEREG;

typedef struct	_AFEDATA_		// data  get form AFE
{
	U16	usTS[4];				// 采集的外部温度码值
	U16	usTI;					// 采集的内部温度码值
	S16	ssVADCCurr;				// 采集的VADC电流码值
	S16	ssCADCCurr;				// 采集的CADC电流码值 
	S16	ssCell[20];				// 采集的cell电压码值	
	U16 usVTOP;					// 采集的B+总压码值
	U16 usVCHGR;				// 采集的CHGD电压码值
}AFEDATA;

typedef struct _SYSINFOR_ // data for up-computer monitor
{
	union
	{
		U16 usPackStatus;
		struct
		{
			U16 bDSGMOS		:1;		// 放电MOS状态
			U16 bCHGMOS		:1;		// 充电MOS状态
			U16 bPDSGMOS	:1;		// 预放电MOS状态
			U16 bDSGING		:1;		// 放电状态
			U16 bCHGING		:1;		// 充电状态
			U16 bPACKOV		:1;		// 总压过压保护
			U16 bPACKUV		:1;		// 总压欠压保护
			U16 bAFE_ERR	:1;		// AFE通讯是否异常
			U16 bBAL		:1;		// 均衡状态
            U16				:4;		// 预留(占位)
			U16 bHighSide	:1;		// 高边方案
			U16 bSLEEP		:1;		// AFE SLEEP模式
			U16 bIDLE		:1;		// AFE IDLE模式
		};
	};
	union
	{
		U16 usBatStatus;
		struct
		{
			U16 bUTC		:1;		// 充电低温保护
			U16 bOTC		:1;		// 充电高温保护
			U16 bUTD		:1;		// 放电低温保护
			U16 bOTD		:1;		// 放电高温保护
			U16 bICOT		:1;		// 芯片内部高温保护
			U16 bOV			:1;		// 单节过压保护
			U16 bUV			:1;		// 单节欠压保护
			U16 bOCC		:1;		// 充电过流保护
			U16 bSC			:1;		// 硬件短路保护
			U16 bOCD2		:1;		// 放电过流2保护
			U16 bOCD1		:1;		// 放电过流1保护
			U16 bCTO		:1;		// 断线状态
			U16				:4;		// 预留(占位)
		};
	};
  
	union
	{
		U16 usPackConfig;
		struct
		{
			U16 bPDSGMOS_EN	: 1;    // 是否支持预放电功能
			U16 bBAL_EN		: 1;    // 是否支持均衡功能
			U16 bCTO_EN		: 1;    // 是否支持断线功能
			U16 Reserved	: 13;	// 预留(占位)
		};
	};
    
	union
	{
		struct
		{
			U8 ucCellNum;			// 电芯串数
			S16	ssVCell[20];		// 电芯电压
			S16 ssVCellMin;			// 最小电芯电压
			S16 ssVCellMax;			// 最大电芯电压
            U32 uiVoltage1;			// 电芯总压(计算所有电芯电压之和)
			U32 uiVoltage2;			// BAT+总压
			S32	siCADCCurr;			// CADC计算后的平均电流
			S32	siVADCCurr;			// VADC计算后的平均电流
			U16	usTemp[4];			// TS1-4外部温度值
			U16 usTempMin;			// 最小外部温度值
			U16	usTempMax;			// 最大外部温度值
			U16 usTempI;			// 内部温度值
			U32 uiVCHGD;			// C+电压
			U32 uiCTOChannel;		// 断线的对应位置
			U32	uiBALChannel;		// 均衡的对应位置
			U16	usSoftVersion;		// 软件版本
			U16 usManufactureAccess;
		};
	};    
}SYSINFOR;

typedef struct _PARAMETER_
{
	union
	{
		// 占位用处，确保使用了512个字节（一个扇区2048个字节）
		U8 reversed[DATAFLASH_BLOCK_SIZE-4];

		struct
		{
			// MCU参数区开始 SubClassID=0x00
			union
			{
				U16 E2usSubclass00;		// 此变量只用来索引 subclass0 的地址
				U16 E2usSysParaMap;		// 此变量无实际意义，只表明 subclass0 是系统信息区域
				struct
				{
					union
					{
						U16 E2usPackConfigMap;
						struct
						{
							U16 bPDSGMOS_EN : 1;		// 预放电控制
							U16 bBAL_EN     : 1;		// 均衡功能
							U16 bCTO_EN     : 1;		// 是否支持断线
							U16             : 13;		// 占位
						};
					};
					
					U16 E2usPackUVVol;			// 电池总压欠压保护电压
					U16 E2usPackUVRVol;			// 电池总压欠压保护恢复电压
					U8  E2ucPackUVDelay;		// 电池总压欠压保护延时
					U8  E2ucPackUVRDelay;		// 电池总压欠压保护恢复延时
					
					U16 E2usPackOVVol;			// 电池总压过压保护电压
					U16 E2usPackOVRVol;			// 电池总压过压保护恢复电压
					U8  E2ucPackOVDelay;		// 电池总压过压保护延时
					U8  E2ucPackOVRDelay;		// 电池总压过压保护恢复延时
					
					U16 E2usOVRVol;				// 电芯欠压保护恢复电压
                    U16 E2usUVRVol;				// 电芯欠压保护恢复电压
					U16 E2usUTCRTemp;			// 充电低温保护恢复温度
                    U16 E2usOTCRTemp;			// 充电高温保护恢复温度
                    U16 E2usUTDRTemp;			// 放电低温保护恢复温度
                    U16 E2usOTDRTemp;			// 放电高温保护恢复温度
					
					U8  E2ucOVRDelay;			// 过压保护恢复延时
                    U8  E2ucUVRDelay;			// 欠压保护恢复延时
					U8  E2ucOUTRDelay;			// 高低温保护恢复延时
					
					U8	E2ucLoadChgChkDelay;	// 负载/充电器检测延时
					U16 E2usChgChkVol;			// 充电器接入检测电压
					U16 E2usChgRChkVol;			// 充电器移除检测电压
					
					U16 E2usBalanceVol;			// 平衡开启电压
					U16	E2usBalanceVolDiff;		// 平衡开启压差
					S16 E2ssBalanceCur;			// 平衡开启电流
					S16 E2ssDfilterCur;			// 零电流允许偏差
					U8	E2ucBalanceDelay;		// 平衡进入延时
					
                    U8  E2ucRamCheckFlg0;
				};
			};
			// AFE相关参数及保护恢复参数 SubClassID=0x0A
			union
			{
				U16 E2usSubclass0A;
				U16 E2usAFEParaMap;
				struct
				{
					U16	E2usAFEOTCTemp;			// 温度相关参数间接对应AFE寄存器码值
					U16	E2usAFEOTDTemp;
					U16	E2usAFEUTCTemp;
					U16	E2usAFEUTDTemp;
					
					U8	E2ucAFESCONF1;
					U8	E2ucAFESCONF2;
					U8	E2ucAFESCONF3;
					U8	E2ucAFESCONF4;
					U8	E2ucAFESCONF5;
					U8	E2ucAFESCONF6;
					U8	E2ucAFESCONF7;
					U8	E2ucAFEOWV_ALARMH;
					U8	E2ucAFEALARML;
					U8	E2ucAFEOVT_OVH;
					U8	E2ucAFEOVL;
					U8	E2ucAFEUVT_UVH;
					U8	E2ucAFEUVL;
					U8	E2ucAFEOCD1V_OCD1T;
					U8	E2ucAFEOCD2V_OCD2T;
					U8	E2ucAFESCV_SCT; 
					U8	E2ucAFEOCCV_OCCT;
					
                    U8  E2ucRamCheckFlgA;
				};
			};

			// 校准参数区开始 SubClassID=0x0B
			union
			{
				U16 E2usSubclass0B;
				U16 E2usCaliParaMap;
				struct
				{
					U16 E2usVPackGain1;				// 计算总压增益（电芯之和）
					U16 E2usVPackGain2;				// B+总压增益
					S16 E2ssCurrGain1;				// VADC电流增益
					S16 E2ssCurrOffset1;			// VADC电流偏移量
					S16 E2ssCurrGain2;				// CADC电流增益
					S16 E2ssCurrOffset2;			// CADC电流偏移量
					S16 E2ssTSnOffset[4];			// TSn(1~4)温度偏移量
                    U8  E2ucRamCheckFlgB;
				};
			};
			
			// 最后末尾的标志位
			union
			{
				U16 E2usSubclassEnd;
			};
		};
	};
	U16 rev;
	U16 E2usCheckFlag;
}PARAMETER;

extern U16 const  NTC103AT[];

extern AFEREG REG;
extern AFEDATA AFE;
extern SYSINFOR Info;
extern PARAMETER parameter;

extern void Delay1ms(U8 delaycnt);
extern void Delay5us(U8 delaycnt);
extern U8 CRC8Calcu(U8 *p, U8 Length);
#endif



