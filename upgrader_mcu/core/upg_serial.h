#ifndef UPG_SERIAL_H
#define UPG_SERIAL_H

#include "upg_protocol.h"

#include <stdint.h>

typedef struct
{
    uint8_t cmd;
    uint16_t seq;
    uint8_t flags;
    uint16_t len;
    const uint8_t *payload;
} UpgSerialFrameView;

typedef void (*UpgSerialFrameFn)(const UpgSerialFrameView *frame, void *user);
typedef void (*UpgSerialErrorFn)(uint8_t status, void *user);

typedef struct
{
    uint8_t state;
    uint16_t pos;
    uint16_t expected_total;
    uint8_t buffer[UPG_SERIAL_MAX_FRAME];
} UpgSerialParser;

void UpgSerial_InitParser(UpgSerialParser *parser);
uint8_t UpgSerial_Encode(uint8_t cmd, uint16_t seq, uint8_t flags, const uint8_t *payload, uint16_t len, uint8_t *out, uint16_t out_cap, uint16_t *out_len);
void UpgSerial_ParseBytes(UpgSerialParser *parser, const uint8_t *data, uint16_t len, UpgSerialFrameFn on_frame, UpgSerialErrorFn on_error, void *user);

#endif
