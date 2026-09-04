#ifndef I2C_AFE1_H
#define I2C_AFE1_H

#include "afe3520/Afe3520.h"

/*
 * Compatibility header only. The historical file name remains in the Keil
 * project, but the device behind this interface is SH3673520 over SPI1.
 */
#define AFE_ID                  0x35U
#define E2PROM_ID               0xA0U

/* Legacy names used by generic DataDeal.c are mapped to SH3673520 RAM. */
#define MTP_CONF                AFE3520_REG_SCONF1
#define MTP_BALANCEH            AFE3520_REG_BALANCEH
#define MTP_BALANCEL            AFE3520_REG_BALANCEL
#define MTP_BSTATUS1            AFE3520_REG_BSTATUS1
#define MTP_BSTATUS2            AFE3520_REG_BSTATUS2
#define MTP_BSTATUS3            AFE3520_REG_BSTATUS1
#define MTP_TEMP1               AFE3520_REG_TEMP1H
#define MTP_TEMP2               (AFE3520_REG_TEMP1H + 2U)
#define MTP_TEMP3               (AFE3520_REG_TEMP1H + 4U)
#define MTP_CUR                 AFE3520_REG_CURH
#define MTP_CELL1               AFE3520_REG_CELL1H
#define MTP_ADC2                AFE3520_REG_CADCDH
#define MTP_BFLAG1              AFE3520_REG_FLAG1
#define MTP_BFLAG2              AFE3520_REG_FLAG2
#define MTP_RSTSTAT             AFE3520_REG_FLAG1

/* Old temperature-ROM aliases are intentionally unsupported; 3520 temperature
 * thresholds live in RAM 0x51..0x54 and are rebuilt from the unified parameter image. */
#define MTP_OTC                 AFE3520_REG_OTC
#define MTP_OTCR                AFE3520_REG_OTC
#define MTP_UTC                 AFE3520_REG_UTC
#define MTP_UTCR                AFE3520_REG_UTC
#define MTP_OTD                 AFE3520_REG_OTD
#define MTP_OTDR                AFE3520_REG_OTD
#define MTP_UTD                 AFE3520_REG_UTD
#define MTP_UTDR                AFE3520_REG_UTD
#define MTP_TR                  AFE3520_REG_TEMPIH

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
