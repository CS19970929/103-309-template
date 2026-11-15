#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <assert.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <stddef.h>
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
static void System_ERROR_UserCallback(int error) { if(error==ERROR_EEPROM_STORE) ++errors; }
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
static UINT16 FlashReadOneHalfWord(UINT32 address) { return *(UINT16 *)(uintptr_t)address; }
static void host_storage_init(void) {
    assert(VirtualAlloc((void *)0x08000000,0x20000,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE)==(void *)0x08000000);
    memset((void *)0x08000000,0xFF,0x20000);
    assert(VirtualAlloc((void *)0x1FFF0000,0x10000,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE)==(void *)0x1FFF0000);
    *(UINT16 *)0x1FFFF7E0=128;
}
/* STORAGE_HARDWARE_STUBS_END */
/* Actual parameter tables, validators and boot initializer. */
/* PARAMETER_HEADERS */
#undef PROJECT_CFG_LOG_RECORD_REPEAT_MIN_INTERVAL_SEC
#define PROJECT_CFG_LOG_RECORD_REPEAT_MIN_INTERVAL_SEC 0
struct PRT_E2ROM_PARAS PRT_E2ROMParas;
struct OTHER_ELEMENT OtherElement;
UINT16 g_u16CalibCoefK[KB_NUM];
INT16 g_i16CalibCoefB[KB_NUM];
UINT8 SeriesNum;
UINT32 g_u32CS_Res_AFE;
AFE_Parameters_RS485_Typedef AFE_Parameters_RS485_Struction=AFE_PARAMETERS_RS485_STRUCTION_DEFAULT;
#define ERROR_REMOVE_EEPROM_COM 0
static UINT16 update_generation,update_mask;
static char firmware_version[32]="storage-test-v1";
#undef BMS_SOFTWARE_VERDION_DEFAULT
#define BMS_SOFTWARE_VERDION_DEFAULT firmware_version
#undef PROJECT_CFG_STORAGE_UPDATE_GENERATION
#define PROJECT_CFG_STORAGE_UPDATE_GENERATION update_generation
#undef PROJECT_CFG_STORAGE_UPDATE_MASK
#define PROJECT_CFG_STORAGE_UPDATE_MASK update_mask

/* PRODUCTION_SOURCE */

/* LOG_SOURCE */
static unsigned char saved[16384];
static void save_flash(void) { memcpy(saved,(void *)FLASH_PARAMETER_RESET_BEGIN,sizeof(saved)); }
static void restore_flash(void) {
    memcpy((void *)FLASH_PARAMETER_RESET_BEGIN,saved,sizeof(saved)); mutations=0; cut_after=-1;
    
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
static void test_module_updates(void) {
    STORAGE_FLASH_CONFIG_DATA defaults,custom,actual;
    STORAGE_FLASH_SOC_DATA soc={0},loaded_soc;
    UINT8 point,records[100][2];
    unsigned mask;
    int group,word,writes,cut,cut_checks=0;
    update_generation=update_mask=0;
    memset((void *)FLASH_PARAMETER_RESET_BEGIN,0xFF,16384);
    memset((void *)FLASH_STATS_A,0x5A,2048);
    assert(StorageFlash_InitParameters() && Storage_LoadConfig(&defaults));
    assert(sizeof(defaults)==372);
    custom=defaults;
    ++custom.afe[0]; custom.rw.protect[0]=4001;
    custom.rw.other[12]=(custom.rw.other[12]==1) ? 2 : 1;
    custom.rw.other[17]=11;custom.rw.other[24]+=10;custom.rw.other[25]=8;
    ++custom.rw.other[26];++custom.rw.other[27];
    ++custom.rw.reserved[2];++custom.rw.heat_cool[0];
    /* A current-format record with old per-module tags migrates without resetting. */
    for(group=STORAGE_PARAM_VERSION_FIRST;group<STORAGE_BAUD_MODE_INDEX;++group)
        custom.rw.reserved[group]=8U;
    assert(Storage_SaveConfig(&custom));
    assert(StorageFlash_SetBaudMode(3U) && StorageFlash_WriteBatteryNumber((const UINT8 *)"BAT00000001"));
    assert(Storage_LoadConfig(&custom));
    soc.u16FormatVersion=FLASH_STORAGE_SOC_DATA_VERSION;soc.u16SocNow=37;soc.u32CycleTimes=12345;
    assert(StorageFlash_SaveSocData(&soc));
    assert(StorageFlash_AppendLog(7,8));save_flash();
    for(mask=0U;mask<(1U<<STORAGE_PARAM_COUNT);++mask) {
        restore_flash();
        update_generation=mask ? 1U : 0U;
        update_mask=(UINT16)mask;
        assert(StorageFlash_InitParameters() && Storage_LoadConfig(&actual));
        for(word=0;word<FLASH_STORAGE_AFE_WORD_COUNT;++word)
            assert(actual.afe[word]==((mask&(1U<<STORAGE_PARAM_AFE)) ? defaults.afe[word] : custom.afe[word]));
        for(word=0;word<E2P_PARA_NUM_PROTECT;++word)
            assert(actual.rw.protect[word]==((mask&(1U<<STORAGE_PARAM_PROTECT)) ? defaults.rw.protect[word] : custom.rw.protect[word]));
        for(word=0;word<32;++word) {
            group=(word==12 || (word>=24 && word<=27)) ? STORAGE_PARAM_SOC : STORAGE_PARAM_OTHER;
            assert(actual.rw.other[word]==((mask&(1U<<group)) ? defaults.rw.other[word] : custom.rw.other[word]));
        }
        for(word=0;word<6;++word)
            assert(actual.rw.reserved[word]==((mask&(1U<<STORAGE_PARAM_CALIB)) ? defaults.rw.reserved[word] : custom.rw.reserved[word]));
        for(word=0;word<24;++word)
            assert(actual.rw.heat_cool[word]==((mask&(1U<<STORAGE_PARAM_HEAT_COOL)) ? defaults.rw.heat_cool[word] : custom.rw.heat_cool[word]));
        assert(actual.rw.reserved[STORAGE_UPDATE_GENERATION_INDEX]==(mask ? 1U : 0U));
        assert(actual.rw.reserved[STORAGE_UPDATE_TAG_INDEX]==STORAGE_UPDATE_TAG);
        for(word=STORAGE_BAUD_MODE_INDEX;word<FLASH_STORAGE_RW_PARAM_RESERVED_WORD_COUNT;++word)
            assert(actual.rw.reserved[word]==custom.rw.reserved[word]);
        assert(!actual.initializing && !memcmp(actual.reset_version,custom.reset_version,32));
        assert(StorageFlash_LoadSocData(&loaded_soc)==!(mask&(1U<<STORAGE_PARAM_SOC_STATE)));
        if(!(mask&(1U<<STORAGE_PARAM_SOC_STATE))) assert(!memcmp(&soc,&loaded_soc,sizeof(soc)));
        assert(StorageFlash_LoadLogData(&point,records));
        if(mask&(1U<<STORAGE_PARAM_LOG)) assert(point==0 && records[0][0]==0);
        else assert(point==1 && records[0][0]==7 && records[0][1]==8);
        for(word=0;word<2048;++word) assert(((UINT8 *)FLASH_STATS_A)[word]==0x5A);
        mutations=0;assert(StorageFlash_InitParameters() && !mutations);
    }
    restore_flash();update_generation=update_mask=0;
    assert(StorageFlash_InitParameters());
    PRT_E2ROMParas.u16VcellOvp_First=4002;assert(StorageFlash_SaveParameters(0U));
    strcpy(firmware_version,"module-next-firmware");
    mutations=0;assert(StorageFlash_InitParameters() && !mutations &&
        PRT_E2ROMParas.u16VcellOvp_First==4002);
    update_generation=2U;update_mask=1U<<STORAGE_PARAM_PROTECT;
    assert(StorageFlash_InitParameters() && PRT_E2ROMParas.u16VcellOvp_First==defaults.rw.protect[0]);
    PRT_E2ROMParas.u16VcellOvp_First=4002;assert(StorageFlash_SaveParameters(0U));
    mutations=0;assert(StorageFlash_InitParameters() && !mutations &&
        PRT_E2ROMParas.u16VcellOvp_First==4002);
    for(group=0;group<=STORAGE_PARAM_COUNT;++group) {
        restore_flash();update_generation=3U;
        update_mask=(UINT16)(group==STORAGE_PARAM_COUNT ?
            ((1U<<STORAGE_PARAM_SOC_STATE)|(1U<<STORAGE_PARAM_LOG)) :
            (1U<<group));
        mutations=0;assert(StorageFlash_InitParameters());writes=mutations;
        for(cut=0;cut<=writes;++cut) {
            restore_flash();cut_after=cut;(void)StorageFlash_InitParameters();cut_after=-1;
            assert(StorageFlash_InitParameters() && Storage_LoadConfig(&actual) && !actual.initializing);
            assert(actual.rw.reserved[STORAGE_UPDATE_GENERATION_INDEX]==3U);
            assert(StorageFlash_LoadLogData(&point,records));
            assert(point==((update_mask&(1U<<STORAGE_PARAM_LOG))?0:1));
            assert(StorageFlash_LoadSocData(&loaded_soc)==!(update_mask&(1U<<STORAGE_PARAM_SOC_STATE)));
            for(word=0;word<2048;++word) assert(((UINT8 *)FLASH_STATS_A)[word]==0x5A);
            mutations=0;assert(StorageFlash_InitParameters() && !mutations);
            ++cut_checks;
        }
    }
    /* New factory records start at the compiled update generation. */
    memset((void *)FLASH_PARAMETER_RESET_BEGIN,0xFF,14336);
    update_generation=65534U;update_mask=1U<<STORAGE_PARAM_PROTECT;
    assert(StorageFlash_InitParameters() && Storage_LoadConfig(&actual));
    assert(actual.rw.reserved[STORAGE_UPDATE_GENERATION_INDEX]==65534U);
    mutations=0;assert(StorageFlash_InitParameters() && !mutations);
    /* Explicit all-group update preserves lifetime counters. */
    restore_flash();update_generation=4U;update_mask=(1U<<STORAGE_PARAM_COUNT)-1U;
    assert(StorageFlash_InitParameters() && Storage_LoadConfig(&actual));
    assert(!memcmp(actual.afe,defaults.afe,sizeof(actual.afe)));
    assert(!memcmp(actual.rw.other,defaults.rw.other,sizeof(actual.rw.other)));
    assert(!StorageFlash_LoadSocData(&loaded_soc));
    assert(StorageFlash_LoadLogData(&point,records) && !point);
    for(word=0;word<2048;++word) assert(((UINT8 *)FLASH_STATS_A)[word]==0x5A);
    printf("PASS module update mask: all selections and %d power-cut cases; no version-string reset, statistics preserved\n",cut_checks);
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
    StorageFlash_InitStatistics(RCC_CSR_SFTRSTF,FLASH_UNDERVOLTAGE_SLEEP_VALUE);
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
    update_generation=11U;update_mask=1U<<STORAGE_PARAM_LOG;
    assert(StorageFlash_InitParameters());
    assert(OtherElement.u16Sleep_TimeNormal==10080);
    StorageFlash_InitStatistics(RCC_CSR_SFTRSTF,FLASH_DEEP_SLEEP_VALUE);
    assert(StorageFlash_ReadStatistics(counts) && counts[1]==1 && counts[10]==4);
    /* Committed corruption is an error, not a fresh zero lifetime counter. */
    page=Storage_StatsPage();
    *(UINT16 *)(page+sizeof(STORAGE_RECORD)+sizeof(STORAGE_STATS_DATA)+4)^=1;
    StorageFlash_InitStatistics(RCC_CSR_SFTRSTF,BOOT_FLAG_RESET_VALUE);
    assert(!StorageFlash_ReadStatistics(counts) && !StorageFlash_ClearFaultStatistics());
    puts("PASS statistics: 32-bit lifetime, 11 fields, deduplicated edges/reboots, STOP versus watchdog, rotation cuts, clear cuts, bounded retry/wrap, saturation, reset/log isolation, corruption rejection");
}
static void test_communication_storage(void)
{
    STORAGE_FLASH_CONFIG_DATA old_value,new_value,loaded;
    UINT8 number[11];
    int operation,cut,writes,result,total=0;
    assert(Storage_LoadConfig(&old_value));
    old_value.initializing=0;
    for(cut=15;cut<24;++cut) old_value.rw.reserved[cut]=0xFFFF;
    assert(Storage_SaveConfig(&old_value));
    assert(StorageFlash_GetBaudMode()==1);
    assert(StorageFlash_ReadBatteryNumber(number) && !memcmp(number,"00000000000",11));
    for(operation=0;operation<4;++operation) {
        assert(Storage_LoadConfig(&old_value)); save_flash(); mutations=0;
        if(operation & 1) result=StorageFlash_WriteBatteryNumber((const UINT8 *)(operation==1 ? "BAT00000001" : "BAT00000002"));
        else result=StorageFlash_SetBaudMode(operation==0 ? 2 : 3);
        assert(result); writes=mutations;
        assert(Storage_LoadConfig(&new_value));
        for(cut=0;cut<=writes;++cut) {
            restore_flash(); cut_after=cut;
            if(operation & 1) result=StorageFlash_WriteBatteryNumber((const UINT8 *)(operation==1 ? "BAT00000001" : "BAT00000002"));
            else result=StorageFlash_SetBaudMode(operation==0 ? 2 : 3);
            assert(Storage_LoadConfig(&loaded));
            assert(!memcmp(&loaded,result ? &new_value : &old_value,sizeof(loaded)));
            assert(!memcmp((void *)FLASH_STATE_A,saved+(FLASH_STATE_A-FLASH_PARAMETER_RESET_BEGIN),FLASH_STORAGE_END-FLASH_STATE_A));
            ++total;
        }
        restore_flash(); assert(Storage_SaveConfig(&new_value));
    }
    mutations=0;
    assert(StorageFlash_SetBaudMode(3));
    assert(StorageFlash_WriteBatteryNumber((const UINT8 *)"BAT00000002"));
    assert(!mutations);
    assert(!StorageFlash_SetBaudMode(0) && !StorageFlash_SetBaudMode(4));
    assert(!StorageFlash_WriteBatteryNumber(0) && !StorageFlash_ReadBatteryNumber(0));
    assert(!StorageFlash_WriteBatteryNumber((const UINT8 *)"BAD\x80" "0000000") && !mutations);
    /* Normal parameter/calibration saves preserve both new private fields. */
    assert(StorageFlash_SaveParameters(0U));
    assert(StorageFlash_GetBaudMode()==3);
    assert(StorageFlash_ReadBatteryNumber(number) && !memcmp(number,"BAT00000002",11));
    update_generation=update_mask=0;
    assert(StorageFlash_InitParameters());
    assert(StorageFlash_GetBaudMode()==3);
    assert(StorageFlash_ReadBatteryNumber(number) && !memcmp(number,"BAT00000002",11));
    for(operation=0;operation<11;++operation) {
        memset(number,'A',sizeof(number)); number[operation]=0x1F; mutations=0;
        assert(!StorageFlash_WriteBatteryNumber(number) && !mutations);
        number[operation]=0x7F;
        assert(!StorageFlash_WriteBatteryNumber(number) && !mutations);
    }
    assert(StorageFlash_WriteBatteryNumber((const UINT8 *)" ~09AZaz-_."));
    assert(StorageFlash_ReadBatteryNumber(number) && !memcmp(number," ~09AZaz-_.",11));
    assert(Storage_LoadConfig(&old_value));
    assert(old_value.rw.reserved[STORAGE_BATTERY_NUMBER_INDEX]==0x207EU);
    assert(old_value.rw.reserved[STORAGE_BATTERY_NUMBER_INDEX+5U]==0x2EFFU);
    loaded=old_value; loaded.rw.reserved[STORAGE_BATTERY_NUMBER_TAG_INDEX]=0x1234U;
    assert(Storage_SaveConfig(&loaded)); mutations=0;
    assert(!StorageFlash_ReadBatteryNumber(number) && !mutations);
    loaded=old_value; loaded.rw.reserved[STORAGE_BATTERY_NUMBER_INDEX]=0x1F7EU;
    assert(Storage_SaveConfig(&loaded)); mutations=0;
    assert(!StorageFlash_ReadBatteryNumber(number) && !mutations);
    for(operation=0;operation<=4;operation+=4) {
        loaded=old_value; loaded.rw.reserved[STORAGE_BAUD_MODE_INDEX]=(UINT16)operation;
        assert(Storage_SaveConfig(&loaded)); mutations=0;
        assert(StorageFlash_GetBaudMode()==1U && !mutations);
    }
    assert(Storage_SaveConfig(&old_value));
    assert(Storage_LoadConfig(&loaded)); loaded.initializing=STORAGE_PARAM_RESET_LOG;
    assert(Storage_SaveConfig(&loaded));
    assert(!StorageFlash_SetBaudMode(1) && !StorageFlash_WriteBatteryNumber((const UINT8 *)"BAT00000003"));
    printf("PASS communication storage: %d cuts, first/update writes, complete old/new config, state/log/statistics isolation, defaults, no-op, boot and RW-save preservation\n",total);
    puts("PASS communication fields: ASCII endpoints, all 11 invalid positions, high-byte-first packing, padding, invalid tag/data rejection, invalid baud fallback");
}

static void test_parameter_application(void)
{
    STORAGE_FLASH_CONFIG_DATA defaults;
    struct PRT_E2ROM_PARAS protect_before;
    struct OTHER_ELEMENT other_before;
    struct HEAT_COOL_ELEMENT heat_before;
    UINT16 k_before[KB_NUM]; INT16 b_before[KB_NUM];
    AFE_Parameters_RS485_Typedef afe_before;
    UINT8 series_before; UINT32 resistance_before;
    assert(StorageFlash_InitParameters());
    PRT_E2ROMParas.u16VcellOvp_First=4003;
    assert(StorageFlash_SaveParameters(0U));
    protect_before=PRT_E2ROMParas; other_before=OtherElement; heat_before=Heat_Cool_Element;
    memcpy(k_before,g_u16CalibCoefK,sizeof(k_before));
    memcpy(b_before,g_i16CalibCoefB,sizeof(b_before));
    afe_before=AFE_Parameters_RS485_Struction;
    series_before=SeriesNum; resistance_before=g_u32CS_Res_AFE;
    mutations=0;
    Storage_BuildDefaultConfig(&defaults);
    assert(Storage_ConfigValid(&defaults) && !mutations);
    assert(!memcmp(&protect_before,&PRT_E2ROMParas,sizeof(protect_before)));
    assert(!memcmp(&other_before,&OtherElement,sizeof(other_before)));
    assert(!memcmp(&heat_before,&Heat_Cool_Element,sizeof(heat_before)));
    assert(!memcmp(k_before,g_u16CalibCoefK,sizeof(k_before)));
    assert(!memcmp(b_before,g_i16CalibCoefB,sizeof(b_before)));
    assert(!memcmp(&afe_before,&AFE_Parameters_RS485_Struction,sizeof(afe_before)));
    assert(SeriesNum==series_before && g_u32CS_Res_AFE==resistance_before);
    update_generation=12U;update_mask=1U<<STORAGE_PARAM_PROTECT;
    mutations=0; cut_after=0;
    assert(!StorageFlash_InitParameters()); cut_after=-1;
    assert(!memcmp(&protect_before,&PRT_E2ROMParas,sizeof(protect_before)));
    assert(!memcmp(&other_before,&OtherElement,sizeof(other_before)));
    assert(!memcmp(&heat_before,&Heat_Cool_Element,sizeof(heat_before)));
    assert(!memcmp(k_before,g_u16CalibCoefK,sizeof(k_before)));
    assert(!memcmp(b_before,g_i16CalibCoefB,sizeof(b_before)));
    assert(!memcmp(&afe_before,&AFE_Parameters_RS485_Struction,sizeof(afe_before)));
    assert(SeriesNum==series_before && g_u32CS_Res_AFE==resistance_before);
    assert(StorageFlash_InitParameters() && PRT_E2ROMParas.u16VcellOvp_First==COV_1);
    mutations=0; assert(StorageFlash_InitParameters() && !mutations);
    puts("PASS parameter application: pure default snapshot, failed boot preserves all live parameters, successful commit applies once");
}

/* SOC_PERSISTENCE_TESTS */

int main(void) {
    STORAGE_FLASH_CONFIG_DATA old_value,new_value,loaded;
    STORAGE_FLASH_SOC_DATA soc,loaded_soc;
    int writes,cut,i;
    UINT16 previous_soc;
    host_storage_init();
    Storage_BuildDefaultConfig(&old_value);
    new_value=old_value;
    ++new_value.rw.protect[0];
    assert(!Storage_LoadConfig(&loaded));
    assert(Storage_SaveConfig(&old_value)); save_flash();
    mutations=0; assert(Storage_SaveConfig(&old_value) && mutations==0);
    assert(Storage_SaveConfig(&new_value)); writes=mutations;
    for(cut=0;cut<=writes;++cut) {
        restore_flash(); cut_after=cut;
        Storage_SaveConfig(&new_value);
        cut_after=-1; 
        assert(Storage_LoadConfig(&loaded));
        assert(!memcmp(&loaded,&old_value,sizeof(loaded)) || !memcmp(&loaded,&new_value,sizeof(loaded)));
        assert(Storage_SaveConfig(&new_value));
        assert(Storage_LoadConfig(&loaded) && !memcmp(&loaded,&new_value,sizeof(loaded)));
    }
    /* Corrupt metadata and commit words, not just the payload. */
    restore_flash(); *(UINT32 *)(FLASH_CONFIG_A+4)^=1;
    assert(!Storage_LoadConfig(&loaded));
    restore_flash(); *(UINT16 *)(FLASH_CONFIG_A+14)=0xFFFF;
    assert(!Storage_LoadConfig(&loaded));
    restore_flash();
    /* A newer CRC-valid but unsafe configuration must not mask an older safe copy. */
    {
        STORAGE_FLASH_CONFIG_DATA unsafe=new_value;
        unsafe.rw.protect[0]=0xFFFFU;
        assert(!Storage_SaveConfig(&unsafe));
        assert(Storage_Erase(FLASH_CONFIG_B));
        assert(Storage_WriteRecord(FLASH_CONFIG_B,STORAGE_CONFIG_MAGIC,2U,&unsafe,sizeof(unsafe)));
        assert(Storage_LoadConfig(&loaded) && !memcmp(&loaded,&old_value,sizeof(loaded)));
        assert(Storage_SaveConfig(&new_value));
        assert(Storage_LoadConfig(&loaded) && !memcmp(&loaded,&new_value,sizeof(loaded)));
    }
    restore_flash();
    assert(Storage_Erase(FLASH_CONFIG_B));
    assert(Storage_WriteRecord(FLASH_CONFIG_B,STORAGE_CONFIG_MAGIC,UINT32_MAX,&new_value,sizeof(new_value)));
    assert(Storage_Erase(FLASH_CONFIG_A));
    assert(Storage_WriteRecord(FLASH_CONFIG_A,STORAGE_CONFIG_MAGIC,0,&old_value,sizeof(old_value)));
    assert(Storage_LoadConfig(&loaded) && !memcmp(&loaded,&old_value,sizeof(loaded)));
    assert(!Storage_Erase(0x08004800) && !Storage_Erase(FLASH_STORAGE_END));
    assert(!Storage_SaveConfig(0) && !Storage_LoadConfig(0));
    printf("PASS storage: config atomic commit, %d cut points, header CRC, sequence wrap, bounds, no-op saves\n",writes+1);
    /* A valid SOC snapshot remains recoverable through state page rotation. */
    memset(&soc,0,sizeof(soc)); soc.u16FormatVersion=FLASH_STORAGE_SOC_DATA_VERSION;
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
    }
    puts("PASS storage: SOC state append and rotation, power cuts retain complete state");
    memset((void *)FLASH_PARAMETER_RESET_BEGIN,0xFF,16384);
    memset((void *)FLASH_STATS_A,0x5A,2048);
    update_generation=update_mask=0;
    assert(StorageFlash_InitParameters());
    PRT_E2ROMParas.u16VcellOvp_First=4001;
    assert(StorageFlash_SaveParameters(0U));
    assert(StorageFlash_SaveSocData(&soc) && StorageFlash_AppendLog(1,171));
    strcpy(firmware_version,"storage-test-v2");
    mutations=0;
    assert(StorageFlash_InitParameters() && !mutations &&
        PRT_E2ROMParas.u16VcellOvp_First==4001);
    assert(StorageFlash_LoadSocData(&loaded_soc));
    assert(Storage_LoadConfig(&loaded));
    loaded.rw.reserved[0]=0xFFFFU;
    assert(!Storage_RWParametersValid(&loaded.rw));
    puts("PASS storage: new firmware version preserves saved parameters, SOC and logs");
    test_parameter_application();
    test_log();
    test_module_updates();
    test_statistics();
    test_communication_storage();
    test_soc_cycle_persistence();test_soc_smooth_persistence();
    return 0;
}
