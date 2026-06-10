#include "main.h"

volatile union SYS_TIME g_st_SysTimeFlag;
struct CBC_ELEMENT CBC_Element;

UINT8 g_u81msCnt = 0;
UINT8 g_u810msClockCnt = 0;
UINT8 g_u81msClockCnt = 0;

UINT8 gu8_200msCnt = 0;
UINT8 gu8_200msAccClock_Flag = 0;
UINT8 gu8_200msAccClock_Flag2 = 0;
UINT8 gu8_1000msAccClock_Flag = 0;

#define SYS_TIME_1MS_PENDING_MAX ((UINT8)20)
#define SYS_TIME_10MS_PHASE_PENDING_MAX ((UINT8)25)

static volatile UINT8 s_u8Sys1msPending = 0;
static volatile UINT8 s_u8Sys10msPhasePending = 0;
static UINT8 s_u8Sys10msNextPhase = 1;

volatile UINT16 gu16_SysTime1msOverrunCnt = 0;
volatile UINT16 gu16_SysTime10msPhaseOverrunCnt = 0;

volatile APP_TRACE_TASK g_stAppTraceTask[APP_TRACE_TASK_NUM];
volatile APP_TRACE_WARN_CHECK g_stAppTraceWarnCheck[APP_WARN_CHECK_NUM];
volatile UINT32 gu32_AppTraceLoopCnt = 0;
volatile UINT32 gu32_AppTrace1msTick = 0;
volatile UINT32 gu32_AppTrace10msPhaseTick = 0;
volatile UINT32 gu32_AppTrace10msFlag1Tick = 0;
volatile UINT8 gu8_AppTraceCurrentTask = APP_TRACE_TASK_NONE;
volatile UINT8 gu8_AppTraceCurrentWarnCheck = APP_WARN_CHECK_NONE;
volatile UINT8 gu8_AppTraceLast10msPhase = 0;

static UINT8 fac_us = 0; // us延时倍乘数
static UINT16 fac_ms = 0;

// 使用LSI
void Init_IWDG(void)
{
	RCC_APB1PeriphClockCmd(RCC_APB1Periph_PWR, ENABLE); // 使能PWR外设时钟，待机模式，RTC，看门狗
	IWDG_WriteAccessCmd(IWDG_WriteAccess_Enable);		// 打开独立看门狗寄存器操作权限
#ifndef __FUNC_RTC__
	IWDG_SetPrescaler(IWDG_Prescaler_64); // 预分频系数
	IWDG_SetReload(800);				  // 设置重载计数值，k = Xms / (1 / (40KHz/64)) = X/64*40; 4096最高
#else
	IWDG_SetPrescaler(IWDG_Prescaler_256); // 预分频系数
	IWDG_SetReload(0x0FFF);				   // 设置重载计数值，k = Xms / (1 / (40KHz/64)) = X/64*40; 4096最高
										   // 800——1.28s，80——128ms
#endif															// 设置重载计数值，k = Xms / (1 / (40KHz/64)) = X/64*40; 4096最高
																// 800——1.28s，80——128ms
	IWDG_ReloadCounter();										// 喂狗
	IWDG_Enable();												// 使能IWDG
	DBGMCU->CR |= ((uint32_t)0x00000100); /* Debug IWDG Stop */ // STlink使用
	DBGMCU->CR |= ((uint32_t)0x00000200);						/* Debug WWDG Stop */
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
void InitTimer(void)
{
	TIM_TimeBaseInitTypeDef TIM_TimeBaseStructure;
	NVIC_InitTypeDef NVIC_InitStructure;

	RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM3, ENABLE); // 时钟3使能

	// 定时器TIM3初始化
	if (0 == System_ErrFlag.u8ErrFlag_HSE)
	{
		// TIM_TimeBaseStructure.TIM_Prescaler = 72 - 1; // 设置用来作为TIMx时钟频率除数的预分频值——计数分频
		TIM_TimeBaseStructure.TIM_Prescaler = 80 - 1; // 设置用来作为TIMx时钟频率除数的预分频值——计数分频
	}
	else
	{
		TIM_TimeBaseStructure.TIM_Prescaler = 8 - 1; // 设置用来作为TIMx时钟频率除数的预分频值——计数分频
	}
	TIM_TimeBaseStructure.TIM_Period = 99;						// 设置在下一个更新事件装入活动的自动重装载寄存器周期的值
	TIM_TimeBaseStructure.TIM_ClockDivision = TIM_CKD_DIV1;		// 设置时钟分割:TDTS = Tck_tim——时钟分频
	TIM_TimeBaseStructure.TIM_CounterMode = TIM_CounterMode_Up; // TIM向上计数模式
																// 我看了，向下计数是从自动装载值递减至0，向上计数是从0增加至装载值，也就是说在中断时间上没什么区别
	TIM_TimeBaseInit(TIM3, &TIM_TimeBaseStructure);				// 根据指定的参数初始化TIMx的时间基数单位
	TIM_ClearITPendingBit(TIM3, TIM_IT_Update);					// 清除更新中断请求位，据说这个能防止打开中断瞬间立刻进入中断函数
	TIM_ITConfig(TIM3, TIM_IT_Update, ENABLE);					// 使能指定的TIM3中断,允许更新中断

	/*	TIM3 中断嵌套设计*/
	NVIC_InitStructure.NVIC_IRQChannel = TIM3_IRQn;
	NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 1; // 抢占优先级低于USART，避免串口接收溢出
	NVIC_InitStructure.NVIC_IRQChannelSubPriority = 3;		  // 从优先级3级
	NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
	NVIC_Init(&NVIC_InitStructure);

	TIM_Cmd(TIM3, ENABLE); // 使能TIMx
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

static UINT16 AppTrace_DeltaToU16(UINT32 u32Now, UINT32 u32Last)
{
	UINT32 u32Delta = u32Now - u32Last;

	if (u32Delta > 0xFFFF)
	{
		return 0xFFFF;
	}
	return (UINT16)u32Delta;
}

void AppTrace_LoopBegin(void)
{
	gu32_AppTraceLoopCnt++;
}

void AppTrace_TaskBegin(UINT8 u8TaskId)
{
	volatile APP_TRACE_TASK *pstTrace;
	UINT16 u16Delta;

	if (u8TaskId >= APP_TRACE_TASK_NUM)
	{
		return;
	}

	pstTrace = &g_stAppTraceTask[u8TaskId];
	pstTrace->active = 1;
	pstTrace->runCnt++;

	if (pstTrace->lastLoopCnt != 0)
	{
		u16Delta = AppTrace_DeltaToU16(gu32_AppTraceLoopCnt, pstTrace->lastLoopCnt);
		pstTrace->lastLoopInterval = u16Delta;
		if (u16Delta > pstTrace->maxLoopInterval)
		{
			pstTrace->maxLoopInterval = u16Delta;
		}
	}

	if (pstTrace->last10msFlag1Tick != 0)
	{
		u16Delta = AppTrace_DeltaToU16(gu32_AppTrace10msFlag1Tick, pstTrace->last10msFlag1Tick);
		pstTrace->last10msFlag1Interval = u16Delta;
		if (u16Delta > pstTrace->max10msFlag1Interval)
		{
			pstTrace->max10msFlag1Interval = u16Delta;
		}
	}

	pstTrace->lastLoopCnt = gu32_AppTraceLoopCnt;
	pstTrace->last1msTick = gu32_AppTrace1msTick;
	pstTrace->last10msPhaseTick = gu32_AppTrace10msPhaseTick;
	pstTrace->last10msFlag1Tick = gu32_AppTrace10msFlag1Tick;
	gu8_AppTraceCurrentTask = u8TaskId;
}

void AppTrace_TaskEnd(UINT8 u8TaskId)
{
	if (u8TaskId < APP_TRACE_TASK_NUM)
	{
		g_stAppTraceTask[u8TaskId].active = 0;
	}
	if (gu8_AppTraceCurrentTask == u8TaskId)
	{
		gu8_AppTraceCurrentTask = APP_TRACE_TASK_NONE;
	}
}

void AppTrace_WarnCheckBegin(UINT8 u8CheckId)
{
	volatile APP_TRACE_WARN_CHECK *pstTrace;
	UINT32 u32WarnCtrlCnt;
	UINT16 u16Delta;

	if (u8CheckId >= APP_WARN_CHECK_NUM)
	{
		return;
	}

	u32WarnCtrlCnt = g_stAppTraceTask[APP_TRACE_TASK_WARN_CTRL].runCnt;
	pstTrace = &g_stAppTraceWarnCheck[u8CheckId];
	pstTrace->active = 1;
	pstTrace->runCnt++;

	if (pstTrace->lastWarnCtrlCnt != 0)
	{
		u16Delta = AppTrace_DeltaToU16(u32WarnCtrlCnt, pstTrace->lastWarnCtrlCnt);
		pstTrace->lastWarnCtrlInterval = u16Delta;
		if (u16Delta > pstTrace->maxWarnCtrlInterval)
		{
			pstTrace->maxWarnCtrlInterval = u16Delta;
		}
	}

	pstTrace->lastWarnCtrlCnt = u32WarnCtrlCnt;
	gu8_AppTraceCurrentWarnCheck = u8CheckId;
}

void AppTrace_WarnCheckEnd(UINT8 u8CheckId)
{
	if (u8CheckId < APP_WARN_CHECK_NUM)
	{
		g_stAppTraceWarnCheck[u8CheckId].active = 0;
	}
	if (gu8_AppTraceCurrentWarnCheck == u8CheckId)
	{
		gu8_AppTraceCurrentWarnCheck = APP_WARN_CHECK_NONE;
	}
}
static void SysTime_AddPending(volatile UINT8 *pu8Pending, UINT8 u8Max, volatile UINT16 *pu16OverrunCnt)
{
	if (*pu8Pending < u8Max)
	{
		(*pu8Pending)++;
	}
	else if (*pu16OverrunCnt < 0xFFFF)
	{
		(*pu16OverrunCnt)++;
	}
}

static UINT8 SysTime_TakePending(volatile UINT8 *pu8Pending)
{
	UINT8 u8HasEvent = 0;
	UINT32 u32Primask;

	u32Primask = __get_PRIMASK();
	__disable_irq();
	if (*pu8Pending > 0)
	{
		(*pu8Pending)--;
		u8HasEvent = 1;
	}
	__set_PRIMASK(u32Primask);

	return u8HasEvent;
}

static UINT8 SysTime_Take10msPhase(UINT8 *pu8Phase)
{
	UINT8 u8HasEvent;
	UINT32 u32Primask;

	u32Primask = __get_PRIMASK();
	__disable_irq();
	if (s_u8Sys10msPhasePending > 0)
	{
		s_u8Sys10msPhasePending--;
		*pu8Phase = s_u8Sys10msNextPhase;
		s_u8Sys10msNextPhase++;
		if (s_u8Sys10msNextPhase >= 5)
		{
			s_u8Sys10msNextPhase = 0;
		}
		u8HasEvent = 1;
	}
	else
	{
		u8HasEvent = 0;
	}
	__set_PRIMASK(u32Primask);

	return u8HasEvent;
}

void App_SysTime(void)
{
	// static UINT8 s_u8Cnt20ms = 0;
	static UINT8 s_u8Cnt50ms = 0;
	static UINT8 s_u8Cnt100ms = 0;

	static UINT8 s_u8Cnt200ms1 = 0;
	static UINT8 s_u8Cnt200ms2 = 4;
	static UINT8 s_u8Cnt200ms3 = 8;
	static UINT8 s_u8Cnt200ms4 = 12;
	static UINT8 s_u8Cnt200ms5 = 16;

	static UINT8 s_u8Cnt1000ms1 = 0;
	static UINT8 s_u8Cnt1000ms2 = 33;
	static UINT8 s_u8Cnt1000ms3 = 66;

	UINT8 u8Phase = 0;

	g_st_SysTimeFlag.bits.b1Sys1msFlag = 0;
	if (SysTime_TakePending(&s_u8Sys1msPending))
	{
		gu32_AppTrace1msTick++;
		// 1ms定时标志
		g_st_SysTimeFlag.bits.b1Sys1msFlag = 1;
	}

	g_st_SysTimeFlag.bits.b1Sys10msFlag1 = 0;
	g_st_SysTimeFlag.bits.b1Sys10msFlag2 = 0;
	g_st_SysTimeFlag.bits.b1Sys10msFlag3 = 0;
	g_st_SysTimeFlag.bits.b1Sys10msFlag4 = 0;
	g_st_SysTimeFlag.bits.b1Sys10msFlag5 = 0;
	if (SysTime_Take10msPhase(&u8Phase))
	{
		gu32_AppTrace10msPhaseTick++;
		gu8_AppTraceLast10msPhase = u8Phase;
		if (u8Phase == 0)
		{
			gu32_AppTrace10msFlag1Tick++;
		}
		// 10ms分相定时标志，按TIM3入队顺序逐个派发，避免主循环慢时漏标志
		switch (u8Phase)
		{
		case 0:
			g_st_SysTimeFlag.bits.b1Sys10msFlag1 = 1;
			// MCUO_DEBUG_LED2 = !MCUO_DEBUG_LED2;
			break;

		case 1:
			s_u8Cnt50ms++;
			g_st_SysTimeFlag.bits.b1Sys10msFlag2 = 1;
			break;

		case 2:
			s_u8Cnt100ms++;
			g_st_SysTimeFlag.bits.b1Sys10msFlag3 = 1;
			break;

		case 3:
			s_u8Cnt200ms1++;
			s_u8Cnt200ms2++;
			s_u8Cnt200ms3++;
			s_u8Cnt200ms4++;
			s_u8Cnt200ms5++;
			g_st_SysTimeFlag.bits.b1Sys10msFlag4 = 1;
			break;

		case 4:
			s_u8Cnt1000ms1++;
			s_u8Cnt1000ms2++;
			s_u8Cnt1000ms3++;
			g_st_SysTimeFlag.bits.b1Sys10msFlag5 = 1;
			break;

		default:
			break;
		}
	}

	g_st_SysTimeFlag.bits.b1Sys50msFlag = 0;
	if (s_u8Cnt50ms >= 5)
	{
		s_u8Cnt50ms = 0;
		g_st_SysTimeFlag.bits.b1Sys50msFlag = 1; // 50ms定时标志
	}

	g_st_SysTimeFlag.bits.b1Sys100msFlag = 0;
	if (s_u8Cnt100ms >= 10)
	{
		s_u8Cnt100ms = 0;
		g_st_SysTimeFlag.bits.b1Sys100msFlag = 1; // 100ms定时标志
	}

	g_st_SysTimeFlag.bits.b1Sys200msFlag1 = 0;
	g_st_SysTimeFlag.bits.b1Sys200msFlag2 = 0;
	g_st_SysTimeFlag.bits.b1Sys200msFlag3 = 0;
	g_st_SysTimeFlag.bits.b1Sys200msFlag4 = 0;
	g_st_SysTimeFlag.bits.b1Sys200msFlag5 = 0;
	if (s_u8Cnt200ms1 >= 20)
	{
		s_u8Cnt200ms1 = 0;
		g_st_SysTimeFlag.bits.b1Sys200msFlag1 = 1; // 200ms定时标志
	}
	if (s_u8Cnt200ms2 >= 20)
	{
		s_u8Cnt200ms2 = 0;
		g_st_SysTimeFlag.bits.b1Sys200msFlag2 = 1; // 200ms定时标志
	}
	if (s_u8Cnt200ms3 >= 20)
	{
		s_u8Cnt200ms3 = 0;
		g_st_SysTimeFlag.bits.b1Sys200msFlag3 = 1; // 200ms定时标志
	}
	if (s_u8Cnt200ms4 >= 20)
	{
		s_u8Cnt200ms4 = 0;
		g_st_SysTimeFlag.bits.b1Sys200msFlag4 = 1; // 200ms定时标志
	}
	if (s_u8Cnt200ms5 >= 20)
	{
		s_u8Cnt200ms5 = 0;
		g_st_SysTimeFlag.bits.b1Sys200msFlag5 = 1; // 200ms定时标志
	}

	g_st_SysTimeFlag.bits.b1Sys1000msFlag1 = 0;
	g_st_SysTimeFlag.bits.b1Sys1000msFlag2 = 0;
	g_st_SysTimeFlag.bits.b1Sys1000msFlag3 = 0;
	if (s_u8Cnt1000ms1 >= 100)
	{
		s_u8Cnt1000ms1 = 0;
		g_st_SysTimeFlag.bits.b1Sys1000msFlag1 = 1; // 1000ms定时标志
	}
	if (s_u8Cnt1000ms2 >= 100)
	{
		s_u8Cnt1000ms2 = 0;
		g_st_SysTimeFlag.bits.b1Sys1000msFlag2 = 1; // 1000ms定时标志
	}
	if (s_u8Cnt1000ms3 >= 100)
	{
		s_u8Cnt1000ms3 = 0;
		g_st_SysTimeFlag.bits.b1Sys1000msFlag3 = 1; // 1000ms定时标志
													// MCUO_DEBUG_LED2 = !MCUO_DEBUG_LED2;
	}
}

void IWDG_Feed(void)
{
	IWDG_ReloadCounter();
}

// 定时器3中断服务程序
void TIM3_IRQHandler(void)
{ // TIM3中断
	static uint16_t cnt_1000ms = 0;
	if (TIM_GetITStatus(TIM3, TIM_IT_Update) != RESET)
	{												// 检查TIM3更新中断发生与否
		TIM_ClearITPendingBit(TIM3, TIM_IT_Update); // 清除TIMx更新中断标志
		if ((++g_u81msCnt) >= 1)
		{ // 1ms
			g_u81msCnt = 0;
			g_u81msClockCnt++;
			gu8_200msCnt++;

			SysTime_AddPending(&s_u8Sys1msPending, SYS_TIME_1MS_PENDING_MAX, &gu16_SysTime1msOverrunCnt);

			if (g_u81msClockCnt >= 2)
			{ // 2ms
				g_u81msClockCnt = 0;
				g_u810msClockCnt++;
				SysTime_AddPending(&s_u8Sys10msPhasePending, SYS_TIME_10MS_PHASE_PENDING_MAX, &gu16_SysTime10msPhaseOverrunCnt);
				if (g_u810msClockCnt >= 5)
				{ // 10ms
					g_u810msClockCnt = 0;
				}
			}

			if (gu8_200msCnt >= 200)
			{
				gu8_200msCnt = 0;
				gu8_200msAccClock_Flag = 1;
				gu8_200msAccClock_Flag2 = 1;
				MCUO_DEBUG_LED1 = !MCUO_DEBUG_LED1;
			}
			if (++cnt_1000ms >= 1000)
			{
				cnt_1000ms = 0;
				gu8_1000msAccClock_Flag = 1;
			}
		}
	}
}
