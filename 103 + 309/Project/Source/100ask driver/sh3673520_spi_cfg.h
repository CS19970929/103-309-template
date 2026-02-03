#ifndef __SH3673520_SPI_CFG_H
#define __SH3673520_SPI_CFG_H

#include <stdint.h>
#include "driver_spi_soft.h"

/* ====== 1) 3520 对应的 SPI Mode（必须按手册来！）======
 * 常见情况：
 *  - 很多 AFE 用 MODE0 或 MODE3
 */
#define SH3673520_SPI_MODE          SPI_MODE0

/* ====== 2) 速度：先慢后快（调通优先） ====== */
#define SH3673520_SPI_HALF_US       (5)     /* half cycle 5us => SCK ~100kHz 级别 */

/* ====== 3) CS 时序（按手册要求） ====== */
#define SH3673520_CS_SETUP_US       (5)
#define SH3673520_CS_HOLD_US        (5)
#define SH3673520_CS_HIGH_US        (5)

/* ====== 4) 协议帧定义（按手册修改） ======
 * 下面是“模板”，你按手册改成真实协议：
 *
 * 常见协议之一示例（不代表3520一定如此）：
 *  - 1Byte: [R/W bit | 7bit addr]
 *  - 写：cmd + data...
 *  - 读：cmd + dummy + data...
 *
 * 如果3520是 16bit addr，或者读需要 2 个 dummy byte，或者带 CRC，
 * 都在这里改。
 */

/* 地址宽度：1 或 2 */
#define SH3673520_ADDR_BYTES        (1)

/* 命令格式：返回要发送的 cmd byte（如果是两字节地址，你需要自己拆） */
#define SH3673520_CMD_RW_BIT_POS    (7)     /* 示例：bit7 作为 R/W */
#define SH3673520_CMD_READ          (1)
#define SH3673520_CMD_WRITE         (0)

/* 读操作 dummy 字节数（很多芯片读会需要 1 个 dummy） */
#define SH3673520_READ_DUMMY_BYTES  (0)

/* 是否带 CRC：0/1（若带 CRC，你在 sh3673520_spi.c 里启用对应逻辑） */
#define SH3673520_HAS_CRC           (0)

#endif /* __SH3673520_SPI_CFG_H */
