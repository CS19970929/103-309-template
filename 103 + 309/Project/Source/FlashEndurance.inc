/* Included by Flash.c: deliberately reuses its real journal implementation. */
static STORAGE_FLASH_SOC_DATA s_testBackup, s_testLast;
static UINT32 s_testSubmitted, s_testSaved, s_testSkipped, s_testFailed, s_testMismatch;
static UINT32 s_testLastTick;
static UINT16 s_testMode, s_testFault;

UINT8 FlashTest_Active(void) { return s_testMode != 0U; }

static UINT8 FlashTest_SaveVerified(const STORAGE_FLASH_SOC_DATA *data)
{
    STORAGE_FLASH_SOC_DATA loaded;
    if (!StorageFlash_SaveSocDataReal(data)) { ++s_testFailed; s_testFault=1U; return 0U; }
    if (!StorageFlash_LoadSocDataReal(&loaded) || memcmp(&loaded,data,sizeof(loaded)))
    { ++s_testMismatch; s_testFault=2U; return 0U; }
    return 1U;
}

UINT8 FlashTest_Command(UINT16 address, UINT16 value)
{
    STORAGE_FLASH_SOC_DATA sample;
    s_testLastTick=SysTime_Get10msTickCount();
    if (address == FLASH_TEST_BASE)
    {
        if (value == FLASH_TEST_STOP)
        {
            if (!s_testMode) return 1U;
            if (!FlashTest_SaveVerified(&s_testBackup)) return 0U;
            s_testMode=0U;
            return 1U;
        }
        if (s_testMode || (value != FLASH_TEST_START_POLICY && value != FLASH_TEST_START_FORCE)) return 0U;
        if (!StorageFlash_LoadSocDataReal(&s_testBackup) || s_testBackup.u16Reserved[0]==FLASH_TEST_SNAPSHOT_MARKER) return 0U;
        s_testLast=s_testBackup;
        memset(s_testEraseAttempt,0,sizeof(s_testEraseAttempt));
        memset(s_testEraseSuccess,0,sizeof(s_testEraseSuccess));
        s_testSubmitted=s_testSaved=s_testSkipped=s_testFailed=s_testMismatch=s_testProgramFailure=0U;
        s_testFault=0U;
        s_testMode=value;
        return 1U;
    }
    if (address != FLASH_TEST_BASE+1U || !s_testMode || s_testFault || value>100U) return 0U;
    ++s_testSubmitted;
    /* SOC-only scenario: other normal save triggers are deliberately held fixed. */
    if (s_testMode==FLASH_TEST_START_POLICY && value==s_testLast.u16SocNow)
    { ++s_testSkipped; return 1U; }
    sample=s_testBackup;
    sample.u16SocNow=value;
    sample.u16Reserved[0]=FLASH_TEST_SNAPSHOT_MARKER;
    sample.u32CapNow=(sample.u32CapFull/100U)*value;
    if (!FlashTest_SaveVerified(&sample)) return 0U;
    s_testLast=sample;
    ++s_testSaved;
    return 1U;
}

UINT8 FlashTest_Restore(const UINT16 *words)
{
    STORAGE_FLASH_SOC_DATA data;
    memcpy(&data,words,sizeof(data));
    if (s_testMode || data.u16FormatVersion != FLASH_STORAGE_SOC_DATA_VERSION_CURRENT ||
        data.u16Reserved[0]==FLASH_TEST_SNAPSHOT_MARKER || data.u16SocNow>100U || data.u16DsgSocInt>100U || data.u32CapNow>data.u32CapFull) return 0U;
    return FlashTest_SaveVerified(&data);
}

static void FlashTest_Put32(UINT16 *w, UINT32 value)
{ w[0]=(UINT16)(value>>16); w[1]=(UINT16)value; }

void FlashTest_Read(UINT8 *bytes)
{
    UINT16 w[FLASH_TEST_WORDS],i;
    STORAGE_FLASH_SOC_DATA current;
    memset(w,0,sizeof(w));
    w[0]=0x4654U; w[1]=1U; w[2]=s_testMode; w[3]=s_testFault;
    FlashTest_Put32(w+4,s_testSubmitted); FlashTest_Put32(w+6,s_testSaved);
    FlashTest_Put32(w+8,s_testSkipped); FlashTest_Put32(w+10,s_testFailed);
    FlashTest_Put32(w+12,s_testMismatch); FlashTest_Put32(w+14,s_testProgramFailure);
    for (i=0U;i<8U;++i)
    { FlashTest_Put32(w+16U+2U*i,s_testEraseAttempt[i]); FlashTest_Put32(w+32U+2U*i,s_testEraseSuccess[i]); }
    w[48]=sizeof(STORAGE_FLASH_SOC_DATA)/2U;
    w[49]=StorageFlash_LoadSocDataReal(&current);
    if (w[49]) memcpy(w+50,&current,sizeof(current));
    memcpy(w+72,&s_testBackup,sizeof(s_testBackup));
    for(i=0U;i<FLASH_TEST_WORDS;++i) { bytes[i*2U]=(UINT8)(w[i]>>8); bytes[i*2U+1U]=(UINT8)w[i]; }
}

void FlashTest_Service(void)
{
    /* A lost PC connection stops writes immediately; restore once after 10 seconds.
       A restore error latches, leaving normal SOC persistence blocked for diagnosis. */
    if(s_testMode && !s_testFault && (UINT32)(SysTime_Get10msTickCount()-s_testLastTick)>1000U)
        (void)FlashTest_Command(FLASH_TEST_BASE,FLASH_TEST_STOP);
}
