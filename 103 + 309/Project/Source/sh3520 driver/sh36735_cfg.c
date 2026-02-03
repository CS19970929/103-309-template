#include "sh36735_cfg.h"
#include "sh36735_spi.h"

void sh36735_cfg_default_ternary_20s(sh36735_cfg_t *cfg)
{
    if (!cfg) return;

    // 这是一份“能跑起来”的默认值：保护阈值请你按实际 pack 改（尤其是 OV/UV/OCD/SC/OCC）
    cfg->cell_count    = 20;
    cfg->pump_enable   = true;

    cfg->pds_g_ic_ctrl = true;   // 预放电由 IC 自动时序更安全
    cfg->chgmos        = true;
    cfg->dsgmos        = true;

    cfg->wdt_enable    = true;
    cfg->wdt_sel       = 0;      // WDT[1:0]，具体时间看文档

    cfg->ov_en = true;
    cfg->uv_en = true;
    cfg->ocd_en= true;
    cfg->sc_en = true;

    cfg->ts1_en = cfg->ts2_en = cfg->ts3_en = cfg->ts4_en = false; // 默认先不开温度保护（便于先跑通通信）

    // 中断：你可以把 OV/UV/OCD/SC/OCC/TEMP/WDT/WK/VADC 等开起来
    cfg->alarmh = 0; // e.g. SH_ALARMH_VADC_INT;
    cfg->alarml = 0; // e.g. SH_ALARML_OV_INT | SH_ALARML_UV_INT;

    // 阈值寄存器编码：请按你 PDF 的“OV/UV/OCD/SC/OCC 电压 + 延时”编码方式填
    // 这里先放 0，表示保持默认（你也可以在 apply 里选择跳过写 0）
    cfg->ovh_ovt = 0;
    cfg->ovl     = 0;
    cfg->uvh_uvt = 0;
    cfg->uvl     = 0;
    cfg->ocd1v_ocd1t = 0;
    cfg->ocd2v_ocd2t = 0;
    cfg->scv_sct     = 0;
    cfg->occv_occt   = 0;
}

static bool wr(uint8_t reg, uint8_t val)
{
    return sh36735_write_reg_u8(reg, val);
}

bool sh36735_apply_cfg(const sh36735_cfg_t *cfg)
{
    if (!cfg) return false;

    // WarmUp 后再配置更稳妥（上层做延时）
    // 1) SCONF4：串数 CN[4:0] + 预放电时间等（这里只写 CN）
    uint8_t sconf4 = (uint8_t)(cfg->cell_count & 0x1F);
    if (!wr(SH_REG_SCONF4, sconf4)) return false;

    // 2) SCONF2：LTCLR/PD_EN/PD_CTL/PUMP_EN/PDSG_CTL/PDSGMOS/DSGMOS/CHGMOS
    uint8_t sconf2 = 0;
    sconf2 |= cfg->pump_enable   ? SH_SCONF2_PUMP_EN : 0;
    sconf2 |= cfg->pds_g_ic_ctrl ? SH_SCONF2_PDSGMOS : 0;
    sconf2 |= cfg->dsgmos        ? SH_SCONF2_DSGMOS  : 0;
    sconf2 |= cfg->chgmos        ? SH_SCONF2_CHGMOS  : 0;
    // LTCLR 建议在清标志位时再置 1，这里先不置

    if (!wr(SH_REG_SCONF2, sconf2)) return false;

    // 3) SCONF5：MOS_EN/OCC_EN/CADC_EN/WDT_EN/WDT[1:0] 等
    // 这里只配置 WDT
    uint8_t sconf5 = 0;
    if (cfg->wdt_enable) sconf5 |= (1u<<2); // WDT_EN 位在 bit2? 以你的 PDF 为准（寄存器表里 WDT_EN 在 bit2）
    sconf5 |= (cfg->wdt_sel & 0x03);
    if (!wr(SH_REG_SCONF5, sconf5)) return false;

    // 4) SCONF6：保护使能
    uint8_t sconf6 = 0;
    sconf6 |= cfg->ts4_en ? SH_SCONF6_TS4_EN : 0;
    sconf6 |= cfg->ts3_en ? SH_SCONF6_TS3_EN : 0;
    sconf6 |= cfg->ts2_en ? SH_SCONF6_TS2_EN : 0;
    sconf6 |= cfg->ts1_en ? SH_SCONF6_TS1_EN : 0;
    sconf6 |= cfg->sc_en  ? SH_SCONF6_SC_EN  : 0;
    sconf6 |= cfg->ocd_en ? SH_SCONF6_OCD_EN : 0;
    sconf6 |= cfg->uv_en  ? SH_SCONF6_UV_EN  : 0;
    sconf6 |= cfg->ov_en  ? SH_SCONF6_OV_EN  : 0;
    if (!wr(SH_REG_SCONF6, sconf6)) return false;

    // 5) 中断
    if (!wr(SH_REG_OWV_ALARMH, cfg->alarmh)) return false;
    if (!wr(SH_REG_ALARML,     cfg->alarml)) return false;

    // 6) 阈值（若为 0 则跳过写，避免你误把阈值写成 0）
    if (cfg->ovh_ovt) if (!wr(SH_REG_OVT_OVH, cfg->ovh_ovt)) return false;
    if (cfg->ovl)     if (!wr(SH_REG_OVL,     cfg->ovl))     return false;
    if (cfg->uvh_uvt) if (!wr(SH_REG_UVT_UVH, cfg->uvh_uvt)) return false;
    if (cfg->uvl)     if (!wr(SH_REG_UVL,     cfg->uvl))     return false;

    if (cfg->ocd1v_ocd1t) if (!wr(SH_REG_OCD1V_OCD1T, cfg->ocd1v_ocd1t)) return false;
    if (cfg->ocd2v_ocd2t) if (!wr(SH_REG_OCD2V_OCD2T, cfg->ocd2v_ocd2t)) return false;
    if (cfg->scv_sct)     if (!wr(SH_REG_SCV_SCT,     cfg->scv_sct))     return false;
    if (cfg->occv_occt)   if (!wr(SH_REG_OCCV_OCCT,   cfg->occv_occt))   return false;

    return true;
}
