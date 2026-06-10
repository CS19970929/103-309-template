#ifndef SYSTEM_INIT_H
#define SYSTEM_INIT_H

//λ������,ʵ��51���Ƶ�GPIO���ƹ���
//����ʵ��˼��,�ο�<<CM3Ȩ��ָ��>>������(87ҳ~92ҳ).
//IO�ڲ����궨��
#define BITBAND(addr, bitnum) ((addr & 0xF0000000)+0x2000000+((addr &0xFFFFF)<<5)+(bitnum<<2)) 
#define MEM_ADDR(addr)  *((volatile unsigned long  *)(addr)) 
#define BIT_ADDR(addr, bitnum)   MEM_ADDR(BITBAND(addr, bitnum)) 
//IO�ڵ�ַӳ��
#define GPIOA_ODR_Addr    (GPIOA_BASE+12) //0x4001080C
#define GPIOB_ODR_Addr    (GPIOB_BASE+12) //0x40010C0C
#define GPIOC_ODR_Addr    (GPIOC_BASE+12) //0x4001100C
#define GPIOD_ODR_Addr    (GPIOD_BASE+12) //0x4001140C
#define GPIOE_ODR_Addr    (GPIOE_BASE+12) //0x4001180C
#define GPIOF_ODR_Addr    (GPIOF_BASE+12) //0x40011A0C
#define GPIOG_ODR_Addr    (GPIOG_BASE+12) //0x40011E0C

#define GPIOA_IDR_Addr    (GPIOA_BASE+8) //0x40010808
#define GPIOB_IDR_Addr    (GPIOB_BASE+8) //0x40010C08
#define GPIOC_IDR_Addr    (GPIOC_BASE+8) //0x40011008
#define GPIOD_IDR_Addr    (GPIOD_BASE+8) //0x40011408
#define GPIOE_IDR_Addr    (GPIOE_BASE+8) //0x40011808
#define GPIOF_IDR_Addr    (GPIOF_BASE+8) //0x40011A08
#define GPIOG_IDR_Addr    (GPIOG_BASE+8) //0x40011E08

//IO�ڲ���,ֻ�Ե�һ��IO��!
//ȷ��n��ֵС��16!
#define PAout(n)   BIT_ADDR(GPIOA_ODR_Addr,n)  //���
#define PAin(n)    BIT_ADDR(GPIOA_IDR_Addr,n)  //����

#define PBout(n)   BIT_ADDR(GPIOB_ODR_Addr,n)  //���
#define PBin(n)    BIT_ADDR(GPIOB_IDR_Addr,n)  //����

#define PCout(n)   BIT_ADDR(GPIOC_ODR_Addr,n)  //���
#define PCin(n)    BIT_ADDR(GPIOC_IDR_Addr,n)  //����

#define PDout(n)   BIT_ADDR(GPIOD_ODR_Addr,n)  //���
#define PDin(n)    BIT_ADDR(GPIOD_IDR_Addr,n)  //����

#define PEout(n)   BIT_ADDR(GPIOE_ODR_Addr,n)  //���
#define PEin(n)    BIT_ADDR(GPIOE_IDR_Addr,n)  //����

#define PFout(n)   BIT_ADDR(GPIOF_ODR_Addr,n)  //���
#define PFin(n)    BIT_ADDR(GPIOF_IDR_Addr,n)  //����

#define PGout(n)   BIT_ADDR(GPIOG_ODR_Addr,n)  //���
#define PGin(n)    BIT_ADDR(GPIOG_IDR_Addr,n)  //����


#define MCUO_DEBUG_LED1 	PBout(15)		//LED1

//��Դģ��
#define MCUO_DRV_CMNT		PCout(12)		//
#define MCUO_PWSV_CTR		PCout(13)		//
#define MCUO_PWSV_STB		PDout(2)		//
#define MCUO_BLE_EN 		PBout(12)



//����ģ��
#define MCUO_E2PR_WP		PBout(13)	//EEPROMд����
#define MCUO_DRV_DET_CHG	PBout(3)	//
//#define MCUO_DRV_DET_LOAD	PAout(8)	//

#define MCUI_INT_WK_MCU		PAin(0)		//����MCU
#define MCUI_INT_WK_CHG		PAin(15)	//��绽��
//#define MCUI_INT_WK_LOAD	PDin(2)		//���ػ���
#define MCUI_CBC_DSG 		PBin(12)


//����ģ��
#define MCUO_MOS_PRE 		PCout(12)	//PC12�����MOS



#define MCUO_RELAY_HEAT 	PCout(6)		//���ȼ̵���
#define MCUO_RELAY_COOL 	PCout(6)		//�����̵���

#define MCUO_ENO_DO1		PCout(0)	//O��1
#define MCUO_ENO_DO2		PCout(1)	//O��2
#define MCUO_ENO_DO3		PCout(2)	//O��3
#define MCUO_ENO_DO4		PCout(3)	//O��4
#define MCUO_ENO_DO5		PBout(4)	//O��5
#define MCUO_ENO_DO6		PBout(1)	//O��6
#define MCUI_ENI_DI1		PBin(5)		//I��1


union SYS_TIME {			//TODO
    UINT16 all;
    struct StatusSysTimeFlagBit {
        UINT8 b1Sys10msFlag1        :1;
        UINT8 b1Sys10msFlag2        :1;
        UINT8 b1Sys10msFlag3        :1;
        UINT8 b1Sys10msFlag4        :1;
		
        UINT8 b1Sys10msFlag5        :1;
		//UINT8 b1Sys20msFlag        	:1;
		UINT8 b1Sys1msFlag        	:1;	
		UINT8 b1Sys50msFlag        	:1;
		UINT8 b1Sys100msFlag       	:1;

		UINT8 b1Sys200msFlag1       :1;
		UINT8 b1Sys200msFlag2       :1;
		UINT8 b1Sys200msFlag3       :1;
		UINT8 b1Sys200msFlag4       :1;
		
		UINT8 b1Sys200msFlag5       :1;
		UINT8 b1Sys1000msFlag1      :1;
		UINT8 b1Sys1000msFlag2      :1;
		UINT8 b1Sys1000msFlag3      :1;
     }bits;
};


struct CBC_ELEMENT {
	UINT8 u8CBC_CHG_ErrFlag;	//����CBC������־λ
	UINT8 u8CBC_CHG_Cnt;		//���ֳ��CBC�Ĵ���
	UINT8 u8CBC_DSG_ErrFlag;	//����CBC������־λ
	UINT8 u8CBC_DSG_Cnt;		//���ַŵ�CBC�Ĵ���
};


typedef enum _APP_TRACE_TASK_ID {
	APP_TRACE_TASK_SYS_TIME = 0,
	APP_TRACE_TASK_WARN_CTRL,
	APP_TRACE_TASK_LED_BAR,
	APP_TRACE_TASK_AFE_GET,
	APP_TRACE_TASK_POWER_UI,
	APP_TRACE_TASK_SCI,
	APP_TRACE_TASK_ANALOG_CAL,
	APP_TRACE_TASK_EEPROM,
	APP_TRACE_TASK_CELL_BALANCE,
	APP_TRACE_TASK_SLEEP,
	APP_TRACE_TASK_SOC,
	APP_TRACE_TASK_BMS_EUAVCAN,
	APP_TRACE_TASK_HEAT_COOL,
	APP_TRACE_TASK_CHARGER_ON,
	APP_TRACE_TASK_FLASH_UPDATE,
	APP_TRACE_TASK_LOG_RECORD,
	APP_TRACE_TASK_PRO_ID,
	APP_TRACE_TASK_IWDG_FEED,
	APP_TRACE_TASK_NUM
} APP_TRACE_TASK_ID;

#define APP_TRACE_TASK_NONE ((UINT8)0xFF)

typedef enum _APP_WARN_CHECK_ID {
	APP_WARN_CHECK_CELL_OVP_SECOND = 0,
	APP_WARN_CHECK_CELL_OVP_THIRD,
	APP_WARN_CHECK_CELL_UVP_SECOND,
	APP_WARN_CHECK_CELL_UVP_THIRD,
	APP_WARN_CHECK_BAT_OVP_SECOND,
	APP_WARN_CHECK_BAT_OVP_THIRD,
	APP_WARN_CHECK_BAT_UVP_SECOND,
	APP_WARN_CHECK_BAT_UVP_THIRD,
	APP_WARN_CHECK_MOS_OTP_SECOND,
	APP_WARN_CHECK_MOS_OTP_THIRD,
	APP_WARN_CHECK_VDELTA_OP_SECOND,
	APP_WARN_CHECK_VDELTA_OP_THIRD,
	APP_WARN_CHECK_IDISCHG_OCP_SECOND,
	APP_WARN_CHECK_IDISCHG_OCP_THIRD,
	APP_WARN_CHECK_ICHG_OCP_SECOND,
	APP_WARN_CHECK_ICHG_OCP_THIRD,
	APP_WARN_CHECK_CELL_SOC_UP_SECOND,
	APP_WARN_CHECK_CELL_SOC_UP_THIRD,
	APP_WARN_CHECK_CELL_DISCHG_OTP_SECOND,
	APP_WARN_CHECK_CELL_DISCHG_OTP_THIRD,
	APP_WARN_CHECK_CELL_DISCHG_UTP_SECOND,
	APP_WARN_CHECK_CELL_DISCHG_UTP_THIRD,
	APP_WARN_CHECK_CELL_CHG_OTP_SECOND,
	APP_WARN_CHECK_CELL_CHG_OTP_THIRD,
	APP_WARN_CHECK_CELL_CHG_UTP_SECOND,
	APP_WARN_CHECK_CELL_CHG_UTP_THIRD,
	APP_WARN_CHECK_NUM
} APP_WARN_CHECK_ID;

#define APP_WARN_CHECK_NONE ((UINT8)0xFF)

typedef struct _APP_TRACE_TASK {
	volatile UINT32 runCnt;
	volatile UINT32 lastLoopCnt;
	volatile UINT32 last1msTick;
	volatile UINT32 last10msPhaseTick;
	volatile UINT32 last10msFlag1Tick;
	volatile UINT16 lastLoopInterval;
	volatile UINT16 maxLoopInterval;
	volatile UINT16 last10msFlag1Interval;
	volatile UINT16 max10msFlag1Interval;
	volatile UINT8 active;
} APP_TRACE_TASK;

typedef struct _APP_TRACE_WARN_CHECK {
	volatile UINT32 runCnt;
	volatile UINT32 lastWarnCtrlCnt;
	volatile UINT16 lastWarnCtrlInterval;
	volatile UINT16 maxWarnCtrlInterval;
	volatile UINT8 active;
} APP_TRACE_WARN_CHECK;
void IWDG_Feed(void);
#define Feed_IWatchDog IWDG_Feed()

extern volatile union SYS_TIME g_st_SysTimeFlag;
extern struct CBC_ELEMENT CBC_Element;
extern UINT8 gu8_200msAccClock_Flag;
extern UINT8 gu8_1000msAccClock_Flag;
extern volatile UINT16 gu16_SysTime1msOverrunCnt;
extern volatile UINT16 gu16_SysTime10msPhaseOverrunCnt;
extern volatile APP_TRACE_TASK g_stAppTraceTask[APP_TRACE_TASK_NUM];
extern volatile APP_TRACE_WARN_CHECK g_stAppTraceWarnCheck[APP_WARN_CHECK_NUM];
extern volatile UINT32 gu32_AppTraceLoopCnt;
extern volatile UINT32 gu32_AppTrace1msTick;
extern volatile UINT32 gu32_AppTrace10msPhaseTick;
extern volatile UINT32 gu32_AppTrace10msFlag1Tick;
extern volatile UINT8 gu8_AppTraceCurrentTask;
extern volatile UINT8 gu8_AppTraceCurrentWarnCheck;
extern volatile UINT8 gu8_AppTraceLast10msPhase;


void InitDelay(void);
void __delay_ms(UINT16 nms);
void __delay_us(UINT32 nus);
void InitTimer(void);
void InitIO(void);
//void InitKey(void);
void InitNVIC(void);
void Init_IWDG(void);
void App_CBC(void);
void AppTrace_LoopBegin(void);
void AppTrace_TaskBegin(UINT8 u8TaskId);
void AppTrace_TaskEnd(UINT8 u8TaskId);
void AppTrace_WarnCheckBegin(UINT8 u8CheckId);
void AppTrace_WarnCheckEnd(UINT8 u8CheckId);

#define APP_TRACE_RUN_TASK(TaskId, TaskFunc) \
	do { \
		AppTrace_TaskBegin((UINT8)(TaskId)); \
		TaskFunc(); \
		AppTrace_TaskEnd((UINT8)(TaskId)); \
	} while (0)
void App_SysTime(void);
void App_ChgDet_Status(void);


#endif	/* SYSTEM_INIT_H */

