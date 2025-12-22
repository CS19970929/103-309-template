#include "main.h"


void sleep(void);
static bool rtc_monitor(void);
static bool isException(void);
static bool updataData_rtc(void);
static void report_wkup_sig(void);
static bool isErr_enterRTC(void);

static enum _SLEEP_MODE g_sleepModeSelect = NO_SLEEP;
enum irqWakeup g_irq_t = NO_IRQ;
bool is_wakeup = false;

// 进入IDLE模式
// 0，IDLE。1，Sleep。2，SHIP(没通讯上)
UINT8 AFE_SleepMode_Judge(void)
{
	UINT8 result = 0;

	if (MTPRead(MTP_BSTATUS1, 3, &SH367309_Reg_Store.REG_BSTATUS1.all))
	{
		if (SH367309_Reg_Store.REG_BSTATUS1.all || SH367309_Reg_Store.REG_BSTATUS2.all || SH367309_Reg_Store.REG_BSTATUS3.bits.L0V || SH367309_Reg_Store.REG_BSTATUS3.bits.PCHG_FET)
		{
			// 不能进入IDLE
			result = 1;
		}
		else
		{
			result = 0;
		}
	}
	else
	{
		result = 2;
	}

	return result;
}



void BQ769x0_SleepMode_Ctrl(void)
{
    static UINT8 su8_StartUp_Flag = 0;
    static UINT8 su8_SleepExtComCnt = 0;
    static UINT16 su16_RTC2_100msTCnt = 0;
    static uint32_t deepsleep_cnt = 0;

    UINT8 u8_CurComDelay_Flag = 0;

    // todo 统一rtc_sleep()和App_SleepDeal()过放休眠
    if (AFE_SleepMode_Judge() == 1)
    {
        su16_RTC2_100msTCnt = 0;
        // print_vcell();
        if (++deepsleep_cnt >= (uint32_t)OtherElement.u16Sleep_TimeVlow * 60)
        {
            entersleep(DEEP_MODE);
        }
        log_w("%d s enter deep sleep", (60 * OtherElement.u16Sleep_TimeVlow - deepsleep_cnt));
        return;
    }
    // else if (AFE_SleepMode_Judge() == 2)
    // {
    //     if(g_stCellInfoReport.unMdlFault_Third.bits.b1CellUvp || g_stCellInfoReport.unMdlFault_Third.bits.b1BatUvp)
    //     {
    //         log_a("err");
    //     }
    //     return;
    // }
    else
    {
        deepsleep_cnt = 0;
    }

    switch (su8_StartUp_Flag)
    {
    case 0:
        su8_StartUp_Flag = 1;
        break;
    case 1:
        if (isErr_enterRTC())
        {
            u8_CurComDelay_Flag = 1;
        }
        else if (su8_SleepExtComCnt != RTC_ExtComCnt)
        {
            su8_SleepExtComCnt = RTC_ExtComCnt;
            u8_CurComDelay_Flag = 1;
        }

        if (u8_CurComDelay_Flag)
        {
            su16_RTC2_100msTCnt = 0;
        }
        else
        {
            if (AFE_SleepMode_Judge() == 0)
            {
							//if (++su16_RTC2_100msTCnt >= OtherElement.time_enter_rtc)
                if (++su16_RTC2_100msTCnt >= 30)
                {
                    su16_RTC2_100msTCnt = 0;

                    entersleep(HICCUP_MODE);

                    log_w("enter rtc mode 1\n");
                }
                log_w("%d s enter rtc mode1", (OtherElement.time_enter_rtc - su16_RTC2_100msTCnt));
            }
            else
            {
                log_a("err");
            }
        }
        break;
    default:
        break;
    }
}

static bool rtc_monitor(void)
{
    bool result = false;

#if (AFE_TYPE == bq76xx_afe)
    result = rtc_monitor_bq7x();
#elif (AFE_TYPE == sh36xx)
    result = rtc_monitor_sh367309();
#else
#error "error!!!"
#endif

    return result;
}

static bool rtc_monitor_bq7x(void)
{
#if AFE_TYPE == bq76xx_afe
    bool result = false;

    I2CReadRegisterByteWithCRC(DEVICE_ADDR_AFE1, SYS_CTRL2, &(Registers_AFE1.SysCtrl2.SysCtrl2Byte));
    I2CReadRegisterByteWithCRC(DEVICE_ADDR_AFE1, SYS_STAT, &(Registers_AFE1.SysStatus.StatusByte));

    if (!Registers_AFE1.SysCtrl2.SysCtrl2Bit.CHG_ON || !Registers_AFE1.SysCtrl2.SysCtrl2Bit.DSG_ON)
    {
        g_irq_t = chg_dsg_close;
        log_e("chg dsg off\n");

        result = true;
    }

    return result;
#endif
    return false;
}

static bool rtc_monitor_sh367309(void)
{
#if AFE_TYPE == sh36xx

    bool result = false;
    if (MTPRead(MTP_BALANCEH, 5, &SH367309_Reg_Store.u8_MTP_BALANCEH))
    {
        // g_stCellInfoReport.u16BalanceFlag1 = SH367309_Reg_Store.u8_MTP_BALANCEL;
        // g_stCellInfoReport.u16BalanceFlag2 = SH367309_Reg_Store.u8_MTP_BALANCEH;
        // SystemStatus.bits.b1Status_MOS_PRE = SH367309_Reg_Store.REG_BSTATUS3.bits.PCHG_FET;
        SystemStatus.bits.b1Status_MOS_CHG = SH367309_Reg_Store.REG_BSTATUS3.bits.CHG_FET;
        SystemStatus.bits.b1Status_MOS_DSG = SH367309_Reg_Store.REG_BSTATUS3.bits.DSG_FET;

        // TemperatureCheck();
        // Fault_ChangeToMCU();
#if 0
		if (SH367309_Reg_Store.REG_BSTATUS1.bits.OV)
		{
			result = true;
			log_w("过压\n");
		}
		if (SH367309_Reg_Store.REG_BSTATUS1.bits.UV)
		{
			result = true;
			log_w("低压\n");
		}
#endif
        if (!SystemStatus.bits.b1Status_MOS_CHG)
        {
            result = true;
            log_w("CHG close\n");
            g_irq_t = chg_dsg_close;
        }
        if (!SystemStatus.bits.b1Status_MOS_DSG)
        {
            result = true;
            log_w("DSG close\n");
            g_irq_t = chg_dsg_close;
        }
    }
    return result;

#endif
}

bool isException(void)
{
    // todo rtc起来读afe保护状态 2、ocv逻辑 大电流 延时ocv
    if (!updataData_rtc())
    {
        return true;
    }

    // todo read AFE status and to deal logi
    if (isHaveCurrent() || rtc_monitor() || isVol_cuv() || isVol_cov())
    {
        return true;
    }
    // TOTEST
    // else if (AFE_SleepMode_Judge() == 1)
    // {
    //     log_e("过放休眠");

    //     return true;
    // }

    return false;
}

static bool updataData_rtc(void)
{
#if (AFE_TYPE == bq76xx_afe)

    return updataData_rtc_bq7x();

#elif (AFE_TYPE == sh36xx)

    return updataData_rtc_sh3x();

#else

#error "error！！！“

#endif

    // return true;
}

// todo 需要考虑afe采样时序
static bool updataData_rtc_bq7x(void)
{
#if (AFE_TYPE == bq76xx_afe)
    // if (UpdateVoltageFromBqMaximo(DEVICE_ADDR_AFE1) && UpdateVoltageFromBqMaximo2(DEVICE_ADDR_AFE1))
    if (UpdateVoltageFromBqMaximo(DEVICE_ADDR_AFE1))
    {
        log_e("IIC error!!!!!!!!!!!!!!!!!!!!!!!!!\n");

        return false;
    }
    // if (UpdateVoltageFromBqMaximo2(DEVICE_ADDR_AFE1))
    // {
    // 	log_w("IIC2 error!!!!!!!!!!!!!!!!!!!!!!!!!\n");
    // return false;
    // }
    DataLoad_CellVolt();
    DataLoad_CellVoltMaxMinFind();

#ifdef __test__
    DataLoad_Temperature();
    DataLoad_TemperatureMaxMinFind();
    DataLoad_Current();
#endif

    return true;

#endif
}

static bool updataData_rtc_sh3x(void)
{
#if (AFE_TYPE == sh36xx)
    // if (UpdateVoltageFromBqMaximo(DEVICE_ADDR_AFE1) && UpdateVoltageFromBqMaximo2(DEVICE_ADDR_AFE1))
    if (UpdateVoltageFromBqMaximo())
    {
        log_e("IIC error!!!!!!!!!!!!!!!!!!!!!!!!!\n");

        return false;
    }
    // if (UpdateVoltageFromBqMaximo2(DEVICE_ADDR_AFE1))
    // {
    // 	log_w("IIC2 error!!!!!!!!!!!!!!!!!!!!!!!!!\n");
    // return false;
    // }
    DataLoad_CellVolt();
    DataLoad_CellVoltMaxMinFind();

#ifdef __test__
    DataLoad_Temperature();
    DataLoad_TemperatureMaxMinFind();
    DataLoad_Current();
#endif

    return true;

#endif
}

void sleep(void)
{
    if (!gu8_1000msAccClock_Flag)
        return;
    gu8_1000msAccClock_Flag = 0;

    BQ769x0_SleepMode_Ctrl();

    static uint8_t state_sleep = 0;
    static uint32_t sleep_cnt = 0;

    switch (state_sleep)
    {
    case 0:
    {
        if (g_sleepModeSelect == HICCUP_MODE)
        {
            // Sleep_Mode.bits.b1_ToSleepFlag = 1;
            // LogRecord_Flag.bits.Log_Sleep = 1;
            // USART_DeInit(USART1);
            state_sleep = 1;
            break;
        }
        if (g_sleepModeSelect == DEEP_MODE)
        {
            Sleep_Mode.bits.b1_ToSleepFlag = 1;
            LogRecord_Flag.bits.Log_Sleep = 1;
            state_sleep = 1;
            break;
        }
    }
    case 1:
    {
        if (Sleep_Mode.bits.b1_ToSleepFlag)
        {
            return;
        }
        switch (g_sleepModeSelect)
        {
        case NORMAL_MODE:
            if (FLASH_COMPLETE == FlashWriteOneHalfWord(FLASH_ADDR_SLEEP_FLAG, FLASH_NORMAL_SLEEP_VALUE))
            {
                ;
            }
            break;
        case HICCUP_MODE:
        {
            // before_rtcsleep();
        rtcsleep:
            Init_RTC();
            IOstatus_RTCMode();
            InitWakeUp_RTCMode();
            // USART_DeInit(USART1);
            // USART_DeInit(USART2);
            // USART_DeInit(USART3);
            sys_time.wakeup_reason = 0;
            is_rtc_wakekup = false;
            g_irq_t = NO_IRQ;
            // __delay_ms(100);

            Feed_IWatchDog;
            Sys_StopMode();
            Feed_IWatchDog;
            // DISABLE_INT();
#if 1
#if defined(UART1_WAKEUP_ENABLE)
            exti_conf(EXTI_Line7, EXTI_Trigger_Rising, DISABLE);
#endif
#if defined(UART2_WAKEUP_ENABLE)
            exti_conf(EXTI_Line3, EXTI_Trigger_Rising, DISABLE);
#endif
#if defined(RS485_WAKEUP_ENABLE)
            exti_conf(EXTI_Line12, EXTI_Trigger_Rising, DISABLE);
#endif
            // exti_conf(EXTI_Line13, EXTI_Trigger_Rising, DISABLE);
            // exti_conf(EXTI_Line0, EXTI_Trigger_Rising, DISABLE);
            // exti_conf(EXTI_Line17, EXTI_Trigger_Rising, DISABLE);
            // RTC_AlarmCmd(RTC_Alarm_A, DISABLE);
            RTC_ITConfig(RTC_FLAG_ALR, DISABLE);
#endif

            if (is_rtc_wakekup)
                ++sleep_cnt;
            // deal_wakeup();
            Init();
            if (is_rtc_wakekup)
            {
                // Init();
                // entersleep(HICCUP_MODE);
                // 还没更新
                // getdata_and_analyse()
                if (isException())
                {
                    is_rtc_wakekup = false;
                    // todo wakeup or deep sleep
                    goto error;
                }
                else
                {
                    //todo
                    // update_rtc_soc(&sleep_cnt);
#if 0
                    // if (sleep_cnt / 3 >= OtherElement.u16Sleep_TimeNormal)
                    if (sleep_cnt * OtherElement.time_sleep_rtcing / 60 >= OtherElement.u16Sleep_TimeNormal)
                    // if (sleep_cnt * 20 / 60 >= OtherElement.u16Sleep_TimeNormal)
                    {
                        log_e("enter normal sleep");
                        before_wakeup(&sleep_cnt);
                        // entersleep(DEEP_MODE);
                        LogRecord_Flag.bits.Log_Sleep = 1;

                        goto DEEP_SLEEP;
                    }
                    // run_idle_and_record();
#endif

                    // log_i("continue rtc, sleep %ds, %d min sleep\n", OtherElement.time_sleep_rtcing, OtherElement.u16Sleep_TimeNormal - sleep_cnt / 3);
                    log_i("continue rtc, sleep %ds, %d min sleep\n", OtherElement.time_sleep_rtcing, OtherElement.u16Sleep_TimeNormal - sleep_cnt * OtherElement.time_sleep_rtcing / 60);

                    goto rtcsleep;
                }
            }
        error:
            // todo
            //  deal_exception_and_record();
            Init();

            state_sleep = 0;
            entersleep(NO_SLEEP);

            report_wkup_sig();

            before_wakeup(&sleep_cnt);
            sleep_cnt = 0;
        }
        break;
        case DEEP_MODE:
        DEEP_SLEEP:
            if (FLASH_COMPLETE == FlashWriteOneHalfWord(FLASH_ADDR_SLEEP_FLAG, FLASH_DEEP_SLEEP_VALUE))
            {
                // App_LogRecord();
                LogEvent_Record(LogRecord_Flag.bits.Log_Sleep, BMS_SLEEP, &su32_Interval_S_Tcnt);

                log_w("deep sleep\n");
                MCU_RESET();
                break;
            }
        default:
            // 不调整引脚进入休眠，功耗会很大
            break;
        }
    }
    default:
        break;
    }

#if 0
sleep:
    is_rtc_wakekup = false;
    static uint32_t sleep_cnt = 0;

    switch (g_sleepModeSelect)
    {
    case NORMAL_MODE:
        if (FLASH_COMPLETE == FlashWriteOneHalfWord(FLASH_ADDR_SLEEP_FLAG, FLASH_NORMAL_SLEEP_VALUE))
        {
            ;
        }
        break;
    case HICCUP_MODE:
    {
        // todo 加热期间不允许 休眠 重构相关
        Init_RTC();
        IOstatus_RTCMode();
        InitWakeUp_RTCMode();

        sys_time.wakeup_reason = 0;

        Feed_IWatchDog;
        Sys_StopMode();
        Feed_IWatchDog;

#if defined(UART1_WAKEUP_ENABLE)
        exti_conf(EXTI_Line10, EXTI_Trigger_Rising, DISABLE);
#endif
#if defined(UART3_WAKEUP_ENABLE)
        exti_conf(EXTI_Line3, EXTI_Trigger_Rising, DISABLE);
#endif
#if defined(RS485_CAN_WAKEUP_ENABLE)
        exti_conf(EXTI_Line14, EXTI_Trigger_Rising, DISABLE);
#endif
        RTC_AlarmCmd(RTC_Alarm_A, DISABLE);

        ++sleep_cnt;
        sys_time.sys_tick_1ms++;

        Init();

        if (is_rtc_wakekup)
        {
            g_sleepModeSelect = HICCUP_MODE;

            if (isException())
            {
                is_rtc_wakekup = false;
                sleep_cnt = 0;

                Init();
                BSP_Printf("exception wakeup\n");
            }
            else
            {
                BSP_Printf("continue rtc\n");
                BSP_Printf("delay\n");
#ifdef __test__
                // __delay_ms(2000);
#endif
                deal_sleep_cnt(&sleep_cnt);
                goto sleep;
            }
        }
        else
        {
            sleep_cnt = 0;
            Init();
            // BSP_Printf("g_test %d\n", g_test);
            BSP_Printf("no rtc wakeup");
        }
        g_sleepModeSelect = NO_SLEEP;
    }
    break;
    case DEEP_MODE:
    deep:
        if (FLASH_COMPLETE == FlashWriteOneHalfWord(FLASH_ADDR_SLEEP_FLAG, FLASH_DEEP_SLEEP_VALUE))
        {
            BSP_Printf("deep sleep\n");
            MCU_RESET();
        }
        break;
    default:
        // 不调整引脚进入休眠，功耗会很大
        break;
    }
#endif
}


void entersleep(enum _SLEEP_MODE mode)
{
    switch (mode)
    {
    case HICCUP_MODE:
        Sleep_Mode.bits.b1ForceToSleep_L1 = 1;
        g_sleepModeSelect = HICCUP_MODE;
        break;
    case NORMAL_MODE:

        break;
    case DEEP_MODE:
        Sleep_Mode.bits.b1ForceToSleep_L3 = 1;
        g_sleepModeSelect = DEEP_MODE;
#ifdef __FUNC__LED__
        set_LED_state(LED_BAR_NORMAL, 4);
#endif // DEBUG
        break;
    case NO_SLEEP:
        g_sleepModeSelect = NO_SLEEP;
        Sleep_Status = SLEEP_HICCUP_SHIFT;
        Sleep_Mode.all = 0;
        break;
    default:
        break;
    }
}