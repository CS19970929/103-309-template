#include "sh36735_spi.h"
#include "conf.h"

#ifndef SH_WRITE_CRC_INCLUDE_LEN
#define SH_WRITE_CRC_INCLUDE_LEN 0
#endif

#ifndef SH_SPI_RETRY_MAX
#define SH_SPI_RETRY_MAX 5u
#endif

#define SH_SPI_READ_IDLE_BYTE 0xFFu

static uint8_t sh36735_crc8_update(uint8_t crc, uint8_t data)
{
    int b;

    crc ^= data;
    for (b = 0; b < 8; b++) {
        if (crc & 0x80) {
            crc = (uint8_t)((crc << 1) ^ 0x07);
        } else {
            crc = (uint8_t)(crc << 1);
        }
    }

    return crc;
}

uint8_t sh36735_crc8(const uint8_t *buf, uint32_t len)
{
    uint8_t crc = 0x00;
    uint32_t i;

    for (i = 0; i < len; i++) {
        crc = sh36735_crc8_update(crc, buf[i]);
    }

    return crc;
}

static void sh_spi_begin_frame(void)
{
    sh_cs_high();
    sh_delay_us(1);
    sh_cs_low();
    sh_delay_us(1);
}

static void sh_spi_end_frame(void)
{
    sh_delay_us(1);
    sh_cs_high();
    sh_delay_us(1);
}

static void sh_spi_set_error(bool error)
{
    sys_time.crc_err = error;
}

static uint8_t sh_crc8_write(uint8_t cmd, uint8_t reg, uint8_t val)
{
#if SH_WRITE_CRC_INCLUDE_LEN
    uint8_t tmp[4] = { cmd, reg, 0x01, val };
    return sh36735_crc8(tmp, 4);
#else
    uint8_t tmp[3] = { cmd, reg, val };
    return sh36735_crc8(tmp, 3);
#endif
}

static bool sh36735_write_reg_u8_once(uint8_t reg, uint8_t val)
{
    uint8_t crc = sh_crc8_write(SH_SPI_CMD_WRITE_REG, reg, val);
    uint8_t rx0;
    uint8_t rx1;
    uint8_t rx2;
    uint8_t rx3;
    uint8_t rx4;

    sh_spi_begin_frame();

    rx0 = sh36735_spi_xfer(SH_SPI_CMD_WRITE_REG);
    rx1 = sh36735_spi_xfer(reg);
    rx2 = sh36735_spi_xfer(val);
    rx3 = sh36735_spi_xfer(crc);
    rx4 = sh36735_spi_xfer(0x00);

    sh_spi_end_frame();

    return ((rx0 == SH_SPI_READ_IDLE_BYTE)
            && (rx1 == SH_SPI_CMD_WRITE_REG)
            && (rx2 == reg)
            && (rx3 == val)
            && (rx4 == SH_SPI_ACK_OK));
}

bool sh36735_write_reg_u8(uint8_t reg, uint8_t val)
{
    uint8_t retry;

    for (retry = 0; retry < SH_SPI_RETRY_MAX; ++retry) {
        if (sh36735_write_reg_u8_once(reg, val)) {
            sh_spi_set_error(false);
            return true;
        }
        sh_delay_us(1000);
    }

    sh_spi_set_error(true);
    return false;
}

static bool sh36735_read_regs_once(uint8_t reg, uint8_t *buf, uint8_t n)
{
    uint8_t rx0;
    uint8_t rx1;
    uint8_t rx2;
    uint8_t rx3;
    uint8_t crc_rx;
    uint8_t crc;
    uint8_t data[255];
    uint8_t i;
    bool frame_ok;

    sh_spi_begin_frame();

    rx0 = sh36735_spi_xfer(SH_SPI_CMD_READ_REG);
    rx1 = sh36735_spi_xfer(reg);
    rx2 = sh36735_spi_xfer(n);
    rx3 = sh36735_spi_xfer(0x00);

    for (i = 0; i < n; i++) {
        data[i] = sh36735_spi_xfer(0x00);
    }

    crc_rx = sh36735_spi_xfer(0x00);

    sh_spi_end_frame();

    crc = 0x00;
    crc = sh36735_crc8_update(crc, rx0);
    crc = sh36735_crc8_update(crc, rx1);
    crc = sh36735_crc8_update(crc, rx2);
    crc = sh36735_crc8_update(crc, rx3);
    for (i = 0; i < n; i++) {
        crc = sh36735_crc8_update(crc, data[i]);
    }

    frame_ok = ((rx0 == SH_SPI_READ_IDLE_BYTE)
                && (rx1 == SH_SPI_CMD_READ_REG)
                && (rx2 == reg)
                && (rx3 == n)
                && (crc == crc_rx));

    if (frame_ok) {
        for (i = 0; i < n; i++) {
            buf[i] = data[i];
        }
    }

    return frame_ok;
}

bool sh36735_read_regs(uint8_t reg, uint8_t *buf, uint8_t n)
{
    uint8_t retry;

    if (!buf || n == 0) {
        sh_spi_set_error(true);
        return false;
    }

    for (retry = 0; retry < SH_SPI_RETRY_MAX; ++retry) {
        if (sh36735_read_regs_once(reg, buf, n)) {
            sh_spi_set_error(false);
            return true;
        }
        sh_delay_us(1000);
    }

    sh_spi_set_error(true);
    return false;
}

static bool sh36735_sw_reset_once(void)
{
    uint8_t tmp[3] = { SH_SPI_CMD_SW_RESET, 0xBB, 0xCC };
    uint8_t crc = sh36735_crc8(tmp, 3);
    uint8_t rx0;
    uint8_t rx1;
    uint8_t rx2;
    uint8_t rx3;
    uint8_t rx4;

    sh_spi_begin_frame();

    rx0 = sh36735_spi_xfer(SH_SPI_CMD_SW_RESET);
    rx1 = sh36735_spi_xfer(0xBB);
    rx2 = sh36735_spi_xfer(0xCC);
    rx3 = sh36735_spi_xfer(crc);
    rx4 = sh36735_spi_xfer(0x00);

    sh_spi_end_frame();

    return ((rx0 == SH_SPI_READ_IDLE_BYTE)
            && (rx1 == SH_SPI_CMD_SW_RESET)
            && (rx2 == 0xBB)
            && (rx3 == 0xCC)
            && (rx4 == SH_SPI_ACK_OK));
}

bool sh36735_sw_reset(void)
{
    uint8_t retry;

    for (retry = 0; retry < SH_SPI_RETRY_MAX; ++retry) {
        if (sh36735_sw_reset_once()) {
            sh_spi_set_error(false);
            return true;
        }
        sh_delay_us(1000);
    }

    sh_spi_set_error(true);
    return false;
}
