#ifndef MAIN_H
#define MAIN_H

//#define USE_STDPERIPH_DRIVER	//û�������һ���assert_param()�����Ĵ���Ҫ�ڹ����ļ�������
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


#define UPDNLMT16(Var,Max,Min)	{(Var)=((Var)>=(Max))?(Max):(Var);(Var)=((Var)<=(Min))?(Min):(Var);}
#define S2U(x)   (*((volatile UINT16*)(&(x))))
#define MCU_RESET()	NVIC_SystemReset()
#define CRC_KEY 	7
#define I2C_RW_W	0
#define I2C_RW_R	1

// typedef enum _IO_STATUS {
// OPEN = 1, CLOSE = 0
// }IO_STATUS;

typedef enum _BOOL {
FALSE = 0, TRUE
}BOOL;


//10msʱ��������
#define DELAYB10MS_0MS       ((UINT16)0)            //0ms
#define DELAYB10MS_30MS      ((UINT16)3)            //30ms
#define DELAYB10MS_50MS      ((UINT16)5)            //50ms
#define DELAYB10MS_100MS     ((UINT16)10)           //100ms
#define DELAYB10MS_200MS     ((UINT16)20)           //200ms
#define DELAYB10MS_250MS     ((UINT16)25)           //250ms
#define DELAYB10MS_500MS     ((UINT16)50)           //500ms
#define DELAYB10MS_1S        ((UINT16)100)          //1s
#define DELAYB10MS_1S5       ((UINT16)150)          //1.5s
#define DELAYB10MS_2S        ((UINT16)200)          //2s
#define DELAYB10MS_2S5       ((UINT16)250)          //2.5s
#define DELAYB10MS_3S        ((UINT16)300)          //3s
#define DELAYB10MS_4S        ((UINT16)400)          // 4s
#define DELAYB10MS_5S        ((UINT16)500)          // 5s
#define DELAYB10MS_10S       ((UINT16)1000)         //10s
#define DELAYB10MS_30S       ((UINT16)3000)         //30s
#define DELAYB10MS_2MIN      ((UINT16)12000)         //30s


/* Project feature switches and SCI roles are derived from conf/Project_Config.h. */

void InitSystemWakeUp(void);
void open_chg_close_dsg(void);
void open_dsg_close_chg(void);
void enter_fac_mode(bool on);
UINT8 MosStartup_Is5vChargeActive(void);
void MosStartup_ApplyInitialState(void);
UINT8 FactoryAging_IsActive(void);
UINT8 FactoryAging_ShouldStartOnBoot(void);
UINT8 FactoryAging_SaveProgressBeforeSleep(void);


extern const unsigned char SeriesSelect_AFE1[16][16];
extern UINT8 SeriesNum;

#endif	/* MAIN_H */
