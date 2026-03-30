#include "sh36735_spi.h"
#include "conf.h"

// 如果你实测写 CRC 需要包含“长度=1”这个字节，把下面改成 1
#ifndef SH_WRITE_CRC_INCLUDE_LEN
#define SH_WRITE_CRC_INCLUDE_LEN 0
#endif

uint8_t sh36735_crc8(const uint8_t *buf, uint32_t len)
{
    // poly 0x07, init 0x00, no xorout
    uint8_t crc = 0x00;
    for (uint32_t i = 0; i < len; i++) {
        crc ^= buf[i];
        for (int b = 0; b < 8; b++) {
            if (crc & 0x80) crc = (uint8_t)((crc << 1) ^ 0x07);
            else            crc = (uint8_t)(crc << 1);
        }
    }
    return crc;
}

static inline uint8_t sh_crc8_write(uint8_t cmd, uint8_t reg, uint8_t val)
{
#if SH_WRITE_CRC_INCLUDE_LEN
    uint8_t tmp[4] = { cmd, reg, 0x01, val };
    return sh36735_crc8(tmp, 4);
#else
    uint8_t tmp[3] = { cmd, reg, val };
    return sh36735_crc8(tmp, 3);
#endif
}

bool sh36735_write_reg_u8(uint8_t reg, uint8_t val)
{
    uint8_t crc = sh_crc8_write(SH_SPI_CMD_WRITE_REG, reg, val);

    sh_cs_low();
    sh_delay_us(1);

    (void)sh36735_spi_xfer(SH_SPI_CMD_WRITE_REG);
    (void)sh36735_spi_xfer(reg);
    (void)sh36735_spi_xfer(val);
    uint8_t ack = sh36735_spi_xfer(crc);
    sh36735_spi_xfer(0x00);

    sh_delay_us(1);
    sh_cs_high();

    return (ack == SH_SPI_ACK_OK);
}

bool sh36735_read_regs(uint8_t reg, uint8_t *buf, uint8_t n)
{
    uint8_t *p = (uint8_t *)buf;
    // uint8_t n = (uint8_t)(n / 3);

    if (!buf || n == 0) return false;

    sh_cs_low();
    sh_delay_us(100);

    // 发送读命令同时收到一个字节（通常为 0xFF），CRC 需要从该字节开始
    uint8_t rx0 = sh36735_spi_xfer(SH_SPI_CMD_READ_REG);

    (void)sh36735_spi_xfer(reg);
    (void)sh36735_spi_xfer(n);

    sh36735_spi_xfer(0x00);

    for (uint8_t i = 0; i < n; i++) {
        buf[i] = sh36735_spi_xfer(0x00);
    }

    //  for (uint8_t i = 0; i < n; i += 2) {
    //     uint8_t t = p[i];
    //     p[i] = p[i + 1];
    //     p[i + 1] = t;
    // }

    uint8_t crc_rx = sh36735_spi_xfer(0x00);

    sh_delay_us(1);
    sh_cs_high();

    // CRC over {rx0, 0x02, reg, n, data...}
    uint8_t tmp[4 + 255];
    tmp[0] = rx0;
    tmp[1] = SH_SPI_CMD_READ_REG;
    tmp[2] = reg;
    tmp[3] = n;
    for (uint8_t i = 0; i < n; i++) tmp[4 + i] = buf[i];

    uint8_t crc = sh36735_crc8(tmp, (uint32_t)(4 + n));
    if(crc != crc_rx)
        sys_time.crc_err = true;
    else
        sys_time.crc_err = false;
        
    return (crc == crc_rx);
}

bool sh36735_sw_reset(void)
{
    uint8_t tmp[3] = { SH_SPI_CMD_SW_RESET, 0xBB, 0xCC };
    uint8_t crc = sh36735_crc8(tmp, 3);

    sh_cs_low();
    sh_delay_us(1);

    (void)sh36735_spi_xfer(SH_SPI_CMD_SW_RESET);
    (void)sh36735_spi_xfer(0xBB);
    (void)sh36735_spi_xfer(0xCC);
    uint8_t ack = sh36735_spi_xfer(crc);

    sh_delay_us(1);
    sh_cs_high();

    return (ack == SH_SPI_ACK_OK);
}
