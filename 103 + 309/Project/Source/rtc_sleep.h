#ifndef __RTC_SLEEP__
#define __RTC_SLEEP__

// #include "stm32f0xx_it.h"			//里面有一些硬件错误之类的中断，还是需要的

#define SET_BIT(REG, BIT)     ((REG) |= (BIT))

#define CLEAR_BIT(REG, BIT)   ((REG) &= ~(BIT))

#define READ_BIT(REG, BIT)    ((REG) & (BIT))

#define CLEAR_REG(REG)        ((REG) = (0x0))

#define WRITE_REG(REG, VAL)   ((REG) = (VAL))

#define READ_REG(REG)         ((REG))

#define MODIFY_REG(REG, CLEARMASK, SETMASK)  WRITE_REG((REG), (((READ_REG(REG)) & (~(CLEARMASK))) | (SETMASK)))

#define POSITION_VAL(VAL)     (__CLZ(__RBIT(VAL)))


//todo
#define VcellMin    g_stCellInfoReport.u16VCellMin
#define VcellMax    g_stCellInfoReport.u16VCellMax

#define SOC_DISP    g_stCellInfoReport.SocElement.u16Soc
#define sOC_REAL    g_stCellInfoReport.soc_real

typedef enum _SLEEP_MODE {
NORMAL_MODE = 0, HICCUP_MODE, DEEP_MODE, NO_SLEEP,
}SLEEP_MODE;



#define enumToStr(WEEK)    #WEEK

extern bool is_wakeup;

extern UINT16 gu8_WakeUp_Type;

void rtc_sleep(void);

uint8_t get_rtc_soc(void);
void set_rtc_soc(uint8_t _soc);

// void set_irq_wksource(enum irqWakeup irq);
void set_irq_wksource(uint8_t irq);


bool isVol_low_sleep(void);
void entersleep(enum _SLEEP_MODE mode);

void sleep(void);



#endif
