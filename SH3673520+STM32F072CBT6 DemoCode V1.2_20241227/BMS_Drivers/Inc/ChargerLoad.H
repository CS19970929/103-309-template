#ifndef __CHARGERLOAD_H
#define __CHARGERLOAD_H
#include "Common.h"

#define LOADCHG_DELAY_CNT		(parameter.E2ucLoadChgChkDelay*1)		// 充电器和负载检测延时
#define ALTER_DELAY_CNT			LOADCHG_DELAY_CNT+1
#define CHGR_CHK_VOL			(parameter.E2usChgRChkVol*100)			// 充电器移除电压阈值

extern BOOL bLoadRStatusFlg;				// 负载未连接状态标志位
extern BOOL bChgerRStatusFlg;				// 充电器移除标志位
extern void LoadChgerChkProcess(void);
#endif
