#ifndef __AFE_H
#define __AFE_H

#include "Common.h"

#define TIME_1S_5S			5
#define	CHKRAM_NUM			20

#define AFE_REG_FLAG1		0x0000
#define AFE_REG_FLAG2		0x1000

#define AFE_RDERR_FLAG		0x0001
#define AFE_RDERR_STATUS	0x0002
#define AFE_RDERR_CELL		0x0004
#define AFE_RDERR_VTOP		0x0008
#define AFE_RDERR_VCHG		0x0010
#define AFE_RDERR_TEMP		0x0020
#define AFE_RDERR_TEMPI		0x0040
#define AFE_RDERR_CADC		0x0080
#define AFE_RDERR_CUR		0x0100

typedef enum 
{
  SH_DISABLE = 0U, 
  SH_ENABLE = !SH_DISABLE
} AFE_FunctionalState;

typedef enum						// AFE保护类型
{
	AFE_FLAG_OV				= 0x01|AFE_REG_FLAG1,
	AFE_FLAG_UV				= 0x02|AFE_REG_FLAG1,
	AFE_FLAG_OCD1			= 0x04|AFE_REG_FLAG1,
	AFE_FLAG_OCD2			= 0x08|AFE_REG_FLAG1,
	AFE_FLAG_SC				= 0x10|AFE_REG_FLAG1,
	AFE_FLAG_OCC			= 0x20|AFE_REG_FLAG1,
	AFE_FLAG_UTC			= 0x10|AFE_REG_FLAG2,
	AFE_FLAG_OTC			= 0x20|AFE_REG_FLAG2,
	AFE_FLAG_UTD			= 0x40|AFE_REG_FLAG2,
	AFE_FLAG_OTD			= 0x80|AFE_REG_FLAG2,
}AFE_ProtectType;

typedef enum						// AFE工作模式
{
	AFE_Mode_Normal			= 0x00,
	AFE_Mode_Idle			= 0x55,
	AFE_Mode_Sleep			= 0xAA, 
	AFE_Mode_PowerDown		= 0x33, 
}AFE_ModeType;

typedef enum						// 负载检测上拉电流
{
	AFE_RLD_60uA			= 0,
	AFE_RLD_500uA			= 1,
}AFE_LoadSourceType;

typedef enum						// 负载连接唤醒方式
{
	AFE_Load_On				= 0,
	AFE_Load_Off			= 1,
}AFE_LoadWakeUpType;

extern BOOL SH_AFE_ClearProtectFlag(AFE_ProtectType AFE_Protect);
extern void SH_AFE_ClearAllProtectFlag(void);
extern void SH_AFE_SetChgMos(AFE_FunctionalState state);
extern void SH_AFE_SetDsgMos(AFE_FunctionalState state);
extern void SH_AFE_WorkModeConfig(AFE_ModeType AFE_Mode);
extern void SH_AFE_ChargerWakeUpConfig(AFE_FunctionalState state);
extern void SH_AFE_LoadWakeUpConfig(AFE_LoadWakeUpType AFE_LDWK, AFE_LoadSourceType AFE_RLD, AFE_FunctionalState state);
extern void SH_AFE_ChargerCheckConfig(AFE_FunctionalState state);
extern void SH_AFE_LoadCheckConfig(AFE_LoadSourceType AFE_RLD, AFE_FunctionalState state);
extern void SH_AFE_BalanceConfig(U32 BALChannel);
extern void SH_AFE_TrigerOWNCheck(AFE_FunctionalState state);
extern U32 SH_AFE_GetOWNValue(void);

extern BOOL SH_AFE_RegisterCheck(void);
extern void SH_AFE_SPICheck(void);
extern BOOL SH_AFE_RegisterInit(void);
extern U16 SH_AFE_ReadRegister(U8 CellNum, U8 TsNum);
#endif

