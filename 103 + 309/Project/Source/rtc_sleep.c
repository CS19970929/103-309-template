#include "main.h"

void sleep(void)
{
    if (!gu8_1000msAccClock_Flag)
        return;
    gu8_1000msAccClock_Flag = 0;

    BQ769x0_SleepMode_Ctrl();

    static uint8_t state_sleep = 0;
    static uint32_t sleep_cnt = 0;

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
}
