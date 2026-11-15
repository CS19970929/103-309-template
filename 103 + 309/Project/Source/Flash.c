#include "main.h"
#include "Heat_Cool.h"
#include <stddef.h>

/* On-Flash layout. Keep field order, widths and reserved-word meanings. */
#define FLASH_STORAGE_RW_PARAM_RESERVED_WORD_COUNT 24
#define FLASH_STORAGE_RW_PARAM_HEAT_COOL_WORD_COUNT 24
/* reserved[0..5] hold current calibration; [6..14] are private update tags.
 * Do not reorder groups: their positions are part of the stored format. */
#define STORAGE_PARAM_VERSION_FIRST 6U
/* Single explicit update generation and group mask; retain the stored layout. */
#define STORAGE_UPDATE_GENERATION_INDEX STORAGE_PARAM_VERSION_FIRST
#define STORAGE_UPDATE_TAG_INDEX (STORAGE_PARAM_VERSION_FIRST + 1U)
#define STORAGE_UPDATE_TAG 0x5355U
/* Private fields fit the existing 24-word reserved area; record size is unchanged. */
#define STORAGE_BAUD_MODE_INDEX 15U
#define STORAGE_BATTERY_NUMBER_INDEX 16U
#define STORAGE_BATTERY_NUMBER_TAG_INDEX 22U
#define STORAGE_BATTERY_NUMBER_TAG 0x424EU
/* Existing private words link a committed cycle command to its SOC snapshot. */
#define STORAGE_SOC_CYCLE_GENERATION_INDEX 23U
enum STORAGE_PARAM_GROUP {
    STORAGE_PARAM_AFE, STORAGE_PARAM_PROTECT, STORAGE_PARAM_SOC,
    STORAGE_PARAM_OTHER, STORAGE_PARAM_CALIB, STORAGE_PARAM_HEAT_COOL,
    STORAGE_PARAM_SOC_STATE, STORAGE_PARAM_LOG,
    STORAGE_PARAM_COUNT
};
#define STORAGE_PARAM_RESET_ALL   1U
#define STORAGE_PARAM_RESET_SOC   2U
#define STORAGE_PARAM_RESET_RESERVED 4U /* Old pending bit; no aging behavior. */
#define STORAGE_PARAM_RESET_LOG   8U
#define STORAGE_PARAM_RESET_MASK 15U

typedef struct
{
	UINT16 protect[E2P_PARA_NUM_PROTECT];
	UINT16 other[E2P_PARA_NUM_OTHER_ELEMENT1];
	UINT16 reserved[FLASH_STORAGE_RW_PARAM_RESERVED_WORD_COUNT];
	UINT16 heat_cool[FLASH_STORAGE_RW_PARAM_HEAT_COOL_WORD_COUNT];
} STORAGE_FLASH_RW_PARAM_DATA;

typedef struct {
    UINT16 afe[FLASH_STORAGE_AFE_WORD_COUNT];
    STORAGE_FLASH_RW_PARAM_DATA rw;
    UINT8 reserved_legacy[32]; /* Retain offset/CRC, unused by current firmware. */
    UINT16 initializing; /* Durable pending STORAGE_PARAM_RESET_* bits. */
} STORAGE_FLASH_CONFIG_DATA;

static UINT16 FlashReadOneHalfWord(UINT32 faddr);

/* Watchable last hardware error: 1 erase, 2 erase verify,
 * 3 program, 4 program verify, 5 record verify. Zero means no error. */
static volatile struct {
    UINT32 address;
    UINT16 status;
    UINT8 stage;
} s_flash_error;

#define FLASH_SIZE_REG_ADDR ((UINT32)0x1FFFF7E0)

#define APP_UPGRADE_MAILBOX_ADDR ((UINT32)0x20004FE0)
#define APP_UPGRADE_MAILBOX_MAGIC ((UINT32)0x49415031)
#define APP_UPGRADE_MAILBOX_REQUEST ((UINT32)0x5AA55AA5)
typedef struct { UINT32 magic, magic_inv, request, request_inv, crc; } APP_UPGRADE_MAILBOX;
static FLASH_Status FlashErasePageVerified(uint32_t page_addr)
{
	UINT32 offset;
	FLASH_Status result;

	result = FLASH_ErasePage(page_addr);
	if (result != FLASH_COMPLETE)
	{
		s_flash_error.address=page_addr;
		s_flash_error.status=(UINT16)result;
		s_flash_error.stage=1U;
		return result;
	}

	for (offset = 0; offset < FLASH_STORAGE_PAGE_SIZE; offset += 2)
	{
		if (FlashReadOneHalfWord(page_addr + offset) != 0xFFFF)
		{
			s_flash_error.address=page_addr+offset;
			s_flash_error.status=(UINT16)FLASH_ERROR_PG;
			s_flash_error.stage=2U;
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
		s_flash_error.address=addr;
		s_flash_error.status=(UINT16)result;
		s_flash_error.stage=3U;
		return result;
	}

	if (FlashReadOneHalfWord(addr) != data)
	{
		s_flash_error.address=addr;
		s_flash_error.status=(UINT16)FLASH_ERROR_PG;
		s_flash_error.stage=4U;
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

typedef char StorageRwLayoutCheck[(sizeof(STORAGE_FLASH_RW_PARAM_DATA)==290U) ? 1 : -1];
typedef char StorageConfigLayoutCheck[(sizeof(STORAGE_FLASH_CONFIG_DATA)==372U &&
    offsetof(STORAGE_FLASH_CONFIG_DATA,rw)==48U &&
    offsetof(STORAGE_FLASH_CONFIG_DATA,reserved_legacy)==338U &&
    offsetof(STORAGE_FLASH_CONFIG_DATA,initializing)==370U) ? 1 : -1];
typedef char StorageSocLayoutCheck[(sizeof(STORAGE_FLASH_SOC_DATA)==40U &&
    offsetof(STORAGE_FLASH_SOC_DATA,u32CycleTimes)==8U &&
    offsetof(STORAGE_FLASH_SOC_DATA,u16Reserved)==30U) ? 1 : -1];
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
    if (Storage_RecordValid(address,magic,length) &&
        memcmp((const void *)(address+sizeof(h)),payload,length)==0) return 1U;
    if (result==FLASH_COMPLETE) {
        s_flash_error.address=address;
        s_flash_error.status=(UINT16)FLASH_ERROR_PG;
        s_flash_error.stage=5U;
    }
    return 0U;
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

/* CRC/commit prove integrity; safe application also requires legal parameters. */
static UINT8 Storage_ConfigValid(const STORAGE_FLASH_CONFIG_DATA *config);

static UINT32 Storage_SelectConfig(void)
{
    const STORAGE_FLASH_CONFIG_DATA *a=(const void *)(FLASH_CONFIG_A+sizeof(STORAGE_RECORD));
    const STORAGE_FLASH_CONFIG_DATA *b=(const void *)(FLASH_CONFIG_B+sizeof(STORAGE_RECORD));
    UINT8 va=Storage_RecordValid(FLASH_CONFIG_A,STORAGE_CONFIG_MAGIC,sizeof(*a));
    UINT8 vb=Storage_RecordValid(FLASH_CONFIG_B,STORAGE_CONFIG_MAGIC,sizeof(*b));
    UINT32 latest=Storage_SelectPair(FLASH_CONFIG_A,FLASH_CONFIG_B,
                                      STORAGE_CONFIG_MAGIC,sizeof(*a));
    /* Only the newest committed pending transaction can block recovery. */
    if (latest) {
        const STORAGE_FLASH_CONFIG_DATA *pending=(const void *)(latest+sizeof(STORAGE_RECORD));
        if (pending->initializing && !Storage_ConfigValid(pending)) return 0U;
    }
    if (va && !Storage_ConfigValid(a)) va=0U;
    if (vb && !Storage_ConfigValid(b)) vb=0U;
    if (!va) return vb ? FLASH_CONFIG_B : 0U;
    if (!vb) return FLASH_CONFIG_A;
    return Storage_Newer(((const STORAGE_RECORD *)FLASH_CONFIG_B)->sequence,
                         ((const STORAGE_RECORD *)FLASH_CONFIG_A)->sequence) ?
                         FLASH_CONFIG_B : FLASH_CONFIG_A;
}

static UINT8 Storage_LoadConfig(STORAGE_FLASH_CONFIG_DATA *data)
{
    UINT32 address=Storage_SelectConfig();
    if (!data || !address) return 0U;
    memcpy(data,(const void *)(address+sizeof(STORAGE_RECORD)),sizeof(*data));
    return 1U;
}

static UINT8 Storage_SaveConfig(const STORAGE_FLASH_CONFIG_DATA *data)
{
    UINT32 address, target, sequence=1U;
    UINT8 result;
    if (!data || !Storage_ConfigValid(data)) {
        System_ERROR_UserCallback(ERROR_EEPROM_STORE);
        return 0U;
    }
    address=Storage_SelectConfig();
    if (address) {
        if (memcmp((const void *)(address+sizeof(STORAGE_RECORD)),data,sizeof(*data))==0) return 1U;
        sequence=((const STORAGE_RECORD *)address)->sequence+1U;
    }
    target=address==FLASH_CONFIG_A ? FLASH_CONFIG_B : FLASH_CONFIG_A;

    result=(UINT8)(Storage_Erase(target) &&
        Storage_WriteRecord(target,STORAGE_CONFIG_MAGIC,sequence,data,sizeof(*data)));

    if (!result) System_ERROR_UserCallback(ERROR_EEPROM_STORE);
    return result;
}

UINT16 StorageFlash_GetSocCycleGeneration(void)
{
    STORAGE_FLASH_CONFIG_DATA config;
    if (!Storage_LoadConfig(&config)) return STORAGE_SOC_CYCLE_GENERATION_NONE;
    return config.rw.reserved[STORAGE_SOC_CYCLE_GENERATION_INDEX];
}

UINT16 StorageFlash_GetBaudMode(void)
{
    STORAGE_FLASH_CONFIG_DATA config;
    UINT16 mode;
    if (!Storage_LoadConfig(&config)) return 1U;
    mode=config.rw.reserved[STORAGE_BAUD_MODE_INDEX];
    return (mode>=1U && mode<=3U) ? mode : 1U;
}

UINT8 StorageFlash_SetBaudMode(UINT16 mode)
{
    STORAGE_FLASH_CONFIG_DATA config;
    if (mode<1U || mode>3U || !Storage_LoadConfig(&config) || config.initializing)
        return 0U;
    config.rw.reserved[STORAGE_BAUD_MODE_INDEX]=mode;
    return Storage_SaveConfig(&config);
}

UINT8 StorageFlash_ReadBatteryNumber(UINT8 number[BATTERY_NUMBER_LENGTH])
{
    STORAGE_FLASH_CONFIG_DATA config;
    UINT16 i, value;
    if (!number || !Storage_LoadConfig(&config)) return 0U;
    if (config.rw.reserved[STORAGE_BATTERY_NUMBER_TAG_INDEX]==0xFFFFU) {
        memset(number,'0',BATTERY_NUMBER_LENGTH);
        return 1U;
    }
    if (config.rw.reserved[STORAGE_BATTERY_NUMBER_TAG_INDEX]!=STORAGE_BATTERY_NUMBER_TAG)
        return 0U;
    for (i=0U; i<BATTERY_NUMBER_LENGTH; ++i) {
        value=config.rw.reserved[STORAGE_BATTERY_NUMBER_INDEX+i/2U];
        number[i]=(UINT8)((i & 1U) ? value : value>>8);
        if (number[i]<0x20U || number[i]>0x7EU) return 0U;
    }
    return 1U;
}

UINT8 StorageFlash_WriteBatteryNumber(const UINT8 number[BATTERY_NUMBER_LENGTH])
{
    STORAGE_FLASH_CONFIG_DATA config;
    UINT16 i;
    if (!number) return 0U;
    for (i=0U; i<BATTERY_NUMBER_LENGTH; ++i)
        if (number[i]<0x20U || number[i]>0x7EU) return 0U;
    if (!Storage_LoadConfig(&config) || config.initializing) return 0U;
    for (i=0U; i<6U; ++i)
        config.rw.reserved[STORAGE_BATTERY_NUMBER_INDEX+i]=(UINT16)(
            ((UINT16)number[2U*i]<<8) | (i<5U ? number[2U*i+1U] : 0xFFU));
    config.rw.reserved[STORAGE_BATTERY_NUMBER_TAG_INDEX]=STORAGE_BATTERY_NUMBER_TAG;
    return Storage_SaveConfig(&config);
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
typedef struct {
    STORAGE_FLASH_SOC_DATA soc;
    UINT8 reserved_aging[8]; /* Preserve current record offsets, no aging logic. */
    UINT16 valid;
    UINT16 reserved;
} STORAGE_STATE;
#define STORAGE_STATE_SPAN (sizeof(STORAGE_RECORD)+sizeof(STORAGE_STATE))
typedef char StorageStateLayoutCheck[(sizeof(STORAGE_STATE)==52U && STORAGE_STATE_SPAN==68U) ? 1 : -1];

static UINT32 Storage_StateLatest(void)
{
    UINT32 page, address, latest=0U;
    for (page=FLASH_STATE_A; page<FLASH_STATE_A+2U*FLASH_STORAGE_PAGE_SIZE; page+=FLASH_STORAGE_PAGE_SIZE) {
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

    if (next+STORAGE_STATE_SPAN>page+FLASH_STORAGE_PAGE_SIZE) {
        next=page==FLASH_STATE_A ? FLASH_STATE_B : FLASH_STATE_A;
        if (!Storage_Erase(next)) goto done;
    }
    result=Storage_WriteRecord(next,STORAGE_STATE_MAGIC,sequence,state,sizeof(*state));
done:

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

static UINT8 Storage_ResumeSocCycleWrite(const STORAGE_FLASH_CONFIG_DATA *config)
{
    STORAGE_STATE state;
    UINT16 generation;
    if (!config) return 0U;
    generation=config->rw.reserved[STORAGE_SOC_CYCLE_GENERATION_INDEX];
    if (generation==STORAGE_SOC_CYCLE_GENERATION_NONE) return 1U;
    Storage_LoadState(&state);
    if (!(state.valid & STORAGE_STATE_SOC) ||
        state.soc.u16FormatVersion!=FLASH_STORAGE_SOC_DATA_VERSION ||
        state.soc.u16Reserved[STORAGE_SOC_CYCLE_SNAPSHOT_INDEX]==generation) return 1U;
    /* Finish the committed command before module defaults can change its value. */
    state.soc.u32CycleTimes=(UINT32)config->rw.other[STORAGE_SOC_CYCLE_PARAMETER_INDEX]*100U;
    state.soc.u32LearnPassedAs10=0U;
    state.soc.u16DsgSocInt=0U;
    state.soc.u16Reserved[STORAGE_SOC_CYCLE_SNAPSHOT_INDEX]=generation;
    return Storage_SaveState(&state);
}

UINT8 StorageFlash_SaveAfeData(const UINT16 *values, UINT16 word_count)
{
    STORAGE_FLASH_CONFIG_DATA config;
    if (!values || word_count!=FLASH_STORAGE_AFE_WORD_COUNT || !Storage_LoadConfig(&config)) return 0U;
    memcpy(config.afe,values,sizeof(config.afe));
    return Storage_SaveConfig(&config);
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

    if (!result) System_ERROR_UserCallback(ERROR_EEPROM_STORE);
    return result;
}

UINT8 StorageFlash_AppendLog(UINT8 event, UINT8 delta)
{
    if (event==0xFFU || delta>171U) return 0U;
    return Storage_LogAppend((UINT16)(event | ((UINT16)delta<<8)));
}

UINT8 StorageFlash_ClearLog(void) { return Storage_LogAppend(STORAGE_LOG_CLEAR); }

static UINT8 Storage_CommitParameterUpdate(STORAGE_FLASH_CONFIG_DATA *config)
{
    UINT32 address;
    STORAGE_STATE state;
    if (!config || (config->initializing & ~STORAGE_PARAM_RESET_MASK)) return 0U;
    /* Durable intent precedes all destructive erases. A reboot resumes this
     * operation before loading state or permitting normal AFE startup. */
    if (!Storage_SaveConfig(config)) return 0U;
    if (!config->initializing) return 1U;
    if (config->initializing & STORAGE_PARAM_RESET_ALL) {

        for (address=FLASH_PARAMETER_RESET_BEGIN; address<FLASH_STATS_A; address+=FLASH_STORAGE_PAGE_SIZE) {
            if (address==FLASH_CONFIG_A || address==FLASH_CONFIG_B) continue;
            if (!Storage_Erase(address)) { return 0U; }
        }

    } else {
        if (config->initializing & STORAGE_PARAM_RESET_SOC) {
            Storage_LoadState(&state);
            memset(&state.soc,0,sizeof(state.soc));
            state.valid &= (UINT16)~STORAGE_STATE_SOC;
            if (!Storage_SaveState(&state)) return 0U;
        }
        if ((config->initializing & STORAGE_PARAM_RESET_LOG) && !StorageFlash_ClearLog()) return 0U;
    }
    config->initializing=0U;
    return Storage_SaveConfig(config);
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

static UINT8 Storage_SaveStatistics(UINT8 type, UINT16 value)
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
        if (!Storage_StatsReplay(page,&s_statistics)) goto damaged;
        s_statistics.available=1U;
        if (!sleep_reset) {
            Storage_StatsApply(&s_statistics.data,STATS_BOOT,0U);
            (void)Storage_SaveStatistics(STATS_BOOT,0U);
        }
    } else if ((StorageFlash_IsAreaBlank(FLASH_STATS_A,FLASH_STORAGE_PAGE_SIZE) &&
                StorageFlash_IsAreaBlank(FLASH_STATS_B,FLASH_STORAGE_PAGE_SIZE)) ||
               Storage_StatsInitialWriteInterrupted()) {
        s_statistics.available=1U;
        s_statistics.data.count[10]=1U;
        (void)Storage_SaveStatistics(STATS_BOOT,0U);
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
    if (!s_statistics.dirty) (void)Storage_SaveStatistics(STATS_FAULT_MASK,active_faults);
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
    if (Storage_SaveStatistics(STATS_CLEAR,0U)) return 1U;
    s_statistics.data=previous;
    return 0U;
}

void StorageFlash_StatisticsTask(void)
{
    if (s_statistics.available && s_statistics.dirty &&
        (UINT32)(SysTime_Get10msTickCount()-s_statistics.last_attempt_tick)>=STORAGE_STATS_RETRY_TICKS)
        (void)Storage_SaveStatistics(0U,0U);
}

void StorageFlash_FlushStatistics(void)
{
    if (s_statistics.available && s_statistics.dirty) (void)Storage_SaveStatistics(0U,0U);
}

/* Parameter defaults and runtime application. */
struct HEAT_COOL_ELEMENT Heat_Cool_Element;

static const struct PRT_E2ROM_PARAS s_protect_default = E2P_PROTECT_DEFAULT_PRT;
static const struct PRT_E2ROM_PARAS s_protect_min = E2P_PROTECT_MIN_PRT;
static const struct PRT_E2ROM_PARAS s_protect_max = E2P_PROTECT_MAX_PRT;
static const struct OTHER_ELEMENT s_other_default = OtherElement_default;
static const struct OTHER_ELEMENT s_other_min = OtherElement_min;
static const struct OTHER_ELEMENT s_other_max = OtherElement_max;
static const struct HEAT_COOL_ELEMENT s_heat_cool_default = HeatCoolElement_Default;
static const struct HEAT_COOL_ELEMENT s_heat_cool_min = HeatCoolElement_Min;
static const struct HEAT_COOL_ELEMENT s_heat_cool_max = HeatCoolElement_Max;

typedef char StorageProtectSizeCheck[(sizeof(s_protect_default)==E2P_PARA_NUM_PROTECT*sizeof(UINT16)) ? 1 : -1];
typedef char StorageOtherSizeCheck[(sizeof(s_other_default)==E2P_PARA_NUM_OTHER_ELEMENT1*sizeof(UINT16)) ? 1 : -1];
typedef char StorageHeatCoolSizeCheck[(sizeof(s_heat_cool_default)==FLASH_STORAGE_RW_PARAM_HEAT_COOL_WORD_COUNT*sizeof(UINT16)) ? 1 : -1];
typedef char StorageParamVersionSizeCheck[(STORAGE_PARAM_VERSION_FIRST+STORAGE_PARAM_COUNT<=FLASH_STORAGE_RW_PARAM_RESERVED_WORD_COUNT) ? 1 : -1];
typedef char StorageCycleOffsetCheck[(offsetof(struct OTHER_ELEMENT,u16Soc_Cycle_times)/sizeof(UINT16)==STORAGE_SOC_CYCLE_PARAMETER_INDEX) ? 1 : -1];


/*
 * Store current K/B in existing reserved words so the RW parameter record size
 * includes both charge and discharge calibration.
 */
#define RW_PARAM_CURRENT_CAL_MAGIC_INDEX       ((UINT16)0U)
#define RW_PARAM_CURRENT_CAL_VERSION_INDEX     ((UINT16)1U)
#define RW_PARAM_CURRENT_CAL_CHG_K_INDEX       ((UINT16)2U)
#define RW_PARAM_CURRENT_CAL_CHG_B_INDEX       ((UINT16)3U)
#define RW_PARAM_CURRENT_CAL_DSG_K_INDEX       ((UINT16)4U)
#define RW_PARAM_CURRENT_CAL_DSG_B_INDEX       ((UINT16)5U)
#define RW_PARAM_CURRENT_CAL_MAGIC             ((UINT16)0x4349U)
#define RW_PARAM_CURRENT_CAL_VERSION           ((UINT16)3U)

static const UINT16 s_current_calibration_default[STORAGE_PARAM_VERSION_FIRST] = {
    RW_PARAM_CURRENT_CAL_MAGIC, RW_PARAM_CURRENT_CAL_VERSION,
    SYSKDEFAULT, (UINT16)SYSBDEFAULT, SYSKDEFAULT, (UINT16)SYSBDEFAULT
};

static UINT8 Storage_CurrentCalibrationValid(UINT16 chg_k, INT16 chg_b,
                                            UINT16 dsg_k, INT16 dsg_b)
{
	if ((chg_k < CURRENT_CAL_K_MIN) || (chg_k > CURRENT_CAL_K_MAX) ||
		(dsg_k < CURRENT_CAL_K_MIN) || (dsg_k > CURRENT_CAL_K_MAX))
	{
		return 0U;
	}
	if ((chg_b < CURRENT_CAL_B_MIN_Q10_A) || (chg_b > CURRENT_CAL_B_MAX_Q10_A) ||
		(dsg_b < CURRENT_CAL_B_MIN_Q10_A) || (dsg_b > CURRENT_CAL_B_MAX_Q10_A))
	{
		return 0U;
	}
	return 1U;
}

static UINT8 Storage_WordsInRange(const UINT16 *values, const UINT16 *min_values, const UINT16 *max_values, UINT16 count)
{
	UINT16 i;

	for (i = 0; i < count; ++i)
	{
		if ((values[i] < min_values[i]) || (values[i] > max_values[i]))
		{
			return 0;
		}
	}

	return 1;
}

static UINT8 Storage_RWParametersValid(const STORAGE_FLASH_RW_PARAM_DATA *data)
{
	if (!Storage_WordsInRange(data->protect,
								 &s_protect_min.u16VcellOvp_First,
								 &s_protect_max.u16VcellOvp_First,
								 E2P_PARA_NUM_PROTECT))
	{
		return 0;
	}
	if (!Storage_WordsInRange(data->other,
								 &s_other_min.u16Balance_OpenVoltage,
								 &s_other_max.u16Balance_OpenVoltage,
								 E2P_PARA_NUM_OTHER_ELEMENT1))
	{
		return 0;
	}
	if (!Storage_WordsInRange(data->heat_cool,
								 &s_heat_cool_min.u16Heat_OpenTemp,
								 &s_heat_cool_max.u16Heat_OpenTemp,
								 FLASH_STORAGE_RW_PARAM_HEAT_COOL_WORD_COUNT))
	{
		return 0;
	}

	if (data->reserved[RW_PARAM_CURRENT_CAL_MAGIC_INDEX]!=RW_PARAM_CURRENT_CAL_MAGIC ||
        data->reserved[RW_PARAM_CURRENT_CAL_VERSION_INDEX]!=RW_PARAM_CURRENT_CAL_VERSION ||
		!Storage_CurrentCalibrationValid(
			data->reserved[RW_PARAM_CURRENT_CAL_CHG_K_INDEX],
			(INT16)data->reserved[RW_PARAM_CURRENT_CAL_CHG_B_INDEX],
			data->reserved[RW_PARAM_CURRENT_CAL_DSG_K_INDEX],
			(INT16)data->reserved[RW_PARAM_CURRENT_CAL_DSG_B_INDEX]))
	{
		return 0;
	}
	return 1;
}

/* Construct a snapshot without changing live parameters before a successful commit. */
static void Storage_BuildDefaultConfig(STORAGE_FLASH_CONFIG_DATA *config)
{
    UINT16 i;
    const AFE_Value_Typedef *afe=&AFE_Parameters_RS485_Struction.u16VcellOvp;
    memset(config,0,sizeof(*config));
    memcpy(config->rw.protect,&s_protect_default,sizeof(config->rw.protect));
    memcpy(config->rw.other,&s_other_default,sizeof(config->rw.other));
    memcpy(config->rw.heat_cool,&s_heat_cool_default,sizeof(config->rw.heat_cool));
    memset(config->rw.reserved,0xFF,sizeof(config->rw.reserved));
    memcpy(config->rw.reserved,s_current_calibration_default,sizeof(s_current_calibration_default));
    config->rw.reserved[STORAGE_UPDATE_GENERATION_INDEX]=PROJECT_CFG_STORAGE_UPDATE_GENERATION;
    config->rw.reserved[STORAGE_UPDATE_TAG_INDEX]=STORAGE_UPDATE_TAG;
    for (i=0U; i<FLASH_STORAGE_AFE_WORD_COUNT; ++i) config->afe[i]=afe[i].defaultValue;
}

static UINT8 Storage_ConfigValid(const STORAGE_FLASH_CONFIG_DATA *config)
{
    UINT16 i;
    const AFE_Value_Typedef *afe=&AFE_Parameters_RS485_Struction.u16VcellOvp;
    if ((config->initializing & ~STORAGE_PARAM_RESET_MASK)!=0U ||
        !Storage_RWParametersValid(&config->rw)) return 0U;
    for (i=0U; i<FLASH_STORAGE_AFE_WORD_COUNT; ++i)
        if (config->afe[i]<afe[i].minValue || config->afe[i]>afe[i].maxValue) return 0U;
    return 1U;
}

static void Storage_ApplyConfig(const STORAGE_FLASH_CONFIG_DATA *config)
{
    UINT16 i;
    memcpy(&PRT_E2ROMParas,config->rw.protect,sizeof(config->rw.protect));
    memcpy(&OtherElement,config->rw.other,sizeof(config->rw.other));
    memcpy(&Heat_Cool_Element,config->rw.heat_cool,sizeof(config->rw.heat_cool));
    /* Only current calibration is persisted; all other K/B retain their defaults. */
    for (i=0U; i<KB_NUM; ++i) {
        g_u16CalibCoefK[i]=SYSKDEFAULT;
        g_i16CalibCoefB[i]=SYSBDEFAULT;
    }
    g_u16CalibCoefK[MDL_ICHG]=config->rw.reserved[RW_PARAM_CURRENT_CAL_CHG_K_INDEX];
    g_i16CalibCoefB[MDL_ICHG]=(INT16)config->rw.reserved[RW_PARAM_CURRENT_CAL_CHG_B_INDEX];
    g_u16CalibCoefK[MDL_IDSG]=config->rw.reserved[RW_PARAM_CURRENT_CAL_DSG_K_INDEX];
    g_i16CalibCoefB[MDL_IDSG]=(INT16)config->rw.reserved[RW_PARAM_CURRENT_CAL_DSG_B_INDEX];
    SeriesNum=(UINT8)OtherElement.u16Sys_SeriesNum;
    if (OtherElement.u16Sys_CS_Res!=0U)
        g_u32CS_Res_AFE=((UINT32)OtherElement.u16Sys_CS_Res_Num*1000U)/OtherElement.u16Sys_CS_Res;
    for (i=0U; i<FLASH_STORAGE_AFE_WORD_COUNT; ++i)
        (&AFE_Parameters_RS485_Struction.u16VcellOvp)[i].curValue=config->afe[i];
}

static UINT8 Storage_IsSocParameter(UINT16 word)
{
    return (UINT8)(word==offsetof(struct OTHER_ELEMENT,u16Soc_TableSelect)/sizeof(UINT16) ||
        (word>=offsetof(struct OTHER_ELEMENT,u16Soc_Ah)/sizeof(UINT16) &&
         word<=offsetof(struct OTHER_ELEMENT,u16Soc_V_0)/sizeof(UINT16)));
}

static UINT8 Storage_UpdateModules(STORAGE_FLASH_CONFIG_DATA *stored)
{
    UINT16 group, word;
    UINT16 generation=PROJECT_CFG_STORAGE_UPDATE_GENERATION;
    UINT16 mask=PROJECT_CFG_STORAGE_UPDATE_MASK;
    UINT8 changed=0U;
    const AFE_Value_Typedef *afe=&AFE_Parameters_RS485_Struction.u16VcellOvp;
    const UINT16 *other_default=(const UINT16 *)&s_other_default;
    if (stored->rw.reserved[STORAGE_UPDATE_TAG_INDEX]!=STORAGE_UPDATE_TAG) {
        /* Preserve an existing current-format configuration before replacing old tags. */
        stored->rw.reserved[STORAGE_UPDATE_GENERATION_INDEX]=0U;
        stored->rw.reserved[STORAGE_UPDATE_TAG_INDEX]=STORAGE_UPDATE_TAG;
        changed=1U;
    }
    if (!generation || stored->rw.reserved[STORAGE_UPDATE_GENERATION_INDEX]==generation)
        return (UINT8)(!changed || Storage_SaveConfig(stored));
    for (group=0U; group<STORAGE_PARAM_COUNT; ++group) {
        if ((mask & (1U<<group))==0U) continue;
        switch (group) {
        case STORAGE_PARAM_AFE:
            for (word=0U; word<FLASH_STORAGE_AFE_WORD_COUNT; ++word)
                stored->afe[word]=afe[word].defaultValue;
            break;
        case STORAGE_PARAM_PROTECT:
            memcpy(stored->rw.protect,&s_protect_default,sizeof(stored->rw.protect)); break;
        case STORAGE_PARAM_SOC:
        case STORAGE_PARAM_OTHER:
            for (word=0U; word<E2P_PARA_NUM_OTHER_ELEMENT1; ++word)
                if (Storage_IsSocParameter(word)==(group==STORAGE_PARAM_SOC))
                    stored->rw.other[word]=other_default[word];
            break;
        case STORAGE_PARAM_CALIB:
            memcpy(stored->rw.reserved,s_current_calibration_default,STORAGE_PARAM_VERSION_FIRST*sizeof(UINT16)); break;
        case STORAGE_PARAM_HEAT_COOL:
            memcpy(stored->rw.heat_cool,&s_heat_cool_default,sizeof(stored->rw.heat_cool)); break;
        case STORAGE_PARAM_SOC_STATE: stored->initializing |= STORAGE_PARAM_RESET_SOC; break;
        case STORAGE_PARAM_LOG: stored->initializing |= STORAGE_PARAM_RESET_LOG; break;
        }
    }
    stored->rw.reserved[STORAGE_UPDATE_GENERATION_INDEX]=generation;
    return Storage_CommitParameterUpdate(stored);
}

UINT8 StorageFlash_SaveParameters(UINT8 cycle_count_written)
{
    STORAGE_FLASH_CONFIG_DATA config;
    STORAGE_STATE state;
    UINT16 generation;
    if (!Storage_LoadConfig(&config)) return 0U;
    /* Preserve versions, baud mode, battery number and cycle generation. */
    memcpy(config.rw.protect,&PRT_E2ROMParas,sizeof(config.rw.protect));
    memcpy(config.rw.other,&OtherElement,sizeof(config.rw.other));
    memcpy(config.rw.heat_cool,&Heat_Cool_Element,sizeof(config.rw.heat_cool));
    config.rw.reserved[RW_PARAM_CURRENT_CAL_MAGIC_INDEX]=RW_PARAM_CURRENT_CAL_MAGIC;
    config.rw.reserved[RW_PARAM_CURRENT_CAL_VERSION_INDEX]=RW_PARAM_CURRENT_CAL_VERSION;
    config.rw.reserved[RW_PARAM_CURRENT_CAL_CHG_K_INDEX]=g_u16CalibCoefK[MDL_ICHG];
    config.rw.reserved[RW_PARAM_CURRENT_CAL_CHG_B_INDEX]=(UINT16)g_i16CalibCoefB[MDL_ICHG];
    config.rw.reserved[RW_PARAM_CURRENT_CAL_DSG_K_INDEX]=g_u16CalibCoefK[MDL_IDSG];
    config.rw.reserved[RW_PARAM_CURRENT_CAL_DSG_B_INDEX]=(UINT16)g_i16CalibCoefB[MDL_IDSG];
    if (cycle_count_written) {
        generation=config.rw.reserved[STORAGE_SOC_CYCLE_GENERATION_INDEX];
        generation=generation>=0xFFFEU ? 1U : (UINT16)(generation+1U);
        Storage_LoadState(&state);
        /* Skip the last snapshot tag even after wrap and repeated failed saves. */
        if (generation==state.soc.u16Reserved[STORAGE_SOC_CYCLE_SNAPSHOT_INDEX])
            generation=generation>=0xFFFEU ? 1U : (UINT16)(generation+1U);
        config.rw.reserved[STORAGE_SOC_CYCLE_GENERATION_INDEX]=generation;
    }
    return Storage_SaveConfig(&config);
}

/* A torn first installation may be erased and retried only if every written
 * half-word matches the expected initial record prefix. */
static UINT8 Storage_FirstConfigWriteInterrupted(const STORAGE_FLASH_CONFIG_DATA *defaults)
{
    STORAGE_RECORD header;
    UINT16 offset, actual, expected;
    UINT8 blank_seen=0U;
    if (!StorageFlash_IsAreaBlank(FLASH_CONFIG_B,FLASH_STORAGE_PAGE_SIZE) ||
        FlashReadOneHalfWord(FLASH_CONFIG_A+14U)!=0xFFFFU ||
        !StorageFlash_IsAreaBlank(FLASH_CONFIG_A+sizeof(header)+sizeof(*defaults),
            FLASH_STORAGE_PAGE_SIZE-sizeof(header)-sizeof(*defaults))) return 0U;
    header.magic=STORAGE_CONFIG_MAGIC;
    header.sequence=1U;
    header.length=sizeof(*defaults);
    header.format=STORAGE_RECORD_FORMAT;
    header.commit=0xFFFFU;
    header.crc=Storage_RecordCrc(&header,defaults);
    for (offset=0U; offset<sizeof(header)+sizeof(*defaults); offset+=2U) {
        if (offset==14U) continue; /* Commit must remain erased. */
        actual=FlashReadOneHalfWord(FLASH_CONFIG_A+offset);
        expected=offset<sizeof(header) ?
            ((const UINT16 *)&header)[offset/2U] :
            ((const UINT16 *)defaults)[(offset-sizeof(header))/2U];
        if (actual==0xFFFFU && expected!=0xFFFFU) blank_seen=1U;
        else if (actual!=expected || (blank_seen && actual!=0xFFFFU)) return 0U;
    }
    return 1U;
}

UINT8 StorageFlash_InitParameters(void)
{
    STORAGE_FLASH_CONFIG_DATA config;
    UINT8 valid;
    System_ERROR_UserCallback(ERROR_REMOVE_EEPROM_COM);
    Storage_BuildDefaultConfig(&config);
    if (!Storage_ConfigValid(&config)) goto failed;
    valid=Storage_LoadConfig(&config);
    if (valid && config.initializing && !Storage_CommitParameterUpdate(&config)) goto failed;
    if (valid && !Storage_ResumeSocCycleWrite(&config)) goto failed;
    if (!valid) {
        Storage_BuildDefaultConfig(&config);
        /* Reject nonblank damaged data instead of implicitly erasing valid state. */
        if (!StorageFlash_IsAreaBlank(FLASH_STATE_A,2U*FLASH_STORAGE_PAGE_SIZE) ||
            !StorageFlash_IsAreaBlank(FLASH_LOG_A,2U*FLASH_STORAGE_PAGE_SIZE)) goto failed;
        if (!(StorageFlash_IsAreaBlank(FLASH_CONFIG_A,FLASH_STORAGE_PAGE_SIZE) &&
              StorageFlash_IsAreaBlank(FLASH_CONFIG_B,FLASH_STORAGE_PAGE_SIZE)) &&
            !Storage_FirstConfigWriteInterrupted(&config)) goto failed;
        if (!Storage_SaveConfig(&config)) goto failed;
    } else if (!Storage_UpdateModules(&config)) goto failed;
    Storage_ApplyConfig(&config);
    ReadEEPROM_EventRecord_Parameters();
    return 1U;
failed:
    System_ERROR_UserCallback(ERROR_EEPROM_STORE);
    return 0U;
}

/* App-to-IAP mailbox. */
static UINT16 FlashReadOneHalfWord(UINT32 faddr)
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
