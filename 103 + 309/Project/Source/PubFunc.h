#ifndef PUBFUNC_H
#define PUBFUNC_H

#include "Project_Types.h"
#include "stm32f10x.h"

typedef enum {ODD = 0, EVEN = !ODD} Parity;

typedef	struct{
	UINT16	u16ChkVal;                      // ��ǰ�Ƚ�����С
	UINT16	u16OPValB;                      // �Ƚϴ�ֵ
	UINT16	u16OPValS;                      // �Ƚ�Сֵ
    UINT16   *i16ChkCnt;                     // ָ���ʱ����ַ
    UINT16  u16TimeCntB;                    // ������ֵʱ��
	UINT16  u16TimeCntS;                    // ����Сֵʱ��
    UINT8	u8FlagLogic;                    // ���߼� 1��u8Flag=s_u16ChkVal > s_u16OPValB ? 1 : 0 ���߼� 1��u8Flag=s_u16ChkVal > s_u16OPValB ? 0 : 1
	UINT8	u8FlagBit;                      // �жϽ��
}SPUBOPUPCHK;


UINT16 Sci_CRC16RTU( UINT8 * pszBuf, UINT8 unLength);
UINT8 App_PubOPUPChk(SPUBOPUPCHK *t_sPubOPChk);
UINT16 GetEndValue(const UINT16 * ptbl,UINT16 tblsize,UINT16 dat);
unsigned char CRC8(unsigned char *ptr, unsigned char len,unsigned char key);
UINT32 ModulusSub(UINT32 Data1, UINT32 Data2);
void Delay_Base10us(int n);

void Delay1ms(UINT8 delaycnt);
void MemoryCopy(UINT8 *source, UINT8 *target, UINT8 length);
UINT16 U16_SwapEndian(UINT16 target);
UINT8 Monitor_TempBreak(UINT16* temp_AD);
void jtag_disableAndConfIO(void);

#endif	/* PUBFUNC_H */
