#ifndef __BALANCE_H
#define __BALANCE_H

#include "Common.h"

extern void CellOpenProcess(void);
extern void BalProcess(void);
 
//平衡及断线检测相关的变量
#define BALANCE_DELAY_CNT			(parameter.E2ucBalanceDelay*1)		// 平衡进入延时
#define BalUpdate_DELAY_CNT			100									// 均衡更新延时100*70ms

#define CTO_DELAY_CNT				10									// 10s

#endif
