#ifndef PUBFUNC_H
#define PUBFUNC_H


UINT16 Sci_CRC16RTU( UINT8 * pszBuf, UINT8 unLength);
void Delay1ms(UINT8 delaycnt);
UINT8 Monitor_TempBreak(UINT16* temp_AD);
void jtag_disableAndConfIO(void);

#endif	/* PUBFUNC_H */

