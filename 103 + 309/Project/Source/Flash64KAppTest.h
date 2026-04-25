#ifndef FLASH64K_APP_TEST_H
#define FLASH64K_APP_TEST_H

void StorageFlash_RunAppQuickTest(void);
void StorageFlash_AppUseTest_Task(void);
void StorageFlash_AppUseTest_OnSocSaved(const STORAGE_FLASH_SOC_DATA *expect, UINT8 save_ok);
void StorageFlash_AppUseTest_OnAfeSaved(const UINT16 *expect, UINT16 word_count, UINT8 save_ok);

#endif
