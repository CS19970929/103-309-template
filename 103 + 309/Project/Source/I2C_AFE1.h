#ifndef I2C_AFE1_H
#define I2C_AFE1_H

//#define AFE_ID				0x34
#define AFE_ID				0x34

#define E2PROM_ID			0xA0

//Define MTP register addr
#define MTP_OTC             0x11
#define MTP_OTCR            0x12
#define MTP_UTC             0x13
#define MTP_UTCR            0x14
#define MTP_OTD             0x15
#define MTP_OTDR            0x16
#define MTP_UTD             0x17
#define MTP_UTDR            0x18
#define MTP_TR              0x19

#define MTP_CONF			0x40
#define MTP_BALANCEH		0x41
#define MTP_BALANCEL		0x42
#define MTP_BSTATUS1		0x43
#define MTP_BSTATUS2		0x44
#define MTP_BSTATUS3		0x45
#define MTP_TEMP1			0x46
#define MTP_TEMP2			0x48
#define MTP_TEMP3			0x4A
#define MTP_CUR				0x4C
#define MTP_CELL1			0x4E
#define MTP_CELL2			0x50
#define MTP_CELL3			0x52
#define MTP_CELL4			0x54
#define MTP_CELL5			0x56
#define MTP_CELL6			0x58
#define MTP_CELL7			0x5A
#define MTP_CELL8			0x5C
#define MTP_CELL9			0x5E
#define MTP_CELL10			0x60
#define MTP_CELL11			0x62
#define MTP_CELL12			0x64
#define MTP_CELL13			0x66
#define MTP_CELL14			0x68
#define MTP_CELL15			0x6A
#define MTP_CELL16			0x6C
#define MTP_ADC2			0x6E
#define MTP_BFLAG1			0x70
#define MTP_BFLAG2			0x71
#define MTP_RSTSTAT			0x72

#define AFE_FLAG2_CADC_FLG          ((UINT8)0x01u)
#define AFE_FLAG2_VADC_FLG          ((UINT8)0x02u)
#define AFE_FLAG2_CONVERSION_MASK   ((UINT8)(AFE_FLAG2_CADC_FLG | AFE_FLAG2_VADC_FLG))

typedef union __MTP_REG_sconf2 {
    UINT8 all;
    struct _MTP_REG_sconf2 {
		UINT8 CHGMOS     			:1;		//单节过压
		UINT8 DSGMOS     			:1;		//单节低压
		UINT8 PDSGMOS      		:1;		//放电过流1保护状态
		UINT8 PDSG_CTL      		:1;		//放电过流2保护状态
		
		UINT8 PUMP_EN     			:1;		//充电过流保护状态
		UINT8 PD_CTL  				:1;		//短路保护状态
		UINT8 PD_EN  				:1;		//二次过充电保护状态位
		UINT8 LTCLR  				:1;		//看门狗溢出位
     }bits;
}sconf2;

typedef union __MTP_REG_sconf3 {
    UINT8 all;
    struct _MTP_REG_sconf3 {
		UINT8 OWD_TRG     			:1;		//单节过压
		UINT8 OWD_EN     			:1;		//单节低压
		UINT8 CRLD_EN      			:2;		//放电过流1保护状态
		
		UINT8 LD_WK     			:2;		//充电过流保护状态
		UINT8 GRK_WK  				:1;		//短路保护状态
		UINT8 RES  					:1;		//看门狗溢出位
     }bits;
}sconf3;

// typedef union __MTP_REG_sconf4 {
//     UINT8 all;
//     struct _MTP_REG_sconf4 {
// 		UINT8 OWD_TRG     			:1;		//单节过压
// 		UINT8 OWD_EN     			:1;		//单节低压
// 		UINT8 CRLD_EN      			:2;		//放电过流1保护状态
		
// 		UINT8 LD_WK     			:2;		//充电过流保护状态
// 		UINT8 GRK_WK  				:1;		//短路保护状态
// 		UINT8 RES  					:1;		//看门狗溢出位
//      }bits;
// }sconf4;

//todo mos强制开启要不要？？？分口
// typedef union __MTP_REG_sconf3 {
//     UINT8 all;
//     struct _MTP_REG_sconf3 {
// 		UINT8 OWD_TRG     			:1;		//单节过压
// 		UINT8 OWD_EN     			:1;		//单节低压
// 		UINT8 CRLD_EN      			:2;		//放电过流1保护状态
		
// 		UINT8 LD_WK     			:2;		//充电过流保护状态
// 		UINT8 GRK_WK  				:1;		//短路保护状态
// 		UINT8 RES  					:1;		//看门狗溢出位
//      }bits;
// }sconf5;

typedef union __MTP_REG_sconf6 {
    UINT8 all;
    struct _MTP_REG_sconf6 {
		UINT8 ov_en     			:1;		//单节过压
		UINT8 uv_en     			:1;		//单节低压
		UINT8 ocd_en      			:1;		//放电过流1保护状态
		UINT8 sc_en      			:1;		//放电过流1保护状态
		
		UINT8 ts1_en     			:1;		//充电过流保护状态
		UINT8 ts2_en   				:1;		//短路保护状态
		UINT8 ts3_en   				:1;		//短路保护状态
		UINT8 ts4_en   				:1;		//短路保护状态
     }bits;
}sconf6;

typedef union __MTP_REG_flag1 {
    UINT8 all;
    struct _MTP_REG_flag1 {
		UINT8 ov_flg     			:1;		//单节过压
		UINT8 uv_flg     			:1;		//单节过压
		UINT8 ocd1_flg     			:1;		//单节低压
		UINT8 ocd2_flg      			:1;		//放电过流1保护状态

		UINT8 sc_flg      			:1;		//放电过流1保护状态
		UINT8 occ_flg     			:1;		//充电过流保护状态
		UINT8 wk_flg   				:1;		//短路保护状态
		UINT8 rst1_flg   				:1;		//短路保护状态
     }bits;
}reg_flag1;

typedef union __MTP_REG_flag2 {
    UINT8 all;
    struct _MTP_REG_flag2 {
		UINT8 cadc_flg     			:1;		//单节过压
		UINT8 vadc_flg     			:1;		//单节过压
		UINT8 wdt_flg     			:1;		//单节低压
		UINT8 rst2_flg      			:1;		//放电过流1保护状态

		UINT8 utc_flg      			:1;		//放电过流1保护状态
		UINT8 otc_flg     			:1;		//充电过流保护状态
		UINT8 utd_flg   				:1;		//短路保护状态
		UINT8 otd_flg   				:1;		//短路保护状态
     }bits;
}reg_flag2;

typedef union __MTP_REG_bstatus1 {
    UINT8 all;
    struct _MTP_REG_bstatus1 {
		UINT8 CHG_FET     			:1;		//单节过压
		UINT8 DSG_FET     			:1;		//单节过压
		UINT8 PDSG_FET     			:1;		//单节低压
		UINT8 res      				:1;		//放电过流1保护状态

		UINT8 HCHG_FET      			:1;		//放电过流1保护状态
		UINT8 HDSG_FET     			:1;		//充电过流保护状态
		UINT8 E2P_ERR   				:1;		//短路保护状态
		UINT8 res2   				:1;		//短路保护状态
     }bits;
}reg_bstatus1;

typedef union __MTP_REG_bstatus2 {
    UINT8 all;
    struct _MTP_REG_bstatus2 {
		UINT8 LOADOFF     			:1;		//单节过压
		UINT8 LOADON     			:1;		//单节过压
		UINT8 RES     			:1;		//单节低压
		UINT8 BAL      				:1;		//放电过流1保护状态

		UINT8 IDLE      			:1;		//放电过流1保护状态
		UINT8 SLEEP     			:1;		//充电过流保护状态
		UINT8 DSGING   				:1;		//短路保护状态
		UINT8 CHGING   				:1;		//短路保护状态
     }bits;
}reg_bstatus2;






typedef struct _AFEDATA_{
	uint8_t sonf1;
	sconf2 sonf2;
	sconf3 sonf3;
	uint8_t sonf4;
	uint8_t sonf5;
	uint8_t sonf6;
	uint8_t sonf7;
	uint8_t OWV_ALARMH;
	uint8_t ALARML;
	uint8_t OVT_OVH;
	uint8_t OVL;
	uint8_t UVT_UVH;
	uint8_t UVL;
	uint8_t OCD1V_OCD1T;
	uint8_t OCD2V_OCD2T;
	uint8_t SCV_SCT;
	uint8_t OCCV_OCCT;
	uint8_t OTC;
	uint8_t OTD;
	uint8_t UTC;
	uint8_t UTD;
	uint8_t BALANCEH;
	uint8_t BALANCEM;
	uint8_t BALANCEL;
	// uint8_t res[17];

	reg_flag1 flag1;
	reg_flag2 flag2;
	uint8_t flag3;
	reg_bstatus1 bstatus1;
	reg_bstatus2 bstatus2;

	UINT16 Temp1;		//����֮��V*100
	UINT16 Temp2;
	UINT16 Temp3;
	UINT16 Temp4;
	UINT16 TempI;

	INT16 Cur1;			//ʵʱ����ֵ����Vadc
	UINT16 Cell[20];
	INT16 Cadc;			//����׼�Ŀ��ؼƣ������
	uint16_t VTOP;			//����׼�Ŀ��ؼƣ������
	uint16_t VCHGR;			//����׼�Ŀ��ؼƣ������
	uint8_t owdh;			//����׼�Ŀ��ؼƣ������
	uint8_t odwm;			//����׼�Ŀ��ؼƣ������
	uint8_t owdl;			//����׼�Ŀ��ؼƣ������
}AFEDATA;


struct SH367309_Read {			/* AD Read	*/
	UINT16		u16VCell[20];   // mv
	UINT16		u16TempBat[5];					
	UINT32		vbatB;       	// mv
	UINT32		vbatC;       	// mv
	UINT16      u16Current;     // mA
};


#define _TWI_COM

#if 1
#define TWI_CLK_OUT		{GPIOB->CRH&=0xFFFFFFF0;GPIOB->CRH|=(UINT32)3<<(0<<2);}
#define TWI_CLK_IN      {GPIOB->CRH&=0xFFFFFFF0;GPIOB->CRH|=(UINT32)8<<(0<<2);}
#define TWI_CLK_HIGH	(PBout(8) = 1)
#define TWI_CLK_LOW	    (PBout(8) = 0)

#define TWI_DAT_OUT		{GPIOB->CRH&=0xFFFFFF0F;GPIOB->CRH|=(UINT32)3<<(1<<2);}
#define TWI_DAT_IN      {GPIOB->CRH&=0xFFFFFF0F;GPIOB->CRH|=(UINT32)8<<(1<<2);}
#define TWI_DAT_HIGH	(PBout(9) = 1)
#define TWI_DAT_LOW	    (PBout(9) = 0)

//#define TWI_RD_CLK		(uint16_t)(GPIOB->IDR&GPIO_Pin_8)  //����SDA 
//#define TWI_RD_DAT		(uint16_t)(GPIOB->IDR&GPIO_Pin_9)  //����SDA 
//#define TWI_RD_CLK		(PBin(8))  //����SDA
//#define TWI_RD_DAT		(PBin(9))  //����SDA
#define TWI_RD_CLK		(PBin(8))  //����SDA
#define TWI_RD_DAT		(PBin(9))  //����SDA
#endif

//����������Ǹ���ѡһ
#if 0
#define TWI_CLK_OUT		F_TWI_CLK_OUT()
#define TWI_CLK_IN      F_TWI_CLK_IN()
#define TWI_CLK_HIGH	F_TWI_CLK_HIGH()
#define TWI_CLK_LOW	    F_TWI_CLK_LOW()

#define TWI_DAT_OUT		F_TWI_DAT_OUT()
#define TWI_DAT_IN      F_TWI_DAT_IN()
#define TWI_DAT_HIGH	F_TWI_DAT_HIGH()
#define TWI_DAT_LOW	    F_TWI_DAT_LOW()

#define TWI_RD_CLK		F_TWI_RD_CLK()
#define TWI_RD_DAT		F_TWI_RD_DAT()
#endif


extern struct SH367309_Read SH367309_Read_AFE1;
extern AFEDATA Registers_AFE1;
extern UINT8 AFE1_LastFlag2ConversionFlags;

#define AFE_UPDATE_OK              ((UINT8)0x00)
#define AFE_UPDATE_ERR_SCONF       ((UINT8)0x01)
#define AFE_UPDATE_ERR_THRESHOLD   ((UINT8)0x02)
#define AFE_UPDATE_ERR_STATUS      ((UINT8)0x04)
#define AFE_UPDATE_ERR_ADC         ((UINT8)0x08)


UINT8 MTPWrite(UINT8 WrAddr, UINT8 Length, UINT8 *WrBuf);
UINT8 MTPRead(UINT8 RdAddr, UINT8 Length, UINT8 *RdBuf);
UINT8 MTPWriteROM(UINT8 WrAddr, UINT8 Length, UINT8 *WrBuf);
void InitAFE1_Sleep(UINT8 mode);
void InitAFE1(void);
UINT8 UpdateVoltageFromBqMaximo(void);

void initAFE1_IIC(void);

#endif	/* I2C_AFE1_H */

