#include "main.h"
#include "can_app.h"
#include "Can_HDX.h"
#include "FactoryAging.h"
#include <string.h>

static UINT8 can_app_crc_ok(const UINT8 data[8])
{
    UINT16 expect_crc = (UINT16)(((UINT16)data[6] << 8) | data[7]);
    UINT16 actual_crc = Sci_CRC16RTU((UINT8 *)data, 6U);

    return (expect_crc == actual_crc) ? 1U : 0U;
}

static void can_app_fill_crc(UINT8 data[8])
{
    UINT16 crc = Sci_CRC16RTU(data, 6U);
    data[6] = (UINT8)(crc >> 8);
    data[7] = (UINT8)crc;
}

static UINT8 can_app_u16_to_percent(UINT16 value)
{
    return (value > 100U) ? 100U : (UINT8)value;
}

static UINT8 can_app_aging_guard_ok(const UINT8 data[8], UINT8 action)
{
    return ((data[3] == CAN_APP_AGING_GUARD) &&
            (data[4] == action) &&
            (data[5] == (UINT8)CAN_ADRESS_STD_ID)) ? 1U : 0U;
}

static void can_app_fill_aging_ack(UINT8 *value0, UINT8 *value1)
{
    UINT32 hours;

    if (value0 != 0)
    {
        *value0 = FactoryAging_GetState();
    }
    if (value1 != 0)
    {
        hours = (FactoryAging_GetRemainingSeconds() + 3599U) / 3600U;
        *value1 = (hours > 0xFFU) ? 0xFFU : (UINT8)hours;
    }
}

static void can_app_send_ack(UINT8 cmd, UINT8 status, UINT8 value0, UINT8 value1)
{
    CanTxMsg tx_msg;

    memset(&tx_msg, 0, sizeof(tx_msg));
    tx_msg.StdId = CAN_APP_ACK_ID;
    tx_msg.IDE = CAN_ID_STD;
    tx_msg.RTR = CAN_RTR_DATA;
    tx_msg.DLC = 8U;
    tx_msg.Data[0] = 0x5AU;
    tx_msg.Data[1] = 0xA5U;
    tx_msg.Data[2] = cmd;
    tx_msg.Data[3] = status;
    tx_msg.Data[4] = value0;
    tx_msg.Data[5] = value1;
    can_app_fill_crc(tx_msg.Data);
    (void)Can_HDX_Transmit(&tx_msg);
}

static void can_app_send_word_frame(UINT8 seq, UINT16 value)
{
    CanTxMsg tx_msg;

    memset(&tx_msg, 0, sizeof(tx_msg));
    tx_msg.StdId = CAN_APP_ACK_ID;
    tx_msg.IDE = CAN_ID_STD;
    tx_msg.RTR = CAN_RTR_DATA;
    tx_msg.DLC = 8U;
    tx_msg.Data[0] = 0x5AU;
    tx_msg.Data[1] = 0xA5U;
    tx_msg.Data[2] = CAN_APP_CMD_READ_BLOCK_DATA;
    tx_msg.Data[3] = seq;
    tx_msg.Data[4] = (UINT8)(value >> 8);
    tx_msg.Data[5] = (UINT8)value;
    can_app_fill_crc(tx_msg.Data);
    (void)Can_HDX_Transmit(&tx_msg);
}

static UINT8 can_app_status_from_host_error(UINT8 error)
{
    if (error == RS485_OK)
    {
        return CAN_APP_ACK_OK;
    }
    if ((error == RS485_SCIL_ERR_ADDRESS) ||
        (error == RS485_SCIL_ERR_COUNT) ||
        (error == RS485_SCIL_ERR_RANGE))
    {
        return CAN_APP_ACK_BAD_PARAM;
    }
    return CAN_APP_ACK_BMS_ERROR;
}

static void can_app_stop_read_block(CanAppRuntime *app)
{
    app->read_block_active = 0U;
    app->read_block_count = 0U;
    app->read_block_index = 0U;
}

void CanApp_ServiceReadBlock(UINT32 now_tick, CanAppRuntime *app,
                             UINT32 tick, UINT8 tx_queue_count)
{
    if (app->read_block_active == 0U)
    {
        return;
    }
    if (app->read_block_index >= app->read_block_count)
    {
        can_app_stop_read_block(app);
        return;
    }
    if (tx_queue_count > (32U - 4U))
    {
        return;
    }

    {
        UINT32 elapsed = (UINT32)(tick - app->read_block_last_tick);
        if (elapsed < CAN_APP_READ_BLOCK_FRAME_INTERVAL_TICKS)
        {
            return;
        }
    }

    (void)now_tick;
    can_app_send_word_frame(app->read_block_index,
                            app->read_block_words[app->read_block_index]);
    app->read_block_index++;
    app->read_block_last_tick = tick;
    if (app->read_block_index >= app->read_block_count)
    {
        can_app_stop_read_block(app);
    }
}

void CanApp_ServiceEnterIapDelay(CanAppRuntime *app)
{
    if (app->enter_iap_delay_ticks == 0U)
    {
        return;
    }
    if (--app->enter_iap_delay_ticks > 0U)
    {
        return;
    }
    APP_To_IAP_Jump();
}

UINT8 CanApp_IsReadBlockActive(const CanAppRuntime *app)
{
    return app->read_block_active;
}

void CanApp_HandleCmd(const UINT8 data[8], CanAppRuntime *app)
{
    UINT8 status = CAN_APP_ACK_OK;
    UINT8 value0 = 0U;
    UINT8 value1 = 0U;
    UINT8 cmd;
    UINT16 reg_addr;
    UINT16 reg_value;
    UINT8 reg_count;
    UINT8 host_error;

    if ((data[0] != 0xA5U) ||
        (data[1] != 0x5AU) ||
        (can_app_crc_ok(data) == 0U))
    {
        return;
    }

    cmd = data[2];
    if (cmd != CAN_APP_CMD_READ_BLOCK_DATA)
    {
        can_app_stop_read_block(app);
    }

    switch (cmd)
    {
    case CAN_APP_CMD_GET_STATUS:
        value0 = can_app_u16_to_percent(g_stCellInfoReport.SocElement.u16Soc);
        value1 = can_app_u16_to_percent(g_stCellInfoReport.SocElement.u16Soh);
        break;

    case CAN_APP_CMD_ENTER_IAP:
        if ((data[3] != 0xC3U) ||
            (data[4] != 0x3CU) ||
            (data[5] != (UINT8)CAN_ADRESS_STD_ID))
        {
            status = CAN_APP_ACK_BAD_PARAM;
            break;
        }
        if (AppUpgrade_RequestIap() == 0U)
        {
            status = CAN_APP_ACK_FLASH_ERR;
            break;
        }
        value0 = 0x08U;
        value1 = 0x48U;
        app->enter_iap_delay_ticks = CAN_APP_ENTER_IAP_DELAY_TICKS;
        break;

    case CAN_APP_CMD_READ_REG:
        reg_addr = (UINT16)(((UINT16)data[3] << 8) | data[4]);
        host_error = Sci_HostReadWords(reg_addr, 1U, &reg_value);
        status = can_app_status_from_host_error(host_error);
        if (status == CAN_APP_ACK_OK)
        {
            value0 = (UINT8)(reg_value >> 8);
            value1 = (UINT8)reg_value;
        }
        break;

    case CAN_APP_CMD_READ_BLOCK:
        reg_addr = (UINT16)(((UINT16)data[3] << 8) | data[4]);
        reg_count = data[5];
        if ((reg_count == 0U) ||
            (reg_count > CAN_APP_READ_BLOCK_MAX_WORDS) ||
            (((UINT32)reg_addr + (UINT32)reg_count - 1U) > (UINT32)0xFFFFU))
        {
            status = CAN_APP_ACK_BAD_PARAM;
            break;
        }
        host_error = Sci_HostReadWords(reg_addr, reg_count, app->read_block_words);
        status = can_app_status_from_host_error(host_error);
        if (status == CAN_APP_ACK_OK)
        {
            value0 = reg_count;
            value1 = 0U;
            app->read_block_count = reg_count;
            app->read_block_index = 0U;
            app->read_block_active = 1U;
            app->read_block_last_tick = 0U;
        }
        break;

    case CAN_APP_CMD_WRITE_PREP:
        app->write_addr = (UINT16)(((UINT16)data[3] << 8) | data[4]);
        app->write_value_hi = data[5];
        app->write_pending = 1U;
        value0 = data[3];
        value1 = data[4];
        break;

    case CAN_APP_CMD_WRITE_COMMIT:
        if (app->write_pending == 0U)
        {
            status = CAN_APP_ACK_BAD_CMD;
            break;
        }
        {
            UINT16 write_value = (UINT16)(((UINT16)app->write_value_hi << 8) | data[3]);
            host_error = Sci_HostWriteSingle(app->write_addr, write_value);
            status = can_app_status_from_host_error(host_error);
        }
        app->write_pending = 0U;
        break;

    case CAN_APP_CMD_AGING_START:
        if (!can_app_aging_guard_ok(data, CAN_APP_AGING_ACTION_START))
        {
            status = CAN_APP_ACK_BAD_PARAM;
            break;
        }
        if (!FactoryAging_StartFromHost())
        {
            status = CAN_APP_ACK_BMS_ERROR;
        }
        can_app_fill_aging_ack(&value0, &value1);
        break;

    case CAN_APP_CMD_AGING_STOP:
        if (!can_app_aging_guard_ok(data, CAN_APP_AGING_ACTION_STOP))
        {
            status = CAN_APP_ACK_BAD_PARAM;
            break;
        }
        if (!FactoryAging_StopFromHost())
        {
            status = CAN_APP_ACK_BMS_ERROR;
        }
        can_app_fill_aging_ack(&value0, &value1);
        break;

    case CAN_APP_CMD_AGING_RESET_TIME:
        if (!can_app_aging_guard_ok(data, CAN_APP_AGING_ACTION_RESET_TIME))
        {
            status = CAN_APP_ACK_BAD_PARAM;
            break;
        }
        FactoryAging_ResetTime();
        can_app_fill_aging_ack(&value0, &value1);
        break;

    case CAN_APP_CMD_AGING_SET_HOURS:
        {
            UINT16 hours = (UINT16)(((UINT16)data[6] << 8) | data[7]);
            if (!FactoryAging_SetDurationHours(hours))
            {
                status = CAN_APP_ACK_BAD_PARAM;
            }
        }
        break;

    default:
        status = CAN_APP_ACK_BAD_CMD;
        break;
    }

    can_app_send_ack(cmd, status, value0, value1);
}