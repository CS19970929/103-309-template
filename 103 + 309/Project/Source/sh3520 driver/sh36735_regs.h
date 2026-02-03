#ifndef __SH36735_REGS_H__
#define __SH36735_REGS_H__

/*
 * 只把“最常用/必须用”的寄存器地址和位定义拉出来，方便你读写。
 * 完整寄存器表请参考说明书 10 章。
 */

// ========== 可写 RAM 寄存器：0x40 ~ 0x59 ==========
#define SH_REG_SCONF1          0x40
#define SH_REG_SCONF2          0x41
#define SH_REG_SCONF3          0x42
#define SH_REG_SCONF4          0x43
#define SH_REG_SCONF5          0x44
#define SH_REG_SCONF6          0x45
#define SH_REG_SCONF7          0x46
#define SH_REG_OWV_ALARMH      0x47
#define SH_REG_ALARML          0x48
#define SH_REG_OVT_OVH         0x49
#define SH_REG_OVL             0x4A
#define SH_REG_UVT_UVH         0x4B
#define SH_REG_UVL             0x4C
#define SH_REG_OCD1V_OCD1T     0x4D
#define SH_REG_OCD2V_OCD2T     0x4E
#define SH_REG_SCV_SCT         0x4F
#define SH_REG_OCCV_OCCT       0x50
// ... 0x51~0x59 (温度阈值、均衡、负载检测等) 你按需补

// ========== 常读寄存器（只读 RAM）：0x5A ~ 0x99 ==========
#define SH_REG_BSTATUS1        0x5A
#define SH_REG_BSTATUS2        0x5B
#define SH_REG_FLAG1           0x5C
#define SH_REG_FLAG2           0x5D  // 注：不同版本文档 FLAG2/温度寄存器起始可能不同，请以你的 PDF 为准
// 说明书中温度寄存器 0x5D~0x64，内部温度 0x65~0x66，电流 0x67~0x68 等
// 为避免版本差异，这里不硬编码后续地址，建议你在工程中按你的 PDF 校对后补全。

// ========== SCONF2 bits (0x41) ==========
#define SH_SCONF2_LTCLR        (1u<<7)
#define SH_SCONF2_PD_EN        (1u<<6)
#define SH_SCONF2_PD_CTL       (1u<<5)
#define SH_SCONF2_PUMP_EN      (1u<<4)
#define SH_SCONF2_PDSG_CTL     (1u<<3)
#define SH_SCONF2_PDSGMOS      (1u<<2)
#define SH_SCONF2_DSGMOS       (1u<<1)
#define SH_SCONF2_CHGMOS       (1u<<0)

// ========== SCONF6 bits (0x45) ==========
#define SH_SCONF6_TS4_EN        (1u<<7)
#define SH_SCONF6_TS3_EN        (1u<<6)
#define SH_SCONF6_TS2_EN        (1u<<5)
#define SH_SCONF6_TS1_EN        (1u<<4)
#define SH_SCONF6_SC_EN         (1u<<3)
#define SH_SCONF6_OCD_EN        (1u<<2)
#define SH_SCONF6_UV_EN         (1u<<1)
#define SH_SCONF6_OV_EN         (1u<<0)

// ========== ALARMH / ALARML bits ==========
#define SH_ALARMH_LOADON_INT    (1u<<3)
#define SH_ALARMH_LOADOFF_INT   (1u<<2)
#define SH_ALARMH_VADC_INT      (1u<<1)
#define SH_ALARMH_CADC_INT      (1u<<0)

#define SH_ALARML_WK_INT        (1u<<7)
#define SH_ALARML_WDT_INT       (1u<<6)
#define SH_ALARML_OWD_INT       (1u<<5)
#define SH_ALARML_TEMP_INT      (1u<<4)
#define SH_ALARML_OCC_INT       (1u<<3)
#define SH_ALARML_OCD_INT       (1u<<2)
#define SH_ALARML_UV_INT        (1u<<1)
#define SH_ALARML_OV_INT        (1u<<0)

#endif // __SH36735_REGS_H__
