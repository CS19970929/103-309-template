#ifndef SH367309_FUNC_H
#define SH367309_FUNC_H

#include "conf.h"

/* Legacy fault view only. Hardware settings: afe3520/Afe3520Config.h. */
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

typedef struct {
    MTP_REG_BSTATUS1 REG_BSTATUS1; /* Published fault view, not physical registers. */
    MTP_REG_BSTATUS2 REG_BSTATUS2;
} SH367309_REG_STORE;

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

#endif	/* SH367309_FUNC_H */

