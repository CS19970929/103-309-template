#include "main.h"
#include "afe3520/Afe3520.h"
#include <string.h>

/*
 * CV1.0A specifies CPOL=1/CPHA=1 and <=1MHz. The vendor V1.2 demo calculates
 * write CRC over CMD+ADDR+DATA, while one sentence in CV1.0A says an implicit
 * write-length byte is also covered. The wire diagram has no length byte.
 * We therefore start with vendor-demo behavior and, on NACK only, probe the
 * implicit-length CRC variant once; successful mode is remembered.
 */
#define AFE3520_WRITE_CRC_MODE_UNKNOWN   0U
#define AFE3520_WRITE_CRC_MODE_DEMO      1U
#define AFE3520_WRITE_CRC_MODE_LEN1      2U
#define AFE3520_SPI_DUMMY                0x00U
#define AFE3520_SPI_IDLE                 0xFFU
#define AFE3520_READ_MAX                 ((uint8_t)(AFE3520_REG_LAST - AFE3520_REG_SCONF1 + 1U))

static AFE3520_SNAPSHOT s_snapshot;
static AFE3520_DIAG s_diag;
static AFE3520_REG_CONFIG s_activeConfig;
static uint8_t s_ready;
static uint8_t s_configDirty = 1U;
static uint8_t s_writeCrcMode = AFE3520_WRITE_CRC_MODE_UNKNOWN;
static uint8_t s_shadowSconf2;
static uint8_t s_shadowSconf3;
static uint8_t s_shadowSconf5;
static uint8_t s_shadowSconf6;

extern const UINT16 iSheldTemp_10K_NTC[141];

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
    for (i = 0U; i < length; ++i)
    {
        crc = Afe3520_Crc8Update(crc, data[i]);
    }
    return crc;
}

static void Afe3520_SetError(AFE3520_RESULT result)
{
    s_diag.lastError = result;
}

static void Afe3520_CsLow(void)
{
    GPIO_ResetBits(GPIOA, GPIO_Pin_4);
}

static void Afe3520_CsHigh(void)
{
    GPIO_SetBits(GPIOA, GPIO_Pin_4);
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

    gpio.GPIO_Pin = GPIO_Pin_5 | GPIO_Pin_7;
    gpio.GPIO_Speed = GPIO_Speed_50MHz;
    gpio.GPIO_Mode = GPIO_Mode_AF_PP;
    GPIO_Init(GPIOA, &gpio);

    gpio.GPIO_Pin = GPIO_Pin_6;
    gpio.GPIO_Mode = GPIO_Mode_IN_FLOATING;
    GPIO_Init(GPIOA, &gpio);

    gpio.GPIO_Pin = GPIO_Pin_4;
    gpio.GPIO_Mode = GPIO_Mode_Out_PP;
    GPIO_Init(GPIOA, &gpio);
    Afe3520_CsHigh();

    SPI_I2S_DeInit(SPI1);
    spi.SPI_Direction = SPI_Direction_2Lines_FullDuplex;
    spi.SPI_Mode = SPI_Mode_Master;
    spi.SPI_DataSize = SPI_DataSize_8b;
    spi.SPI_CPOL = SPI_CPOL_High;
    spi.SPI_CPHA = SPI_CPHA_2Edge;
    spi.SPI_NSS = SPI_NSS_Soft;
    /* 72MHz / 128 = 562.5kHz, safely below the 1MHz AFE maximum. */
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
        (rx[2] != reg) || (rx[3] != value))
    {
        return AFE3520_ERR_SPI;
    }
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
    AFE3520_RESULT result;
    uint8_t preferred = s_writeCrcMode;

    if ((reg < AFE3520_REG_SCONF1) || (reg > AFE3520_REG_FLAG2))
    {
        return AFE3520_ERR_ARG;
    }

    if (preferred == AFE3520_WRITE_CRC_MODE_UNKNOWN)
    {
        preferred = AFE3520_WRITE_CRC_MODE_DEMO;
    }

    for (retry = 0U; retry < AFE3520_SPI_RETRY_MAX; ++retry)
    {
        result = Afe3520_WriteOnceMode(reg, value, preferred);
        if (result == AFE3520_OK)
        {
            s_writeCrcMode = preferred;
            Afe3520_SetError(AFE3520_OK);
            return AFE3520_OK;
        }

        /* Only an ACK failure can plausibly be the CV1.0A wording ambiguity. */
        if ((result == AFE3520_ERR_ACK) && (s_writeCrcMode == AFE3520_WRITE_CRC_MODE_UNKNOWN))
        {
            uint8_t alternate = (preferred == AFE3520_WRITE_CRC_MODE_DEMO) ?
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
        (rx2 != reg) || (rx3 != len))
    {
        return AFE3520_ERR_SPI;
    }
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
    AFE3520_RESULT result = AFE3520_ERR_ARG;
    uint16_t last;

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

    /* CADC input range is +/-100mV. Equivalent shunt is CS_Res/CS_Res_Num mOhm.
     * I[mA] = raw * 200000 / 65536 / R[mOhm]
     *       = raw * 3125 / 1024 * CS_Res_Num / CS_Res. */
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

    s_snapshot.flag1 = raw[AFE3520_REG_FLAG1 - AFE3520_REG_FLAG1];
    s_snapshot.flag2 = raw[AFE3520_REG_FLAG2 - AFE3520_REG_FLAG1];
    s_snapshot.flag3 = raw[AFE3520_REG_FLAG3 - AFE3520_REG_FLAG1];
    s_snapshot.bstatus1 = raw[AFE3520_REG_BSTATUS1 - AFE3520_REG_FLAG1];
    s_snapshot.bstatus2 = raw[AFE3520_REG_BSTATUS2 - AFE3520_REG_FLAG1];

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
        /* 13-bit VADC, nominal 5V full scale. */
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

    /* Any AFE reset restores RAM defaults. Detect it and force re-application. */
    if ((s_snapshot.flag1 & AFE3520_FLAG1_RST1) != 0U)
    {
        s_configDirty = 1U;
    }
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
        /* FLAG registers are not part of this image; cfg covers 0x40..0x54 only. */
        if (actual[i] != expected[i])
        {
            ++s_diag.verifyErrorCount;
            Afe3520_SetError(AFE3520_ERR_VERIFY);
            return AFE3520_ERR_VERIFY;
        }
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

    /* Program 0x40..0x54 as one logical transaction. MOS commands in SCONF2 are
     * kept OFF in the stored configuration and are applied only by the arbiter. */
    for (i = 0U; i < (uint8_t)sizeof(*cfg); ++i)
    {
        result = Afe3520_Write((uint8_t)(AFE3520_REG_SCONF1 + i), bytes[i]);
        if (result != AFE3520_OK) return result;
    }
    result = Afe3520_VerifyConfig(cfg);
    if (result != AFE3520_OK) return result;

    s_activeConfig = *cfg;
    s_shadowSconf2 = cfg->sconf2;
    s_shadowSconf3 = cfg->sconf3;
    s_shadowSconf5 = cfg->sconf5;
    s_shadowSconf6 = cfg->sconf6;
    s_configDirty = 0U;
    ++s_diag.configRepairCount;
    return AFE3520_OK;
}

AFE3520_RESULT Afe3520_ClearFlags(uint8_t flag1Mask, uint8_t flag2Mask)
{
    AFE3520_RESULT result;
    uint8_t flag;

    s_shadowSconf2 |= AFE3520_SCONF2_LTCLR;
    result = Afe3520_Write(AFE3520_REG_SCONF2, s_shadowSconf2);
    if (result != AFE3520_OK) return result;

    if (flag1Mask != 0U)
    {
        flag = (uint8_t)(s_snapshot.flag1 & (uint8_t)~flag1Mask);
        result = Afe3520_Write(AFE3520_REG_FLAG1, flag);
        if (result != AFE3520_OK) return result;
    }
    if (flag2Mask != 0U)
    {
        flag = (uint8_t)(s_snapshot.flag2 & (uint8_t)~flag2Mask);
        result = Afe3520_Write(AFE3520_REG_FLAG2, flag);
    }
    return result;
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
    /* CV1.0A: these two writes must be consecutive AFE instructions. */
    result = Afe3520_Write(AFE3520_REG_SCONF2, pd);
    if (result != AFE3520_OK) return result;
    result = Afe3520_Write(AFE3520_REG_SCONF1, AFE3520_MODE_POWERDOWN);
    if (result == AFE3520_OK) s_ready = 0U;
    return result;
}

AFE3520_RESULT Afe3520_TriggerOpenWire(void)
{
    uint8_t value = (uint8_t)(s_shadowSconf3 | 0x03U);
    AFE3520_RESULT result = Afe3520_Write(AFE3520_REG_SCONF3, value);
    if (result == AFE3520_OK) s_shadowSconf3 = value;
    return result;
}

AFE3520_RESULT Afe3520_Init(void)
{
    uint8_t probe;
    memset(&s_snapshot, 0, sizeof(s_snapshot));
    memset(&s_diag, 0, sizeof(s_diag));
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
    if ((table == 0) || (count == 0U)) return 0U;
    bestDiff = (targetMs > table[0]) ? (uint32_t)(targetMs - table[0]) : (uint32_t)(table[0] - targetMs);
    for (i = 1U; i < count; ++i)
    {
        uint32_t diff = (targetMs > table[i]) ? (uint32_t)(targetMs - table[i]) : (uint32_t)(table[i] - targetMs);
        if (diff < bestDiff) { best = i; bestDiff = diff; }
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
