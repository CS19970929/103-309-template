#ifndef DATADEAL_SYS_H
#define DATADEAL_SYS_H

#include "conf.h"

#define CompensateNUM 			16

#define CopperLoss_Min 			(UINT16)0
#define CopperLoss_Default 		(UINT16)0
#define CopperLoss_Max 			(UINT16)10000

#define CopperLossNum_Min 		(UINT16)0
#define CopperLossNum_Default 	(UINT16)0
#define CopperLossNum_Max 		(UINT16)32

typedef enum _CUR {
CurCHG = 0, CurDSG
}_Cur;


enum TempArray {
	AFE1_TEMP1 = 0,
	AFE1_TEMP2,
	AFE1_TEMP3,
	AFE2_TEMP1,
	AFE2_TEMP2,
	AFE2_TEMP3,
	ENV_TEMP1,
	ENV_TEMP2,
	ENV_TEMP3,
	MOS_TEMP1,
	TEMP_NUM
};


enum tagInfoForKBArray {
	VOLT_C1 = 0,
	VOLT_C2,
	VOLT_C3,
	VOLT_C4,
	VOLT_C5,
	VOLT_C6,
	VOLT_C7,
	VOLT_C8,
	VOLT_C9,
	VOLT_C10,
	VOLT_C11,
	VOLT_C12,
	VOLT_C13,
	VOLT_C14,
	VOLT_C15,
	VOLT_C16,
	VOLT_C17,
	VOLT_C18,
	VOLT_C19,
	VOLT_C20,
	VOLT_C21,
	VOLT_C22,
	VOLT_C23,
	VOLT_C24,
	VOLT_C25,
	VOLT_C26,
	VOLT_C27,
	VOLT_C28,
	VOLT_C29,
	VOLT_C30,
	VOLT_C31,
	VOLT_C32,
	VOLT_AFE1,
	VOLT_AFE2,
	VOLT_VBUS,

	MDL_ICHG,
	MDL_IDSG,
	MDL_TEMP1,
	MDL_TEMP2,
	MDL_TEMP3,
	MDL_TEMP4,
	MDL_TEMP5,
	MDL_TEMP6,
	MDL_TEMP_ENV1,
	MDL_TEMP_ENV2,
	MDL_TEMP_ENV3,
	MDL_TEMP_MOS1,
    KB_NUM		  // KB number_47
};

typedef enum _AFE_CURRENT_ZERO_STATE {
    AFE_CURRENT_ZERO_IDLE = 0,
    AFE_CURRENT_ZERO_STARTUP = 1,
    AFE_CURRENT_ZERO_READY = 2,
    AFE_CURRENT_ZERO_TIMEOUT = 3,
    AFE_CURRENT_ZERO_IIC_FAIL = 4,
    AFE_CURRENT_ZERO_RANGE_FAIL = 5
} AFE_CURRENT_ZERO_STATE;

#if PROJECT_CFG_DEBUG_WATCH_ENABLE
typedef enum _AFE_CURRENT_DIR {
    AFE_CURRENT_DIR_ZERO = 0,
    AFE_CURRENT_DIR_CHG = 1,
    AFE_CURRENT_DIR_DSG = 2
} AFE_CURRENT_DIR;

typedef struct _AFE_CURRENT_OBSERVE {
    UINT16 u16RawCode;
    INT32 i32RawSigned;
    INT32 i32ZeroOffsetRaw;
    INT32 i32CorrectedRaw;
    UINT32 u32AbsRaw;
    UINT32 u32Current_mA;
    UINT32 u32ChgCurrent_mA;
    UINT32 u32DsgCurrent_mA;
    UINT16 u16Ichg_A10;
    UINT16 u16IDsg_A10;
    UINT16 u16ZeroLimitRaw;
    UINT16 u16ZeroDeadbandRaw;
    UINT16 u16ZeroDeltaRaw;
    UINT8 u8ZeroState;
    UINT8 u8ZeroReady;
    UINT8 u8StableCnt;
    UINT8 u8StartupSampleCnt;
    UINT8 u8StartupColdBoot;
    UINT8 u8StartupDiscardCnt;
    UINT8 u8StartupFailCnt;
    UINT8 u8StartupRangeFailCnt;
    UINT8 u8KbCalibEnable;
    UINT8 u8Direction;
    UINT8 u8CtlcState;
} AFE_CURRENT_OBSERVE;
#endif

#define SYSKMAX   		((UINT16)1536)      // 1.5
#define SYSKDEFAULT		((UINT16)1024)      // 1
#define SYSKMIN   		((UINT16)512)       // 0.5

#define SYSBMAX   		((INT16)30000)      // 30
#define SYSBDEFAULT		((INT16)0)      	// 0
#define SYSBMIN   		((INT16)-30000)     // -30


struct OTHER_ELEMENT {
    UINT16 u16Balance_OpenVoltage;	//mV�����⿪����ѹ
    UINT16 u16Balance_OpenWindow;	//mV�����⿪��ѹ��
    UINT16 u16Balance_CloseWindow;	//mV������ر�ѹ��
    UINT16 u16Balance_Res1;			//����λ
    UINT16 u16Balance_Res2;			//����λ
    UINT16 u16Balance_Res3;			//����λ
    UINT16 u16Balance_Res4;			//����λ
    UINT16 u16Balance_Res5;			//����λ

	UINT16 u16CS_Cur_CHGmax;		//A*10
	UINT16 u16CS_Cur_DSGmax;		//A*10
	UINT16 u16CBC_DelayT;			//us*10
	UINT16 u16CBC_Cur_DSG;			//A*10
	
	UINT16 u16Soc_TableSelect;		//ԭ����u16Password_Once
	UINT16 u16Password_Always;		//û��
	UINT16 u16CurLimit_Vdelta;		//mV
	UINT16 u16CurLimit_Cur;			//A*10

    UINT16 u16Sleep_VNormal;      	//mV
	UINT16 u16Sleep_TimeNormal;		//min
    UINT16 u16Sleep_Vlow;     		//mV
	UINT16 u16Sleep_TimeVlow;		//min
	UINT16 u16Sleep_VirCur_Chg;     //A *10
    UINT16 u16Sleep_VirCur_Dsg;    	//A *10
	UINT16 u16Sleep_RTC_WakeUpTime;	//min��RTC����ʱ��
	UINT16 u16Sleep_TimeRTC;		//min������RTC����ʱ��

	UINT16 u16Soc_Ah;               //10*Ah
	UINT16 u16Soc_Cycle_times;		//ѭ������*1
	UINT16 u16Soc_V_100;			//SOCΪ100�ĵ�ѹ��
	UINT16 u16Soc_V_0;				//SOCΪ0�ĵ�ѹ��

	UINT16 u16Sys_SeriesNum;		//N
	UINT16 u16Sys_CS_Res;			//m��
	UINT16 u16Sys_CS_Res_Num;		//N
	UINT16 u16Sys_PreChg_Time;		//s��Ԥ��ʱ��
};


#define  BMS_HARDWARE_VERDION_DEFAULT   "T3_27Ah"
// #define  BMS_SOFTWARE_VERDION_DEFAULT   "FD-260429-T3MAX"  //32
#define  BMS_SOFTWARE_VERDION_DEFAULT   "D010"  //32
// #define  BMS_SERIAL_NUMBER_DEFAULT  	  "FD_20260523"
// #define  BMS_SERIAL_NUMBER_DEFAULT  	  "cs-666FD_20268888"
#define  BMS_SERIAL_NUMBER_DEFAULT  	  "cs-666-8888-0527"

#define SNum 		10



#define CS_Cur_CHGmax	((INT32)CS_Res_Num*1250/CS_Res-10)
#define CS_Cur_DSGmax	CS_Cur_CHGmax
#define CBC_DelayT		2000
// #define CBC_Cur_DSG		((CS_Cur_CHGmax<<2)/5)
#define CBC_Cur_DSG		(2000)

#if (BAT_TYPE == BAT_MASTER)
#define CS_Res			2
#define CS_Res_Num		3
#define BMS_CAPCITY     180
#elif (BAT_TYPE == BAT_SLAVE)
#define CS_Res			2
#define CS_Res_Num		4
#define BMS_CAPCITY     270
#endif	


#define OtherElement_min		{1000,	1,		0,		0,		0,	0,	0,	0,\
	                           	 0,		0,		0,		0,\
	                           	 0,		0,		0,		0,\
	                             1000,	1,		1000,	1,		0, 	0, 	0, 	0,\
	                             1, 	1, 		0, 		0,\
	                             3,		1,		1,		0}


#ifdef TERNARYLI
#define OtherElement_default 	{4160,	30,	20,		0,	0,		0,	0,	0,\
	                             CS_Cur_CHGmax,	CS_Cur_DSGmax,CBC_DelayT,CBC_Cur_DSG,\
	                             SOC_TABLE_TERNARYLI,	0,		1000,	30,\
	                             3200,	7200,	3000,	1440,		10,		10,	240,3,\
	                             BMS_CAPCITY,	3,		4180,	3000,\
	                             SNum,CS_Res,CS_Res_Num,10}
#elif (defined(LIFEPO))
#define OtherElement_default 	{3300,	50,	20,	0,	0,	0,	0,	0,\
	                             CS_Cur_CHGmax,	CS_Cur_DSGmax,CBC_DelayT,CBC_Cur_DSG,\
	                             SOC_TABLE_LIFEPO,0,1000,30,\
	                             3200,	7200,	3000,	10,	10,	10,	240, 3,\
	                             BMS_CAPCITY,	3,		3600,	3000,\
	                             SNum,CS_Res,CS_Res_Num,10}
#endif


#define OtherElement_max 		{5000,2000,2000,65000,65000,65000,65000,65000,\
	                             65000,65000,65000,65000,\
	                             65000,65000,65000,65000,\
	                             5000,65000,5000,65000,50000,50000,50000,50000,\
	                             65000,50000,50000,50000,\
	                             32,65000,10000,50000}



extern UINT16 g_u16CalibCoefK[KB_NUM];
extern INT16  g_i16CalibCoefB[KB_NUM];
extern UINT16 CopperLoss[CompensateNUM];
extern UINT16 CopperLoss_Num[CompensateNUM];
extern struct OTHER_ELEMENT OtherElement;
extern UINT32 g_u32CS_Res_AFE;
extern UINT32 g_u32AfeCurrentSampleSeq;
#if PROJECT_CFG_DEBUG_WATCH_ENABLE
extern AFE_CURRENT_OBSERVE g_stAfeCurrentObserve;
#endif

void App_AFEGet(  void);
void AfeCurrent_SetStartupColdBoot(UINT8 cold_boot);
void AfeCurrent_PrepareStartupZero(void);
void AfeCurrent_StartupZeroCal(void);
UINT8 AfeCurrent_IsStartupZeroDone(void);
void open_ctlc(void);
void close_ctlc(void);

#endif	/* DATADEAL_SYS_H */
/* ── SH367309_DataDeal.h section ── */
/* ── SH367309_DataDeal.h section ── */


#define E2P_ADDR_E2POS_AFE_Parameters			3000		//到974，AFE参数保存到eeprom的起始地址
#define RS485_CMD_ADDR_AFE_ROM_PARAMETERS_START 0x2400  
#define RS485_CMD_ADDR_AFE_ROM_PARAMETERS_END 	0x2417
#define AFE_PARAMETES_TOTAL_LENGTH  			24    //单位，寄存器


#define RS485_ADDR_RW_AFE_PARAMETER  0x2400

#ifdef LIFEPO
#define AFE_COV           (3780)
#define AFE_COV_recover   (3500)
#define AFE_COV_filter     100

#define AFE_CUV           (2800)
#define AFE_CUV_recover     (2900)
#define AFE_CUV_filter     (100)
#else
#define AFE_COV           (4250)
#define AFE_COV_recover   (4150)
#define AFE_COV_filter     100

#define AFE_CUV           (2740)
#define AFE_CUV_recover     (3000)
#define AFE_CUV_filter     (100)
#endif // LIFEPO

#define AFE_OTC           ((60 + 40) * 10)
#define AFE_OTC_recover     ((50 + 40) * 10)
#define AFE_OTC_filter      100

#define AFE_UTC           ((-7 + 40) * 10)
#define AFE_UTC_recover     ((0 + 40) * 10)
#define AFE_UTC_filter      100

#define AFE_OTD           ((70 + 40) * 10)
#define AFE_OTD_recover     ((60 + 40) * 10)
#define AFE_OTD_filter      100

#define AFE_UTD         ((-20 + 40) * 10)
#define AFE_UTD_recover     ((-10 + 40) * 10)
#define AFE_UTD_filter      100


#define AFE_OCC1       		(500) 
#define AFE_OCC1_filter  	(10)
#define AFE_OCC2       		(600) 
#define AFE_OCC2_filter  	(10)

#define AFE_ODC1       		(800) 
#define AFE_ODC1_filter  	(10)
#define AFE_ODC2       		(1000) 
#define AFE_ODC2_filter  	(10)


/*curValue*/  /*defaultValue*/ /*maxValue*/ /*minValue*/
#define AFE_PARAMETERS_RS485_STRUCTION_DEFAULT  {\
	/*单节过压*/			AFE_COV,			AFE_COV,			5000,	1000,\
	/*单节过压恢复*/		AFE_COV_recover,	AFE_COV_recover,	5000,	1000,\
	/*单节过压延时*/		AFE_COV_filter,		AFE_COV_filter,		50000,	1,\
	/*单节低压*/			AFE_CUV,			AFE_CUV,			5000,	1000,\
	/*单节低压恢复*/		AFE_CUV_recover,	AFE_CUV_recover,	5000,	1000,\
	/*单节低压延时*/		AFE_CUV_filter,		AFE_CUV_filter,		50000,	1,\
	/*一级充电过流*/		AFE_OCC1,			AFE_OCC1,			50000,	10,\
	/*一级充电过流延时*/	AFE_OCC1_filter,	AFE_OCC1_filter,	50000,	1,\
	/*二级充电过流*/		AFE_OCC2,			AFE_OCC2,			50000,	10,\
	/*二级充电过流延时*/	AFE_OCC2_filter,	AFE_OCC2_filter,	50000,	1,\
	/*一级放电过流*/		AFE_ODC1,			AFE_ODC1,			50000,	10,\
	/*一级放电过流延时*/    AFE_ODC1_filter,	AFE_ODC1_filter,	50000,	1,\
	/*二级放电过流*/		AFE_ODC2,	        AFE_ODC2,			50000,	10,\
	/*二级放电过流延时*/    AFE_ODC2_filter,	AFE_ODC2_filter,	50000,	1,\
	/*充电高温*/			AFE_OTC,	       AFE_OTC,				2000,	400,\
	/*充电高温恢复*/		AFE_OTC_recover,	AFE_OTC_recover,	50000,	1,\
	/*充电低温*/			AFE_UTC,	       AFE_UTC,				800,	0,\
	/*充电低温恢复*/		AFE_UTC_recover,	AFE_UTC_recover,	50000,	1,\
	/*放电高温*/			AFE_OTD,	       AFE_OTD,				2000,	400,\
	/*放电高温恢复*/		AFE_OTD_recover,	AFE_OTD_recover,	50000,	1,\
	/*放电低温*/			AFE_UTD,	       AFE_UTD,				800,	0,\
	/*放电低温恢复*/		AFE_UTD_recover,	AFE_UTD_recover,	50000,	1,\
	/*短路电流*/			200,	200,	65000,	0,\
	/*短路延时*/			320,	320,		65000,	0,\
}


//typedef  unsigned char UINT8;
//typedef  unsigned short UINT16;

/******************************* AFE寄存器结构体 *****************************/
typedef struct {
	UINT8 CN 		:4;  		//5-15，对应串数，别的为16串
	UINT8 BAL 		:1;			//0：平衡功能由内部SH367309控制，1:由外部MCU控
	UINT8 OCPM 		:1;			//0:充电过流只关闭充电MOS，放电过流只关放电MOS。1则同时关
	UINT8 ENMOS 	:1;			/*0:禁止充电MOS恢复控制位，1:启动充电MOS恢复控制位。
												(当过充电/温度保护(温度实际关2个)关闭充电MOS后，如果检测到放电状态，则开启充电MOS) */									
	UINT8 ENPCH 	:1;			//0:禁止预充电功能，1：启动预充功能
	
	UINT8 EUVR 		:1;			//0：过放保护状态释放与负载释放无关，意味着负载释放只和电流保护有关了
	UINT8 OCRA 		:1;			/*0：不允许。1：允许---“电流保护定时恢复”功能，
												也即意味着只能负载释放才能解除电流保护，不能自动恢复 */
	UINT8 CTLC 		:2;			//
	UINT8 DIS_PF 	:1;			//
	UINT8 UV_OP 	:1;			//
	UINT8 Reserve 	:1;			//
	UINT8 E0VB 		:1;			//
	
}BYTE_00H_01H_TypeDef;


typedef struct {
	UINT8 OVH 		:2;  		//过充保护前2位
	UINT8 LDRT 		:2;			//11，负载释放延时2000ms，这个和短路保护释放延时有关系
	UINT8 OVT		:4;			//过充电保护延时
	UINT8 OVL;					//过充保护后8位
}BYTE_02H_03H_TypeDef;


typedef struct {
	UINT8 OVRH 		:2;  		//过充保护恢复前2位
	UINT8 Reserve 	:2;		
	UINT8 UVT		:4;			//低压保护延时
	UINT8 OVRL;					//过充保护恢复后8位
}BYTE_04H_05H_TypeDef;

typedef struct {
	UINT8 UV;					//低压保护
	UINT8 UVR;					//低压保护恢复
}BYTE_06H_07H_TypeDef;


typedef struct {
	UINT8 BALV;					//均衡开启电压
	UINT8 PREV;					//预充电压
}BYTE_08H_09H_TypeDef;

typedef struct {
	UINT8 L0V;				//低电压禁止充电电压
	UINT8 PFV;				//二次过充电保护电压设置寄存器，放大一些，不用这个
}BYTE_0AH_0BH_TypeDef;


typedef struct {
	UINT8 OCD1T		:4;			//放电过流1保护延时
	UINT8 OCD1V		:4;			//放电过流保护1保护电压
	UINT8 OCD2T		:4;			//放电过流2保护延时
	UINT8 OCD2V		:4;			//放电过流保护2保护电压
}BYTE_0CH_0DH_TypeDef;

typedef struct {
	UINT8 SCT		:4;			//短路保护延时
	UINT8 SCV		:4;			//短路保护电压
	UINT8 OCCT		:4;			//充电过流保护延时
	UINT8 OCCV		:4;			//充电过流保护电压
}BYTE_0EH_0FH_TypeDef;

typedef struct {
	UINT8 PFT		:2;			//短路保护延时
	UINT8 OCRT		:2;			//短路保护电压
	UINT8 MOST		:2;			//充电过流保护延时
	UINT8 CHS		:2;			//充电过流保护电压
}BYTE_10H_TypeDef;

typedef struct {
	UINT8 OTC;					//充电高温保护
	UINT8 OTCR;					//充电高温保护恢复
	UINT8 UTC;					//充电低温保护
	UINT8 UTCR;					//充电低温保护恢复
	UINT8 OTD;					//放电高温保护
	UINT8 OTDR;					//放电高温保护恢复
	UINT8 UTD;					//放电低温保护
	UINT8 UTDR;					//放电低温保护恢复
	UINT8 TR;
}BYTE_11H_19H_TypeDef;


typedef	struct {
	BYTE_00H_01H_TypeDef 	m00H_01H;
	BYTE_02H_03H_TypeDef	m02H_03H;
	BYTE_04H_05H_TypeDef	m04H_05H;
	BYTE_06H_07H_TypeDef 	m06H_07H;
	BYTE_08H_09H_TypeDef 	m08H_09H;
	BYTE_0AH_0BH_TypeDef 	m0AH_0BH;
	BYTE_0CH_0DH_TypeDef	m0CH_0DH;
	BYTE_0EH_0FH_TypeDef 	m0EH_0FH;
	BYTE_10H_TypeDef 			m10H;
	BYTE_11H_19H_TypeDef	m11H_19H;
}AFE_ROM_PARAMETERS_TypeDef;



/******************************* AFE保护参数结构体 *****************************/
typedef struct {
	UINT16 curValue;			//当前值
	UINT16 defaultValue;		//默认值
	UINT16 maxValue;			//最大值
	UINT16 minValue;			//最小值
}AFE_Value_Typedef;

typedef struct{
	AFE_Value_Typedef	u16VcellOvp;  		//单节过压 mv
	AFE_Value_Typedef	u16VcellOvp_Rcv;	//过压恢复 mv
	AFE_Value_Typedef	u16VcellOvp_Filter;	//过压延时 10ms
	
	AFE_Value_Typedef	u16VcellUvp;		//单节低压
	AFE_Value_Typedef	u16VcellUvp_Rcv;
	AFE_Value_Typedef	u16VcellUvp_Filter;
	
	AFE_Value_Typedef	u16IchgOcp_First;	//一级充电过流 A*10
	AFE_Value_Typedef	u16IchgOcp_Filter_First;
	
	AFE_Value_Typedef	u16IchgOcp_Second;	//二级充电过流
	AFE_Value_Typedef	u16IchgOcp_Filter_Second;
	
	AFE_Value_Typedef	u16IdsgOcp_First;	//一级放电过流
	AFE_Value_Typedef	u16IdsgOcp_Filter_First;
	
	AFE_Value_Typedef	u16IdsgOcp_Second;	//二级放电过流
	AFE_Value_Typedef	u16IdsgOcp_Filter_Second;
	
	AFE_Value_Typedef	u16TChgOTp;			//充电高温 (℃*10+400)
	AFE_Value_Typedef	u16TChgOTp_Rcv;
	AFE_Value_Typedef	u16TchgUTp;			//充电低温
	AFE_Value_Typedef	u16TchgUTp_Rcv;
	AFE_Value_Typedef	u16TdischgOTp;		//放电高温
	AFE_Value_Typedef	u16TdischgOTp_Rcv;
	AFE_Value_Typedef	u16TdischgUTp;		//放电低温
	AFE_Value_Typedef	u16TdischgUTp_Rcv;
	AFE_Value_Typedef 	u16CBC_Cur_DSG;
	AFE_Value_Typedef 	u16CBC_DelayT;
}AFE_Parameters_RS485_Typedef; 



void App_SH367309_Supplement(void);

void Sci_ACK_0x03_RW_AFE_Parameters(struct RS485MSG *s,UINT8 t_u8BuffTemp[]);
UINT8 Sci_WrRegs_0x10_AFE_Parameters(UINT16 u16Channel,struct RS485MSG *s);
void Sci_WrReg_0x06_Reset_AFE_Parameters(struct RS485MSG *s);

UINT8 EEPROM_ResetData_AFE_ParametersToDefault(void);
void ReadEEPROM_AFE_Parameters(void);


extern int AFE_PARAM_WRITE_Flag;

#endif  /* DATADEAL_SYS_H */
