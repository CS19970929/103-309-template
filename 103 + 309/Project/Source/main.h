#ifndef MAIN_H
#define MAIN_H

#include <math.h>
#include "stm32f10x.h"
#include "stm32f10x_it.h"
#include "string.h"

#include "DataDeal.h"
#include "Sci_Upper.h"
#include "System_Init.h"
#include "System_Monitor.h"
#include "EEPROM.h"
#include "Fault.h"
#include "SOC.h"
#include "ADC.h"
#include "RTC.h"
#include "PubFunc.h"
#include "Can_HDX.h"
#include "I2C_AFE1.h"
#include "Flash.h"
#include "SleepDeal.h"
#include "ProductionID.h"
#include "SH367309_Func.h"
#include "SH367309_DataDeal.h"
#include "LogRecord.h"
#include "LedBar.h"
#include "AppInit.h"
#include "MosStartup.h"

#include "ShortFunc.h"
#include "conf.h"
#include "rtc_sleep.h"


#define UPDNLMT16(Var,Max,Min)	{(Var)=((Var)>=(Max))?(Max):(Var);(Var)=((Var)<=(Min))?(Min):(Var);}
#define S2U(x)   (*((volatile UINT16*)(&(x))))
#define U16_SwapEndian(target)  ((((target) & 0xFFU) << 8) | (((target) >> 8) & 0xFFU))
#define MCU_RESET()	NVIC_SystemReset()

typedef enum _IO_STATUS {
	OPEN = 1, CLOSE = 0
}IO_STATUS;

typedef enum _BOOL {
	FALSE = 0, TRUE
}BOOL;


/* 10ms delay constants */
#define DELAYB10MS_0MS       ((UINT16)0)
#define DELAYB10MS_30MS      ((UINT16)3)
#define DELAYB10MS_50MS      ((UINT16)5)
#define DELAYB10MS_100MS     ((UINT16)10)
#define DELAYB10MS_200MS     ((UINT16)20)
#define DELAYB10MS_250MS     ((UINT16)25)
#define DELAYB10MS_500MS     ((UINT16)50)
#define DELAYB10MS_1S        ((UINT16)100)
#define DELAYB10MS_1S5       ((UINT16)150)
#define DELAYB10MS_2S        ((UINT16)200)
#define DELAYB10MS_2S5       ((UINT16)250)
#define DELAYB10MS_3S        ((UINT16)300)
#define DELAYB10MS_4S        ((UINT16)400)
#define DELAYB10MS_5S        ((UINT16)500)
#define DELAYB10MS_10S       ((UINT16)1000)
#define DELAYB10MS_30S       ((UINT16)3000)
#define DELAYB10MS_2MIN      ((UINT16)12000)


extern UINT8 SeriesNum;

#endif	/* MAIN_H */
