#ifndef __PROTECT_H
#define __PROTECT_H
#include "Common.h"


//电压保护相关参数
#define OVR_DELAY_CNT           (parameter.E2ucOVRDelay*1) 
#define UVR_DELAY_CNT           (parameter.E2ucUVRDelay*1)

#define PACKOV_VOL				(parameter.E2usPackOVVol*100)
#define PACKOVR_VOL			    (parameter.E2usPackOVRVol*100)
#define PACKUV_VOL				(parameter.E2usPackUVVol*100)
#define PACKUVR_VOL			    (parameter.E2usPackUVRVol*100)
#define	PACKOV_DELAY_CNT		(parameter.E2ucPackOVDelay*1)
#define	PACKOVR_DELAY_CNT		(parameter.E2ucPackOVRDelay*1)
#define	PACKUV_DELAY_CNT		(parameter.E2ucPackUVDelay*1)
#define	PACKUVR_DELAY_CNT		(parameter.E2ucPackUVRDelay*1)

#define TEMPER_DELAY_CNT		(parameter.E2ucOUTRDelay*1)		// 温度恢复延时阈值
#define TEMP_OTI				2731+100*10						// 内部温度保护阈值，100℃
#define TEMP_OTIR				2731+90*10						// 内部温度保护阈值，90℃

extern void ProtectProcess(void);

#endif









