#ifndef AFE_DRIVER_H
#define AFE_DRIVER_H

//#define AFE_ID              0x34
#define AFE_ID              0x34

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

typedef struct _AFEDATA_{
	UINT16 Temp1;		//����֮��V*100
	UINT16 Temp2;
	UINT16 Temp3;
	INT16 Cur1;			//ʵʱ����ֵ����Vadc
	UINT16 Cell[16];
	INT16 Cadc;			//����׼�Ŀ��ؼƣ������
}AFEDATA;


struct SH367309_Read {			/* AD Read	*/
	UINT16		u16VCell[16];   // mv
	UINT16		u16TempBat[3];					
	UINT32		u32VBat;       	// mv
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


UINT8 MTPWrite(UINT8 WrAddr, UINT8 Length, UINT8 *WrBuf);
UINT8 MTPRead(UINT8 RdAddr, UINT8 Length, UINT8 *RdBuf);
UINT8 MTPWriteROM(UINT8 WrAddr, UINT8 Length, UINT8 *WrBuf);
void InitAFE1_Sleep(UINT8 mode);
void InitAFE1(void);
UINT8 UpdateVoltageFromBqMaximo(void);

void initAFE1_IIC(void);

/* ── SH367309_Func.h section ── */
#include "conf.h"

#ifdef TERNARYLI
#define VAL_CELL_OVP			((UINT16)4205)	//��λmV	
#define VAL_CELL_OVP_REC		((UINT16)4010)	//��λmV�����䱣���ָ�
#define VAL_CELL_UVP			((UINT16)2800)	//��λmV	
#define VAL_CELL_UVP_REC		((UINT16)2900)	//��λmV����ѹ�����ָ�
#define VAL_BAL_OPEN			((UINT16)5000)	//��λmV�����⿪����ѹ

#elif (defined(LIFEPO))
#define VAL_CELL_OVP			((UINT16)3750)	//��λmV	
#define VAL_CELL_OVP_REC		((UINT16)3650)	//��λmV�����䱣���ָ�
#define VAL_CELL_UVP			((UINT16)2500)	//��λmV	
#define VAL_CELL_UVP_REC		((UINT16)2600)	//��λmV����ѹ�����ָ�
#define VAL_BAL_OPEN			((UINT16)5000)	//��λmV�����⿪����ѹ

#endif

/*
���϶�
�����±���------->70
�����±����ָ�--->65
�����±���------->-5
�����±����ָ�--->0
�ŵ���±���------->70
�ŵ���±����ָ�--->65
�ŵ���±���------->-5
�ŵ���±����ָ�--->0
//��Ϊû��TR��ֵ�������Ȼ���Ϊ(X + 40)
//Ҫ����±�������һ��ҪС��25���϶ȣ���Ȼ����������.
//����ֻ��8λ�洢���ݣ���-256�������������25���϶ȣ��������Ḻ��(Rt/(Rs+Rr)-0.5)
*/
#define VAL_CHG_OTP				((UINT8)110)
#define VAL_CHG_OTP_RCV			((UINT8)105)
#define VAL_CHG_UTP				((UINT8)35)
#define VAL_CHG_UTP_RCV			((UINT8)40)
#define VAL_DSG_OTP				((UINT8)110)
#define VAL_DSG_OTP_RCV			((UINT8)105)
#define VAL_DSG_UTP				((UINT8)35)
#define VAL_DSG_UTP_RCV			((UINT8)40)


#define BIT_ENPCH	(0<<7)		//0:��ֹԤ��繦�ܣ�1������Ԥ�书��
#define BIT_ENMOS	(1<<6)		//0:��ֹ���MOS�ָ���1:�������MOS�ָ�����λ��
								//�������/�¶ȱ���(�¶�ʵ�ʹ�2��)�رճ��MOS�������⵽�ŵ�״̬���������MOS
								//������ɣ�MOSӦ�û�û��ô�ȡ�
#define BIT_OCPM	(0<<5)		//0:������ֻ�رճ��MOS���ŵ����ֻ�طŵ�MOS��1��ͬʱ��
#define BIT_BAL		(1<<4)		//0��ƽ�⹦�����ڲ�SH367309���ƣ�1:���ⲿMCU��
#define BIT0_3_CN	(0)			//5-15����Ӧ���������Ϊ16��
#define BYTE_00H_SCONF1			BIT_ENPCH|BIT_ENMOS|BIT_OCPM|BIT_BAL|BIT0_3_CN

#define BIT_E0VB	(0<<7)		//0���رա�1������---����ֹ��ѹ��о��硱����
#define BIT_UV_OP	(0<<5)		//0������ֻ�رշŵ�MOS��1�����Źرճ�ŵ�MOS
#define BIT_DIS_PF	(1<<4)		//0��������1����ֹ---���ι���籣����ͬʱҲ�Ƕ��߼�⹦��(����ˣ���ô����һ����)
								//�����־λΪ0��������������⣬���¶ϵ��ϵ����׳���ȫ����ѹΪ10000mV���ң����Խ�ֹ��(������PF���Ż����VSS��ƽ)
#define BIT2_3_CTLC	(0<<2)		//MOS���ڲ��߼����ƣ�CTL������Ч�����忴�����
								//2������Ϊ���Ʒŵ�MOS��Ԥ�Ź���
#define BIT_OCRA	(0<<1)		//0����������1������---������������ʱ�ָ������ܣ�Ҳ����ζ��ֻ�ܸ����ͷŲ��ܽ�����������������Զ��ָ�
#define BIT_EUVR	(0)			//0�����ű���״̬�ͷ��븺���ͷ��޹أ���ζ�Ÿ����ͷ�ֻ�͵��������й���
								//���ű��������븺���ͷŲ��ܽ����
								//�ȸ�Ϊ0���ԣ�����Ļ�1��������س��ܣ�P+��Զ��⵽һ���ߵ�ƽ�������Ƴ�û����⵽
								//�Ļ�0������û����
#define BYTE_01H_SCONF2			BIT_E0VB|BIT_UV_OP|BIT_DIS_PF|BIT2_3_CTLC|BIT_OCRA|BIT_EUVR


//���µ�ѹ�Ĵ�СҪ��
//PFV->OV->OVR->UVR->UV->Vpd->PREV->L0V
#define BIT4_7_OVT				(6<<4)		//0110������籣����ʱ1s
#define BIT2_3_LDRT				(11<<2)		//11�������ͷ���ʱ2000ms������Ͷ�·�����ͷ���ʱ�й�ϵ
#define BIT0_1_OVH				(UINT8)((VAL_CELL_OVP/5)>>8)			//���䱣��ǰ2λ
#define BYTE_02H_OVT_LDRT_OVH	BIT4_7_OVT|BIT2_3_LDRT|BIT0_1_OVH

#define BIT0_7_OVL				(UINT8)((VAL_CELL_OVP/5)&0x00FF)		//���䱣����8λ
#define BYTE_03H_OVL			BIT0_7_OVL

#define BIT4_7_UVT				(6<<4)									//0110�����ű�����ʱ1s
#define BIT0_1_OVR				(UINT8)((VAL_CELL_OVP_REC/5)>>8)		//���䱣���ָ�ǰ2λ
#define BYTE_04H_UVT_OVRH		BIT4_7_UVT|BIT0_1_OVR

#define BIT0_7_OVR				(UINT8)((VAL_CELL_OVP_REC/5)&0x00FF)	//���䱣���ָ���8λ
#define BYTE_05H_OVRL			BIT0_7_OVR

#define BIT0_7_UV				(UINT8)((VAL_CELL_UVP/20)&0x00FF)		//��ѹ����
#define BYTE_06H_UV				BIT0_7_UV

#define BIT0_7_UVR				(UINT8)((VAL_CELL_UVP_REC/20)&0x00FF)	//��ѹ�����ָ�
#define BYTE_07H_UVR			BIT0_7_UVR

#define BIT0_7_BALV 			(UINT8)((VAL_BAL_OPEN/20)&0x00FF)		//���⿪����ѹ
#define BYTE_08H_BALV			BIT0_7_BALV

#define BIT0_7_PREV				(UINT8)((2000/20)&0x00FF)				//Ԥ���ѹ
#define BYTE_09H_PREV			BIT0_7_PREV

#define BIT0_7_L0V				(UINT8)(1000/20)				//�͵�ѹ��ֹ����ѹ
#define BYTE_0AH_L0V			BIT0_7_L0V

#define BIT0_7_PFV				(UINT8)(5000/20)				//���ι���籣����ѹ���üĴ������Ŵ�һЩ���������
#define BYTE_0BH_PFV			BIT0_7_PFV



#define BIT4_7_OCD1V			(0<<4)		//1000���ŵ��������1������ѹ=100mV��0000Ϊ20mV
#define BIT0_3_OCD1T			(6)			//0110���ŵ����1������ʱ1s
#define BYTE_0CH_OCD1V_OCD1T	BIT4_7_OCD1V|BIT0_3_OCD1T

#define BIT4_7_OCD2V			(15<<4)		//1000���ŵ��������1������ѹ=120mV��0000Ϊ30mV��Ū�����
#define BIT0_3_OCD2T			(6)			//0110���ŵ����1������ʱ200ms
#define BYTE_0DH_OCD2V_OCD2T	BIT4_7_OCD2V|BIT0_3_OCD2T

#define BIT4_7_SCV				(0<<4)		//1000����·������ѹ=290mV,0010Ϊ110mV
#define BIT0_3_SCT				(1)			//0001����·������ʱ64us
#define BYTE_0EH_SCV_SCT		BIT4_7_SCV|BIT0_3_SCT

#define BIT4_7_OCCV				(0<<4)		//1000��������������ѹ=100mV,0000Ϊ20mV
#define BIT0_3_OCCT				(10)		//1010��������������ʱ1s
#define BYTE_0FH_OCCV_OCCT		BIT4_7_OCCV|BIT0_3_OCCT

#define BIT6_7_CHS				(0<<6)		//��ŵ�״̬����ѹ=200uV������BSTATUS3��CHGING��DSGINGλ����λ�Ľ���
#define BIT4_5_MOST				(0<<4)		//��ŵ�MOSFET������ʱ=64us
#define BIT2_3_OCRT				(2<<2)		//��ŵ�����Զ��ظ���ʱ=32s
#define BIT0_1_PFT				(0)			//���ι��䱣����ʱ=9s
#define BYTE_10H_MOST_OCRT_PFT	BIT6_7_CHS|BIT4_5_MOST|BIT2_3_OCRT|BIT0_1_PFT


/*
���϶�
�����±���------->
�����±����ָ�--->
�����±���------->
�����±����ָ�--->
�ŵ���±���------->
�ŵ���±����ָ�--->
�ŵ���±���------->
�ŵ���±����ָ�--->
//��Ϊû��TR��ֵ�������Ȼ���Ϊ(X + 40)
*/
#define BYTE_11H_OTC			VAL_CHG_OTP
#define BYTE_12H_OTCR			VAL_CHG_OTP_RCV
#define BYTE_13H_UTC			VAL_CHG_UTP
#define BYTE_14H_UTCR			VAL_CHG_UTP_RCV
#define BYTE_15H_OTD			VAL_DSG_OTP
#define BYTE_16H_OTDR			VAL_DSG_OTP_RCV
#define BYTE_17H_UTD			VAL_DSG_UTP
#define BYTE_18H_UTDR			VAL_DSG_UTP_RCV

#define BYTE_19H_TR				10			//�ڲ�Ĭ��Ӧ����10k����ʵ���Ƕ��ٶ�������ɣ������ֵ��Ч

typedef union __MTP_REG_BSTATUS1 {
    UINT8 all;
    struct _MTP_REG_BSTATUS1 {
		UINT8 OV     			:1;		//���ڹ�ѹ
		UINT8 UV     			:1;		//���ڵ�ѹ
		UINT8 OCD1      		:1;		//�ŵ����1����״̬
		UINT8 OCD2      		:1;		//�ŵ����2����״̬
		
		UINT8 OCC     			:1;		//����������״̬
		UINT8 SC  				:1;		//��·����״̬
		UINT8 PF  				:1;		//���ι���籣��״̬λ
		UINT8 WDT  				:1;		//���Ź����λ
     }bits;
}MTP_REG_BSTATUS1;


typedef union __MTP_REG_BSTATUS2 {
    UINT8 all;
    struct _MTP_REG_BSTATUS2 {
		UINT8 UTC  				:1;		//�����±���״̬λ
		UINT8 OTC  				:1;		//�����±���״̬λ
		UINT8 UTD      			:1;		//�ŵ���±���״̬λ
		UINT8 OTD   			:1;		//�ŵ���±���״̬λ
		
		UINT8 Rcv				:4;		//����λ
		//UINT8 Rcv2				:8;		//����λ
     }bits;
}MTP_REG_BSTATUS2;


typedef union __MTP_REG_BSTATUS3 {
    UINT8 all;
    struct _MTP_REG_BSTATUS3 {
		UINT8 DSG_FET     		:1;		//�ŵ��״̬
		UINT8 CHG_FET     		:1;		//����״̬
		UINT8 PCHG_FET      	:1;		//Ԥ���״̬
		UINT8 L0V      			:1;		//�͵�ѹ��ֹ���״̬λ
		
		UINT8 EEPR_WR     		:1;		//EEPROMд����״̬λ
		UINT8 RCV  				:1;		//����λ
		UINT8 DSGING  			:1;		//�ŵ�״̬
		UINT8 CHGING  			:1;		//���״̬
     }bits;
}MTP_REG_BSTATUS3;


typedef union __MTP_REG_CONF {
    UINT8 all;
    struct _MTP_REG_CONF {
		UINT8 IDLE     		:1;		//�ŵ��״̬
		UINT8 SLEEP     	:1;		//����״̬
		UINT8 ENWDT      	:1;		//Ԥ���״̬
		UINT8 CADCON      	:1;		//�͵�ѹ��ֹ���״̬λ
		
		UINT8 CHGMOS     	:1;		//EEPROMд����״̬λ
		UINT8 DSGMOS  		:1;		//����λ
		UINT8 PCHMOS  		:1;		//�ŵ�״̬
		UINT8 OCRC  		:1;		//���״̬
     }bits;
}MTP_REG_CONF;


typedef struct _AFE_REG_STORE {
	UINT8 u8_MTP_SCONF2;				//01H
	UINT8 u8_MTP_SCV_SCT;				//0EH
	
	MTP_REG_CONF REG_MTP_CONF;			//40H
	UINT8 u8_MTP_BALANCEH;				//41H
	UINT8 u8_MTP_BALANCEL;				//42H
	MTP_REG_BSTATUS1 REG_BSTATUS1;		//43H
	MTP_REG_BSTATUS2 REG_BSTATUS2;		//44H
	MTP_REG_BSTATUS3 REG_BSTATUS3;		//45H
	UINT8 TR;					//71H
	UINT16 TR_ResRef;			//680 + 5*TR����λ��k��*100
}SH367309_REG_STORE;


typedef enum _FAULT_AFE_TO_MCU {
	CellOvp,
	CellUvp,
	IdischgOcp1,
	IdischgOcp2,
	IchgOcp,
	CellChgUTp,
	CellChgOTp,
	CellDsgUTp,
	CellDsgOTp,
}FAULT_AFE_TO_MCU;


extern SH367309_REG_STORE SH367309_Reg_Store;



UINT8 AFE_CheckStatus(void);
UINT8 AFE_IsReady(void);
void AFE_Reset(void);

void AFE_Sleep(void);
void AFE_IDLE(void);
void AFE_SHIP(void);
UINT32 AFE_CalcuVbat(void);

UINT8 SH367309_SC_DelayT_Set(void);
void SH367309_DriverMos_Ctrl(GPIO_Type Type, UINT8 OnOFF);
bool SH367309_UpdataAfeConfig(void);
void SH367309_Enable_AFE_Wdt_Cadc_Drivers(void);


void App_SH367309(void);

#endif  /* AFE_DRIVER_H */

