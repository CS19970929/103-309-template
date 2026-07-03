#include "main.h"

volatile union SYS_TIME g_st_SysTimeFlag;
static volatile union SYS_TIME s_st_SysTimePending;
static volatile UINT32 s_u32Sys10msTickCount = 0U;

static UINT8 s_u8Cnt50ms = 0;
static UINT8 s_u8Cnt100ms = 0;
static UINT8 s_u8Cnt200ms = 0;
static UINT8 s_u8Cnt1000ms = 0;

static UINT8 fac_us = 0; // us延时倍乘数
static UINT16 fac_ms = 0;

#define SYS_TIME_200MS_PENDING_LIMIT ((UINT8)5U)

static volatile UINT8 s_u8Sys200msPendingPeriods = 0U;
static volatile UINT16 s_u16Sys200msOverflowCnt = 0U;

#define LOW_POWER_DEBUG_MASK (DBGMCU_CR_DBG_SLEEP |     \
							  DBGMCU_CR_DBG_STOP |      \
							  DBGMCU_CR_DBG_STANDBY |   \
							  DBGMCU_CR_DBG_IWDG_STOP | \
							  DBGMCU_CR_DBG_WWDG_STOP)

void EnableLowPowerDebug(void)
{
#ifdef __EnableLowPowerDebug__
	DBGMCU->CR |= LOW_POWER_DEBUG_MASK;
#else
	DBGMCU->CR &= (UINT32)(~LOW_POWER_DEBUG_MASK);
#endif
}

// 使用LSI
void Init_IWDG(void)
{
#if PROJECT_CFG_WDOG_ENABLE
	RCC_APB1PeriphClockCmd(RCC_APB1Periph_PWR, ENABLE); // 使能PWR外设时钟，待机模式，RTC，看门狗
	IWDG_WriteAccessCmd(IWDG_WriteAccess_Enable);		// 打开独立看门狗寄存器操作权限
#ifndef __FUNC_RTC__
	IWDG_SetPrescaler(IWDG_Prescaler_64); // 预分频系数
	IWDG_SetReload(800);				  // 设置重载计数值，k = Xms / (1 / (40KHz/64)) = X/64*40; 4096最高
#else
	IWDG_SetPrescaler(IWDG_Prescaler_256); // 预分频系数
	IWDG_SetReload(0x0FFF);				   // 设置重载计数值，k = Xms / (1 / (40KHz/64)) = X/64*40; 4096最高
										   // 800——1.28s，80——128ms
#endif					  // 设置重载计数值，k = Xms / (1 / (40KHz/64)) = X/64*40; 4096最高
						  // 800——1.28s，80——128ms
	IWDG_ReloadCounter(); // 喂狗
	IWDG_Enable();		  // 使能IWDG
#endif
}

// 关于NVIC_PriorityGroupConfig这个函数
// https://blog.csdn.net/zhuminzeng/article/details/8880138
// 第0组：所有4位用于指定响应优先级
// 第1组：最高1位用于指定抢占式优先级，最低3位用于指定响应优先级
void InitNVIC(void)
{
	NVIC_PriorityGroupConfig(NVIC_PriorityGroup_1); // 中断嵌套设计
}

// 非PCLK1(最大36MHz)，所以为72MHz
static UINT16 Timer_GetPrescalerFor100kHz(void)
{
	UINT32 div = SystemCoreClock / 100000U;

	if (div == 0U)
	{
		div = 1U;
	}
	if (div > 0x10000U)
	{
		div = 0x10000U;
	}

	return (UINT16)(div - 1U);
}

static void SysTime_ResetCounters(void)
{
	UINT32 primask = __get_PRIMASK();

	__disable_irq();

	g_st_SysTimeFlag.all = 0U;
	s_st_SysTimePending.all = 0U;
	s_u32Sys10msTickCount = 0U;
	ADC_ResetAnlogCalSchedule();

	s_u8Cnt50ms = 0U;
	s_u8Cnt100ms = 0U;
	s_u8Cnt200ms = 0U;
	s_u8Cnt1000ms = 0U;
	s_u8Sys200msPendingPeriods = 0U;
	s_u16Sys200msOverflowCnt = 0U;

	if (primask == 0U)
	{
		__enable_irq();
	}
}

void InitTimer(void)
{
	TIM_TimeBaseInitTypeDef timer_init;
	NVIC_InitTypeDef nvic_init;

	RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM3, ENABLE);
	TIM_Cmd(TIM3, DISABLE);
	TIM_ITConfig(TIM3, TIM_IT_Update, DISABLE);

	timer_init.TIM_Prescaler = Timer_GetPrescalerFor100kHz();
	timer_init.TIM_Period = 999U;
	timer_init.TIM_ClockDivision = TIM_CKD_DIV1;
	timer_init.TIM_CounterMode = TIM_CounterMode_Up;
	timer_init.TIM_RepetitionCounter = 0x00;
	TIM_TimeBaseInit(TIM3, &timer_init);
	TIM_SetCounter(TIM3, 0U);
	TIM_ClearITPendingBit(TIM3, TIM_IT_Update);
	TIM_ITConfig(TIM3, TIM_IT_Update, ENABLE);

	nvic_init.NVIC_IRQChannel = TIM3_IRQn;
	nvic_init.NVIC_IRQChannelPreemptionPriority = 0;
	nvic_init.NVIC_IRQChannelSubPriority = 3;
	nvic_init.NVIC_IRQChannelCmd = ENABLE;
	NVIC_Init(&nvic_init);

	SysTime_ResetCounters();
	TIM_Cmd(TIM3, ENABLE);
}
/*
HCLK即SYSCLK分频得来（在此未分频），即HCLK==SystemCoreClock
SysTick时钟是HCLK8分频，即SysTick时钟频率==HCLK/8==SystemCoreClock/8
*/
void InitDelay(void)
{
	SysTick->CTRL &= ~(1 << 2);			// 1=内核时钟(FCLK)       0=外部时钟源(STCLK)
	fac_us = SystemCoreClock / 8000000; // SysTick时钟是SYSCLK 8分频，即SysTick时钟频率=SYSCLK/8，1us要计的个数为还得/1MHz
	fac_ms = (INT16)fac_us * 1000;		// 每个ms需要的systick时钟数
}

void __delay_us(UINT32 nus)
{
	UINT32 temp;
	SysTick->LOAD = nus * fac_us;			  // 时间加载
	SysTick->VAL = 0x00;					  // 清空计数器
	SysTick->CTRL |= SysTick_CTRL_ENABLE_Msk; // 开始倒数
	do
	{
		temp = SysTick->CTRL;
	} while ((temp & 0x01) && !(temp & (1 << 16))); // 等待时间到达
	SysTick->CTRL &= ~SysTick_CTRL_ENABLE_Msk; // 关闭计数器
	SysTick->VAL = 0X00;					   // 清空计数器
}

// 注意nms的范围
// SysTick->LOAD为24位寄存器,所以,最大延时为:
// nms<=2^24*8*1000/SYSCLK = 2^24/fac_ms
// SYSCLK单位为Hz,nms单位为ms
// 对72M条件下,nms<=1864
// 48M为nms<=2^24/6000 = 2796
// 意思就是2^24次计数对应时间长度(基于HCLK/8   )
void __delay_ms(UINT16 nms)
{
	UINT32 temp;
	SysTick->LOAD = (UINT32)nms * fac_ms;	  // 时间加载(SysTick->LOAD为24bit)
	SysTick->VAL = 0x00;					  // 清空计数器
	SysTick->CTRL |= SysTick_CTRL_ENABLE_Msk; // 开始倒数
	do
	{
		temp = SysTick->CTRL;
		Feed_IWatchDog;
	} while ((temp & 0x01) && !(temp & (1 << 16))); // 等待时间到达
	SysTick->CTRL &= ~SysTick_CTRL_ENABLE_Msk; // 关闭计数器
	SysTick->VAL = 0X00;					   // 清空计数器
}

void SysTime_LatchTaskFlags(void)
{
	UINT32 primask = __get_PRIMASK();

	__disable_irq();
	g_st_SysTimeFlag.all = s_st_SysTimePending.all;
	s_st_SysTimePending.all = 0U;

	if (primask == 0U)
	{
		__enable_irq();
	}
}

UINT8 SysTime_HasPendingTaskFlags(void)
{
	return (s_st_SysTimePending.all != 0U) ? 1U : 0U;
}

UINT32 SysTime_Get10msTickCount(void)
{
	UINT32 tick_count;
	UINT32 primask = __get_PRIMASK();

	__disable_irq();
	tick_count = s_u32Sys10msTickCount;

	if (primask == 0U)
	{
		__enable_irq();
	}

	return tick_count;
}

UINT8 SysTime_Take200msTaskPeriod(void)
{
	UINT8 has_period = 0U;
	UINT32 primask = __get_PRIMASK();

	__disable_irq();
	if (s_u8Sys200msPendingPeriods != 0U)
	{
		s_u8Sys200msPendingPeriods--;
		has_period = 1U;
	}

	if (primask == 0U)
	{
		__enable_irq();
	}

	return has_period;
}

UINT16 SysTime_Get200msTaskOverflowCount(void)
{
	UINT16 overflow_count;
	UINT32 primask = __get_PRIMASK();

	__disable_irq();
	overflow_count = s_u16Sys200msOverflowCnt;

	if (primask == 0U)
	{
		__enable_irq();
	}

	return overflow_count;
}

static void SysTime_Post200msTaskPeriod(void)
{
	if (s_u8Sys200msPendingPeriods < SYS_TIME_200MS_PENDING_LIMIT)
	{
		s_u8Sys200msPendingPeriods++;
	}
	else if (s_u16Sys200msOverflowCnt < (UINT16)0xFFFFU)
	{
		s_u16Sys200msOverflowCnt++;
	}
}

static void SysTime_Post10msTick(void)
{
	s_u32Sys10msTickCount++;
	s_st_SysTimePending.bits.b1Sys10msFlag = 1U;

	if (++s_u8Cnt50ms >= 5U)
	{
		s_u8Cnt50ms = 0U;
		s_st_SysTimePending.bits.b1Sys50msFlag = 1U;
	}

	if (++s_u8Cnt100ms >= 10U)
	{
		s_u8Cnt100ms = 0U;
		s_st_SysTimePending.bits.b1Sys100msFlag = 1U;
	}

	if (++s_u8Cnt200ms >= 20U)
	{
		s_u8Cnt200ms = 0U;
		s_st_SysTimePending.bits.b1Sys200msFlag = 1U;
		SysTime_Post200msTaskPeriod();
	}

	if (++s_u8Cnt1000ms >= 100U)
	{
		s_u8Cnt1000ms = 0U;
		s_st_SysTimePending.bits.b1Sys1000msFlag = 1U;
	}
}
void IWDG_Feed(void)
{
#if PROJECT_CFG_WDOG_ENABLE
	IWDG_ReloadCounter();
#endif
}

void TIM3_IRQHandler(void)
{
	static uint8_t sleep_state = 0;

	if (TIM_GetITStatus(TIM3, TIM_IT_Update) != RESET)
	{
		TIM_ClearITPendingBit(TIM3, TIM_IT_Update);
		SysTime_Post10msTick();
		sys_time.rtc_sec_cnt = RTC_GetCounter();

		// switch (sleep_state)
		// {
		// case 0:
		// 	if (1 == MCUI_ENI_DI1 && !g_stCellInfoReport.u16Ichg)
		// 	{
		// 		MCUO_AFE_CTLC = 0;
		// 		if (!IsChargeActive())
		// 		{
		// 			MCUO_AFE_CTLC = 1;
		// 			LowPower_Request(NORMAL_MODE);
		// 		}
		// 		else
		// 			MCUO_AFE_CTLC = 1;
		// 	}
		// 	break;
		// default:
		// 	break;
		// }
	}
}
