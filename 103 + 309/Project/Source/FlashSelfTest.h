#ifndef FLASH_SELF_TEST_H
#define FLASH_SELF_TEST_H

#include <stdint.h>
#include "conf/Project_Config.h"

#if PROJECT_CFG_STORAGE_SELF_TEST_ENABLE
/* These are test pages only; the production pairs remain at 0x0801E000. */
#define FLASH_SELF_TEST_BEGIN 0x0801C000UL
#define FLASH_SELF_TEST_END   0x0801E000UL
#define FLASH_SELF_TEST_CONFIG FLASH_SELF_TEST_BEGIN
#define FLASH_SELF_TEST_STATE  0x0801C800UL
#define FLASH_SELF_TEST_LOG    0x0801D000UL
#define FLASH_SELF_TEST_STATS  0x0801D800UL

enum FLASH_SELF_TEST_DOMAIN { FLASH_TEST_CONFIG, FLASH_TEST_SOC, FLASH_TEST_LOG,
    FLASH_TEST_STATS, FLASH_TEST_DOMAIN_COUNT };
enum FLASH_SELF_TEST_STATUS { FLASH_TEST_RUNNING=1, FLASH_TEST_PASS,
    FLASH_TEST_FAIL, FLASH_TEST_BUDGET_LIMIT };
enum FLASH_SELF_TEST_PHASE { FLASH_TEST_PREPARE, FLASH_TEST_CONFIG_PHASE,
    FLASH_TEST_SOC_PHASE, FLASH_TEST_LOG_PHASE, FLASH_TEST_STATS_PHASE,
    FLASH_TEST_BOOT_PHASE, FLASH_TEST_CLEAR_PHASE, FLASH_TEST_RECOVER_PHASE,
    FLASH_TEST_FINAL_PHASE, FLASH_TEST_COMPLETE };
enum FLASH_SELF_TEST_ERROR { FLASH_TEST_NO_ERROR, FLASH_TEST_ERASE_ERROR,
    FLASH_TEST_PROGRAM_ERROR, FLASH_TEST_READ_ERROR, FLASH_TEST_VERIFY_ERROR,
    FLASH_TEST_PROFILE_ERROR, FLASH_TEST_WATCHDOG_ERROR, FLASH_TEST_UART_ERROR,
    FLASH_TEST_SCHEDULER_ERROR, FLASH_TEST_BUDGET_ERROR };

typedef struct {
    uint32_t signature;
    uint32_t goal[FLASH_TEST_DOMAIN_COUNT];
    uint32_t count[FLASH_TEST_DOMAIN_COUNT];
    uint32_t phase;
} FLASH_SELF_TEST_PROGRESS;

typedef struct {
    FLASH_SELF_TEST_PROGRESS progress;
    uint32_t session_erases[8];
    uint32_t session_halfwords;
    uint32_t checks;
    uint32_t first_error_address;
    uint32_t first_error;
    uint8_t status;
    uint8_t resumed;
    uint8_t budget_ok;
} FLASH_SELF_TEST;

void FlashSelfTest_Init(uint32_t reset_cause);
void FlashSelfTest_Task(void);
uint8_t FlashSelfTest_IsActive(void);
#else
#define FlashSelfTest_Init(reset_cause) ((void)0)
#define FlashSelfTest_Task() ((void)0)
#define FlashSelfTest_IsActive() 0U
#endif

#endif
