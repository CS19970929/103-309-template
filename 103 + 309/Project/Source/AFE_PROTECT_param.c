#include "main.h"


typedef struct {
    uint16_t ov_mv;      // 过充保护电压 (mV)
    uint32_t ovt_ms;     // 过充保护延时 (ms)
    uint16_t ov_code;    // 原始OV码 (0..1023)
    uint8_t  ovt_code;   // 原始OVT码 (0..7)
} sh_ovh_ovt_t;

/* OVT[2:0] 对应延时表（单位：ms），按图中的映射 */
static const uint32_t k_ovt_delay_ms[8] = {
    140,   // 000
    280,   // 001
    490,   // 010
    988,   // 011 (默认)
    2030,  // 100
    3010,  // 101
    4970,  // 110
    10010  // 111
};

typedef struct {
    uint16_t uv_mv;      // 过充保护电压 (mV)
    uint32_t uvt_ms;     // 过充保护延时 (ms)
    uint16_t uv_code;    // 原始OV码 (0..1023)
    uint8_t  uvt_code;   // 原始OVT码 (0..7)
} sh_uvh_uvt_t;

/* OVT[2:0] 对应延时表（单位：ms），按图中的映射 */
static const uint32_t k_uvt_delay_ms[8] = {
    490,   // 000
    770,   // 001
    980,   // 010
    1470,   // 011 (默认)
    2030,  // 100
    3010,  // 101
    4970,  // 110
    10010  // 111
};

typedef struct {
    uint16_t ocd1_mv;      // 过充保护电压 (mV)
    uint32_t ocd1_ms;     // 过充保护延时 (ms)
    uint16_t ocd1_code;    // 原始OV码 (0..1023)
    uint8_t  ocd1t_code;   // 原始OVT码 (0..7)
} sh_ocd1_ocd1t_t;

/* OVT[2:0] 对应延时表（单位：ms），按图中的映射 */
static const uint32_t k_ocd1t_delay_ms[8] = {
    140,   // 000
    280,   // 001
    490,   // 010
    980,   // 011 (默认)
    2030,  // 100
    3010,  // 101
    4970,  // 110
    10010  // 111
};

typedef struct {
    uint16_t ocd2_mv;      // 过充保护电压 (mV)
    uint32_t ocd2_ms;     // 过充保护延时 (ms)
    uint16_t ocd2_code;    // 原始OV码 (0..1023)
    uint8_t  ocd2t_code;   // 原始OVT码 (0..7)
} sh_ocd2_ocd2t_t;

/* ------------------ 解码：从0x49/0x4A两个寄存器值 -> 电压/延时 ------------------ */
static inline sh_ovh_ovt_t sh_decode_ovh_ovt(uint8_t reg49, uint8_t reg4A, bool idle_mode)
{
    sh_ovh_ovt_t out;

    // OV[9:0] = reg4A[1:0] << 8 | reg49[7:0]
    out.ov_code = (uint16_t)(((uint16_t)(reg49 & 0x03) << 8) | reg4A);
    out.ov_mv   = (uint16_t)(out.ov_code * 5u);

    // OVT[2:0] = reg4A[6:4]
    out.ovt_code = (uint8_t)((reg49 >> 4) & 0x07);
    out.ovt_ms   = k_ovt_delay_ms[out.ovt_code];

    // IDLE模式：延时为设定值的4倍
    if (idle_mode) out.ovt_ms *= 4u;

    return out;
}

/* ------------------ 编码：从目标电压/延时 -> 回填到0x49/0x4A ------------------ */
/* 说明：
 * - ov_mv 以 5mV 步进量化；会自动四舍五入到最近的5mV
 * - ovt_code 直接给 0..7 最稳（如果你想按ms找最近值，也给了函数）
 * - reg4A_reserved_keep：传入当前reg4A，用于保留Reserved位（3:2、7等）
 */
static inline uint16_t sh_quantize_ov_mv_to_code(uint16_t ov_mv)
{
    // 四舍五入到5mV： (mv + 2) / 5
    uint32_t code = ((uint32_t)ov_mv + 2u) / 5u;
    if (code > 0x3FFu) code = 0x3FFu; // 10-bit clamp
    return (uint16_t)code;
}

static inline void sh_encode_ovh_ovt(uint16_t ov_mv, uint8_t ovt_code,
                                     uint8_t reg4A_reserved_keep,
                                     uint8_t *out_reg49, uint8_t *out_reg4A)
{
    uint16_t ov_code = sh_quantize_ov_mv_to_code(ov_mv);
    ovt_code &= 0x07;

    // reg49 = OV[7:0]
    *out_reg49 = (uint8_t)(ov_code & 0xFFu);

    // reg4A: 保留 reserved，再写入 OVT
    uint8_t r = reg4A_reserved_keep;

    // 清掉要写的位：6:4 和 1:0
    r &= (uint8_t)~((uint8_t)(0x07u << 4) | 0x03u);

    // 写回
    r |= (uint8_t)(ovt_code << 4);
    r |= (uint8_t)((ov_code >> 8) & 0x03u);

    *out_reg4A = r;
}

/* 可选：给你一个“按毫秒找最近OVT码”的工具（不想手填0..7时用） */
static inline uint8_t sh_pick_ovt_code_by_ms(uint32_t target_ms)
{
    uint8_t best = 0;
    uint32_t best_err = (target_ms > k_ovt_delay_ms[0]) ? (target_ms - k_ovt_delay_ms[0])
                                                       : (k_ovt_delay_ms[0] - target_ms);
    for (uint8_t i = 1; i < 8; i++) {
        uint32_t d = k_ovt_delay_ms[i];
        uint32_t err = (target_ms > d) ? (target_ms - d) : (d - target_ms);
        if (err < best_err) { best_err = err; best = i; }
    }
    return best;
}



sh_ovh_ovt_t ov_param;
sh_uvh_uvt_t uv_param;
sh_ocd1_ocd1t_t ocd1_param;
sh_ocd2_ocd2t_t ocd2_param;

static inline sh_uvh_uvt_t sh_decode_uvh_uvt(uint8_t reg49, uint8_t reg4A, bool idle_mode)
{
    sh_uvh_uvt_t out;

    // OV[9:0] = reg4A[1:0] << 8 | reg49[7:0]
    out.uv_code = (uint16_t)(((uint16_t)(reg49 & 0x03) << 8) | reg4A);
    out.uv_mv   = (uint16_t)(out.uv_code * 5u);

    // OVT[2:0] = reg4A[6:4]
    out.uvt_code = (uint8_t)((reg49 >> 4) & 0x07);
    out.uvt_ms   = k_uvt_delay_ms[out.uvt_code];

    // IDLE模式：延时为设定值的4倍
    if (idle_mode) out.uvt_ms *= 4u;

    return out;
}

static inline sh_ocd1_ocd1t_t sh_decode_ocd1_ocd1t(uint8_t reg, bool idle_mode)
{
    sh_ocd1_ocd1t_t out;

    // OV[9:0] = reg4A[1:0] << 8 | reg49[7:0]
    // out.ocd1_code = (uint16_t)(((uint8_t)(reg49 & 0x03) << 8) | reg4A);
    out.ocd1_code = (reg & 0xf);
    out.ocd1_mv   = (uint16_t)(out.ocd1_code * 5u) + 5;

    // OVT[2:0] = reg4A[6:4]
    out.ocd1t_code = (uint8_t)((reg >> 4) & 0x07);
    out.ocd1_ms   = k_ocd1t_delay_ms[out.ocd1t_code];

    // IDLE模式：延时为设定值的4倍
    if (idle_mode) out.ocd1_ms *= 4u;

    return out;
}

static inline sh_ocd2_ocd2t_t sh_decode_ocd2_ocd2t(uint8_t reg, bool idle_mode)
{
    sh_ocd2_ocd2t_t out;

    out.ocd2_code = (reg & 0xf);
    out.ocd2_mv   = (uint16_t)(out.ocd2_code * 10u) + 10;

    out.ocd2t_code = (uint8_t)((reg >> 4) & 0xf);
    out.ocd2_ms   = out.ocd2t_code * 25 + 25;

    // IDLE模式：延时为设定值的4倍
    if (idle_mode) out.ocd2_ms *= 4u;

    return out;
}


void test_read_afe_param(void)
{
    ov_param = sh_decode_ovh_ovt(Registers_AFE1.OVT_OVH, Registers_AFE1.OVL, false);
    uv_param = sh_decode_uvh_uvt(Registers_AFE1.UVT_UVH, Registers_AFE1.UVL, false);
    ocd1_param = sh_decode_ocd1_ocd1t(Registers_AFE1.OCD1V_OCD1T, false);
    ocd2_param = sh_decode_ocd2_ocd2t(Registers_AFE1.OCD2V_OCD2T, false);
    // v.ov_mv  = 4200mV
    // v.ovt_ms = 988ms（因为 reg4A[6:4]=0b011）

    // uint8_t new49, new4A;
    // uint8_t cur4A = reg4A; // 读出来的旧值，用来保留Reserved

    // uint8_t ovt_code = 3; // 011 -> 988ms
    // sh_encode_ovh_ovt(4200, ovt_code, cur4A, &new49, &new4A);
// 然后把 new49 写0x49，new4A 写0x4A
}

