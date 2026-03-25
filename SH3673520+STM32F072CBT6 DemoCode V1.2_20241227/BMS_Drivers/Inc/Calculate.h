#ifndef __CALCULATE_H
#define __CALCULATE_H

#include "Common.h"

//定义校准
#define CALIPACKVOL1	5000
#define CALIPACKVOL2	50000
#define CALICUR			10000

extern U16 CalcuTemp(U16 getdata);

extern void SH_AFE_GetStatus(BOOL HighSide);
extern void SH_AFE_GetCellVol(U8 Num, S16 *VCell, U32 *VolTotal);
extern void SH_AFE_GetTotalVol(U32 *VolTotal);
extern void SH_AFE_GetChgVol(U32 *VolChg);
extern void SH_AFE_GetTempe(U8 Num, U16 *Temp);
extern void SH_AFE_GetTempI(U16 *Temp);
extern void SH_AFE_GetVadcCurr(S32 *Curr);
extern void SH_AFE_GetCadcCurr(S32 *Curr);
extern void SH_AFE_CetMaxMin(U8 Num, U16 *Array, U16 *Min, U16 *Max);

extern void SH_AFE_GetBattInfo(U8 CellNum, U8 TsNum, BOOL HighSide);

#endif




