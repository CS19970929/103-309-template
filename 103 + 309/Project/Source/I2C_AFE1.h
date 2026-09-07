#ifndef I2C_AFE1_H
#define I2C_AFE1_H

#include "afe3520/Afe3520.h"

/*
 * Compatibility header only. The historical file name remains in the Keil
 * project, but the device behind this interface is SH3673520 over SPI1.
 */
#define AFE_ID                  0x35U
#define E2PROM_ID               0xA0U

/* DataDeal current adapter is the only remaining register alias. */
#define MTP_ADC2 AFE3520_REG_CADCDH

typedef struct _AFEDATA_
{
    UINT16 Temp1;
    UINT16 Temp2;
    UINT16 Temp3;
    UINT16 Temp4;
    INT16 Cur1;
    UINT16 Cell[AFE3520_CELL_MAX];
    INT16 Cadc;
} AFEDATA;

struct SH367309_Read
{
    UINT16 u16VCell[AFE3520_CELL_MAX];
    UINT16 u16TempBat[AFE3520_TEMP_MAX];
    UINT32 u32VBat;
    UINT16 u16Current; /* compatibility CADC proxy code, not native SH3673520 code */
};

extern struct SH367309_Read SH367309_Read_AFE1;
extern AFEDATA Registers_AFE1;

UINT8 MTPWrite(UINT8 WrAddr, UINT8 Length, UINT8 *WrBuf);
UINT8 MTPRead(UINT8 RdAddr, UINT8 Length, UINT8 *RdBuf);
UINT8 MTPWriteROM(UINT8 WrAddr, UINT8 Length, UINT8 *WrBuf);
void InitAFE1_Sleep(UINT8 mode);
void InitAFE1(void);
UINT8 UpdateVoltageFromBqMaximo(void);
void initAFE1_IIC(void); /* compatibility name: initializes SPI1 */

#endif /* I2C_AFE1_H */
