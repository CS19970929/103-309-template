#include "upg_core.h"
#include "upg_protocol.h"
#include "upg_serial.h"
#include "upg_utils.h"

#include <stdio.h>
#include <string.h>

typedef struct
{
    UpgCore core;
    UpgCanFrame can_frames[512];
    unsigned can_count;
    uint8_t serial[UPG_SERIAL_MAX_FRAME];
    uint16_t serial_len;
    unsigned serial_count;
    unsigned failures;
} TestEnv;

#define CHECK_TRUE(env, expr) test_check((env), (expr) ? 1 : 0, #expr, __LINE__)
#define CHECK_EQ_U32(env, actual, expected) test_check_u32((env), (uint32_t)(actual), (uint32_t)(expected), #actual, __LINE__)

static void test_check(TestEnv *env, int ok, const char *expr, int line)
{
    if (!ok)
    {
        printf("FAIL line %d: %s\n", line, expr);
        env->failures++;
    }
}

static void test_check_u32(TestEnv *env, uint32_t actual, uint32_t expected, const char *expr, int line)
{
    if (actual != expected)
    {
        printf("FAIL line %d: %s actual=0x%08lX expected=0x%08lX\n",
               line, expr, (unsigned long)actual, (unsigned long)expected);
        env->failures++;
    }
}

static int fake_can_tx(void *user, const UpgCanFrame *frame)
{
    TestEnv *env = (TestEnv *)user;
    if (env->can_count >= (sizeof(env->can_frames) / sizeof(env->can_frames[0])))
    {
        return -1;
    }
    env->can_frames[env->can_count++] = *frame;
    return 0;
}

static int fake_serial_tx(void *user, const uint8_t *data, uint16_t len)
{
    TestEnv *env = (TestEnv *)user;
    if (len > sizeof(env->serial))
    {
        return -1;
    }
    memcpy(env->serial, data, len);
    env->serial_len = len;
    env->serial_count++;
    return 0;
}

static void test_env_init(TestEnv *env)
{
    UpgHal hal;

    memset(env, 0, sizeof(*env));
    hal.can_tx = fake_can_tx;
    hal.serial_tx = fake_serial_tx;
    hal.reset = 0;
    hal.user = env;
    UpgCore_Init(&env->core, &hal);
}

static void send_pc_frame(TestEnv *env, uint8_t cmd, uint16_t seq, const uint8_t *payload, uint16_t len)
{
    uint8_t frame[UPG_SERIAL_MAX_FRAME];
    uint16_t frame_len = 0U;

    CHECK_TRUE(env, UpgSerial_Encode(cmd, seq, 0U, payload, len, frame, sizeof(frame), &frame_len) != 0U);
    UpgCore_OnSerialBytes(&env->core, frame, frame_len);
}

static uint8_t last_status(const TestEnv *env)
{
    if (env->serial_len < 13U)
    {
        return 0xFEU;
    }
    return env->serial[11];
}

static const uint8_t *last_data(const TestEnv *env)
{
    return &env->serial[13];
}

static void inject_feidao(TestEnv *env, uint8_t src, uint8_t dst, uint8_t ctrl, uint8_t index, uint8_t chd, const uint8_t data[8])
{
    UpgCanFrame frame;

    UpgFeidao_MakeFrame(&frame, src, dst, ctrl, index, chd, data);
    UpgCore_OnCanFrame(&env->core, &frame);
}

static void test_serial_object_read(void)
{
    TestEnv env;
    uint8_t payload[4];
    uint8_t ack[8] = {1, 2, 3, 4, 5, 6, 7, 8};

    test_env_init(&env);
    payload[0] = 0x02U;
    payload[1] = 0x04U;
    UpgWriteBe16(&payload[2], 1000U);
    send_pc_frame(&env, UPG_CMD_CAN_OBJECT_READ, 1U, payload, sizeof(payload));

    CHECK_EQ_U32(&env, env.can_count, 1U);
    CHECK_EQ_U32(&env, env.can_frames[0].id, UpgFeidao_BuildId(FEIDAO_NODE_IOT, FEIDAO_NODE_BATTERY, FEIDAO_CTRL_READ, 0x02U, 0x04U));

    inject_feidao(&env, FEIDAO_NODE_BATTERY, FEIDAO_NODE_IOT, FEIDAO_CTRL_ACK, 0x02U, 0x04U, ack);
    CHECK_EQ_U32(&env, last_status(&env), UPG_STATUS_OK);
    CHECK_EQ_U32(&env, last_data(&env)[0], 0x02U);
    CHECK_EQ_U32(&env, last_data(&env)[1], 0x04U);
    CHECK_EQ_U32(&env, last_data(&env)[2], 1U);

    if (env.failures == 0U)
    {
        printf("PASS test_serial_object_read\n");
    }
}

static void test_param_write_read_modify_write(void)
{
    TestEnv env;
    uint8_t payload[9];
    uint8_t read_ack[8] = {0x0F, 0xA0, 0x09, 0xC4, 0, 0, 0, 0};
    uint8_t write_ack[8] = {0};

    test_env_init(&env);
    UpgWriteBe16(&payload[0], 0x1001U);
    UpgWriteBeS32(&payload[2], 4200);
    payload[6] = 0xA5U;
    UpgWriteBe16(&payload[7], 1000U);
    send_pc_frame(&env, UPG_CMD_PARAM_WRITE, 2U, payload, sizeof(payload));

    CHECK_EQ_U32(&env, env.can_count, 1U);
    CHECK_EQ_U32(&env, env.can_frames[0].id, UpgFeidao_BuildId(FEIDAO_NODE_IOT, FEIDAO_NODE_BATTERY, FEIDAO_CTRL_READ, 0x20U, 0x00U));

    inject_feidao(&env, FEIDAO_NODE_BATTERY, FEIDAO_NODE_IOT, FEIDAO_CTRL_ACK, 0x20U, 0x00U, read_ack);
    CHECK_EQ_U32(&env, env.can_count, 2U);
    CHECK_EQ_U32(&env, env.can_frames[1].id, UpgFeidao_BuildId(FEIDAO_NODE_IOT, FEIDAO_NODE_BATTERY, FEIDAO_CTRL_WRITE, 0x20U, 0x00U));
    CHECK_EQ_U32(&env, env.can_frames[1].data[0], 0x10U);
    CHECK_EQ_U32(&env, env.can_frames[1].data[1], 0x68U);
    CHECK_EQ_U32(&env, env.can_frames[1].data[2], 0x09U);
    CHECK_EQ_U32(&env, env.can_frames[1].data[3], 0xC4U);

    inject_feidao(&env, FEIDAO_NODE_BATTERY, FEIDAO_NODE_IOT, FEIDAO_CTRL_ACK, 0x20U, 0x00U, write_ack);
    CHECK_EQ_U32(&env, last_status(&env), UPG_STATUS_OK);
    CHECK_EQ_U32(&env, UpgReadBe16(&last_data(&env)[0]), 0x1001U);
    CHECK_EQ_U32(&env, UpgReadBe32(&last_data(&env)[2]), 4200U);

    if (env.failures == 0U)
    {
        printf("PASS test_param_write_read_modify_write\n");
    }
}

static void test_upgrade_prepare_commit_finish(void)
{
    TestEnv env;
    uint8_t prep[10];
    uint8_t start_ack[8] = {1, 0, 0, 0, 0, 0, 0, 0};
    uint8_t data_payload[6 + 16];
    uint8_t commit[6];
    uint8_t chunk_ack[8] = {FEIDAO_UPGRADE_STATUS_CHUNK_OK, 0, 0, 0, 0, 0, 0, 0};
    uint8_t done_ack[8] = {FEIDAO_UPGRADE_STATUS_DONE, 0, 0, 0, 0, 0, 0, 0};
    uint16_t i;
    unsigned can_before_commit;

    test_env_init(&env);
    UpgWriteBe32(&prep[0], 32U);
    UpgWriteBe16(&prep[4], 0x1234U);
    UpgWriteBe16(&prep[6], 1U);
    UpgWriteBe16(&prep[8], 1000U);
    send_pc_frame(&env, UPG_CMD_UPGRADE_PREPARE, 3U, prep, sizeof(prep));

    CHECK_EQ_U32(&env, env.can_count, 1U);
    CHECK_EQ_U32(&env, env.can_frames[0].id, UpgFeidao_BuildId(FEIDAO_NODE_IOT, FEIDAO_NODE_BATTERY, FEIDAO_CTRL_WRITE, FEIDAO_UPGRADE_START_INDEX, 0x00U));
    CHECK_EQ_U32(&env, UpgReadBe16(&env.can_frames[0].data[0]), 1U);
    CHECK_EQ_U32(&env, UpgReadBe16(&env.can_frames[0].data[2]), 0x1234U);

    inject_feidao(&env, FEIDAO_NODE_BATTERY, FEIDAO_NODE_IOT, FEIDAO_CTRL_ACK, FEIDAO_UPGRADE_START_INDEX, FEIDAO_UPGRADE_START_ACK_CHD, start_ack);
    CHECK_EQ_U32(&env, last_status(&env), UPG_STATUS_OK);

    UpgWriteBe16(&data_payload[0], 0U);
    UpgWriteBe16(&data_payload[2], 0U);
    UpgWriteBe16(&data_payload[4], 16U);
    for (i = 0U; i < 16U; i++)
    {
        data_payload[6U + i] = (uint8_t)i;
    }
    send_pc_frame(&env, UPG_CMD_UPGRADE_PACKET_DATA, 4U, data_payload, sizeof(data_payload));
    CHECK_EQ_U32(&env, last_status(&env), UPG_STATUS_OK);

    can_before_commit = env.can_count;
    UpgWriteBe16(&commit[0], 0U);
    UpgWriteBe16(&commit[2], 16U);
    UpgWriteBe16(&commit[4], 1000U);
    send_pc_frame(&env, UPG_CMD_UPGRADE_PACKET_COMMIT, 5U, commit, sizeof(commit));
    CHECK_EQ_U32(&env, env.can_count, can_before_commit + 4U);
    CHECK_EQ_U32(&env, env.can_frames[can_before_commit].id, UpgFeidao_BuildId(FEIDAO_NODE_IOT, FEIDAO_NODE_BATTERY, FEIDAO_CTRL_LONG_START, FEIDAO_UPGRADE_DATA_INDEX, 0x00U));
    CHECK_EQ_U32(&env, env.can_frames[can_before_commit + 1U].id, UpgFeidao_BuildId(FEIDAO_NODE_IOT, FEIDAO_NODE_BATTERY, FEIDAO_CTRL_LONG_DATA, FEIDAO_UPGRADE_DATA_INDEX, 0x00U));
    CHECK_EQ_U32(&env, env.can_frames[can_before_commit + 2U].id, UpgFeidao_BuildId(FEIDAO_NODE_IOT, FEIDAO_NODE_BATTERY, FEIDAO_CTRL_LONG_DATA, FEIDAO_UPGRADE_DATA_INDEX, 0x01U));
    CHECK_EQ_U32(&env, env.can_frames[can_before_commit + 3U].id, UpgFeidao_BuildId(FEIDAO_NODE_IOT, FEIDAO_NODE_BATTERY, FEIDAO_CTRL_LONG_END, FEIDAO_UPGRADE_DATA_INDEX, 0x00U));

    inject_feidao(&env, FEIDAO_NODE_BATTERY, FEIDAO_NODE_IOT, FEIDAO_CTRL_LONG_START, FEIDAO_UPGRADE_DATA_INDEX, 0x00U, chunk_ack);
    CHECK_EQ_U32(&env, last_status(&env), UPG_STATUS_OK);

    send_pc_frame(&env, UPG_CMD_UPGRADE_FINISH, 6U, commit, 2U);
    inject_feidao(&env, FEIDAO_NODE_BATTERY, FEIDAO_NODE_IOT, FEIDAO_CTRL_LONG_START, FEIDAO_UPGRADE_DATA_INDEX, 0x00U, done_ack);
    CHECK_EQ_U32(&env, last_status(&env), UPG_STATUS_OK);

    if (env.failures == 0U)
    {
        printf("PASS test_upgrade_prepare_commit_finish\n");
    }
}

static void test_snapshot_cache(void)
{
    TestEnv env;
    uint8_t data[8];

    test_env_init(&env);
    UpgCore_SetNow(&env.core, 1000U);
    UpgWriteBe32(&data[0], 54000U);
    UpgWriteBe32(&data[4], (uint32_t)(int32_t)-1200);
    inject_feidao(&env, FEIDAO_NODE_BATTERY, FEIDAO_NODE_BROADCAST, FEIDAO_CTRL_WRITE, 0x02U, 0x00U, data);

    send_pc_frame(&env, UPG_CMD_READ_BMS_SNAPSHOT, 7U, 0, 0U);
    CHECK_EQ_U32(&env, last_status(&env), UPG_STATUS_OK);
    CHECK_EQ_U32(&env, UpgReadBe32(&last_data(&env)[0]), 0x00000001UL);
    CHECK_EQ_U32(&env, UpgReadBe32(&last_data(&env)[8]), 54000U);

    if (env.failures == 0U)
    {
        printf("PASS test_snapshot_cache\n");
    }
}

int main(void)
{
    TestEnv env;

    memset(&env, 0, sizeof(env));
    test_serial_object_read();
    test_param_write_read_modify_write();
    test_upgrade_prepare_commit_finish();
    test_snapshot_cache();

    return 0;
}
