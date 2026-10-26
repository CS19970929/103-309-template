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

static FLASH_Status FlashErasePageVerified(uint32_t page_addr)
{
	UINT32 offset;
	FLASH_Status result;

	result = FLASH_ErasePage(page_addr);
	if (result != FLASH_COMPLETE)
	{
		return result;
	}

	for (offset = 0; offset < FLASH_STORAGE_PAGE_SIZE; offset += 2)
	{
		if (FlashReadOneHalfWord(page_addr + offset) != 0xFFFF)
		{
			return FLASH_ERROR_PG;
		}
	}

	return FLASH_COMPLETE;
}

static FLASH_Status FlashProgramHalfWordVerified(uint32_t addr, uint16_t data)
{
	FLASH_Status result;

	result = FLASH_ProgramHalfWord(addr, data);
	if (result != FLASH_COMPLETE)
	{
		return result;
	}

	if (FlashReadOneHalfWord(addr) != data)
	{
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

UINT8 StorageFlash_LoadConfig(STORAGE_FLASH_CONFIG_DATA *data)
{
    UINT32 address;
    if (data==0) return 0U;
    address=Storage_SelectPair(FLASH_CONFIG_A,FLASH_CONFIG_B,STORAGE_CONFIG_MAGIC,sizeof(*data));
    if (!address) return 0U;
    memcpy(data,(const void *)(address+sizeof(STORAGE_RECORD)),sizeof(*data));
    return 1U;
}

UINT8 StorageFlash_SaveConfig(const STORAGE_FLASH_CONFIG_DATA *data)
{
    UINT32 address, target, sequence=1U;
    UINT8 result;
    if (data==0) return 0U;
    address=Storage_SelectPair(FLASH_CONFIG_A,FLASH_CONFIG_B,STORAGE_CONFIG_MAGIC,sizeof(*data));
    if (address) {
        if (memcmp((const void *)(address+sizeof(STORAGE_RECORD)),data,sizeof(*data))==0) return 1U;
        sequence=((const STORAGE_RECORD *)address)->sequence+1U;
    }
    target=address==FLASH_CONFIG_A ? FLASH_CONFIG_B : FLASH_CONFIG_A;
    StorageFlash_BeginWrite();
    result=(UINT8)(Storage_Erase(target) &&
        Storage_WriteRecord(target,STORAGE_CONFIG_MAGIC,sequence,data,sizeof(*data)));
    StorageFlash_EndWrite();
    if (!result) System_ERROR_UserCallback(ERROR_EEPROM_STORE);
    return result;
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

static UINT32 Storage_StateLatest(void)
{
    UINT32 page, address, latest=0U;
    for (page=FLASH_STATE_A; page<=FLASH_STATE_B; page+=FLASH_STORAGE_PAGE_SIZE) {
        for (address=page; address+STORAGE_STATE_SPAN<=page+FLASH_STORAGE_PAGE_SIZE;
             address+=STORAGE_STATE_SPAN) {
            if (Storage_RecordValid(address,STORAGE_STATE_MAGIC,sizeof(STORAGE_STATE)) &&
                (!latest || Storage_Newer(((const STORAGE_RECORD *)address)->sequence,
                                         ((const STORAGE_RECORD *)latest)->sequence))) latest=address;
        }
    }
    return latest;
}

static void Storage_LoadState(STORAGE_STATE *state)
{
    UINT32 address=Storage_StateLatest();
    memset(state,0,sizeof(*state));
    if (address) memcpy(state,(const void *)(address+sizeof(STORAGE_RECORD)),sizeof(*state));
}

static UINT8 Storage_SaveState(const STORAGE_STATE *state)
{
    UINT32 latest=Storage_StateLatest(), page=FLASH_STATE_A, address, next;
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
        next=page==FLASH_STATE_A ? FLASH_STATE_B : FLASH_STATE_A;
        if (!Storage_Erase(next)) goto done;
    }
    result=Storage_WriteRecord(next,STORAGE_STATE_MAGIC,sequence,state,sizeof(*state));
done:
    StorageFlash_EndWrite();
    if (!result) System_ERROR_UserCallback(ERROR_EEPROM_STORE);
    return result;
}

UINT8 StorageFlash_LoadSocData(STORAGE_FLASH_SOC_DATA *data)
{
    STORAGE_STATE state;
    if (!data) return 0U;
    Storage_LoadState(&state);
    if (!(state.valid & STORAGE_STATE_SOC) || state.soc.u16FormatVersion!=FLASH_STORAGE_SOC_DATA_VERSION) return 0U;
    *data=state.soc;
    return 1U;
}

UINT8 StorageFlash_SaveSocData(const STORAGE_FLASH_SOC_DATA *data)
{
    STORAGE_STATE state;
    if (!data || data->u16FormatVersion!=FLASH_STORAGE_SOC_DATA_VERSION) return 0U;
    Storage_LoadState(&state); state.soc=*data; state.valid|=STORAGE_STATE_SOC;
    return Storage_SaveState(&state);
}

UINT8 StorageFlash_LoadFactoryAgingData(STORAGE_FLASH_FACTORY_AGING_DATA *data)
{
    STORAGE_STATE state;
    if (!data) return 0U;
    Storage_LoadState(&state);
    if (!(state.valid & STORAGE_STATE_AGING)) return 0U;
    *data=state.aging;
    return 1U;
}

UINT8 StorageFlash_SaveFactoryAgingData(const STORAGE_FLASH_FACTORY_AGING_DATA *data)
{
    STORAGE_STATE state;
    if (!data) return 0U;
    Storage_LoadState(&state); state.aging=*data; state.valid|=STORAGE_STATE_AGING;
    return Storage_SaveState(&state);
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
    config.rw=*data;
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

static UINT32 Storage_LogLatestPage(void)
{
    return Storage_SelectPair(FLASH_LOG_A,FLASH_LOG_B,STORAGE_LOG_MAGIC,sizeof(STORAGE_LOG_CHECKPOINT));
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

UINT8 StorageFlash_LoadLogData(UINT8 *point, UINT8 records[100][2])
{
    UINT32 latest=Storage_LogLatestPage();
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

static UINT8 Storage_LogAppend(UINT16 data)
{
    UINT32 page=Storage_LogLatestPage(), address, next, sequence=1U;
    STORAGE_LOG_CHECKPOINT checkpoint;
    STORAGE_LOG_ENTRY entry;
    UINT8 result=0U;
    StorageFlash_BeginWrite();
    if (!page) {
        page=FLASH_LOG_A;
        memset(&checkpoint,0,sizeof(checkpoint));
        if (!Storage_Erase(page) || !Storage_WriteRecord(page,STORAGE_LOG_MAGIC,sequence,&checkpoint,sizeof(checkpoint))) goto done;
    }
    next=page+STORAGE_LOG_START;
    for (address=next; address+sizeof(entry)<=page+FLASH_STORAGE_PAGE_SIZE; address+=sizeof(entry))
        if (!StorageFlash_IsAreaBlank(address,sizeof(entry))) next=address+sizeof(entry);
    if (next+sizeof(entry)>page+FLASH_STORAGE_PAGE_SIZE) {
        sequence=((const STORAGE_RECORD *)page)->sequence+1U;
        memset(&checkpoint,0,sizeof(checkpoint));
        if (!StorageFlash_LoadLogData(&checkpoint.point,checkpoint.records)) goto done;
        page=page==FLASH_LOG_A ? FLASH_LOG_B : FLASH_LOG_A;
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

UINT8 StorageFlash_AppendLog(UINT8 event, UINT8 delta)
{
    if (event==0xFFU || delta>171U) return 0U;
    return Storage_LogAppend((UINT16)(event | ((UINT16)delta<<8)));
}

UINT8 StorageFlash_ClearLog(void) { return Storage_LogAppend(STORAGE_LOG_CLEAR); }

UINT8 StorageFlash_ResetParameters(STORAGE_FLASH_CONFIG_DATA *defaults)
{
    UINT32 address;
    if (!defaults) return 0U;
    /* Durable intent precedes all destructive erases. A reboot resumes this
     * operation before loading state or permitting normal AFE startup. */
    defaults->initializing=1U;
    if (!StorageFlash_SaveConfig(defaults)) return 0U;
    StorageFlash_BeginWrite();
    for (address=FLASH_PARAMETER_RESET_BEGIN; address<FLASH_STATS_A; address+=FLASH_STORAGE_PAGE_SIZE) {
        if (address==FLASH_CONFIG_A || address==FLASH_CONFIG_B) continue;
        if (!Storage_Erase(address)) { StorageFlash_EndWrite(); return 0U; }
    }
    StorageFlash_EndWrite();
    defaults->initializing=0U;
    return StorageFlash_SaveConfig(defaults);
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
static struct {
    STORAGE_STATS_DATA data;
    UINT32 last_attempt_tick;
    UINT8 available, dirty;
} s_statistics;
typedef char StorageStatsEntrySizeCheck[(sizeof(STORAGE_STATS_ENTRY)==8U) ? 1 : -1];

static UINT32 Storage_StatsPage(void)
{
    return Storage_SelectPair(FLASH_STATS_A,FLASH_STATS_B,STORAGE_STATS_MAGIC,sizeof(STORAGE_STATS_DATA));
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

static UINT8 Storage_StatsReplay(UINT32 page)
{
    UINT32 address;
    memcpy(&s_statistics.data,(const void *)(page+sizeof(STORAGE_RECORD)),sizeof(s_statistics.data));
    if (s_statistics.data.active_faults & ~STORAGE_STATS_FAULT_BITS) return 0U;
    for (address=page+sizeof(STORAGE_RECORD)+sizeof(STORAGE_STATS_DATA);
         address+sizeof(STORAGE_STATS_ENTRY)<=page+FLASH_STORAGE_PAGE_SIZE;
         address+=sizeof(STORAGE_STATS_ENTRY)) {
        const STORAGE_STATS_ENTRY *entry=(const STORAGE_STATS_ENTRY *)address;
        if (entry->commit!=STORAGE_RECORD_COMMIT) continue; /* incomplete append */
        if (!Storage_StatsEntryValid(entry)) return 0U; /* damaged committed data */
        Storage_StatsApply(&s_statistics.data,entry->type,entry->value);
    }
    return 1U;
}

static UINT8 Storage_StatsSave(UINT8 type, UINT16 value)
{
    UINT32 page=Storage_StatsPage(), address, next=0U, sequence=1U;
    STORAGE_STATS_ENTRY entry;
    UINT8 result=0U;
    if (page) {
        sequence=((const STORAGE_RECORD *)page)->sequence+1U;
        next=page+sizeof(STORAGE_RECORD)+sizeof(STORAGE_STATS_DATA);
        for (address=next; address+sizeof(entry)<=page+FLASH_STORAGE_PAGE_SIZE; address+=sizeof(entry))
            if (!StorageFlash_IsAreaBlank(address,sizeof(entry))) next=address+sizeof(entry);
    }
    s_statistics.last_attempt_tick=SysTime_Get10msTickCount();
    StorageFlash_BeginWrite();
    /* RAM already includes the requested change. A checkpoint after an
     * uncertain write avoids replaying a delta twice or losing earlier faults. */
    if (!page || s_statistics.dirty || next+sizeof(entry)>page+FLASH_STORAGE_PAGE_SIZE) {
        address=page==FLASH_STATS_A ? FLASH_STATS_B : FLASH_STATS_A;
        result=(UINT8)(Storage_Erase(address) && Storage_WriteRecord(address,STORAGE_STATS_MAGIC,
                            sequence,&s_statistics.data,sizeof(s_statistics.data)));
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
    s_statistics.dirty=(UINT8)!result;
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
    page=Storage_StatsPage();
    if (page) {
        if (!Storage_StatsReplay(page)) goto damaged;
        s_statistics.available=1U;
        if (!sleep_reset) {
            Storage_StatsApply(&s_statistics.data,STATS_BOOT,0U);
            (void)Storage_StatsSave(STATS_BOOT,0U);
        }
    } else if ((StorageFlash_IsAreaBlank(FLASH_STATS_A,FLASH_STORAGE_PAGE_SIZE) &&
                StorageFlash_IsAreaBlank(FLASH_STATS_B,FLASH_STORAGE_PAGE_SIZE)) ||
               Storage_StatsInitialWriteInterrupted()) {
        s_statistics.available=1U;
        s_statistics.data.count[10]=1U;
        (void)Storage_StatsSave(STATS_BOOT,0U);
    } else goto damaged;
    return;
damaged:
    System_ERROR_UserCallback(ERROR_EEPROM_STORE);
}

void StorageFlash_UpdateFaultStatistics(UINT16 active_faults)
{
    if (!s_statistics.available || (active_faults & ~STORAGE_STATS_FAULT_BITS)!=0U ||
        active_faults==s_statistics.data.active_faults) return;
    Storage_StatsApply(&s_statistics.data,STATS_FAULT_MASK,active_faults);
    if (!s_statistics.dirty) (void)Storage_StatsSave(STATS_FAULT_MASK,active_faults);
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
    if (Storage_StatsSave(STATS_CLEAR,0U)) return 1U;
    s_statistics.data=previous;
    return 0U;
}

void StorageFlash_StatisticsTask(void)
{
    if (s_statistics.available && s_statistics.dirty &&
        (UINT32)(SysTime_Get10msTickCount()-s_statistics.last_attempt_tick)>=STORAGE_STATS_RETRY_TICKS)
        (void)Storage_StatsSave(0U,0U);
}

void StorageFlash_FlushStatistics(void)
{
    if (s_statistics.available && s_statistics.dirty) (void)Storage_StatsSave(0U,0U);
}

void StorageFlash_PrintBootCheck(void)
{
    printf("\r\n[FLASH_BOOT] flash=%uKB page=%lu config=0x%08lX state=0x%08lX log=0x%08lX\r\n",
        *((volatile UINT16 *)FLASH_SIZE_REG_ADDR),(unsigned long)FLASH_STORAGE_PAGE_SIZE,
        (unsigned long)Storage_SelectPair(FLASH_CONFIG_A,FLASH_CONFIG_B,STORAGE_CONFIG_MAGIC,sizeof(STORAGE_FLASH_CONFIG_DATA)),
        (unsigned long)Storage_StateLatest(),(unsigned long)Storage_LogLatestPage());
}

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
