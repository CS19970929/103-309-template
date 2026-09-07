#ifndef BMS_PARAMETERS_H
#define BMS_PARAMETERS_H

#include "conf.h"


#define RS485_CMD_ADDR_BMS_PARAMETERS_START 0x2400
#define RS485_CMD_ADDR_BMS_PARAMETERS_END 	0x2417
#define BMS_PARAMETER_COUNT  			24    //单位，寄存器


#define RS485_ADDR_RW_BMS_PARAMETER  0x2400

#ifdef LIFEPO
#define BMS_SW_COV           (3750)
#define BMS_SW_COV_recover   (3500)
#define BMS_SW_COV_filter     100

#define BMS_SW_CUV           (2500)
#define BMS_SW_CUV_recover     (2800)
#define BMS_SW_CUV_filter     (100)
#else
#define BMS_SW_COV           (4225)
#define BMS_SW_COV_recover   (4150)
#define BMS_SW_COV_filter     100

#define BMS_SW_CUV           (2700)
#define BMS_SW_CUV_recover     (3000)
#define BMS_SW_CUV_filter     (100)
#endif // LIFEPO

#define BMS_SW_OTC           ((50 + 40) * 10)
#define BMS_SW_OTC_recover     ((45 + 40) * 10)

#define BMS_SW_UTC           ((0 + 40) * 10)
#define BMS_SW_UTC_recover     ((3 + 40) * 10)

#define BMS_SW_OTD           ((70 + 40) * 10)
#define BMS_SW_OTD_recover     ((60 + 40) * 10)

#define BMS_SW_UTD         ((-20 + 40) * 10)
#define BMS_SW_UTD_recover     ((-10 + 40) * 10)


#define BMS_SW_OCC1_filter  	(10)
#define BMS_SW_OCC2_filter  	(10)

#define BMS_SW_ODC1_filter  	(100)
#define BMS_SW_ODC2_filter  	(50)


/*curValue*/  /*defaultValue*/ /*maxValue*/ /*minValue*/
#define BMS_PARAMETERS_DEFAULT  {\
	/*单节过压*/			BMS_SW_COV,			BMS_SW_COV,			5000,	1000,\
	/*单节过压恢复*/		BMS_SW_COV_recover,	BMS_SW_COV_recover,	5000,	1000,\
	/*单节过压延时*/		BMS_SW_COV_filter,		BMS_SW_COV_filter,		50000,	1,\
	/*单节低压*/			BMS_SW_CUV,			BMS_SW_CUV,			5000,	1000,\
	/*单节低压恢复*/		BMS_SW_CUV_recover,	BMS_SW_CUV_recover,	5000,	1000,\
	/*单节低压延时*/		BMS_SW_CUV_filter,		BMS_SW_CUV_filter,		50000,	1,\
	/*一级充电过流*/		BMS_SW_OCC1,			BMS_SW_OCC1,			50000,	10,\
	/*一级充电过流延时*/	BMS_SW_OCC1_filter,	BMS_SW_OCC1_filter,	50000,	1,\
	/*二级充电过流*/		BMS_SW_OCC2,			BMS_SW_OCC2,			50000,	10,\
	/*二级充电过流延时*/	BMS_SW_OCC2_filter,	BMS_SW_OCC2_filter,	50000,	1,\
	/*一级放电过流*/		BMS_SW_ODC1,			BMS_SW_ODC1,			50000,	10,\
	/*一级放电过流延时*/    BMS_SW_ODC1_filter,	BMS_SW_ODC1_filter,	50000,	1,\
	/*二级放电过流*/		BMS_SW_ODC2,	        BMS_SW_ODC2,			50000,	10,\
	/*二级放电过流延时*/    BMS_SW_ODC2_filter,	BMS_SW_ODC2_filter,	50000,	1,\
	/*充电高温*/			BMS_SW_OTC,	       BMS_SW_OTC,				2000,	400,\
	/*充电高温恢复*/		BMS_SW_OTC_recover,	BMS_SW_OTC_recover,	50000,	1,\
	/*充电低温*/			BMS_SW_UTC,	       BMS_SW_UTC,				800,	0,\
	/*充电低温恢复*/		BMS_SW_UTC_recover,	BMS_SW_UTC_recover,	50000,	1,\
	/*放电高温*/			BMS_SW_OTD,	       BMS_SW_OTD,				2000,	400,\
	/*放电高温恢复*/		BMS_SW_OTD_recover,	BMS_SW_OTD_recover,	50000,	1,\
	/*放电低温*/			BMS_SW_UTD,	       BMS_SW_UTD,				800,	0,\
	/*放电低温恢复*/		BMS_SW_UTD_recover,	BMS_SW_UTD_recover,	50000,	1,\
	/*短路电流*/			CBC_Cur_DSG,	   CBC_Cur_DSG ,	65000,	0,\
	/*短路延时*/			CBC_DelayT,	      CBC_DelayT,		65000,	0,\
}


/* MCU software-protection parameter schema; hardware image is separate. */
typedef struct {
	UINT16 curValue;			//当前值
	UINT16 defaultValue;		//默认值
	UINT16 maxValue;			//最大值
	UINT16 minValue;			//最小值
}BMS_PARAMETER_VALUE;

typedef struct{
	BMS_PARAMETER_VALUE	u16VcellOvp;  		//单节过压 mv
	BMS_PARAMETER_VALUE	u16VcellOvp_Rcv;	//过压恢复 mv
	BMS_PARAMETER_VALUE	u16VcellOvp_Filter;	//过压延时 10ms

	BMS_PARAMETER_VALUE	u16VcellUvp;		//单节低压
	BMS_PARAMETER_VALUE	u16VcellUvp_Rcv;
	BMS_PARAMETER_VALUE	u16VcellUvp_Filter;

	BMS_PARAMETER_VALUE	u16IchgOcp_First;	//一级充电过流 A*10
	BMS_PARAMETER_VALUE	u16IchgOcp_Filter_First;

	BMS_PARAMETER_VALUE	u16IchgOcp_Second;	//二级充电过流
	BMS_PARAMETER_VALUE	u16IchgOcp_Filter_Second;

	BMS_PARAMETER_VALUE	u16IdsgOcp_First;	//一级放电过流
	BMS_PARAMETER_VALUE	u16IdsgOcp_Filter_First;

	BMS_PARAMETER_VALUE	u16IdsgOcp_Second;	//二级放电过流
	BMS_PARAMETER_VALUE	u16IdsgOcp_Filter_Second;

	BMS_PARAMETER_VALUE	u16TChgOTp;			//充电高温 (℃*10+400)
	BMS_PARAMETER_VALUE	u16TChgOTp_Rcv;
	BMS_PARAMETER_VALUE	u16TchgUTp;			//充电低温
	BMS_PARAMETER_VALUE	u16TchgUTp_Rcv;
	BMS_PARAMETER_VALUE	u16TdischgOTp;		//放电高温
	BMS_PARAMETER_VALUE	u16TdischgOTp_Rcv;
	BMS_PARAMETER_VALUE	u16TdischgUTp;		//放电低温
	BMS_PARAMETER_VALUE	u16TdischgUTp_Rcv;
	BMS_PARAMETER_VALUE 	u16CBC_Cur_DSG;
	BMS_PARAMETER_VALUE 	u16CBC_DelayT;
}BMS_PARAMETERS;


extern BMS_PARAMETERS g_bmsParameters ;


void Sci_ACK_0x03_RW_AFE_Parameters(struct RS485MSG *s,UINT8 t_u8BuffTemp[]);
UINT8 Sci_WrRegs_0x10_AFE_Parameters(UINT16 u16Channel,struct RS485MSG *s);
void Sci_WrReg_0x06_Reset_AFE_Parameters(struct RS485MSG *s);

UINT8 EEPROM_ResetData_AFE_ParametersToDefault(void);
void ReadEEPROM_AFE_Parameters(void);



#endif
