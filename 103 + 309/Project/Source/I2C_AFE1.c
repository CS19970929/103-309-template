#include "main.h"
#include "afe3520/BmsProtection3520.h"
#include "afe3520/Afe3520Board.h"

/* Keil compile-slot adapter: Afe3520.c is the real transport/measurement unit. */
#include "afe3520/Afe3520.c"

AFEDATA Registers_AFE1;
struct SH367309_Read SH367309_Read_AFE1;

static INT16 Afe3520_NativeToLegacyCadc(INT16 nativeRaw)
{
    /*
     * Generic DataDeal.c still applies the historical current calibration:
     * I = raw * 20 * (CS_Res_Num*1000/CS_Res) / 2147.
     * SH3673520 native CADC is nominally +/-100mV full scale:
     * I = native * 200000/65536 * CS_Res_Num/CS_Res.
     * Equating both gives legacyRaw = nativeRaw * 21470 / 65536.
     * This adapter lets the existing calibrated SOC/current pipeline continue
     * while native SH3673520 CADC remains available inside Afe3520_SNAPSHOT.
     */
    return (INT16)(((INT32)nativeRaw * 21470L) / 65536L);
}

static void Afe3520_InitBoardControlPins(void)
{
    GPIO_InitTypeDef gpio;
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA | RCC_APB2Periph_GPIOB, ENABLE);

    /* SHIP is active-low. Normal boot must release it before SPI access. */
    gpio.GPIO_Pin = AFE3520_PIN_SHIP;
    gpio.GPIO_Speed = GPIO_Speed_2MHz;
    gpio.GPIO_Mode = GPIO_Mode_Out_PP;
    GPIO_Init(AFE3520_GPIO_SHIP, &gpio);
    GPIO_SetBits(AFE3520_GPIO_SHIP, AFE3520_PIN_SHIP);

    /* CTLC is the independent board-level MOS safety gate. Keep it low until
     * the AFE RAM image has been written and read-back verified. */
    gpio.GPIO_Pin = AFE3520_PIN_CTLC;
    gpio.GPIO_Speed = GPIO_Speed_2MHz;
    gpio.GPIO_Mode = GPIO_Mode_Out_PP;
    GPIO_Init(AFE3520_GPIO_CTLC, &gpio);
    GPIO_ResetBits(AFE3520_GPIO_CTLC, AFE3520_PIN_CTLC);

    gpio.GPIO_Pin = AFE3520_PIN_PRO_EN;
    gpio.GPIO_Mode = GPIO_Mode_Out_PP;
    GPIO_Init(AFE3520_GPIO_PRO_EN, &gpio);

    gpio.GPIO_Pin = AFE3520_PIN_DSG_DET | AFE3520_PIN_CHG_DET;
    gpio.GPIO_Mode = GPIO_Mode_IN_FLOATING;
    GPIO_Init(GPIOA, &gpio);
}

UINT8 MTPRead(UINT8 RdAddr, UINT8 Length, UINT8 *RdBuf)
{
    if ((RdBuf == 0) || (Length == 0U)) return 0U;

    if ((RdAddr == MTP_ADC2) && (Length == 2U))
    {
        const AFE3520_SNAPSHOT *snap = Afe3520_GetSnapshot();
        INT16 proxy;
        if (!snap->valid && (Afe3520_Service() != AFE3520_OK)) return 0U;
        snap = Afe3520_GetSnapshot();
        proxy = Afe3520_NativeToLegacyCadc((INT16)snap->cadcRaw);
        RdBuf[0] = (UINT8)(((UINT16)proxy) >> 8);
        RdBuf[1] = (UINT8)proxy;
        return 1U;
    }
    return (Afe3520_Read(RdAddr, RdBuf, Length) == AFE3520_OK) ? 1U : 0U;
}

UINT8 MTPWrite(UINT8 WrAddr, UINT8 Length, UINT8 *WrBuf)
{
    UINT8 i;
    if ((WrBuf == 0) || (Length == 0U)) return 0U;
    for (i = 0U; i < Length; ++i)
    {
        if (Afe3520_Write((UINT8)(WrAddr + i), WrBuf[i]) != AFE3520_OK) return 0U;
    }
    return 1U;
}

UINT8 MTPWriteROM(UINT8 WrAddr, UINT8 Length, UINT8 *WrBuf)
{
    /* SH3673520 protection configuration is RAM. The historical function name
     * survives only as a build/API compatibility shim. */
    return MTPWrite(WrAddr, Length, WrBuf);
}

void initAFE1_IIC(void)
{
    /* STOP entry converts most GPIO to analog. Rebuild SPI pins on every wake.
     * Mark config dirty because a concurrent AFE reset/WDT recovery may also
     * have restored RAM defaults while the MCU was asleep. */
    Afe3520_PortInit();
    Afe3520_MarkConfigDirty();
}

void InitAFE1(void)
{
    Afe3520_InitBoardControlPins();
    Bms3520_ProtectionInit();

    if (Afe3520_Init() != AFE3520_OK)
    {
        SystemRuntime_SetAfeStatus(0U, 0U);
        System_ERROR_UserCallback(ERROR_AFE1);
        return;
    }

    if (!Bms3520_ApplyAndVerifyAfeConfig())
    {
        SystemRuntime_SetAfeStatus(0U, 0U);
        return;
    }

    SystemRuntime_SetAfeStatus(0U, 1U);
    System_ERROR_UserCallback(ERROR_REMOVE_AFE1);
}

void InitAFE1_Sleep(UINT8 mode)
{
    switch (mode)
    {
    case 0U:
        (void)Afe3520_EnterIdle();
        break;
    case 1U:
        (void)Afe3520_EnterSleep();
        break;
    default:
        (void)Afe3520_EnterPowerDown();
        break;
    }
}

UINT8 UpdateVoltageFromBqMaximo(void)
{
    const AFE3520_SNAPSHOT *snap;
    UINT8 i;
    INT32 tempEncoded;
    INT16 proxy;

    if (Afe3520_Service() != AFE3520_OK)
    {
        SystemRuntime_SetAfeStatus(0U, 0U);
        return 0U;
    }

    snap = Afe3520_GetSnapshot();
    if (!snap->valid) return 0U;

    memset(&Registers_AFE1, 0, sizeof(Registers_AFE1));
    for (i = 0U; i < AFE3520_CELL_MAX; ++i)
    {
        SH367309_Read_AFE1.u16VCell[i] = snap->cellMv[i];
        Registers_AFE1.Cell[i] = snap->cellMv[i];
    }

    for (i = 0U; i < AFE3520_TEMP_MAX; ++i)
    {
        /* Generic report convention: (degC + 40) * 10. */
        tempEncoded = (INT32)snap->tempDeciC[i] + 400L;
        if (tempEncoded < 0L) tempEncoded = 0L;
        if (tempEncoded > 2000L) tempEncoded = 2000L;
        SH367309_Read_AFE1.u16TempBat[i] = (UINT16)tempEncoded;
    }
    Registers_AFE1.Temp1 = SH367309_Read_AFE1.u16TempBat[0];
    Registers_AFE1.Temp2 = SH367309_Read_AFE1.u16TempBat[1];
    Registers_AFE1.Temp3 = SH367309_Read_AFE1.u16TempBat[2];
    Registers_AFE1.Temp4 = SH367309_Read_AFE1.u16TempBat[3];

    proxy = Afe3520_NativeToLegacyCadc((INT16)snap->cadcRaw);
    SH367309_Read_AFE1.u16Current = (UINT16)proxy;
    Registers_AFE1.Cadc = proxy;
    SH367309_Read_AFE1.u32VBat = AFE_CalcuVbat();

    SystemRuntime_SetAfeStatus(0U, 1U);
    return 1U;
}
