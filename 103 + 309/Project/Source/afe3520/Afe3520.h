#ifndef AFE3520_H
#define AFE3520_H

#include "conf.h"
#include "stm32f10x.h"
#include <stdint.h>
#include <stdbool.h>

/* SH3673510/14/17/20 CV1.0A, SH3673520 = 20 cells max. */
#define AFE3520_CELL_MAX                 20U
#define AFE3520_TEMP_MAX                 4U
#define AFE3520_SPI_RETRY_MAX            5U
#define AFE3520_SPI_MAX_HZ               1000000UL

#define AFE3520_CMD_WRITE                0x01U
#define AFE3520_CMD_READ                 0x02U
#define AFE3520_CMD_RESET                0x0BU
#define AFE3520_ACK_OK                   0xA5U
#define AFE3520_ACK_FAIL                 0xFFU

/* Writable/readable RAM map. */
#define AFE3520_REG_SCONF1               0x40U
#define AFE3520_REG_SCONF2               0x41U
#define AFE3520_REG_SCONF3               0x42U
#define AFE3520_REG_SCONF4               0x43U
#define AFE3520_REG_SCONF5               0x44U
#define AFE3520_REG_SCONF6               0x45U
#define AFE3520_REG_SCONF7               0x46U
#define AFE3520_REG_OWV_ALARMH           0x47U
#define AFE3520_REG_ALARML               0x48U
#define AFE3520_REG_OVT_OVH              0x49U
#define AFE3520_REG_OVL                  0x4AU
#define AFE3520_REG_UVT_UVH              0x4BU
#define AFE3520_REG_UVL                  0x4CU
#define AFE3520_REG_OCD1V_OCD1T          0x4DU
#define AFE3520_REG_OCD2V_OCD2T          0x4EU
#define AFE3520_REG_SCV_SCT              0x4FU
#define AFE3520_REG_OCCV_OCCT            0x50U
#define AFE3520_REG_OTC                  0x51U
#define AFE3520_REG_OTD                  0x52U
#define AFE3520_REG_UTC                  0x53U
#define AFE3520_REG_UTD                  0x54U
#define AFE3520_REG_BALANCEH             0x55U
#define AFE3520_REG_BALANCEM             0x56U
#define AFE3520_REG_BALANCEL             0x57U
#define AFE3520_REG_FLAG1                0x58U
#define AFE3520_REG_FLAG2                0x59U
#define AFE3520_REG_FLAG3                0x5AU
#define AFE3520_REG_BSTATUS1             0x5BU
#define AFE3520_REG_BSTATUS2             0x5CU
#define AFE3520_REG_TEMP1H               0x5DU
#define AFE3520_REG_TEMPIH               0x65U
#define AFE3520_REG_CURH                 0x67U
#define AFE3520_REG_CELL1H               0x69U
#define AFE3520_REG_CADCDH               0x91U
#define AFE3520_REG_VTOPH                0x93U
#define AFE3520_REG_VCHGRH               0x95U
#define AFE3520_REG_OWDH                 0x97U
#define AFE3520_REG_LAST                 0x99U

#define AFE3520_SCONF2_LTCLR              0x80U
#define AFE3520_SCONF2_PD_EN              0x40U
#define AFE3520_SCONF2_PD_CTL             0x20U
#define AFE3520_SCONF2_PUMP_EN            0x10U
#define AFE3520_SCONF2_PDSG_CTL           0x08U
#define AFE3520_SCONF2_PDSGMOS            0x04U
#define AFE3520_SCONF2_DSGMOS             0x02U
#define AFE3520_SCONF2_CHGMOS             0x01U

#define AFE3520_SCONF5_MOS_EN              0x20U
#define AFE3520_SCONF5_OCC_EN              0x10U
#define AFE3520_SCONF5_CADC_EN             0x08U
#define AFE3520_SCONF5_WDT_EN              0x04U

#define AFE3520_SCONF6_TS4_EN              0x80U
#define AFE3520_SCONF6_TS3_EN              0x40U
#define AFE3520_SCONF6_TS2_EN              0x20U
#define AFE3520_SCONF6_TS1_EN              0x10U
#define AFE3520_SCONF6_SC_EN               0x08U
#define AFE3520_SCONF6_OCD_EN              0x04U
#define AFE3520_SCONF6_UV_EN               0x02U
#define AFE3520_SCONF6_OV_EN               0x01U

#define AFE3520_FLAG1_OV                   0x01U
#define AFE3520_FLAG1_UV                   0x02U
#define AFE3520_FLAG1_OCD1                 0x04U
#define AFE3520_FLAG1_OCD2                 0x08U
#define AFE3520_FLAG1_SC                   0x10U
#define AFE3520_FLAG1_OCC                  0x20U
#define AFE3520_FLAG1_WK                   0x40U
#define AFE3520_FLAG1_RST1                 0x80U

#define AFE3520_FLAG2_CADC                 0x01U
#define AFE3520_FLAG2_VADC                 0x02U
#define AFE3520_FLAG2_WDT                  0x04U
#define AFE3520_FLAG2_OWD                  0x08U
#define AFE3520_FLAG2_UTC                  0x10U
#define AFE3520_FLAG2_OTC                  0x20U
#define AFE3520_FLAG2_UTD                  0x40U
#define AFE3520_FLAG2_OTD                  0x80U

#define AFE3520_BSTATUS2_BAL                0x08U
#define AFE3520_BSTATUS2_IDLE               0x10U
#define AFE3520_BSTATUS2_SLEEP              0x20U
#define AFE3520_BSTATUS2_DSGING             0x40U
#define AFE3520_BSTATUS2_CHGING             0x80U

#define AFE3520_MODE_NORMAL                 0x00U
#define AFE3520_MODE_IDLE                   0x55U
#define AFE3520_MODE_SLEEP                  0xAAU
#define AFE3520_MODE_POWERDOWN              0x33U

/* Unified fault model. Directional blocks are separated from global blocks. */
#define AFE3520_BLOCK_CHG_HW_OV              (1UL << 0)
#define AFE3520_BLOCK_CHG_HW_OCC             (1UL << 1)
#define AFE3520_BLOCK_CHG_HW_TEMP            (1UL << 2)
#define AFE3520_BLOCK_CHG_SW_OV              (1UL << 3)
#define AFE3520_BLOCK_CHG_SW_OCP             (1UL << 4)
#define AFE3520_BLOCK_CHG_SW_TEMP            (1UL << 5)
#define AFE3520_BLOCK_DSG_HW_UV              (1UL << 8)
#define AFE3520_BLOCK_DSG_HW_OCD             (1UL << 9)
#define AFE3520_BLOCK_DSG_HW_TEMP             (1UL << 10)
#define AFE3520_BLOCK_DSG_SW_UV              (1UL << 11)
#define AFE3520_BLOCK_DSG_SW_OCP             (1UL << 12)
#define AFE3520_BLOCK_DSG_SW_TEMP             (1UL << 13)
#define AFE3520_BLOCK_GLOBAL_SHORT            (1UL << 16)
#define AFE3520_BLOCK_GLOBAL_AFE_COMM         (1UL << 17)
#define AFE3520_BLOCK_GLOBAL_AFE_CONFIG       (1UL << 18)
#define AFE3520_BLOCK_GLOBAL_WDT              (1UL << 19)
#define AFE3520_BLOCK_GLOBAL_INTERNAL_TEMP    (1UL << 20)
#define AFE3520_BLOCK_GLOBAL_OPEN_WIRE        (1UL << 21)
#define AFE3520_BLOCK_GLOBAL_SYSTEM           (1UL << 22)

#define AFE3520_GLOBAL_BLOCK_MASK (AFE3520_BLOCK_GLOBAL_SHORT | \
                                   AFE3520_BLOCK_GLOBAL_AFE_COMM | \
                                   AFE3520_BLOCK_GLOBAL_AFE_CONFIG | \
                                   AFE3520_BLOCK_GLOBAL_WDT | \
                                   AFE3520_BLOCK_GLOBAL_INTERNAL_TEMP | \
                                   AFE3520_BLOCK_GLOBAL_OPEN_WIRE | \
                                   AFE3520_BLOCK_GLOBAL_SYSTEM)

typedef enum
{
    AFE3520_OK = 0,
    AFE3520_ERR_ARG,
    AFE3520_ERR_SPI,
    AFE3520_ERR_CRC,
    AFE3520_ERR_ACK,
    AFE3520_ERR_VERIFY,
    AFE3520_ERR_CONFIG
} AFE3520_RESULT;

typedef struct
{
    uint32_t transferCount;
    uint32_t retryCount;
    uint32_t crcErrorCount;
    uint32_t ackErrorCount;
    uint32_t verifyErrorCount;
    uint32_t resetCount;
    uint32_t configRepairCount;
    AFE3520_RESULT lastError;
} AFE3520_DIAG;

typedef struct
{
    uint8_t sconf1;
    uint8_t sconf2;
    uint8_t sconf3;
    uint8_t sconf4;
    uint8_t sconf5;
    uint8_t sconf6;
    uint8_t sconf7;
    uint8_t alarmh;
    uint8_t alarml;
    uint8_t ovtOvh;
    uint8_t ovl;
    uint8_t uvtUvh;
    uint8_t uvl;
    uint8_t ocd1;
    uint8_t ocd2;
    uint8_t sc;
    uint8_t occ;
    uint8_t otc;
    uint8_t otd;
    uint8_t utc;
    uint8_t utd;
} AFE3520_REG_CONFIG;

typedef struct
{
    uint8_t valid;
    uint8_t flag1;
    uint8_t flag2;
    uint8_t flag3;
    uint8_t bstatus1;
    uint8_t bstatus2;
    uint16_t cellMv[AFE3520_CELL_MAX];
    int16_t tempDeciC[AFE3520_TEMP_MAX];
    int16_t internalTempDeciC;
    int32_t currentMa;
    uint16_t cadcRaw;
    uint16_t vadcRaw;
    uint16_t vtopRaw;
    uint16_t chargerRaw;
    uint32_t openWireMask;
    uint32_t sampleSequence;
} AFE3520_SNAPSHOT;

void Afe3520_PortInit(void);
AFE3520_RESULT Afe3520_Read(uint8_t reg, uint8_t *data, uint8_t len);
AFE3520_RESULT Afe3520_Write(uint8_t reg, uint8_t value);
AFE3520_RESULT Afe3520_SoftReset(void);
AFE3520_RESULT Afe3520_Init(void);
AFE3520_RESULT Afe3520_ApplyConfig(const AFE3520_REG_CONFIG *cfg);
AFE3520_RESULT Afe3520_VerifyConfig(const AFE3520_REG_CONFIG *cfg);
AFE3520_RESULT Afe3520_Service(void);
AFE3520_RESULT Afe3520_ClearFlags(uint8_t flag1Mask, uint8_t flag2Mask);
AFE3520_RESULT Afe3520_SetMos(uint8_t chargeOn, uint8_t dischargeOn, uint8_t preDischargeOn);
AFE3520_RESULT Afe3520_SetBalance(uint32_t mask);
AFE3520_RESULT Afe3520_EnterIdle(void);
AFE3520_RESULT Afe3520_EnterSleep(void);
AFE3520_RESULT Afe3520_EnterPowerDown(void);
AFE3520_RESULT Afe3520_TriggerOpenWire(void);
const AFE3520_SNAPSHOT *Afe3520_GetSnapshot(void);
const AFE3520_DIAG *Afe3520_GetDiag(void);
uint8_t Afe3520_IsReady(void);
uint8_t Afe3520_ConfigDirty(void);
void Afe3520_MarkConfigDirty(void);

/* Physical conversion/config helpers. */
uint8_t Afe3520_EncodeOvUvMv(uint16_t mv, uint8_t *hi2, uint8_t *lo8);
uint8_t Afe3520_PickDelayCode(const uint16_t *table, uint8_t count, uint16_t targetMs);
uint8_t Afe3520_EncodeOcd1Mv(uint16_t senseMv);
uint8_t Afe3520_EncodeOcd2Mv(uint16_t senseMv);
uint8_t Afe3520_EncodeOccUv(uint32_t senseUv);

#endif /* AFE3520_H */
