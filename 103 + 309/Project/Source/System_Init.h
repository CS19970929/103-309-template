#ifndef SYSTEM_INIT_H
#define SYSTEM_INIT_H

#define BITBAND(addr, bitnum) ((addr & 0xF0000000)+0x2000000+((addr &0xFFFFF)<<5)+(bitnum<<2)) 
#define MEM_ADDR(addr)  *((volatile unsigned long  *)(addr)) 
#define BIT_ADDR(addr, bitnum)   MEM_ADDR(BITBAND(addr, bitnum)) 
//IO�ڵ�ַӳ��
#define GPIOB_ODR_Addr    (GPIOB_BASE+12) //0x40010C0C

#define GPIOA_IDR_Addr    (GPIOA_BASE+8)
#define GPIOB_IDR_Addr    (GPIOB_BASE+8) //0x40010C08

//IO�ڲ���,ֻ�Ե�һ��IO��!
//ȷ��n��ֵС��16!
#define PAin(n)    BIT_ADDR(GPIOA_IDR_Addr,n)  //����

#define PBout(n)   BIT_ADDR(GPIOB_ODR_Addr,n)  //���
#define PBin(n)    BIT_ADDR(GPIOB_IDR_Addr,n)  //����

#define MCUI_ENI_DI1		PAin(9)		//I��1


union SYS_TIME {
	UINT16 all;
	struct StatusSysTimeFlagBit {
		UINT16 b1Sys10msFlag   : 1;
		UINT16 b1Sys1000msFlag : 1;
		UINT16 reserved        : 14;
	} bits;
};


void IWDG_Feed(void);
#define Feed_IWatchDog IWDG_Feed()

extern volatile union SYS_TIME g_st_SysTimeFlag;


void InitDelay(void);
void __delay_ms(UINT16 nms);
void __delay_us(UINT32 nus);
void InitTimer(void);
void InitNVIC(void);
void Init_IWDG(void);
void ClearLowPowerDebug(void);
void SysTime_LatchTaskFlags(void);
UINT32 SysTime_Get10msTickCount(void);
UINT32 SysTime_GetMicroseconds(void);
UINT8 SysTime_Take200msTaskPeriod(void);


#endif	/* SYSTEM_INIT_H */
