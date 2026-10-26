#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
typedef uint8_t UINT8; typedef uint16_t UINT16; typedef uint32_t UINT32;
typedef int32_t INT32;
typedef enum { FLASH_COMPLETE, FLASH_ERROR_PG } FLASH_Status;
#define STM32F10X_MD
#define FLASH_FLAG_EOP 1
#define FLASH_FLAG_PGERR 2
#define FLASH_FLAG_WRPRTERR 4
#define ERROR_EEPROM_STORE 1
#include "Flash.h"
static int mutations, cut_after=-1, errors;
static void System_ERROR_UserCallback(int error) { ++errors; }
static void FLASH_Unlock(void) {}
static void FLASH_Lock(void) {}
static void FLASH_ClearFlag(int flags) {}
static int mutation_allowed(void) { return cut_after<0 || mutations<cut_after; }
static FLASH_Status FLASH_ErasePage(uint32_t address) {
    assert(address>=FLASH_STORAGE_BEGIN && address<=FLASH_STORAGE_END-1024);
    assert((address & 1023)==0);
    if(!mutation_allowed()) return FLASH_ERROR_PG;
    ++mutations; memset((void *)(uintptr_t)address,0xFF,1024); return FLASH_COMPLETE;
}
static FLASH_Status FLASH_ProgramHalfWord(uint32_t address,uint16_t value) {
    UINT16 *p=(UINT16 *)(uintptr_t)address;
    assert(address>=FLASH_STORAGE_BEGIN && address<FLASH_STORAGE_END && !(address & 1));
    if(!mutation_allowed()) return FLASH_ERROR_PG;
    assert((*p & value)==value); ++mutations; *p=value; return FLASH_COMPLETE;
}
UINT16 FlashReadOneHalfWord(UINT32 address) { return *(UINT16 *)(uintptr_t)address; }
static UINT8 StorageFlash_IsAreaBlank(uint32_t address,UINT16 length) {
    UINT16 i; for(i=0;i<length;i+=2) if(FlashReadOneHalfWord(address+i)!=0xFFFF) return 0;
    return 1;
}
/* PRODUCTION_SOURCE */
static unsigned char saved[8192];
static void save_flash(void) { memcpy(saved,(void *)FLASH_STORAGE_BEGIN,sizeof(saved)); }
static void restore_flash(void) {
    memcpy((void *)FLASH_STORAGE_BEGIN,saved,sizeof(saved)); mutations=0; cut_after=-1;
    memset(&s_flash,0,sizeof(s_flash));
}
int main(void) {
    STORAGE_FLASH_CONFIG_DATA old_value,new_value,loaded;
    int writes,cut;
    assert(VirtualAlloc((void *)0x08000000,0x20000,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE)==(void *)0x08000000);
    memset((void *)0x08000000,0xFF,0x20000);
    memset(&old_value,0x12,sizeof(old_value)); memset(&new_value,0x34,sizeof(new_value));
    assert(!StorageFlash_LoadConfig(&loaded));
    assert(StorageFlash_SaveConfig(&old_value)); save_flash();
    mutations=0; assert(StorageFlash_SaveConfig(&old_value) && mutations==0);
    assert(StorageFlash_SaveConfig(&new_value)); writes=mutations;
    for(cut=0;cut<=writes;++cut) {
        restore_flash(); cut_after=cut;
        StorageFlash_SaveConfig(&new_value);
        cut_after=-1; memset(&s_flash,0,sizeof(s_flash));
        assert(StorageFlash_LoadConfig(&loaded));
        assert(!memcmp(&loaded,&old_value,sizeof(loaded)) || !memcmp(&loaded,&new_value,sizeof(loaded)));
        assert(StorageFlash_SaveConfig(&new_value));
        assert(StorageFlash_LoadConfig(&loaded) && !memcmp(&loaded,&new_value,sizeof(loaded)));
    }
    /* Corrupt metadata and commit words, not just the payload. */
    restore_flash(); *(UINT32 *)(FLASH_CONFIG_A+4)^=1;
    assert(!StorageFlash_LoadConfig(&loaded));
    restore_flash(); *(UINT16 *)(FLASH_CONFIG_A+14)=0xFFFF;
    assert(!StorageFlash_LoadConfig(&loaded));
    restore_flash();
    assert(Storage_Erase(FLASH_CONFIG_B));
    assert(Storage_WriteRecord(FLASH_CONFIG_B,STORAGE_CONFIG_MAGIC,UINT32_MAX,&new_value,sizeof(new_value)));
    assert(Storage_Erase(FLASH_CONFIG_A));
    assert(Storage_WriteRecord(FLASH_CONFIG_A,STORAGE_CONFIG_MAGIC,0,&old_value,sizeof(old_value)));
    assert(StorageFlash_LoadConfig(&loaded) && !memcmp(&loaded,&old_value,sizeof(loaded)));
    assert(!Storage_Erase(0x08004800) && !Storage_Erase(FLASH_STORAGE_END));
    assert(!StorageFlash_SaveConfig(0) && !StorageFlash_LoadConfig(0));
    printf("PASS storage: config atomic commit, %d cut points, header CRC, sequence wrap, bounds, no-op saves\n",writes+1);
    return 0;
}
