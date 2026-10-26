#include "main.h"

#define FLASH_SIZE_REG_ADDR ((UINT32)0x1FFFF7E0)
#define APP_UPGRADE_MAILBOX_ADDR ((UINT32)0x20004FE0)
#define APP_UPGRADE_MAILBOX_MAGIC ((UINT32)0x49415031)
#define APP_UPGRADE_MAILBOX_REQUEST ((UINT32)0x5AA55AA5)
typedef struct { UINT32 magic, magic_inv, request, request_inv, crc; } APP_UPGRADE_MAILBOX;
typedef struct { volatile UINT8 busy; } FLASH_RUNTIME;

static FLASH_RUNTIME s_flash;

static void StorageFlash_BeginWrite(void)
{
	s_flash.busy = 1U;
}

static void StorageFlash_EndWrite(void)
{
	s_flash.busy = 0U;
}

UINT8 StorageFlash_IsBusy(void)
{
	return s_flash.busy;
}

#if PROJECT_CFG_STORAGE_SELF_TEST_ENABLE
#define FLASH_TEST_ERASE_LIMIT 10000U
static void FlashSelfTest_Fail(UINT32 error, UINT32 address)
{
    volatile FLASH_SELF_TEST *test=&sys_time.flash_test;
    if (!test->first_error) {
        test->first_error=error;
        test->first_error_address=address;
    }
    test->status=(error==FLASH_TEST_BUDGET_ERROR) ? FLASH_TEST_BUDGET_LIMIT : FLASH_TEST_FAIL;
}

static UINT8 FlashSelfTest_Address(UINT32 address)
{
    return (UINT8)(address>=FLASH_SELF_TEST_BEGIN && address<FLASH_SELF_TEST_END);
}
#endif

static FLASH_Status FlashErasePageVerified(uint32_t page_addr)
{
	UINT32 offset;
	FLASH_Status result;

#if PROJECT_CFG_STORAGE_SELF_TEST_ENABLE
    if (FlashSelfTest_Address(page_addr) &&
        sys_time.flash_test.session_erases[(page_addr-FLASH_SELF_TEST_BEGIN)/FLASH_STORAGE_PAGE_SIZE]>=FLASH_TEST_ERASE_LIMIT) {
        FlashSelfTest_Fail(FLASH_TEST_BUDGET_ERROR,page_addr);
        return FLASH_ERROR_PG;
    }
#endif
	result = FLASH_ErasePage(page_addr);
#if PROJECT_CFG_STORAGE_SELF_TEST_ENABLE
    if (FlashSelfTest_Address(page_addr)) {
        if (result==FLASH_COMPLETE)
            ++sys_time.flash_test.session_erases[(page_addr-FLASH_SELF_TEST_BEGIN)/FLASH_STORAGE_PAGE_SIZE];
        else FlashSelfTest_Fail(FLASH_TEST_ERASE_ERROR,page_addr);
    }
#endif
	if (result != FLASH_COMPLETE)
	{
		return result;
	}

	for (offset = 0; offset < FLASH_STORAGE_PAGE_SIZE; offset += 2)
	{
		if (FlashReadOneHalfWord(page_addr + offset) != 0xFFFF)
		{
#if PROJECT_CFG_STORAGE_SELF_TEST_ENABLE
            if (FlashSelfTest_Address(page_addr)) FlashSelfTest_Fail(FLASH_TEST_VERIFY_ERROR,page_addr+offset);
#endif
			return FLASH_ERROR_PG;
		}
	}

	return FLASH_COMPLETE;
}

static FLASH_Status FlashProgramHalfWordVerified(uint32_t addr, uint16_t data)
{
	FLASH_Status result;

	result = FLASH_ProgramHalfWord(addr, data);
#if PROJECT_CFG_STORAGE_SELF_TEST_ENABLE
    if (FlashSelfTest_Address(addr)) {
        if (result==FLASH_COMPLETE) ++sys_time.flash_test.session_halfwords;
        else FlashSelfTest_Fail(FLASH_TEST_PROGRAM_ERROR,addr);
    }
#endif
	if (result != FLASH_COMPLETE)
	{
		return result;
	}

	if (FlashReadOneHalfWord(addr) != data)
	{
#if PROJECT_CFG_STORAGE_SELF_TEST_ENABLE
        if (FlashSelfTest_Address(addr)) FlashSelfTest_Fail(FLASH_TEST_VERIFY_ERROR,addr);
#endif
		return FLASH_ERROR_PG;
	}

	return FLASH_COMPLETE;
}

static FLASH_Status FlashProgramBytesVerified(uint32_t addr, const UINT8 *data, UINT16 length)
{
	UINT16 offset = 0;
	UINT16 half_word;
	FLASH_Status result = FLASH_COMPLETE;

	while (offset < length)
	{
		half_word = data[offset];
		if ((offset + 1) < length)
		{
			half_word |= ((UINT16)data[offset + 1] << 8);
		}
		else
		{
			half_word |= 0xFF00;
		}

		result = FlashProgramHalfWordVerified(addr + offset, half_word);
		if (result != FLASH_COMPLETE)
		{
			return result;
		}
		offset += 2;
	}

	return result;
}


static UINT8 StorageFlash_IsAreaBlank(uint32_t addr, UINT16 length);

/* A record becomes visible only after its final half-word is committed. */
#define STORAGE_RECORD_FORMAT 0x5301U
#define STORAGE_RECORD_COMMIT 0xA55AU
#define STORAGE_CONFIG_MAGIC  0x43464731UL

typedef struct {
    UINT32 magic;
    UINT32 sequence;
    UINT16 length;
    UINT16 crc;
    UINT16 format;
    UINT16 commit;
} STORAGE_RECORD;

typedef char StorageRecordSizeCheck[(sizeof(STORAGE_RECORD)==16U) ? 1 : -1];
typedef char StorageConfigSizeCheck[(sizeof(STORAGE_FLASH_CONFIG_DATA)+sizeof(STORAGE_RECORD)
                                   <=FLASH_STORAGE_PAGE_SIZE) ? 1 : -1];

static UINT16 Storage_CrcUpdate(UINT16 crc, const UINT8 *data, UINT16 length)
{
    UINT8 bit;
    while (length-- != 0U) {
        crc ^= *data++;
        for (bit=0U; bit<8U; ++bit)
            crc=(crc & 1U) ? (UINT16)((crc>>1)^0xA001U) : (UINT16)(crc>>1);
    }
    return crc;
}

static UINT16 Storage_RecordCrc(const STORAGE_RECORD *header, const void *payload)
{
    UINT16 crc=Storage_CrcUpdate(0xFFFFU,(const UINT8 *)header,10U);
    crc=Storage_CrcUpdate(crc,(const UINT8 *)&header->format,2U);
    return Storage_CrcUpdate(crc,(const UINT8 *)payload,header->length);
}

static UINT8 Storage_Newer(UINT32 a, UINT32 b)
{
    return (UINT8)((INT32)(a-b)>0);
}

static UINT8 Storage_AddressValid(UINT32 address, UINT32 length)
{
#if PROJECT_CFG_STORAGE_SELF_TEST_ENABLE
    if (FlashSelfTest_Address(address))
        return (UINT8)(*((volatile UINT16 *)FLASH_SIZE_REG_ADDR)>=128U &&
                      length<=FLASH_SELF_TEST_END-address && (address & 1U)==0U);
#endif
    return (UINT8)(*((volatile UINT16 *)FLASH_SIZE_REG_ADDR)>=128U &&
                   address>=FLASH_STORAGE_BEGIN && address<FLASH_STORAGE_END &&
                   length<=FLASH_STORAGE_END-address && (address & 1U)==0U);
}

static UINT8 Storage_Erase(UINT32 address)
{
    FLASH_Status result;
    if (*((volatile UINT16 *)FLASH_SIZE_REG_ADDR)<128U ||
        address<FLASH_PARAMETER_RESET_BEGIN || address>FLASH_STORAGE_END-FLASH_STORAGE_PAGE_SIZE ||
        (address & (FLASH_STORAGE_PAGE_SIZE-1U))!=0U)
        return 0U;
    FLASH_Unlock();
    FLASH_ClearFlag(FLASH_FLAG_EOP | FLASH_FLAG_PGERR | FLASH_FLAG_WRPRTERR);
    result=FlashErasePageVerified(address);
    FLASH_Lock();
    return (UINT8)(result==FLASH_COMPLETE);
}

static UINT8 Storage_RecordValid(UINT32 address, UINT32 magic, UINT16 length)
{
    const STORAGE_RECORD *h=(const STORAGE_RECORD *)address;
    if (!Storage_AddressValid(address,(UINT32)sizeof(*h)+length)) return 0U;
    return (UINT8)(h->magic==magic && h->format==STORAGE_RECORD_FORMAT &&
        h->length==length && h->commit==STORAGE_RECORD_COMMIT &&
        h->crc==Storage_RecordCrc(h,(const void *)(address+sizeof(*h))));
}

static UINT8 Storage_WriteRecord(UINT32 address, UINT32 magic, UINT32 sequence,
                                const void *payload, UINT16 length)
{
    STORAGE_RECORD h;
    FLASH_Status result;
    if (payload==0 || !Storage_AddressValid(address,sizeof(h)+(UINT32)length) ||
        !StorageFlash_IsAreaBlank(address,(UINT16)(sizeof(h)+length))) return 0U;
    h.magic=magic; h.sequence=sequence; h.length=length;
    h.format=STORAGE_RECORD_FORMAT; h.commit=0xFFFFU;
    h.crc=Storage_RecordCrc(&h,payload);
    FLASH_Unlock();
    FLASH_ClearFlag(FLASH_FLAG_EOP | FLASH_FLAG_PGERR | FLASH_FLAG_WRPRTERR);
    result=FlashProgramBytesVerified(address,(const UINT8 *)&h,14U);
    if (result==FLASH_COMPLETE)
        result=FlashProgramBytesVerified(address+sizeof(h),(const UINT8 *)payload,length);
    if (result==FLASH_COMPLETE)
        result=FlashProgramHalfWordVerified(address+14U,STORAGE_RECORD_COMMIT);
    FLASH_Lock();
    /* A readable committed record is authoritative even if the peripheral
     * reported an error after programming its final half-word. */
    return (UINT8)(Storage_RecordValid(address,magic,length) &&
                  memcmp((const void *)(address+sizeof(h)),payload,length)==0);
}

static UINT32 Storage_SelectPair(UINT32 a, UINT32 b, UINT32 magic, UINT16 length)
{
    UINT8 va=Storage_RecordValid(a,magic,length);
    UINT8 vb=Storage_RecordValid(b,magic,length);
    if (!va) return vb ? b : 0U;
    if (!vb) return a;
    return Storage_Newer(((const STORAGE_RECORD *)b)->sequence,
                         ((const STORAGE_RECORD *)a)->sequence) ? b : a;
}

static UINT8 Storage_LoadConfigAt(UINT32 base, STORAGE_FLASH_CONFIG_DATA *data)
{
    UINT32 address;
    if (data==0) return 0U;
    address=Storage_SelectPair(base,(base+FLASH_STORAGE_PAGE_SIZE),STORAGE_CONFIG_MAGIC,sizeof(*data));
    if (!address) return 0U;
    memcpy(data,(const void *)(address+sizeof(STORAGE_RECORD)),sizeof(*data));
    return 1U;
}

static UINT8 Storage_SaveConfigAt(UINT32 base, const STORAGE_FLASH_CONFIG_DATA *data)
{
    UINT32 address, target, sequence=1U;
    UINT8 result;
    if (data==0) return 0U;
    address=Storage_SelectPair(base,(base+FLASH_STORAGE_PAGE_SIZE),STORAGE_CONFIG_MAGIC,sizeof(*data));
    if (address) {
        if (memcmp((const void *)(address+sizeof(STORAGE_RECORD)),data,sizeof(*data))==0) return 1U;
        sequence=((const STORAGE_RECORD *)address)->sequence+1U;
    }
    target=address==base ? (base+FLASH_STORAGE_PAGE_SIZE) : base;
    StorageFlash_BeginWrite();
    result=(UINT8)(Storage_Erase(target) &&
        Storage_WriteRecord(target,STORAGE_CONFIG_MAGIC,sequence,data,sizeof(*data)));
    StorageFlash_EndWrite();
    if (!result) System_ERROR_UserCallback(ERROR_EEPROM_STORE);
    return result;
}

UINT8 StorageFlash_LoadConfig(STORAGE_FLASH_CONFIG_DATA *data)
{
    return Storage_LoadConfigAt(FLASH_CONFIG_A,data);
}

UINT8 StorageFlash_SaveConfig(const STORAGE_FLASH_CONFIG_DATA *data)
{
    return Storage_SaveConfigAt(FLASH_CONFIG_A,data);
}

static UINT8 StorageFlash_IsAreaBlank(uint32_t addr, UINT16 length)
{
	UINT16 offset;

	for (offset = 0; offset < length; offset += 2)
	{
		if (FlashReadOneHalfWord(addr + offset) != 0xFFFF)
		{
			return 0;
		}
	}

	return 1;
}


/* State snapshots are small and append within the active page. A failed tail
 * is skipped; page rotation commits the replacement before the old is reused. */
#define STORAGE_STATE_MAGIC 0x53544131UL
#define STORAGE_STATE_SOC 1U
#define STORAGE_STATE_AGING 2U
typedef struct {
    STORAGE_FLASH_SOC_DATA soc;
    STORAGE_FLASH_FACTORY_AGING_DATA aging;
    UINT16 valid;
    UINT16 reserved;
} STORAGE_STATE;
#define STORAGE_STATE_SPAN (sizeof(STORAGE_RECORD)+sizeof(STORAGE_STATE))

static UINT32 Storage_StateLatest(UINT32 base)
{
    UINT32 page, address, latest=0U;
    for (page=base; page<base+2U*FLASH_STORAGE_PAGE_SIZE; page+=FLASH_STORAGE_PAGE_SIZE) {
        for (address=page; address+STORAGE_STATE_SPAN<=page+FLASH_STORAGE_PAGE_SIZE;
             address+=STORAGE_STATE_SPAN) {
            if (Storage_RecordValid(address,STORAGE_STATE_MAGIC,sizeof(STORAGE_STATE)) &&
                (!latest || Storage_Newer(((const STORAGE_RECORD *)address)->sequence,
                                         ((const STORAGE_RECORD *)latest)->sequence))) latest=address;
        }
    }
    return latest;
}

static void Storage_LoadState(UINT32 base, STORAGE_STATE *state)
{
    UINT32 address=Storage_StateLatest(base);
    memset(state,0,sizeof(*state));
    if (address) memcpy(state,(const void *)(address+sizeof(STORAGE_RECORD)),sizeof(*state));
}

static UINT8 Storage_SaveState(UINT32 base, const STORAGE_STATE *state)
{
    UINT32 latest=Storage_StateLatest(base), page=base, address, next;
    UINT32 sequence=1U;
    UINT8 result=0U;
    if (latest) {
        if (!memcmp((const void *)(latest+sizeof(STORAGE_RECORD)),state,sizeof(*state))) return 1U;
        page=latest & ~(FLASH_STORAGE_PAGE_SIZE-1U);
        sequence=((const STORAGE_RECORD *)latest)->sequence+1U;
    }
    next=page;
    for (address=page; address+STORAGE_STATE_SPAN<=page+FLASH_STORAGE_PAGE_SIZE;
         address+=STORAGE_STATE_SPAN)
        if (!StorageFlash_IsAreaBlank(address,STORAGE_STATE_SPAN)) next=address+STORAGE_STATE_SPAN;
    StorageFlash_BeginWrite();
    if (next+STORAGE_STATE_SPAN>page+FLASH_STORAGE_PAGE_SIZE) {
        next=page==base ? base+FLASH_STORAGE_PAGE_SIZE : base;
        if (!Storage_Erase(next)) goto done;
    }
    result=Storage_WriteRecord(next,STORAGE_STATE_MAGIC,sequence,state,sizeof(*state));
done:
    StorageFlash_EndWrite();
    if (!result) System_ERROR_UserCallback(ERROR_EEPROM_STORE);
    return result;
}

static UINT8 Storage_LoadSocAt(UINT32 base, STORAGE_FLASH_SOC_DATA *data)
{
    STORAGE_STATE state;
    if (!data) return 0U;
    Storage_LoadState(base,&state);
    if (!(state.valid & STORAGE_STATE_SOC) || state.soc.u16FormatVersion!=FLASH_STORAGE_SOC_DATA_VERSION) return 0U;
    *data=state.soc;
    return 1U;
}

static UINT8 Storage_SaveSocAt(UINT32 base, const STORAGE_FLASH_SOC_DATA *data)
{
    STORAGE_STATE state;
    if (!data || data->u16FormatVersion!=FLASH_STORAGE_SOC_DATA_VERSION) return 0U;
    Storage_LoadState(base,&state); state.soc=*data; state.valid|=STORAGE_STATE_SOC;
    return Storage_SaveState(base,&state);
}

static UINT8 Storage_LoadFactoryAgingAt(UINT32 base, STORAGE_FLASH_FACTORY_AGING_DATA *data)
{
    STORAGE_STATE state;
    if (!data) return 0U;
    Storage_LoadState(base,&state);
    if (!(state.valid & STORAGE_STATE_AGING)) return 0U;
    *data=state.aging;
    return 1U;
}

static UINT8 Storage_SaveFactoryAgingAt(UINT32 base, const STORAGE_FLASH_FACTORY_AGING_DATA *data)
{
    STORAGE_STATE state;
    if (!data) return 0U;
    Storage_LoadState(base,&state); state.aging=*data; state.valid|=STORAGE_STATE_AGING;
    return Storage_SaveState(base,&state);
}

UINT8 StorageFlash_LoadSocData(STORAGE_FLASH_SOC_DATA *data)
{
    return Storage_LoadSocAt(FLASH_STATE_A,data);
}

UINT8 StorageFlash_SaveSocData(const STORAGE_FLASH_SOC_DATA *data)
{
    return Storage_SaveSocAt(FLASH_STATE_A,data);
}

UINT8 StorageFlash_LoadFactoryAgingData(STORAGE_FLASH_FACTORY_AGING_DATA *data)
{
    return Storage_LoadFactoryAgingAt(FLASH_STATE_A,data);
}

UINT8 StorageFlash_SaveFactoryAgingData(const STORAGE_FLASH_FACTORY_AGING_DATA *data)
{
    return Storage_SaveFactoryAgingAt(FLASH_STATE_A,data);
}

UINT8 StorageFlash_LoadAfeData(UINT16 *values, UINT16 word_count)
{
    STORAGE_FLASH_CONFIG_DATA config;
    if (!values || word_count!=FLASH_STORAGE_AFE_WORD_COUNT || !StorageFlash_LoadConfig(&config)) return 0U;
    memcpy(values,config.afe,sizeof(config.afe));
    return 1U;
}

UINT8 StorageFlash_SaveAfeData(const UINT16 *values, UINT16 word_count)
{
    STORAGE_FLASH_CONFIG_DATA config;
    if (!values || word_count!=FLASH_STORAGE_AFE_WORD_COUNT || !StorageFlash_LoadConfig(&config)) return 0U;
    memcpy(config.afe,values,sizeof(config.afe));
    return StorageFlash_SaveConfig(&config);
}

UINT8 StorageFlash_LoadRwParamData(STORAGE_FLASH_RW_PARAM_DATA *data)
{
    STORAGE_FLASH_CONFIG_DATA config;
    if (!data || !StorageFlash_LoadConfig(&config)) return 0U;
    *data=config.rw;
    return 1U;
}

UINT8 StorageFlash_SaveRwParamData(const STORAGE_FLASH_RW_PARAM_DATA *data)
{
    STORAGE_FLASH_CONFIG_DATA config;
    if (!data || !StorageFlash_LoadConfig(&config)) return 0U;
    /* A normal host parameter save must not erase private module versions. */
    memcpy(config.rw.protect,data->protect,sizeof(config.rw.protect));
    memcpy(config.rw.other,data->other,sizeof(config.rw.other));
    memcpy(config.rw.reserved,data->reserved,STORAGE_PARAM_VERSION_FIRST*sizeof(UINT16));
    memcpy(config.rw.heat_cool,data->heat_cool,sizeof(config.rw.heat_cool));
    return StorageFlash_SaveConfig(&config);
}

/* Each page starts with a committed checkpoint. This also preserves all 100
 * entries when repeated power cuts leave many incomplete append slots. */
#define STORAGE_LOG_MAGIC 0x4C4F4732UL
#define STORAGE_LOG_CLEAR 0xFFFFU
typedef struct { UINT8 point, reserved, records[100][2]; } STORAGE_LOG_CHECKPOINT;
#define STORAGE_LOG_START (sizeof(STORAGE_RECORD)+sizeof(STORAGE_LOG_CHECKPOINT))
typedef struct { UINT16 data, crc, commit; } STORAGE_LOG_ENTRY;
typedef char StorageLogCapacityCheck[((FLASH_STORAGE_PAGE_SIZE-STORAGE_LOG_START)/
                      sizeof(STORAGE_LOG_ENTRY)>FLASH_STORAGE_LOG_RECORD_COUNT) ? 1 : -1];

static UINT32 Storage_LogLatestPage(UINT32 base)
{
    return Storage_SelectPair(base,base+FLASH_STORAGE_PAGE_SIZE,STORAGE_LOG_MAGIC,sizeof(STORAGE_LOG_CHECKPOINT));
}

static UINT8 Storage_LogEntryValid(const STORAGE_LOG_ENTRY *entry)
{
    return (UINT8)(entry->commit==STORAGE_RECORD_COMMIT &&
        entry->crc==Storage_CrcUpdate(0xFFFFU,(const UINT8 *)&entry->data,2U));
}

static void Storage_LogReplay(UINT32 page, UINT8 *point, UINT8 records[100][2])
{
    UINT32 address;
    for (address=page+STORAGE_LOG_START;
         address+sizeof(STORAGE_LOG_ENTRY)<=page+FLASH_STORAGE_PAGE_SIZE;
         address+=sizeof(STORAGE_LOG_ENTRY)) {
        const STORAGE_LOG_ENTRY *entry=(const STORAGE_LOG_ENTRY *)address;
        if (!Storage_LogEntryValid(entry)) continue;
        if (entry->data==STORAGE_LOG_CLEAR) { memset(records,0,200U); *point=0U; }
        else {
            if (*point>=100U) *point=0U;
            records[*point][0]=(UINT8)entry->data;
            records[*point][1]=(UINT8)(entry->data>>8);
            ++*point;
        }
    }
}

static UINT8 Storage_LoadLogAt(UINT32 base, UINT8 *point, UINT8 records[100][2])
{
    UINT32 latest=Storage_LogLatestPage(base);
    const STORAGE_LOG_CHECKPOINT *checkpoint;
    if (!point || !records) return 0U;
    *point=0U; memset(records,0,200U);
    if (!latest) return 1U;
    checkpoint=(const STORAGE_LOG_CHECKPOINT *)(latest+sizeof(STORAGE_RECORD));
    if (checkpoint->point>100U) return 0U;
    *point=checkpoint->point;
    memcpy(records,checkpoint->records,200U);
    Storage_LogReplay(latest,point,records);
    return 1U;
}

static UINT8 Storage_LogAppend(UINT32 base, UINT16 data)
{
    UINT32 page=Storage_LogLatestPage(base), address, next, sequence=1U;
    STORAGE_LOG_CHECKPOINT checkpoint;
    STORAGE_LOG_ENTRY entry;
    UINT8 result=0U;
    StorageFlash_BeginWrite();
    if (!page) {
        page=base;
        memset(&checkpoint,0,sizeof(checkpoint));
        if (!Storage_Erase(page) || !Storage_WriteRecord(page,STORAGE_LOG_MAGIC,sequence,&checkpoint,sizeof(checkpoint))) goto done;
    }
    next=page+STORAGE_LOG_START;
    for (address=next; address+sizeof(entry)<=page+FLASH_STORAGE_PAGE_SIZE; address+=sizeof(entry))
        if (!StorageFlash_IsAreaBlank(address,sizeof(entry))) next=address+sizeof(entry);
    if (next+sizeof(entry)>page+FLASH_STORAGE_PAGE_SIZE) {
        sequence=((const STORAGE_RECORD *)page)->sequence+1U;
        memset(&checkpoint,0,sizeof(checkpoint));
        if (!Storage_LoadLogAt(base,&checkpoint.point,checkpoint.records)) goto done;
        page=page==base ? base+FLASH_STORAGE_PAGE_SIZE : base;
        if (!Storage_Erase(page) || !Storage_WriteRecord(page,STORAGE_LOG_MAGIC,sequence,&checkpoint,sizeof(checkpoint))) goto done;
        next=page+STORAGE_LOG_START;
    }
    entry.data=data; entry.crc=Storage_CrcUpdate(0xFFFFU,(const UINT8 *)&data,2U);
    entry.commit=STORAGE_RECORD_COMMIT;
    FLASH_Unlock();
    FLASH_ClearFlag(FLASH_FLAG_EOP | FLASH_FLAG_PGERR | FLASH_FLAG_WRPRTERR);
    (void)FlashProgramBytesVerified(next,(const UINT8 *)&entry,sizeof(entry));
    FLASH_Lock();
    result=(UINT8)(Storage_LogEntryValid((const STORAGE_LOG_ENTRY *)next) &&
                  memcmp((const void *)next,&entry,sizeof(entry))==0);
done:
    StorageFlash_EndWrite();
    if (!result) System_ERROR_UserCallback(ERROR_EEPROM_STORE);
    return result;
}

UINT8 StorageFlash_LoadLogData(UINT8 *point, UINT8 records[100][2])
{
    return Storage_LoadLogAt(FLASH_LOG_A,point,records);
}

UINT8 StorageFlash_AppendLog(UINT8 event, UINT8 delta)
{
    if (event==0xFFU || delta>171U) return 0U;
    return Storage_LogAppend(FLASH_LOG_A,(UINT16)(event | ((UINT16)delta<<8)));
}

UINT8 StorageFlash_ClearLog(void) { return Storage_LogAppend(FLASH_LOG_A,STORAGE_LOG_CLEAR); }

UINT8 StorageFlash_CommitParameterUpdate(STORAGE_FLASH_CONFIG_DATA *config)
{
    UINT32 address;
    STORAGE_STATE state;
    if (!config || (config->initializing & ~STORAGE_PARAM_RESET_MASK)) return 0U;
    /* Durable intent precedes all destructive erases. A reboot resumes this
     * operation before loading state or permitting normal AFE startup. */
    if (!StorageFlash_SaveConfig(config)) return 0U;
    if (!config->initializing) return 1U;
    if (config->initializing & STORAGE_PARAM_RESET_ALL) {
        StorageFlash_BeginWrite();
        for (address=FLASH_PARAMETER_RESET_BEGIN; address<FLASH_STATS_A; address+=FLASH_STORAGE_PAGE_SIZE) {
            if (address==FLASH_CONFIG_A || address==FLASH_CONFIG_B) continue;
            if (!Storage_Erase(address)) { StorageFlash_EndWrite(); return 0U; }
        }
        StorageFlash_EndWrite();
    } else {
        if (config->initializing & (STORAGE_PARAM_RESET_SOC | STORAGE_PARAM_RESET_AGING)) {
            Storage_LoadState(FLASH_STATE_A,&state);
            if (config->initializing & STORAGE_PARAM_RESET_SOC) {
                memset(&state.soc,0,sizeof(state.soc)); state.valid &= ~STORAGE_STATE_SOC;
            }
            if (config->initializing & STORAGE_PARAM_RESET_AGING) {
                memset(&state.aging,0,sizeof(state.aging)); state.valid &= ~STORAGE_STATE_AGING;
            }
            if (!Storage_SaveState(FLASH_STATE_A,&state)) return 0U;
        }
        if ((config->initializing & STORAGE_PARAM_RESET_LOG) && !StorageFlash_ClearLog()) return 0U;
    }
    config->initializing=0U;
    return StorageFlash_SaveConfig(config);
}

/* Lifetime statistics have their own pages and never depend on the log ring.
 * Falling edges must also be stored, so a continuous fault is not counted again
 * after reset or the customer's clear command. */
#define STORAGE_STATS_MAGIC 0x53545331UL
#define STORAGE_STATS_FAULT_BITS 0x03FFU
#define STORAGE_STATS_RETRY_TICKS 100U
enum { STATS_FAULT_MASK=1, STATS_BOOT, STATS_CLEAR };
typedef struct {
    UINT32 count[FLASH_STATISTICS_COUNT];
    UINT16 active_faults;
    UINT16 reserved;
} STORAGE_STATS_DATA;
typedef struct {
    UINT8 type, reserved;
    UINT16 value, crc, commit;
} STORAGE_STATS_ENTRY;
typedef struct {
    STORAGE_STATS_DATA data;
    UINT32 last_attempt_tick;
    UINT8 available, dirty;
} STORAGE_STATISTICS;
static STORAGE_STATISTICS s_statistics;
typedef char StorageStatsEntrySizeCheck[(sizeof(STORAGE_STATS_ENTRY)==8U) ? 1 : -1];

static UINT32 Storage_StatsPage(UINT32 base)
{
    return Storage_SelectPair(base,base+FLASH_STORAGE_PAGE_SIZE,STORAGE_STATS_MAGIC,sizeof(STORAGE_STATS_DATA));
}

static void Storage_StatsIncrement(UINT32 *count)
{
    if (*count!=0xFFFFFFFFUL) ++*count;
}

static void Storage_StatsApply(STORAGE_STATS_DATA *data, UINT8 type, UINT16 value)
{
    UINT8 i;
    if (type==STATS_FAULT_MASK) {
        UINT16 rising=(UINT16)(value & (UINT16)~data->active_faults);
        for (i=0U; i<FLASH_STATISTICS_COUNT-1U; ++i)
            if (rising & (1U<<i)) Storage_StatsIncrement(&data->count[i]);
        data->active_faults=value;
    } else if (type==STATS_BOOT) Storage_StatsIncrement(&data->count[10]);
    else if (type==STATS_CLEAR) memset(data->count,0,10U*sizeof(UINT32));
}

static UINT8 Storage_StatsEntryValid(const STORAGE_STATS_ENTRY *entry)
{
    return (UINT8)(entry->commit==STORAGE_RECORD_COMMIT && entry->reserved==0U &&
        entry->crc==Storage_CrcUpdate(0xFFFFU,(const UINT8 *)entry,4U) &&
        ((entry->type==STATS_FAULT_MASK && (entry->value & ~STORAGE_STATS_FAULT_BITS)==0U) ||
         ((entry->type==STATS_BOOT || entry->type==STATS_CLEAR) && entry->value==0U)));
}

static UINT8 Storage_StatsReplay(UINT32 page, STORAGE_STATISTICS *runtime)
{
    UINT32 address;
    memcpy(&runtime->data,(const void *)(page+sizeof(STORAGE_RECORD)),sizeof(runtime->data));
    if (runtime->data.active_faults & ~STORAGE_STATS_FAULT_BITS) return 0U;
    for (address=page+sizeof(STORAGE_RECORD)+sizeof(STORAGE_STATS_DATA);
         address+sizeof(STORAGE_STATS_ENTRY)<=page+FLASH_STORAGE_PAGE_SIZE;
         address+=sizeof(STORAGE_STATS_ENTRY)) {
        const STORAGE_STATS_ENTRY *entry=(const STORAGE_STATS_ENTRY *)address;
        if (entry->commit!=STORAGE_RECORD_COMMIT) continue; /* incomplete append */
        if (!Storage_StatsEntryValid(entry)) return 0U; /* damaged committed data */
        Storage_StatsApply(&runtime->data,entry->type,entry->value);
    }
    return 1U;
}

static UINT8 Storage_StatsSaveAt(UINT32 base, STORAGE_STATISTICS *runtime, UINT8 type, UINT16 value)
{
    UINT32 page=Storage_StatsPage(base), address, next=0U, sequence=1U;
    STORAGE_STATS_ENTRY entry;
    UINT8 result=0U;
    if (page) {
        sequence=((const STORAGE_RECORD *)page)->sequence+1U;
        next=page+sizeof(STORAGE_RECORD)+sizeof(STORAGE_STATS_DATA);
        for (address=next; address+sizeof(entry)<=page+FLASH_STORAGE_PAGE_SIZE; address+=sizeof(entry))
            if (!StorageFlash_IsAreaBlank(address,sizeof(entry))) next=address+sizeof(entry);
    }
    runtime->last_attempt_tick=SysTime_Get10msTickCount();
    StorageFlash_BeginWrite();
    /* RAM already includes the requested change. A checkpoint after an
     * uncertain write avoids replaying a delta twice or losing earlier faults. */
    if (!page || runtime->dirty || next+sizeof(entry)>page+FLASH_STORAGE_PAGE_SIZE) {
        address=page==base ? base+FLASH_STORAGE_PAGE_SIZE : base;
        result=(UINT8)(Storage_Erase(address) && Storage_WriteRecord(address,STORAGE_STATS_MAGIC,
                            sequence,&runtime->data,sizeof(runtime->data)));
    } else {
        entry.type=type; entry.reserved=0U; entry.value=value;
        entry.crc=Storage_CrcUpdate(0xFFFFU,(const UINT8 *)&entry,4U);
        entry.commit=STORAGE_RECORD_COMMIT;
        FLASH_Unlock();
        FLASH_ClearFlag(FLASH_FLAG_EOP | FLASH_FLAG_PGERR | FLASH_FLAG_WRPRTERR);
        (void)FlashProgramBytesVerified(next,(const UINT8 *)&entry,sizeof(entry));
        FLASH_Lock();
        result=(UINT8)(Storage_StatsEntryValid((const STORAGE_STATS_ENTRY *)next) &&
                       memcmp((const void *)next,&entry,sizeof(entry))==0);
    }
    StorageFlash_EndWrite();
    runtime->dirty=(UINT8)!result;
    if (!result) System_ERROR_UserCallback(ERROR_EEPROM_STORE);
    return result;
}

/* Recognize only a prefix of our known first initialization write. This allows
 * a first-install power cut to resume, without treating arbitrary damaged
 * lifetime data or an incompatible format as an empty statistics area. */
static UINT8 Storage_StatsInitialWriteInterrupted(void)
{
    STORAGE_RECORD header;
    STORAGE_STATS_DATA initial;
    const UINT16 *expected, *actual;
    UINT16 i, value;
    UINT8 blank_seen=0U;
    if (!StorageFlash_IsAreaBlank(FLASH_STATS_B,FLASH_STORAGE_PAGE_SIZE) ||
        FlashReadOneHalfWord(FLASH_STATS_A+14U)!=0xFFFFU ||
        !StorageFlash_IsAreaBlank(FLASH_STATS_A+sizeof(header)+sizeof(initial),
            FLASH_STORAGE_PAGE_SIZE-sizeof(header)-sizeof(initial))) return 0U;
    memset(&initial,0,sizeof(initial)); initial.count[10]=1U;
    header.magic=STORAGE_STATS_MAGIC; header.sequence=1U; header.length=sizeof(initial);
    header.format=STORAGE_RECORD_FORMAT; header.commit=0xFFFFU;
    header.crc=Storage_RecordCrc(&header,&initial);
    for (i=0U; i<7U+sizeof(initial)/2U; ++i) {
        if (i<7U) { expected=(const UINT16 *)&header+i; actual=(const UINT16 *)FLASH_STATS_A+i; }
        else { expected=(const UINT16 *)&initial+(i-7U); actual=(const UINT16 *)(FLASH_STATS_A+sizeof(header))+(i-7U); }
        value=*actual;
        if (value==0xFFFFU) blank_seen=1U;
        else if (blank_seen || value!=*expected) return 0U;
    }
    return 1U;
}

void StorageFlash_InitStatistics(UINT32 reset_cause, UINT16 sleep_flag)
{
    UINT32 page;
    UINT8 sleep_reset=(UINT8)((reset_cause & RCC_CSR_SFTRSTF)!=0U &&
        (reset_cause & (RCC_CSR_IWDGRSTF | RCC_CSR_WWDGRSTF | RCC_CSR_PORRSTF | RCC_CSR_LPWRRSTF))==0U &&
        (sleep_flag==FLASH_NORMAL_SLEEP_VALUE || sleep_flag==FLASH_HICCUP_SLEEP_VALUE ||
         sleep_flag==FLASH_DEEP_SLEEP_VALUE || sleep_flag==FLASH_SLEEP_CHARGER_WAKE_VALUE ||
         sleep_flag==FLASH_UNDERVOLTAGE_SLEEP_VALUE));
    memset(&s_statistics,0,sizeof(s_statistics));
    if (*((volatile UINT16 *)FLASH_SIZE_REG_ADDR)<128U) goto damaged;
    page=Storage_StatsPage(FLASH_STATS_A);
    if (page) {
        if (!Storage_StatsReplay(page,&s_statistics)) goto damaged;
        s_statistics.available=1U;
        if (!sleep_reset) {
            Storage_StatsApply(&s_statistics.data,STATS_BOOT,0U);
            (void)Storage_StatsSaveAt(FLASH_STATS_A,&s_statistics,STATS_BOOT,0U);
        }
    } else if ((StorageFlash_IsAreaBlank(FLASH_STATS_A,FLASH_STORAGE_PAGE_SIZE) &&
                StorageFlash_IsAreaBlank(FLASH_STATS_B,FLASH_STORAGE_PAGE_SIZE)) ||
               Storage_StatsInitialWriteInterrupted()) {
        s_statistics.available=1U;
        s_statistics.data.count[10]=1U;
        (void)Storage_StatsSaveAt(FLASH_STATS_A,&s_statistics,STATS_BOOT,0U);
    } else goto damaged;
    return;
damaged:
    System_ERROR_UserCallback(ERROR_EEPROM_STORE);
}

static void Storage_UpdateFaultStatisticsAt(UINT32 base, STORAGE_STATISTICS *runtime, UINT16 active_faults)
{
    if (!runtime->available || (active_faults & ~STORAGE_STATS_FAULT_BITS)!=0U ||
        active_faults==runtime->data.active_faults) return;
    Storage_StatsApply(&runtime->data,STATS_FAULT_MASK,active_faults);
    if (!runtime->dirty) (void)Storage_StatsSaveAt(base,runtime,STATS_FAULT_MASK,active_faults);
}

void StorageFlash_UpdateFaultStatistics(UINT16 active_faults)
{
    Storage_UpdateFaultStatisticsAt(FLASH_STATS_A,&s_statistics,active_faults);
}

UINT8 StorageFlash_ReadStatistics(UINT32 counts[FLASH_STATISTICS_COUNT])
{
    if (!counts || !s_statistics.available) return 0U;
    memcpy(counts,s_statistics.data.count,sizeof(s_statistics.data.count));
    return 1U;
}

UINT8 StorageFlash_ClearFaultStatistics(void)
{
    STORAGE_STATS_DATA previous;
    if (!s_statistics.available) return 0U;
    previous=s_statistics.data;
    Storage_StatsApply(&s_statistics.data,STATS_CLEAR,0U);
    if (Storage_StatsSaveAt(FLASH_STATS_A,&s_statistics,STATS_CLEAR,0U)) return 1U;
    s_statistics.data=previous;
    return 0U;
}

void StorageFlash_StatisticsTask(void)
{
    if (s_statistics.available && s_statistics.dirty &&
        (UINT32)(SysTime_Get10msTickCount()-s_statistics.last_attempt_tick)>=STORAGE_STATS_RETRY_TICKS)
        (void)Storage_StatsSaveAt(FLASH_STATS_A,&s_statistics,0U,0U);
}

void StorageFlash_FlushStatistics(void)
{
    if (s_statistics.available && s_statistics.dirty) (void)Storage_StatsSaveAt(FLASH_STATS_A,&s_statistics,0U,0U);
}

void StorageFlash_PrintBootCheck(void)
{
    printf("\r\n[FLASH_BOOT] flash=%uKB page=%lu config=0x%08lX state=0x%08lX log=0x%08lX\r\n",
        *((volatile UINT16 *)FLASH_SIZE_REG_ADDR),(unsigned long)FLASH_STORAGE_PAGE_SIZE,
        (unsigned long)Storage_SelectPair(FLASH_CONFIG_A,FLASH_CONFIG_B,STORAGE_CONFIG_MAGIC,sizeof(STORAGE_FLASH_CONFIG_DATA)),
        (unsigned long)Storage_StateLatest(FLASH_STATE_A),(unsigned long)Storage_LogLatestPage(FLASH_LOG_A));
}

/* Only the test build has a second set of data and a background runner.
 * Address arguments are private: production wrappers never select test pages. */
#if PROJECT_CFG_STORAGE_SELF_TEST_ENABLE
#define FLASH_TEST_SIGNATURE 0x54535431UL
#define FLASH_TEST_INTERVAL_TICKS 2U
#define FLASH_TEST_LOG_CLEAR_PERIOD 1024U
#define FLASH_TEST_BOOT_COUNT 17U
#define FLASH_TEST_LOG_CAPACITY ((FLASH_STORAGE_PAGE_SIZE-STORAGE_LOG_START)/sizeof(STORAGE_LOG_ENTRY))
#define FLASH_TEST_STATS_CAPACITY ((FLASH_STORAGE_PAGE_SIZE-sizeof(STORAGE_RECORD)-sizeof(STORAGE_STATS_DATA))/sizeof(STORAGE_STATS_ENTRY))
static const UINT32 s_self_test_goals[FLASH_TEST_DOMAIN_COUNT]={
    PROJECT_CFG_STORAGE_SELF_TEST_CONFIG_WRITES, PROJECT_CFG_STORAGE_SELF_TEST_SOC_WRITES,
    PROJECT_CFG_STORAGE_SELF_TEST_LOG_WRITES, PROJECT_CFG_STORAGE_SELF_TEST_STATS_WRITES
};
static UINT32 s_self_test_tick, s_self_test_ore;
static UINT16 s_self_test_overflow;
static UINT8 s_self_test_loaded, s_self_test_erase_page;
typedef char FlashTestProgressFits[(sizeof(FLASH_SELF_TEST_PROGRESS)<=sizeof(STORAGE_FLASH_RW_PARAM_DATA)) ? 1 : -1];
typedef char FlashTestAreaEnds[(FLASH_SELF_TEST_END==FLASH_STORAGE_BEGIN &&
    FLASH_SELF_TEST_BEGIN==FLASH_PARAMETER_RESET_BEGIN) ? 1 : -1];

static UINT32 FlashSelfTest_UartErrors(void)
{
#if PROJECT_CFG_IRQ_DEBUG_ENABLE
    return sys_time.irq.uart1.ore+sys_time.irq.uart2.ore+sys_time.irq.uart3.ore;
#else
    return 0U;
#endif
}

static void FlashSelfTest_MakeConfig(STORAGE_FLASH_CONFIG_DATA *config)
{
    FLASH_SELF_TEST_PROGRESS progress;
    memcpy(&progress,(const void *)&sys_time.flash_test.progress,sizeof(progress));
    memset(config,(UINT8)(progress.count[FLASH_TEST_CONFIG]^0x5AU),sizeof(*config));
    memcpy(&config->rw,&progress,sizeof(progress));
}

static UINT8 FlashSelfTest_SaveProgress(UINT8 duplicate)
{
    STORAGE_FLASH_CONFIG_DATA expected, actual;
    UINT32 before;
    FlashSelfTest_MakeConfig(&expected);
    if (!Storage_SaveConfigAt(FLASH_SELF_TEST_CONFIG,&expected) ||
        !Storage_LoadConfigAt(FLASH_SELF_TEST_CONFIG,&actual) || memcmp(&expected,&actual,sizeof(actual))) return 0U;
    ++sys_time.flash_test.checks;
    if (duplicate) {
        before=sys_time.flash_test.session_halfwords;
        if (!Storage_SaveConfigAt(FLASH_SELF_TEST_CONFIG,&expected) || before!=sys_time.flash_test.session_halfwords) return 0U;
        ++sys_time.flash_test.checks;
    }
    return 1U;
}

static UINT8 FlashSelfTest_Advance(UINT32 phase)
{
    sys_time.flash_test.progress.phase=phase;
    return FlashSelfTest_SaveProgress(0U);
}

static UINT8 FlashSelfTest_StateValid(void)
{
    UINT32 page,address;
    for (page=FLASH_SELF_TEST_STATE;page<FLASH_SELF_TEST_STATE+2U*FLASH_STORAGE_PAGE_SIZE;page+=FLASH_STORAGE_PAGE_SIZE)
        for (address=page;address+STORAGE_STATE_SPAN<=page+FLASH_STORAGE_PAGE_SIZE;address+=STORAGE_STATE_SPAN)
            if (!StorageFlash_IsAreaBlank(address,STORAGE_STATE_SPAN) &&
                !Storage_RecordValid(address,STORAGE_STATE_MAGIC,sizeof(STORAGE_STATE))) return 0U;
    return 1U;
}

static UINT8 FlashSelfTest_LoadProgress(void)
{
    STORAGE_FLASH_CONFIG_DATA actual, expected;
    FLASH_SELF_TEST_PROGRESS progress;
    UINT8 i;
    for (i=0U;i<2U;++i) {
        UINT32 address=FLASH_SELF_TEST_CONFIG+(UINT32)i*FLASH_STORAGE_PAGE_SIZE;
        if (!StorageFlash_IsAreaBlank(address,FLASH_STORAGE_PAGE_SIZE) &&
            !Storage_RecordValid(address,STORAGE_CONFIG_MAGIC,sizeof(actual))) return 0U;
    }
    if (!Storage_LoadConfigAt(FLASH_SELF_TEST_CONFIG,&actual)) {
        if (!StorageFlash_IsAreaBlank(FLASH_SELF_TEST_BEGIN,FLASH_SELF_TEST_END-FLASH_SELF_TEST_BEGIN)) return 0U;
        return FlashSelfTest_SaveProgress(0U);
    }
    memcpy(&progress,&actual.rw,sizeof(progress));
    if (progress.signature!=FLASH_TEST_SIGNATURE || progress.phase>FLASH_TEST_COMPLETE ||
        memcmp(progress.goal,s_self_test_goals,sizeof(progress.goal))) {
        FlashSelfTest_Fail(FLASH_TEST_PROFILE_ERROR,FLASH_SELF_TEST_CONFIG); return 0U;
    }
    for (i=0U;i<FLASH_TEST_DOMAIN_COUNT;++i)
        if (progress.count[i]>progress.goal[i]) return 0U;
    memcpy((void *)&sys_time.flash_test.progress,&progress,sizeof(progress));
    FlashSelfTest_MakeConfig(&expected);
    if (memcmp(&actual,&expected,sizeof(actual))) return 0U;
    sys_time.flash_test.resumed=1U;
    return 1U;
}

static void FlashSelfTest_MakeSoc(STORAGE_FLASH_SOC_DATA *soc, UINT32 ordinal)
{
    UINT32 step=ordinal%200U;
    memset(soc,0,sizeof(*soc));
    soc->u16FormatVersion=FLASH_STORAGE_SOC_DATA_VERSION;
    soc->u16SocNow=(UINT16)(step<=100U ? step : 200U-step);
    soc->u16MaxErrorPercent=100U;
    soc->u32CycleTimes=(ordinal/200U)*100U+(step>100U ? step-100U : 0U);
    soc->u32CapFull=360000U;
    soc->u32CapNow=soc->u32CapFull*soc->u16SocNow/100U;
    soc->u32LearnPassedAs10=(ordinal%100U)*36U;
    soc->u16DsgSocInt=(UINT16)(ordinal%100U);
    soc->u16Flags=(UINT16)((ordinal/200U)&1U);
    /* A test-only token in existing reserved bytes makes restart validation
     * unambiguous. It is never written to the production SOC pair. */
    memcpy(soc->u16Reserved,&ordinal,sizeof(ordinal));
}

static UINT8 FlashSelfTest_CheckSoc(UINT32 ordinal)
{
    STORAGE_FLASH_SOC_DATA actual, expected;
    FlashSelfTest_MakeSoc(&expected,ordinal);
    if (!Storage_LoadSocAt(FLASH_SELF_TEST_STATE,&actual)) return 0U;
    ++sys_time.flash_test.checks;
    return (UINT8)(memcmp(&actual,&expected,sizeof(actual))==0);
}

static UINT8 FlashSelfTest_SocStep(void)
{
    STORAGE_FLASH_SOC_DATA soc;
    STORAGE_FLASH_FACTORY_AGING_DATA aging, actual_aging;
    UINT32 ordinal=0U, before;
    if (!FlashSelfTest_StateValid()) return 0U;
    if (Storage_LoadSocAt(FLASH_SELF_TEST_STATE,&soc)) {
        memcpy(&ordinal,soc.u16Reserved,sizeof(ordinal));
        if (!ordinal || ordinal>s_self_test_goals[FLASH_TEST_SOC] || !FlashSelfTest_CheckSoc(ordinal)) return 0U;
    } else if (sys_time.flash_test.progress.count[FLASH_TEST_SOC]) return 0U;
    sys_time.flash_test.progress.count[FLASH_TEST_SOC]=ordinal;
    aging.u32Elapsed10ms=ordinal/200U;
    aging.u16State=FLASH_FACTORY_AGING_STATE_RUNNING; aging.u16DurationHours=12U;
    if (!Storage_LoadFactoryAgingAt(FLASH_SELF_TEST_STATE,&actual_aging)) {
        if (ordinal || !StorageFlash_IsAreaBlank(FLASH_SELF_TEST_STATE,2U*FLASH_STORAGE_PAGE_SIZE)) return 0U;
    } else if (memcmp(&aging,&actual_aging,sizeof(aging))) {
        if (!ordinal || ordinal%200U || actual_aging.u32Elapsed10ms+1U!=aging.u32Elapsed10ms ||
            actual_aging.u16State!=aging.u16State || actual_aging.u16DurationHours!=aging.u16DurationHours) return 0U;
    } else goto aging_ready;
    {
        if (!Storage_SaveFactoryAgingAt(FLASH_SELF_TEST_STATE,&aging) ||
            !Storage_LoadFactoryAgingAt(FLASH_SELF_TEST_STATE,&actual_aging) ||
            memcmp(&aging,&actual_aging,sizeof(aging))) return 0U;
        return (UINT8)(ordinal==0U || FlashSelfTest_CheckSoc(ordinal));
    }
aging_ready:
    if (ordinal==s_self_test_goals[FLASH_TEST_SOC]) return FlashSelfTest_Advance(FLASH_TEST_LOG_PHASE);
    FlashSelfTest_MakeSoc(&soc,ordinal+1U);
    if (!Storage_SaveSocAt(FLASH_SELF_TEST_STATE,&soc) || !FlashSelfTest_CheckSoc(ordinal+1U) ||
        !Storage_LoadFactoryAgingAt(FLASH_SELF_TEST_STATE,&actual_aging) || memcmp(&aging,&actual_aging,sizeof(aging))) return 0U;
    sys_time.flash_test.progress.count[FLASH_TEST_SOC]=ordinal+1U;
    if (((ordinal+1U)%64U)==0U) {
        before=sys_time.flash_test.session_halfwords;
        if (!Storage_SaveSocAt(FLASH_SELF_TEST_STATE,&soc) || before!=sys_time.flash_test.session_halfwords) return 0U;
    }
    return 1U;
}

static UINT16 FlashSelfTest_LogValue(UINT32 ordinal)
{
    if ((ordinal%FLASH_TEST_LOG_CLEAR_PERIOD)==0U) return STORAGE_LOG_CLEAR;
    return (UINT16)((VCELL_OVP+ordinal%10U) | (((ordinal/10U)%172U)<<8));
}

static UINT8 FlashSelfTest_LogCount(UINT32 *count)
{
    UINT32 page=Storage_LogLatestPage(FLASH_SELF_TEST_LOG), address, entries=0U, generation;
    if (!page) {
        *count=0U;
        return StorageFlash_IsAreaBlank(FLASH_SELF_TEST_LOG,2U*FLASH_STORAGE_PAGE_SIZE);
    }
    generation=((const STORAGE_RECORD *)page)->sequence;
    if (!generation || generation>1U+s_self_test_goals[FLASH_TEST_LOG]/FLASH_TEST_LOG_CAPACITY) return 0U;
    for (address=page+STORAGE_LOG_START;address+sizeof(STORAGE_LOG_ENTRY)<=page+FLASH_STORAGE_PAGE_SIZE;address+=sizeof(STORAGE_LOG_ENTRY)) {
        if (StorageFlash_IsAreaBlank(address,sizeof(STORAGE_LOG_ENTRY))) continue;
        /* The self-test never hides a torn slot as a successful test write. */
        if (!Storage_LogEntryValid((const STORAGE_LOG_ENTRY *)address)) return 0U;
        ++entries;
    }
    *count=(generation-1U)*FLASH_TEST_LOG_CAPACITY+entries;
    return (UINT8)(*count<=s_self_test_goals[FLASH_TEST_LOG]);
}

static UINT8 FlashSelfTest_CheckLog(UINT32 count)
{
    UINT8 point, records[100][2];
    UINT16 i,index,value;
    UINT32 available=count%FLASH_TEST_LOG_CLEAR_PERIOD;
    if (!Storage_LoadLogAt(FLASH_SELF_TEST_LOG,&point,records) || point>100U) return 0U;
    if (available>100U) available=100U;
    for (i=0U;i<100U;++i) {
        index=(UINT16)((point+99U-i)%100U);
        value=i<available ? FlashSelfTest_LogValue(count-i) : 0U;
        if (records[index][0]!=(UINT8)value || records[index][1]!=(UINT8)(value>>8)) return 0U;
    }
    ++sys_time.flash_test.checks;
    return 1U;
}

static UINT8 FlashSelfTest_LogStep(void)
{
    UINT32 count;
    if (!FlashSelfTest_LogCount(&count) || !FlashSelfTest_CheckLog(count)) return 0U;
    sys_time.flash_test.progress.count[FLASH_TEST_LOG]=count;
    if (count==s_self_test_goals[FLASH_TEST_LOG]) return FlashSelfTest_Advance(FLASH_TEST_STATS_PHASE);
    if (!Storage_LogAppend(FLASH_SELF_TEST_LOG,FlashSelfTest_LogValue(count+1U)) || !FlashSelfTest_CheckLog(count+1U)) return 0U;
    sys_time.flash_test.progress.count[FLASH_TEST_LOG]=count+1U;
    return 1U;
}

static UINT8 FlashSelfTest_ReadStats(STORAGE_STATISTICS *runtime)
{
    UINT32 page=Storage_StatsPage(FLASH_SELF_TEST_STATS),address;
    memset(runtime,0,sizeof(*runtime));
    if (!page || !Storage_StatsReplay(page,runtime)) return 0U;
    for (address=page+sizeof(STORAGE_RECORD)+sizeof(STORAGE_STATS_DATA);
         address+sizeof(STORAGE_STATS_ENTRY)<=page+FLASH_STORAGE_PAGE_SIZE;address+=sizeof(STORAGE_STATS_ENTRY))
        if (!StorageFlash_IsAreaBlank(address,sizeof(STORAGE_STATS_ENTRY)) &&
            !Storage_StatsEntryValid((const STORAGE_STATS_ENTRY *)address)) return 0U;
    runtime->available=1U;
    ++sys_time.flash_test.checks;
    return 1U;
}

static void FlashSelfTest_MakeStats(STORAGE_STATS_DATA *expected, UINT32 ordinal, UINT32 boots)
{
    UINT8 i;
    UINT32 rises=(ordinal+1U)/2U;
    memset(expected,0,sizeof(*expected));
    for (i=0U;i<10U;++i) expected->count[i]=(rises+9U-i)/10U;
    expected->count[10]=boots;
    expected->active_faults=(UINT16)((ordinal&1U) ? (1U<<((ordinal/2U)%10U)) : 0U);
}

static UINT8 FlashSelfTest_StatsStep(void)
{
    STORAGE_STATISTICS runtime, reloaded;
    STORAGE_STATS_DATA expected;
    UINT32 ordinal=0U,rises=0U,boots;
    UINT8 i;
    UINT32 phase=sys_time.flash_test.progress.phase;
    if (!FlashSelfTest_ReadStats(&runtime)) {
        if (phase!=FLASH_TEST_STATS_PHASE || sys_time.flash_test.progress.count[FLASH_TEST_STATS] ||
            !StorageFlash_IsAreaBlank(FLASH_SELF_TEST_STATS,2U*FLASH_STORAGE_PAGE_SIZE)) return 0U;
        memset(&runtime,0,sizeof(runtime)); runtime.available=1U; runtime.data.count[10]=1U;
        return Storage_StatsSaveAt(FLASH_SELF_TEST_STATS,&runtime,STATS_BOOT,0U);
    }
    boots=runtime.data.count[10];
    if (phase==FLASH_TEST_STATS_PHASE) {
        for (i=0U;i<10U;++i) {
            if (runtime.data.count[i]>s_self_test_goals[FLASH_TEST_STATS]-rises) return 0U;
            rises+=runtime.data.count[i];
        }
        if (rises>s_self_test_goals[FLASH_TEST_STATS] || (rises==0U && runtime.data.active_faults)) return 0U;
        ordinal=2U*rises-(runtime.data.active_faults ? 1U : 0U);
        FlashSelfTest_MakeStats(&expected,ordinal,1U);
        if (ordinal>s_self_test_goals[FLASH_TEST_STATS] || memcmp(&expected,&runtime.data,sizeof(expected))) return 0U;
        sys_time.flash_test.progress.count[FLASH_TEST_STATS]=ordinal;
        if (ordinal==s_self_test_goals[FLASH_TEST_STATS]) return FlashSelfTest_Advance(FLASH_TEST_BOOT_PHASE);
        FlashSelfTest_MakeStats(&expected,ordinal+1U,1U);
        Storage_UpdateFaultStatisticsAt(FLASH_SELF_TEST_STATS,&runtime,expected.active_faults);
        if (runtime.dirty || !FlashSelfTest_ReadStats(&reloaded) || memcmp(&expected,&reloaded.data,sizeof(expected))) return 0U;
        sys_time.flash_test.progress.count[FLASH_TEST_STATS]=ordinal+1U;
    } else if (phase==FLASH_TEST_BOOT_PHASE) {
        FlashSelfTest_MakeStats(&expected,s_self_test_goals[FLASH_TEST_STATS],boots);
        if (!boots || boots>FLASH_TEST_BOOT_COUNT || memcmp(&expected,&runtime.data,sizeof(expected))) return 0U;
        if (boots==FLASH_TEST_BOOT_COUNT) return FlashSelfTest_Advance(FLASH_TEST_CLEAR_PHASE);
        Storage_StatsApply(&runtime.data,STATS_BOOT,0U);
        if (!Storage_StatsSaveAt(FLASH_SELF_TEST_STATS,&runtime,STATS_BOOT,0U)) return 0U;
    } else if (phase==FLASH_TEST_CLEAR_PHASE) {
        FlashSelfTest_MakeStats(&expected,s_self_test_goals[FLASH_TEST_STATS],FLASH_TEST_BOOT_COUNT);
        if (!memcmp(&expected,&runtime.data,sizeof(expected)) && expected.active_faults!=STORAGE_STATS_FAULT_BITS) {
            Storage_UpdateFaultStatisticsAt(FLASH_SELF_TEST_STATS,&runtime,STORAGE_STATS_FAULT_BITS);
            if (runtime.dirty) return 0U;
        } else {
            Storage_StatsApply(&expected,STATS_FAULT_MASK,STORAGE_STATS_FAULT_BITS);
            if (!memcmp(&expected,&runtime.data,sizeof(expected))) {
                Storage_StatsApply(&runtime.data,STATS_CLEAR,0U);
                if (!Storage_StatsSaveAt(FLASH_SELF_TEST_STATS,&runtime,STATS_CLEAR,0U)) return 0U;
            } else {
                Storage_StatsApply(&expected,STATS_CLEAR,0U);
                if (memcmp(&expected,&runtime.data,sizeof(expected))) return 0U;
                /* Same active fault after clear must neither count nor write. */
                ordinal=sys_time.flash_test.session_halfwords;
                Storage_UpdateFaultStatisticsAt(FLASH_SELF_TEST_STATS,&runtime,STORAGE_STATS_FAULT_BITS);
                if (ordinal!=sys_time.flash_test.session_halfwords) return 0U;
                return FlashSelfTest_Advance(FLASH_TEST_RECOVER_PHASE);
            }
        }
    } else {
        memset(&expected,0,sizeof(expected)); expected.count[10]=FLASH_TEST_BOOT_COUNT;
        expected.active_faults=STORAGE_STATS_FAULT_BITS;
        if (!memcmp(&expected,&runtime.data,sizeof(expected))) {
            Storage_UpdateFaultStatisticsAt(FLASH_SELF_TEST_STATS,&runtime,0U);
        } else {
            expected.active_faults=0U;
            if (!memcmp(&expected,&runtime.data,sizeof(expected))) Storage_UpdateFaultStatisticsAt(FLASH_SELF_TEST_STATS,&runtime,1U);
            else {
                expected.count[0]=1U; expected.active_faults=1U;
                if (memcmp(&expected,&runtime.data,sizeof(expected))) return 0U;
                return FlashSelfTest_Advance(FLASH_TEST_FINAL_PHASE);
            }
        }
        if (runtime.dirty) return 0U;
    }
    if (!FlashSelfTest_ReadStats(&reloaded) || memcmp(&runtime.data,&reloaded.data,sizeof(runtime.data))) return 0U;
    return 1U;
}

static UINT8 FlashSelfTest_CheckFinal(void)
{
    STORAGE_STATISTICS statistics;
    STORAGE_STATS_DATA expected;
    STORAGE_FLASH_FACTORY_AGING_DATA aging;
    UINT32 count;
    UINT8 i;
    for (i=0U;i<FLASH_TEST_DOMAIN_COUNT;++i)
        if (sys_time.flash_test.progress.count[i]!=s_self_test_goals[i]) return 0U;
    if (!FlashSelfTest_CheckSoc(s_self_test_goals[FLASH_TEST_SOC]) ||
        !Storage_LoadFactoryAgingAt(FLASH_SELF_TEST_STATE,&aging) ||
        aging.u32Elapsed10ms!=s_self_test_goals[FLASH_TEST_SOC]/200U ||
        aging.u16State!=FLASH_FACTORY_AGING_STATE_RUNNING || aging.u16DurationHours!=12U ||
        !FlashSelfTest_LogCount(&count) || count!=s_self_test_goals[FLASH_TEST_LOG] ||
        !FlashSelfTest_CheckLog(count) || !FlashSelfTest_ReadStats(&statistics)) return 0U;
    memset(&expected,0,sizeof(expected)); expected.count[0]=1U;
    expected.count[10]=FLASH_TEST_BOOT_COUNT; expected.active_faults=1U;
    return (UINT8)(memcmp(&expected,&statistics.data,sizeof(expected))==0);
}

void FlashSelfTest_Init(uint32_t reset_cause)
{
    volatile FLASH_SELF_TEST *test=&sys_time.flash_test;
    memset((void *)test,0,sizeof(*test));
    test->status=FLASH_TEST_RUNNING;
    test->progress.signature=FLASH_TEST_SIGNATURE;
    memcpy((void *)test->progress.goal,s_self_test_goals,sizeof(s_self_test_goals));
    s_self_test_loaded=0U; s_self_test_erase_page=2U;
    s_self_test_tick=SysTime_Get10msTickCount();
    s_self_test_ore=FlashSelfTest_UartErrors();
    s_self_test_overflow=SysTime_Get200msTaskOverflowCount();
    if (*((volatile UINT16 *)FLASH_SIZE_REG_ADDR)<128U) {
        FlashSelfTest_Fail(FLASH_TEST_PROFILE_ERROR,FLASH_SIZE_REG_ADDR); return;
    }
    /* Upper bounds include initialization, aging updates and phase checkpoints.
     * This is a cycle-count budget, not a ten-year retention certificate. */
    test->budget_ok=(UINT8)(s_self_test_goals[FLASH_TEST_CONFIG]/2U+16U<FLASH_TEST_ERASE_LIMIT &&
        (s_self_test_goals[FLASH_TEST_SOC]+s_self_test_goals[FLASH_TEST_SOC]/200U+2U)/
            (2U*(FLASH_STORAGE_PAGE_SIZE/STORAGE_STATE_SPAN))+2U<FLASH_TEST_ERASE_LIMIT &&
        s_self_test_goals[FLASH_TEST_LOG]/(2U*FLASH_TEST_LOG_CAPACITY)+2U<FLASH_TEST_ERASE_LIMIT &&
        s_self_test_goals[FLASH_TEST_STATS]/(2U*FLASH_TEST_STATS_CAPACITY)+8U<FLASH_TEST_ERASE_LIMIT);
    if (!test->budget_ok) FlashSelfTest_Fail(FLASH_TEST_BUDGET_ERROR,FLASH_SELF_TEST_BEGIN);
    else if (reset_cause & (RCC_CSR_IWDGRSTF | RCC_CSR_WWDGRSTF))
        FlashSelfTest_Fail(FLASH_TEST_WATCHDOG_ERROR,0U);
}

uint8_t FlashSelfTest_IsActive(void)
{
    return (uint8_t)(sys_time.flash_test.status==FLASH_TEST_RUNNING);
}

void FlashSelfTest_Task(void)
{
    volatile FLASH_SELF_TEST *test=&sys_time.flash_test;
    UINT32 now=SysTime_Get10msTickCount(), phase;
    UINT8 ok=1U;
    if (!FlashSelfTest_IsActive() || g_stLowPowerRtcStatus.mode!=NO_SLEEP) return;
    if (s_self_test_ore!=FlashSelfTest_UartErrors()) {
        FlashSelfTest_Fail(FLASH_TEST_UART_ERROR,0U); return;
    }
    if (s_self_test_overflow!=SysTime_Get200msTaskOverflowCount()) {
        FlashSelfTest_Fail(FLASH_TEST_SCHEDULER_ERROR,0U); return;
    }
    if ((UINT32)(now-s_self_test_tick)<FLASH_TEST_INTERVAL_TICKS || StorageFlash_IsBusy() ||
        u8FlashUpdateFlag || Sci_IsAnyPortBusy() || Can_IsBusy() || SysTime_HasPendingTaskFlags() ||
        System_ERROR_UserCallback(ERROR_STATUS_AFE1)) return;
    s_self_test_tick=now;
    if (!s_self_test_loaded) {
        if (!FlashSelfTest_LoadProgress()) FlashSelfTest_Fail(FLASH_TEST_READ_ERROR,FLASH_SELF_TEST_CONFIG);
        else s_self_test_loaded=1U;
        return;
    }
    phase=test->progress.phase;
    switch (phase) {
    case FLASH_TEST_PREPARE:
        if (s_self_test_erase_page<8U) {
            StorageFlash_BeginWrite();
            ok=Storage_Erase(FLASH_SELF_TEST_BEGIN+(UINT32)s_self_test_erase_page*FLASH_STORAGE_PAGE_SIZE);
            StorageFlash_EndWrite();
            if (ok) ++s_self_test_erase_page;
        } else ok=FlashSelfTest_Advance(FLASH_TEST_CONFIG_PHASE);
        break;
    case FLASH_TEST_CONFIG_PHASE:
        if (test->progress.count[FLASH_TEST_CONFIG]==s_self_test_goals[FLASH_TEST_CONFIG])
            ok=FlashSelfTest_Advance(FLASH_TEST_SOC_PHASE);
        else {
            ++test->progress.count[FLASH_TEST_CONFIG];
            ok=FlashSelfTest_SaveProgress((UINT8)((test->progress.count[FLASH_TEST_CONFIG]%64U)==0U));
            if (!ok) --test->progress.count[FLASH_TEST_CONFIG];
        }
        break;
    case FLASH_TEST_SOC_PHASE: ok=FlashSelfTest_SocStep(); break;
    case FLASH_TEST_LOG_PHASE: ok=FlashSelfTest_LogStep(); break;
    case FLASH_TEST_STATS_PHASE:
    case FLASH_TEST_BOOT_PHASE:
    case FLASH_TEST_CLEAR_PHASE:
    case FLASH_TEST_RECOVER_PHASE: ok=FlashSelfTest_StatsStep(); break;
    case FLASH_TEST_FINAL_PHASE:
    case FLASH_TEST_COMPLETE:
        ok=FlashSelfTest_CheckFinal();
        if (ok && phase!=FLASH_TEST_COMPLETE) ok=FlashSelfTest_Advance(FLASH_TEST_COMPLETE);
        if (ok && test->status==FLASH_TEST_RUNNING) test->status=FLASH_TEST_PASS;
        break;
    default: ok=0U; break;
    }
    if (!ok && test->status==FLASH_TEST_RUNNING) FlashSelfTest_Fail(FLASH_TEST_VERIFY_ERROR,FLASH_SELF_TEST_BEGIN);
}
#endif

UINT16 FlashReadOneHalfWord(UINT32 faddr)
{
	return *(vu16 *)faddr;
}

static volatile APP_UPGRADE_MAILBOX *AppUpgrade_Mailbox(void)
{
	return (volatile APP_UPGRADE_MAILBOX *)APP_UPGRADE_MAILBOX_ADDR;
}

static UINT32 AppUpgrade_MailboxCrc(UINT32 magic, UINT32 request)
{
	return magic ^ request ^ 0xA5A55A5A;
}

static UINT8 AppUpgrade_IsIapRequested(void)
{
	volatile APP_UPGRADE_MAILBOX *mailbox;

	mailbox = AppUpgrade_Mailbox();
	if ((mailbox->magic != APP_UPGRADE_MAILBOX_MAGIC) ||
		(mailbox->magic_inv != (UINT32)~APP_UPGRADE_MAILBOX_MAGIC) ||
		(mailbox->request != APP_UPGRADE_MAILBOX_REQUEST) ||
		(mailbox->request_inv != (UINT32)~APP_UPGRADE_MAILBOX_REQUEST) ||
		(mailbox->crc != AppUpgrade_MailboxCrc(APP_UPGRADE_MAILBOX_MAGIC, APP_UPGRADE_MAILBOX_REQUEST)))
	{
		return 0U;
	}

	return 1U;
}

UINT8 AppUpgrade_RequestIap(void)
{
	volatile APP_UPGRADE_MAILBOX *mailbox;

	mailbox = AppUpgrade_Mailbox();
	mailbox->magic = APP_UPGRADE_MAILBOX_MAGIC;
	mailbox->magic_inv = (UINT32)~APP_UPGRADE_MAILBOX_MAGIC;
	mailbox->request = APP_UPGRADE_MAILBOX_REQUEST;
	mailbox->request_inv = (UINT32)~APP_UPGRADE_MAILBOX_REQUEST;
	mailbox->crc = AppUpgrade_MailboxCrc(APP_UPGRADE_MAILBOX_MAGIC, APP_UPGRADE_MAILBOX_REQUEST);

	return AppUpgrade_IsIapRequested();
}

void App_FlashUpdate(void)
{
	if (1 == u8FlashUpdateFlag)
	{
		SH367309_DriverMos_Ctrl(GPIO_CHG, 0);
		SH367309_DriverMos_Ctrl(GPIO_DSG, 0);
		__delay_ms(10);
		u8FlashUpdateFlag = 0;
		__disable_fault_irq();
		MCU_RESET();
	}
}

void APP_To_IAP_Jump(void)
{
	if (AppUpgrade_RequestIap() != 0U)
	{
		__disable_fault_irq();
		MCU_RESET();
	}
}

void InitAreaSelect(void)
{
	if (AppUpgrade_IsIapRequested() != 0U)
	{
		__disable_fault_irq();
		MCU_RESET();
	}
}
