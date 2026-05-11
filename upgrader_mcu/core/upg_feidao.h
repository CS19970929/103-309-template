#ifndef UPG_FEIDAO_H
#define UPG_FEIDAO_H

#include "upg_protocol.h"

#include <stdint.h>

typedef struct
{
    uint8_t src;
    uint8_t dst;
    uint8_t ctrl;
    uint8_t index;
    uint8_t chd;
} UpgFeidaoId;

typedef struct
{
    uint32_t id;
    uint8_t extended;
    uint8_t dlc;
    uint8_t data[8];
} UpgCanFrame;

uint32_t UpgFeidao_BuildId(uint8_t src, uint8_t dst, uint8_t ctrl, uint8_t index, uint8_t chd);
UpgFeidaoId UpgFeidao_DecodeId(uint32_t id);
void UpgFeidao_MakeFrame(UpgCanFrame *frame, uint8_t src, uint8_t dst, uint8_t ctrl, uint8_t index, uint8_t chd, const uint8_t data[8]);
uint8_t UpgFeidao_IsAckFor(const UpgCanFrame *frame, uint8_t src, uint8_t dst, uint8_t index, uint8_t chd);
uint8_t UpgFeidao_IsErrAckFor(const UpgCanFrame *frame, uint8_t src, uint8_t dst, uint8_t index, uint8_t chd);

#endif
