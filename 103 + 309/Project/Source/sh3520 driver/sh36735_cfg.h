#ifndef __SH36735_CFG_H__
#define __SH36735_CFG_H__

#include <stdint.h>
#include <stdbool.h>
#include "sh36735_regs.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * 这里提供一个“够用且稳定”的配置入口：把常用保护、电荷泵、MOS 控制、串数等配置好。
 * 你可以把参数改成你自己的三元锂/LFP。
 */

typedef struct
{
    uint8_t cell_count;      // 4~20 -> 写入 SCONF4.CN[4:0]
    bool    pump_enable;     // SCONF2.PUMP_EN
    bool    chgmos;          // SCONF2.CHGMOS
    bool    dsgmos;          // SCONF2.DSGMOS
    bool    pds_g_ic_ctrl;   // SCONF2.PDSGMOS (1=IC控制预放电)
    bool    wdt_enable;      // SCONF5.WDT_EN
    uint8_t wdt_sel;         // SCONF5.WDT[1:0]

    // 保护使能（SCONF6）
    bool ov_en;
    bool uv_en;
    bool ocd_en;
    bool sc_en;
    bool ts1_en, ts2_en, ts3_en, ts4_en;

    // 中断使能
    uint8_t alarmh;
    uint8_t alarml;

    // 下面这些阈值寄存器，你按你电芯参数填（文档给了编码方式）
    uint8_t ovh_ovt; // 0x49
    uint8_t ovl;     // 0x4A
    uint8_t uvh_uvt; // 0x4B
    uint8_t uvl;     // 0x4C

    uint8_t ocd1v_ocd1t; // 0x4D
    uint8_t ocd2v_ocd2t; // 0x4E
    uint8_t scv_sct;     // 0x4F
    uint8_t occv_occt;   // 0x50

} sh36735_cfg_t;

void sh36735_cfg_default_ternary_20s(sh36735_cfg_t *cfg);

// 将 cfg 写入芯片（只写 0x40~0x50 这段最关键寄存器）
bool sh36735_apply_cfg(const sh36735_cfg_t *cfg);

#ifdef __cplusplus
}
#endif

#endif // __SH36735_CFG_H__
