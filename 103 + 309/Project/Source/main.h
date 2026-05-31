#ifndef MAIN_H
#define MAIN_H

#include <math.h>
#include "stm32f10x.h"
#include "stm32f10x_it.h"
#include "string.h"

#include "data_model.h"               /* must precede Sci_Upper.h */
#include "Sci_Upper.h"
#include "system.h"
#include "EEPROM.h"
#include "Fault.h"
#include "SOC.h"
#include "ADC.h"
#include "RTC.h"
#include "can.h"
#include "afe_driver.h"
#include "Flash.h"
#include "SleepDeal.h"
#include "ProductionID.h"
#include "LogRecord.h"
#include "LedBar.h"
#include "AppInit.h"
#include "MosStartup.h"
#include "utils.h"

#include "conf.h"
#include "low_power.h"

#include "elog.h"

/* ── Utility macros ── */
#define UPDNLMT16(Var,Max,Min)  {(Var)=((Var)>=(Max))?(Max):(Var);(Var)=((Var)<=(Min))?(Min):(Var);}
#define MCU_RESET()             NVIC_SystemReset()

/* ── Project feature switches derived from conf/Project_Config.h ── */

extern UINT8 SeriesNum;

#endif  /* MAIN_H */
