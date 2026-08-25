#include "main.h"

#define FLASH_STORAGE_MAGIC_SOC             ((UINT32)0x534F4331U)
#define FLASH_STORAGE_MAGIC_AFE             ((UINT32)0x41464531U)
#define FLASH_STORAGE_MAGIC_RW_PARAM        ((UINT32)0x52575031U)
#define FLASH_STORAGE_MAGIC_CONFIG          ((UINT32)0x43464731U)
#define FLASH_STORAGE_MAGIC_LOG             ((UINT32)0x4C4F4731U)
#define FLASH_STORAGE_MAGIC_FACTORY_AGING   ((UINT32)0x41474531U)
#define FLASH_STORAGE_RECORD_VERSION_LEGACY ((UINT16)0x0001U)
#define FLASH_STORAGE_RECORD_VERSION        ((UINT16)0x0002U)
#define FLASH_SIZE_REG_ADDR                 ((UINT32)0x1FFFF7E0U)
#define FLASH_ERASE_RETRY_MAX               ((UINT8)3U)
#define APP_UPGRADE_MAILBOX_ADDR            ((UINT32)0x20004FE0U)
#define APP_UPGRADE_MAILBOX_MAGIC           ((UINT32)0x49415031U)
#define APP_UPGRADE_MAILBOX_REQUEST         ((UINT32)0x5AA55AA5U)

typedef struct
{
	UINT32 magic;
	UINT16 version;
	UINT16 length;
	UINT32 sequence;
	UINT16 crc;
	UINT16 reserved;
} STORAGE_FLASH_HEADER;

typedef struct
{
	UINT32 magic;
	UINT16 version;
	UINT16 length;
	UINT32 sequence;
} STORAGE_FLASH_CRC_META;

typedef struct
{
	UINT8 point;
	UINT8 reserved;
	UINT8 records[FLASH_STORAGE_LOG_RECORD_COUNT][2];
} STORAGE_FLASH_LOG_DATA;

typedef struct
{
	UINT32 magic;
	UINT32 magic_inv;
	UINT32 request;
	UINT32 request_inv;
	UINT32 crc;
} APP_UPGRADE_MAILBOX;

typedef struct FLASH_RUNTIME_TAG { volatile UINT8 busy; } FLASH_RUNTIME;
static FLASH_RUNTIME s_flash;
static void StorageFlash_BeginWrite(void) { s_flash.busy = 1U; }
static void StorageFlash_EndWrite(void) { s_flash.busy = 0U; }
UINT8 StorageFlash_IsBusy(void) { return s_flash.busy; }

static UINT16 StorageFlash_CrcUpdate(UINT16 crc, const UINT8 *data, UINT16 length)
{
	UINT16 i; UINT8 bit;
	if ((data == 0) || (length == 0U)) return crc;
	for (i = 0U; i < length; ++i)
	{
		crc ^= data[i];
		for (bit = 0U; bit < 8U; ++bit)
			crc = (crc & 1U) ? (UINT16)((crc >> 1) ^ 0xA001U) : (UINT16)(crc >> 1);
	}
	return crc;
}
static UINT16 StorageFlash_CalcLegacyPayloadCrc(const UINT8 *payload, UINT16 length)
{ return StorageFlash_CrcUpdate(0xFFFFU, payload, length); }
static UINT16 StorageFlash_CalcRecordCrc(UINT32 magic, UINT16 version, UINT16 length, UINT32 sequence, const UINT8 *payload)
{
	STORAGE_FLASH_CRC_META meta; UINT16 crc = 0xFFFFU;
	meta.magic = magic; meta.version = version; meta.length = length; meta.sequence = sequence;
	crc = StorageFlash_CrcUpdate(crc, (const UINT8 *)&meta, (UINT16)sizeof(meta));
	return StorageFlash_CrcUpdate(crc, payload, length);
}

static FLASH_Status FlashErasePageVerified(uint32_t page_addr)
{
	UINT32 offset; FLASH_Status result;
	if ((page_addr % FLASH_STORAGE_PAGE_SIZE) != 0U) return FLASH_ERROR_PG;
	result = FLASH_ErasePage(page_addr); if (result != FLASH_COMPLETE) return result;
	for (offset = 0U; offset < FLASH_STORAGE_PAGE_SIZE; offset += 2U)
		if (FlashReadOneHalfWord(page_addr + offset) != 0xFFFFU) return FLASH_ERROR_PG;
	return FLASH_COMPLETE;
}
static FLASH_Status FlashProgramHalfWordVerified(uint32_t addr, uint16_t data)
{
	FLASH_Status result; if ((addr & 1U) != 0U) return FLASH_ERROR_PG;
	result = FLASH_ProgramHalfWord(addr, data); if (result != FLASH_COMPLETE) return result;
	return (FlashReadOneHalfWord(addr) == data) ? FLASH_COMPLETE : FLASH_ERROR_PG;
}
static FLASH_Status FlashProgramBytesVerified(uint32_t addr, const UINT8 *data, UINT16 length)
{
	UINT16 offset = 0U, half_word; FLASH_Status result;
	if ((data == 0) && (length != 0U)) return FLASH_ERROR_PG;
	while (offset < length)
	{
		half_word = data[offset];
		if ((offset + 1U) < length) half_word |= ((UINT16)data[offset + 1U] << 8); else half_word |= 0xFF00U;
		result = FlashProgramHalfWordVerified(addr + offset, half_word); if (result != FLASH_COMPLETE) return result;
		offset += 2U;
	}
	return FLASH_COMPLETE;
}
static UINT16 StorageFlash_RecordSpan(UINT16 payload_length)
{
	UINT32 span = (UINT32)sizeof(STORAGE_FLASH_HEADER) + payload_length;
	span = (span + (FLASH_STORAGE_RECORD_ALIGNMENT - 1U)) & ~((UINT32)FLASH_STORAGE_RECORD_ALIGNMENT - 1U);
	return (UINT16)span;
}
static UINT8 StorageFlash_IsAreaBlank(uint32_t addr, UINT16 length)
{
	UINT16 offset; for (offset = 0U; offset < length; offset += 2U)
		if (FlashReadOneHalfWord(addr + offset) != 0xFFFFU) return 0U;
	return 1U;
}
static UINT8 StorageFlash_ReadRecord(uint32_t record_addr, UINT32 expect_magic, UINT16 expect_length, UINT8 *payload, UINT32 *sequence)
{
	const STORAGE_FLASH_HEADER *header = (const STORAGE_FLASH_HEADER *)record_addr;
	const UINT8 *payload_addr = (const UINT8 *)(record_addr + sizeof(STORAGE_FLASH_HEADER)); UINT16 crc;
	if (((record_addr & ((UINT32)FLASH_STORAGE_RECORD_ALIGNMENT - 1U)) != 0U) || header->magic != expect_magic ||
		header->length != expect_length || ((header->version != FLASH_STORAGE_RECORD_VERSION) &&
		(header->version != FLASH_STORAGE_RECORD_VERSION_LEGACY))) return 0U;
	crc = (header->version == FLASH_STORAGE_RECORD_VERSION) ?
		StorageFlash_CalcRecordCrc(header->magic, header->version, header->length, header->sequence, payload_addr) :
		StorageFlash_CalcLegacyPayloadCrc(payload_addr, expect_length);
	if (crc != header->crc) return 0U;
	if (payload != 0) memcpy(payload, payload_addr, expect_length); if (sequence != 0) *sequence = header->sequence;
	return 1U;
}
static FLASH_Status StorageFlash_ProgramRecord(uint32_t record_addr, UINT32 magic, const UINT8 *payload, UINT16 length, UINT32 sequence)
{
	STORAGE_FLASH_HEADER header; FLASH_Status result;
	if ((payload == 0) || ((record_addr & ((UINT32)FLASH_STORAGE_RECORD_ALIGNMENT - 1U)) != 0U)) return FLASH_ERROR_PG;
	header.magic = magic; header.version = FLASH_STORAGE_RECORD_VERSION; header.length = length; header.sequence = sequence;
	header.crc = StorageFlash_CalcRecordCrc(magic, FLASH_STORAGE_RECORD_VERSION, length, sequence, payload); header.reserved = 0xFFFFU;
	result = FlashProgramBytesVerified(record_addr, (const UINT8 *)&header, (UINT16)sizeof(header));
	if (result == FLASH_COMPLETE) result = FlashProgramBytesVerified(record_addr + sizeof(header), payload, length);
	return result;
}
static UINT8 StorageFlash_LoadPair(uint32_t slot_a, uint32_t slot_b, UINT32 magic, UINT16 length, UINT8 *payload)
{
	UINT8 va, vb; UINT32 sa=0U,sb=0U; uint32_t chosen; if (payload==0) return 0U;
	va=StorageFlash_ReadRecord(slot_a,magic,length,0,&sa); vb=StorageFlash_ReadRecord(slot_b,magic,length,0,&sb);
	if(!va&&!vb)return 0U; chosen=(va&&vb)?((sa>=sb)?slot_a:slot_b):(va?slot_a:slot_b);
	return StorageFlash_ReadRecord(chosen,magic,length,payload,0);
}
static FLASH_Status StorageFlash_WritePairSlot(uint32_t slot, UINT32 magic, const UINT8 *payload, UINT16 length, UINT32 seq)
{
	FLASH_Status result; FLASH_Unlock(); FLASH_ClearFlag(FLASH_FLAG_EOP|FLASH_FLAG_PGERR|FLASH_FLAG_WRPRTERR);
	result=FlashErasePageVerified(slot); if(result==FLASH_COMPLETE) result=StorageFlash_ProgramRecord(slot,magic,payload,length,seq); FLASH_Lock(); return result;
}
static UINT8 StorageFlash_SavePair(uint32_t a,uint32_t b,UINT32 magic,const UINT8 *payload,UINT16 length)
{
	UINT8 va,vb; UINT32 sa=0U,sb=0U,next=1U,verify=0U; uint32_t target=a; FLASH_Status result;
	if((payload==0)||((UINT32)sizeof(STORAGE_FLASH_HEADER)+length>FLASH_STORAGE_PAGE_SIZE)){System_ERROR_UserCallback(ERROR_EEPROM_STORE);return 0U;}
	va=StorageFlash_ReadRecord(a,magic,length,0,&sa); vb=StorageFlash_ReadRecord(b,magic,length,0,&sb);
	if(va&&vb){if(sa>=sb){next=sa+1U;target=b;}else{next=sb+1U;target=a;}}else if(va){next=sa+1U;target=b;}else if(vb){next=sb+1U;target=a;}
	result=StorageFlash_WritePairSlot(target,magic,payload,length,next);
	if(result!=FLASH_COMPLETE||!StorageFlash_ReadRecord(target,magic,length,0,&verify)||verify!=next||
		memcmp((const void *)(target+sizeof(STORAGE_FLASH_HEADER)),payload,length)!=0){System_ERROR_UserCallback(ERROR_EEPROM_STORE);return 0U;} return 1U;
}
static UINT8 StorageFlash_LoadJournalPage(uint32_t slot,UINT32 magic,UINT16 length,UINT8 *payload,UINT32 *sequence,UINT32 *next_addr)
{
	UINT16 span=StorageFlash_RecordSpan(length); UINT32 off,addr,latest_addr=0U,latest=0U,current=0U,blank=slot+FLASH_STORAGE_PAGE_SIZE; UINT8 found=0U;
	if((span==0U)||(span>FLASH_STORAGE_PAGE_SIZE))return 0U;
	for(off=0U;(off+span)<=FLASH_STORAGE_PAGE_SIZE;off+=span){addr=slot+off;if(StorageFlash_IsAreaBlank(addr,span)){blank=addr;break;}
		if(StorageFlash_ReadRecord(addr,magic,length,0,&current)&&((!found)||(current>=latest))){latest=current;latest_addr=addr;found=1U;}}
	if(found&&(payload!=0))memcpy(payload,(const void *)(latest_addr+sizeof(STORAGE_FLASH_HEADER)),length); if(sequence!=0)*sequence=found?latest:0U;if(next_addr!=0)*next_addr=blank;return found;
}
static UINT8 StorageFlash_LoadJournalPair(uint32_t a,uint32_t b,UINT32 magic,UINT16 length,UINT8 *payload)
{
	UINT8 va,vb;UINT32 sa=0U,sb=0U;uint32_t chosen;if(payload==0)return 0U;va=StorageFlash_LoadJournalPage(a,magic,length,0,&sa,0);vb=StorageFlash_LoadJournalPage(b,magic,length,0,&sb,0);
	if(!va&&!vb)return 0U;chosen=(va&&vb)?((sa>=sb)?a:b):(va?a:b);return StorageFlash_LoadJournalPage(chosen,magic,length,payload,0,0);
}
static UINT8 StorageFlash_SaveJournalPair(uint32_t a,uint32_t b,UINT32 magic,const UINT8 *payload,UINT16 length)
{
	UINT16 span=StorageFlash_RecordSpan(length);UINT8 va,vb,erase=0U;UINT32 sa=0U,sb=0U,na=a,nb=b,next=1U,page=a,addr=a,verify=0U;FLASH_Status result;
	if((payload==0)||(span==0U)||(span>FLASH_STORAGE_PAGE_SIZE)){System_ERROR_UserCallback(ERROR_EEPROM_STORE);return 0U;}
	va=StorageFlash_LoadJournalPage(a,magic,length,0,&sa,&na);vb=StorageFlash_LoadJournalPage(b,magic,length,0,&sb,&nb);
	if(va&&vb){if(sa>=sb){next=sa+1U;page=a;addr=na;}else{next=sb+1U;page=b;addr=nb;}}else if(va){next=sa+1U;page=a;addr=na;}else if(vb){next=sb+1U;page=b;addr=nb;}else erase=StorageFlash_IsAreaBlank(page,(UINT16)FLASH_STORAGE_PAGE_SIZE)?0U:1U;
	if((addr+span)>(page+FLASH_STORAGE_PAGE_SIZE)){page=(page==a)?b:a;addr=page;erase=1U;}
	FLASH_Unlock();FLASH_ClearFlag(FLASH_FLAG_EOP|FLASH_FLAG_PGERR|FLASH_FLAG_WRPRTERR);if(erase){result=FlashErasePageVerified(page);if(result!=FLASH_COMPLETE){FLASH_Lock();System_ERROR_UserCallback(ERROR_EEPROM_STORE);return 0U;}}
	result=StorageFlash_ProgramRecord(addr,magic,payload,length,next);FLASH_Lock();if(result!=FLASH_COMPLETE||!StorageFlash_ReadRecord(addr,magic,length,0,&verify)||verify!=next||memcmp((const void *)(addr+sizeof(STORAGE_FLASH_HEADER)),payload,length)!=0){System_ERROR_UserCallback(ERROR_EEPROM_STORE);return 0U;}return 1U;
}
static UINT8 StorageFlash_SaveJournalPage(uint32_t slot,UINT32 magic,const UINT8 *payload,UINT16 length)
{
	UINT16 span=StorageFlash_RecordSpan(length);UINT8 valid,erase=0U;UINT32 seq=0U,addr=slot,next=1U,verify=0U;FLASH_Status result;
	if((payload==0)||(span==0U)||(span>FLASH_STORAGE_PAGE_SIZE)){System_ERROR_UserCallback(ERROR_EEPROM_STORE);return 0U;}
	valid=StorageFlash_LoadJournalPage(slot,magic,length,0,&seq,&addr);if(valid)next=seq+1U;else{erase=StorageFlash_IsAreaBlank(slot,(UINT16)FLASH_STORAGE_PAGE_SIZE)?0U:1U;addr=slot;}
	if((addr+span)>(slot+FLASH_STORAGE_PAGE_SIZE)){erase=1U;addr=slot;}
	FLASH_Unlock();FLASH_ClearFlag(FLASH_FLAG_EOP|FLASH_FLAG_PGERR|FLASH_FLAG_WRPRTERR);if(erase){result=FlashErasePageVerified(slot);if(result!=FLASH_COMPLETE){FLASH_Lock();System_ERROR_UserCallback(ERROR_EEPROM_STORE);return 0U;}}
	result=StorageFlash_ProgramRecord(addr,magic,payload,length,next);FLASH_Lock();if(result!=FLASH_COMPLETE||!StorageFlash_ReadRecord(addr,magic,length,0,&verify)||verify!=next||memcmp((const void *)(addr+sizeof(STORAGE_FLASH_HEADER)),payload,length)!=0){System_ERROR_UserCallback(ERROR_EEPROM_STORE);return 0U;}return 1U;
}

FLASH_Status FlashWriteOneHalfWord(uint32_t StartAddr,uint16_t Buffer)
{
	FLASH_Status result=FLASH_ERROR_PG;UINT8 retry;StorageFlash_BeginWrite();FLASH_Unlock();FLASH_ClearFlag(FLASH_FLAG_EOP|FLASH_FLAG_PGERR|FLASH_FLAG_WRPRTERR);
	for(retry=0U;retry<FLASH_ERASE_RETRY_MAX;++retry){result=FlashErasePageVerified(StartAddr);if(result==FLASH_COMPLETE)break;FLASH_ClearFlag(FLASH_FLAG_EOP|FLASH_FLAG_PGERR|FLASH_FLAG_WRPRTERR);}if(result==FLASH_COMPLETE)result=FlashProgramHalfWordVerified(StartAddr,Buffer);FLASH_Lock();StorageFlash_EndWrite();return result;
}
UINT16 FlashReadOneHalfWord(UINT32 faddr){return *(vu16 *)faddr;}
static volatile APP_UPGRADE_MAILBOX *AppUpgrade_Mailbox(void){return (volatile APP_UPGRADE_MAILBOX *)APP_UPGRADE_MAILBOX_ADDR;}
static UINT32 AppUpgrade_MailboxCrc(UINT32 magic,UINT32 request){return magic^request^0xA5A55A5AU;}
static UINT8 AppUpgrade_IsIapRequested(void){volatile APP_UPGRADE_MAILBOX *m=AppUpgrade_Mailbox();return((m->magic==APP_UPGRADE_MAILBOX_MAGIC)&&(m->magic_inv==(UINT32)~APP_UPGRADE_MAILBOX_MAGIC)&&(m->request==APP_UPGRADE_MAILBOX_REQUEST)&&(m->request_inv==(UINT32)~APP_UPGRADE_MAILBOX_REQUEST)&&(m->crc==AppUpgrade_MailboxCrc(APP_UPGRADE_MAILBOX_MAGIC,APP_UPGRADE_MAILBOX_REQUEST)))?1U:0U;}
UINT8 AppUpgrade_RequestIap(void){volatile APP_UPGRADE_MAILBOX *m=AppUpgrade_Mailbox();m->magic=APP_UPGRADE_MAILBOX_MAGIC;m->magic_inv=(UINT32)~APP_UPGRADE_MAILBOX_MAGIC;m->request=APP_UPGRADE_MAILBOX_REQUEST;m->request_inv=(UINT32)~APP_UPGRADE_MAILBOX_REQUEST;m->crc=AppUpgrade_MailboxCrc(APP_UPGRADE_MAILBOX_MAGIC,APP_UPGRADE_MAILBOX_REQUEST);return AppUpgrade_IsIapRequested();}

static UINT8 StorageFlash_LoadConfigData(STORAGE_FLASH_CONFIG_DATA *data){if(data==0)return 0U;if(!StorageFlash_LoadPair(FLASH_ADDR_STORAGE_CONFIG_SLOT_A,FLASH_ADDR_STORAGE_CONFIG_SLOT_B,FLASH_STORAGE_MAGIC_CONFIG,(UINT16)sizeof(*data),(UINT8 *)data))return 0U;return(data->u16FormatVersion==FLASH_STORAGE_CONFIG_FORMAT_VERSION)?1U:0U;}
static UINT8 StorageFlash_SaveConfigData(const STORAGE_FLASH_CONFIG_DATA *data){STORAGE_FLASH_CONFIG_DATA save;UINT8 result;if(data==0)return 0U;save=*data;save.u16FormatVersion=FLASH_STORAGE_CONFIG_FORMAT_VERSION;StorageFlash_BeginWrite();result=StorageFlash_SavePair(FLASH_ADDR_STORAGE_CONFIG_SLOT_A,FLASH_ADDR_STORAGE_CONFIG_SLOT_B,FLASH_STORAGE_MAGIC_CONFIG,(const UINT8 *)&save,(UINT16)sizeof(save));StorageFlash_EndWrite();return result;}
static UINT8 StorageFlash_LoadLegacyAfe(UINT16 *values){return StorageFlash_LoadPair(FLASH_ADDR_STORAGE_AFE_SLOT_A,FLASH_ADDR_STORAGE_AFE_SLOT_B,FLASH_STORAGE_MAGIC_AFE,(UINT16)(FLASH_STORAGE_AFE_WORD_COUNT*sizeof(UINT16)),(UINT8 *)values);}
static UINT8 StorageFlash_LoadLegacyRw(STORAGE_FLASH_RW_PARAM_DATA *data){return StorageFlash_LoadPair(FLASH_ADDR_STORAGE_RW_PARAM_SLOT_A,FLASH_ADDR_STORAGE_RW_PARAM_SLOT_B,FLASH_STORAGE_MAGIC_RW_PARAM,(UINT16)sizeof(*data),(UINT8 *)data);}
static void StorageFlash_ConfigFromLegacy(STORAGE_FLASH_CONFIG_DATA *c,const UINT16 *afe,const STORAGE_FLASH_RW_PARAM_DATA *rw){memset(c,0xFF,sizeof(*c));c->u16FormatVersion=FLASH_STORAGE_CONFIG_FORMAT_VERSION;c->u16AppliedPolicyVersion=FlashReadOneHalfWord(FLASH_ADDR_UPGRADE_PARAM_FLAG);memcpy(c->afe,afe,sizeof(c->afe));memcpy(c->protect,rw->protect,sizeof(c->protect));memcpy(c->other,rw->other,sizeof(c->other));memcpy(c->reserved,rw->reserved,sizeof(c->reserved));}
static UINT8 StorageFlash_TryMigrateLegacyConfig(STORAGE_FLASH_CONFIG_DATA *c){UINT16 afe[FLASH_STORAGE_AFE_WORD_COUNT];STORAGE_FLASH_RW_PARAM_DATA rw;if(!StorageFlash_LoadLegacyAfe(afe)||!StorageFlash_LoadLegacyRw(&rw))return 0U;StorageFlash_ConfigFromLegacy(c,afe,&rw);return StorageFlash_SaveConfigData(c);}
UINT8 StorageFlash_LoadAfeData(UINT16 *v,UINT16 n){STORAGE_FLASH_CONFIG_DATA c;if((v==0)||(n!=FLASH_STORAGE_AFE_WORD_COUNT))return 0U;if(StorageFlash_LoadConfigData(&c)){memcpy(v,c.afe,sizeof(c.afe));return 1U;}if(!StorageFlash_LoadLegacyAfe(v))return 0U;(void)StorageFlash_TryMigrateLegacyConfig(&c);return 1U;}
UINT8 StorageFlash_SaveAfeData(const UINT16 *v,UINT16 n){STORAGE_FLASH_CONFIG_DATA c;STORAGE_FLASH_RW_PARAM_DATA rw;UINT8 result;if((v==0)||(n!=FLASH_STORAGE_AFE_WORD_COUNT))return 0U;if(StorageFlash_LoadConfigData(&c)){memcpy(c.afe,v,sizeof(c.afe));return StorageFlash_SaveConfigData(&c);}if(StorageFlash_LoadLegacyRw(&rw)){StorageFlash_ConfigFromLegacy(&c,v,&rw);return StorageFlash_SaveConfigData(&c);}StorageFlash_BeginWrite();result=StorageFlash_SavePair(FLASH_ADDR_STORAGE_AFE_SLOT_A,FLASH_ADDR_STORAGE_AFE_SLOT_B,FLASH_STORAGE_MAGIC_AFE,(const UINT8 *)v,(UINT16)(n*sizeof(UINT16)));StorageFlash_EndWrite();return result;}
UINT8 StorageFlash_LoadRwParamData(STORAGE_FLASH_RW_PARAM_DATA *d){STORAGE_FLASH_CONFIG_DATA c;if(d==0)return 0U;if(StorageFlash_LoadConfigData(&c)){memcpy(d->protect,c.protect,sizeof(d->protect));memcpy(d->other,c.other,sizeof(d->other));memcpy(d->reserved,c.reserved,sizeof(d->reserved));return 1U;}if(!StorageFlash_LoadLegacyRw(d))return 0U;(void)StorageFlash_TryMigrateLegacyConfig(&c);return 1U;}
UINT8 StorageFlash_SaveRwParamData(const STORAGE_FLASH_RW_PARAM_DATA *d){STORAGE_FLASH_CONFIG_DATA c;UINT16 afe[FLASH_STORAGE_AFE_WORD_COUNT];UINT8 result;if(d==0)return 0U;if(StorageFlash_LoadConfigData(&c)){memcpy(c.protect,d->protect,sizeof(c.protect));memcpy(c.other,d->other,sizeof(c.other));memcpy(c.reserved,d->reserved,sizeof(c.reserved));return StorageFlash_SaveConfigData(&c);}if(StorageFlash_LoadLegacyAfe(afe)){StorageFlash_ConfigFromLegacy(&c,afe,d);return StorageFlash_SaveConfigData(&c);}StorageFlash_BeginWrite();result=StorageFlash_SavePair(FLASH_ADDR_STORAGE_RW_PARAM_SLOT_A,FLASH_ADDR_STORAGE_RW_PARAM_SLOT_B,FLASH_STORAGE_MAGIC_RW_PARAM,(const UINT8 *)d,(UINT16)sizeof(*d));StorageFlash_EndWrite();return result;}
UINT16 StorageFlash_GetConfigPolicyVersion(void){STORAGE_FLASH_CONFIG_DATA c;if(StorageFlash_LoadConfigData(&c))return c.u16AppliedPolicyVersion;return FlashReadOneHalfWord(FLASH_ADDR_UPGRADE_PARAM_FLAG);}
UINT8 StorageFlash_SetConfigPolicyVersion(UINT16 version){STORAGE_FLASH_CONFIG_DATA c;if(!StorageFlash_LoadConfigData(&c)){if(!StorageFlash_TryMigrateLegacyConfig(&c))return 0U;if(!StorageFlash_LoadConfigData(&c))return 0U;}if(c.u16AppliedPolicyVersion==version)return 1U;c.u16AppliedPolicyVersion=version;return StorageFlash_SaveConfigData(&c);}

UINT8 StorageFlash_LoadSocData(STORAGE_FLASH_SOC_DATA *data)
{
	if(data==0)return 0U;
	if(!StorageFlash_LoadJournalPair(FLASH_ADDR_STORAGE_SOC_SLOT_A,FLASH_ADDR_STORAGE_SOC_SLOT_B,
		FLASH_STORAGE_MAGIC_SOC,(UINT16)sizeof(*data),(UINT8 *)data))return 0U;
	return(data->u16FormatVersion==FLASH_STORAGE_SOC_DATA_VERSION_CURRENT)?1U:0U;
}
UINT8 StorageFlash_SaveSocData(const STORAGE_FLASH_SOC_DATA *data)
{
	STORAGE_FLASH_SOC_DATA save;UINT8 result;if(data==0)return 0U;save=*data;save.u16FormatVersion=FLASH_STORAGE_SOC_DATA_VERSION_CURRENT;
	StorageFlash_BeginWrite();result=StorageFlash_SaveJournalPair(FLASH_ADDR_STORAGE_SOC_SLOT_A,FLASH_ADDR_STORAGE_SOC_SLOT_B,FLASH_STORAGE_MAGIC_SOC,(const UINT8 *)&save,(UINT16)sizeof(save));StorageFlash_EndWrite();return result;
}

UINT8 StorageFlash_LoadLogData(UINT8 *point,UINT8 records[FLASH_STORAGE_LOG_RECORD_COUNT][2]){STORAGE_FLASH_LOG_DATA data;if((point==0)||(records==0))return 0U;if(!StorageFlash_LoadJournalPair(FLASH_ADDR_STORAGE_LOG_SLOT_A,FLASH_ADDR_STORAGE_LOG_SLOT_B,FLASH_STORAGE_MAGIC_LOG,(UINT16)sizeof(data),(UINT8 *)&data))return 0U;*point=data.point;memcpy(records,data.records,sizeof(data.records));return 1U;}
UINT8 StorageFlash_SaveLogData(UINT8 point,const UINT8 records[FLASH_STORAGE_LOG_RECORD_COUNT][2]){STORAGE_FLASH_LOG_DATA data,current;UINT8 result;if(records==0)return 0U;memset(&data,0,sizeof(data));data.point=point;memcpy(data.records,records,sizeof(data.records));if(StorageFlash_LoadJournalPair(FLASH_ADDR_STORAGE_LOG_SLOT_A,FLASH_ADDR_STORAGE_LOG_SLOT_B,FLASH_STORAGE_MAGIC_LOG,(UINT16)sizeof(current),(UINT8 *)&current)&&memcmp(&current,&data,sizeof(data))==0)return 1U;StorageFlash_BeginWrite();result=StorageFlash_SaveJournalPair(FLASH_ADDR_STORAGE_LOG_SLOT_A,FLASH_ADDR_STORAGE_LOG_SLOT_B,FLASH_STORAGE_MAGIC_LOG,(const UINT8 *)&data,(UINT16)sizeof(data));StorageFlash_EndWrite();return result;}
UINT8 StorageFlash_LoadFactoryAgingData(STORAGE_FLASH_FACTORY_AGING_DATA *data){if(data==0)return 0U;if(StorageFlash_LoadJournalPage(FLASH_ADDR_FACTORY_AGING_FLAG,FLASH_STORAGE_MAGIC_FACTORY_AGING,(UINT16)sizeof(*data),(UINT8 *)data,0,0))return 1U;if(FlashReadOneHalfWord(FLASH_ADDR_FACTORY_AGING_FLAG)==FLASH_FACTORY_AGING_DONE_VALUE){memset(data,0,sizeof(*data));data->u16State=FLASH_FACTORY_AGING_STATE_DONE;return 1U;}return 0U;}
UINT8 StorageFlash_SaveFactoryAgingData(const STORAGE_FLASH_FACTORY_AGING_DATA *data){UINT8 result;if(data==0)return 0U;StorageFlash_BeginWrite();result=StorageFlash_SaveJournalPage(FLASH_ADDR_FACTORY_AGING_FLAG,FLASH_STORAGE_MAGIC_FACTORY_AGING,(const UINT8 *)data,(UINT16)sizeof(*data));StorageFlash_EndWrite();return result;}
void StorageFlash_PrintBootCheck(void){UINT16 kb=*((volatile UINT16 *)FLASH_SIZE_REG_ADDR);STORAGE_FLASH_CONFIG_DATA c;STORAGE_FLASH_FACTORY_AGING_DATA a;printf("\r\n[FLASH_BOOT] flash_size_reg=%uKB page=%lu align=%u\r\n",kb,(unsigned long)FLASH_STORAGE_PAGE_SIZE,FLASH_STORAGE_RECORD_ALIGNMENT);if(kb<128U){printf("[FLASH_BOOT] rear64 unavailable: skip 0x08010000+ storage check\r\n");return;}printf("[FLASH_BOOT] config=%u policy=0x%04X iap_mailbox=%u aging=%u\r\n",StorageFlash_LoadConfigData(&c),StorageFlash_GetConfigPolicyVersion(),AppUpgrade_IsIapRequested(),StorageFlash_LoadFactoryAgingData(&a));}
void App_FlashUpdate(void){if(1==u8FlashUpdateFlag){SH367309_DriverMos_Ctrl(GPIO_CHG,0);SH367309_DriverMos_Ctrl(GPIO_DSG,0);__delay_ms(10);u8FlashUpdateFlag=0;__disable_fault_irq();MCU_RESET();}}
void APP_To_IAP_Jump(void){if(AppUpgrade_RequestIap()!=0U){__disable_fault_irq();MCU_RESET();}}
void InitAreaSelect(void){if(AppUpgrade_IsIapRequested()!=0U){__disable_fault_irq();MCU_RESET();}}
