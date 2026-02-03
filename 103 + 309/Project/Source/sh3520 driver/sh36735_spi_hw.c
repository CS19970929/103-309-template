#include "sh36735_spi.h"

static void sh_spi_gpio_init(void)
{
    GPIO_InitTypeDef gpio;

    RCC_APB2PeriphClockCmd(SH_SPI_GPIO_CLK | SH_CS_GPIO_CLK | SH_SPIx_CLK, ENABLE);

    // CS: 推挽输出，默认高
    gpio.GPIO_Pin   = SH_CS_PIN;
    gpio.GPIO_Speed = GPIO_Speed_50MHz;
    gpio.GPIO_Mode  = GPIO_Mode_Out_PP;
    GPIO_Init(SH_CS_PORT, &gpio);
    sh_cs_high();

    // SCK/MOSI: 复用推挽
    gpio.GPIO_Pin   = SH_SPI_SCK_PIN | SH_SPI_MOSI_PIN;
    gpio.GPIO_Speed = GPIO_Speed_50MHz;
    gpio.GPIO_Mode  = GPIO_Mode_AF_PP;
    GPIO_Init(SH_SPI_PORT, &gpio);

    // MISO: 浮空输入（你也可改成上拉输入）
    gpio.GPIO_Pin   = SH_SPI_MISO_PIN;
    gpio.GPIO_Mode  = GPIO_Mode_IN_FLOATING;
    GPIO_Init(SH_SPI_PORT, &gpio);
}

void sh36735_spi_hw_init(uint32_t pclk_hz, uint32_t spi_hz)
{
    (void)pclk_hz; // StdPeriphLib 下我们直接选分频（粗略），也可用该参数精确算

    sh_spi_gpio_init();

    SPI_InitTypeDef spi;

    SPI_I2S_DeInit(SH_SPIx);

    spi.SPI_Direction = SPI_Direction_2Lines_FullDuplex;
    spi.SPI_Mode      = SPI_Mode_Master;
    spi.SPI_DataSize  = SPI_DataSize_8b;

    // 关键：CPOL=1, CPHA=1 => SPI Mode 3
    spi.SPI_CPOL      = SPI_CPOL_High;
    spi.SPI_CPHA      = SPI_CPHA_2Edge;

    spi.SPI_NSS       = SPI_NSS_Soft;
    spi.SPI_FirstBit  = SPI_FirstBit_MSB;

    // 选择分频，使 SCK <= 1MHz
    // SPI clk = PCLK / prescaler. PCLK2 常见 72MHz -> /64=1.125MHz(/128=562.5k)
    // 为稳妥默认用 /128，避免超过 1MHz。
    // 你也可以根据 spi_hz 计算更合适的 prescaler。
    if (spi_hz >= 900000) {
        spi.SPI_BaudRatePrescaler = SPI_BaudRatePrescaler_128;
    } else if (spi_hz >= 450000) {
        spi.SPI_BaudRatePrescaler = SPI_BaudRatePrescaler_128;
    } else if (spi_hz >= 225000) {
        spi.SPI_BaudRatePrescaler = SPI_BaudRatePrescaler_256; // 若库无 256，改回 128
    } else {
        spi.SPI_BaudRatePrescaler = SPI_BaudRatePrescaler_128;
    }

    spi.SPI_CRCPolynomial = 7;

    SPI_Init(SH_SPIx, &spi);
    SPI_Cmd(SH_SPIx, ENABLE);

    // 让 SCK 进入空闲高（CPOL=1），一般硬件自动保证；这里给一点时间
    sh_delay_us(10);
}

uint8_t sh36735_spi_xfer(uint8_t tx)
{
    // 等待 TXE
    while (SPI_I2S_GetFlagStatus(SH_SPIx, SPI_I2S_FLAG_TXE) == RESET) {}
    SPI_I2S_SendData(SH_SPIx, tx);

    // 等待 RXNE
    while (SPI_I2S_GetFlagStatus(SH_SPIx, SPI_I2S_FLAG_RXNE) == RESET) {}
    return (uint8_t)SPI_I2S_ReceiveData(SH_SPIx);
}
