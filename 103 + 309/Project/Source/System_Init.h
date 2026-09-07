#ifndef SYSTEM_INIT_H
#define SYSTEM_INIT_H

union SYS_TIME {
	UINT16 all;
	struct StatusSysTimeFlagBit {
		UINT16 b1Sys10msFlag   : 1;
		UINT16 b1Sys50msFlag   : 1;
		UINT16 b1Sys100msFlag  : 1;
		UINT16 b1Sys200msFlag  : 1;
		UINT16 b1Sys1000msFlag : 1;
		UINT16 reserved        : 11;
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
void EnableLowPowerDebug(void);
void SysTime_LatchTaskFlags(void);
UINT8 SysTime_HasPendingTaskFlags(void);
UINT32 SysTime_Get10msTickCount(void);
UINT8 SysTime_Take200msTaskPeriod(void);
UINT16 SysTime_Get200msTaskOverflowCount(void);


#endif	/* SYSTEM_INIT_H */
