#ifndef UTILS_H
#define UTILS_H

#include "main.h"

/* ── Lookup table interpolation ── */
UINT16 GetEndValue(const UINT16 *ptbl, UINT16 tblsize, UINT16 dat);

/* ── Hysteresis comparator ── */
typedef struct
{
    UINT8 u8FlagLogic;   /* 0/1 — over/under direction */
    UINT8 u8FlagBit;     /* current output state */
    UINT16 u16ChkVal;    /* value to check */
    UINT16 u16OPValB;    /* upper threshold */
    UINT16 u16OPValS;    /* lower threshold */
    UINT16 u16TimeCntB;  /* filter count — upper */
    UINT16 u16TimeCntS;  /* filter count — lower */
    UINT16 *i16ChkCnt;   /* pointer to counter variable */
} SPUBOPUPCHK;

UINT8 App_PubOPUPChk(SPUBOPUPCHK *t_sPubOPChk);

/* ── CRC ── */
UINT16 Sci_CRC16RTU(UINT8 *pszBuf, UINT8 unLength);
unsigned char CRC8(unsigned char *ptr, unsigned char len, unsigned char key);

/* ── Math ── */
UINT32 ModulusSub(UINT32 Data1, UINT32 Data2);

/* ── Byte Swap ── */
UINT16 U16_SwapEndian(UINT16 target);
UINT16 *U16_SwapEndian_Adress(UINT16 *target);

/* ── Memory ── */
void MemoryCopy(UINT8 *source, UINT8 *target, UINT8 length);

/* ── Delay (busy-wait) ── */
void Delay_Base10us(int n);
void Delay1ms(UINT8 delaycnt);

/* ── Temperature break detection ── */
UINT8 Monitor_TempBreak(UINT16 *temp_AD);

/* ── JTAG disable ── */
void jtag_disableAndConfIO(void);

/* ── Short circuit parameter init ── */
void InitShortCur(void);

#endif /* UTILS_H */
