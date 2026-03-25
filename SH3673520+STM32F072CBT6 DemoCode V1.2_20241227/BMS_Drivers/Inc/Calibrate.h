#ifndef __CALIBRATE_H
#define __CALIBRATE_H

#include "Common.h"

//校准相关
extern BOOL bCaliFlg;				// 上位机发送校准命令后置位该标志

extern U32	uiExtVPack;				// 总压校准输入电压值
extern S32	siExtCur;				// 电流校准输入电流值
extern U32	uiExtTemp[4];			// 温度校准输入温度值
extern U8	ucExtcaliSwitch1;

extern void CaliProcess(void);

#endif


