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
    assert(address>=FLASH_PARAMETER_RESET_BEGIN && address<=FLASH_STORAGE_END-1024);
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
/* PRODUCTION_SOURCE */
/* Only the unrelated application parameter tables are stubbed. The real boot
 * initializer, version comparison, reset sequencing and Flash code run below. */
typedef struct { UINT16 minValue,maxValue,curValue,defaultValue; } AFE_Value_Typedef;
static struct { AFE_Value_Typedef u16VcellOvp,rest[23]; } AFE_Parameters_RS485_Struction;
static STORAGE_FLASH_RW_PARAM_DATA runtime_rw;
static int reset_on_version=1;
static char firmware_version[32]="storage-test-v1";
#define BMS_SOFTWARE_VERDION_DEFAULT firmware_version
#define PROJECT_CFG_RESET_PARAMS_ON_NEW_VERSION reset_on_version
static void EEPROM_LoadDefaultRuntimeData(void) { memset(&runtime_rw,0x11,sizeof(runtime_rw)); }
static void EEPROM_BuildRWParamData(STORAGE_FLASH_RW_PARAM_DATA *data) { *data=runtime_rw; }
static UINT8 EEPROM_RWParamDataIsValid(const STORAGE_FLASH_RW_PARAM_DATA *data) { return data->protect[0]!=0xFFFF; }
static void EEPROM_ApplyRWParamData(const STORAGE_FLASH_RW_PARAM_DATA *data) { runtime_rw=*data; }
static void ReadEEPROM_EventRecord_Parameters(void) {}
/* CONFIG_INIT_SOURCE */
static unsigned char saved[16384];
static void save_flash(void) { memcpy(saved,(void *)FLASH_PARAMETER_RESET_BEGIN,sizeof(saved)); }
static void restore_flash(void) {
    memcpy((void *)FLASH_PARAMETER_RESET_BEGIN,saved,sizeof(saved)); mutations=0; cut_after=-1;
    memset(&s_flash,0,sizeof(s_flash));
}
int main(void) {
    STORAGE_FLASH_CONFIG_DATA old_value,new_value,loaded;
    STORAGE_FLASH_SOC_DATA soc,loaded_soc;
    STORAGE_FLASH_FACTORY_AGING_DATA aging={1234,FLASH_FACTORY_AGING_STATE_RUNNING,12},loaded_aging;
    int writes,cut,i;
    assert(VirtualAlloc((void *)0x08000000,0x20000,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE)==(void *)0x08000000);
    memset((void *)0x08000000,0xFF,0x20000);
    assert(VirtualAlloc((void *)0x1FFF0000,0x10000,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE)==(void *)0x1FFF0000);
    *(UINT16 *)0x1FFFF7E0=128;
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
    /* Shared state updates retain the other owner's last committed data. */
    memset(&soc,0,sizeof(soc)); soc.u16FormatVersion=FLASH_STORAGE_SOC_DATA_VERSION;
    assert(StorageFlash_SaveFactoryAgingData(&aging));
    assert(StorageFlash_SaveSocData(&soc));
    for(i=0;i<30;++i) { soc.u16SocNow=(UINT16)i; assert(StorageFlash_SaveSocData(&soc)); }
    save_flash(); soc.u16SocNow=90;
    mutations=0; assert(StorageFlash_SaveSocData(&soc)); writes=mutations;
    for(cut=0;cut<=writes;++cut) {
        restore_flash(); cut_after=cut; StorageFlash_SaveSocData(&soc); cut_after=-1;
        assert(StorageFlash_LoadSocData(&loaded_soc));
        assert(loaded_soc.u16SocNow==29 || loaded_soc.u16SocNow==90);
        assert(StorageFlash_LoadFactoryAgingData(&loaded_aging) && !memcmp(&aging,&loaded_aging,sizeof(aging)));
    }
    puts("PASS storage: SOC/aging merge, state append and rotation, power cuts retain complete state");
    memset((void *)FLASH_PARAMETER_RESET_BEGIN,0xFF,16384);
    for(i=0;i<24;++i) {
        AFE_Value_Typedef *value=(&AFE_Parameters_RS485_Struction.u16VcellOvp)+i;
        value->minValue=0; value->maxValue=1000; value->defaultValue=100;
    }
    memset((void *)FLASH_STATS_A,0x5A,2048);
    assert(InitE2PROM());
    runtime_rw.protect[0]=123; assert(StorageFlash_SaveRwParamData(&runtime_rw));
    mutations=0; assert(InitE2PROM() && runtime_rw.protect[0]==123 && !mutations);
    assert(StorageFlash_SaveSocData(&soc)); assert(StorageFlash_AppendLog(1,171));
    save_flash(); strcpy(firmware_version,"storage-test-v2");
    mutations=0; assert(InitE2PROM()); writes=mutations;
    for(cut=0;cut<=writes;++cut) {
        restore_flash(); cut_after=cut; InitE2PROM(); cut_after=-1;
        assert(InitE2PROM() && runtime_rw.protect[0]==0x1111);
        assert(!StorageFlash_LoadSocData(&loaded_soc));
        assert(!StorageFlash_LoadFactoryAgingData(&loaded_aging));
        for(i=0;i<2048;++i) assert(((UINT8 *)FLASH_STATS_A)[i]==0x5A);
        mutations=0; assert(InitE2PROM() && !mutations);
    }
    strcpy(firmware_version,"storage-test-v3"); reset_on_version=0;
    runtime_rw.protect[0]=456; assert(StorageFlash_SaveRwParamData(&runtime_rw));
    mutations=0; assert(InitE2PROM() && runtime_rw.protect[0]==456 && !mutations);
    printf("PASS storage: real boot init, once per version, disabled switch, %d interrupted resets, statistics untouched\n",writes+1);
    return 0;
}
