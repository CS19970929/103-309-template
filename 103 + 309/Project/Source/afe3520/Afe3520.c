#include "main.h"
#include "afe3520/Afe3520.h"
#include "afe3520/Afe3520Config.h"
#include "conf/conf_gpio.h"
#include <string.h>

/* Reference protocol: Mode 3 SPI, CRC8 poly 0x07/init 0.
 * Write CRC covers CMD+ADDR+DATA; read CRC includes FF+CMD+ADDR+LEN+DATA. */
#define AFE3520_SPI_DUMMY 0x00U
#define AFE3520_SPI_IDLE  0xFFU

static AFE3520_SNAPSHOT s_snapshot;
static AFE3520_DIAG s_diag;
static uint8_t s_ready;
static uint8_t s_configDirty = 1U;
static uint8_t s_shadowSconf2;
static AFE3520_RESULT s_frameError;

/* 10K NTC lookup table used by the SH3673520 temperature code conversion.
 * The index is encoded temperature in degC + 40, covering -40..100 degC. */
const UINT16 iSheldTemp_10K_NTC[141] = {
    20375, 19204, 18115, 17100, 16152, 15266, 14437, 13661, 12934, 12251,
    11611, 11008, 10442, 9909, 9407, 8935, 8489, 8068, 7672, 7297,
    6943, 6608, 6292, 5993, 5710, 5442, 5188, 4948, 4720, 4504,
    4300, 4105, 3921, 3746, 3580, 3422, 3272, 3130, 2994, 2866,
    2751, 2627, 2516, 2410, 2310, 2214, 2123, 2036, 1953, 1874,
    1801, 1726, 1658, 1592, 1530, 1470, 1413, 1358, 1306, 1256,
    1209, 1163, 1119, 1078, 1038, 1000, 963, 928, 894, 862,
    831, 801, 773, 746, 719, 694, 670, 647, 625, 604,
    583, 563, 544, 526, 509, 492, 476, 460, 445, 431,
    416, 403, 390, 378, 366, 355, 343, 333, 322, 312,
    303, 294, 285, 276, 268, 260, 252, 244, 237, 230,
    224, 217, 211, 205, 199, 193, 188, 182, 177, 172,
    167, 163, 158, 154, 150, 146, 142, 138, 134, 131,
    127, 124, 120, 117, 114, 111, 108, 106, 103, 100,
    98
};

static uint8_t Afe3520_Crc8Update(uint8_t crc, uint8_t data)
{
    uint8_t i;
    crc ^= data;
    for (i = 0U; i < 8U; ++i)
    {
        crc = (crc & 0x80U) ? (uint8_t)((crc << 1) ^ 0x07U) : (uint8_t)(crc << 1);
    }
    return crc;
}

static uint8_t Afe3520_Crc8(const uint8_t *data, uint16_t length)
{
    uint16_t i;
    uint8_t crc = 0U;
    for (i = 0U; i < length; ++i) crc = Afe3520_Crc8Update(crc, data[i]);
    return crc;
}

static void Afe3520_SetError(AFE3520_RESULT result)
{
    s_diag.lastError = result;
    if (result != AFE3520_OK) s_diag.lastFault = result;
}

static void Afe3520_CsLow(void)
{
    GPIO_ResetBits(GPIO_CS_SPI, PIN_CS_SPI);
}

static void Afe3520_CsHigh(void)
{
    GPIO_SetBits(GPIO_CS_SPI, PIN_CS_SPI);
}

static void Afe3520_SpiDelayUs(uint32_t us)
{
    volatile uint32_t n = us * 12U;
    while (n-- != 0U) __NOP();
}

#if AFE3520_CFG_USE_HARDWARE_SPI
static void Afe3520_HardwareSpiInit(void)
{
    SPI_InitTypeDef spi;
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_SPI1, ENABLE);
    /* SPL deinit uses the APB2 reset line: clears stuck BSY, OVR and MODF. */
    SPI_I2S_DeInit(SPI1);
    SPI_StructInit(&spi);
    spi.SPI_Direction = SPI_Direction_2Lines_FullDuplex;
    spi.SPI_Mode = SPI_Mode_Master;
    spi.SPI_DataSize = SPI_DataSize_8b;
    spi.SPI_CPOL = SPI_CPOL_High;
    spi.SPI_CPHA = SPI_CPHA_2Edge;
    spi.SPI_NSS = SPI_NSS_Soft;
#if AFE3520_CFG_SPI_DIVIDER == 128
    spi.SPI_BaudRatePrescaler = SPI_BaudRatePrescaler_128;
#else
    spi.SPI_BaudRatePrescaler = SPI_BaudRatePrescaler_256;
#endif
    spi.SPI_FirstBit = SPI_FirstBit_MSB;
    SPI_Init(SPI1, &spi);
    SPI_NSSInternalSoftwareConfig(SPI1, SPI_NSSInternalSoft_Set);
    SPI_Cmd(SPI1, ENABLE);
}

static uint8_t Afe3520_WaitSpiFlag(uint16_t flag, FlagStatus state)
{
    uint32_t remaining = AFE3520_CFG_SPI_POLL_LIMIT;
    if (s_frameError != AFE3520_OK) return 0U;
    while (remaining-- != 0U)
    {
        if ((SPI_I2S_GetFlagStatus(SPI1, SPI_I2S_FLAG_OVR) != RESET) ||
            (SPI_I2S_GetFlagStatus(SPI1, SPI_FLAG_MODF) != RESET))
        {
            ++s_diag.spiErrorCount;
            s_frameError = AFE3520_ERR_SPI;
            return 0U;
        }
        if (SPI_I2S_GetFlagStatus(SPI1, flag) == state) return 1U;
    }
    ++s_diag.timeoutCount;
    s_frameError = AFE3520_ERR_TIMEOUT;
    return 0U;
}
#endif

static void Afe3520_RecoverBus(void)
{
    Afe3520_CsHigh();
#if AFE3520_CFG_USE_HARDWARE_SPI
    Afe3520_HardwareSpiInit();
#else
    GPIO_SetBits(GPIO_SCLK_SPI, PIN_SCLK_SPI);
    GPIO_SetBits(GPIO_MOSI_SPI, PIN_MOSI_SPI);
#endif
    ++s_diag.busRecoveryCount;
}

static uint8_t Afe3520_SpiByte(uint8_t tx)
{
#if AFE3520_CFG_USE_HARDWARE_SPI
    if (!Afe3520_WaitSpiFlag(SPI_I2S_FLAG_TXE, SET)) return 0U;
    SPI_I2S_SendData(SPI1, tx);
    if (!Afe3520_WaitSpiFlag(SPI_I2S_FLAG_RXNE, SET)) return 0U;
    return (uint8_t)SPI_I2S_ReceiveData(SPI1);
#else
    uint8_t i;
    uint8_t rx = 0U;

    for (i = 0U; i < 8U; ++i)
    {
        if ((tx & 0x80U) != 0U)
            GPIO_SetBits(GPIO_MOSI_SPI, PIN_MOSI_SPI);
        else
            GPIO_ResetBits(GPIO_MOSI_SPI, PIN_MOSI_SPI);
        tx <<= 1;

        /* SH3673520 Mode 3: idle high, change on falling edge, sample on rising edge. */
        GPIO_ResetBits(GPIO_SCLK_SPI, PIN_SCLK_SPI);
        Afe3520_SpiDelayUs(1U);
        GPIO_SetBits(GPIO_SCLK_SPI, PIN_SCLK_SPI);
        Afe3520_SpiDelayUs(1U);

        rx <<= 1;
        if (GPIO_ReadInputDataBit(GPIO_MISO_SPI, PIN_MISO_SPI) != Bit_RESET)
            rx |= 1U;
    }

    GPIO_SetBits(GPIO_SCLK_SPI, PIN_SCLK_SPI);
    return rx;
#endif
}

static void Afe3520_BeginFrame(void)
{
    s_frameError = AFE3520_OK;
    Afe3520_CsHigh();
    Afe3520_SpiDelayUs(1U);
    Afe3520_CsLow();
    Afe3520_SpiDelayUs(1U);
}

static void Afe3520_EndFrame(void)
{
#if AFE3520_CFG_USE_HARDWARE_SPI
    (void)Afe3520_WaitSpiFlag(SPI_I2S_FLAG_BSY, RESET);
#endif
    Afe3520_SpiDelayUs(1U);
    Afe3520_CsHigh();
    Afe3520_SpiDelayUs(1U);
}

void Afe3520_PortInit(void)
{
    GPIO_InitTypeDef gpio;
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE);
    /* Preload idle levels before switching pins to outputs, including wake. */
    GPIO_SetBits(GPIO_CS_SPI, PIN_CS_SPI);
    GPIO_SetBits(GPIO_SCLK_SPI, PIN_SCLK_SPI);
    GPIO_SetBits(GPIO_MOSI_SPI, PIN_MOSI_SPI);
    gpio.GPIO_Speed = GPIO_Speed_2MHz;
    gpio.GPIO_Mode = GPIO_Mode_Out_PP;
    gpio.GPIO_Pin = PIN_CS_SPI;
    GPIO_Init(GPIO_CS_SPI, &gpio);
#if AFE3520_CFG_USE_HARDWARE_SPI
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_AFIO, ENABLE);
    GPIO_PinRemapConfig(GPIO_Remap_SPI1, DISABLE);
    gpio.GPIO_Mode = GPIO_Mode_AF_PP;
#endif
    gpio.GPIO_Pin = PIN_SCLK_SPI;
    GPIO_Init(GPIO_SCLK_SPI, &gpio);
    gpio.GPIO_Pin = PIN_MOSI_SPI;
    GPIO_Init(GPIO_MOSI_SPI, &gpio);
    gpio.GPIO_Pin = PIN_MISO_SPI;
    gpio.GPIO_Mode = GPIO_Mode_IN_FLOATING;
    GPIO_Init(GPIO_MISO_SPI, &gpio);
#if AFE3520_CFG_USE_HARDWARE_SPI
    Afe3520_HardwareSpiInit();
#endif
}

static uint8_t Afe3520_WriteCrc(uint8_t reg, uint8_t value)
{
    uint8_t bytes[3] = {AFE3520_CMD_WRITE, reg, value};
    return Afe3520_Crc8(bytes, 3U);
}

static AFE3520_RESULT Afe3520_WriteOnce(uint8_t reg, uint8_t value)
{
    uint8_t rx[5];
    uint8_t crc = Afe3520_WriteCrc(reg, value);

    Afe3520_BeginFrame();
    rx[0] = Afe3520_SpiByte(AFE3520_CMD_WRITE);
    rx[1] = Afe3520_SpiByte(reg);
    rx[2] = Afe3520_SpiByte(value);
    rx[3] = Afe3520_SpiByte(crc);
    rx[4] = Afe3520_SpiByte(AFE3520_SPI_DUMMY);
    Afe3520_EndFrame();
    ++s_diag.transferCount;
    if (s_frameError != AFE3520_OK) return s_frameError;

    if ((rx[0] != AFE3520_SPI_IDLE) || (rx[1] != AFE3520_CMD_WRITE) ||
        (rx[2] != reg) || (rx[3] != value)) return AFE3520_ERR_SPI;
    if (rx[4] != AFE3520_ACK_OK)
    {
        ++s_diag.ackErrorCount;
        return AFE3520_ERR_ACK;
    }
    return AFE3520_OK;
}

AFE3520_RESULT Afe3520_Write(uint8_t reg, uint8_t value)
{
    uint8_t retry;
    AFE3520_RESULT result = AFE3520_ERR_ARG;
    if ((reg < AFE3520_REG_SCONF1) || (reg > AFE3520_REG_FLAG2)) return result;
    for (retry = 0U; retry < AFE3520_SPI_RETRY_MAX; ++retry)
    {
        result = Afe3520_WriteOnce(reg, value);
        if (result == AFE3520_OK) break;
        ++s_diag.retryCount;
        Afe3520_RecoverBus();
        Delay1ms(1U); /* CS is high: retry always starts a fresh transaction. */
    }
    Afe3520_SetError(result);
    if (result != AFE3520_OK) Afe3520_Invalidate();
    return result;
}

static AFE3520_RESULT Afe3520_ReadOnce(uint8_t reg, uint8_t *data, uint8_t len)
{
    uint8_t i;
    uint8_t rx0, rx1, rx2, rx3, rxCrc;
    uint8_t crc = 0U;

    Afe3520_BeginFrame();
    rx0 = Afe3520_SpiByte(AFE3520_CMD_READ);
    rx1 = Afe3520_SpiByte(reg);
    rx2 = Afe3520_SpiByte(len);
    rx3 = Afe3520_SpiByte(AFE3520_SPI_DUMMY);
    crc = Afe3520_Crc8Update(crc, rx0);
    crc = Afe3520_Crc8Update(crc, rx1);
    crc = Afe3520_Crc8Update(crc, rx2);
    crc = Afe3520_Crc8Update(crc, rx3);
    for (i = 0U; i < len; ++i)
    {
        data[i] = Afe3520_SpiByte(AFE3520_SPI_DUMMY);
        crc = Afe3520_Crc8Update(crc, data[i]);
    }
    rxCrc = Afe3520_SpiByte(AFE3520_SPI_DUMMY);
    Afe3520_EndFrame();
    ++s_diag.transferCount;
    if (s_frameError != AFE3520_OK) return s_frameError;

    if ((rx0 != AFE3520_SPI_IDLE) || (rx1 != AFE3520_CMD_READ) ||
        (rx2 != reg) || (rx3 != len)) return AFE3520_ERR_SPI;
    if (crc != rxCrc)
    {
        ++s_diag.crcErrorCount;
        return AFE3520_ERR_CRC;
    }
    return AFE3520_OK;
}

AFE3520_RESULT Afe3520_Read(uint8_t reg, uint8_t *data, uint8_t len)
{
    uint8_t retry;
    uint16_t last;
    AFE3520_RESULT result = AFE3520_ERR_ARG;

    if ((data == 0) || (len == 0U)) return AFE3520_ERR_ARG;
    last = (uint16_t)reg + (uint16_t)len - 1U;
    if ((reg < AFE3520_REG_SCONF1) || (last > AFE3520_REG_LAST)) return AFE3520_ERR_ARG;

    for (retry = 0U; retry < AFE3520_SPI_RETRY_MAX; ++retry)
    {
        result = Afe3520_ReadOnce(reg, data, len);
        if (result == AFE3520_OK)
        {
            Afe3520_SetError(AFE3520_OK);
            return result;
        }
        ++s_diag.retryCount;
        Afe3520_RecoverBus();
        Delay1ms(1U);
    }
    Afe3520_SetError(result);
    if (result != AFE3520_OK) Afe3520_Invalidate();
    return result;
}

AFE3520_RESULT Afe3520_SoftReset(void)
{
    uint8_t retry;
    uint8_t tx[3] = {AFE3520_CMD_RESET, 0xBBU, 0xCCU};
    uint8_t crc = Afe3520_Crc8(tx, 3U);
    uint8_t rx[5];

    for (retry = 0U; retry < AFE3520_SPI_RETRY_MAX; ++retry)
    {
        Afe3520_BeginFrame();
        rx[0] = Afe3520_SpiByte(tx[0]);
        rx[1] = Afe3520_SpiByte(tx[1]);
        rx[2] = Afe3520_SpiByte(tx[2]);
        rx[3] = Afe3520_SpiByte(crc);
        rx[4] = Afe3520_SpiByte(0U);
        Afe3520_EndFrame();
        ++s_diag.transferCount;
        if ((s_frameError == AFE3520_OK) && (rx[0] == 0xFFU) && (rx[1] == tx[0]) && (rx[2] == tx[1]) &&
            (rx[3] == tx[2]) && (rx[4] == AFE3520_ACK_OK))
        {
            ++s_diag.resetCount;
            Afe3520_Invalidate();
            Delay1ms(5U);
            return AFE3520_OK;
        }
        ++s_diag.retryCount;
        Afe3520_RecoverBus();
        Delay1ms(1U);
    }
    Afe3520_Invalidate();
    Afe3520_SetError(s_frameError != AFE3520_OK ? s_frameError : AFE3520_ERR_ACK);
    return s_diag.lastError;
}

static uint16_t Afe3520_Be16(const uint8_t *p)
{
    return (uint16_t)(((uint16_t)p[0] << 8) | p[1]);
}

static int16_t Afe3520_TempFromCode(uint16_t code)
{
    uint16_t lo = 0U;
    uint16_t hi = 140U;
    uint16_t mid;
    uint32_t resistance;

    if (code >= 32768U) return -400;
    resistance = ((uint32_t)code * 1000UL) / (32768UL - code);
    if (resistance >= iSheldTemp_10K_NTC[0]) return -400;
    if (resistance <= iSheldTemp_10K_NTC[140]) return 1000;
    while ((hi - lo) > 1U)
    {
        mid = (uint16_t)((lo + hi) / 2U);
        if (resistance > iSheldTemp_10K_NTC[mid]) hi = mid;
        else lo = mid;
    }
    return (int16_t)(((int16_t)lo - 40) * 10);
}

static int32_t Afe3520_CurrentMaFromCadc(int16_t raw)
{
    int32_t value = raw;
    int32_t sign = 1;
    uint32_t magnitude;
    uint32_t ma;

    if (value < 0)
    {
        sign = -1;
        magnitude = (uint32_t)(-value);
    }
    else magnitude = (uint32_t)value;

    /* Nominal SH3673520 CADC range +/-100mV. Board current calibration remains
     * in the existing K/B layer; this value is primarily native diagnostics. */
    ma = (magnitude * 3125UL * (uint32_t)CS_Res_Num +
          (512UL * (uint32_t)CS_Res)) /
         (1024UL * (uint32_t)CS_Res);
    return (sign > 0) ? (int32_t)ma : -(int32_t)ma;
}

static void Afe3520_ParseSnapshot(const uint8_t *raw)
{
    uint8_t i;
    uint16_t offset;
    uint16_t code;
    int16_t cadc;

    s_snapshot.flag1 = raw[0U];
    s_snapshot.flag2 = raw[1U];
    s_snapshot.flag3 = raw[2U];
    s_snapshot.bstatus1 = raw[3U];
    s_snapshot.bstatus2 = raw[4U];

    for (i = 0U; i < AFE3520_TEMP_MAX; ++i)
    {
        offset = (uint16_t)(AFE3520_REG_TEMP1H - AFE3520_REG_FLAG1 + (uint16_t)i * 2U);
        code = Afe3520_Be16(&raw[offset]);
        s_snapshot.tempDeciC[i] = Afe3520_TempFromCode(code);
    }

    offset = (uint16_t)(AFE3520_REG_TEMPIH - AFE3520_REG_FLAG1);
    code = Afe3520_Be16(&raw[offset]);
    s_snapshot.internalTempDeciC = (int16_t)((((int32_t)code * 5625L / 16384L - 5625L) * 612L / 1090L) + 410L);

    offset = (uint16_t)(AFE3520_REG_CURH - AFE3520_REG_FLAG1);
    s_snapshot.vadcRaw = Afe3520_Be16(&raw[offset]);

    for (i = 0U; i < AFE3520_CELL_MAX; ++i)
    {
        offset = (uint16_t)(AFE3520_REG_CELL1H - AFE3520_REG_FLAG1 + (uint16_t)i * 2U);
        code = Afe3520_Be16(&raw[offset]);
        /* Reference-board convention: Vcell(mV) = ADC_code * 5 / 32. */
        s_snapshot.cellMv[i] = (uint16_t)(((uint32_t)code * 5UL + 16UL) >> 5);
    }

    offset = (uint16_t)(AFE3520_REG_CADCDH - AFE3520_REG_FLAG1);
    s_snapshot.cadcRaw = Afe3520_Be16(&raw[offset]);
    cadc = (int16_t)s_snapshot.cadcRaw;
    s_snapshot.currentMa = Afe3520_CurrentMaFromCadc(cadc);

    offset = (uint16_t)(AFE3520_REG_VTOPH - AFE3520_REG_FLAG1);
    s_snapshot.vtopRaw = Afe3520_Be16(&raw[offset]);
    offset = (uint16_t)(AFE3520_REG_VCHGRH - AFE3520_REG_FLAG1);
    s_snapshot.chargerRaw = Afe3520_Be16(&raw[offset]);
    offset = (uint16_t)(AFE3520_REG_OWDH - AFE3520_REG_FLAG1);
    s_snapshot.openWireMask = ((uint32_t)raw[offset] << 16) |
                              ((uint32_t)raw[offset + 1U] << 8) |
                              raw[offset + 2U];
    if (++s_snapshot.sampleSequence == 0U) ++s_snapshot.sampleSequence;
    s_snapshot.valid = 1U;
}

AFE3520_RESULT Afe3520_Service(void)
{
    uint8_t raw[AFE3520_REG_LAST - AFE3520_REG_FLAG1 + 1U];
    AFE3520_RESULT result;

    result = Afe3520_Read(AFE3520_REG_FLAG1, raw, (uint8_t)sizeof(raw));
    if (result != AFE3520_OK)
    {
        s_snapshot.valid = 0U;
        s_ready = 0U;
        return result;
    }
    Afe3520_ParseSnapshot(raw);
    if ((s_snapshot.flag1 & AFE3520_FLAG1_RST1) ||
        (s_snapshot.flag2 & AFE3520_FLAG2_RST2)) s_configDirty = 1U;
    s_ready = 1U;
    return AFE3520_OK;
}

AFE3520_RESULT Afe3520_VerifyConfig(const AFE3520_REG_CONFIG *cfg)
{
    uint8_t actual[AFE3520_CONFIG_LENGTH];
    uint8_t i, mask;
    AFE3520_RESULT result;
    if (cfg == 0) return AFE3520_ERR_ARG;
    result = Afe3520_Read(AFE3520_REG_SCONF1, actual, sizeof(actual));
    if (result != AFE3520_OK) return result;
    for (i = 0U; i < AFE3520_CONFIG_LENGTH; ++i)
    {
        if ((cfg->writeMask & (1UL << i)) == 0U) continue;
        /* LTCLR is a command bit. Reference explicitly excludes it. */
        mask = (i == AFE3520_REG_SCONF2 - AFE3520_REG_SCONF1) ? 0x7FU : 0xFFU;
        if ((actual[i] & mask) != (cfg->value[i] & mask))
        {
            ++s_diag.verifyErrorCount;
            Afe3520_SetError(AFE3520_ERR_VERIFY);
            return AFE3520_ERR_VERIFY;
        }
    }
    return AFE3520_OK;
}

AFE3520_RESULT Afe3520_ClearFlags(uint8_t flag1Mask, uint8_t flag2Mask)
{
    AFE3520_RESULT result;
    uint8_t i, flag;
    uint8_t masks[2] = {flag1Mask, flag2Mask};
    /* Fresh read and LTCLR before EACH flag write, as in the reference.
     * Never clear an unrelated latch using a stale 200ms snapshot. */
    for (i = 0U; i < 2U; ++i)
    {
        if (masks[i] == 0U) continue;
        result = Afe3520_Read((uint8_t)(AFE3520_REG_FLAG1 + i), &flag, 1U);
        if (result != AFE3520_OK) return result;
        result = Afe3520_Write(AFE3520_REG_SCONF2,
                              (uint8_t)(s_shadowSconf2 | AFE3520_SCONF2_LTCLR));
        if (result != AFE3520_OK) return result;
        result = Afe3520_Write((uint8_t)(AFE3520_REG_FLAG1 + i),
                              (uint8_t)(flag & (uint8_t)~masks[i]));
        if (result != AFE3520_OK) return result;
    }
    return AFE3520_OK;
}

AFE3520_RESULT Afe3520_ApplyConfig(const AFE3520_REG_CONFIG *cfg)
{
    uint8_t i;
    AFE3520_RESULT result;
    if (cfg == 0) return AFE3520_ERR_ARG;
    s_configDirty = 1U;
    for (i = 0U; i < AFE3520_CONFIG_LENGTH; ++i)
    {
        if ((cfg->writeMask & (1UL << i)) == 0U) continue;
        result = Afe3520_Write((uint8_t)(AFE3520_REG_SCONF1 + i), cfg->value[i]);
        if (result != AFE3520_OK) return result;
    }
    result = Afe3520_VerifyConfig(cfg);
    if (result != AFE3520_OK) return result;
    s_shadowSconf2 = cfg->value[AFE3520_REG_SCONF2 - AFE3520_REG_SCONF1];
    /* Balance must be confirmed off before configuration becomes valid. */
    result = Afe3520_SetBalance(0U);
    if (result != AFE3520_OK) return result;
    /* Only acknowledge resets here. Protection latches need safe recovery. */
    result = Afe3520_ClearFlags(AFE3520_FLAG1_RST1, AFE3520_FLAG2_RST2);
    if (result != AFE3520_OK) return result;
    s_configDirty = 0U;
    ++s_diag.configRepairCount;
    return AFE3520_OK;
}

AFE3520_RESULT Afe3520_SetMos(uint8_t chargeOn, uint8_t dischargeOn, uint8_t preDischargeOn)
{
    uint8_t next = (uint8_t)(s_shadowSconf2 | AFE3520_SCONF2_LTCLR);
    AFE3520_RESULT result;
    next &= (uint8_t)~(AFE3520_SCONF2_CHGMOS | AFE3520_SCONF2_DSGMOS | AFE3520_SCONF2_PDSGMOS);
    if (chargeOn) next |= AFE3520_SCONF2_CHGMOS;
    if (dischargeOn) next |= AFE3520_SCONF2_DSGMOS;
    if (preDischargeOn) next |= AFE3520_SCONF2_PDSGMOS;
    result = Afe3520_Write(AFE3520_REG_SCONF2, next);
    if (result != AFE3520_OK) return result;
    s_shadowSconf2 = next;
    /* Acknowledged command and actual FET feedback are different states. */
    {
        uint8_t status;
        result = Afe3520_Read(AFE3520_REG_BSTATUS1, &status, 1U);
        if (result == AFE3520_OK) s_snapshot.bstatus1 = status;
        return result;
    }
}

AFE3520_RESULT Afe3520_SetBalance(uint32_t mask)
{
    if (Afe3520_Write(AFE3520_REG_BALANCEH, (uint8_t)((mask >> 16) & 0x0FU)) != AFE3520_OK) return AFE3520_ERR_SPI;
    if (Afe3520_Write(AFE3520_REG_BALANCEM, (uint8_t)(mask >> 8)) != AFE3520_OK) return AFE3520_ERR_SPI;
    return Afe3520_Write(AFE3520_REG_BALANCEL, (uint8_t)mask);
}

AFE3520_RESULT Afe3520_EnterIdle(void)
{
    return Afe3520_Write(AFE3520_REG_SCONF1, AFE3520_MODE_IDLE);
}

AFE3520_RESULT Afe3520_EnterSleep(void)
{
    AFE3520_RESULT result = Afe3520_SetBalance(0U);
    if (result != AFE3520_OK) return result;
    result = Afe3520_Write(AFE3520_REG_SCONF1, AFE3520_MODE_SLEEP);
    if (result == AFE3520_OK) Afe3520_Invalidate();
    return result;
}

AFE3520_RESULT Afe3520_EnterPowerDown(void)
{
    AFE3520_RESULT result;
    uint8_t pd = (uint8_t)(s_shadowSconf2 | AFE3520_SCONF2_PD_CTL);

    /* CV1.0A requires these to be consecutive AFE instructions. No read or
     * verify is permitted between PD_CTL and SCONF1=0x33. */
    result = Afe3520_Write(AFE3520_REG_SCONF2, pd);
    if (result != AFE3520_OK) return result;
    result = Afe3520_Write(AFE3520_REG_SCONF1, AFE3520_MODE_POWERDOWN);
    if (result == AFE3520_OK)
    {
        s_ready = 0U;
        s_configDirty = 1U;
    }
    return result;
}

AFE3520_RESULT Afe3520_Init(void)
{
    uint8_t probe;
    memset(&s_snapshot, 0, sizeof(s_snapshot));
    Afe3520_PortInit();
    Delay1ms(5U);
    if (Afe3520_Read(AFE3520_REG_BSTATUS2, &probe, 1U) != AFE3520_OK)
    {
        s_ready = 0U;
        return AFE3520_ERR_SPI;
    }
    s_ready = 1U;
    s_configDirty = 1U;
    return AFE3520_OK;
}

const AFE3520_SNAPSHOT *Afe3520_GetSnapshot(void) { return &s_snapshot; }
const AFE3520_DIAG *Afe3520_GetDiag(void) { return &s_diag; }
uint8_t Afe3520_IsReady(void) { return s_ready; }
uint8_t Afe3520_ConfigDirty(void) { return s_configDirty; }
void Afe3520_MarkConfigDirty(void) { s_configDirty = 1U; }

void Afe3520_Invalidate(void)
{
    s_snapshot.valid = 0U;
    s_ready = 0U;
    s_configDirty = 1U;
}
