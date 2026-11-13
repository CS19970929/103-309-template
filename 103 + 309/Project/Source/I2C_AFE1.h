#ifndef I2C_AFE1_H
#define I2C_AFE1_H
#define AFE_ID				0x34


//Define MTP register addr

#define MTP_CONF			0x40
#define MTP_BALANCEH		0x41
#define MTP_BSTATUS3		0x45
#define MTP_TEMP1			0x46
#define MTP_ADC2			0x6E
#define MTP_BFLAG2			0x71

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



#define TWI_CLK_OUT		{GPIOB->CRH&=0xFFFFFFF0;GPIOB->CRH|=(UINT32)3<<(0<<2);}
#define TWI_CLK_IN      {GPIOB->CRH&=0xFFFFFFF0;GPIOB->CRH|=(UINT32)8<<(0<<2);}
#define TWI_CLK_HIGH	(PBout(8) = 1)
#define TWI_CLK_LOW	    (PBout(8) = 0)

#define TWI_DAT_OUT		{GPIOB->CRH&=0xFFFFFF0F;GPIOB->CRH|=(UINT32)3<<(1<<2);}
#define TWI_DAT_IN      {GPIOB->CRH&=0xFFFFFF0F;GPIOB->CRH|=(UINT32)8<<(1<<2);}
#define TWI_DAT_HIGH	(PBout(9) = 1)
#define TWI_DAT_LOW	    (PBout(9) = 0)
#define TWI_RD_CLK		(PBin(8))  //����SDA
#define TWI_RD_DAT		(PBin(9))  //����SDA

extern struct SH367309_Read SH367309_Read_AFE1;
extern AFEDATA Registers_AFE1;


UINT8 MTPWrite(UINT8 WrAddr, UINT8 Length, UINT8 *WrBuf);
UINT8 MTPRead(UINT8 RdAddr, UINT8 Length, UINT8 *RdBuf);
UINT8 MTPWriteROM(UINT8 WrAddr, UINT8 Length, UINT8 *WrBuf);
void InitAFE1_Sleep(UINT8 mode);
void InitAFE1(void);
UINT8 UpdateVoltageFromBqMaximo(void);

void initAFE1_IIC(void);

#endif	/* I2C_AFE1_H */

