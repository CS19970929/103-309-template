#include "upg_params.h"

#include "upg_utils.h"

static const UpgParamDef s_params[] = {
    {0x1001U, 0x20U, 0x00U, 0U, UPG_PARAM_U16, 3000, 5000, 1U, 1U},
    {0x1002U, 0x20U, 0x00U, 2U, UPG_PARAM_U16, 1500, 3500, 1U, 1U},
    {0x1003U, 0x20U, 0x01U, 0U, UPG_PARAM_U16, 1, 500, 1U, 1U},
    {0x1004U, 0x20U, 0x01U, 2U, UPG_PARAM_U16, 1, 500, 1U, 1U},
    {0x1101U, 0x02U, 0x02U, 1U, UPG_PARAM_U8, 0, 100, 0U, 0U}
};

uint16_t UpgParam_Count(void)
{
    return (uint16_t)(sizeof(s_params) / sizeof(s_params[0]));
}

const UpgParamDef *UpgParam_Find(uint16_t param_id)
{
    uint16_t index;

    for (index = 0U; index < UpgParam_Count(); index++)
    {
        if (s_params[index].param_id == param_id)
        {
            return &s_params[index];
        }
    }
    return 0;
}

uint8_t UpgParam_ReadRaw(const UpgParamDef *param, const uint8_t can_data[8], int32_t *raw_value)
{
    uint8_t offset;

    if ((param == 0) || (can_data == 0) || (raw_value == 0))
    {
        return 0U;
    }

    offset = param->byte_offset;
    switch (param->type)
    {
    case UPG_PARAM_U8:
        if (offset > 7U)
        {
            return 0U;
        }
        *raw_value = (int32_t)can_data[offset];
        return 1U;
    case UPG_PARAM_U16:
        if (offset > 6U)
        {
            return 0U;
        }
        *raw_value = (int32_t)UpgReadBe16(&can_data[offset]);
        return 1U;
    case UPG_PARAM_U32:
        if (offset > 4U)
        {
            return 0U;
        }
        *raw_value = (int32_t)UpgReadBe32(&can_data[offset]);
        return 1U;
    case UPG_PARAM_I16:
        if (offset > 6U)
        {
            return 0U;
        }
        *raw_value = (int16_t)UpgReadBe16(&can_data[offset]);
        return 1U;
    case UPG_PARAM_I32:
        if (offset > 4U)
        {
            return 0U;
        }
        *raw_value = UpgReadBeS32(&can_data[offset]);
        return 1U;
    default:
        return 0U;
    }
}

uint8_t UpgParam_WriteRaw(const UpgParamDef *param, int32_t raw_value, uint8_t can_data[8])
{
    uint8_t offset;

    if ((param == 0) || (can_data == 0))
    {
        return 0U;
    }
    if ((raw_value < param->min_value) || (raw_value > param->max_value))
    {
        return 0U;
    }
    if (param->writable == 0U)
    {
        return 0U;
    }

    offset = param->byte_offset;
    switch (param->type)
    {
    case UPG_PARAM_U8:
        if (offset > 7U)
        {
            return 0U;
        }
        can_data[offset] = (uint8_t)raw_value;
        return 1U;
    case UPG_PARAM_U16:
    case UPG_PARAM_I16:
        if (offset > 6U)
        {
            return 0U;
        }
        UpgWriteBe16(&can_data[offset], (uint16_t)raw_value);
        return 1U;
    case UPG_PARAM_U32:
    case UPG_PARAM_I32:
        if (offset > 4U)
        {
            return 0U;
        }
        UpgWriteBe32(&can_data[offset], (uint32_t)raw_value);
        return 1U;
    default:
        return 0U;
    }
}
