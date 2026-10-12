#include "main.h"

UINT8 SleepDeal_CommitResetSleep(UINT8 mode)
{
    UINT16 flag;

    if (mode == NORMAL_MODE)
        flag = FLASH_NORMAL_SLEEP_VALUE;
    else if (mode == DEEP_MODE)
        flag = FLASH_DEEP_SLEEP_VALUE;
    else
        return 0U;

    if (!SOC_SaveBeforeSleep())
        return 0U;

    /* Keep the legacy Flash address/values; retries do not erase an equal flag. */
    if (FlashReadOneHalfWord(FLASH_ADDR_SLEEP_FLAG) != flag &&
        FlashWriteOneHalfWord(FLASH_ADDR_SLEEP_FLAG, flag) != FLASH_COMPLETE)
        return 0U;
    if (FlashReadOneHalfWord(FLASH_ADDR_SLEEP_FLAG) != flag)
        return 0U;

    InitAFE1_Sleep(0U);
    if (!AFE_Sleep())
    {
        /* Do not leave a failed AFE command armed for the next unrelated reset. */
        if (FlashWriteOneHalfWord(FLASH_ADDR_SLEEP_FLAG, FLASH_SLEEP_RESET_VALUE) != FLASH_COMPLETE)
            log_e("sleep flag rollback failed");
        return 0U;
    }
    /* Record sleep only after the AFE accepted its command. */
    LogRecord_Flag.bits.Log_Sleep = 1U;
    LogEvent_Record(1U, BMS_SLEEP, &su32_Interval_S_Tcnt);
    MCU_RESET();
    return 1U;
}

void IsSleepStartUp(void)
{
    UINT16 flag = FlashReadOneHalfWord(FLASH_ADDR_SLEEP_FLAG);
    if (flag != FLASH_HICCUP_SLEEP_VALUE && flag != FLASH_NORMAL_SLEEP_VALUE &&
        flag != FLASH_DEEP_SLEEP_VALUE)
        return;
    if (FlashWriteOneHalfWord(FLASH_ADDR_SLEEP_FLAG, FLASH_SLEEP_RESET_VALUE) != FLASH_COMPLETE)
        return;
    if (flag == FLASH_HICCUP_SLEEP_VALUE)
    {
        if (!Init_RTC())
            return;
        IOstatus_RTCMode();
        InitWakeUp_RTCMode();
        if (!RTC_WKTimeConfig())
            return;
    }
    else if (flag == FLASH_NORMAL_SLEEP_VALUE)
    {
        IOstatus_NormalMode();
        InitWakeUp_NormalMode();
    }
    else
    {
        IOstatus_DeepMode();
        InitWakeUp_DeepMode();
    }
    /* Preserve reset-style wake and the existing sources for each legacy mode. */
    Sys_StopMode();
    MCU_RESET();
}
