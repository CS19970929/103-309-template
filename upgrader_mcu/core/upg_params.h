#ifndef UPG_PARAMS_H
#define UPG_PARAMS_H

#include <stdint.h>

typedef enum
{
    UPG_PARAM_U8 = 0,
    UPG_PARAM_U16,
    UPG_PARAM_U32,
    UPG_PARAM_I16,
    UPG_PARAM_I32
} UpgParamType;

typedef struct
{
    uint16_t param_id;
    uint8_t can_index;
    uint8_t can_chd;
    uint8_t byte_offset;
    UpgParamType type;
    int32_t min_value;
    int32_t max_value;
    uint8_t writable;
    uint8_t require_confirm;
} UpgParamDef;

const UpgParamDef *UpgParam_Find(uint16_t param_id);
uint8_t UpgParam_ReadRaw(const UpgParamDef *param, const uint8_t can_data[8], int32_t *raw_value);
uint8_t UpgParam_WriteRaw(const UpgParamDef *param, int32_t raw_value, uint8_t can_data[8]);
uint16_t UpgParam_Count(void);

#endif
