#ifndef MAIN_H
#define MAIN_H

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
#include "Runtime.h"
#include "MosStartup.h"

#include "conf.h"
#include "rtc_sleep.h"


#define UPDNLMT16(Var,Max,Min)	{(Var)=((Var)>=(Max))?(Max):(Var);(Var)=((Var)<=(Min))?(Min):(Var);}
#define U16_SwapEndian(target)  ((((target) & 0xFFU) << 8) | (((target) >> 8) & 0xFFU))
#define MCU_RESET()	NVIC_SystemReset()

typedef enum _IO_STATUS {
	OPEN = 1, CLOSE = 0
}IO_STATUS;

typedef enum _BOOL {
	FALSE = 0, TRUE
}BOOL;

extern UINT8 SeriesNum;

#endif	/* MAIN_H */
