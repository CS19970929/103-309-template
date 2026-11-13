#ifndef I2C_AFE1_H
#define I2C_AFE1_H

#include "Board_Config.h"
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



/* Software I2C uses CRH directly. Keep its fast, compile-time pin selection. */
#if (BOARD_AFE_I2C_SCL_SOURCE < 8U || BOARD_AFE_I2C_SCL_SOURCE > 15U || \
     BOARD_AFE_I2C_SDA_SOURCE < 8U || BOARD_AFE_I2C_SDA_SOURCE > 15U || \
     BOARD_AFE_I2C_SCL_SOURCE == BOARD_AFE_I2C_SDA_SOURCE)
#error "AFE software I2C requires two different CRH pins (8..15)"
#endif
#define TWI_CLK_SHIFT ((BOARD_AFE_I2C_SCL_SOURCE - 8U) * 4U)
#define TWI_DAT_SHIFT ((BOARD_AFE_I2C_SDA_SOURCE - 8U) * 4U)
#define TWI_CLK_OUT {BOARD_AFE_I2C_GPIO->CRH &= ~(0x0FUL << TWI_CLK_SHIFT); BOARD_AFE_I2C_GPIO->CRH |= 3UL << TWI_CLK_SHIFT;}
#define TWI_CLK_IN  {BOARD_AFE_I2C_GPIO->CRH &= ~(0x0FUL << TWI_CLK_SHIFT); BOARD_AFE_I2C_GPIO->CRH |= 8UL << TWI_CLK_SHIFT;}
#define TWI_DAT_OUT {BOARD_AFE_I2C_GPIO->CRH &= ~(0x0FUL << TWI_DAT_SHIFT); BOARD_AFE_I2C_GPIO->CRH |= 3UL << TWI_DAT_SHIFT;}
#define TWI_DAT_IN  {BOARD_AFE_I2C_GPIO->CRH &= ~(0x0FUL << TWI_DAT_SHIFT); BOARD_AFE_I2C_GPIO->CRH |= 8UL << TWI_DAT_SHIFT;}
#define TWI_CLK_HIGH (BIT_ADDR((uint32_t)&BOARD_AFE_I2C_GPIO->ODR, BOARD_AFE_I2C_SCL_SOURCE) = 1)
#define TWI_CLK_LOW  (BIT_ADDR((uint32_t)&BOARD_AFE_I2C_GPIO->ODR, BOARD_AFE_I2C_SCL_SOURCE) = 0)
#define TWI_DAT_HIGH (BIT_ADDR((uint32_t)&BOARD_AFE_I2C_GPIO->ODR, BOARD_AFE_I2C_SDA_SOURCE) = 1)
#define TWI_DAT_LOW  (BIT_ADDR((uint32_t)&BOARD_AFE_I2C_GPIO->ODR, BOARD_AFE_I2C_SDA_SOURCE) = 0)
#define TWI_RD_CLK   BIT_ADDR((uint32_t)&BOARD_AFE_I2C_GPIO->IDR, BOARD_AFE_I2C_SCL_SOURCE)
#define TWI_RD_DAT   BIT_ADDR((uint32_t)&BOARD_AFE_I2C_GPIO->IDR, BOARD_AFE_I2C_SDA_SOURCE)


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

