#include "upg_serial.h"

#include "upg_crc16.h"
#include "upg_utils.h"

#include <string.h>

#define UPG_SERIAL_HEADER_NO_SOF_LEN ((uint16_t)9U)
#define UPG_SERIAL_MIN_TOTAL_LEN     ((uint16_t)13U)

void UpgSerial_InitParser(UpgSerialParser *parser)
{
    if (parser == 0)
    {
        return;
    }
    parser->state = 0U;
    parser->pos = 0U;
    parser->expected_total = 0U;
}

uint8_t UpgSerial_Encode(uint8_t cmd, uint16_t seq, uint8_t flags, const uint8_t *payload, uint16_t len, uint8_t *out, uint16_t out_cap, uint16_t *out_len)
{
    uint16_t total;
    uint16_t header_crc;
    uint16_t payload_crc;

    if ((out == 0) || (out_len == 0) || (len > UPG_SERIAL_MAX_PAYLOAD))
    {
        return 0U;
    }
    total = (uint16_t)(UPG_SERIAL_MIN_TOTAL_LEN + len);
    if (out_cap < total)
    {
        return 0U;
    }
    if ((len > 0U) && (payload == 0))
    {
        return 0U;
    }

    out[0] = UPG_SERIAL_SOF0;
    out[1] = UPG_SERIAL_SOF1;
    out[2] = UPG_VERSION;
    out[3] = cmd;
    UpgWriteBe16(&out[4], seq);
    out[6] = flags;
    UpgWriteBe16(&out[7], len);
    header_crc = UpgCrc16_Calc(&out[2], 7U);
    UpgWriteBe16(&out[9], header_crc);
    if (len > 0U)
    {
        memcpy(&out[11], payload, len);
        payload_crc = UpgCrc16_Calc(payload, len);
    }
    else
    {
        payload_crc = 0xFFFFU;
    }
    UpgWriteBe16(&out[11U + len], payload_crc);
    *out_len = total;
    return 1U;
}

static void UpgSerial_Reset(UpgSerialParser *parser)
{
    parser->state = 0U;
    parser->pos = 0U;
    parser->expected_total = 0U;
}

static void UpgSerial_HandleComplete(UpgSerialParser *parser, UpgSerialFrameFn on_frame, UpgSerialErrorFn on_error, void *user)
{
    uint16_t payload_len;
    uint16_t header_crc_expected;
    uint16_t header_crc_actual;
    uint16_t payload_crc_expected;
    uint16_t payload_crc_actual;
    UpgSerialFrameView frame;

    if (parser->expected_total < UPG_SERIAL_MIN_TOTAL_LEN)
    {
        if (on_error != 0)
        {
            on_error(UPG_STATUS_BAD_PARAM, user);
        }
        UpgSerial_Reset(parser);
        return;
    }

    payload_len = UpgReadBe16(&parser->buffer[7]);
    header_crc_expected = UpgReadBe16(&parser->buffer[9]);
    header_crc_actual = UpgCrc16_Calc(&parser->buffer[2], 7U);
    if (header_crc_expected != header_crc_actual)
    {
        if (on_error != 0)
        {
            on_error(UPG_STATUS_SERIAL_CRC_ERROR, user);
        }
        UpgSerial_Reset(parser);
        return;
    }

    payload_crc_expected = UpgReadBe16(&parser->buffer[11U + payload_len]);
    payload_crc_actual = (payload_len == 0U) ? 0xFFFFU : UpgCrc16_Calc(&parser->buffer[11], payload_len);
    if (payload_crc_expected != payload_crc_actual)
    {
        if (on_error != 0)
        {
            on_error(UPG_STATUS_SERIAL_CRC_ERROR, user);
        }
        UpgSerial_Reset(parser);
        return;
    }

    if (on_frame != 0)
    {
        frame.cmd = parser->buffer[3];
        frame.seq = UpgReadBe16(&parser->buffer[4]);
        frame.flags = parser->buffer[6];
        frame.len = payload_len;
        frame.payload = (payload_len == 0U) ? 0 : &parser->buffer[11];
        on_frame(&frame, user);
    }
    UpgSerial_Reset(parser);
}

void UpgSerial_ParseBytes(UpgSerialParser *parser, const uint8_t *data, uint16_t len, UpgSerialFrameFn on_frame, UpgSerialErrorFn on_error, void *user)
{
    uint16_t index;
    uint8_t byte_value;
    uint16_t payload_len;

    if ((parser == 0) || (data == 0))
    {
        return;
    }

    for (index = 0U; index < len; index++)
    {
        byte_value = data[index];
        if (parser->state == 0U)
        {
            if (byte_value == UPG_SERIAL_SOF0)
            {
                parser->buffer[0] = byte_value;
                parser->pos = 1U;
                parser->state = 1U;
            }
            continue;
        }
        if (parser->state == 1U)
        {
            if (byte_value == UPG_SERIAL_SOF1)
            {
                parser->buffer[1] = byte_value;
                parser->pos = 2U;
                parser->state = 2U;
            }
            else if (byte_value == UPG_SERIAL_SOF0)
            {
                parser->buffer[0] = byte_value;
                parser->pos = 1U;
            }
            else
            {
                UpgSerial_Reset(parser);
            }
            continue;
        }

        if (parser->pos >= UPG_SERIAL_MAX_FRAME)
        {
            if (on_error != 0)
            {
                on_error(UPG_STATUS_NO_BUFFER, user);
            }
            UpgSerial_Reset(parser);
            continue;
        }
        parser->buffer[parser->pos++] = byte_value;

        if (parser->pos == 9U)
        {
            payload_len = UpgReadBe16(&parser->buffer[7]);
            if (payload_len > UPG_SERIAL_MAX_PAYLOAD)
            {
                if (on_error != 0)
                {
                    on_error(UPG_STATUS_NO_BUFFER, user);
                }
                UpgSerial_Reset(parser);
                continue;
            }
            parser->expected_total = (uint16_t)(UPG_SERIAL_MIN_TOTAL_LEN + payload_len);
        }
        if ((parser->expected_total != 0U) && (parser->pos >= parser->expected_total))
        {
            UpgSerial_HandleComplete(parser, on_frame, on_error, user);
        }
    }
}
