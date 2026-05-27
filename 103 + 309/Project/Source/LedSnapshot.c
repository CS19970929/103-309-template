#include "main.h"
#include "LedSnapshot.h"

#define LED_SNAPSHOT_MAGIC       ((UINT16)0x4C53)
#define LED_SNAPSHOT_VERSION     ((UINT16)0x0001)
#define LED_SNAPSHOT_XOR_KEY     ((UINT16)0xA55A)

#define LED_SNAPSHOT_MAGIC_REG   BKP_DR2
#define LED_SNAPSHOT_DATA_REG    BKP_DR3
#define LED_SNAPSHOT_STATE_REG   BKP_DR4
#define LED_SNAPSHOT_CRC_REG     BKP_DR5

static void LedSnapshot_EnableBackupAccess(void);
static UINT16 LedSnapshot_Checksum(UINT16 data, UINT16 state);
static UINT8 LedSnapshot_RuntimeAlarm(UINT8 soc);

void LedSnapshot_Save(UINT8 soc, UINT8 flags, UINT8 power_on)
{
    UINT16 data;
    UINT16 state;
    UINT16 checksum;

    if (soc > 100) {
        soc = 100;
    }

    data = (UINT16)(((UINT16)soc << 8) | flags);
    state = (UINT16)(((UINT16)(power_on ? 1 : 0) << 8) | LED_SNAPSHOT_VERSION);
    checksum = LedSnapshot_Checksum(data, state);

    LedSnapshot_EnableBackupAccess();
    BKP_WriteBackupRegister(LED_SNAPSHOT_MAGIC_REG, LED_SNAPSHOT_MAGIC);
    BKP_WriteBackupRegister(LED_SNAPSHOT_DATA_REG, data);
    BKP_WriteBackupRegister(LED_SNAPSHOT_STATE_REG, state);
    BKP_WriteBackupRegister(LED_SNAPSHOT_CRC_REG, checksum);
}

UINT8 LedSnapshot_Load(UINT8 *soc, UINT8 *flags, UINT8 *power_on)
{
    UINT16 magic;
    UINT16 data;
    UINT16 state;
    UINT16 checksum;

    if (soc) {
        *soc = 0;
    }
    if (flags) {
        *flags = LED_SNAPSHOT_FLAG_ALARM;
    }
    if (power_on) {
        *power_on = 0;
    }

    LedSnapshot_EnableBackupAccess();
    magic = BKP_ReadBackupRegister(LED_SNAPSHOT_MAGIC_REG);
    data = BKP_ReadBackupRegister(LED_SNAPSHOT_DATA_REG);
    state = BKP_ReadBackupRegister(LED_SNAPSHOT_STATE_REG);
    checksum = BKP_ReadBackupRegister(LED_SNAPSHOT_CRC_REG);

    if (magic != LED_SNAPSHOT_MAGIC) {
        return 0;
    }
    if ((state & 0x00FF) != LED_SNAPSHOT_VERSION) {
        return 0;
    }
    if (checksum != LedSnapshot_Checksum(data, state)) {
        return 0;
    }

    if (soc) {
        *soc = (UINT8)((data >> 8) & 0x00FF);
        if (*soc > 100) {
            *soc = 100;
        }
    }
    if (flags) {
        *flags = (UINT8)(data & 0x00FF);
    }
    if (power_on) {
        *power_on = (UINT8)((state >> 8) & 0x0001);
    }

    return 1;
}

void LedSnapshot_SaveRuntime(void)
{
    UINT8 soc;
    UINT8 flags;

    soc = (UINT8)g_stCellInfoReport.SocElement.u16Soc;
    if (soc > 100) {
        soc = 100;
    }

    flags = 0;
    if (LedSnapshot_RuntimeAlarm(soc)) {
        flags |= LED_SNAPSHOT_FLAG_ALARM;
    }
    if (sys_time.power_on) {
        flags |= LED_SNAPSHOT_FLAG_POWER_ON;
    }

    LedSnapshot_Save(soc, flags, sys_time.power_on ? 1 : 0);
}

static void LedSnapshot_EnableBackupAccess(void)
{
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_PWR | RCC_APB1Periph_BKP, ENABLE);
    PWR_BackupAccessCmd(ENABLE);
}

static UINT16 LedSnapshot_Checksum(UINT16 data, UINT16 state)
{
    return (UINT16)(LED_SNAPSHOT_MAGIC ^ data ^ state ^ LED_SNAPSHOT_XOR_KEY);
}

static UINT8 LedSnapshot_RuntimeAlarm(UINT8 soc)
{
    if (soc < 20) {
        return 1;
    }
    if (g_stCellInfoReport.unMdlFault_Third.bits.b1CellUvp) {
        return 1;
    }
    if (g_stCellInfoReport.unMdlFault_Third.bits.b1BatUvp) {
        return 1;
    }
    if (g_stCellInfoReport.unMdlFault_Third.bits.b1SocLow) {
        return 1;
    }
    return 0;
}
