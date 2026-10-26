/* Run the real background self-test against emulated hardware, not a board. */
#include "FlashSelfTest.h"
#include "rtc_sleep.h"
static struct {
    volatile FLASH_SELF_TEST flash_test;
    struct { struct { UINT32 ore; } uart1,uart2,uart3; } irq;
} sys_time;
volatile struct LOW_POWER_RTC_STATUS g_stLowPowerRtcStatus;
static UINT8 u8FlashUpdateFlag;
static int sci_busy,can_busy,pending,afe_error;
static UINT16 task_overflow;
static UINT16 SysTime_Get200msTaskOverflowCount(void) { return task_overflow; }
static UINT8 SysTime_HasPendingTaskFlags(void) { return (UINT8)pending; }
static UINT8 Sci_IsAnyPortBusy(void) { return (UINT8)sci_busy; }
static UINT8 Can_IsBusy(void) { return (UINT8)can_busy; }
#define ERROR_STATUS_AFE1 999
/* PRODUCTION_SOURCE */
static UINT8 production_before[8192];
static void restart_test(UINT32 cause) {
    memset(hw_erases,0,sizeof(hw_erases)); hw_halfwords=0;
    FlashSelfTest_Init(cause);
}
static void step(void) { FlashSelfTest_Task(); }
static void until_phase(UINT32 phase) {
    int tries=0;
    while(sys_time.flash_test.progress.phase!=phase && FlashSelfTest_IsActive()) {
        step(); assert(++tries<500000);
    }
    assert(FlashSelfTest_IsActive() && sys_time.flash_test.progress.phase==phase);
}
static void reset_test(void) {
    memset((void *)FLASH_SELF_TEST_BEGIN,0xFF,8192);
    memset(&s_flash,0,sizeof(s_flash)); mutations=errors=0; cut_after=-1; error_after_commit=0;
    sci_busy=can_busy=pending=afe_error=0; task_overflow=0; u8FlashUpdateFlag=0;
    *(UINT16 *)0x1FFFF7E0=128; tick=0; g_stLowPowerRtcStatus.mode=NO_SLEEP;
    restart_test(RCC_CSR_PORRSTF);
}
static void check_communication_pattern(void) {
    STORAGE_FLASH_CONFIG_DATA config;
    FLASH_SELF_TEST_PROGRESS progress;
    char number[BATTERY_NUMBER_LENGTH+1U];
    UINT16 i,word;
    assert(Storage_LoadConfigAt(FLASH_SELF_TEST_CONFIG,&config));
    memcpy(&progress,&config.rw,sizeof(progress));
    assert(config.rw.reserved[STORAGE_BAUD_MODE_INDEX]==progress.count[FLASH_TEST_CONFIG]%3U+1U);
    assert(config.rw.reserved[STORAGE_BATTERY_NUMBER_TAG_INDEX]==STORAGE_BATTERY_NUMBER_TAG);
    snprintf(number,sizeof(number),"%011lu",(unsigned long)progress.count[FLASH_TEST_CONFIG]);
    for(i=0;i<BATTERY_NUMBER_LENGTH;++i) {
        word=config.rw.reserved[STORAGE_BATTERY_NUMBER_INDEX+i/2U];
        assert((UINT8)((i & 1U) ? word : word>>8)==(UINT8)number[i]);
    }
    assert((config.rw.reserved[STORAGE_BATTERY_NUMBER_INDEX+BATTERY_NUMBER_LENGTH/2U] & 0xFFU)==0xFFU);
}
static void test_completion_and_isolation(void) {
    STORAGE_FLASH_CONFIG_DATA config,actual_config;
    STORAGE_FLASH_SOC_DATA soc,actual_soc;
    UINT32 counts[11],baseline[11],n=0U;
    UINT8 number[BATTERY_NUMBER_LENGTH],point,records[100][2],expected_records[100][2];
    UINT8 expected_point;
    int saved_mutations,stage_resets=0;
    reset_test();
    memset(&config,0x35,sizeof(config)); config.initializing=0U;
    assert(StorageFlash_SaveConfig(&config) && StorageFlash_SetBaudMode(3U));
    assert(StorageFlash_WriteBatteryNumber((const UINT8 *)"BAT00000001"));
    assert(StorageFlash_LoadConfig(&config));
    memset(&soc,0,sizeof(soc)); soc.u16FormatVersion=FLASH_STORAGE_SOC_DATA_VERSION; soc.u16SocNow=37;
    assert(StorageFlash_SaveSocData(&soc));
    assert(StorageFlash_AppendLog(VCELL_OVP,3));
    assert(StorageFlash_LoadLogData(&expected_point,expected_records));
    StorageFlash_InitStatistics(RCC_CSR_PORRSTF,BOOT_FLAG_RESET_VALUE);
    StorageFlash_UpdateFaultStatistics(1U); assert(StorageFlash_ReadStatistics(baseline));
    while(FlashSelfTest_IsActive()) {
        if((n%127U)==0U) {
            config.rw.other[0]++; assert(StorageFlash_SaveConfig(&config));
            soc.u16SocNow=(UINT16)(n%101U); assert(StorageFlash_SaveSocData(&soc));
        }
        step(); assert(++n<500000U);
        if(sys_time.flash_test.progress.phase==FLASH_TEST_CONFIG_PHASE) check_communication_pattern();
        if(stage_resets<5 && sys_time.flash_test.progress.phase==(UINT32)(FLASH_TEST_SOC_PHASE+stage_resets) &&
            (n%137U)==0U) {
            restart_test(RCC_CSR_SFTRSTF); ++stage_resets;
        }
    }
    assert(sys_time.flash_test.status==FLASH_TEST_PASS && !sys_time.flash_test.first_error);
    for(n=0;n<8;++n) assert(sys_time.flash_test.session_erases[n]==hw_erases[n]);
    assert(sys_time.flash_test.session_halfwords==hw_halfwords);
    assert(sys_time.flash_test.progress.count[FLASH_TEST_SOC]==PROJECT_CFG_STORAGE_SELF_TEST_SOC_WRITES);
    assert(sys_time.flash_test.progress.count[FLASH_TEST_LOG]==PROJECT_CFG_STORAGE_SELF_TEST_LOG_WRITES);
    assert(sys_time.flash_test.progress.count[FLASH_TEST_STATS]==PROJECT_CFG_STORAGE_SELF_TEST_STATS_WRITES);
    assert(sys_time.flash_test.progress.count[FLASH_TEST_CONFIG]==PROJECT_CFG_STORAGE_SELF_TEST_CONFIG_WRITES);
    assert(StorageFlash_LoadConfig(&actual_config) && !memcmp(&actual_config,&config,sizeof(config)));
    assert(StorageFlash_GetBaudMode()==3U);
    assert(StorageFlash_ReadBatteryNumber(number) && !memcmp(number,"BAT00000001",BATTERY_NUMBER_LENGTH));
    assert(StorageFlash_LoadSocData(&actual_soc) && !memcmp(&actual_soc,&soc,sizeof(soc)));
    assert(StorageFlash_LoadLogData(&point,records) && point==expected_point && !memcmp(records,expected_records,200));
    assert(StorageFlash_ReadStatistics(counts) && !memcmp(counts,baseline,sizeof(counts)));
    saved_mutations=mutations;
    restart_test(RCC_CSR_PORRSTF); step(); step();
    assert(sys_time.flash_test.status==FLASH_TEST_PASS && sys_time.flash_test.resumed && mutations==saved_mutations);
    puts("PASS self-test: valid baud/11-byte serial/tag patterns, goals, rotations/clears, restart/resume, completion marker; production communication/config/SOC/log/statistics unchanged");
}
static void test_pause_and_noop(void) {
    int i,before;
    reset_test(); memcpy(production_before,(void *)FLASH_STORAGE_BEGIN,sizeof(production_before));
    for(i=0;i<6;++i) {
        sci_busy=i==0; can_busy=i==1; pending=i==2; afe_error=i==3;
        u8FlashUpdateFlag=(UINT8)(i==4); g_stLowPowerRtcStatus.mode=(UINT8)(i==5 ? NORMAL_MODE : NO_SLEEP);
        before=mutations; step(); assert(FlashSelfTest_IsActive() && mutations==before);
    }
    sci_busy=can_busy=pending=afe_error=0; u8FlashUpdateFlag=0; g_stLowPowerRtcStatus.mode=NO_SLEEP;
    until_phase(FLASH_TEST_SOC_PHASE);
    while(sys_time.flash_test.progress.count[FLASH_TEST_SOC]<64U) step();
    assert(sys_time.flash_test.status==FLASH_TEST_RUNNING);
    assert(!memcmp(production_before,(void *)FLASH_STORAGE_BEGIN,sizeof(production_before)));
    before=mutations; step();
    assert(mutations>before && sys_time.flash_test.progress.count[FLASH_TEST_SOC]==65U);
    pending=1; before=mutations; step(); assert(mutations==before);
    pending=0; step();
    assert(mutations>before && sys_time.flash_test.progress.count[FLASH_TEST_SOC]==66U);
    tick=UINT32_MAX; step(); tick=0U; step();
    assert(sys_time.flash_test.progress.count[FLASH_TEST_SOC]==68U);
    task_overflow=1; step(); assert(sys_time.flash_test.status==FLASH_TEST_FAIL && sys_time.flash_test.first_error==FLASH_TEST_SCHEDULER_ERROR);
    puts("PASS self-test scheduling: no tick wait, one SOC commit per pass; UART/CAN/pending/AFE/IAP/NORMAL defer, duplicate no-write checks, tick wrap, scheduler error stops");
}
static void test_failures(void) {
    int before;
    UINT32 address;
    STORAGE_FLASH_CONFIG_DATA config;
    FLASH_SELF_TEST_PROGRESS progress;
    reset_test(); step(); before=mutations; cut_after=mutations; step();
    assert(sys_time.flash_test.status==FLASH_TEST_FAIL && sys_time.flash_test.first_error==FLASH_TEST_ERASE_ERROR);
    cut_after=-1; step(); assert(mutations==before);
    reset_test(); until_phase(FLASH_TEST_CONFIG_PHASE); cut_after=mutations+1; step();
    assert(sys_time.flash_test.status==FLASH_TEST_FAIL && sys_time.flash_test.first_error==FLASH_TEST_PROGRAM_ERROR);
    before=mutations; cut_after=-1; step(); assert(mutations==before);
    reset_test(); until_phase(FLASH_TEST_CONFIG_PHASE); error_after_commit=1; step();
    assert(sys_time.flash_test.status==FLASH_TEST_FAIL); error_after_commit=0;
    reset_test(); until_phase(FLASH_TEST_SOC_PHASE);
    while(sys_time.flash_test.progress.count[FLASH_TEST_SOC]<3U) step();
    address=Storage_StateLatest(FLASH_SELF_TEST_STATE); *(UINT16 *)(address+16U)^=1U;
    before=mutations; step(); assert(sys_time.flash_test.status==FLASH_TEST_FAIL && mutations==before);
    reset_test(); until_phase(FLASH_TEST_LOG_PHASE); step(); step();
    address=Storage_LogLatestPage(FLASH_SELF_TEST_LOG)+STORAGE_LOG_START; *(UINT16 *)(address+2U)^=1U;
    before=mutations; step(); assert(sys_time.flash_test.status==FLASH_TEST_FAIL && mutations==before);
    reset_test(); until_phase(FLASH_TEST_STATS_PHASE); step(); step();
    address=Storage_StatsPage(FLASH_SELF_TEST_STATS)+sizeof(STORAGE_RECORD)+sizeof(STORAGE_STATS_DATA);
    *(UINT16 *)(address+4U)^=1U; before=mutations; step();
    assert(sys_time.flash_test.status==FLASH_TEST_FAIL && mutations==before);
    reset_test(); *(UINT16 *)FLASH_SELF_TEST_CONFIG=0x1234;
    before=mutations; step(); assert(sys_time.flash_test.status==FLASH_TEST_FAIL && mutations==before);
    reset_test(); *(UINT16 *)0x1FFFF7E0=64; restart_test(0);
    assert(sys_time.flash_test.status==FLASH_TEST_FAIL && !mutations);
    reset_test(); restart_test(RCC_CSR_IWDGRSTF);
    assert(sys_time.flash_test.status==FLASH_TEST_FAIL && sys_time.flash_test.first_error==FLASH_TEST_WATCHDOG_ERROR && !mutations);
    reset_test(); before=mutations; ++sys_time.irq.uart1.ore; step();
    assert(sys_time.flash_test.status==FLASH_TEST_FAIL && sys_time.flash_test.first_error==FLASH_TEST_UART_ERROR && mutations==before);
    reset_test(); step(); assert(Storage_LoadConfigAt(FLASH_SELF_TEST_CONFIG,&config));
    memcpy(&progress,&config.rw,sizeof(progress)); progress.signature=0x54535431UL;
    memcpy(&config.rw,&progress,sizeof(progress));
    assert(Storage_SaveConfigAt(FLASH_SELF_TEST_CONFIG,&config)); before=mutations;
    restart_test(RCC_CSR_SFTRSTF); step();
    assert(sys_time.flash_test.status==FLASH_TEST_FAIL && sys_time.flash_test.first_error==FLASH_TEST_PROFILE_ERROR && mutations==before);
    puts("PASS self-test faults: erase/program/post-commit errors stop; corrupt state/log/statistics and unknown data preserved; 64 KiB and watchdog rejected");
    puts("PASS self-test profile: older TST1 marker rejected without erasing or accepting its result");
}
int main(void) {
    host_storage_init();
#if PROJECT_CFG_STORAGE_SELF_TEST_SOC_WRITES > 300000
    reset_test(); assert(sys_time.flash_test.status==FLASH_TEST_BUDGET_LIMIT && !mutations);
    puts("PASS self-test budget: excessive target rejected before any write");
#else
    test_completion_and_isolation(); test_pause_and_noop(); test_failures();
#endif
    return 0;
}
