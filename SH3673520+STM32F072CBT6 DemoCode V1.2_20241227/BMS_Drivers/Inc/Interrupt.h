#ifndef __INTERRUPT_H
#define __INTERRUPT_H
#include "Common.h"


#define TIME_5MS_70MS	14
#define TIME_5MS_1S		200


extern BOOL	bTimer5msFlg;
extern BOOL	bTimer70msFlg;
extern BOOL	bTimer1sFlg;
extern U8 ucTimer70ms;
extern U8 ucTimer1s;

extern void InterruptTimer3(void);

#endif
