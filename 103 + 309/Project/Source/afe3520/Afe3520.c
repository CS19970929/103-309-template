#include "main.h"
#include "afe3520/Afe3520.h"
#include "afe3520/Afe3520Board.h"
#include <string.h>

/*
 * CV1.0A specifies SPI mode 3 (CPOL=1/CPHA=1), <=1MHz and CRC8
 * x^8+x^2+x+1, init=0. The vendor V1.2 demo calculates write CRC over
 * CMD+ADDR+DATA, while one sentence in CV1.0A says a length byte participates.
 * The write wire diagram has no length field. Start with the shipping demo
 * behavior; only after a valid echoed frame followed by NACK, probe LEN=1 CRC.
 */
#define AFE3520_WRITE_CRC_MODE_UNKNOWN   0U
#define AFE3520_WRITE_CRC_MODE_DEMO      1U
#define AFE3520_WRITE_CRC_MODE_LEN1      2U
#define AFE3520_SPI_DUMMY                0x00U
#define AFE3520_SPI_IDLE                 0xFFU
#define AFE3520_SCONF3_OWD_TRG           0x01U

static AFE3520_SNAPSHOT s_snapshot;
static AFE3520_DIAG s_diag;
static uint8_t s_ready;
static uint8_t s_configDirty = 1U;
static uint8_t s_writeCrcMode = AFE3520_WRITE_CRC_MODE_UNKNOWN;
static uint8_t s_shadowSconf2;
static uint8_t s_shadowSconf3;

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
}

static void Afe3520_CsLow(void)
{
    GPIO_ResetBits(AFE3520_GPIO_SPI, AFE3520_PIN_CS);
}

static void Afe3520_CsHigh(void)
{
    GPIO_SetBits(AFE3520_GPIO_SPI, AFE3520_PIN_CS);
}

static uint8_t Afe3520_SpiByte(uint8_t tx)
{
    uint32_t guard = 100000UL;
    while ((SPI1->SR & SPI_SR_TXE) == 0U)
    {
        if (--guard == 0U)
        {
            Afe3520_SetError(AFE3520_ERR_SPI);
            return 0xFFU;
        }
    }
    *(__IO uint8_t *)&SPI1->DR = tx;
    guard = 100000UL;
    while ((SPI1->SR & SPI_SR_RXNE) == 0U)
    {
        if (--guard == 0U)
        {
            Afe3520_SetError(AFE3520_ERR_SPI);
            return 0xFFU;
        }
    }
    return *(__IO uint8_t *)&SPI1->DR;
}

static void Afe3520_BeginFrame(void)
{
    Afe3520_CsHigh();
    __NOP(); __NOP(); __NOP();
    Afe3520_CsLow();
    __NOP(); __NOP(); __NOP();
}

static void Afe3520_EndFrame(void)
{
    __NOP(); __NOP(); __NOP();
    Afe3520_CsHigh();
    __NOP(); __NOP(); __NOP();
}

void Afe3520_PortInit(void)
{
    GPIO_InitTypeDef gpio;
    SPI_InitTypeDef spi;

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA | RCC_APB2Periph_AFIO | RCC_APB2Periph_SPI1, ENABLE);

    gpio.GPIO_Pin = AFE3520_PIN_SCK | AFE3520_PIN_MOSI;
    gpio.GPIO_Speed = GPIO_Speed_50MHz;
    gpio.GPIO_Mode = GPIO_Mode_AF_PP;
    GPIO_Init(AFE3520_GPIO_SPI, &gpio);

    gpio.GPIO_Pin = AFE3520_PIN_MISO;
    gpio.GPIO_Mode = GPIO_Mode_IN_FLOATING;
    GPIO_Init(AFE3520_GPIO_SPI, &gpio);

    gpio.GPIO_Pin = AFE3520_PIN_CS;
    gpio.GPIO_Mode = GPIO_Mode_Out_PP;
    GPIO_Init(AFE3520_GPIO_SPI, &gpio);
    Afe3520_CsHigh();

    SPI_I2S_DeInit(SPI1);
    spi.SPI_Direction = SPI_Direction_2Lines_FullDuplex;
    spi.SPI_Mode = SPI_Mode_Master;
    spi.SPI_DataSize = SPI_DataSize_8b;
    spi.SPI_CPOL = SPI_CPOL_High;
    spi.SPI_CPHA = SPI_CPHA_2Edge;
    spi.SPI_NSS = SPI_NSS_Soft;
    /* 72MHz/128=562.5kHz, below the SH3673520 1MHz ceiling. */
    spi.SPI_BaudRatePrescaler = SPI_BaudRatePrescaler_128;
    spi.SPI_FirstBit = SPI_FirstBit_MSB;
    spi.SPI_CRCPolynomial = 7U;
    SPI_Init(SPI1, &spi);
    SPI_Cmd(SPI1, ENABLE);
}

static uint8_t Afe3520_WriteCrc(uint8_t reg, uint8_t value, uint8_t mode)
{
    uint8_t bytes[4];
    bytes[0] = AFE3520_CMD_WRITE;
    bytes[1] = reg;
    if (mode == AFE3520_WRITE_CRC_MODE_LEN1)
    {
        bytes[2] = 1U;
        bytes[3] = value;
        return Afe3520_Crc8(bytes, 4U);
    }
    bytes[2] = value;
    return Afe3520_Crc8(bytes, 3U);
}

static AFE3520_RESULT Afe3520_WriteOnceMode(uint8_t reg, uint8_t value, uint8_t mode)
{
    uint8_t rx[5];
    uint8_t crc = Afe3520_WriteCrc(reg, value, mode);

    Afe3520_BeginFrame();
    rx[0] = Afe3520_SpiByte(AFE3520_CMD_WRITE);
    rx[1] = Afe3520_SpiByte(reg);
    rx[2] = Afe3520_SpiByte(value);
    rx[3] = Afe3520_SpiByte(crc);
    rx[4] = Afe3520_SpiByte(AFE3520_SPI_DUMMY);
    Afe3520_EndFrame();
    ++s_diag.transferCount;

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
    uint8_t preferred = s_writeCrcMode;
    uint8_t alternate;
    AFE3520_RESULT result = AFE3520_ERR_ARG;

    if ((reg < AFE3520_REG_SCONF1) || (reg > AFE3520_REG_FLAG2)) return AFE3520_ERR_ARG;
    if (preferred == AFE3520_WRITE_CRC_MODE_UNKNOWN) preferred = AFE3520_WRITE_CRC_MODE_DEMO;

    for (retry = 0U; retry < AFE3520_SPI_RETRY_MAX; ++retry)
    {
        result = Afe3520_WriteOnceMode(reg, value, preferred);
        if (result == AFE3520_OK)
        {
            s_writeCrcMode = preferred;
            Afe3520_SetError(AFE3520_OK);
            return AFE3520_OK;
        }

        if ((result == AFE3520_ERR_ACK) && (s_writeCrcMode == AFE3520_WRITE_CRC_MODE_UNKNOWN))
        {
            alternate = (preferred == AFE3520_WRITE_CRC_MODE_DEMO) ?
                        AFE3520_WRITE_CRC_MODE_LEN1 : AFE3520_WRITE_CRC_MODE_DEMO;
            result = Afe3520_WriteOnceMode(reg, value, alternate);
            if (result == AFE3520_OK)
            {
                s_writeCrcMode = alternate;
                Afe3520_SetError(AFE3520_OK);
                return AFE3520_OK;
            }
        }
        ++s_diag.retryCount;
        Delay1ms(1U);
    }
    Afe3520_SetError(result);
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
        Delay1ms(1U);
    }
    Afe3520_SetError(result);
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
        if ((rx[0] == 0xFFU) && (rx[1] == tx[0]) && (rx[2] == tx[1]) &&
            (rx[3] == tx[2]) && (rx[4] == AFE3520_ACK_OK))
        {
            ++s_diag.resetCount;
            s_ready = 0U;
            s_configDirty = 1U;
            Delay1ms(5U);
            return AFE3520_OK;
        }
        ++s_diag.retryCount;
        Delay1ms(1U);
    }
    Afe3520_SetError(AFE3520_ERR_ACK);
    return AFE3520_ERR_ACK;
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
        s_snapshot.cellMv[i] = (uint16_t)(((uint32_t)code * 5000UL + 4096UL) / 8192UL);
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
    if ((s_snapshot.flag1 & AFE3520_FLAG1_RST1) != 0U) s_configDirty = 1U;
    s_ready = 1U;
    return AFE3520_OK;
}

static const uint8_t *Afe3520_ConfigBytes(const AFE3520_REG_CONFIG *cfg)
{
    return (const uint8_t *)cfg;
}

AFE3520_RESULT Afe3520_VerifyConfig(const AFE3520_REG_CONFIG *cfg)
{
    uint8_t actual[21];
    const uint8_t *expected;
    uint8_t i;

    if (cfg == 0) return AFE3520_ERR_ARG;
    if (Afe3520_Read(AFE3520_REG_SCONF1, actual, (uint8_t)sizeof(actual)) != AFE3520_OK)
        return AFE3520_ERR_SPI;
    expected = Afe3520_ConfigBytes(cfg);
    for (i = 0U; i < (uint8_t)sizeof(actual); ++i)
    {
        if (actual[i] != expected[i])
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
    uint8_t flags[2];
    uint8_t next;

    /* Never clear from a cached snapshot: a new fault could arrive between the
     * 200ms sample and recovery. Read both flag bytes immediately before the
     * LTCLR write and preserve every bit not explicitly requested for clear. */
    result = Afe3520_Read(AFE3520_REG_FLAG1, flags, 2U);
    if (result != AFE3520_OK) return result;

    s_shadowSconf2 |= AFE3520_SCONF2_LTCLR;
    result = Afe3520_Write(AFE3520_REG_SCONF2, s_shadowSconf2);
    if (result != AFE3520_OK) return result;

    if (flag1Mask != 0U)
    {
        next = (uint8_t)(flags[0] & (uint8_t)~flag1Mask);
        result = Afe3520_Write(AFE3520_REG_FLAG1, next);
        if (result != AFE3520_OK) return result;
    }
    if (flag2Mask != 0U)
    {
        next = (uint8_t)(flags[1] & (uint8_t)~flag2Mask);
        result = Afe3520_Write(AFE3520_REG_FLAG2, next);
        if (result != AFE3520_OK) return result;
    }
    return AFE3520_OK;
}

AFE3520_RESULT Afe3520_ApplyConfig(const AFE3520_REG_CONFIG *cfg)
{
    const uint8_t *bytes;
    uint8_t i;
    AFE3520_RESULT result;

    if (cfg == 0) return AFE3520_ERR_ARG;
    bytes = Afe3520_ConfigBytes(cfg);
    for (i = 0U; i < (uint8_t)sizeof(*cfg); ++i)
    {
        result = Afe3520_Write((uint8_t)(AFE3520_REG_SCONF1 + i), bytes[i]);
        if (result != AFE3520_OK) return result;
    }
    result = Afe3520_VerifyConfig(cfg);
    if (result != AFE3520_OK) return result;

    s_shadowSconf2 = cfg->sconf2;
    s_shadowSconf3 = (uint8_t)(cfg->sconf3 & (uint8_t)~AFE3520_SCONF3_OWD_TRG);
    s_configDirty = 0U;
    ++s_diag.configRepairCount;

    /* RST1 remains latched after reset while all RAM config has already been
     * restored and read-back verified. Clear only RST1 now, otherwise Service
     * would mark the image dirty on every 200ms cycle and rewrite Flash-facing
     * configuration indefinitely. */
    result = Afe3520_ClearFlags(AFE3520_FLAG1_RST1, 0U);
    if (result != AFE3520_OK)
    {
        s_configDirty = 1U;
        return result;
    }
    return AFE3520_OK;
}

AFE3520_RESULT Afe3520_SetMos(uint8_t chargeOn, uint8_t dischargeOn, uint8_t preDischargeOn)
{
    uint8_t next = s_shadowSconf2;
    next &= (uint8_t)~(AFE3520_SCONF2_CHGMOS | AFE3520_SCONF2_DSGMOS | AFE3520_SCONF2_PDSGMOS);
    if (chargeOn) next |= AFE3520_SCONF2_CHGMOS;
    if (dischargeOn) next |= AFE3520_SCONF2_DSGMOS;
    if (preDischargeOn) next |= AFE3520_SCONF2_PDSGMOS;
    if (Afe3520_Write(AFE3520_REG_SCONF2, next) == AFE3520_OK)
    {
        s_shadowSconf2 = next;
        return AFE3520_OK;
    }
    return AFE3520_ERR_SPI;
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
    return Afe3520_Write(AFE3520_REG_SCONF1, AFE3520_MODE_SLEEP);
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

AFE3520_RESULT Afe3520_TriggerOpenWire(void)
{
    uint8_t trigger = (uint8_t)(s_shadowSconf3 | AFE3520_SCONF3_OWD_TRG);
    /* OWD_TRG is a command bit, not persistent configuration. Never copy it
     * back into the shadow image; subsequent config verification expects 0. */
    return Afe3520_Write(AFE3520_REG_SCONF3, trigger);
}

AFE3520_RESULT Afe3520_Init(void)
{
    uint8_t probe;
    memset(&s_snapshot, 0, sizeof(s_snapshot));
    memset(&s_diag, 0, sizeof(s_diag));
    s_writeCrcMode = AFE3520_WRITE_CRC_MODE_UNKNOWN;
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

uint8_t Afe3520_EncodeOvUvMv(uint16_t mv, uint8_t *hi2, uint8_t *lo8)
{
    uint16_t code;
    if ((hi2 == 0) || (lo8 == 0)) return 0U;
    code = (uint16_t)(((uint32_t)mv + 2UL) / 5UL);
    if (code > 0x03FFU) code = 0x03FFU;
    *hi2 = (uint8_t)((code >> 8) & 0x03U);
    *lo8 = (uint8_t)code;
    return 1U;
}

uint8_t Afe3520_PickDelayCode(const uint16_t *table, uint8_t count, uint16_t targetMs)
{
    uint8_t i;
    uint8_t best = 0U;
    uint32_t bestDiff;
    uint32_t diff;
    if ((table == 0) || (count == 0U)) return 0U;
    bestDiff = (targetMs > table[0]) ? (uint32_t)(targetMs - table[0]) : (uint32_t)(table[0] - targetMs);
    for (i = 1U; i < count; ++i)
    {
        diff = (targetMs > table[i]) ? (uint32_t)(targetMs - table[i]) : (uint32_t)(table[i] - targetMs);
        if (diff < bestDiff)
        {
            best = i;
            bestDiff = diff;
        }
    }
    return best;
}

uint8_t Afe3520_EncodeOcd1Mv(uint16_t senseMv)
{
    uint16_t code = (senseMv <= 5U) ? 0U : (uint16_t)((senseMv - 5U + 2U) / 5U);
    return (uint8_t)((code > 15U) ? 15U : code);
}

uint8_t Afe3520_EncodeOcd2Mv(uint16_t senseMv)
{
    uint16_t code = (senseMv <= 10U) ? 0U : (uint16_t)((senseMv - 10U + 5U) / 10U);
    return (uint8_t)((code > 15U) ? 15U : code);
}

uint8_t Afe3520_EncodeOccUv(uint32_t senseUv)
{
    uint32_t code = (senseUv <= 1375UL) ? 0UL : ((senseUv + 687UL) / 1375UL) - 1UL;
    return (uint8_t)((code > 31UL) ? 31UL : code);
}
