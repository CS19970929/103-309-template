#ifndef __POWERON_H
#define __POWERON_H
#include "Common.h"


#define LOWPOWER_DELAY_CNT    28
//#define PD_DELAY_CNT        parameter.E2ucPDDelay
//#define SLEEP_DELAY_CNT     parameter.E2ucSleepDelay


extern BOOL bPDFlg;
extern BOOL bSleepFlg;

extern void LowPowerProcess(void);

#endif








