#include "main.h"

/* ====== 你按实际改这几个宏就行 ====== */
#define AFE_SPI              SPI1

#define AFE_SPI_RCC_SPI      RCC_APB2Periph_SPI1
#define AFE_SPI_RCC_GPIO     RCC_APB2Periph_GPIOA

#define AFE_PIN_SCK          GPIO_Pin_5   // PA5
#define AFE_PIN_MISO         GPIO_Pin_6   // PA6
#define AFE_PIN_MOSI         GPIO_Pin_7   // PA7
#define AFE_GPIO_SPI         GPIOA

#define AFE_PIN_CS           GPIO_Pin_4   // PA4 (示例)
#define AFE_GPIO_CS          GPIOA

/* ====== 简单 CS 控制 ====== */
static inline void AFE_CS_L(void) { GPIO_ResetBits(AFE_GPIO_CS, AFE_PIN_CS); }
static inline void AFE_CS_H(void) { GPIO_SetBits(AFE_GPIO_CS, AFE_PIN_CS); }

/* ====== 简单超时（避免卡死） ====== */
static int spi_wait_flag(volatile uint16_t *sr, uint16_t mask, uint32_t timeout)
{
    while (((*sr) & mask) == 0u) {
        if (timeout-- == 0u) return -1;
    }
    return 0;
}

/* ====== SPI 收发 1 字节（全双工，写一个同时读一个） ====== */
uint8_t AFE_SPI_TxRxByte(uint8_t tx)
{
    /* 等 TXE */
    if (spi_wait_flag(&AFE_SPI->SR, SPI_I2S_FLAG_TXE, 200000) < 0) return 0xFF;

    SPI_I2S_SendData(AFE_SPI, tx);

    /* 等 RXNE */
    if (spi_wait_flag(&AFE_SPI->SR, SPI_I2S_FLAG_RXNE, 200000) < 0) return 0xFF;

    return (uint8_t)SPI_I2S_ReceiveData(AFE_SPI);
}

/* ====== 可选：收发一段 buffer（常用于读寄存器 N 字节） ====== */
void AFE_SPI_TxRxBuf(const uint8_t *tx, uint8_t *rx, uint16_t len)
{
    while (len--) {
        uint8_t t = tx ? *tx++ : 0x00;
        uint8_t r = AFE_SPI_TxRxByte(t);
        if (rx) *rx++ = r;
    }

    /* 等待总线不忙，确保最后一个字节完全发出 */
    while (SPI_I2S_GetFlagStatus(AFE_SPI, SPI_I2S_FLAG_BSY) == SET) {;}
}

/* ====== 初始化：GPIO + SPI（Mode3） ====== */
void AFE_SPI_Init(void)
{
    GPIO_InitTypeDef gpio;
    SPI_InitTypeDef  spi;

    /* 开时钟：GPIOA + SPI1 */
    RCC_APB2PeriphClockCmd(AFE_SPI_RCC_GPIO | AFE_SPI_RCC_SPI, ENABLE);

    /* SCK / MOSI：AF 推挽输出 */
    gpio.GPIO_Pin   = AFE_PIN_SCK | AFE_PIN_MOSI;
    gpio.GPIO_Mode  = GPIO_Mode_AF_PP;
    gpio.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(AFE_GPIO_SPI, &gpio);

    /* MISO：输入浮空（或按需要改成上拉） */
    gpio.GPIO_Pin   = AFE_PIN_MISO;
    gpio.GPIO_Mode  = GPIO_Mode_IN_FLOATING;
    GPIO_Init(AFE_GPIO_SPI, &gpio);

    /* CS：普通推挽输出，默认拉高（不选中） */
    gpio.GPIO_Pin   = AFE_PIN_CS;
    gpio.GPIO_Mode  = GPIO_Mode_Out_PP;
    gpio.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(AFE_GPIO_CS, &gpio);
    AFE_CS_H();

    /* SPI 参数 */
    SPI_StructInit(&spi);
    spi.SPI_Direction = SPI_Direction_2Lines_FullDuplex;
    spi.SPI_Mode      = SPI_Mode_Master;
    spi.SPI_DataSize  = SPI_DataSize_8b;

    /* 关键：对应 AFE 图里的 CPOL=1 CPHA=1 => Mode 3 */
    spi.SPI_CPOL      = SPI_CPOL_High;
    spi.SPI_CPHA      = SPI_CPHA_2Edge;

    /* NSS 用软件管理（更稳） */
    spi.SPI_NSS       = SPI_NSS_Soft;

    /* MSB first（图里 MSB->LSB） */
    spi.SPI_FirstBit  = SPI_FirstBit_MSB;

    /* 先慢速起步，通了再提速 */
    spi.SPI_BaudRatePrescaler = SPI_BaudRatePrescaler_64;

    spi.SPI_CRCPolynomial = 7; // 不用硬件CRC也无所谓

    SPI_Init(AFE_SPI, &spi);

    /* 重要：主机 + 软件NSS 时，内部 SSI 置 1，避免 MODF */
    SPI_NSSInternalSoftwareConfig(AFE_SPI, SPI_NSSInternalSoft_Set);

    SPI_Cmd(AFE_SPI, ENABLE);

    /* 清一下可能的残留 */
    (void)AFE_SPI->DR;
    (void)AFE_SPI->SR;
}

// ===== 你按文档确认/填写 =====
// static uint8_t afe_crc8(const uint8_t *p, uint16_t len); // 按文档多项式实现
static uint8_t afe_crc8(const uint8_t *p, uint16_t len) // 按文档多项式实现
{
    return 0x55;
}

static uint8_t afe_write_reg_u8(uint8_t reg, uint8_t data)
{
    uint8_t tx[5];
    uint8_t ack;

    tx[0] = 0x01;      // Write CMD
    tx[1] = reg;       // Reg Addr
    tx[2] = data;      // 1 byte
    tx[3] = afe_crc8(tx, 3);
    tx[4] = 0x00;      // dummy per timing

    AFE_CS_L();
    (void)AFE_SPI_TxRxByte(tx[0]);
    (void)AFE_SPI_TxRxByte(tx[1]);
    (void)AFE_SPI_TxRxByte(tx[2]);
    (void)AFE_SPI_TxRxByte(tx[3]);
    ack = AFE_SPI_TxRxByte(tx[4]); // 这里通常读到 0xA5 或 0xFF
    AFE_CS_H();

    return ack; // 0xA5 OK, 0xFF FAIL
}

static int afe_read_reg(uint8_t reg, uint8_t *out, uint8_t len)
{
    uint8_t crc_rx, crc_calc;

    AFE_CS_L();
    (void)AFE_SPI_TxRxByte(0x02);   // Read CMD
    (void)AFE_SPI_TxRxByte(reg);    // Reg Addr
    (void)AFE_SPI_TxRxByte(len);    // Data Length N (不含CRC)

    // 读数据：主机发dummy换回从机数据
    for (uint8_t i = 0; i < len; i++) {
        out[i] = AFE_SPI_TxRxByte(0x00);
    }
    crc_rx = AFE_SPI_TxRxByte(0x00); // 读回 CRC8
    AFE_CS_H();

    // 校验 CRC：通常是对 (CMD+ADDR+LEN+DATA...) 或 (ADDR+LEN+DATA...) 做CRC
    // 具体覆盖范围按文档。下面给一个常见模板：对 [0x02, reg, len, data...] 做CRC
    {
        uint8_t tmp[3 + 255];
        tmp[0] = 0x02;
        tmp[1] = reg;
        tmp[2] = len;
        for (uint8_t i = 0; i < len; i++) tmp[3+i] = out[i];
        crc_calc = afe_crc8(tmp, 3 + len);
    }

    return (crc_calc == crc_rx) ? 0 : -1;
}

// ===== 这些宏你必须按 SH3673520 寄存器表填写 =====
#define REG_SYS_CTRL      0x00  // TODO
#define REG_CELL_CFG      0x00  // TODO
#define REG_VMEAS_EN      0x00  // TODO
#define REG_SCAN_TRIG     0x00  // TODO
#define REG_STATUS        0x00  // TODO
#define REG_STATUS_RDY_MASK  (1u<<0) // TODO: DRDY/SCAN_DONE 位

#define REG_VC_BASE       0x00  // TODO: VC1_H 起始地址（或 RAM 起始地址）
#define VC_BYTES_PER_CELL 2     // 常见：2字节/单体

// LSB：每个ADC码对应多少uV/mV（按文档填写）
#define VC_LSB_uV         1000  // TODO：举例=1000uV=1mV/LSB（别照抄）

static int afe_wait_ready(uint32_t timeout)
{
    uint8_t st;
    while (timeout--) {
        if (afe_read_reg(REG_STATUS, &st, 1) == 0) {
            if (st & REG_STATUS_RDY_MASK) return 0;
        }
    }
    return -1;
}

// 上电配置一次：cell_num 是串数
int afe_cell_voltage_config(uint8_t cell_num)
{
    // 1) 进入正常模式 + ADC使能
    if (afe_write_reg_u8(REG_SYS_CTRL, /*TODO: NORMAL|ADC_EN*/ 0x00) != 0xA5) return -1;

    // 2) 配置串数
    if (afe_write_reg_u8(REG_CELL_CFG, /*TODO: cell_num encode*/ cell_num) != 0xA5) return -1;

    // 3) 使能电压测量
    if (afe_write_reg_u8(REG_VMEAS_EN, /*TODO: enable VC channels*/ 0xFF) != 0xA5) return -1;

    return 0;
}

uint8_t buf[2];
void read_vcell_test(void)
{

    afe_read_reg(0x6B, buf, 2);
}

// 读取全部单体电压，输出 mV
int afe_read_cell_voltages_mv(uint8_t cell_num, uint16_t *mv_out)
{
    uint8_t raw[VC_BYTES_PER_CELL * 16]; // 先按最大16串准备，按你产品改
    uint16_t bytes = (uint16_t)cell_num * VC_BYTES_PER_CELL;

    // 1) 触发一次扫描
    if (afe_write_reg_u8(REG_SCAN_TRIG, /*TODO: trigger*/ 0x01) != 0xA5) return -1;

    // 2) 等待完成
    if (afe_wait_ready(20000) < 0) return -2;

    // 3) 读回结果（推荐一次读一整段连续结果）
    if (afe_read_reg(REG_VC_BASE, raw, (uint8_t)bytes) < 0) return -3;

    // 4) 解析 + 换算
    for (uint8_t i = 0; i < cell_num; i++) {
        uint16_t code = ((uint16_t)raw[2*i] << 8) | raw[2*i + 1]; // 大端/小端按文档改
        // 换算：mV = code * LSB(uV) / 1000
        uint32_t uv = (uint32_t)code * (uint32_t)VC_LSB_uV;
        mv_out[i] = (uint16_t)((uv + 500) / 1000); // 四舍五入
    }

    return 0;
}
