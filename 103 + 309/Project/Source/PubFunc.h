#ifndef PUBFUNC_H
#define PUBFUNC_H


UINT16 Sci_CRC16RTU( UINT8 * pszBuf, UINT8 unLength);
UINT16 GetEndValue(const UINT16 * ptbl,UINT16 tblsize,UINT16 dat);
void Delay1ms(UINT8 delaycnt);
UINT8 Monitor_TempBreak(UINT16* temp_AD);
void jtag_disableAndConfIO(void);

#endif	/* PUBFUNC_H */

