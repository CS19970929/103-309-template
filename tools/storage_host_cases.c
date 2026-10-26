#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <assert.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
typedef uint8_t UINT8; typedef uint16_t UINT16; typedef uint32_t UINT32;
typedef int8_t INT8; typedef int16_t INT16; typedef int32_t INT32;
typedef enum { FLASH_COMPLETE, FLASH_ERROR_PG } FLASH_Status;
#define STM32F10X_MD
#define FLASH_FLAG_EOP 1
#define FLASH_FLAG_PGERR 2
#define FLASH_FLAG_WRPRTERR 4
#define ERROR_EEPROM_STORE 1
/* RESET_FLAGS */
static UINT32 tick;
static UINT32 SysTime_Get10msTickCount(void) { return tick; }
#include "Flash.h"
struct RS485MSG;
#include "LogRecord.h"
#define PROJECT_CFG_LOG_RECORD_REPEAT_MIN_INTERVAL_SEC 0
static int mutations, cut_after=-1, errors, error_after_commit;
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
    assert((*p & value)==value); ++mutations; *p=value;
    return error_after_commit && value==0xA55A ? FLASH_ERROR_PG : FLASH_COMPLETE;
}
UINT16 FlashReadOneHalfWord(UINT32 address) { return *(UINT16 *)(uintptr_t)address; }
static void host_storage_init(void) {
    assert(VirtualAlloc((void *)0x08000000,0x20000,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE)==(void *)0x08000000);
    memset((void *)0x08000000,0xFF,0x20000);
    assert(VirtualAlloc((void *)0x1FFF0000,0x10000,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE)==(void *)0x1FFF0000);
    *(UINT16 *)0x1FFFF7E0=128;
}
/* PRODUCTION_SOURCE */
/* Actual parameter tables, validators and boot initializer. */
/* PARAMETER_HEADERS */
struct PRT_E2ROM_PARAS PRT_E2ROMParas;
struct OTHER_ELEMENT OtherElement;
UINT16 g_u16CalibCoefK[KB_NUM];
INT16 g_i16CalibCoefB[KB_NUM];
UINT8 SeriesNum;
UINT32 g_u32CS_Res_AFE;
AFE_Parameters_RS485_Typedef AFE_Parameters_RS485_Struction=AFE_PARAMETERS_RS485_STRUCTION_DEFAULT;
#define E2P_PARA_NUM_PROTECT FLASH_STORAGE_RW_PARAM_PROTECT_WORD_COUNT
#define E2P_PARA_NUM_OTHER_ELEMENT1 FLASH_STORAGE_RW_PARAM_OTHER_WORD_COUNT
#define E2P_PARA_NUM_RESERVED_RW_PARAM FLASH_STORAGE_RW_PARAM_RESERVED_WORD_COUNT
#define ERROR_REMOVE_EEPROM_COM 0
static int reset_on_version=1;
static char firmware_version[32]="storage-test-v1";
#undef BMS_SOFTWARE_VERDION_DEFAULT
#define BMS_SOFTWARE_VERDION_DEFAULT firmware_version
#undef PROJECT_CFG_RESET_PARAMS_ON_NEW_VERSION
#define PROJECT_CFG_RESET_PARAMS_ON_NEW_VERSION reset_on_version
/* CONFIG_INIT_SOURCE */
/* LOG_SOURCE */
static unsigned char saved[16384];
static void save_flash(void) { memcpy(saved,(void *)FLASH_PARAMETER_RESET_BEGIN,sizeof(saved)); }
static void restore_flash(void) {
    memcpy((void *)FLASH_PARAMETER_RESET_BEGIN,saved,sizeof(saved)); mutations=0; cut_after=-1;
    memset(&s_flash,0,sizeof(s_flash));
}
static void test_log(void) {
    UINT8 expected[200]={0},before[200],actual[200],next[200];
    UINT32 seconds;
    int i,j,writes,cut,ok;
    memset((void *)FLASH_LOG_A,0xFF,2048); memset(&s_log_record,0,sizeof(s_log_record));
    for(i=0;i<402;++i) {
        seconds=61; mutations=0;
        assert(LogEvent_EEPROM(VCELL_OVP+(i % 10),&seconds));
        memmove(expected+2,expected,198); expected[0]=(UINT8)(VCELL_OVP+i%10); expected[1]=1;
        if(i!=0 && (i%134)!=0) assert(mutations==3); /* only 6 bytes per event */
        memset(&s_log_record,0,sizeof(s_log_record)); ReadEEPROM_EventRecord_Parameters();
        Sci_ACK_0x03_ReadRegs_EventRecord(actual); assert(!memcmp(expected,actual,200));
    }
    save_flash(); memcpy(before,expected,200);
    seconds=61; mutations=0; assert(LogEvent_EEPROM(VCELL_UVP,&seconds)); writes=mutations;
    Sci_ACK_0x03_ReadRegs_EventRecord(next);
    for(cut=0;cut<=writes;++cut) {
        restore_flash(); memset(&s_log_record,0,sizeof(s_log_record)); ReadEEPROM_EventRecord_Parameters();
        seconds=61; cut_after=cut; LogEvent_EEPROM(VCELL_UVP,&seconds); cut_after=-1;
        memset(&s_log_record,0,sizeof(s_log_record)); ReadEEPROM_EventRecord_Parameters();
        Sci_ACK_0x03_ReadRegs_EventRecord(actual);
        assert(!memcmp(actual,before,200) || !memcmp(actual,next,200));
    }
    restore_flash(); memset(&s_log_record,0,sizeof(s_log_record)); ReadEEPROM_EventRecord_Parameters();
    /* Repeated incomplete slots plus rotation must retain the full last 100. */
    seconds=61; assert(LogEvent_EEPROM(CHG_OCP,&seconds));
    Sci_ACK_0x03_ReadRegs_EventRecord(before);
    for(j=0;j<200;++j) {
        seconds=61; mutations=0; cut_after=1;
        assert(!LogEvent_EEPROM(CHG_OTP,&seconds)); cut_after=-1;
        memset(&s_log_record,0,sizeof(s_log_record)); ReadEEPROM_EventRecord_Parameters();
        Sci_ACK_0x03_ReadRegs_EventRecord(actual); assert(!memcmp(actual,before,200));
    }
    seconds=61; assert(LogEvent_EEPROM(DSG_OTP,&seconds));
    save_flash(); Sci_ACK_0x03_ReadRegs_EventRecord(before);
    mutations=0; assert(EEPROM_ResetData_EventRecord_ToDefault()); writes=mutations;
    for(cut=0;cut<=writes;++cut) {
        restore_flash(); memset(&s_log_record,0,sizeof(s_log_record)); ReadEEPROM_EventRecord_Parameters();
        cut_after=cut; ok=EEPROM_ResetData_EventRecord_ToDefault(); cut_after=-1;
        Sci_ACK_0x03_ReadRegs_EventRecord(actual); if(!ok) assert(!memcmp(actual,before,200));
        memset(&s_log_record,0,sizeof(s_log_record)); ReadEEPROM_EventRecord_Parameters();
        Sci_ACK_0x03_ReadRegs_EventRecord(actual);
        memset(next,0,200); assert(!memcmp(actual,before,200) || !memcmp(actual,next,200));
    }
    /* A failed event is retried, rather than swallowed by its RAM latch. */
    restore_flash(); memset(&s_log_record,0,sizeof(s_log_record)); ReadEEPROM_EventRecord_Parameters();
    seconds=100; mutations=0; cut_after=0; LogEvent_Record(1,CHG_OTP,&seconds);
    assert(!s_log_record.eventLatch[CHG_OTP] && seconds==100);
    cut_after=-1; LogEvent_Record(1,CHG_OTP,&seconds);
    assert(s_log_record.eventLatch[CHG_OTP] && seconds==0);
    puts("PASS log: 6-byte appends, 402-event wrap/reboot, all rotation cut points, 200 torn writes, atomic clear, retry, unchanged 200-byte host view");
}
static void test_statistics(void) {
    UINT32 counts[11],old_count,new_count;
    int i,writes,cut;
    UINT32 page,entry_address;
    memset((void *)FLASH_STATS_A,0xFF,2048);
    mutations=0;
    StorageFlash_InitStatistics(RCC_CSR_PORRSTF,BOOT_FLAG_RESET_VALUE);
    writes=mutations;
    for(cut=0;cut<=writes;++cut) {
        memset((void *)FLASH_STATS_A,0xFF,2048); mutations=0; cut_after=cut;
        StorageFlash_InitStatistics(RCC_CSR_PORRSTF,BOOT_FLAG_RESET_VALUE); cut_after=-1;
        StorageFlash_InitStatistics(RCC_CSR_SFTRSTF,BOOT_FLAG_RESET_VALUE);
        assert(StorageFlash_ReadStatistics(counts) && (counts[10]==1 || counts[10]==2) && !s_statistics.dirty);
    }
    memset((void *)FLASH_STATS_A,0xFF,2048);
    StorageFlash_InitStatistics(RCC_CSR_PORRSTF,BOOT_FLAG_RESET_VALUE);
    assert(StorageFlash_ReadStatistics(counts) && counts[10]==1);
    StorageFlash_UpdateFaultStatistics(0x3FF);
    StorageFlash_UpdateFaultStatistics(0x3FF);
    assert(StorageFlash_ReadStatistics(counts));
    for(i=0;i<10;++i) assert(counts[i]==1);
    StorageFlash_InitStatistics(RCC_CSR_SFTRSTF,BOOT_FLAG_RESET_VALUE);
    StorageFlash_UpdateFaultStatistics(0x3FF);
    assert(StorageFlash_ReadStatistics(counts) && counts[0]==1 && counts[10]==2);
    StorageFlash_InitStatistics(RCC_CSR_SFTRSTF,FLASH_DEEP_SLEEP_VALUE);
    assert(StorageFlash_ReadStatistics(counts) && counts[10]==2);
    StorageFlash_InitStatistics(RCC_CSR_SFTRSTF|RCC_CSR_IWDGRSTF,FLASH_DEEP_SLEEP_VALUE);
    assert(StorageFlash_ReadStatistics(counts) && counts[10]==3);
    StorageFlash_InitStatistics(RCC_CSR_WWDGRSTF,FLASH_NORMAL_SLEEP_VALUE);
    assert(StorageFlash_ReadStatistics(counts) && counts[10]==4);
    assert(StorageFlash_ClearFaultStatistics());
    StorageFlash_UpdateFaultStatistics(0x3FF);
    assert(StorageFlash_ReadStatistics(counts) && counts[0]==0 && counts[10]==4);
    StorageFlash_UpdateFaultStatistics(0); StorageFlash_UpdateFaultStatistics(0x3FF);
    assert(StorageFlash_ReadStatistics(counts) && counts[9]==1);
    /* More than two page capacities: replay only the newest checkpoint/page. */
    for(i=0;i<300;++i) { StorageFlash_UpdateFaultStatistics(0); StorageFlash_UpdateFaultStatistics(1); }
    StorageFlash_InitStatistics(RCC_CSR_SFTRSTF,FLASH_NORMAL_SLEEP_VALUE);
    assert(StorageFlash_ReadStatistics(counts) && counts[0]==301 && counts[9]==1 && counts[10]==4);
    save_flash(); mutations=0;
    StorageFlash_InitStatistics(RCC_CSR_SFTRSTF,BOOT_FLAG_RESET_VALUE); writes=mutations;
    for(cut=0;cut<=writes;++cut) {
        restore_flash(); StorageFlash_InitStatistics(RCC_CSR_SFTRSTF,FLASH_DEEP_SLEEP_VALUE);
        cut_after=cut; StorageFlash_InitStatistics(RCC_CSR_SFTRSTF,BOOT_FLAG_RESET_VALUE); cut_after=-1;
        StorageFlash_InitStatistics(RCC_CSR_SFTRSTF,FLASH_DEEP_SLEEP_VALUE);
        assert(StorageFlash_ReadStatistics(counts) && counts[0]==301 && (counts[10]==4 || counts[10]==5));
    }
    restore_flash(); StorageFlash_InitStatistics(RCC_CSR_SFTRSTF,FLASH_DEEP_SLEEP_VALUE);
    /* Fill this page, then cut every erase/header/checkpoint half-word at rotation. */
    while(1) {
        page=Storage_StatsPage();
        entry_address=page+sizeof(STORAGE_RECORD)+sizeof(STORAGE_STATS_DATA);
        while(entry_address+sizeof(STORAGE_STATS_ENTRY)<=page+1024 &&
              !StorageFlash_IsAreaBlank(entry_address,sizeof(STORAGE_STATS_ENTRY))) entry_address+=sizeof(STORAGE_STATS_ENTRY);
        if(entry_address+sizeof(STORAGE_STATS_ENTRY)>page+1024) break;
        StorageFlash_UpdateFaultStatistics(s_statistics.data.active_faults ? 0 : 1);
    }
    assert(StorageFlash_ReadStatistics(counts)); old_count=counts[0];
    save_flash(); mutations=0;
    StorageFlash_UpdateFaultStatistics(s_statistics.data.active_faults ? 0 : 1);
    assert(StorageFlash_ReadStatistics(counts)); new_count=counts[0]; writes=mutations;
    for(cut=0;cut<=writes;++cut) {
        restore_flash(); StorageFlash_InitStatistics(RCC_CSR_SFTRSTF,FLASH_DEEP_SLEEP_VALUE);
        cut_after=cut; StorageFlash_UpdateFaultStatistics(s_statistics.data.active_faults ? 0 : 1); cut_after=-1;
        StorageFlash_InitStatistics(RCC_CSR_SFTRSTF,FLASH_DEEP_SLEEP_VALUE);
        assert(StorageFlash_ReadStatistics(counts) && (counts[0]==old_count || counts[0]==new_count) && counts[10]==4);
    }
    /* Delayed retry persists the RAM checkpoint, never re-adds uncertain deltas. */
    restore_flash(); StorageFlash_InitStatistics(RCC_CSR_SFTRSTF,FLASH_DEEP_SLEEP_VALUE);
    tick=UINT32_MAX-10; mutations=0; cut_after=0;
    StorageFlash_UpdateFaultStatistics(0); StorageFlash_UpdateFaultStatistics(1);
    StorageFlash_UpdateFaultStatistics(0); StorageFlash_UpdateFaultStatistics(1);
    assert(s_statistics.dirty); assert(StorageFlash_ReadStatistics(counts)); new_count=counts[0];
    cut_after=-1; mutations=0; tick+=99; StorageFlash_StatisticsTask(); assert(!mutations);
    ++tick; StorageFlash_StatisticsTask(); assert(!s_statistics.dirty);
    StorageFlash_InitStatistics(RCC_CSR_SFTRSTF,FLASH_DEEP_SLEEP_VALUE);
    assert(StorageFlash_ReadStatistics(counts) && counts[0]==new_count);
    save_flash(); mutations=0; assert(StorageFlash_ClearFaultStatistics()); writes=mutations;
    for(cut=0;cut<=writes;++cut) {
        restore_flash(); StorageFlash_InitStatistics(RCC_CSR_SFTRSTF,FLASH_DEEP_SLEEP_VALUE);
        cut_after=cut; StorageFlash_ClearFaultStatistics(); cut_after=-1;
        StorageFlash_InitStatistics(RCC_CSR_SFTRSTF,FLASH_DEEP_SLEEP_VALUE);
        assert(StorageFlash_ReadStatistics(counts) && (counts[0]==new_count || counts[0]==0) && counts[10]==4);
    }
    restore_flash(); StorageFlash_InitStatistics(RCC_CSR_SFTRSTF,FLASH_DEEP_SLEEP_VALUE);
    s_statistics.data.count[0]=0xFFFFFFFFUL; s_statistics.data.count[1]=70000;
    s_statistics.dirty=1; StorageFlash_FlushStatistics();
    StorageFlash_UpdateFaultStatistics(0); StorageFlash_UpdateFaultStatistics(3);
    StorageFlash_InitStatistics(RCC_CSR_SFTRSTF,FLASH_DEEP_SLEEP_VALUE);
    assert(StorageFlash_ReadStatistics(counts) && counts[0]==0xFFFFFFFFUL && counts[1]==70001);
    error_after_commit=1; assert(StorageFlash_ClearFaultStatistics()); error_after_commit=0;
    StorageFlash_UpdateFaultStatistics(0); StorageFlash_UpdateFaultStatistics(2);
    assert(StorageFlash_ClearLog());
    reset_on_version=1; strcpy(firmware_version,"storage-test-v4");
    assert(InitE2PROM());
    StorageFlash_InitStatistics(RCC_CSR_SFTRSTF,FLASH_DEEP_SLEEP_VALUE);
    assert(StorageFlash_ReadStatistics(counts) && counts[1]==1 && counts[10]==4);
    /* Committed corruption is an error, not a fresh zero lifetime counter. */
    page=Storage_StatsPage();
    *(UINT16 *)(page+sizeof(STORAGE_RECORD)+sizeof(STORAGE_STATS_DATA)+4)^=1;
    StorageFlash_InitStatistics(RCC_CSR_SFTRSTF,BOOT_FLAG_RESET_VALUE);
    assert(!StorageFlash_ReadStatistics(counts) && !StorageFlash_ClearFaultStatistics());
    puts("PASS statistics: 32-bit lifetime, 11 fields, deduplicated edges/reboots, STOP versus watchdog, rotation cuts, clear cuts, bounded retry/wrap, saturation, reset/log isolation, corruption rejection");
}
int main(void) {
    STORAGE_FLASH_CONFIG_DATA old_value,new_value,loaded;
    STORAGE_FLASH_SOC_DATA soc,loaded_soc;
    STORAGE_FLASH_FACTORY_AGING_DATA aging={1234,FLASH_FACTORY_AGING_STATE_RUNNING,12},loaded_aging;
    int writes,cut,i;
    UINT16 previous_soc;
    host_storage_init();
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
    while ((Storage_StateLatest() & (FLASH_STORAGE_PAGE_SIZE-1U))+2U*STORAGE_STATE_SPAN<=FLASH_STORAGE_PAGE_SIZE) {
        ++soc.u16SocNow; assert(StorageFlash_SaveSocData(&soc));
    }
    previous_soc=soc.u16SocNow;
    save_flash(); soc.u16SocNow=90;
    mutations=0; assert(StorageFlash_SaveSocData(&soc)); writes=mutations;
    for(cut=0;cut<=writes;++cut) {
        restore_flash(); cut_after=cut; StorageFlash_SaveSocData(&soc); cut_after=-1;
        assert(StorageFlash_LoadSocData(&loaded_soc));
        assert(loaded_soc.u16SocNow==previous_soc || loaded_soc.u16SocNow==90);
        assert(StorageFlash_LoadFactoryAgingData(&loaded_aging) && !memcmp(&aging,&loaded_aging,sizeof(aging)));
    }
    puts("PASS storage: SOC/aging merge, state append and rotation, power cuts retain complete state");
    memset((void *)FLASH_PARAMETER_RESET_BEGIN,0xFF,16384);
    memset((void *)FLASH_STATS_A,0x5A,2048);
    assert(InitE2PROM());
    PRT_E2ROMParas.u16VcellOvp_First=4001; assert(EEPROM_SaveRWParametersToFlash());
    mutations=0; assert(InitE2PROM() && PRT_E2ROMParas.u16VcellOvp_First==4001 && !mutations);
    assert(StorageFlash_SaveSocData(&soc)); assert(StorageFlash_AppendLog(1,171));
    save_flash(); strcpy(firmware_version,"storage-test-v2");
    mutations=0; assert(InitE2PROM()); writes=mutations;
    for(cut=0;cut<=writes;++cut) {
        restore_flash(); cut_after=cut; InitE2PROM(); cut_after=-1;
        assert(InitE2PROM() && PRT_E2ROMParas.u16VcellOvp_First==COV_1);
        assert(!StorageFlash_LoadSocData(&loaded_soc));
        assert(!StorageFlash_LoadFactoryAgingData(&loaded_aging));
        for(i=0;i<2048;++i) assert(((UINT8 *)FLASH_STATS_A)[i]==0x5A);
        mutations=0; assert(InitE2PROM() && !mutations);
    }
    strcpy(firmware_version,"storage-test-v3"); reset_on_version=0;
    PRT_E2ROMParas.u16VcellOvp_First=4002; assert(EEPROM_SaveRWParametersToFlash());
    mutations=0; assert(InitE2PROM() && PRT_E2ROMParas.u16VcellOvp_First==4002 && !mutations);
    assert(StorageFlash_LoadConfig(&loaded)); loaded.rw.reserved[0]=0xFFFF;
    assert(!EEPROM_RWParamDataIsValid(&loaded.rw));
    printf("PASS storage: real boot init, once per version, disabled switch, %d interrupted resets, statistics untouched\n",writes+1);
    test_log();
    test_statistics();
    return 0;
}
