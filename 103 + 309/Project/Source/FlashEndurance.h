#ifndef FLASH_ENDURANCE_H
#define FLASH_ENDURANCE_H
/* Engineering firmware only. No test endpoints or RAM overhead in production. */
#ifndef FLASH_ENDURANCE_TEST_ENABLE
#define FLASH_ENDURANCE_TEST_ENABLE 0
#endif
#define FLASH_TEST_SNAPSHOT_MARKER 0xF17EU
#define FLASH_TEST_BASE 0x2700U
#define FLASH_TEST_WORDS 96U
#define FLASH_TEST_RESTORE 0x2720U
#define FLASH_TEST_START_POLICY 0xA501U
#define FLASH_TEST_START_FORCE 0xA502U
#define FLASH_TEST_STOP 0U
#if FLASH_ENDURANCE_TEST_ENABLE
UINT8 FlashTest_Active(void);
UINT8 FlashTest_Command(UINT16 address, UINT16 value);
UINT8 FlashTest_Restore(const UINT16 *words);
void FlashTest_Read(UINT8 *bytes);
void FlashTest_Service(void);
#endif
#endif
