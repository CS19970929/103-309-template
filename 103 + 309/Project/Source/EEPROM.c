#include "main.h"
#include "UpgradeParamPolicy.h"
#include "FactoryAging.h"
#include "SocEnhance.h"

#if STORAGE_RW_PARAM_PROTECT_WORD_COUNT != E2P_PARA_NUM_PROTECT
#error "RW parameter protect word count mismatch"
#endif
#if STORAGE_RW_PARAM_OTHER_WORD_COUNT != E2P_PARA_NUM_OTHER_ELEMENT1
#error "RW parameter other word count mismatch"
#endif
#if STORAGE_RW_PARAM_RESERVED_WORD_COUNT != E2P_PARA_NUM_RESERVED_RW_PARAM
#error "RW parameter reserved word count mismatch"
#endif
#if STORAGE_CALIB_COEF_COUNT != KB_NUM
#error "Calibration coefficient count mismatch"
#endif

/* --------------------------------------------------------------------------
 * Runtime parameter persistence
 * -------------------------------------------------------------------------- */

static void EEPROM_UpdateOtherElementRuntime(void)
{
	SeriesNum = (UINT8)OtherElement.u16Sys_SeriesNum;
	if (OtherElement.u16Sys_CS_Res != 0)
	{
		g_u32CS_Res_AFE = ((UINT32)OtherElement.u16Sys_CS_Res_Num * 1000) / OtherElement.u16Sys_CS_Res;
	}
}

static void EEPROM_LoadDefaultProtect(void)
{
	UINT8 i;
	const struct PRT_E2ROM_PARAS protect_default = E2P_PROTECT_DEFAULT_PRT;

	for (i = 0; i < E2P_PARA_NUM_PROTECT; ++i)
	{
		*(&PRT_E2ROMParas.u16VcellOvp_First + i) = *(&protect_default.u16VcellOvp_First + i);
	}
}

static void EEPROM_LoadDefaultCalib(void)
{
	UINT8 i;

	for (i = 0; i < KB_NUM; ++i)
	{
		g_u16CalibCoefK[i] = SYSKDEFAULT;
		g_i16CalibCoefB[i] = SYSBDEFAULT;
	}
}

static UINT8 EEPROM_CalibrationDataIsValid(const STORAGE_CALIB_DATA *data)
{
	UINT16 i;

	if (data == 0)
	{
		return 0U;
	}

	for (i = 0U; i < STORAGE_CALIB_COEF_COUNT; ++i)
	{
		if ((data->k[i] < SYSKMIN) || (data->k[i] > SYSKMAX) ||
			(data->b[i] < SYSBMIN) || (data->b[i] > SYSBMAX))
		{
			return 0U;
		}
	}
	return 1U;
}

static void EEPROM_BuildCalibrationData(STORAGE_CALIB_DATA *data)
{
	UINT16 i;

	for (i = 0U; i < STORAGE_CALIB_COEF_COUNT; ++i)
	{
		data->k[i] = g_u16CalibCoefK[i];
		data->b[i] = g_i16CalibCoefB[i];
	}
}

static void EEPROM_ApplyCalibrationData(const STORAGE_CALIB_DATA *data)
{
	UINT16 i;

	for (i = 0U; i < STORAGE_CALIB_COEF_COUNT; ++i)
	{
		g_u16CalibCoefK[i] = data->k[i];
		g_i16CalibCoefB[i] = data->b[i];
	}
}

static void EEPROM_LoadCalibrationFromStorage(void)
{
	STORAGE_CALIB_DATA data;
	UINT8 loaded;

	loaded = Storage_LoadCalibrationData(&data);
	if (loaded != 0U)
	{
		if (EEPROM_CalibrationDataIsValid(&data) != 0U)
		{
			EEPROM_ApplyCalibrationData(&data);
			return;
		}

		/* A stored calibration object exists but its values are not usable. */
		System_ERROR_UserCallback(ERROR_EEPROM_STORE);
	}

	/* Missing data is a normal first-boot migration: defaults are already in RAM. */
	EEPROM_BuildCalibrationData(&data);
	(void)Storage_SaveCalibrationData(&data);
}

UINT8 EEPROM_SaveCalibrationPair(UINT16 index, UINT16 k, INT16 b)
{
	STORAGE_CALIB_DATA candidate;

	if ((index >= STORAGE_CALIB_COEF_COUNT) ||
		(k < SYSKMIN) || (k > SYSKMAX) ||
		(b < SYSBMIN) || (b > SYSBMAX))
	{
		return 0U;
	}

	EEPROM_BuildCalibrationData(&candidate);
	candidate.k[index] = k;
	candidate.b[index] = b;
	if (EEPROM_CalibrationDataIsValid(&candidate) == 0U)
	{
		return 0U;
	}

	/* Persist first; only then commit the new calibration into live RAM. */
	if (Storage_SaveCalibrationData(&candidate) == 0U)
	{
		return 0U;
	}
	EEPROM_ApplyCalibrationData(&candidate);
	return 1U;
}

UINT8 EEPROM_ResetCalibrationToDefault(void)
{
	STORAGE_CALIB_DATA defaults;
	UINT16 i;

	for (i = 0U; i < STORAGE_CALIB_COEF_COUNT; ++i)
	{
		defaults.k[i] = SYSKDEFAULT;
		defaults.b[i] = SYSBDEFAULT;
	}

	if (Storage_SaveCalibrationData(&defaults) == 0U)
	{
		return 0U;
	}
	EEPROM_ApplyCalibrationData(&defaults);
	return 1U;
}

static void EEPROM_LoadDefaultOtherElement(void)
{
	UINT8 i;
	const struct OTHER_ELEMENT other_default = OtherElement_default;

	for (i = 0; i < E2P_PARA_NUM_OTHER_ELEMENT1; ++i)
	{
		*(&OtherElement.u16Balance_OpenVoltage + i) = *(&other_default.u16Balance_OpenVoltage + i);
	}

	EEPROM_UpdateOtherElementRuntime();
}

static void EEPROM_BuildRWParamData(STORAGE_RW_PARAM_DATA *data)
{
	UINT16 i;

	for (i = 0; i < E2P_PARA_NUM_PROTECT; ++i)
	{
		data->protect[i] = *(&PRT_E2ROMParas.u16VcellOvp_First + i);
	}
	for (i = 0; i < E2P_PARA_NUM_OTHER_ELEMENT1; ++i)
	{
		data->other[i] = *(&OtherElement.u16Balance_OpenVoltage + i);
	}
	for (i = 0; i < E2P_PARA_NUM_RESERVED_RW_PARAM; ++i)
	{
		data->reserved[i] = 0xFFFFU;
	}
}

static UINT8 EEPROM_WordBlockInRange(const UINT16 *values, const UINT16 *min_values, const UINT16 *max_values, UINT16 count)
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

static UINT8 EEPROM_RWParamDataIsValid(const STORAGE_RW_PARAM_DATA *data)
{
	const struct PRT_E2ROM_PARAS protect_min = E2P_PROTECT_MIN_PRT;
	const struct PRT_E2ROM_PARAS protect_max = E2P_PROTECT_MAX_PRT;
	const struct OTHER_ELEMENT other_min = OtherElement_min;
	const struct OTHER_ELEMENT other_max = OtherElement_max;

	if (!EEPROM_WordBlockInRange(data->protect,
								 &protect_min.u16VcellOvp_First,
								 &protect_max.u16VcellOvp_First,
								 E2P_PARA_NUM_PROTECT))
	{
		return 0;
	}
	if (!EEPROM_WordBlockInRange(data->other,
								 &other_min.u16Balance_OpenVoltage,
								 &other_max.u16Balance_OpenVoltage,
								 E2P_PARA_NUM_OTHER_ELEMENT1))
	{
		return 0;
	}
	return 1;
}

static void EEPROM_ApplyRWParamData(const STORAGE_RW_PARAM_DATA *data)
{
	UINT16 i;

	for (i = 0; i < E2P_PARA_NUM_PROTECT; ++i)
	{
		*(&PRT_E2ROMParas.u16VcellOvp_First + i) = data->protect[i];
	}
	for (i = 0; i < E2P_PARA_NUM_OTHER_ELEMENT1; ++i)
	{
		*(&OtherElement.u16Balance_OpenVoltage + i) = data->other[i];
	}
	EEPROM_UpdateOtherElementRuntime();
}

UINT8 EEPROM_SaveRWParametersToFlash(void)
{
	STORAGE_RW_PARAM_DATA data;

	EEPROM_BuildRWParamData(&data);
	if (!EEPROM_RWParamDataIsValid(&data))
	{
		System_ERROR_UserCallback(ERROR_EEPROM_STORE);
		return 0;
	}

	return Storage_SaveRwParamData(&data);
}

static void EEPROM_LoadRWParametersFromFlash(void)
{
	STORAGE_RW_PARAM_DATA data;

	if (Storage_LoadRwParamData(&data) && EEPROM_RWParamDataIsValid(&data))
	{
		EEPROM_ApplyRWParamData(&data);
		return;
	}

	System_ERROR_UserCallback(ERROR_EEPROM_STORE);
	(void)EEPROM_SaveRWParametersToFlash();
}

static void EEPROM_LoadDefaultRuntimeData(void)
{
	EEPROM_LoadDefaultProtect();
	EEPROM_LoadDefaultCalib();
	EEPROM_LoadDefaultOtherElement();

	System_ERROR_UserCallback(ERROR_REMOVE_EEPROM_COM);
	System_ERROR_UserCallback(ERROR_REMOVE_EEPROM_STORE);
}

#if UPGRADE_PARAM_POLICY_ENABLE && UPGRADE_PARAM_RESET_SOC_CONFIG
static void EEPROM_LoadDefaultSocConfig(void)
{
	const struct OTHER_ELEMENT other_default = OtherElement_default;

	OtherElement.u16Soc_TableSelect = other_default.u16Soc_TableSelect;
	OtherElement.u16Soc_Ah = other_default.u16Soc_Ah;
	OtherElement.u16Soc_Cycle_times = other_default.u16Soc_Cycle_times;
	OtherElement.u16Soc_V_100 = other_default.u16Soc_V_100;
	OtherElement.u16Soc_V_0 = other_default.u16Soc_V_0;
}
#endif

#if UPGRADE_PARAM_POLICY_ENABLE && UPGRADE_PARAM_RESET_BALANCE_OPEN_VOLTAGE
static void EEPROM_LoadDefaultBalanceOpenVoltage(void)
{
	const struct OTHER_ELEMENT other_default = OtherElement_default;

	OtherElement.u16Balance_OpenVoltage = other_default.u16Balance_OpenVoltage;
}
#endif

#if UPGRADE_PARAM_POLICY_ENABLE && UPGRADE_PARAM_UPDATE_OTHER_ELEMENT
static void EEPROM_LoadConfigOtherElement(void)
{
	UINT8 i;
	const struct OTHER_ELEMENT other_config = UPGRADE_PARAM_OTHER_ELEMENT_CONFIG;

	for (i = 0; i < E2P_PARA_NUM_OTHER_ELEMENT1; ++i)
	{
		*(&OtherElement.u16Balance_OpenVoltage + i) = *(&other_config.u16Balance_OpenVoltage + i);
	}

	EEPROM_UpdateOtherElementRuntime();
}
#endif

UINT8 UpgradeParamPolicy_ApplyOnce(void)
{
#if (!UPGRADE_PARAM_POLICY_ENABLE) || (!UPGRADE_PARAM_POLICY_HAS_ACTION)
	return 1;
#else
	UINT8 result;
	UINT8 rw_param_dirty;

#if (!UPGRADE_PARAM_FORCE_REAPPLY)
	if (FlashReadOneHalfWord(FLASH_ADDR_UPGRADE_PARAM_FLAG) == UPGRADE_PARAM_POLICY_VERSION)
	{
		return 1;
	}
#endif

	result = 1;
	rw_param_dirty = 0;

#if UPGRADE_PARAM_RESET_AFE
	if (!EEPROM_ResetData_AFE_ParametersToDefault())
	{
		result = 0;
	}
#endif

#if UPGRADE_PARAM_RESET_PROTECT
	EEPROM_LoadDefaultProtect();
	rw_param_dirty = 1;
#endif

#if UPGRADE_PARAM_RESET_BALANCE_OPEN_VOLTAGE
	EEPROM_LoadDefaultBalanceOpenVoltage();
	rw_param_dirty = 1;
#endif

#if UPGRADE_PARAM_RESET_SOC_CONFIG
	EEPROM_LoadDefaultSocConfig();
	rw_param_dirty = 1;
#endif

#if UPGRADE_PARAM_UPDATE_OTHER_ELEMENT
	EEPROM_LoadConfigOtherElement();
	rw_param_dirty = 1;
#endif

	if (rw_param_dirty && !EEPROM_SaveRWParametersToFlash())
	{
		result = 0;
	}

#if UPGRADE_PARAM_RESET_SOC_SNAPSHOT
	if (result && !SOC_ResetStoredSnapshotToDefault())
	{
		result = 0;
	}
#endif

#if UPGRADE_PARAM_RESET_EVENT_RECORD
	if (result && !EEPROM_ResetData_EventRecord_ToDefault())
	{
		result = 0;
	}
#endif

#if UPGRADE_PARAM_RESET_FACTORY_AGING_TIME
	if (result && !FactoryAging_ResetTimeByHost())
	{
		result = 0;
	}
#endif

	if (!result)
	{
		return 0;
	}

	if (FlashWriteOneHalfWord(FLASH_ADDR_UPGRADE_PARAM_FLAG, UPGRADE_PARAM_POLICY_VERSION) != FLASH_COMPLETE)
	{
		System_ERROR_UserCallback(ERROR_EEPROM_STORE);
		return 0;
	}

	return 1;
#endif
}

/* --------------------------------------------------------------------------
 * Event-log Flash backend
 *
 * The existing 100-entry snapshot format in pages 4/6 stays unchanged for
 * backward compatibility. Runtime events are appended as small delta records
 * in reserved pages 5/7. A delta is bound to the base snapshot sequence.
 * Therefore a power loss after snapshot compaction but before delta erase does
 * not replay old events: their base_sequence no longer matches the snapshot.
 * -------------------------------------------------------------------------- */

#define STORAGE_LOG_BASE_MAGIC              ((uint32_t)0x4C4F4731U) /* LOG1 */
#define STORAGE_LOG_DELTA_MAGIC             ((uint32_t)0x4C4F4744U) /* LOGD */
#define STORAGE_LOG_RECORD_VERSION          ((uint16_t)0x0001U)

typedef struct
{
	uint32_t magic;
	uint16_t version;
	uint16_t length;
	uint32_t sequence;
	uint16_t crc;
	uint16_t reserved;
} STORAGE_LOG_HEADER;

typedef struct
{
	uint8_t point;
	uint8_t reserved;
	uint8_t records[STORAGE_LOG_RECORD_COUNT][2];
} STORAGE_LOG_BASE_DATA;

typedef struct
{
	uint32_t base_sequence;
	uint8_t event;
	uint8_t delta;
	uint16_t reserved;
} STORAGE_LOG_DELTA_DATA;

static uint16_t StorageLog_CalcCrc(const uint8_t *data, uint16_t length)
{
	uint16_t crc = 0xFFFFU;
	uint16_t index;
	uint8_t bit;

	if ((data == 0) || (length == 0U))
	{
		return 0xFFFFU;
	}

	for (index = 0U; index < length; ++index)
	{
		crc ^= data[index];
		for (bit = 0U; bit < 8U; ++bit)
		{
			if ((crc & 1U) != 0U)
			{
				crc = (uint16_t)((crc >> 1) ^ 0xA001U);
			}
			else
			{
				crc >>= 1;
			}
		}
	}
	return crc;
}

static uint16_t StorageLog_RecordSpan(uint16_t payload_length)
{
	uint16_t span = (uint16_t)(sizeof(STORAGE_LOG_HEADER) + payload_length);
	if ((span & 1U) != 0U)
	{
		++span;
	}
	return span;
}

static uint8_t StorageLog_IsBlank(uint32_t addr, uint16_t length)
{
	uint16_t offset;

	for (offset = 0U; offset < length; offset = (uint16_t)(offset + 2U))
	{
		if (*((volatile uint16_t *)(addr + offset)) != 0xFFFFU)
		{
			return 0U;
		}
	}
	return 1U;
}

static uint8_t StorageLog_ReadRecord(uint32_t addr,
									 uint32_t magic,
									 uint16_t length,
									 uint8_t *payload,
									 uint32_t *sequence)
{
	const STORAGE_LOG_HEADER *header = (const STORAGE_LOG_HEADER *)addr;
	const uint8_t *payload_addr = (const uint8_t *)(addr + sizeof(STORAGE_LOG_HEADER));

	if ((header->magic != magic) ||
		(header->version != STORAGE_LOG_RECORD_VERSION) ||
		(header->length != length))
	{
		return 0U;
	}
	if (StorageLog_CalcCrc(payload_addr, length) != header->crc)
	{
		return 0U;
	}

	if (payload != 0)
	{
		memcpy(payload, payload_addr, length);
	}
	if (sequence != 0)
	{
		*sequence = header->sequence;
	}
	return 1U;
}

static uint8_t StorageLog_ErasePage(uint32_t page_addr)
{
	FLASH_Status result;
	uint32_t offset;

	FLASH_Unlock();
	FLASH_ClearFlag(FLASH_FLAG_EOP | FLASH_FLAG_PGERR | FLASH_FLAG_WRPRTERR);
	result = FLASH_ErasePage(page_addr);
	FLASH_Lock();
	if (result != FLASH_COMPLETE)
	{
		System_ERROR_UserCallback(ERROR_EEPROM_STORE);
		return 0U;
	}

	for (offset = 0U; offset < FLASH_STORAGE_PAGE_SIZE; offset += 2U)
	{
		if (*((volatile uint16_t *)(page_addr + offset)) != 0xFFFFU)
		{
			System_ERROR_UserCallback(ERROR_EEPROM_STORE);
			return 0U;
		}
	}
	return 1U;
}

static FLASH_Status StorageLog_ProgramBytes(uint32_t addr, const uint8_t *data, uint16_t length)
{
	uint16_t offset = 0U;
	uint16_t value;
	FLASH_Status result = FLASH_COMPLETE;

	while (offset < length)
	{
		value = data[offset];
		if ((uint16_t)(offset + 1U) < length)
		{
			value |= (uint16_t)((uint16_t)data[offset + 1U] << 8);
		}
		else
		{
			value |= 0xFF00U;
		}

		result = FLASH_ProgramHalfWord(addr + offset, value);
		if ((result != FLASH_COMPLETE) ||
			(*((volatile uint16_t *)(addr + offset)) != value))
		{
			return FLASH_ERROR_PG;
		}
		offset = (uint16_t)(offset + 2U);
	}
	return result;
}

static uint8_t StorageLog_ProgramRecord(uint32_t addr,
										uint32_t magic,
										const uint8_t *payload,
										uint16_t length,
										uint32_t sequence)
{
	STORAGE_LOG_HEADER header;
	uint8_t verify[sizeof(STORAGE_LOG_BASE_DATA)];
	uint32_t verify_sequence = 0U;
	FLASH_Status result;

	if ((payload == 0) || (length > sizeof(verify)))
	{
		return 0U;
	}

	header.magic = magic;
	header.version = STORAGE_LOG_RECORD_VERSION;
	header.length = length;
	header.sequence = sequence;
	header.crc = StorageLog_CalcCrc(payload, length);
	header.reserved = 0xFFFFU;

	FLASH_Unlock();
	FLASH_ClearFlag(FLASH_FLAG_EOP | FLASH_FLAG_PGERR | FLASH_FLAG_WRPRTERR);
	result = StorageLog_ProgramBytes(addr, (const uint8_t *)&header, (uint16_t)sizeof(header));
	if (result == FLASH_COMPLETE)
	{
		result = StorageLog_ProgramBytes(addr + sizeof(header), payload, length);
	}
	FLASH_Lock();

	if (result != FLASH_COMPLETE)
	{
		System_ERROR_UserCallback(ERROR_EEPROM_STORE);
		return 0U;
	}
	if ((StorageLog_ReadRecord(addr, magic, length, verify, &verify_sequence) == 0U) ||
		(verify_sequence != sequence) ||
		(memcmp(verify, payload, length) != 0))
	{
		System_ERROR_UserCallback(ERROR_EEPROM_STORE);
		return 0U;
	}
	return 1U;
}

static uint8_t StorageLog_FindLatestInBasePage(uint32_t page_addr, uint32_t *sequence)
{
	uint16_t span = StorageLog_RecordSpan((uint16_t)sizeof(STORAGE_LOG_BASE_DATA));
	uint32_t offset;
	uint32_t current_sequence = 0U;
	uint32_t latest_sequence = 0U;
	uint8_t found = 0U;

	for (offset = 0U; (offset + span) <= FLASH_STORAGE_PAGE_SIZE; offset += span)
	{
		if (StorageLog_IsBlank(page_addr + offset, span) != 0U)
		{
			break;
		}
		if (StorageLog_ReadRecord(page_addr + offset,
									 STORAGE_LOG_BASE_MAGIC,
									 (uint16_t)sizeof(STORAGE_LOG_BASE_DATA),
									 0,
									 &current_sequence) != 0U)
		{
			if ((found == 0U) || (current_sequence >= latest_sequence))
			{
				latest_sequence = current_sequence;
				found = 1U;
			}
		}
	}

	if (sequence != 0)
	{
		*sequence = found ? latest_sequence : 0U;
	}
	return found;
}

static uint8_t StorageLog_GetBaseSequence(uint32_t *sequence, uint32_t *current_page)
{
	uint32_t seq_a = 0U;
	uint32_t seq_b = 0U;
	uint8_t valid_a;
	uint8_t valid_b;

	valid_a = StorageLog_FindLatestInBasePage(FLASH_ADDR_STORAGE_LOG_SLOT_A, &seq_a);
	valid_b = StorageLog_FindLatestInBasePage(FLASH_ADDR_STORAGE_LOG_SLOT_B, &seq_b);

	if ((valid_a == 0U) && (valid_b == 0U))
	{
		if (sequence != 0)
		{
			*sequence = 0U;
		}
		if (current_page != 0)
		{
			*current_page = FLASH_ADDR_STORAGE_LOG_SLOT_A;
		}
		return 0U;
	}

	if ((valid_a != 0U) && ((valid_b == 0U) || (seq_a >= seq_b)))
	{
		if (sequence != 0)
		{
			*sequence = seq_a;
		}
		if (current_page != 0)
		{
			*current_page = FLASH_ADDR_STORAGE_LOG_SLOT_A;
		}
	}
	else
	{
		if (sequence != 0)
		{
			*sequence = seq_b;
		}
		if (current_page != 0)
		{
			*current_page = FLASH_ADDR_STORAGE_LOG_SLOT_B;
		}
	}
	return 1U;
}

static uint32_t StorageLog_DeltaPage(uint32_t base_sequence)
{
	return ((base_sequence & 1U) != 0U) ?
		FLASH_ADDR_STORAGE_RESERVED_5 : FLASH_ADDR_STORAGE_RESERVED_7;
}

static void StorageLog_ScanDeltaPage(uint32_t page_addr,
									 uint32_t base_sequence,
									 uint32_t *latest_sequence,
									 uint32_t *next_addr,
									 uint8_t *has_live,
									 uint8_t *has_nonblank)
{
	uint16_t span = StorageLog_RecordSpan((uint16_t)sizeof(STORAGE_LOG_DELTA_DATA));
	uint32_t offset;
	uint32_t record_sequence = 0U;
	uint32_t latest = 0U;
	uint32_t next = page_addr + FLASH_STORAGE_PAGE_SIZE;
	uint8_t live = 0U;
	uint8_t nonblank = 0U;
	STORAGE_LOG_DELTA_DATA data;

	for (offset = 0U; (offset + span) <= FLASH_STORAGE_PAGE_SIZE; offset += span)
	{
		if (StorageLog_IsBlank(page_addr + offset, span) != 0U)
		{
			next = page_addr + offset;
			break;
		}
		nonblank = 1U;
		if (StorageLog_ReadRecord(page_addr + offset,
									 STORAGE_LOG_DELTA_MAGIC,
									 (uint16_t)sizeof(data),
									 (uint8_t *)&data,
									 &record_sequence) != 0U)
		{
			if (data.base_sequence == base_sequence)
			{
				live = 1U;
				if (record_sequence > latest)
				{
					latest = record_sequence;
				}
			}
		}
	}

	if (latest_sequence != 0)
	{
		*latest_sequence = latest;
	}
	if (next_addr != 0)
	{
		*next_addr = next;
	}
	if (has_live != 0)
	{
		*has_live = live;
	}
	if (has_nonblank != 0)
	{
		*has_nonblank = nonblank;
	}
}

static uint8_t StorageLog_ReplayDeltaPage(uint32_t page_addr,
									   uint32_t base_sequence,
									   uint8_t *point,
									   uint8_t records[STORAGE_LOG_RECORD_COUNT][2])
{
	uint16_t span = StorageLog_RecordSpan((uint16_t)sizeof(STORAGE_LOG_DELTA_DATA));
	uint32_t offset;
	STORAGE_LOG_DELTA_DATA data;
	uint32_t record_sequence;
	uint8_t replayed = 0U;

	for (offset = 0U; (offset + span) <= FLASH_STORAGE_PAGE_SIZE; offset += span)
	{
		if (StorageLog_IsBlank(page_addr + offset, span) != 0U)
		{
			break;
		}
		if (StorageLog_ReadRecord(page_addr + offset,
									 STORAGE_LOG_DELTA_MAGIC,
									 (uint16_t)sizeof(data),
									 (uint8_t *)&data,
									 &record_sequence) != 0U)
		{
			(void)record_sequence;
			if (data.base_sequence == base_sequence)
			{
				if (*point >= STORAGE_LOG_RECORD_COUNT)
				{
					*point = 0U;
				}
				records[*point][0] = data.event;
				records[*point][1] = data.delta;
				++(*point);
				replayed = 1U;
			}
		}
	}
	return replayed;
}

static uint8_t StorageLog_WriteBaseSnapshot(uint8_t point,
											const uint8_t records[STORAGE_LOG_RECORD_COUNT][2],
											uint32_t *new_sequence)
{
	uint32_t seq_a = 0U;
	uint32_t seq_b = 0U;
	uint32_t target_page;
	uint32_t sequence;
	STORAGE_LOG_BASE_DATA data;
	uint8_t valid_a;
	uint8_t valid_b;

	valid_a = StorageLog_FindLatestInBasePage(FLASH_ADDR_STORAGE_LOG_SLOT_A, &seq_a);
	valid_b = StorageLog_FindLatestInBasePage(FLASH_ADDR_STORAGE_LOG_SLOT_B, &seq_b);
	if ((valid_a != 0U) && ((valid_b == 0U) || (seq_a >= seq_b)))
	{
		sequence = seq_a + 1U;
		target_page = FLASH_ADDR_STORAGE_LOG_SLOT_B;
	}
	else if (valid_b != 0U)
	{
		sequence = seq_b + 1U;
		target_page = FLASH_ADDR_STORAGE_LOG_SLOT_A;
	}
	else
	{
		sequence = 1U;
		target_page = FLASH_ADDR_STORAGE_LOG_SLOT_A;
	}

	memset(&data, 0, sizeof(data));
	data.point = point;
	memcpy(data.records, records, sizeof(data.records));

	if (StorageLog_ErasePage(target_page) == 0U)
	{
		return 0U;
	}
	if (StorageLog_ProgramRecord(target_page,
									 STORAGE_LOG_BASE_MAGIC,
									 (const uint8_t *)&data,
									 (uint16_t)sizeof(data),
									 sequence) == 0U)
	{
		return 0U;
	}
	if (new_sequence != 0)
	{
		*new_sequence = sequence;
	}
	return 1U;
}

uint8_t Storage_LogLoad(uint8_t *point,
						uint8_t records[STORAGE_LOG_RECORD_COUNT][2])
{
	uint32_t base_sequence;
	uint32_t delta_page;

	if ((point == 0) || (records == 0) || (Storage_IsReady() == 0U))
	{
		return 0U;
	}
	if (Storage_LoadLogData(point, records) == 0U)
	{
		return 0U;
	}
	if (StorageLog_GetBaseSequence(&base_sequence, 0) == 0U)
	{
		return 0U;
	}

	delta_page = StorageLog_DeltaPage(base_sequence);
	(void)StorageLog_ReplayDeltaPage(delta_page, base_sequence, point, records);
	return 1U;
}

uint8_t Storage_LogAppend(uint8_t event, uint8_t delta)
{
	uint32_t base_sequence;
	uint32_t delta_page;
	uint32_t delta_sequence = 0U;
	uint32_t next_addr;
	uint16_t span = StorageLog_RecordSpan((uint16_t)sizeof(STORAGE_LOG_DELTA_DATA));
	uint8_t has_live;
	uint8_t has_nonblank;
	STORAGE_LOG_DELTA_DATA data;

	if ((Storage_IsReady() == 0U) ||
		(StorageLog_GetBaseSequence(&base_sequence, 0) == 0U))
	{
		return 0U;
	}

	delta_page = StorageLog_DeltaPage(base_sequence);
	StorageLog_ScanDeltaPage(delta_page, base_sequence,
							  &delta_sequence, &next_addr,
							  &has_live, &has_nonblank);

	/* The page belongs to an older snapshot generation: recycle it now. */
	if ((has_live == 0U) && (has_nonblank != 0U))
	{
		if (StorageLog_ErasePage(delta_page) == 0U)
		{
			return 0U;
		}
		next_addr = delta_page;
		delta_sequence = 0U;
	}

	if ((next_addr + span) <= (delta_page + FLASH_STORAGE_PAGE_SIZE))
	{
		memset(&data, 0, sizeof(data));
		data.base_sequence = base_sequence;
		data.event = event;
		data.delta = delta;
		return StorageLog_ProgramRecord(next_addr,
									   STORAGE_LOG_DELTA_MAGIC,
									   (const uint8_t *)&data,
									   (uint16_t)sizeof(data),
									   delta_sequence + 1U);
	}
	else
	{
		uint8_t point;
		uint8_t records[STORAGE_LOG_RECORD_COUNT][2];
		uint32_t new_base_sequence;

		if (Storage_LogLoad(&point, records) == 0U)
		{
			return 0U;
		}
		if (point >= STORAGE_LOG_RECORD_COUNT)
		{
			point = 0U;
		}
		records[point][0] = event;
		records[point][1] = delta;
		++point;

		if (StorageLog_WriteBaseSnapshot(point, records, &new_base_sequence) == 0U)
		{
			return 0U;
		}
		(void)new_base_sequence;

		/* Cleanup is not part of commit atomicity. Old deltas are generation-bound. */
		(void)StorageLog_ErasePage(delta_page);
		return 1U;
	}
}

uint8_t Storage_LogClear(void)
{
	uint8_t records[STORAGE_LOG_RECORD_COUNT][2];
	uint32_t new_base_sequence;

	if (Storage_IsReady() == 0U)
	{
		return 0U;
	}

	memset(records, 0, sizeof(records));
	if (StorageLog_WriteBaseSnapshot(0U, records, &new_base_sequence) == 0U)
	{
		return 0U;
	}
	(void)new_base_sequence;

	/* A successful base-sequence advance makes stale deltas harmless immediately. */
	(void)StorageLog_ErasePage(FLASH_ADDR_STORAGE_RESERVED_5);
	(void)StorageLog_ErasePage(FLASH_ADDR_STORAGE_RESERVED_7);
	return 1U;
}

/* --------------------------------------------------------------------------
 * Legacy EEPROM compatibility stubs
 * -------------------------------------------------------------------------- */

UINT8 ReadEEPROM_Byte(UINT16 addr)
{
	(void)addr;
	return 0xFF;
}

UINT8 WriteEEPROM_Byte(UINT16 addr, UINT8 val)
{
	(void)addr;
	(void)val;
	return 0;
}

UINT16 ReadEEPROM_Word_NoZone(UINT16 addr)
{
	(void)addr;
	return 0xFFFF;
}

UINT8 WriteEEPROM_Word_NoZone(UINT16 addr, UINT16 data)
{
	(void)addr;
	(void)data;
	return 0;
}

void InitE2PROM(void)
{
	EEPROM_LoadDefaultRuntimeData();
	EEPROM_LoadRWParametersFromFlash();
	EEPROM_LoadCalibrationFromStorage();
	ReadEEPROM_AFE_Parameters();
	ReadEEPROM_EventRecord_Parameters();
	UpgradeParamPolicy_ApplyOnce();
}
