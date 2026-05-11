#ifndef UPG_CORE_H
#define UPG_CORE_H

#include "upg_feidao.h"
#include "upg_params.h"
#include "upg_serial.h"

#include <stdint.h>

typedef int (*UpgCanTxFn)(void *user, const UpgCanFrame *frame);
typedef int (*UpgSerialTxFn)(void *user, const uint8_t *data, uint16_t len);
typedef void (*UpgResetFn)(void *user);

typedef struct
{
    UpgCanTxFn can_tx;
    UpgSerialTxFn serial_tx;
    UpgResetFn reset;
    void *user;
} UpgHal;

typedef enum
{
    UPG_PENDING_NONE = 0,
    UPG_PENDING_OBJECT_READ,
    UPG_PENDING_OBJECT_WRITE,
    UPG_PENDING_PARAM_READ_OBJECT,
    UPG_PENDING_PARAM_WRITE_READ_OBJECT,
    UPG_PENDING_PARAM_WRITE_OBJECT,
    UPG_PENDING_ENTER_IAP,
    UPG_PENDING_UPGRADE_PREPARE,
    UPG_PENDING_UPGRADE_COMMIT,
    UPG_PENDING_UPGRADE_FINISH
} UpgPendingType;

typedef enum
{
    UPG_UPGRADE_IDLE = 0,
    UPG_UPGRADE_PREPARED,
    UPG_UPGRADE_TRANSFERRING,
    UPG_UPGRADE_WAIT_FINAL,
    UPG_UPGRADE_DONE,
    UPG_UPGRADE_ERROR
} UpgUpgradeState;

typedef struct
{
    uint32_t valid_mask;
    uint32_t last_update_ms;
    uint32_t voltage_mv;
    int32_t current_ma;
    uint8_t soc;
    uint8_t soh;
    int8_t temp_c;
    uint16_t cycles;
    uint8_t protocol_version;
    uint8_t software_version;
    uint8_t work_status;
    uint8_t exception_status;
    uint16_t cap_full;
    uint16_t cap_now;
    uint16_t cap_design;
} UpgBmsSnapshot;

typedef struct
{
    UpgPendingType type;
    uint8_t cmd;
    uint16_t seq;
    uint32_t deadline_ms;
    uint8_t index;
    uint8_t chd;
    const UpgParamDef *param;
    uint16_t param_id;
    int32_t write_value;
    uint8_t object_data[8];
} UpgPending;

typedef struct
{
    UpgUpgradeState state;
    uint32_t image_size;
    uint16_t file_crc16;
    uint16_t total_long_packets;
    uint16_t acked_long_packets;
    uint16_t current_long_index;
    uint16_t current_len;
    uint16_t current_received;
    uint8_t chunk[UPG_LONG_PACKET_BYTES];
    uint8_t last_error;
} UpgUpgradeSession;

typedef struct
{
    UpgHal hal;
    UpgSerialParser serial_parser;
    uint32_t now_ms;
    uint32_t can_bitrate;
    uint8_t host_node;
    uint8_t device_node;
    UpgPending pending;
    UpgUpgradeSession upgrade;
    UpgBmsSnapshot snapshot;
    uint8_t tx_buf[UPG_SERIAL_MAX_FRAME];
} UpgCore;

void UpgCore_Init(UpgCore *ctx, const UpgHal *hal);
void UpgCore_SetNow(UpgCore *ctx, uint32_t now_ms);
void UpgCore_Tick(UpgCore *ctx, uint32_t now_ms);
void UpgCore_OnSerialBytes(UpgCore *ctx, const uint8_t *data, uint16_t len);
void UpgCore_OnCanFrame(UpgCore *ctx, const UpgCanFrame *frame);

#endif
