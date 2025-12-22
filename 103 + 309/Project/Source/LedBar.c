#include "main.h"

LEDBAR_COMMAND LedBar_Command = LED_BAR_STARTUP;

void LedBar_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;

    GPIO_InitStructure.GPIO_Pin = PIN_SOC_25 | PIN_SOC_Y | PIN_SOC_G | PIN_SOC_50 | PIN_SOC_75;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP; // 推挽输出
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_2MHz; // IO口速度为2MHz
    GPIO_Init(PORT_SOC_25, &GPIO_InitStructure);
    // GPIO_InitStructure.GPIO_Pin = PIN_SOC_40;
    // GPIO_Init(PORT_SOC_40, &GPIO_InitStructure);
    // GPIO_InitStructure.GPIO_Pin = PIN_SOC_60;
    // GPIO_Init(PORT_SOC_60, &GPIO_InitStructure);
    // GPIO_InitStructure.GPIO_Pin = PIN_SOC_80;
    // GPIO_Init(PORT_SOC_80, &GPIO_InitStructure);
    GPIO_InitStructure.GPIO_Pin = PIN_SOC_100;
    GPIO_Init(PORT_SOC_100, &GPIO_InitStructure);

    // GPIO_InitStructure.GPIO_Pin = PIN_SOC_RUN | PIN_SOC_ALM;
    // GPIO_Init(PORT_SOC_RUN, &GPIO_InitStructure);

    // GPIO_InitStructure.GPIO_Pin = PIN_SOC_BLE;
    // GPIO_Init(PORT_SOC_BLE, &GPIO_InitStructure);

    GPIO_InitStructure.GPIO_Pin = PIN_SOC_KEY; // 选择要用的GPIO引脚,PA0也可以唤醒
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;
    // GPIO_InitStructure.GPIO_Mode = GPIO_PuPd_NOPULL;
    GPIO_Init(PORT_SOC_KEY, &GPIO_InitStructure);

    if (g_stCellInfoReport.SocElement.u16Soc >= 20)
    {
        MCUO_SOC_G = 1;
        MCUO_SOC_25 = 1;
        // MCUO_SOC_25 = g_stCellInfoReport.SocElement.u16Soc >= 25 ? 1 : 0;
        MCUO_SOC_50 = g_stCellInfoReport.SocElement.u16Soc >= 25 ? 1 : 0;
        MCUO_SOC_75 = g_stCellInfoReport.SocElement.u16Soc >= 50 ? 1 : 0;
        MCUO_SOC_100 = g_stCellInfoReport.SocElement.u16Soc >= 75 ? 1 : 0;
    }
    else
    {
        MCUO_SOC_Y = 1;
    }
}
void LedBar_StartUp(void)
{
    static uint16_t cnt_100ms = 0;
    static bool power_on = false;
    static uint16_t key_cnt = 0;

    if (!power_on)
    {
        if (0 == MCUI_SOC_KEY)
        {
            ++key_cnt;
            cnt_100ms = 0;
            if (key_cnt >= (100 * 4))
            {
                power_on = true;
                MCUO_SOC_G = 0;
                MCUO_SOC_25 = 0;
                MCUO_SOC_50 = 0;
                MCUO_SOC_75 = 0;
                MCUO_SOC_100 = 0;
                // OPEN_DSG();
                Driver_Element.DriverForceExt.bits.b2_Force_MOS_DSG = FORCE_KEEP_MODE;
            }
        }
        else
        {
            key_cnt = 0;
            ++cnt_100ms;
        }

        if (cnt_100ms >= (100 * 9))
        {
            // entersleep(DEEP_MODE);
        }
    }
    else
    {
        static uint16_t cnt = 0;
        ++cnt;
        if (cnt >= 25)
        {
            MCUO_SOC_G = 1;
            MCUO_SOC_25 = 1;
            // MCUO_SOC_25 = g_stCellInfoReport.SocElement.u16Soc >= 25 ? 1 : 0;
            MCUO_SOC_50 = g_stCellInfoReport.SocElement.u16Soc >= 25 ? 1 : 0;
            MCUO_SOC_75 = g_stCellInfoReport.SocElement.u16Soc >= 50 ? 1 : 0;
            MCUO_SOC_100 = g_stCellInfoReport.SocElement.u16Soc >= 75 ? 1 : 0;
            LedBar_Command = LED_BAR_NORMAL;
        }
        else if (cnt >= 20)
            MCUO_SOC_100 = 1;
        else if (cnt >= 15)
            MCUO_SOC_75 = 1;
        else if (cnt >= 10)
            MCUO_SOC_50 = 1;
        else if (cnt >= 5)
        {
            MCUO_SOC_25 = 1;
        }
    }

#if 0
    if (++cnt_100ms <= (10 * 8) || power_on)
    {
        if (MCUI_SOC_KEY == 0)
        {
            key_cnt++;
            if (key_cnt >= ())
        }

        LedBar_Command = LED_BAR_NORMAL;
    }
    else
    {
        // entersleep(DEEP_MODE);
    }
#endif
}

void LedBar_sleep(void)
{
    static uint16_t cnt_100ms = 0;
    static bool power_on = false;
    static uint16_t key_cnt = 0;
    static uint16_t cnt_10ms_toggle = 0;

    if (++cnt_10ms_toggle >= (50))
    {
        cnt_10ms_toggle = 0;
        MCUO_SOC_G = 0;
        MCUO_SOC_Y = 0;

        MCUO_SOC_25 = !MCUO_SOC_25;
        MCUO_SOC_50 = !MCUO_SOC_50;
        MCUO_SOC_75 = !MCUO_SOC_75;
        MCUO_SOC_100 = !MCUO_SOC_100;
    }

    if (!power_on)
    {
        if (0 == MCUI_SOC_KEY)
        {
            ++key_cnt;
            cnt_100ms = 0;
            if (key_cnt >= (100 * 4))
            {
                power_on = true;
                MCUO_SOC_G = 1;
                MCUO_SOC_25 = 1;
                MCUO_SOC_50 = 1;
                MCUO_SOC_75 = 1;
                MCUO_SOC_100 = 1;
                // OPEN_DSG();
                CLOSE_DSG();
            }
        }
        else
        {
            key_cnt = 0;
            ++cnt_100ms;
        }

        if (cnt_100ms >= (100 * 9))
        {
            LedBar_Command = LED_BAR_NORMAL;
        }
    }
    else
    {
        static uint16_t cnt = 0;
        ++cnt;
        if (cnt >= 25)
        {
            MCUO_SOC_G = 0;
            entersleep(DEEP_MODE);
        }
        else if (cnt >= 20)
            MCUO_SOC_25 = 0;
        else if (cnt >= 15)
            MCUO_SOC_50 = 0;
        else if (cnt >= 10)
            MCUO_SOC_75 = 0;
        else if (cnt >= 5)
        {
            MCUO_SOC_100 = 0;
            // MCUO_SOC_25 = 1;
            // // MCUO_SOC_25 = g_stCellInfoReport.SocElement.u16Soc >= 25 ? 1 : 0;
            // MCUO_SOC_50 = g_stCellInfoReport.SocElement.u16Soc >= 25 ? 1 : 0;
            // MCUO_SOC_75 = g_stCellInfoReport.SocElement.u16Soc >= 50 ? 1 : 0;
            // MCUO_SOC_100 = g_stCellInfoReport.SocElement.u16Soc >= 75 ? 1 : 0;
            // LedBar_Command = LED_BAR_NORMAL;
        }
    }

#if 0
    if (++cnt_100ms <= (10 * 8) || power_on)
    {
        if (MCUI_SOC_KEY == 0)
        {
            key_cnt++;
            if (key_cnt >= ())
        }

        LedBar_Command = LED_BAR_NORMAL;
    }
    else
    {
        // entersleep(DEEP_MODE);
    }
#endif
}
void LedBar_Show_Normal(void)
{
    static UINT8 su8_ShowStatus = 0; // 开机亮5s
    static UINT16 su16_ShowDelay_Tcnt = 0;
    static uint8_t delay_cnt = 0;

    switch (su8_ShowStatus)
    {
    case 0:

        if (MCUI_SOC_KEY == 0)
        {
            // su8_ShowStatus = 1;
            LedBar_Command = LED_BAR_SLEEP;
            // MCUO_DO1_EN = !MCUO_DO1_EN;
            MCUO_SOC_G = 0;
            MCUO_SOC_25 = 1;
            MCUO_SOC_50 = 1;
            MCUO_SOC_75 = 1;
            MCUO_SOC_100 = 1;
        }

        if (g_stCellInfoReport.u16Ichg)
        {
            LedBar_Command = LED_BAR_CHG;
        }

        if (g_stCellInfoReport.u16IDischg)
        {
            LedBar_Command = LED_BAR_DSG;
        }
        break;
        // fixme 不起作用

    case 1:
#if 0
        // 5s
        if (++su16_ShowDelay_Tcnt <= 10 * 5)
        {
            // MCUO_SOC_RUN = 1;
            MCUO_SOC_20 = g_stCellInfoReport.SocElement.u16Soc > 0 ? 1 : 0;
            // if (g_stCellInfoReport.SocElement.u16Soc > 0 ? 1 : 0)
            //     MCUO_SOC_20_ON;
            // else
            //     MCUO_SOC_20_OFF;
            MCUO_SOC_40 = g_stCellInfoReport.SocElement.u16Soc >= 20 ? 1 : 0;
            MCUO_SOC_60 = g_stCellInfoReport.SocElement.u16Soc >= 40 ? 1 : 0;
            MCUO_SOC_80 = g_stCellInfoReport.SocElement.u16Soc >= 60 ? 1 : 0;
            MCUO_SOC_100 = g_stCellInfoReport.SocElement.u16Soc >= 80 ? 1 : 0;

            // MCUO_SOC_RUN = 1;
            // MCUO_SOC_20 = 1;
            // // if (g_stCellInfoReport.SocElement.u16Soc > 0 ? 1 : 0)
            // //     MCUO_SOC_20_ON;
            // // else
            // //     MCUO_SOC_20_OFF;
            // MCUO_SOC_40 = 1;
            // MCUO_SOC_60 = 1;
            // MCUO_SOC_80 = 1;
            // MCUO_SOC_100 = 1;
        }
        else
        {
            // MCUO_SOC_RUN = 0;
            MCUO_SOC_20 = 0;
            MCUO_SOC_40 = 0;
            MCUO_SOC_60 = 0;
            MCUO_SOC_80 = 0;
            MCUO_SOC_100 = 0;
            su16_ShowDelay_Tcnt = 0;
            su8_ShowStatus = 0;
        }

        // 一直按着
        if (!MCUI_SOC_KEY)
            su16_ShowDelay_Tcnt = 0;
#else

        if (g_stCellInfoReport.unMdlFault_Third.bits.b1CellUvp || g_stCellInfoReport.unMdlFault_Third.bits.b1BatUvp)
        {
            if (++delay_cnt >= 5)
            {
                delay_cnt = 0;
                MCUO_SOC_Y = MCUO_SOC_Y;
                MCUO_SOC_25 = 0;
                MCUO_SOC_50 = 0;
                MCUO_SOC_75 = 0;
                MCUO_SOC_100 = 0;
            }
        }
        else
        {
            delay_cnt = 0;
            MCUO_SOC_25 = g_stCellInfoReport.SocElement.u16Soc >= 0 ? 1 : 0;
            MCUO_SOC_50 = g_stCellInfoReport.SocElement.u16Soc >= 25 ? 1 : 0;
            MCUO_SOC_75 = g_stCellInfoReport.SocElement.u16Soc >= 50 ? 1 : 0;
            MCUO_SOC_100 = g_stCellInfoReport.SocElement.u16Soc >= 80 ? 1 : 0;
        }
        // MCUO_SOC_100 = g_stCellInfoReport.SocElement.u16Soc >= 80 ? 1 : 0;

#endif
        break;

    default:
        break;
    }
}

void LedBar_Show_CHG(void)
{
    static UINT8 su8_temp = 0;
    static UINT16 su16_ShowDelay = 0;

    if (++su16_ShowDelay <= 5)
    {
        su8_temp = ~(1 << (g_stCellInfoReport.SocElement.u16Soc / 20)); // 充电的灭
    }
    else if (++su16_ShowDelay <= 10)
    {
        su8_temp = 0xFF; // 全亮
    }
    else
    {
        su16_ShowDelay = 0;
    }

    // SOC =100的时候，为0x20，运算结果相当于全亮，不会有闪的
    // SOC =0的时候，也要闪
    // MCUO_SOC_RUN = 1;
    MCUO_SOC_G = 1;
    MCUO_SOC_25 = (g_stCellInfoReport.SocElement.u16Soc >= 0 ? 1 : 0) && (su8_temp & 0x01); // 这里的三目运算符为>=，而不是>，因为这个灯一定要亮
    MCUO_SOC_50 = (g_stCellInfoReport.SocElement.u16Soc >= 20 ? 1 : 0) && (su8_temp & 0x02);
    MCUO_SOC_75 = (g_stCellInfoReport.SocElement.u16Soc >= 40 ? 1 : 0) && (su8_temp & 0x04);
    MCUO_SOC_100 = (g_stCellInfoReport.SocElement.u16Soc >= 60 ? 1 : 0) && (su8_temp & 0x08);

    if (g_stCellInfoReport.u16Ichg == 0)
    {
        // MCUO_SOC_RUN = 0;
        // MCUO_SOC_20 = 0;
        // MCUO_SOC_40 = 0;
        // MCUO_SOC_60 = 0;
        // MCUO_SOC_80 = 0;
        // MCUO_SOC_100 = 0;
        LedBar_Command = LED_BAR_NORMAL;
    }
}

void LedBar_Show_DSG(void)
{
    // MCUO_SOC_RUN = 1;
    MCUO_SOC_25 = g_stCellInfoReport.SocElement.u16Soc > 0 ? 1 : 0;
    MCUO_SOC_50 = g_stCellInfoReport.SocElement.u16Soc >= 20 ? 1 : 0;
    MCUO_SOC_75 = g_stCellInfoReport.SocElement.u16Soc >= 40 ? 1 : 0;
    MCUO_SOC_100 = g_stCellInfoReport.SocElement.u16Soc >= 60 ? 1 : 0;
    // MCUO_SOC_100 = g_stCellInfoReport.SocElement.u16Soc >= 80 ? 1 : 0;

    if (g_stCellInfoReport.u16IDischg == 0)
    {
        // MCUO_SOC_RUN = 0;
        // MCUO_SOC_20 = 0;
        // MCUO_SOC_40 = 0;
        // MCUO_SOC_60 = 0;
        // MCUO_SOC_80 = 0;
        // MCUO_SOC_100 = 0;

        LedBar_Command = LED_BAR_NORMAL;
    }
}

void LedBar_Show_Fault(void)
{
    if (g_stCellInfoReport.unMdlFault_Third.all & 0x3FFB ||
        System_ERROR_UserCallback(ERROR_STATUS_TEMP_BREAK) ||
        System_ERROR_UserCallback(ERROR_STATUS_CBC_DSG))
    {
        // MCUO_SOC_ALARM = !MCUO_SOC_ALARM;
    }
    else
    {
        // MCUO_SOC_ALARM = 0;
    }
}

void LedBar_Show_Sleep(void)
{
    static UINT16 su16_SleepDelay_Tcnt = 0;

    if (!MCUI_SOC_KEY)
    {
        if (++su16_SleepDelay_Tcnt >= 20)
        {
            Sleep_Mode.bits.b1ForceToSleep_L3 = 1;
        }
    }
    else
    {
        su16_SleepDelay_Tcnt = 0;
    }
}

void APP_LedBar(void)
{
    // if (0 == g_st_SysTimeFlag.bits.b1Sys100msFlag)
    // {
    //     return;
    // }

    if (SystemStatus.bits.b1StartUpBMS)
    {
        return;
    }

    // static bool test[6] = {false};

    // MCUO_SOC_25 = test[0] == true ? 1 : 0;
    // MCUO_SOC_Y = test[1] == true ? 1 : 0;
    // MCUO_SOC_G = test[2] == true ? 1 : 0;
    // MCUO_SOC_50 = test[3] == true ? 1 : 0;
    // MCUO_SOC_75 = test[4] == true ? 1 : 0;
    // MCUO_SOC_100 = test[5] == true ? 1 : 0;
#if 1
    switch (LedBar_Command)
    {
    case LED_BAR_STARTUP:
        LedBar_StartUp();
        break;

    case LED_BAR_NORMAL:
        LedBar_Show_Normal();
        break;
    case LED_BAR_CHG:
        LedBar_Show_CHG();
        break;
    case LED_BAR_DSG:
        LedBar_Show_DSG();
        break;
    case LED_BAR_SLEEP:
        LedBar_sleep();
        break;
    case LED_BAR_FAULT:
        // 下面长期监控
        break;

    default:
        break;
    }
#endif

    // LedBar_Show_Fault();
    // 好像有问题
    // LedBar_Show_Sleep();

    // if (BlueToothFlag)
    // {
    //     MCUO_SOC_BLE = 1;
    // }
    // else
    //     MCUO_SOC_BLE = 0;
}
