#include "main.h"

volatile union SLEEP_MODE Sleep_Mode; // 鐢ㄤ簬澶栭儴鎺у埗杩涘叆浼戠湢鏍囧織浣?
enum SLEEP_STATUS Sleep_Status = SLEEP_HICCUP_SHIFT;

UINT8 gu8_SleepStatus = 0;
UINT8 RTC_ExtComCnt = 0;

uint8_t reset_sleep_state = 0;

void InitWakeUp_Base(void)
{
	EXTI_InitTypeDef EXTI_InitStruct;
	NVIC_InitTypeDef NVIC_InitStructure;
	GPIO_InitTypeDef GPIO_InitStructure;

	RCC_APB1PeriphClockCmd(RCC_APB1Periph_PWR, ENABLE); // 浣胯兘PWR澶栬?炬椂閽燂紝寰呮満妯″紡锛孯TC锛岀湅闂ㄧ嫍

	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_0; // 閫夋嫨瑕佺敤鐨凣PIO寮曡剼
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN;
	GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_NOPULL; // 璁剧疆寮曡剼妯″紡涓轰笂鎷夎緭鍏ユā寮?
	GPIO_Init(GPIOA, &GPIO_InitStructure);

	SYSCFG_EXTILineConfig(EXTI_PortSourceGPIOA, EXTI_PinSource0);
	EXTI_InitStruct.EXTI_Line = EXTI_Line0;
	EXTI_InitStruct.EXTI_Mode = EXTI_Mode_Interrupt;
	EXTI_InitStruct.EXTI_Trigger = EXTI_Trigger_Rising_Falling; // 涓婂崌娌夸腑鏂?
	EXTI_InitStruct.EXTI_LineCmd = ENABLE;
	EXTI_Init(&EXTI_InitStruct);
	NVIC_InitStructure.NVIC_IRQChannel = EXTI0_1_IRQn; // 浣胯兘鎸夐敭WK_UP鎵�鍦ㄧ殑澶栭儴涓?鏂?閫氶亾
	NVIC_InitStructure.NVIC_IRQChannelPriority = 0x00; // 鎶㈠崰浼樺厛绾?0
	NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;	   // 浣胯兘澶栭儴涓?鏂?閫氶亾
	NVIC_Init(&NVIC_InitStructure);

	// DI1
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_13; // 閫夋嫨瑕佺敤鐨凣PIO寮曡剼
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN;
	GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_NOPULL; // 璁剧疆寮曡剼妯″紡涓轰笂鎷夎緭鍏ユā寮?
	GPIO_Init(GPIOC, &GPIO_InitStructure);

	// 璁剧疆涓?鏂?绾?1锛孍XTI1鍜孭A1鎸傞挬
	SYSCFG_EXTILineConfig(EXTI_PortSourceGPIOC, EXTI_PinSource13);
	// 閰嶇疆PA1_WKUP澶栭儴涓婂崌娌夸腑鏂?
	EXTI_InitStruct.EXTI_Line = EXTI_Line13;
	EXTI_InitStruct.EXTI_Mode = EXTI_Mode_Interrupt;
	EXTI_InitStruct.EXTI_Trigger = EXTI_Trigger_Falling; // 涓婂崌娌夸腑鏂?
	EXTI_InitStruct.EXTI_LineCmd = ENABLE;
	EXTI_Init(&EXTI_InitStruct);
	// 涓?鏂?宓屽?楄?捐??
	NVIC_InitStructure.NVIC_IRQChannel = EXTI4_15_IRQn; // 浣胯兘鎸夐敭WK_UP鎵�鍦ㄧ殑澶栭儴涓?鏂?閫氶亾
	NVIC_InitStructure.NVIC_IRQChannelPriority = 0x00;	// 鎶㈠崰浼樺厛绾?0
	NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;		// 浣胯兘澶栭儴涓?鏂?閫氶亾
	NVIC_Init(&NVIC_InitStructure);

	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_14; // 閫夋嫨瑕佺敤鐨凣PIO寮曡剼
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN;
	GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_NOPULL; // 璁剧疆寮曡剼妯″紡涓轰笂鎷夎緭鍏ユā寮?
	GPIO_Init(GPIOB, &GPIO_InitStructure);

	// 璁剧疆涓?鏂?绾?1锛孍XTI1鍜孭A1鎸傞挬
	SYSCFG_EXTILineConfig(EXTI_PortSourceGPIOB, EXTI_PinSource14);
	// 閰嶇疆PA1_WKUP澶栭儴涓婂崌娌夸腑鏂?
	EXTI_InitStruct.EXTI_Line = EXTI_Line14;
	EXTI_InitStruct.EXTI_Mode = EXTI_Mode_Interrupt;
	EXTI_InitStruct.EXTI_Trigger = EXTI_Trigger_Falling; // 涓婂崌娌夸腑鏂?
	EXTI_InitStruct.EXTI_LineCmd = ENABLE;
	EXTI_Init(&EXTI_InitStruct);
	// 涓?鏂?宓屽?楄?捐??
	NVIC_InitStructure.NVIC_IRQChannel = EXTI4_15_IRQn; // 浣胯兘鎸夐敭WK_UP鎵�鍦ㄧ殑澶栭儴涓?鏂?閫氶亾
	NVIC_InitStructure.NVIC_IRQChannelPriority = 0x00;	// 鎶㈠崰浼樺厛绾?0
	NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;		// 浣胯兘澶栭儴涓?鏂?閫氶亾
	NVIC_Init(&NVIC_InitStructure);
}

void InitWakeUp_NormalMode(void)
{
	EXTI_InitTypeDef EXTI_InitStruct;
	NVIC_InitTypeDef NVIC_InitStructure;
	GPIO_InitTypeDef GPIO_InitStructure;

	// 涓插彛1鐨凴X鍞ら啋
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_10; // 閫夋嫨瑕佺敤鐨凣PIO寮曡剼
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN;
	GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_NOPULL; // 璁剧疆寮曡剼妯″紡涓轰笂鎷夎緭鍏ユā寮?
	GPIO_Init(GPIOA, &GPIO_InitStructure);

	// 璁剧疆涓?鏂?绾?1锛孍XTI1鍜孭A1鎸傞挬
	SYSCFG_EXTILineConfig(EXTI_PortSourceGPIOA, EXTI_PinSource10);
	// 閰嶇疆PA1_WKUP澶栭儴涓婂崌娌夸腑鏂?
	EXTI_InitStruct.EXTI_Line = EXTI_Line10;
	EXTI_InitStruct.EXTI_Mode = EXTI_Mode_Interrupt;
	EXTI_InitStruct.EXTI_Trigger = EXTI_Trigger_Rising; // 涓婂崌娌夸腑鏂?
	EXTI_InitStruct.EXTI_LineCmd = ENABLE;
	EXTI_Init(&EXTI_InitStruct);
	// 涓?鏂?宓屽?楄?捐??
	NVIC_InitStructure.NVIC_IRQChannel = EXTI4_15_IRQn; // 浣胯兘鎸夐敭WK_UP鎵�鍦ㄧ殑澶栭儴涓?鏂?閫氶亾
	NVIC_InitStructure.NVIC_IRQChannelPriority = 0x00;	// 鎶㈠崰浼樺厛绾?0
	NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;		// 浣胯兘澶栭儴涓?鏂?閫氶亾
	NVIC_Init(&NVIC_InitStructure);

	//	//涓插彛1鐨凴X鍞ら啋
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_3; // 閫夋嫨瑕佺敤鐨凣PIO寮曡剼
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN;
	GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_NOPULL; // 璁剧疆寮曡剼妯″紡涓轰笂鎷夎緭鍏ユā寮?
	GPIO_Init(GPIOA, &GPIO_InitStructure);

	// 璁剧疆涓?鏂?绾?1锛孍XTI1鍜孭A1鎸傞挬
	SYSCFG_EXTILineConfig(EXTI_PortSourceGPIOA, EXTI_PinSource3);
	// 閰嶇疆PA1_WKUP澶栭儴涓婂崌娌夸腑鏂?
	EXTI_InitStruct.EXTI_Line = EXTI_Line3;
	EXTI_InitStruct.EXTI_Mode = EXTI_Mode_Interrupt;
	EXTI_InitStruct.EXTI_Trigger = EXTI_Trigger_Rising; // 涓婂崌娌夸腑鏂?
	EXTI_InitStruct.EXTI_LineCmd = ENABLE;
	EXTI_Init(&EXTI_InitStruct);
	// 涓?鏂?宓屽?楄?捐??
	NVIC_InitStructure.NVIC_IRQChannel = EXTI2_3_IRQn; // 浣胯兘鎸夐敭WK_UP鎵�鍦ㄧ殑澶栭儴涓?鏂?閫氶亾
	NVIC_InitStructure.NVIC_IRQChannelPriority = 0x00; // 鎶㈠崰浼樺厛绾?0
	NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;	   // 浣胯兘澶栭儴涓?鏂?閫氶亾
	NVIC_Init(&NVIC_InitStructure);

	InitWakeUp_Base();
}

void InitWakeUp_RTCMode(void)
{
	// InitWakeUp_Base();
	InitWakeUp_NormalMode(); // 鍖呭惈浜咮ase鐨勫敜閱掓柟寮?
	RTC_TimeConfig();
	RTC_AlarmConfig();
}

// 濡傛灉鏄痵tandby妯″紡鐨勮瘽锛孭A0鐨剋kup涓嶇敤閰?
// 閫氳??鍞ら啋瀵规繁搴︿紤鐪犱笉鑳借捣鏁堟灉銆?
void InitWakeUp_DeepMode(void)
{
	InitWakeUp_Base();
}

void delay(int n)
{
	while (n--)
		;
}

void IOstatus_Base(void)
{
	GPIO_InitTypeDef GPIO_InitStructure;

	RCC_AHBPeriphClockCmd(RCC_AHBPeriph_GPIOA, ENABLE); // 寮�鍚疓PIOA鐨勫?栬?炬椂閽?
	RCC_AHBPeriphClockCmd(RCC_AHBPeriph_GPIOB, ENABLE); // 寮�鍚疓PIOB鐨勫?栬?炬椂閽?
	RCC_AHBPeriphClockCmd(RCC_AHBPeriph_GPIOC, ENABLE); // 寮�鍚疓PIOC鐨勫?栬?炬椂閽?
	RCC_AHBPeriphClockCmd(RCC_AHBPeriph_GPIOF, ENABLE); // 寮�鍚疓PIOF鐨勫?栬?炬椂閽?

	ADC_DeInit(ADC1);

	GPIOA->PUPDR = 0;
	GPIOA->MODER = 0XFFFFFFFF;
	GPIOB->PUPDR = 0;
	GPIOB->MODER = 0XFFFFFFFF;
	GPIOC->PUPDR = 0;
	GPIOC->MODER = 0XFFFFFFFF;
	GPIOF->PUPDR = 0;
	GPIOF->MODER = 0XFFFFFFFF;

	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_15;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_OUT;
	GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_Level_1;
	GPIO_Init(GPIOB, &GPIO_InitStructure);
	GPIO_SetBits(GPIOB, GPIO_InitStructure.GPIO_Pin);

	__delay_ms(100);
}

void IOstatus_NormalMode(void)
{
	GPIO_InitTypeDef GPIO_InitStructure;

	{
		InitAFE1(0, 0);
		App_AFEshutdown();

		__delay_ms(500);
	}

	IOstatus_Base();

	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_15;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_OUT;
	GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_Level_1;
	GPIO_Init(GPIOB, &GPIO_InitStructure);
	GPIO_ResetBits(GPIOB, GPIO_InitStructure.GPIO_Pin);

	// 椹卞姩鍋滄??渚涚數
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_11;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_OUT;
	GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_Level_1;
	GPIO_Init(GPIOA, &GPIO_InitStructure);
	GPIO_SetBits(GPIOA, GPIO_InitStructure.GPIO_Pin);

	//	/* 璁〢FE1杩涘叆ship妯″紡 */
}

void IOstatus_RTCMode(void)
{
	GPIO_InitTypeDef GPIO_InitStructure;

	IOstatus_Base();

	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_15;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_OUT;
	GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_Level_1;
	GPIO_Init(GPIOB, &GPIO_InitStructure);
	GPIO_ResetBits(GPIOB, GPIO_InitStructure.GPIO_Pin);
}

void IOstatus_DeepMode(void)
{
	GPIO_InitTypeDef GPIO_InitStructure;

	{
		InitAFE1(0, 0);
		App_AFEshutdown();

		__delay_ms(500);
	}

	IOstatus_Base();

	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_15;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_OUT;
	GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_Level_1;
	GPIO_Init(GPIOB, &GPIO_InitStructure);
	GPIO_ResetBits(GPIOB, GPIO_InitStructure.GPIO_Pin);

	// 椹卞姩鍋滄??渚涚數
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_11;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_OUT;
	GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_Level_1;
	GPIO_Init(GPIOA, &GPIO_InitStructure);
	GPIO_SetBits(GPIOA, GPIO_InitStructure.GPIO_Pin);

	//	/* 璁〢FE1杩涘叆ship妯″紡 */
}

void IORecover_RTCMode(void)
{
	MCU_RESET();
}

void IORecover_NormalMode(void)
{
	// TIM_Cmd(TIM3, ENABLE);	//鐢ㄤ簬App_SleepTest()鍑芥暟
	MCU_RESET(); // 鐢变簬鐩存帴璧颁笅鍘诲?艰嚧鍚勭?嶅洜涓虹幇鍦虹牬鍧忔棤娉曡繘鍏ユ?ｅ父宸ヤ綔妯″紡锛屽畬缇庣殑瑙ｅ喅鍔炴硶鏄?澶嶄綅鍐嶆潵杩?
}

void IORecover_DeepMode(void)
{
	MCU_RESET();
}

// wkup涓嶇敤閰?
void Sys_StandbyMode(void)
{
	RCC_APB1PeriphClockCmd(RCC_APB1Periph_PWR, ENABLE); // 浣胯兘PWR澶栬?炬椂閽燂紝杩欏彞璇濇槸鍚﹂渶瑕侊紵030涓嶉渶瑕佷篃鑳借繘鍏ヤ紤鐪?
	// RCC_APB2PeriphResetCmd(0X01FC,DISABLE);				//澶嶄綅鎵�鏈塈O鍙?   //TODO
	PWR_WakeUpPinCmd(PWR_WakeUpPin_1, ENABLE); // 浣胯兘鍞ら啋绠¤剼鍔熻兘锛孭WR_CSR銆?
											   // 璇ュ紩鑴氫細琚?寮哄埗閰嶇疆涓轰笅鎷夎緭鍏ワ紝鎰忓懗鐫�涓嶉渶瑕侀厤缃?浜嗭紵

	PWR_ClearFlag(PWR_FLAG_WU); // Clear WUF bit in Power Control/Status register (PWR_CSR)
								// 娓匬WR_CR鐩稿叧渚胯兘娓呴櫎PWR_CSR
	PWR_EnterSTANDBYMode();		// 杩涘叆寰呭懡锛圫TANDBY锛夋ā寮忥紝PWR_CR    _PDDS
								// SCB->SCR璁剧疆涓篠LEEPDEEP = 1
}

// 030涔熸槸杩欐牱鍚?
void Sys_StopMode(void)
{
	// RCC_APB1PeriphClockCmd(RCC_APB1Periph_PWR, ENABLE);
	PWR_EnterSTOPMode(PWR_Regulator_LowPower, PWR_STOPEntry_WFI);

// 濡傛灉淇″彿鍒颁簡锛屾病娉曞敜閱掞紝鍗曠墖鏈哄亣姝荤姸鎬併�?
// 灏辨槸浠ヤ笅杩欐?佃瘽鎵ц?屽嚭闂?棰樹簡锛屽?栭儴鏅舵尟鍑洪棶棰?
#if (defined _HSE_8M_PLL_48M) || (defined _HSE_12M_PLL_48M)
	RCC_HSEConfig(RCC_HSE_ON); // 璧锋潵鍚庝細琚?鍒囨崲鍥濰SI
	while (RCC_GetFlagStatus(RCC_FLAG_HSERDY) == RESET)
		;				// 绛夊緟 HSE 鍑嗗?囧氨缁?
	RCC_PLLCmd(ENABLE); // 浣胯兘 PLL
	while (RCC_GetFlagStatus(RCC_FLAG_PLLRDY) == RESET)
		;									   // 绛夊緟 PLL 鍑嗗?囧氨缁?
	RCC_SYSCLKConfig(RCC_SYSCLKSource_PLLCLK); // 閫夋嫨PLL浣滀负绯荤粺鏃堕挓婧?
	while (RCC_GetSYSCLKSource() != 0x08)
		; // 绛夊緟PLL琚?閫夋嫨涓虹郴缁熸椂閽熸簮
#endif
}

void SleepDeal_Continue(void)
{
	UINT8 u8FlashWriteOK_flag = 0;
	static UINT8 s_u8SleepModeSelect = NORMAL_MODE;

	if (Sleep_Mode.bits.b1TestSleep)
	{
		s_u8SleepModeSelect = NORMAL_MODE;
	}
	else if (Sleep_Mode.bits.b1OverCurSleep)
	{
		s_u8SleepModeSelect = DEEP_MODE;
	}
	else if (Sleep_Mode.bits.b1OverVdeltaSleep)
	{
		s_u8SleepModeSelect = DEEP_MODE;
	}
	else if (Sleep_Mode.bits.b1CBCSleep)
	{
		s_u8SleepModeSelect = DEEP_MODE;
	}
	else if (Sleep_Mode.bits.b1ForceToSleep_L1)
	{
		s_u8SleepModeSelect = HICCUP_MODE;
	}
	else if (Sleep_Mode.bits.b1ForceToSleep_L2)
	{
		s_u8SleepModeSelect = NORMAL_MODE;
	}
	else if (Sleep_Mode.bits.b1ForceToSleep_L3)
	{
		s_u8SleepModeSelect = DEEP_MODE;
	}
	else if (Sleep_Mode.bits.b1VcellOVP)
	{
		// s_u8SleepModeSelect = HICCUP_MODE;
		s_u8SleepModeSelect = DEEP_MODE;
	}
	else if (Sleep_Mode.bits.b1VcellUVP)
	{
		// s_u8SleepModeSelect = HICCUP_MODE;
		s_u8SleepModeSelect = DEEP_MODE;
	}
	else if (Sleep_Mode.bits.b1NormalSleep_L1)
	{
		s_u8SleepModeSelect = HICCUP_MODE;
	}
	else if (Sleep_Mode.bits.b1NormalSleep_L2)
	{
		s_u8SleepModeSelect = NORMAL_MODE;
	}
	else if (Sleep_Mode.bits.b1NormalSleep_L3)
	{
		s_u8SleepModeSelect = DEEP_MODE;
	}
	else
	{
		s_u8SleepModeSelect = NORMAL_MODE;
	}

	switch (s_u8SleepModeSelect)
	{
	case NORMAL_MODE:
		if (FLASH_COMPLETE == FlashWriteOneHalfWord(FLASH_ADDR_SLEEP_FLAG, FLASH_NORMAL_SLEEP_VALUE))
		{
			u8FlashWriteOK_flag = 1;
		}
		break;
	case HICCUP_MODE:
		if (FLASH_COMPLETE == FlashWriteOneHalfWord(FLASH_ADDR_SLEEP_FLAG, FLASH_HICCUP_SLEEP_VALUE))
		{
			u8FlashWriteOK_flag = 1;
		}

		break;
	case DEEP_MODE:
		if (FLASH_COMPLETE == FlashWriteOneHalfWord(FLASH_ADDR_SLEEP_FLAG, FLASH_DEEP_SLEEP_VALUE))
		{
			u8FlashWriteOK_flag = 1;
		}
		break;
	default:
		// 涓嶈皟鏁村紩鑴氳繘鍏ヤ紤鐪狅紝鍔熻�椾細寰堝ぇ
		break;
	}

	if (u8FlashWriteOK_flag)
	{
		MCU_RESET();
	}
}

void SleepDeal_OverCurrent(void)
{
	static UINT8 s_u8SleepStatus = FIRST;
	static UINT32 s_u32SleepFirstCnt = 0;
	static UINT32 s_u32SleepHiccupCnt = 0;

	if (!Sleep_Mode.bits.b1OverCurSleep)
	{									   // 鍔犲己闆嶄綑璁捐??
		Sleep_Status = SLEEP_HICCUP_SHIFT; // 鍏跺疄杩欎釜鍙?浠ヤ笉瑕侊紝璁捐?¤?佹眰锛岄櫎浜嗚繖涓?鍑芥暟鍙?浠ユ妸杩欎釜鏍囧織浣嶅幓闄ゅ?栵紝鍒?鐨勫湴鏂逛笉鍙?浠ヤ究鍙?
		return;
	}

	switch (s_u8SleepStatus)
	{
	case FIRST:
		if (++s_u32SleepFirstCnt > 0)
		{ // 鐣欎笅浣嶇疆锛屽悗缁?绗?涓�娆″悗杩涘叆闇�瑕佸欢鏃跺垯杩欓噷鍔?
			s_u32SleepFirstCnt = 0;
			s_u8SleepStatus = HICCUP;
			Sleep_Status = SLEEP_HICCUP_CONTINUE;
		}
		break;

	case HICCUP:
		if (++s_u32SleepHiccupCnt > SleepInitOC)
		{
			s_u32SleepHiccupCnt = 0;
			Sleep_Status = SLEEP_HICCUP_CONTINUE;
		}
		break;

	default:
		s_u8SleepStatus = FIRST; // 涓嬩釜鍥炲悎鍐嶆潵
		break;
	}

	if (0)
	{										// 濡傛灉妫�娴嬪埌娌￠棶棰橈紝鍒欓��鍑轰紤鐪?
		Sleep_Mode.bits.b1OverCurSleep = 0; // 鏀惧埌switch璇?鍙ュ?栭潰锛孎IRST鍜孒ICCUP涓や釜閮芥湁鏁?
		Sleep_Status = SLEEP_HICCUP_SHIFT;
		s_u8SleepStatus = FIRST;
		if (s_u32SleepFirstCnt)
			s_u32SleepFirstCnt = 0;
		if (s_u32SleepHiccupCnt)
			s_u32SleepHiccupCnt = 0;
	}
}

void SleepDeal_VcellOVP(void)
{
}

void SleepDeal_VcellUVP(void)
{
	static UINT8 s_u8SleepStatus = FIRST;
	static UINT32 s_u32SleepFirstCnt = 0;
	static UINT32 s_u32SleepHiccupCnt = 0;

	if (!Sleep_Mode.bits.b1VcellUVP)
	{									   // 鍔犲己闆嶄綑璁捐??
		Sleep_Status = SLEEP_HICCUP_SHIFT; // 鍏跺疄杩欎釜鍙?浠ヤ笉瑕侊紝璁捐?¤?佹眰锛岄櫎浜嗚繖涓?鍑芥暟鍙?浠ユ妸杩欎釜鏍囧織浣嶅幓闄ゅ?栵紝鍒?鐨勫湴鏂逛笉鍙?浠ヤ究鍙?
		return;
	}

	switch (s_u8SleepStatus)
	{
	case FIRST:
		if (++s_u32SleepFirstCnt > 0)
		{ // 鐩存帴杩涘幓
			s_u32SleepFirstCnt = 0;
			s_u8SleepStatus = HICCUP;
			Sleep_Status = SLEEP_HICCUP_CONTINUE;
		}
		break;

	case HICCUP:
		if (++s_u32SleepHiccupCnt > 0)
		{
			s_u32SleepHiccupCnt = 0;
			Sleep_Status = SLEEP_HICCUP_CONTINUE;
		}
		break;

	default:
		s_u8SleepStatus = FIRST; // 涓嬩釜鍥炲悎鍐嶆潵
		break;
	}

	if (0)
	{ // 濡傛灉妫�娴嬪埌娌￠棶棰橈紝鍒欓��鍑轰紤鐪?
		// Sleep_Mode.bits.b1ForceToSleep_L2 = 0;
		// Sleep_Status = SLEEP_HICCUP_SHIFT;
		s_u8SleepStatus = FIRST; // 鐩存帴鍥炲埌绗?涓�娆★紝force鍙?鏈変竴娆★紝涓嶆槸鎵撳棟浼戠湢妯″紡
		if (s_u32SleepFirstCnt)
			s_u32SleepFirstCnt = 0;
		if (s_u32SleepHiccupCnt)
			s_u32SleepHiccupCnt = 0;
	}
}

void SleepDeal_Vdelta(void)
{
#if 0
	static UINT8 s_u8SleepStatus = FIRST;
	static UINT32 s_u32SleepFirstCnt = 0;
	static UINT32 s_u32SleepHiccupCnt = 0;

	if(!Sleep_Mode.bits.b1OverVdeltaSleep) {	//鍔犲己闆嶄綑璁捐??
		Sleep_Status = SLEEP_HICCUP_SHIFT;
		return ;
	}
#endif
}

void SleepDeal_Forced(void)
{
	static UINT8 s_u8SleepStatus = FIRST;
	static UINT32 s_u32SleepFirstCnt = 0;
	static UINT32 s_u32SleepHiccupCnt = 0;

	if (!Sleep_Mode.bits.b1ForceToSleep_L1 && Sleep_Mode.bits.b1ForceToSleep_L2 && Sleep_Mode.bits.b1ForceToSleep_L3)
	{									   // 鍔犲己闆嶄綑璁捐??
		Sleep_Status = SLEEP_HICCUP_SHIFT; // 鍏跺疄杩欎釜鍙?浠ヤ笉瑕侊紝璁捐?¤?佹眰锛岄櫎浜嗚繖涓?鍑芥暟鍙?浠ユ妸杩欎釜鏍囧織浣嶅幓闄ゅ?栵紝鍒?鐨勫湴鏂逛笉鍙?浠ヤ究鍙?
		return;
	}

	switch (s_u8SleepStatus)
	{
	case FIRST:
		if (++s_u32SleepFirstCnt > 0)
		{ // 鐩存帴杩涘幓
			s_u32SleepFirstCnt = 0;
			s_u8SleepStatus = HICCUP;
			Sleep_Status = SLEEP_HICCUP_CONTINUE;
		}
		break;

	case HICCUP:
		if (++s_u32SleepHiccupCnt > 0)
		{
			s_u32SleepHiccupCnt = 0;
			Sleep_Status = SLEEP_HICCUP_CONTINUE;
		}
		break;

	default:
		s_u8SleepStatus = FIRST; // 涓嬩釜鍥炲悎鍐嶆潵
		break;
	}

	if (0)
	{ // 濡傛灉妫�娴嬪埌娌￠棶棰橈紝鍒欓��鍑轰紤鐪?
		// Sleep_Mode.bits.b1ForceToSleep_L2 = 0;
		// Sleep_Status = SLEEP_HICCUP_SHIFT;
		s_u8SleepStatus = FIRST; // 鐩存帴鍥炲埌绗?涓�娆★紝force鍙?鏈変竴娆★紝涓嶆槸鎵撳棟浼戠湢妯″紡
		if (s_u32SleepFirstCnt)
			s_u32SleepFirstCnt = 0;
		if (s_u32SleepHiccupCnt)
			s_u32SleepHiccupCnt = 0;
	}
}

void SleepDeal_CBC(void)
{
	static UINT8 s_u8SleepStatus = FIRST;
	static UINT32 s_u32SleepFirstCnt = 0;
	static UINT32 s_u32SleepHiccupCnt = 0;

	if (!Sleep_Mode.bits.b1CBCSleep)
	{									   // 鍔犲己闆嶄綑璁捐??
		Sleep_Status = SLEEP_HICCUP_SHIFT; // 鍏跺疄杩欎釜鍙?浠ヤ笉瑕侊紝璁捐?¤?佹眰锛岄櫎浜嗚繖涓?鍑芥暟鍙?浠ユ妸杩欎釜鏍囧織浣嶅幓闄ゅ?栵紝鍒?鐨勫湴鏂逛笉鍙?浠ヤ究鍙?
		return;
	}

	switch (s_u8SleepStatus)
	{
	case FIRST:
		if (++s_u32SleepFirstCnt > 0)
		{ // 鐣欎笅浣嶇疆锛屽悗缁?绗?涓�娆″悗杩涘叆闇�瑕佸欢鏃跺垯杩欓噷鍔?
			s_u32SleepFirstCnt = 0;
			s_u8SleepStatus = HICCUP;
			Sleep_Status = SLEEP_HICCUP_CONTINUE;
		}
		break;

	case HICCUP:
		if (++s_u32SleepHiccupCnt > SleepInitCBC)
		{
			s_u32SleepHiccupCnt = 0;
			Sleep_Status = SLEEP_HICCUP_CONTINUE;
		}
		break;

	default:
		s_u8SleepStatus = FIRST; // 涓嬩釜鍥炲悎鍐嶆潵
		break;
	}

	if (0)
	{									// 濡傛灉妫�娴嬪埌娌￠棶棰橈紝鍒欓��鍑轰紤鐪?
		Sleep_Mode.bits.b1CBCSleep = 0; // 鏀惧埌switch璇?鍙ュ?栭潰锛孎IRST鍜孒ICCUP涓や釜閮芥湁鏁?
		// System_OnOFF_Func.bits.b1OnOFF_MOS_Relay = 1; 		//鍦ㄨ繖閲屽?嶅師鏄?鍚︽洿濂斤紵
		Sleep_Status = SLEEP_HICCUP_SHIFT;
		s_u8SleepStatus = FIRST;
		if (s_u32SleepFirstCnt)
			s_u32SleepFirstCnt = 0;
		if (s_u32SleepHiccupCnt)
			s_u32SleepHiccupCnt = 0;
	}
}

void SleepDeal_Normal_L1(void)
{
	static UINT8 s_u8SleepStatus = FIRST;
	static UINT32 s_u32SleepFirstCnt = 0;
	static UINT32 s_u32SleepHiccupCnt = 0;
	static UINT8 su8_SleepExtComCnt = 0;

	if ((Sleep_Mode.all & 0xFFF1) != 0)
	{ // 鏍稿績
		Sleep_Mode.bits.b1NormalSleep_L1 = 0;
		Sleep_Mode.bits.b1NormalSleep_L2 = 0;
		Sleep_Mode.bits.b1NormalSleep_L3 = 0;
		Sleep_Status = SLEEP_HICCUP_SHIFT;
		if (s_u32SleepFirstCnt)
			s_u32SleepFirstCnt = 0;
		if (s_u32SleepHiccupCnt)
			s_u32SleepHiccupCnt = 0;
		return;
	}

	if (su8_SleepExtComCnt != RTC_ExtComCnt)
	{
		su8_SleepExtComCnt = RTC_ExtComCnt;
		if (s_u32SleepFirstCnt)
			s_u32SleepFirstCnt = 0;
		if (s_u32SleepHiccupCnt)
			s_u32SleepHiccupCnt = 0;
	}

	switch (s_u8SleepStatus)
	{
	case FIRST:
		if (OtherElement.u16Sleep_TimeRTC == 0)
		{
			// 涓?0鏃堕粯璁?RTC涓嶈繘鍏ヤ紤鐪?
		}
		else
		{
			if (++s_u32SleepFirstCnt > (UINT32)OtherElement.u16Sleep_TimeRTC * 60)
			{
				// if(++s_u32SleepFirstCnt >= 5) {			//杩欎釜锛岀??涓�娆′釜鍚庨潰閮芥槸涓�鏍?
				s_u32SleepFirstCnt = 0;
				s_u8SleepStatus = HICCUP;
				Sleep_Status = SLEEP_HICCUP_CONTINUE;
			}
		}
		break;

	case HICCUP:
		if (++s_u32SleepHiccupCnt > (UINT32)OtherElement.u16Sleep_TimeRTC * 60)
		{
			s_u32SleepHiccupCnt = 0;
			Sleep_Status = SLEEP_HICCUP_CONTINUE;
		}
		break;

	default:
		s_u8SleepStatus = FIRST; // 涓嬩釜鍥炲悎鍐嶆潵
		break;
	}

	if (g_stCellInfoReport.u16Ichg > OtherElement.u16Sleep_VirCur_Chg || g_stCellInfoReport.u16IDischg > OtherElement.u16Sleep_VirCur_Dsg)
	{
		if (s_u32SleepFirstCnt)
			s_u32SleepFirstCnt = 0;
		if (s_u32SleepHiccupCnt)
			s_u32SleepHiccupCnt = 0;
	}

	if (g_stCellInfoReport.u16VCellMin <= OtherElement.u16Sleep_VNormal || g_stCellInfoReport.u16VCellMin <= OtherElement.u16Sleep_Vlow)
	{
		Sleep_Mode.bits.b1NormalSleep_L1 = 0;
		Sleep_Status = SLEEP_HICCUP_SHIFT;
		s_u8SleepStatus = FIRST;
		if (s_u32SleepFirstCnt)
			s_u32SleepFirstCnt = 0;
		if (s_u32SleepHiccupCnt)
			s_u32SleepHiccupCnt = 0;
	}
	// s_u32SleepFirstCnt = 0;		//杩樻病璋冨ソL1涓嶈繘鍏ヤ紤鐪犮�?
}

void SleepDeal_Normal_L2(void)
{
	static UINT8 s_u8SleepStatus = FIRST;
	static UINT32 s_u32SleepFirstCnt = 0;
	static UINT32 s_u32SleepHiccupCnt = 0;
	static UINT8 su8_SleepExtComCnt = 0;

	if ((Sleep_Mode.all & 0xFFF1) != 0)
	{
		Sleep_Mode.bits.b1NormalSleep_L1 = 0;
		Sleep_Mode.bits.b1NormalSleep_L2 = 0;
		Sleep_Mode.bits.b1NormalSleep_L3 = 0;
		Sleep_Status = SLEEP_HICCUP_SHIFT;
		if (s_u32SleepFirstCnt)
			s_u32SleepFirstCnt = 0;
		if (s_u32SleepHiccupCnt)
			s_u32SleepHiccupCnt = 0;
		return;
	}

	if (su8_SleepExtComCnt != RTC_ExtComCnt)
	{
		su8_SleepExtComCnt = RTC_ExtComCnt;
		if (s_u32SleepFirstCnt)
			s_u32SleepFirstCnt = 0;
		if (s_u32SleepHiccupCnt)
			s_u32SleepHiccupCnt = 0;
	}

	switch (s_u8SleepStatus)
	{
	case FIRST:
		if (++s_u32SleepFirstCnt > (UINT32)OtherElement.u16Sleep_TimeNormal * NORMAL_L2_SLEEP_UNIT)
		{
			// if(++s_u32SleepFirstCnt >= 3) {			//杩欎釜锛岀??涓�娆′釜鍚庨潰閮芥槸涓�鏍?
			s_u32SleepFirstCnt = 0;
			s_u8SleepStatus = HICCUP;
			Sleep_Status = SLEEP_HICCUP_CONTINUE;
		}
		break;

	case HICCUP:
		if (++s_u32SleepHiccupCnt > (UINT32)OtherElement.u16Sleep_TimeNormal * 60)
		{
			// if(++s_u32SleepHiccupCnt >= 1) {
			s_u32SleepHiccupCnt = 0;
			Sleep_Status = SLEEP_HICCUP_CONTINUE;
		}
		break;

	default:
		s_u8SleepStatus = FIRST; // 涓嬩釜鍥炲悎鍐嶆潵
		break;
	}

	if (g_stCellInfoReport.u16Ichg > OtherElement.u16Sleep_VirCur_Chg || g_stCellInfoReport.u16IDischg > OtherElement.u16Sleep_VirCur_Dsg)
	{
		if (s_u32SleepFirstCnt)
			s_u32SleepFirstCnt = 0;
		if (s_u32SleepHiccupCnt)
			s_u32SleepHiccupCnt = 0;
	}

	if (g_stCellInfoReport.u16VCellMin < OtherElement.u16Sleep_Vlow || g_stCellInfoReport.u16VCellMin > OtherElement.u16Sleep_VNormal)
	{ // 瑙﹀彂鏉′欢鎵嶈烦杞?锛屽埆鐨勬椂闂翠笉璺宠浆
		Sleep_Mode.bits.b1NormalSleep_L2 = 0;
		Sleep_Status = SLEEP_HICCUP_SHIFT;
		s_u8SleepStatus = FIRST;
		if (s_u32SleepFirstCnt)
			s_u32SleepFirstCnt = 0;
		if (s_u32SleepHiccupCnt)
			s_u32SleepHiccupCnt = 0;
	}
}

void SleepDeal_Normal_L3(void)
{
	static UINT8 s_u8SleepStatus = FIRST;
	static UINT32 s_u32SleepFirstCnt = 0;
	static UINT32 s_u32SleepHiccupCnt = 0;
	// static UINT8 su8_SleepExtComCnt = 0;

	if ((Sleep_Mode.all & 0xFFF1) != 0)
	{
		Sleep_Mode.bits.b1NormalSleep_L1 = 0;
		Sleep_Mode.bits.b1NormalSleep_L2 = 0;
		Sleep_Mode.bits.b1NormalSleep_L3 = 0;
		Sleep_Status = SLEEP_HICCUP_SHIFT;
		if (s_u32SleepFirstCnt)
			s_u32SleepFirstCnt = 0;
		if (s_u32SleepHiccupCnt)
			s_u32SleepHiccupCnt = 0;
		return;
	}

#if 0
	if(su8_SleepExtComCnt != RTC_ExtComCnt) {
		su8_SleepExtComCnt = RTC_ExtComCnt;
		if(s_u32SleepFirstCnt)s_u32SleepFirstCnt = 0;
		if(s_u32SleepHiccupCnt)s_u32SleepHiccupCnt = 0;
	}
#endif

	switch (s_u8SleepStatus)
	{
	case FIRST:
		if (++s_u32SleepFirstCnt > (UINT32)OtherElement.u16Sleep_TimeVlow * NORMAL_L3_SLEEP_UNIT)
		{
			// if(++s_u32SleepFirstCnt >= 1) {			//杩欎釜锛岀??涓�娆′釜鍚庨潰閮芥槸涓�鏍?
			s_u32SleepFirstCnt = 0;
			s_u8SleepStatus = HICCUP;
			Sleep_Status = SLEEP_HICCUP_CONTINUE;
		}
		break;

	case HICCUP:
		if (++s_u32SleepHiccupCnt > (UINT32)OtherElement.u16Sleep_TimeVlow * 60)
		{
			// if(++s_u32SleepHiccupCnt >= 1) {
			s_u32SleepHiccupCnt = 0;
			Sleep_Status = SLEEP_HICCUP_CONTINUE;
		}
		break;

	default:
		s_u8SleepStatus = FIRST; // 涓嬩釜鍥炲悎鍐嶆潵
		break;
	}

	if (g_stCellInfoReport.u16Ichg > OtherElement.u16Sleep_VirCur_Chg || g_stCellInfoReport.u16IDischg > OtherElement.u16Sleep_VirCur_Dsg)
	{
		if (s_u32SleepFirstCnt)
			s_u32SleepFirstCnt = 0;
		if (s_u32SleepHiccupCnt)
			s_u32SleepHiccupCnt = 0;
	}

	if (g_stCellInfoReport.u16VCellMin >= OtherElement.u16Sleep_Vlow)
	{ // 瑙﹀彂鏉′欢鎵嶈烦杞?锛屽埆鐨勬椂闂翠笉璺宠浆
		Sleep_Mode.bits.b1NormalSleep_L3 = 0;
		Sleep_Status = SLEEP_HICCUP_SHIFT;
		s_u8SleepStatus = FIRST;
		if (s_u32SleepFirstCnt)
			s_u32SleepFirstCnt = 0;
		if (s_u32SleepHiccupCnt)
			s_u32SleepHiccupCnt = 0;
	}
}

// 杩欎釜鍦版柟锛孖O鎺у埗绛栫暐瑕佹敼涓�涓嬶紝璧锋潵寤舵椂1s鍐嶆墦寮�绠″瓙浼氫笉浼氭洿濂斤紵涓嶈繃鐜拌薄璨屼技鐩存帴鎵撳紑娌￠棶棰?
// 杩欎釜浣滀负涓诲惊鐜?锛屽?傛灉寮�澶村垽鏂?鍑虹幇浜嗗埆鐨勯敊璇?锛屽垯璺冲嚭涓诲惊鐜?锛屽幓鎵ц?屽埆鐨?
// 鍏充簬杩欓噷鍜孖O鎺у埗涓诲嚱鏁扮殑閫昏緫闂?棰橈紝A锛屾渶寮�澶村叧浜嶴leep鐨剅eturn闂?棰樸�侭锛屼紤鐪犺捣鏉?IO鏄?鍚︾珛鍒绘墦寮�鐨勯棶棰?
void SleepDeal_Normal_Select(void)
{
	if ((Sleep_Mode.all & 0xFFF1) != 0)
	{ // 鏍稿績
		Sleep_Mode.bits.b1NormalSleep_L1 = 0;
		Sleep_Mode.bits.b1NormalSleep_L2 = 0;
		Sleep_Mode.bits.b1NormalSleep_L3 = 0;
		Sleep_Status = SLEEP_HICCUP_SHIFT;
		return;
	}

	if (g_stCellInfoReport.u16Ichg <= OtherElement.u16Sleep_VirCur_Chg && g_stCellInfoReport.u16IDischg <= OtherElement.u16Sleep_VirCur_Chg)
	{
		if (g_stCellInfoReport.u16VCellMin < OtherElement.u16Sleep_Vlow)
		{
			Sleep_Mode.bits.b1NormalSleep_L3 = 1;
			Sleep_Status = SLEEP_HICCUP_NORMAL_L3;
		}
		else if (g_stCellInfoReport.u16VCellMin > OtherElement.u16Sleep_VNormal)
		{
			Sleep_Mode.bits.b1NormalSleep_L1 = 1;
			Sleep_Status = SLEEP_HICCUP_NORMAL_L1;
		}
		else
		{ // 绛夊彿鍧囩撼鍏?L2
			Sleep_Mode.bits.b1NormalSleep_L2 = 1;
			Sleep_Status = SLEEP_HICCUP_NORMAL_L2;
		}
	}
	else
	{
		// 鏈夌數娴佸垯缁х画鍦ㄨ繖涓?鍑芥暟寰?鐜?
	}
}

// 鏋舵瀯鍐冲畾瑕佹敼涓�鏀癸紝涓嶇劧鍚庢湡浜哄憳澶?闅剧淮鎶や簡
void SleepDeal_Shift(void)
{
	if (Sleep_Mode.bits.b1TestSleep != 0)
	{
		Sleep_Status = SLEEP_HICCUP_TEST;
	}
	else if (Sleep_Mode.bits.b1OverCurSleep != 0)
	{
		// Sleep_Status = SLEEP_HICCUP_CONTINUE;			//鏋舵瀯宸叉敼锛屽厛璺冲埌鐩稿叧鍑芥暟锛屽啀杩涘叆浼戠湢
		Sleep_Status = SLEEP_HICCUP_OVERCUR;
	}
	else if (Sleep_Mode.bits.b1OverVdeltaSleep != 0)
	{
		Sleep_Status = SLEEP_HICCUP_OVDELTA;
	}
	else if (Sleep_Mode.bits.b1CBCSleep != 0)
	{
		Sleep_Status = SLEEP_HICCUP_CBC;
	}
	else if (Sleep_Mode.bits.b1ForceToSleep_L1 != 0)
	{
		Sleep_Status = SLEEP_HICCUP_FORCED;
	}
	else if (Sleep_Mode.bits.b1ForceToSleep_L2 != 0)
	{
		Sleep_Status = SLEEP_HICCUP_FORCED;
	}
	else if (Sleep_Mode.bits.b1ForceToSleep_L3 != 0)
	{
		Sleep_Status = SLEEP_HICCUP_FORCED;
	}

	else if (Sleep_Mode.bits.b1VcellOVP != 0)
	{
		Sleep_Status = SLEEP_HICCUP_VCELLOVP;
	}
	else if (Sleep_Mode.bits.b1VcellUVP != 0)
	{
		Sleep_Status = SLEEP_HICCUP_VCELLUVP;
	}
	else
	{ // 娌℃湁浠ヤ笂鍚勭?嶄繚鎶ょ洿鎺ヨ繘鍏ヤ富寰?鐜?
		Sleep_Status = SLEEP_HICCUP_NORMAL_SELECT;
	}
}

void SleepDeal_Test(void)
{
	static UINT16 s_u16HaltTestCnt = 0;
	if (!Sleep_Mode.bits.b1TestSleep)
	{ // 鍔犲己闆嶄綑璁捐??
		Sleep_Status = SLEEP_HICCUP_SHIFT;
		return;
	}

	if (++s_u16HaltTestCnt >= 2)
	{ // 10s鈥斺�擳est
		s_u16HaltTestCnt = 0;
		Sleep_Status = SLEEP_HICCUP_CONTINUE;
	}
}

void IsSleepStartUp(void)
{
	switch (FlashReadOneHalfWord(FLASH_ADDR_SLEEP_FLAG))
	{
	case FLASH_HICCUP_SLEEP_VALUE:
		if (FLASH_COMPLETE == FlashWriteOneHalfWord(FLASH_ADDR_SLEEP_FLAG, FLASH_SLEEP_RESET_VALUE))
		{
			InitIO();
			InitDelay();
			InitSystemWakeUp();
			// Storage_Init() is handled on normal startup; sleep recovery should not reinitialize parameter storage here.
			Init_RTC();

			IOstatus_RTCMode();
			InitWakeUp_RTCMode();

			Sys_StopMode();
			// Sys_StandbyMode();
			IORecover_RTCMode();
		}
		break;
	case FLASH_NORMAL_SLEEP_VALUE:
		if (FLASH_COMPLETE == FlashWriteOneHalfWord(FLASH_ADDR_SLEEP_FLAG, FLASH_SLEEP_RESET_VALUE))
		{
			InitIO();
			InitDelay();
			InitSystemWakeUp(); // 鎵嬪姩鍏抽棴AFE闇�瑕佸仛鐨?

			IOstatus_NormalMode();
			InitWakeUp_NormalMode();
			Sys_StopMode();
			IORecover_NormalMode();
		}
		break;
	case FLASH_DEEP_SLEEP_VALUE:
		if (FLASH_COMPLETE == FlashWriteOneHalfWord(FLASH_ADDR_SLEEP_FLAG, FLASH_SLEEP_RESET_VALUE))
		{
			InitIO();
			InitDelay();
			InitSystemWakeUp(); // 鎵嬪姩鍏抽棴AFE闇�瑕佸仛鐨?

			IOstatus_DeepMode();
			InitWakeUp_DeepMode();
			// Sys_StandbyMode();		//涓嶈兘鎺屾帶澶栭儴IO锛屽純鐢?
			Sys_StopMode();
			IORecover_DeepMode();
		}
		break;
	case FLASH_SLEEP_RESET_VALUE:
		// 涓嶄綔澶勭悊
		break;
	default:
		break;
	}
}

// 鍚勭?嶇姸鍐佃繘鍏ヤ紤鐪?(澶氫釜寰?鐜?锛屼緥濡傛?ｅ父浼戠湢寰?鐜?锛孋BC浼戠湢寰?鐜?锛屽帇宸?杩囧ぇ淇濇姢寰?鐜?)
// 閫氳繃SleepDeal_NormalQuit()-->涓绘帶璺宠浆鍑芥暟锛岃烦鍒板埆鐨勫惊鐜?
// Sleep_Mode鏍囧織浣嶆槸杩涘叆鍝?绉嶄紤鐪犲惊鐜?锛屽?栭儴鍐冲畾锛屽嚭鐜板埆鐨勪紤鐪犵姸鍐碉紝绔嬪埢璺宠繃鍘诲厛杩涘叆浼戠湢锛岀劧鍚庤嚜鍔ㄥ敜閱掞紝鍐嶅洖鍒扮浉鍏冲嚱鏁板惊鐜?銆?
// 鎵�浠ワ紝娴佺▼鏄?锛?
// Sleep_Mode鏍囧織-->SleepDeal_Normal(姝ｅ父寰?鐜?)-->SleepDeal_NormalQuit(璺宠浆)-->SleepDeal_Continue(浼戠湢)-->鍞ら啋杩涘叆鐩稿叧寰?鐜?鍑芥暟
// 浠ヤ笂鏋舵瀯鍥犲お杩囧?嶆潅澶?杩囬毦浠ヨ??鍚庣画浜哄憳缁存姢锛屼笉澶?濂戝悎瀹為檯娴佺▼锛屽凡琚?淇?鏀逛负濡備笅銆?
// 鍞ら啋杩涘叆鐩稿叧寰?鐜?鍑芥暟锛屽惈鏈夌??涓�娆?FIRST鍜屽悗缁璈ICCUP妯″紡杩涘叆涓ょ?嶆儏鍐碉紝鎵�浠ョ??涓�娆¤兘绔嬪埢杩涘叆锛岀??浜屾?″紑濮嬫墦鍡濊繘鍏?
// Sleep_Mode鏍囧織-->SleepDeal_Normal(姝ｅ父寰?鐜?)-->SleepDeal_NormalQuit(璺宠浆)-->鍞ら啋杩涘叆鐩稿叧寰?鐜?鍑芥暟-->SleepDeal_Continue(浼戠湢)
void App_SleepDeal(void)
{
	if (!System_OnOFF_Func.bits.b1OnOFF_Sleep)
	{			// 鏈変釜鐤戦棶锛屾槸涓嶆槸绔嬪埢鍏充簡锛屼笉闇�瑕佸?嶅師鍛?锛屽潎琛℃槸闇�瑕佸叧鎺夊?嶅師銆?
		return; // Sleep鐨勮瘽锛屽?傛灉鐩存帴涓嶈繘鍘伙紝鍚庣画鎵撳紑浼氭帴鐫�涓婃?＄殑姝ヤ紣
	}			// 鏆備笖鍏堣繖涔堝仛锛屽悗缁?濡傛灉瑕佸叏鐩樺?嶅師锛岃?℃椂娓呴浂鍐嶈?达紝鐩?鍓嶆槸鎺ョ潃涓婃?＄殑姝ヤ紣

	if (reset_sleep_state)
	{
		reset_sleep_state = 0;
		Sleep_Status = SLEEP_HICCUP_SHIFT;

		Sleep_Mode.all = 0;
	}

	if (SystemStatus.bits.b1StartUpBMS)
	{ // 寮�鏈哄畬姣曞啀杩涘叆
		return;
	}
	else
	{
		SystemStatus.bits.b1Status_ToSleep = 1;
	}

// #ifndef __FUNC_RTC__
	if (Sleep_Mode.bits.b1_ToSleepFlag)
	{
		LogRecord_Flag.bits.Log_Sleep = 1;
		return;
	}
// #endif

	if (0 == g_st_SysTimeFlag.bits.b1Sys1000msFlag1 && !Sleep_Mode.bits.b1ForceToSleep_L1 && !Sleep_Mode.bits.b1ForceToSleep_L2 && !Sleep_Mode.bits.b1ForceToSleep_L3)
	{
		return; // 濡傛灉鏄?寮哄埗杩涘叆浼戠湢鐨勫垯蹇呴』蹇?鐐硅繘鍏ヤ紤鐪狅紝涓嶈兘鎷?
	}

	switch (Sleep_Status)
	{
	case SLEEP_HICCUP_SHIFT: // 鍏堣烦鍒拌繖閲岋紝鍐嶈烦鍒癝leepDeal_Continue()锛岀劧鍚庤繘鍏ュ埆鐨勫惊鐜?
		SleepDeal_Shift();	 // 涓绘帶璺宠浆鍑芥暟锛屽紑鏈烘墽琛屼竴閬嶆病浜嬭繘鍏ユ牳蹇冨惊鐜?鍑芥暟
		break;
	case SLEEP_HICCUP_NORMAL_SELECT:
		SleepDeal_Normal_Select();
		break;
	case SLEEP_HICCUP_TEST:
		SleepDeal_Test();
		break;
	case SLEEP_HICCUP_OVERCUR:
		SleepDeal_OverCurrent();
		break;
	case SLEEP_HICCUP_OVDELTA:
		SleepDeal_Vdelta(); // 鐩?鍓嶅帇宸?杩囧ぇ鐩存帴杩涘叆浼戠湢涓嶈捣鏉ワ紝浜?涓?鐏?
		break;
	case SLEEP_HICCUP_CBC:
		SleepDeal_CBC();
		break;
	case SLEEP_HICCUP_FORCED:
		SleepDeal_Forced(); // 杩樻病鍐?
		break;
	case SLEEP_HICCUP_NORMAL_L1:
		SleepDeal_Normal_L1();
		break;
	case SLEEP_HICCUP_NORMAL_L2:
		SleepDeal_Normal_L2();
		break;
	case SLEEP_HICCUP_NORMAL_L3:
		SleepDeal_Normal_L3();
		break;

	case SLEEP_HICCUP_VCELLOVP:
		SleepDeal_VcellOVP();
		break;
	case SLEEP_HICCUP_VCELLUVP:
		SleepDeal_VcellUVP();
		break;

	case SLEEP_HICCUP_CONTINUE:
		SleepDeal_Continue();
		break;
	default:
		Sleep_Status = SLEEP_HICCUP_SHIFT;
		break;
	}

	if (SLEEP_HICCUP_CONTINUE == Sleep_Status)
	{
		Sleep_Mode.bits.b1_ToSleepFlag = 1;
	}
	else
	{
		Sleep_Mode.bits.b1_ToSleepFlag = 0;
	}
}

void IOstatus_TestMode(void)
{
	IOstatus_NormalMode();
}

void InitWakeUp_TestMode(void)
{
	InitWakeUp_NormalMode();
}

void IORecover_TestMode(void)
{
	MCU_RESET();
}

void App_NormalSleepTest(void)
{
	static UINT16 s_u16HaltTestCnt = 0;

	if (0 == g_st_SysTimeFlag.bits.b1Sys1000msFlag1)
	{ // 浼戠湢璧锋潵绛夊緟绯荤粺鍒濆?嬪寲瀹屾垚
		return;
	}

	if (++s_u16HaltTestCnt >= 5)
	{ // 10s鈥斺�擳est
		s_u16HaltTestCnt = 0;
		IOstatus_TestMode();
		InitWakeUp_TestMode();
		TIM_Cmd(TIM17, DISABLE); //
		Sys_StopMode();
		// Sys_StandbyMode();
		IORecover_TestMode();
	}
}

void Sys_SleepOnExitMode(void)
{
	NVIC_SystemLPConfig(NVIC_LP_SLEEPONEXIT, ENABLE); // 搴撳嚱鏁扮増鏈?锛岃?剧疆SLEEP ON EXIT浣嶄负1
	// SCB->SCR|=1<<1;//瀵勫瓨鍣ㄧ増鏈?锛岃?剧疆SLEEP ON EXIT浣嶄负1
	__ASM volatile("wfi");
}

void App_RTCSleepTest(void)
{
	static UINT16 s_u16HaltTestCnt = 0;

	if (0 == g_st_SysTimeFlag.bits.b1Sys200msFlag1)
	{ // 浼戠湢璧锋潵绛夊緟绯荤粺鍒濆?嬪寲瀹屾垚
		return;
	}

	if (++s_u16HaltTestCnt >= 3)
	{ // 10s鈥斺�擳est
		s_u16HaltTestCnt = 0;
		IOstatus_RTCMode();
		InitWakeUp_RTCMode();
		Sys_StopMode();
		IORecover_RTCMode();
	}
}
