#ifndef MAIN_H
#define MAIN_H

//#define USE_STDPERIPH_DRIVER	//û�������һ���assert_param()�����Ĵ���Ҫ�ڹ����ļ�������
#include "Project_Types.h"
#include "Project_Features.h"
#include "BoardControl.h"
#include <math.h>
#include "stm32f10x.h"
#include "stm32f10x_it.h"			//������һЩӲ������֮����жϣ�������Ҫ��
#include "string.h"

#include "DataDeal.h"				//����Sci_Upper.hǰ�����
#include "Sci_Upper.h"
#include "System_Init.h"
#include "System_Monitor.h"
#include "EEPROM.h"
#include "Fault.h"
#include "SOC.h"
#include "ADC.h"
#include "RTC.h"
#include "PubFunc.h"
//#include "Can_GW_SH.h"
#include "Can_HDX.h"
#include "I2C_AFE1.h"
#include "Flash.h"
#include "SleepDeal.h"
#include "Heat_Cool.h"
#include "ProductionID.h"
#include "SH367309_Func.h"
#include "ChargerLoadFunc.h"
#include "SH367309_DataDeal.h"
#include "LogRecord.h"
#include "LedBar.h"

#include "IO_Control.h"
#include "ShortFunc.h"
#include "conf.h"
#include "rtc_sleep.h"

#include "elog.h"


#ifndef MCU_RESET
#define MCU_RESET()	NVIC_SystemReset()
#endif


/* Project feature switches and SCI roles are derived from conf/Project_Config.h. */

void InitSystemWakeUp(void);
UINT8 FactoryAging_IsActive(void);
UINT8 FactoryAging_SaveProgressBeforeSleep(void);


extern const unsigned char SeriesSelect_AFE1[16][16];
extern UINT8 SeriesNum;

#endif	/* MAIN_H */
