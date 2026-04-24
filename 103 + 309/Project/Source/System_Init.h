#ifndef SYSTEM_INIT_H
#define SYSTEM_INIT_H

#define BITBAND(addr, bitnum) ((addr & 0xF0000000)+0x2000000+((addr &0xFFFFF)<<5)+(bitnum<<2)) 
#define MEM_ADDR(addr)  *((volatile unsigned long  *)(addr)) 
#define BIT_ADDR(addr, bitnum)   MEM_ADDR(BITBAND(addr, bitnum)) 
//IO�ڵ�ַӳ��
#define GPIOA_ODR_Addr    (GPIOA_BASE+12) //0x4001080C
#define GPIOB_ODR_Addr    (GPIOB_BASE+12) //0x40010C0C
#define GPIOC_ODR_Addr    (GPIOC_BASE+12) //0x4001100C
#define GPIOD_ODR_Addr    (GPIOD_BASE+12) //0x4001140C
#define GPIOE_ODR_Addr    (GPIOE_BASE+12) //0x4001180C
#define GPIOF_ODR_Addr    (GPIOF_BASE+12) //0x40011A0C
#define GPIOG_ODR_Addr    (GPIOG_BASE+12) //0x40011E0C

#define GPIOA_IDR_Addr    (GPIOA_BASE+8) //0x40010808
#define GPIOB_IDR_Addr    (GPIOB_BASE+8) //0x40010C08
#define GPIOC_IDR_Addr    (GPIOC_BASE+8) //0x40011008
#define GPIOD_IDR_Addr    (GPIOD_BASE+8) //0x40011408
#define GPIOE_IDR_Addr    (GPIOE_BASE+8) //0x40011808
#define GPIOF_IDR_Addr    (GPIOF_BASE+8) //0x40011A08
#define GPIOG_IDR_Addr    (GPIOG_BASE+8) //0x40011E08

//IO�ڲ���,ֻ�Ե�һ��IO��!
//ȷ��n��ֵС��16!
#define PAout(n)   BIT_ADDR(GPIOA_ODR_Addr,n)  //���
#define PAin(n)    BIT_ADDR(GPIOA_IDR_Addr,n)  //����

#define PBout(n)   BIT_ADDR(GPIOB_ODR_Addr,n)  //���
#define PBin(n)    BIT_ADDR(GPIOB_IDR_Addr,n)  //����

#define PCout(n)   BIT_ADDR(GPIOC_ODR_Addr,n)  //���
#define PCin(n)    BIT_ADDR(GPIOC_IDR_Addr,n)  //����

#define PDout(n)   BIT_ADDR(GPIOD_ODR_Addr,n)  //���
#define PDin(n)    BIT_ADDR(GPIOD_IDR_Addr,n)  //����

#define PEout(n)   BIT_ADDR(GPIOE_ODR_Addr,n)  //���
#define PEin(n)    BIT_ADDR(GPIOE_IDR_Addr,n)  //����

#define PFout(n)   BIT_ADDR(GPIOF_ODR_Addr,n)  //���
#define PFin(n)    BIT_ADDR(GPIOF_IDR_Addr,n)  //����

#define PGout(n)   BIT_ADDR(GPIOG_ODR_Addr,n)  //���
#define PGin(n)    BIT_ADDR(GPIOG_IDR_Addr,n)  //����


#define MCUO_E2PR_WP		PBout(13)	//EEPROMд����

#define MCUI_INT_WK_MCU		PAin(0)		//����MCU


#define MCUO_RELAY_HEAT 	PCout(6)		//���ȼ̵���
#define MCUO_RELAY_COOL 	PCout(6)		//�����̵���

#define MCUI_ENI_DI1		PAin(9)		//I��1


union SYS_TIME {			//TODO
    UINT16 all;
    struct StatusSysTimeFlagBit {
        UINT8 b1Sys10msFlag1        :1;
        UINT8 b1Sys10msFlag2        :1;
        UINT8 b1Sys10msFlag3        :1;
        UINT8 b1Sys10msFlag4        :1;
		
        UINT8 b1Sys10msFlag5        :1;
		//UINT8 b1Sys20msFlag        	:1;
		UINT8 b1Sys1msFlag        	:1;	
		UINT8 b1Sys50msFlag        	:1;
		UINT8 b1Sys100msFlag       	:1;

		UINT8 b1Sys200msFlag1       :1;
		UINT8 b1Sys200msFlag2       :1;
		UINT8 b1Sys200msFlag3       :1;
		UINT8 b1Sys200msFlag4       :1;
		
		UINT8 b1Sys200msFlag5       :1;
		UINT8 b1Sys1000msFlag1      :1;
		UINT8 b1Sys1000msFlag2      :1;
		UINT8 b1Sys1000msFlag3      :1;
     }bits;
};


struct CBC_ELEMENT {
	UINT8 u8CBC_CHG_ErrFlag;	//����CBC������־λ
	UINT8 u8CBC_CHG_Cnt;		//���ֳ��CBC�Ĵ���
	UINT8 u8CBC_DSG_ErrFlag;	//����CBC������־λ
	UINT8 u8CBC_DSG_Cnt;		//���ַŵ�CBC�Ĵ���
};


void IWDG_Feed(void);
#define Feed_IWatchDog IWDG_Feed()

extern volatile union SYS_TIME g_st_SysTimeFlag;
extern struct CBC_ELEMENT CBC_Element;
extern UINT8 gu8_200msAccClock_Flag;
extern UINT8 gu8_1000msAccClock_Flag;


void InitDelay(void);
void __delay_ms(UINT16 nms);
void __delay_us(UINT32 nus);
void InitTimer(void);
void InitIO(void);
//void InitKey(void);
void InitNVIC(void);
void Init_IWDG(void);
void App_CBC(void);
void App_SysTime(void);
void App_ChgDet_Status(void);


#endif	/* SYSTEM_INIT_H */

