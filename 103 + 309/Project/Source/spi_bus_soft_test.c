/* soft_spi_mode3_ic.c
 *
 * Software SPI Mode 3 (CPOL=1, CPHA=1)
 * Protocol:
 *  - Write:  [0x01][RegAddr][WriteData][CRC8] then send dummy 0x00 to read ACK (0xA5 OK / 0xFF FAIL)
 *            (doc says write length fixed 1 byte; CRC covers from 0x01 ...)
 *  - Read:   Master sends [0x02][RegAddr][Len][dummy...]
 *            Slave returns leading 0xFF then data then CRC8 at end (CRC includes 0xFF,0x02,addr,len,data...)
 *  - Reset:  [0x0B][0xBB][0xCC][CRC8] then dummy 0x00 to read ACK
 *
 * CRC8: poly = x^8 + x^2 + x + 1 => 0x07, init = 0x00, no reflect, no xorout
 */

#include "stm32f10x.h"
#include <stdint.h>
#include <stddef.h>

/* ===================== 用户按硬件修改这部分 ===================== */
#define SOFTSPI_GPIO_RCC()      RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE)

/* 示例：PA5=SCK, PA7=MOSI, PA6=MISO, PA4=CS */
#define SOFTSPI_GPIO            GPIOA
#define SOFTSPI_PIN_SCK         GPIO_Pin_5
#define SOFTSPI_PIN_MOSI        GPIO_Pin_7
#define SOFTSPI_PIN_MISO        GPIO_Pin_6
#define SOFTSPI_PIN_CS          GPIO_Pin_4

/* 频率不确定时先用“慢”延时，调通后再缩小 */
#ifndef SOFTSPI_DELAY_CYCLES
#define SOFTSPI_DELAY_CYCLES    (60u)   /* 72MHz 时大概是很慢的 SPI */
#endif

static inline void softspi_delay(void)
{
    /* 简单空转延时：可靠优先 */
    for (volatile uint32_t i = 0; i < SOFTSPI_DELAY_CYCLES; i++) {
        __NOP();
    }
}

/* GPIO 快捷操作（用 BSRR/BRR 更稳更快） */
static inline void CS_LOW(void)   { SOFTSPI_GPIO->BRR  = SOFTSPI_PIN_CS; }
static inline void CS_HIGH(void)  { SOFTSPI_GPIO->BSRR = SOFTSPI_PIN_CS; }

static inline void SCK_LOW(void)  { SOFTSPI_GPIO->BRR  = SOFTSPI_PIN_SCK; }
static inline void SCK_HIGH(void) { SOFTSPI_GPIO->BSRR = SOFTSPI_PIN_SCK; }

static inline void MOSI_LOW(void) { SOFTSPI_GPIO->BRR  = SOFTSPI_PIN_MOSI; }
static inline void MOSI_HIGH(void){ SOFTSPI_GPIO->BSRR = SOFTSPI_PIN_MOSI; }

static inline uint8_t MISO_READ(void)
{
    return (SOFTSPI_GPIO->IDR & SOFTSPI_PIN_MISO) ? 1u : 0u;
}
/* =============================================================== */


/* ===================== CRC8 (poly 0x07, init 0x00) ===================== */
static uint8_t crc8_07_init00(const uint8_t *data, size_t len)
{
    uint8_t crc = 0x00;
    for (size_t i = 0; i < len; i++) {
        crc ^= data[i];
        for (uint8_t b = 0; b < 8; b++) {
            if (crc & 0x80) crc = (uint8_t)((crc << 1) ^ 0x07);
            else           crc = (uint8_t)(crc << 1);
        }
    }
    return crc;
}

/* ===================== Soft SPI Mode3 核心：传1字节 ===================== */
/* Mode3: idle SCK=1. For each bit:
 *   1) SCK low  (first edge: falling)
 *   2) set MOSI
 *   3) SCK high (second edge: rising) -> sample MISO
 */
static uint8_t softspi_xfer_u8(uint8_t tx)
{
    uint8_t rx = 0;

    for (int i = 7; i >= 0; i--) {
        /* 下降沿：准备推出数据 */
        SCK_LOW();
        softspi_delay();

        if (tx & (1u << i)) MOSI_HIGH();
        else                MOSI_LOW();

        softspi_delay();

        /* 上升沿：采样数据 */
        SCK_HIGH();
        softspi_delay();

        rx <<= 1;
        rx |= MISO_READ();

        softspi_delay();
    }

    return rx;
}

/* ===================== 初始化 GPIO（推挽输出 + 上拉输入） ===================== */
void softspi_mode3_gpio_init(void)
{
    GPIO_InitTypeDef gi;

    SOFTSPI_GPIO_RCC();

    /* SCK / MOSI / CS: 推挽输出 */
    gi.GPIO_Pin   = SOFTSPI_PIN_SCK | SOFTSPI_PIN_MOSI | SOFTSPI_PIN_CS;
    gi.GPIO_Speed = GPIO_Speed_50MHz;
    gi.GPIO_Mode  = GPIO_Mode_Out_PP;
    GPIO_Init(SOFTSPI_GPIO, &gi);

    /* MISO: 上拉输入（按你硬件情况可改浮空/下拉） */
    gi.GPIO_Pin  = SOFTSPI_PIN_MISO;
    gi.GPIO_Mode = GPIO_Mode_IPU;
    GPIO_Init(SOFTSPI_GPIO, &gi);

    /* Mode3 空闲态：CS=1, SCK=1 */
    CS_HIGH();
    SCK_HIGH();
    MOSI_LOW();
}

/* ===================== 协议层：Write / Read / Reset ===================== */
#define CMD_WRITE   (0x01u)
#define CMD_READ    (0x02u)
#define CMD_RESET   (0x0Bu)

#define IC_ACK_OK   (0xA5u)
#define IC_ACK_FAIL (0xFFu)

/* 片选包裹（可加更严格的时序延时） */
static inline void ic_select(void)
{
    CS_LOW();
    softspi_delay();
}
static inline void ic_deselect(void)
{
    softspi_delay();
    CS_HIGH();
    softspi_delay();
}

/* 写 1 字节寄存器（符合图 9：写长度固定1字节）
 * SDI: 0x01, RegAddr, Data, CRC8, Dummy(0x00)
 * SDO: ...最后一个字节读到 ACK (0xA5/0xFF)
 *
 * 注意：你的图里 ACK 出现在“无效数据(0x00)”这拍里，所以这里最后发 0x00 来读 ACK。
 */
int ic_write_reg_u8(uint8_t reg_addr, uint8_t data)
{
    uint8_t frame[3];
    frame[0] = CMD_WRITE;
    frame[1] = reg_addr;
    frame[2] = data;

    uint8_t crc = crc8_07_init00(frame, sizeof(frame));

    ic_select();

    (void)softspi_xfer_u8(frame[0]);
    (void)softspi_xfer_u8(frame[1]);
    (void)softspi_xfer_u8(frame[2]);
    (void)softspi_xfer_u8(crc);

    /* 发 dummy 来把 ACK clock 出来 */
    uint8_t ack = softspi_xfer_u8(0x00);

    ic_deselect();

    return (ack == IC_ACK_OK) ? 0 : -1;
}

/* 读 N 字节寄存器（符合图 10）
 * 主机发送：0x02, RegAddr, Len, dummy... (Len 次 dummy), 最后再 dummy 读 CRC
 * 从机输出：先吐 0xFF，然后 Data1..DataN，然后 CRC8
 *
 * CRC 计算（按你图下面文字）：从第一字节开始，包括：
 *   0xFF, 读命令, 寄存器地址, 读长度, N个数据
 */
int ic_read_reg(uint8_t reg_addr, uint8_t len, uint8_t *out)
{
    if (len == 0 || out == NULL) return -2;

    /* 先组织用于 CRC 的“逻辑序列”（含 0xFF 前导） */
    /* 注意：数据要先读出来才能算 CRC，所以先读，再复算校验 */
    ic_select();

    /* 发送读命令帧 */
    (void)softspi_xfer_u8(CMD_READ);

    /* 许多器件会在这拍（主机发CMD_READ）时从机输出 0xFF，
       但这个字节是“同时发生”的。最稳妥做法：把每次 xfer 的返回都收着。 */

    (void)softspi_xfer_u8(reg_addr);
    (void)softspi_xfer_u8(len);

    /* 读数据 len 字节 */
    for (uint8_t i = 0; i < len; i++) {
        out[i] = softspi_xfer_u8(0x00);
    }

    /* 最后读从机 CRC */
    uint8_t crc_slave = softspi_xfer_u8(0x00);

    ic_deselect();

    /* 现在复算 CRC：需要 0xFF + CMD_READ + reg + len + data[] */
    uint8_t crc_buf_head[4];
    crc_buf_head[0] = 0xFF;
    crc_buf_head[1] = CMD_READ;
    crc_buf_head[2] = reg_addr;
    crc_buf_head[3] = len;

    uint8_t crc = 0x00;
    /* 分段算 CRC（避免动态分配） */
    crc = crc8_07_init00(crc_buf_head, sizeof(crc_buf_head));
    /* 把 crc 继续“滚动”进 data：这里用一个小函数实现 rolling */
    for (uint8_t i = 0; i < len; i++) {
        crc ^= out[i];
        for (uint8_t b = 0; b < 8; b++) {
            if (crc & 0x80) crc = (uint8_t)((crc << 1) ^ 0x07);
            else           crc = (uint8_t)(crc << 1);
        }
    }

    return (crc == crc_slave) ? 0 : -1;
}

/* 软复位（图 11）
 * SDI: 0x0B, 0xBB, 0xCC, CRC8, dummy(0x00)
 * SDO: ... dummy 拍读到 ACK(0xA5/0xFF)
 */
int ic_soft_reset(void)
{
    uint8_t frame[3];
    frame[0] = CMD_RESET;
    frame[1] = 0xBB;
    frame[2] = 0xCC;

    uint8_t crc = crc8_07_init00(frame, sizeof(frame));

    ic_select();

    (void)softspi_xfer_u8(frame[0]);
    (void)softspi_xfer_u8(frame[1]);
    (void)softspi_xfer_u8(frame[2]);
    (void)softspi_xfer_u8(crc);

    uint8_t ack = softspi_xfer_u8(0x00);

    ic_deselect();

    return (ack == IC_ACK_OK) ? 0 : -1;
}
