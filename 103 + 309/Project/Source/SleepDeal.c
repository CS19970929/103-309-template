#include "main.h"
#include "LedSnapshot.h"
#include "SleepWakeFastUi.h"
// #include "SleepWakeFastUi.h"

volatile union SLEEP_MODE Sleep_Mode; // 用于外部控制进入休眠标志�?
enum SLEEP_STATUS Sleep_Status = SLEEP_HICCUP_SHIFT;

UINT8 gu8_SleepStatus = 0;
UINT8 RTC_ExtComCnt = 0;
uint8_t reset_sleep_state = 0;

// use BKP instead of Flash for the one-shot sleep resume flag.
#define SLEEP_BKP_FLAG_REG BKP_DR6
#define SLEEP_BKP_FLAG_INV_REG BKP_DR7

static UINT16 SleepDeal_ModeToFlag(UINT8 mode);

static UINT8 SleepDeal_IsValidSleepFlag(UINT16 flag)
{
	return (flag == FLASH_NORMAL_SLEEP_VALUE) ||
		   (flag == FLASH_HICCUP_SLEEP_VALUE) ||
		   (flag == FLASH_DEEP_SLEEP_VALUE) ||
		   (flag == FLASH_SLEEP_RESET_VALUE);
}

static void SleepDeal_EnableBackupAccess(void)
{
	RCC_APB1PeriphClockCmd(RCC_APB1Periph_PWR | RCC_APB1Periph_BKP, ENABLE);
	PWR_BackupAccessCmd(ENABLE);
}

static UINT16 SleepDeal_ModeToFlag(UINT8 mode)
{
	if (mode == HICCUP_MODE)
	{
		return FLASH_HICCUP_SLEEP_VALUE;
	}
	if (mode == DEEP_MODE)
	{
		return FLASH_DEEP_SLEEP_VALUE;
	}
	return FLASH_NORMAL_SLEEP_VALUE;
}

UINT8 SleepDeal_SaveSleepModeFlag(UINT16 flag)
{
	UINT16 flag_inv;
	UINT16 read_flag;
	UINT16 read_flag_inv;

	flag_inv = (UINT16)(~flag);
	SleepDeal_EnableBackupAccess();
	BKP_WriteBackupRegister(SLEEP_BKP_FLAG_REG, flag);
	BKP_WriteBackupRegister(SLEEP_BKP_FLAG_INV_REG, flag_inv);

	read_flag = BKP_ReadBackupRegister(SLEEP_BKP_FLAG_REG);
	read_flag_inv = BKP_ReadBackupRegister(SLEEP_BKP_FLAG_INV_REG);

	return (UINT8)((read_flag == flag) && (read_flag_inv == flag_inv));
}

UINT16 SleepDeal_LoadSleepModeFlag(void)
{
	UINT16 flag;
	UINT16 flag_inv;

	SleepDeal_EnableBackupAccess();
	flag = BKP_ReadBackupRegister(SLEEP_BKP_FLAG_REG);
	flag_inv = BKP_ReadBackupRegister(SLEEP_BKP_FLAG_INV_REG);

	if ((flag != (UINT16)(~flag_inv)) || (!SleepDeal_IsValidSleepFlag(flag)))
	{
		(void)SleepDeal_SaveSleepModeFlag(FLASH_SLEEP_RESET_VALUE);
		return FLASH_SLEEP_RESET_VALUE;
	}

	return flag;
}

void SleepDeal_ClearSleepModeFlag(void)
{
	(void)SleepDeal_SaveSleepModeFlag(FLASH_SLEEP_RESET_VALUE);
}

void SleepDeal_Continue(void)
{
	LedSnapshot_SaveRuntime();

	if (SleepDeal_SaveSleepModeFlag(FLASH_DEEP_SLEEP_VALUE))
	{
		InitAFE1_Sleep(0);
		AFE_Sleep();
		MCU_RESET();
	}
}

void SleepDeal_OverCurrent(void)
{
	static UINT8 s_u8SleepStatus = FIRST;
	static UINT32 s_u32SleepFirstCnt = 0;
	static UINT32 s_u32SleepHiccupCnt = 0;

	if (!Sleep_Mode.bits.b1OverCurSleep)
	{									   // 加强雍余设�??
		Sleep_Status = SLEEP_HICCUP_SHIFT; // 其实这个�?以不要，设�?��?�求，除了这�?函数�?以把这个标志位去除�?�，�?的地方不�?以便�?
		return;
	}

	switch (s_u8SleepStatus)
	{
	case FIRST:
		if (++s_u32SleepFirstCnt > 0)
		{ // 留下位置，后�?�?一次后进入需要延时则这里�?
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
		s_u8SleepStatus = FIRST; // 下个回合再来
		break;
	}

	if (0)
	{										// 如果检测到没问题，则退出休�?
		Sleep_Mode.bits.b1OverCurSleep = 0; // 放到switch�?句�?�面，FIRST和HICCUP两个都有�?
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
	{									   // 加强雍余设�??
		Sleep_Status = SLEEP_HICCUP_SHIFT; // 其实这个�?以不要，设�?��?�求，除了这�?函数�?以把这个标志位去除�?�，�?的地方不�?以便�?
		return;
	}

	switch (s_u8SleepStatus)
	{
	case FIRST:
		if (++s_u32SleepFirstCnt > 0)
		{ // 直接进去
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
		s_u8SleepStatus = FIRST; // 下个回合再来
		break;
	}

	if (0)
	{ // 如果检测到没问题，则退出休�?
		// Sleep_Mode.bits.b1ForceToSleep_L2 = 0;
		// Sleep_Status = SLEEP_HICCUP_SHIFT;
		s_u8SleepStatus = FIRST; // 直接回到�?一次，force�?有一次，不是打嗝休眠模式
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
	
	if(!Sleep_Mode.bits.b1OverVdeltaSleep) {	//加强雍余设�??
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
	{									   // 加强雍余设�??
		Sleep_Status = SLEEP_HICCUP_SHIFT; // 其实这个�?以不要，设�?��?�求，除了这�?函数�?以把这个标志位去除�?�，�?的地方不�?以便�?
		return;
	}

	switch (s_u8SleepStatus)
	{
	case FIRST:
		if (++s_u32SleepFirstCnt > 0)
		{ // 直接进去
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
		s_u8SleepStatus = FIRST; // 下个回合再来
		break;
	}

	if (0)
	{ // 如果检测到没问题，则退出休�?
		// Sleep_Mode.bits.b1ForceToSleep_L2 = 0;
		// Sleep_Status = SLEEP_HICCUP_SHIFT;
		s_u8SleepStatus = FIRST; // 直接回到�?一次，force�?有一次，不是打嗝休眠模式
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
	{									   // 加强雍余设�??
		Sleep_Status = SLEEP_HICCUP_SHIFT; // 其实这个�?以不要，设�?��?�求，除了这�?函数�?以把这个标志位去除�?�，�?的地方不�?以便�?
		return;
	}

	switch (s_u8SleepStatus)
	{
	case FIRST:
		if (++s_u32SleepFirstCnt > 0)
		{ // 留下位置，后�?�?一次后进入需要延时则这里�?
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
		s_u8SleepStatus = FIRST; // 下个回合再来
		break;
	}

	if (0)
	{									// 如果检测到没问题，则退出休�?
		Sleep_Mode.bits.b1CBCSleep = 0; // 放到switch�?句�?�面，FIRST和HICCUP两个都有�?
		// System_OnOFF_Func.bits.b1OnOFF_MOS_Relay = 1; 		//在这里�?�原�?否更好？
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
#if 0
#if !defined(__FUNC_RTC__)
	static UINT8 s_u8SleepStatus = FIRST;
	static UINT32 s_u32SleepFirstCnt = 0;
	static UINT32 s_u32SleepHiccupCnt = 0;
	static UINT8 su8_SleepExtComCnt = 0;

	if ((Sleep_Mode.all & 0xFFF1) != 0)
	{ // 核心
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
			// �?0时默�?RTC不进入休�?
		}
		else
		{
			if (++s_u32SleepFirstCnt > (UINT32)OtherElement.u16Sleep_TimeRTC * 60)
			{
				// if(++s_u32SleepFirstCnt >= 5) {			//这个，�??一次个后面都是一�?
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
		s_u8SleepStatus = FIRST; // 下个回合再来
		break;
	}

	if (g_stCellInfoReport.u16Ichg > OtherElement.u16Sleep_VirCur_Chg || g_stCellInfoReport.u16IDischg > OtherElement.u16Sleep_VirCur_Dsg)
	{
		if (s_u32SleepFirstCnt)
			s_u32SleepFirstCnt = 0;
		if (s_u32SleepHiccupCnt)
			s_u32SleepHiccupCnt = 0;
	}

	if (g_stCellInfoReport.u16VCellMin <= OtherElement.u16Sleep_VNormal)
	{
		Sleep_Mode.bits.b1NormalSleep_L1 = 0;
		Sleep_Status = SLEEP_HICCUP_SHIFT;
		s_u8SleepStatus = FIRST;
		if (s_u32SleepFirstCnt)
			s_u32SleepFirstCnt = 0;
		if (s_u32SleepHiccupCnt)
			s_u32SleepHiccupCnt = 0;
	}
#endif
	// s_u32SleepFirstCnt = 0;		//还没调好L1不进入休眠�?
#endif
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

	if (++s_u32SleepFirstCnt > (UINT32)OtherElement.u16Sleep_TimeNormal * 60)
	{
		// if(++s_u32SleepFirstCnt >= 3) {			//这个，�??一次个后面都是一�?
		s_u32SleepFirstCnt = 0;
		s_u8SleepStatus = HICCUP;
		Sleep_Mode.bits.b1NormalSleep_L2 = 1;
		Sleep_Status = SLEEP_HICCUP_CONTINUE;
	}

	if (g_stCellInfoReport.u16Ichg > OtherElement.u16Sleep_VirCur_Chg || g_stCellInfoReport.u16IDischg > OtherElement.u16Sleep_VirCur_Dsg)
	{
		if (s_u32SleepFirstCnt)
			s_u32SleepFirstCnt = 0;
		if (s_u32SleepHiccupCnt)
			s_u32SleepHiccupCnt = 0;
	}

	// if (g_stCellInfoReport.u16VCellMin < OtherElement.u16Sleep_Vlow || g_stCellInfoReport.u16VCellMin > OtherElement.u16Sleep_VNormal)
	if (g_stCellInfoReport.u16VCellMin < OtherElement.u16Sleep_Vlow)
	{ // 触发条件才跳�?，别的时间不跳转
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

	if (++s_u32SleepFirstCnt > (UINT32)OtherElement.u16Sleep_TimeVlow * 60)
	{
		// if(++s_u32SleepFirstCnt >= 1) {			//这个，�??一次个后面都是一�?
		s_u32SleepFirstCnt = 0;
		s_u8SleepStatus = HICCUP;
		Sleep_Mode.bits.b1NormalSleep_L3 = 1;
		Sleep_Status = SLEEP_HICCUP_CONTINUE;
	}

	if (g_stCellInfoReport.u16Ichg > OtherElement.u16Sleep_VirCur_Chg || g_stCellInfoReport.u16IDischg > OtherElement.u16Sleep_VirCur_Dsg)
	{
		if (s_u32SleepFirstCnt)
			s_u32SleepFirstCnt = 0;
		if (s_u32SleepHiccupCnt)
			s_u32SleepHiccupCnt = 0;
	}

	if (g_stCellInfoReport.u16VCellMin >= OtherElement.u16Sleep_Vlow)
	{ // 触发条件才跳�?，别的时间不跳转
		Sleep_Mode.bits.b1NormalSleep_L3 = 0;
		Sleep_Status = SLEEP_HICCUP_SHIFT;
		s_u8SleepStatus = FIRST;
		if (s_u32SleepFirstCnt)
			s_u32SleepFirstCnt = 0;
		if (s_u32SleepHiccupCnt)
			s_u32SleepHiccupCnt = 0;
	}
}

// 这个地方，IO控制策略要改一下，起来延时1s再打开管子会不会更好？不过现象貌似直接打开没问�?
// 这个作为主循�?，�?�果开头判�?出现了别的错�?，则跳出主循�?，去执�?�别�?
// 关于这里和IO控制主函数的逻辑�?题，A，最开头关于Sleep的return�?题。B，休眠起�?IO�?否立刻打开的问�?
void SleepDeal_Normal_Select(void)
{
	if ((Sleep_Mode.all & 0xFFF1) != 0)
	{ // 核心
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
			Sleep_Mode.bits.b1NormalSleep_L3 = 0;
			Sleep_Status = SLEEP_HICCUP_NORMAL_L3;
		}
		// else if (g_stCellInfoReport.u16VCellMin > OtherElement.u16Sleep_VNormal)
		// {
		// 	Sleep_Mode.bits.b1NormalSleep_L1 = 1;
		// 	Sleep_Status = SLEEP_HICCUP_NORMAL_L1;
		// }
		else
		{ // 等号均纳�?L2
			Sleep_Mode.bits.b1NormalSleep_L2 = 0;
			Sleep_Status = SLEEP_HICCUP_NORMAL_L2;
		}
	}
	else
	{
		// 有电流则继续在这�?函数�?�?
	}
}

// 架构决定要改一改，不然后期人员�?难维护了
void SleepDeal_Shift(void)
{
	if (Sleep_Mode.bits.b1TestSleep != 0)
	{
		Sleep_Status = SLEEP_HICCUP_TEST;
	}
	else if (Sleep_Mode.bits.b1OverCurSleep != 0)
	{
		// Sleep_Status = SLEEP_HICCUP_CONTINUE;			//架构已改，先跳到相关函数，再进入休眠
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
	{ // 没有以上各�?�保护直接进入主�?�?
		Sleep_Status = SLEEP_HICCUP_NORMAL_SELECT;
	}
}

void SleepDeal_Test(void)
{
	static UINT16 s_u16HaltTestCnt = 0;
	if (!Sleep_Mode.bits.b1TestSleep)
	{ // 加强雍余设�??
		Sleep_Status = SLEEP_HICCUP_SHIFT;
		return;
	}

	if (++s_u16HaltTestCnt >= 2)
	{ // 10s——Test
		s_u16HaltTestCnt = 0;
		Sleep_Status = SLEEP_HICCUP_CONTINUE;
	}
}

void IsSleepStartUp(void)
{
	// UINT8 mode = DEEP_MODE;
	UINT8 mode = NO_SLEEP;
	bool is_ret_from_deepsleep = false;
	UINT8 last_mode = NO_SLEEP;

	switch (SleepDeal_LoadSleepModeFlag())
	{
	case FLASH_HICCUP_SLEEP_VALUE:
		mode = HICCUP_MODE;
		break;
	case FLASH_NORMAL_SLEEP_VALUE:
		mode = NORMAL_MODE;
		break;
	case FLASH_DEEP_SLEEP_VALUE:
		mode = DEEP_MODE;
		break;
	case FLASH_SLEEP_RESET_VALUE:
	default:
		break;
	}

	if (mode == DEEP_MODE)
	{
		while (1)
		{
			IOstatus_DeepMode();
			InitWakeUp_DeepMode();

			g_irq_t = NO_IRQ;
			Sys_StopMode();
			SystemInit();
			InitDelay();

			UINT8 reason;
			if (SleepWakeFastUi_DetectWakeReason(&reason))
			{
				is_ret_from_deepsleep = true;
				break;
			}
		}
	}
	if (is_ret_from_deepsleep)
		SleepWakeFastUi_ServiceStartupPreview();
}

void App_SleepDeal(void)
{
	static UINT16 force_sleep_delay = 0;
	// if (0 == g_st_SysTimeFlag.bits.b1Sys1000msFlag1 && !Sleep_Mode.bits.b1ForceToSleep_L1 && !Sleep_Mode.bits.b1ForceToSleep_L2 && !Sleep_Mode.bits.b1ForceToSleep_L3)
	if (0 == gu8_1000msAccClock_Flag && !Sleep_Mode.bits.b1ForceToSleep_L1 && !Sleep_Mode.bits.b1ForceToSleep_L2 && !Sleep_Mode.bits.b1ForceToSleep_L3)
	{
		return;
	}
	gu8_1000msAccClock_Flag = 0;

	switch (Sleep_Status)
	{
	case SLEEP_HICCUP_NORMAL_SELECT:
		SleepDeal_Normal_Select();
		break;
	case SLEEP_HICCUP_NORMAL_L2:
		SleepDeal_Normal_L2();
		break;
	case SLEEP_HICCUP_NORMAL_L3:
		SleepDeal_Normal_L3();
		break;
	default:
		Sleep_Status = SLEEP_HICCUP_NORMAL_SELECT;
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

	if (g_stCellInfoReport.u16VCellMin < 2800 && !g_stCellInfoReport.u16Ichg)
	{
		++force_sleep_delay;
		if (force_sleep_delay >= (60 * 10))
		{
			entersleep(DEEP_MODE);
		}
	}
	else
	{
		force_sleep_delay = 0;
	}

	if ((Sleep_Mode.all & 0x00ff))
	{
		extern UINT32 su32_Interval_S_Tcnt;

		LogRecord_Flag.bits.Log_Sleep = 1;
		LogEvent_Record(LogRecord_Flag.bits.Log_Sleep, BMS_SLEEP, &su32_Interval_S_Tcnt);
		SleepDeal_Continue();
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

void Sys_SleepOnExitMode(void)
{
	NVIC_SystemLPConfig(NVIC_LP_SLEEPONEXIT, ENABLE); // 库函数版�?，�?�置SLEEP ON EXIT位为1
	// SCB->SCR|=1<<1;//寄存器版�?，�?�置SLEEP ON EXIT位为1
	__ASM volatile("wfi");
}
