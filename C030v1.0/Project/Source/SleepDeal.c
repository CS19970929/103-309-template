#include "main.h"

void SleepTest(void);

UINT8 RTC_ExtComCnt;

// 通讯唤醒对深度休眠不起效果。不能再Base加入通讯唤醒。
void InitWakeUp_Base(void)
{
	EXTI_InitTypeDef EXTI_InitStruct;
	NVIC_InitTypeDef NVIC_InitStructure;
	GPIO_InitTypeDef GPIO_InitStructure;

	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE); // 使能GPIOA时钟，因为是开机，所以还是要
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_AFIO, ENABLE);  // 因为是开机，所以还是要

	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_0; // 选择要用的GPIO引脚,PA0也可以唤醒
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;
	GPIO_Init(GPIOA, &GPIO_InitStructure);

	GPIO_EXTILineConfig(GPIO_PortSourceGPIOA, GPIO_PinSource0);
	EXTI_InitStruct.EXTI_Line = EXTI_Line0;
	EXTI_InitStruct.EXTI_Mode = EXTI_Mode_Interrupt;
	EXTI_InitStruct.EXTI_Trigger = EXTI_Trigger_Falling;
	EXTI_InitStruct.EXTI_LineCmd = ENABLE;
	EXTI_Init(&EXTI_InitStruct);

	NVIC_InitStructure.NVIC_IRQChannel = EXTI0_IRQn;
	NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 0x01;
	NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0x01;
	NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
	NVIC_Init(&NVIC_InitStructure);

	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_2; // 选择要用的GPIO引脚,PA0也可以唤醒
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;
	GPIO_Init(GPIOC, &GPIO_InitStructure);

	GPIO_EXTILineConfig(GPIO_PortSourceGPIOC, GPIO_PinSource2);
	EXTI_InitStruct.EXTI_Line = EXTI_Line2;
	EXTI_InitStruct.EXTI_Mode = EXTI_Mode_Interrupt;
	EXTI_InitStruct.EXTI_Trigger = EXTI_Trigger_Falling;
	EXTI_InitStruct.EXTI_LineCmd = ENABLE;
	EXTI_Init(&EXTI_InitStruct);

	NVIC_InitStructure.NVIC_IRQChannel = EXTI2_IRQn;
	NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 0x01;
	NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0x01;
	NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
	NVIC_Init(&NVIC_InitStructure);
}

void InitWakeUp_NormalMode(void)
{
	EXTI_InitTypeDef EXTI_InitStruct;
	NVIC_InitTypeDef NVIC_InitStructure;
	GPIO_InitTypeDef GPIO_InitStructure;

	InitWakeUp_Base();

	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE); // 使能GPIOA时钟，因为是开机，所以还是要
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_AFIO, ENABLE);  // 因为是开机，所以还是要

	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_9; // 选择要用的GPIO引脚,PA0也可以唤醒
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;
	GPIO_Init(GPIOC, &GPIO_InitStructure);

	GPIO_EXTILineConfig(GPIO_PortSourceGPIOC, GPIO_PinSource9);
	EXTI_InitStruct.EXTI_Line = EXTI_Line9;
	EXTI_InitStruct.EXTI_Mode = EXTI_Mode_Interrupt;
	EXTI_InitStruct.EXTI_Trigger = EXTI_Trigger_Falling;
	EXTI_InitStruct.EXTI_LineCmd = ENABLE;
	EXTI_Init(&EXTI_InitStruct);

	NVIC_InitStructure.NVIC_IRQChannel = EXTI9_5_IRQn;
	NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 0x01;
	NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0x01;
	NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
	NVIC_Init(&NVIC_InitStructure);

	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_7; // 选择要用的GPIO引脚,PA0也可以唤醒
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;
	GPIO_Init(GPIOB, &GPIO_InitStructure);

	GPIO_EXTILineConfig(GPIO_PortSourceGPIOB, GPIO_PinSource7);
	EXTI_InitStruct.EXTI_Line = EXTI_Line7;
	EXTI_InitStruct.EXTI_Mode = EXTI_Mode_Interrupt;
	EXTI_InitStruct.EXTI_Trigger = EXTI_Trigger_Falling;
	EXTI_InitStruct.EXTI_LineCmd = ENABLE;
	EXTI_Init(&EXTI_InitStruct);

	NVIC_InitStructure.NVIC_IRQChannel = EXTI9_5_IRQn;
	NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 0x01;
	NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0x01;
	NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
	NVIC_Init(&NVIC_InitStructure);

	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_3; // 选择要用的GPIO引脚,PA0也可以唤醒
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;
	GPIO_Init(GPIOA, &GPIO_InitStructure);

	GPIO_EXTILineConfig(GPIO_PortSourceGPIOA, GPIO_PinSource3);
	EXTI_InitStruct.EXTI_Line = EXTI_Line3;
	EXTI_InitStruct.EXTI_Mode = EXTI_Mode_Interrupt;
	EXTI_InitStruct.EXTI_Trigger = EXTI_Trigger_Falling;
	EXTI_InitStruct.EXTI_LineCmd = ENABLE;
	EXTI_Init(&EXTI_InitStruct);

	NVIC_InitStructure.NVIC_IRQChannel = EXTI3_IRQn;
	NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 0x01;
	NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0x01;
	NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
	NVIC_Init(&NVIC_InitStructure);

	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_5; // 选择要用的GPIO引脚,PA0也可以唤醒
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;
	GPIO_Init(GPIOC, &GPIO_InitStructure);

	GPIO_EXTILineConfig(GPIO_PortSourceGPIOC, GPIO_PinSource5);
	EXTI_InitStruct.EXTI_Line = EXTI_Line5;
	EXTI_InitStruct.EXTI_Mode = EXTI_Mode_Interrupt;
	EXTI_InitStruct.EXTI_Trigger = EXTI_Trigger_Rising_Falling;
	EXTI_InitStruct.EXTI_LineCmd = ENABLE;
	EXTI_Init(&EXTI_InitStruct);

	NVIC_InitStructure.NVIC_IRQChannel = EXTI9_5_IRQn;
	NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 0x01;
	NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0x01;
	NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
	NVIC_Init(&NVIC_InitStructure);
}

void InitWakeUp_RTCMode(void)
{
	// InitWakeUp_Base();
	InitWakeUp_NormalMode(); // 包含了Base的唤醒方式
	RTC_WKTimeConfig();
}

// 如果是standby模式的话，PA0的wkup不用配
// 通讯唤醒对深度休眠不能起效果。
void InitWakeUp_DeepMode(void)
{
	InitWakeUp_Base();
}

void IOstatus_Base(void)
{
	GPIO_InitTypeDef GPIO_InitStructure;

	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE); // 使能GPIOA时钟
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE); // 使能GPIOB时钟
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOC, ENABLE); // 使能GPIOC时钟
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOD, ENABLE); // 使能GPIOD时钟
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOE, ENABLE); // 使能GPIOE时钟

	ADC_DeInit(ADC1); // 百度说有用，实际上不知道是否有用

	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_All;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AIN;
	GPIO_Init(GPIOA, &GPIO_InitStructure);

	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_All;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AIN;
	GPIO_Init(GPIOB, &GPIO_InitStructure);

	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_All;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AIN;
	GPIO_Init(GPIOC, &GPIO_InitStructure);

	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_All;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AIN;
	GPIO_Init(GPIOD, &GPIO_InitStructure);

	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_All;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AIN;
	GPIO_Init(GPIOE, &GPIO_InitStructure);

	// 开机时序模块
	// 电源类
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_12;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_2MHz;
	GPIO_Init(GPIOC, &GPIO_InitStructure);
	GPIO_ResetBits(GPIOC, GPIO_InitStructure.GPIO_Pin);

	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_13;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_2MHz;
	GPIO_Init(GPIOC, &GPIO_InitStructure);
	GPIO_ResetBits(GPIOC, GPIO_InitStructure.GPIO_Pin);

	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_2;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_2MHz;
	GPIO_Init(GPIOD, &GPIO_InitStructure);
	GPIO_ResetBits(GPIOD, GPIO_InitStructure.GPIO_Pin);

	__delay_ms(100);
}

void IOstatus_RTCMode(void)
{
	InitIO();
	InitDelay();
	InitSystemWakeUp();
	InitE2PROM(); // 内部EEPROM，不需要初始化
	InitRTC();

	InitAFE1_Sleep(1);
	AFE_IDLE();
	IOstatus_Base();
}

void IOstatus_NormalMode(void)
{
	/*
	RCC_APB1PeriphClockCmd(RCC_APB1Periph_PWR | RCC_APB1Periph_BKP, ENABLE);		//使能PWR外设时钟，待机模式，RTC，看门狗
	PWR_BackupAccessCmd(ENABLE);    						//允许访问RTC
	RCC_LSEConfig(RCC_LSE_ON);								//使能外部LSE晶振，RCC_LSE_Bypass旁路的意思应该是使能这个LSE时钟，但是单片机不用，外围电路用?
	//RCC_LSICmd(DISABLE);
	*/

	InitIO();
	InitDelay();
	InitSystemWakeUp();
	InitRTC(); // 这个函数能降低150uA的功耗，具体是上面那三句话，具体原因不明
	InitAFE1_Sleep(0);
	AFE_Sleep();
	IOstatus_Base();
}

void IOstatus_DeepMode(void)
{
	GPIO_InitTypeDef GPIO_InitStructure;

	IOstatus_NormalMode();
	// IOstatus_Base();

	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_12;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_2MHz;
	GPIO_Init(GPIOB, &GPIO_InitStructure);
	GPIO_ResetBits(GPIOB, GPIO_InitStructure.GPIO_Pin);
}

// 长期更新数据
void RefreshData_SleepFunction(void)
{
	SleepElement.u8_ExtComCnt = RTC_ExtComCnt;
	SleepElement.u16_VCellMax = g_stCellInfoReport.u16VCellMax;
	SleepElement.u16_VCellMin = g_stCellInfoReport.u16VCellMin;
	SleepElement.u16_CurChg = g_stCellInfoReport.u16Ichg;
	SleepElement.u16_CurDsg = g_stCellInfoReport.u16IDischg;
}

void GetData_SleepFunction(void)
{
	// SleepElement.u8_ToSleepFlag;
}

// 一次性赋值
void InitData_SleepFunc(void)
{
	SleepElement.u16_TimeRTC_First = OtherElement.u16Sleep_TimeRTC;
	SleepElement.u16_TimeRTC_Hiccup = OtherElement.u16Sleep_TimeRTC;
	SleepElement.u16_RTC_WakeUpTime = OtherElement.u16Sleep_RTC_WakeUpTime;
	SleepElement.u16_VNormal = OtherElement.u16Sleep_VNormal;
	SleepElement.u16_TimeNormal = OtherElement.u16Sleep_TimeNormal;
	SleepElement.u16_Vlow = OtherElement.u16Sleep_Vlow;
	SleepElement.u16_TimeVlow = OtherElement.u16Sleep_TimeVlow;
	SleepElement.u16_VirCur_Chg = OtherElement.u16Sleep_VirCur_Chg;
	SleepElement.u16_VirCur_Dsg = OtherElement.u16Sleep_VirCur_Dsg;

	// 定制版处理，时间修改地方
	SleepElement.u16_Force_RTC_First = 0;  // 未使用，置零
	SleepElement.u16_Force_RTC_Hiccup = 0; // 未使用，置零
	SleepElement.u16_Force_RTC_WKtime = 0; // 未使用，置零
	SleepElement.u16_Force_Normal_T = 0;   // 意味着直接进入
	SleepElement.u16_Force_Vlow_T = 0;	   // 意味着直接进入
}

void EnterSleepJudge(void)
{
	IsSleepStartUp();
}

void App_SleepDeal(void)
{
	if (SystemStatus.bits.b1StartUpBMS)
	{ // 开机完毕再进入
		return;
	}
	else
	{
		SystemStatus.bits.b1Status_ToSleep = 1;
	}

	if (SleepElement.u8_ToSleepFlag)
	{
		LogRecord_Flag.bits.Log_Sleep = 1;
		return;
	}

	RefreshData_SleepFunction();
	SleepCtrl(g_st_SysTimeFlag.bits.b1Sys1000msFlag1, System_OnOFF_Func.bits.b1OnOFF_Sleep);
	// SleepTest();		//需要立刻进入，则取消屏蔽即刻
}

void SleepTest(void)
{
	static UINT8 su8_aaa = 0;
	if (0 == g_st_SysTimeFlag.bits.b1Sys1000msFlag1)
	{
		return;
	}
	if (++su8_aaa > 3)
	{
		SleepElement.Sleep_Mode.bits.b1ForceToSleep_L1 = 1;
	}
}

void App_NormalSleepTest(void)
{
	static UINT16 s_u16HaltTestCnt = 0;

	if (0 == g_st_SysTimeFlag.bits.b1Sys1000msFlag1)
	{ // 休眠起来等待系统初始化完成
		return;
	}

	if (++s_u16HaltTestCnt >= 5)
	{ // 10s——Test
		s_u16HaltTestCnt = 0;
		IOstatus_NormalMode();
		InitWakeUp_NormalMode();
		PWR_EnterSTOPMode(PWR_Regulator_LowPower, PWR_STOPEntry_WFI);
		// Sys_StandbyMode();
		MCU_RESET();
	}
}

void App_RTCSleepTest(void)
{
	static UINT16 s_u16HaltTestCnt = 0;

	if (0 == g_st_SysTimeFlag.bits.b1Sys1000msFlag1)
	{ // 休眠起来等待系统初始化完成
		return;
	}

	if (++s_u16HaltTestCnt >= 5)
	{ // 10s——Test
		s_u16HaltTestCnt = 0;
		IOstatus_RTCMode();
		InitWakeUp_RTCMode();
		PWR_EnterSTOPMode(PWR_Regulator_LowPower, PWR_STOPEntry_WFI);
		MCU_RESET();
	}
}
