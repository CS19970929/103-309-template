#include "main.h"
#include "LedSnapshot.h"
#include "PowerUi.h"
#include "SleepWakeFastUi.h"

#define WAKE_PREVIEW_DATA_REG       BKP_DR8
#define WAKE_PREVIEW_INV_REG        BKP_DR9
#define WAKE_PREVIEW_MAGIC          ((UINT16)0xA000)
#define WAKE_PREVIEW_MAGIC_MASK     ((UINT16)0xF000)
#define WAKE_PREVIEW_REASON_SHIFT   ((UINT8)4)

#define WAKE_PREVIEW_REASON_KEY     ((UINT8)1)
#define WAKE_PREVIEW_REASON_CHARGE  ((UINT8)2)

#define FAST_UI_TIMEOUT_TICKS       LED_MS_TO_TICKS(LED_BOOT_PREVIEW_TIMEOUT_MS)
#define FAST_UI_CONFIRM_TICKS       LED_MS_TO_TICKS(LED_BOOT_CONFIRM_HOLD_MS)
#define FAST_UI_BLINK_TICKS         LED_MS_TO_TICKS(LED_BLINK_PERIOD_MS)

static void SleepWakeFastUi_EnableBackupAccess(void);
static void SleepWakeFastUi_InitWakeCheckGpio(void);
static UINT8 SleepWakeFastUi_IsValidSleepMode(UINT8 mode);
static UINT16 SleepWakeFastUi_ModeToSleepFlag(UINT8 mode);
static UINT16 SleepWakeFastUi_MakeWakePreviewData(UINT8 mode, UINT8 reason);
static UINT8 SleepWakeFastUi_SaveWakePreview(UINT8 mode, UINT8 reason);
static UINT8 SleepWakeFastUi_LoadWakePreview(UINT8 *mode, UINT8 *reason);
static void SleepWakeFastUi_ClearWakePreview(void);
static UINT8 SleepWakeFastUi_IsChargeWake(void);
static UINT8 SleepWakeFastUi_IsPowerKeyWake(void);
static UINT8 SleepWakeFastUi_DetectWakeReason(UINT8 *reason);
static UINT8 SleepWakeFastUi_ConfirmPowerOn(UINT8 soc, UINT8 alarm);
static void SleepWakeFastUi_EnterStopAgain(UINT8 mode);
static void SleepWakeFastUi_BootAnimation(UINT8 soc, UINT8 alarm);

UINT8 SleepWakeFastUi_ServiceAfterStop(UINT8 sleep_mode)
{
    UINT8 reason;

    SleepWakeFastUi_InitWakeCheckGpio();

    if (SleepWakeFastUi_DetectWakeReason(&reason)) {
        if (SleepWakeFastUi_SaveWakePreview(sleep_mode, reason)) {
            MCU_RESET();
        }
    }

    return SLEEP_WAKE_FAST_NONE;
}

UINT8 SleepWakeFastUi_ServiceStartupPreview(void)
{
    UINT8 mode;
    UINT8 reason;
    UINT8 soc;
    UINT8 flags;
    UINT8 power_on;
    UINT8 blink_on;
    UINT16 timeout_ticks;
    UINT16 hold_ticks;
    UINT16 blink_ticks;
    UINT8 alarm;

    // if (!SleepWakeFastUi_LoadWakePreview(&mode, &reason)) {
    //     SleepWakeFastUi_InitWakeCheckGpio();
    //     mode = DEEP_MODE;
    //     if (!SleepWakeFastUi_DetectWakeReason(&reason)) {
    //         return 0;
    //     }
    // }

    SleepWakeFastUi_InitWakeCheckGpio();
    LedSnapshot_Load(&soc, &flags, &power_on);
    (void)power_on;

    alarm = (UINT8)(flags & LED_SNAPSHOT_FLAG_ALARM);
    blink_on = 1;
    timeout_ticks = FAST_UI_TIMEOUT_TICKS;
    hold_ticks = 0;
    blink_ticks = 0;

    LedBar_FastInit();

#ifdef wdog_enable
    Init_IWDG();
#endif

    if ((reason == WAKE_PREVIEW_REASON_CHARGE) || SleepWakeFastUi_IsChargeWake()) {
        return SleepWakeFastUi_ConfirmPowerOn(soc, alarm);
    }

    LedBar_ShowSocImmediate(soc, alarm, blink_on);
    while (timeout_ticks > 0) {
        if (SleepWakeFastUi_IsChargeWake()) {
            return SleepWakeFastUi_ConfirmPowerOn(soc, alarm);
        }

        if (SleepWakeFastUi_IsPowerKeyWake()) {
            if (hold_ticks < FAST_UI_CONFIRM_TICKS) {
                ++hold_ticks;
            }
            if (hold_ticks >= FAST_UI_CONFIRM_TICKS) {
                return SleepWakeFastUi_ConfirmPowerOn(soc, alarm);
            }
        } else {
            hold_ticks = 0;
        }

        if (++blink_ticks >= FAST_UI_BLINK_TICKS) {
            blink_ticks = 0;
            blink_on = blink_on ? 0 : 1;
            LedBar_ShowSocImmediate(soc, alarm, blink_on);
        }

        __delay_ms(LED_UI_TICK_MS);
        --timeout_ticks;
    }

    LedBar_OutputOff();
    SleepWakeFastUi_EnterStopAgain(mode);
    return 1;
}

static void SleepWakeFastUi_EnableBackupAccess(void)
{
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_PWR | RCC_APB1Periph_BKP, ENABLE);
    PWR_BackupAccessCmd(ENABLE);
}

static void SleepWakeFastUi_InitWakeCheckGpio(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA | RCC_APB2Periph_GPIOB, ENABLE);

    GPIO_InitStructure.GPIO_Pin = PIN_INT_WK_MCU;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPD;
    GPIO_Init(GPIO_INT_WK_MCU, &GPIO_InitStructure);

    GPIO_InitStructure.GPIO_Pin = PIN_SOC_KEY;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPU;
    GPIO_Init(PORT_SOC_KEY, &GPIO_InitStructure);

    GPIO_InitStructure.GPIO_Pin = PIN_KEY1;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPU;
    GPIO_Init(GPIO_KEY1, &GPIO_InitStructure);
}

static UINT8 SleepWakeFastUi_IsValidSleepMode(UINT8 mode)
{
    return (mode == NORMAL_MODE) || (mode == HICCUP_MODE) || (mode == DEEP_MODE);
}

static UINT16 SleepWakeFastUi_ModeToSleepFlag(UINT8 mode)
{
    if (mode == HICCUP_MODE) {
        return FLASH_HICCUP_SLEEP_VALUE;
    }
    if (mode == DEEP_MODE) {
        return FLASH_DEEP_SLEEP_VALUE;
    }
    return FLASH_NORMAL_SLEEP_VALUE;
}

static UINT16 SleepWakeFastUi_MakeWakePreviewData(UINT8 mode, UINT8 reason)
{
    return (UINT16)(WAKE_PREVIEW_MAGIC |
                    (((UINT16)reason & 0x000F) << WAKE_PREVIEW_REASON_SHIFT) |
                    ((UINT16)mode & 0x000F));
}

static UINT8 SleepWakeFastUi_SaveWakePreview(UINT8 mode, UINT8 reason)
{
    UINT16 data;
    UINT16 data_inv;

    if (!SleepWakeFastUi_IsValidSleepMode(mode)) {
        return 0;
    }
    if ((reason != WAKE_PREVIEW_REASON_KEY) && (reason != WAKE_PREVIEW_REASON_CHARGE)) {
        return 0;
    }

    data = SleepWakeFastUi_MakeWakePreviewData(mode, reason);
    data_inv = (UINT16)(~data);

    SleepWakeFastUi_EnableBackupAccess();
    BKP_WriteBackupRegister(WAKE_PREVIEW_DATA_REG, data);
    BKP_WriteBackupRegister(WAKE_PREVIEW_INV_REG, data_inv);

    return (UINT8)((BKP_ReadBackupRegister(WAKE_PREVIEW_DATA_REG) == data) &&
                   (BKP_ReadBackupRegister(WAKE_PREVIEW_INV_REG) == data_inv));
}

static UINT8 SleepWakeFastUi_LoadWakePreview(UINT8 *mode, UINT8 *reason)
{
    UINT16 data;
    UINT16 data_inv;
    UINT8 read_mode;
    UINT8 read_reason;

    SleepWakeFastUi_EnableBackupAccess();
    data = BKP_ReadBackupRegister(WAKE_PREVIEW_DATA_REG);
    data_inv = BKP_ReadBackupRegister(WAKE_PREVIEW_INV_REG);

    if (data != (UINT16)(~data_inv)) {
        return 0;
    }
    if ((data & WAKE_PREVIEW_MAGIC_MASK) != WAKE_PREVIEW_MAGIC) {
        return 0;
    }

    read_mode = (UINT8)(data & 0x000F);
    read_reason = (UINT8)((data >> WAKE_PREVIEW_REASON_SHIFT) & 0x000F);
    if (!SleepWakeFastUi_IsValidSleepMode(read_mode)) {
        SleepWakeFastUi_ClearWakePreview();
        return 0;
    }
    if ((read_reason != WAKE_PREVIEW_REASON_KEY) && (read_reason != WAKE_PREVIEW_REASON_CHARGE)) {
        SleepWakeFastUi_ClearWakePreview();
        return 0;
    }

    *mode = read_mode;
    *reason = read_reason;
    return 1;
}

static void SleepWakeFastUi_ClearWakePreview(void)
{
    SleepWakeFastUi_EnableBackupAccess();
    BKP_WriteBackupRegister(WAKE_PREVIEW_DATA_REG, FLASH_SLEEP_RESET_VALUE);
    BKP_WriteBackupRegister(WAKE_PREVIEW_INV_REG, (UINT16)(~FLASH_SLEEP_RESET_VALUE));
}

static UINT8 SleepWakeFastUi_IsChargeWake(void)
{
    if (g_irq_t == CHG_IRQ) {
        return 1;
    }
    if (GPIO_ReadInputDataBit(GPIO_INT_WK_MCU, PIN_INT_WK_MCU)) {
        return 1;
    }
    return 0;
}

static UINT8 SleepWakeFastUi_IsPowerKeyWake(void)
{
    if ((g_irq_t == bms_keyirq) || (g_irq_t == soc_key)) {
        return 1;
    }
    if (MCUI_SOC_KEY == 0) {
        return 1;
    }
    if (GPIO_ReadInputDataBit(GPIO_KEY1, PIN_KEY1) == Bit_RESET) {
        return 1;
    }
    return 0;
}

static UINT8 SleepWakeFastUi_DetectWakeReason(UINT8 *reason)
{
    if (SleepWakeFastUi_IsChargeWake()) {
        *reason = WAKE_PREVIEW_REASON_CHARGE;
        return 1;
    }
    if (SleepWakeFastUi_IsPowerKeyWake()) {
        *reason = WAKE_PREVIEW_REASON_KEY;
        return 1;
    }
    return 0;
}

static UINT8 SleepWakeFastUi_ConfirmPowerOn(UINT8 soc, UINT8 alarm)
{
    SleepWakeFastUi_BootAnimation(soc, alarm);
    SleepWakeFastUi_ClearWakePreview();
    SleepDeal_ClearSleepModeFlag();
    PowerUi_ConfirmPowerOn();
    return 1;
}

static void SleepWakeFastUi_EnterStopAgain(UINT8 mode)
{
    (void)SleepDeal_SaveSleepModeFlag(SleepWakeFastUi_ModeToSleepFlag(mode));
    SleepWakeFastUi_ClearWakePreview();
    MCU_RESET();
}

static void SleepWakeFastUi_BootAnimation(UINT8 soc, UINT8 alarm)
{
    UINT8 step;

    for (step = 1; step <= 5; ++step) {
        LedBar_ShowBootAnimationStep(step);
        __delay_ms(LED_ANIM_STEP_MS);
    }
    LedBar_ShowSocImmediate(soc, alarm, 1);
}
