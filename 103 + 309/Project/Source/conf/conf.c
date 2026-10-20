// #include "conf.h"
#include "main.h"

Time_T sys_time = {
    .time_enter_rtc = 10,
};

// void GPIO_SetBits(GPIO_TypeDef * GPIOx, uint16_t GPIO_Pin);
// void GPIO_ResetBits(GPIO_TypeDef * GPIOx, uint16_t GPIO_Pin);
// GPIO_ResetBits(GPIOB, GPIO_Pin_15);
// GPIO_SetBits(GPIOB, GPIO_Pin_15);
void InitIO(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_AFIO, ENABLE);  // 使能IO复用功能模块时钟
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE); // 使能GPIOA时钟
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE); // 使能GPIOB时钟
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOC, ENABLE); // 使能GPIOC时钟
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOD, ENABLE); // 使能GPIOD时钟
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOE, ENABLE); // 使能GPIOE时钟

    {
        GPIO_InitStructure.GPIO_Pin = PIN_AFE1_ALM | PIN_AFE1_MODE | PIN_AFE1_SHIP;
        GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
        GPIO_InitStructure.GPIO_Speed = GPIO_Speed_2MHz; // IO口速度为2MHz
        GPIO_Init(GPIOA, &GPIO_InitStructure);

        /* Boot: latch CTLC low before output mode; STOP wake retains its level. */
        if (BmsCurrent_GetBootZeroStatus() == BOOT_CURRENT_ZERO_NOT_RUN)
            GPIO_ResetBits(GPIOB, PIN_AFE1_CTL);
        GPIO_InitStructure.GPIO_Pin = PIN_AFE1_PRO_EN | PIN_AFE1_CTL;
        GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
        GPIO_InitStructure.GPIO_Speed = GPIO_Speed_2MHz; // IO口速度为2MHz
        GPIO_Init(GPIOB, &GPIO_InitStructure);
    }

    // PB15_LED1
    // GPIO_WriteBit(GPIOB, GPIO_Pin_15, Bit_RESET);
    // GPIO_WriteBit(GPIOB, GPIO_Pin_15, Bit_SET);
    GPIO_InitStructure.GPIO_Pin = PIN_DBG_LED;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP; // 推挽输出
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_2MHz; // IO口速度为2MHz
    GPIO_Init(GPIO_DBG_LED, &GPIO_InitStructure);
    // void GPIO_SetBits(GPIO_TypeDef * GPIOx, uint16_t GPIO_Pin);
    // void GPIO_ResetBits(GPIO_TypeDef * GPIOx, uint16_t GPIO_Pin);
    // GPIO_ResetBits(GPIOB, GPIO_Pin_15);
    // GPIO_SetBits(GPIOB, GPIO_Pin_15);

    GPIO_InitStructure.GPIO_Pin = PIN_KEY1;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;
    GPIO_Init(GPIO_KEY1, &GPIO_InitStructure);

    {
        //???这个函数没起作用
        // GPIO_WriteBit(GPIO_M_STB, PIN_M_STB, Bit_RESET);
        // GPIO_WriteBit(GPIO_AD_EN, PIN_AD_EN, Bit_RESET);
        // GPIO_WriteBit(GPIO_BLE_EN, PIN_BLE_EN, Bit_RESET);
        // GPIO_WriteBit(GPIO_SW_EN, PIN_SW_EN, Bit_RESET);

        GPIO_SetBits(GPIO_M_STB, PIN_M_STB);
        GPIO_ResetBits(GPIO_AD_EN, PIN_AD_EN);
        GPIO_ResetBits(GPIO_BLE_EN, PIN_BLE_EN);
        GPIO_ResetBits(GPIO_CMNT_EN, PIN_CMNT_EN);
        GPIO_SetBits(GPIO_SW_EN, PIN_SW_EN);

        GPIO_InitStructure.GPIO_Pin = PIN_M_STB;
        GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP; // 推挽输出
        GPIO_InitStructure.GPIO_Speed = GPIO_Speed_2MHz; // IO口速度为2MHz
        GPIO_Init(GPIO_M_STB, &GPIO_InitStructure);

        GPIO_InitStructure.GPIO_Pin = PIN_AD_EN;
        GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP; // 推挽输出
        GPIO_InitStructure.GPIO_Speed = GPIO_Speed_2MHz; // IO口速度为2MHz
        GPIO_Init(GPIO_AD_EN, &GPIO_InitStructure);

        GPIO_InitStructure.GPIO_Pin = PIN_BLE_EN;
        GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP; // 推挽输出
        GPIO_InitStructure.GPIO_Speed = GPIO_Speed_2MHz; // IO口速度为2MHz
        GPIO_Init(GPIO_BLE_EN, &GPIO_InitStructure);

        GPIO_InitStructure.GPIO_Pin = PIN_SW_EN;
        GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP; // 推挽输出
        GPIO_InitStructure.GPIO_Speed = GPIO_Speed_2MHz; // IO口速度为2MHz
        GPIO_Init(GPIO_SW_EN, &GPIO_InitStructure);

        GPIO_InitStructure.GPIO_Pin = PIN_CMNT_EN;
        GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP; // 推挽输出
        GPIO_InitStructure.GPIO_Speed = GPIO_Speed_2MHz; // IO口速度为2MHz
        GPIO_Init(GPIO_CMNT_EN, &GPIO_InitStructure);
    }

    MCUO_PWSV_STB = 1;
    MCUO_PWSV_CTR = 0;
    MCUO_DRV_CMNT = 0;
    MCUO_AFE_SHIP = 0;
    MCUO_AFE_MODE = 0;
}

void InitWakeUp_Base(void)
{
    EXTI_InitTypeDef EXTI_InitStruct;
    NVIC_InitTypeDef NVIC_InitStructure;
    GPIO_InitTypeDef GPIO_InitStructure;

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE); // ??GPIOA??????????????
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE); // ??GPIOA??????????????
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOC, ENABLE); // ??GPIOA??????????????
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_AFIO, ENABLE);  // ???????????

    jtag_disableAndConfIO();
#if 1
    {
        NVIC_DisableIRQ(EXTI0_IRQn);
        EXTI_InitStruct.EXTI_Line = EXTI_Line0;
        EXTI_InitStruct.EXTI_Mode = EXTI_Mode_Interrupt;
        EXTI_InitStruct.EXTI_Trigger = EXTI_Trigger_Rising;
        EXTI_InitStruct.EXTI_LineCmd = DISABLE;
        EXTI_Init(&EXTI_InitStruct);
        GPIO_InitStructure.GPIO_Pin = PIN_INT_WK_MCU; // ?????GPIO??,PA0?????
        GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPD;
        GPIO_Init(GPIO_INT_WK_MCU, &GPIO_InitStructure);
        GPIO_EXTILineConfig(GPIO_PortSourceGPIOA, GPIO_PinSource0);
        EXTI_ClearITPendingBit(EXTI_Line0);
        EXTI_InitStruct.EXTI_Line = EXTI_Line0;
        EXTI_InitStruct.EXTI_Mode = EXTI_Mode_Interrupt;
        EXTI_InitStruct.EXTI_Trigger = EXTI_Trigger_Rising;
        EXTI_InitStruct.EXTI_LineCmd = ENABLE;
        EXTI_Init(&EXTI_InitStruct);
        NVIC_InitStructure.NVIC_IRQChannel = EXTI0_IRQn;
        NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 0x01;
        NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0x01;
        NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
        NVIC_ClearPendingIRQ(EXTI0_IRQn);
        NVIC_Init(&NVIC_InitStructure);
    }

    {
        NVIC_DisableIRQ(EXTI9_5_IRQn);
        EXTI_InitStruct.EXTI_Line = EXTI_Line9;
        EXTI_InitStruct.EXTI_Mode = EXTI_Mode_Interrupt;
        EXTI_InitStruct.EXTI_Trigger = EXTI_Trigger_Falling;
        EXTI_InitStruct.EXTI_LineCmd = DISABLE;
        EXTI_Init(&EXTI_InitStruct);
        GPIO_InitStructure.GPIO_Pin = PIN_KEY1; // ?????GPIO??,PA0?????
        GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPU;
        GPIO_Init(GPIO_KEY1, &GPIO_InitStructure);
        GPIO_EXTILineConfig(GPIO_PortSourceGPIOA, GPIO_PinSource9);
        EXTI_ClearITPendingBit(EXTI_Line9);
        EXTI_InitStruct.EXTI_Line = EXTI_Line9;
        EXTI_InitStruct.EXTI_Mode = EXTI_Mode_Interrupt;
        EXTI_InitStruct.EXTI_Trigger = EXTI_Trigger_Falling;
        EXTI_InitStruct.EXTI_LineCmd = ENABLE;
        EXTI_Init(&EXTI_InitStruct);
        NVIC_InitStructure.NVIC_IRQChannel = EXTI9_5_IRQn;
        NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 0x01;
        NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0x01;
        NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
        NVIC_ClearPendingIRQ(EXTI9_5_IRQn);
        NVIC_Init(&NVIC_InitStructure);
    }
#endif
}

void InitWakeUp_NormalMode(void)
{
    EXTI_InitTypeDef EXTI_InitStruct;
    NVIC_InitTypeDef NVIC_InitStructure;
    GPIO_InitTypeDef GPIO_InitStructure;

    InitWakeUp_Base();

    {
        /* UART start bits are falling edges. The pull-ups keep an unplugged or
           unpowered communication box at the UART idle level. */
        GPIO_InitStructure.GPIO_Pin = PIN_SCI1_RX;
        GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPU;
        GPIO_Init(GPIO_SCI1_RX, &GPIO_InitStructure);
        GPIO_EXTILineConfig(GPIO_PortSourceGPIOB, GPIO_PinSource7);
        EXTI_ClearITPendingBit(EXTI_Line7);
        EXTI_InitStruct.EXTI_Line = EXTI_Line7;
        EXTI_InitStruct.EXTI_Mode = EXTI_Mode_Interrupt;
        EXTI_InitStruct.EXTI_Trigger = EXTI_Trigger_Falling;
        EXTI_InitStruct.EXTI_LineCmd = ENABLE;
        EXTI_Init(&EXTI_InitStruct);

        NVIC_DisableIRQ(EXTI3_IRQn);
        EXTI_InitStruct.EXTI_Line = EXTI_Line3;
        EXTI_InitStruct.EXTI_Mode = EXTI_Mode_Interrupt;
        EXTI_InitStruct.EXTI_Trigger = EXTI_Trigger_Falling;
        EXTI_InitStruct.EXTI_LineCmd = DISABLE;
        EXTI_Init(&EXTI_InitStruct);
        GPIO_InitStructure.GPIO_Pin = PIN_SCI2_RX;
        GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPU;
        GPIO_Init(GPIO_SCI2_RX, &GPIO_InitStructure);
        GPIO_EXTILineConfig(GPIO_PortSourceGPIOA, GPIO_PinSource3);
        EXTI_ClearITPendingBit(EXTI_Line3);
        EXTI_InitStruct.EXTI_Line = EXTI_Line3;
        EXTI_InitStruct.EXTI_Mode = EXTI_Mode_Interrupt;
        EXTI_InitStruct.EXTI_Trigger = EXTI_Trigger_Falling;
        EXTI_InitStruct.EXTI_LineCmd = ENABLE;
        EXTI_Init(&EXTI_InitStruct);
        NVIC_InitStructure.NVIC_IRQChannel = EXTI3_IRQn;
        NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 0x01;
        NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0x01;
        NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
        NVIC_ClearPendingIRQ(EXTI3_IRQn);
        NVIC_Init(&NVIC_InitStructure);
    }
    {
        NVIC_DisableIRQ(EXTI15_10_IRQn);
        EXTI_InitStruct.EXTI_Line = EXTI_Line12;
        EXTI_InitStruct.EXTI_Mode = EXTI_Mode_Interrupt;
        EXTI_InitStruct.EXTI_Trigger = EXTI_Trigger_Rising;
        EXTI_InitStruct.EXTI_LineCmd = DISABLE;
        EXTI_Init(&EXTI_InitStruct);
        GPIO_InitStructure.GPIO_Pin = PIN_INT_WK_CMNT; // ?????GPIO??,PA0?????
        GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPD;
        GPIO_Init(GPIO_INT_WK_CMNT, &GPIO_InitStructure);

        GPIO_EXTILineConfig(GPIO_PortSourceGPIOB, GPIO_PinSource12);
        EXTI_ClearITPendingBit(EXTI_Line12);
        EXTI_InitStruct.EXTI_Line = EXTI_Line12;
        EXTI_InitStruct.EXTI_Mode = EXTI_Mode_Interrupt;
        EXTI_InitStruct.EXTI_Trigger = EXTI_Trigger_Rising;
        EXTI_InitStruct.EXTI_LineCmd = ENABLE;
        EXTI_Init(&EXTI_InitStruct);

        NVIC_InitStructure.NVIC_IRQChannel = EXTI15_10_IRQn;
        NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 0x01;
        NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0x01;
        NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
        NVIC_ClearPendingIRQ(EXTI15_10_IRQn);
        NVIC_Init(&NVIC_InitStructure);
    }
}

void InitWakeUp_RTCMode(void)
{
    EXTI_InitTypeDef EXTI_InitStruct;
    NVIC_InitTypeDef NVIC_InitStructure;
    GPIO_InitTypeDef GPIO_InitStructure;

    InitWakeUp_Base();

    /* The runtime framework owns the nonblocking PB12 quiet handoff.
       This legacy helper only configures pins/interrupts. */

    /* RTC IO releases CMNT_EN and removes C33V/VCC33-U. The unpowered
       communication circuits can then hold USART RX low, so PA3/PB7 are not
       valid RTC wake sources. Keep only the always-powered communication
       detector on PB12; USARTs are restored after a real wake. */
    EXTI_InitStruct.EXTI_Line = EXTI_Line3 | EXTI_Line7;
    EXTI_InitStruct.EXTI_Mode = EXTI_Mode_Interrupt;
    EXTI_InitStruct.EXTI_Trigger = EXTI_Trigger_Falling;
    EXTI_InitStruct.EXTI_LineCmd = DISABLE;
    EXTI_Init(&EXTI_InitStruct);
    EXTI_ClearITPendingBit(EXTI_Line3 | EXTI_Line7);
    NVIC_DisableIRQ(EXTI3_IRQn);
    NVIC_ClearPendingIRQ(EXTI3_IRQn);

    NVIC_DisableIRQ(EXTI15_10_IRQn);
    EXTI_InitStruct.EXTI_Line = EXTI_Line12;
    EXTI_InitStruct.EXTI_Mode = EXTI_Mode_Interrupt;
    EXTI_InitStruct.EXTI_Trigger = EXTI_Trigger_Rising;
    EXTI_InitStruct.EXTI_LineCmd = DISABLE;
    EXTI_Init(&EXTI_InitStruct);
    GPIO_InitStructure.GPIO_Pin = PIN_INT_WK_CMNT;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPD;
    GPIO_Init(GPIO_INT_WK_CMNT, &GPIO_InitStructure);
    GPIO_EXTILineConfig(GPIO_PortSourceGPIOB, GPIO_PinSource12);
    EXTI_ClearITPendingBit(EXTI_Line12);
    EXTI_InitStruct.EXTI_LineCmd = ENABLE;
    EXTI_Init(&EXTI_InitStruct);
    NVIC_InitStructure.NVIC_IRQChannel = EXTI15_10_IRQn;
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 0x01;
    NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0x01;
    NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
    NVIC_ClearPendingIRQ(EXTI15_10_IRQn);
    NVIC_Init(&NVIC_InitStructure);
}

// ???standby?????PA0?wkup???
// ???????????????
void InitWakeUp_DeepMode(void)
{
    InitWakeUp_Base();
}

void IOstatus_Base(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE); // ??GPIOA??
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE); // ??GPIOB??
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOC, ENABLE); // ??GPIOC??
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOD, ENABLE); // ??GPIOD??
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOE, ENABLE); // ??GPIOE??

    ADC_DeInit(ADC1); // ????????????????

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

    // __delay_ms(100);
}

void IOstatus_RTCMode(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE); // ??GPIOA??
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE); // ??GPIOB??
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOC, ENABLE); // ??GPIOC??
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOD, ENABLE); // ??GPIOD??
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOE, ENABLE); // ??GPIOE??

    ADC_DeInit(ADC1); // ????????????????

    /* Keep only valid always-powered wake inputs digital. RTC mode removes the
       communication rail, so USART RX pins must be analog to avoid leakage and
       false wake levels. Keep SWD pins available for debugging. */
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_All &
        ~(PIN_INT_WK_MCU | PIN_KEY1 | GPIO_Pin_13 | GPIO_Pin_14);
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AIN;
    GPIO_Init(GPIOA, &GPIO_InitStructure);

#if 0
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_All ;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AIN;
    GPIO_Init(GPIOB, &GPIO_InitStructure);
#else
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_All;
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_All &
        ~(PIN_AFE1_CTL | PIN_INT_WK_CMNT);
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AIN;
    GPIO_Init(GPIOB, &GPIO_InitStructure);
#endif

#if 1
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_All;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AIN;
    GPIO_Init(GPIOC, &GPIO_InitStructure);
#else
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_All;
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_All & (~GPIO_Pin_4);
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AIN;
    GPIO_Init(GPIOC, &GPIO_InitStructure);
#endif

    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_All;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AIN;
    GPIO_Init(GPIOD, &GPIO_InitStructure);

    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_All;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AIN;
    GPIO_Init(GPIOE, &GPIO_InitStructure);

    // ??????
    // ???
#if 1
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_12;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_2MHz;
    GPIO_Init(GPIOC, &GPIO_InitStructure);
    // GPIO_ResetBits(GPIOC, GPIO_InitStructure.GPIO_Pin);
    MCUO_DRV_CMNT = 1;

    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_13;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_2MHz;
    GPIO_Init(GPIOC, &GPIO_InitStructure);
    // GPIO_ResetBits(GPIOC, GPIO_InitStructure.GPIO_Pin);
    MCUO_PWSV_CTR = 1;

    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_2;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_2MHz;
    GPIO_Init(GPIOD, &GPIO_InitStructure);
    // GPIO_ResetBits(GPIOD, GPIO_InitStructure.GPIO_Pin);
    MCUO_PWSV_STB = 0;
#endif

    // __delay_ms(100);
}

void IOstatus_NormalMode(void)
{
    IOstatus_Base();
}

void IOstatus_DeepMode(void)
{
    IOstatus_Base();
}

/* STOP keeps CFGR divisors and VTOR; restore only the source used before WFI. */
static UINT8 RestoreRunClockAfterStop(UINT8 previous_source)
{
    UINT32 polls = 1000000UL;
    if (RCC_GetSYSCLKSource() == previous_source)
        return 1U;
    /* This target runs directly from HSE (no PLL); HSI is the STOP wake source. */
    if (previous_source != RCC_CFGR_SWS_HSE)
        return 0U;
    RCC_HSEConfig(RCC_HSE_ON);
    if (RCC_WaitForHSEStartUp() == ERROR)
        return 0U;
    RCC_SYSCLKConfig(RCC_SYSCLKSource_HSE);
    while (RCC_GetSYSCLKSource() != previous_source)
    {
        if (--polls == 0U)
            return 0U;
    }
    return 1U;
}

void Sys_StopMode(void)
{
    UINT32 primask;
    UINT8 previous_source = RCC_GetSYSCLKSource();
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_PWR, ENABLE);
    TIM_Cmd(TIM3, DISABLE);
    TIM_ClearITPendingBit(TIM3, TIM_IT_Update);
    NVIC_ClearPendingIRQ(TIM3_IRQn);
    /* Pending wake IRQs must survive until WFI, including edges during IO setup. */
    primask = __get_PRIMASK();
    __disable_irq();
    __DSB();
    if (g_irq_t == NO_IRQ && !is_rtc_wakekup)
        PWR_EnterSTOPMode(PWR_Regulator_LowPower, PWR_STOPEntry_WFI);
    if (!RestoreRunClockAfterStop(previous_source))
    {
        LowPower_RecordFault(LP_FAULT_CLOCK);
        MCU_RESET();
    }
    SystemCoreClockUpdate();
    InitDelay();
    __set_PRIMASK(primask);
}

/* Cold boot clears charger state; STOP resume must retain ISR events and
   protection latches. Reconfigure its hardware only. */
extern void Init_Charger_AllSeries(void);

void InitRunAfterStop(void)
{
    InitIO();
    ADC_DeInit(ADC1);
    InitADC();
#ifdef __FUNC__HEAT__
    InitHeat_Cool();
#endif
#ifdef __FUNC__LED__
    APP_LedBar();
    set_LED_state(LED_BAR_NORMAL, 4);
#endif
    USART_DeInit(USART1);
    USART_DeInit(USART2);
    /* Restore cached baud rates; EEPROM pins are analog until the end below. */
#ifdef _COMMOM_UPPER_SCI1
    InitSCI1_CommonUpper_rtc();
#endif
#ifdef _COMMOM_UPPER_SCI2
    InitSCI2_CommonUpper_rtc();
#endif
    InitTimer();
    sys_time.wakeup_rtc = true;
    Init_Charger_AllSeries();
    initAFE1_IIC();
    InitE2PROM_i2c();
}
