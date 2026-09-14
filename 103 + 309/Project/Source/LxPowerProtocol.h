#ifndef LX_POWER_PROTOCOL_H
#define LX_POWER_PROTOCOL_H

#include <stdint.h>

/*
 * LX 电池与电源板通讯协议 V0.9
 *
 * 该模块只负责协议帧解析/组包，不直接访问 UART、AFE、Flash 或 MOS。
 * 项目相关数据通过 LX_POWER_PROTOCOL_ADAPTER 注入，便于移植和单独审阅。
 */

#define LX_POWER_PROTOCOL_CELL_COUNT       7U
#define LX_POWER_PROTOCOL_RX_BUFFER_SIZE   64U
#define LX_POWER_PROTOCOL_TX_BUFFER_SIZE   64U

#define LX_POWER_PROTOCOL_DATA1_LENGTH     37U
#define LX_POWER_PROTOCOL_DATA2_LENGTH     15U
#define LX_POWER_PROTOCOL_DATA3_LENGTH     16U
#define LX_POWER_PROTOCOL_DATA4_LENGTH     22U

/* V0.9 正文字段表为 AA ED；文档末尾 AA EE 样例属于文档冲突。 */
#ifndef LX_POWER_PROTOCOL_VENDOR_ID_H
#define LX_POWER_PROTOCOL_VENDOR_ID_H      0xAAU
#endif
#ifndef LX_POWER_PROTOCOL_VENDOR_ID_L
#define LX_POWER_PROTOCOL_VENDOR_ID_L      0xEDU
#endif

typedef struct
{
    uint16_t cell_mv[LX_POWER_PROTOCOL_CELL_COUNT];
    uint8_t temperature_raw;          /* 当前温度：实际温度 + 40 */
    uint8_t balance_bits;             /* bit0..6 对应 Cell1..7 */
    uint16_t pack_voltage_raw;        /* 0.1 V/LSB */
    uint16_t current_raw;             /* 0.01 A/LSB, offset = 100 A */
    uint8_t soc_percent;
    uint8_t fault_bits;
    uint16_t cell_ov_mv;
    uint16_t cell_uv_mv;
    uint8_t high_temperature_raw;     /* 1 C/LSB, offset = 0 */
    uint16_t charge_oc_raw;           /* 0.1 A/LSB, offset = 1000 A */
    uint16_t discharge_oc_raw;        /* 0.1 A/LSB, offset = 1000 A */
    uint16_t rated_capacity_raw;      /* 0.1 Ah/LSB */
    uint16_t cell_ov_recovery_mv;
    uint16_t cell_uv_recovery_mv;
} LX_POWER_PROTOCOL_DATA1;

typedef struct
{
    uint16_t cycle_count;
    uint8_t software_version[2];
    uint8_t battery_id[11];
} LX_POWER_PROTOCOL_DATA2;

typedef struct
{
    uint16_t max_capacity_raw;        /* 0.1 Ah/LSB */
    uint16_t charge_remaining_min;
    uint16_t discharge_remaining_min;
} LX_POWER_PROTOCOL_DATA3;

typedef struct
{
    void (*get_data1)(LX_POWER_PROTOCOL_DATA1 *data);
    void (*get_data2)(LX_POWER_PROTOCOL_DATA2 *data);
    void (*get_data3)(LX_POWER_PROTOCOL_DATA3 *data);
    void (*get_data4_counts)(uint16_t counts[11]);
    uint8_t (*clear_data4_counts)(void);
    uint8_t (*set_software_mos_mode)(uint8_t mode);
    void (*service)(void);
} LX_POWER_PROTOCOL_ADAPTER;

typedef struct
{
    const LX_POWER_PROTOCOL_ADAPTER *adapter;
    uint8_t rx_buffer[LX_POWER_PROTOCOL_RX_BUFFER_SIZE];
    uint8_t tx_buffer[LX_POWER_PROTOCOL_TX_BUFFER_SIZE];
    uint16_t rx_length;
    uint16_t expected_length;
    uint16_t tx_length;
} LX_POWER_PROTOCOL_CONTEXT;

void LxPowerProtocol_Init(LX_POWER_PROTOCOL_CONTEXT *ctx,
                          const LX_POWER_PROTOCOL_ADAPTER *adapter);
void LxPowerProtocol_Reset(LX_POWER_PROTOCOL_CONTEXT *ctx);
uint8_t LxPowerProtocol_IsStartByte(uint8_t data);
uint8_t LxPowerProtocol_Feed(LX_POWER_PROTOCOL_CONTEXT *ctx, uint8_t data);
void LxPowerProtocol_OnRxIdle(LX_POWER_PROTOCOL_CONTEXT *ctx);
void LxPowerProtocol_Process(LX_POWER_PROTOCOL_CONTEXT *ctx);
void LxPowerProtocol_Service(LX_POWER_PROTOCOL_CONTEXT *ctx);
uint8_t *LxPowerProtocol_GetTxBuffer(LX_POWER_PROTOCOL_CONTEXT *ctx);
uint16_t LxPowerProtocol_GetTxLength(const LX_POWER_PROTOCOL_CONTEXT *ctx);
uint8_t LxPowerProtocol_IsBusy(const LX_POWER_PROTOCOL_CONTEXT *ctx);

#endif /* LX_POWER_PROTOCOL_H */
