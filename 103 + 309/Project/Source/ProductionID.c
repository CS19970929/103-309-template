#include "main.h"
#include "DebugWatch.h"

#if STORAGE_PRODUCT_ID_LENGTH_MAX != PRODUCT_ID_LENGTH_MAX
#error "Product ID storage length mismatch"
#endif

PRODUCTION_ID_INFO ProductionInfor;

#if DEBUG_WATCH_ENABLED
void ProductionID_DebugWatchBind(DEBUG_WATCH_ROOT *watch)
{
	watch->public_data.production = &ProductionInfor;
}
#endif

static void InitProID_DefaultData(void)
{
	UINT8 harewareCount = sizeof(BMS_HARDWARE_VERDION_DEFAULT) > PRODUCT_ID_LENGTH_MAX ? PRODUCT_ID_LENGTH_MAX : sizeof(BMS_HARDWARE_VERDION_DEFAULT);
	UINT8 softwareCount = sizeof(BMS_SOFTWARE_VERDION_DEFAULT) > PRODUCT_ID_LENGTH_MAX ? PRODUCT_ID_LENGTH_MAX : sizeof(BMS_SOFTWARE_VERDION_DEFAULT);
	UINT8 serialNumberCount = sizeof(BMS_SERIAL_NUMBER_DEFAULT) > PRODUCT_ID_LENGTH_MAX ? PRODUCT_ID_LENGTH_MAX : sizeof(BMS_SERIAL_NUMBER_DEFAULT);

	memset(&ProductionInfor, 0, sizeof(PRODUCTION_ID_INFO));
	memcpy(&ProductionInfor.BMS_HardWareVersion[0], BMS_HARDWARE_VERDION_DEFAULT, harewareCount);
	memcpy(&ProductionInfor.BMS_SoftWareVersion[0], BMS_SOFTWARE_VERDION_DEFAULT, softwareCount);
	memcpy(&ProductionInfor.BMS_SerialNumber[0], BMS_SERIAL_NUMBER_DEFAULT, serialNumberCount);

	ProductionInfor.BMS_SerialNumberLength = serialNumberCount;
	ProductionInfor.BMS_HardWareVersionLength = harewareCount;
	ProductionInfor.BMS_SoftWareVersionLength = softwareCount;
}

static UINT8 ProductionID_DataIsValid(const STORAGE_PRODUCT_ID_DATA *data)
{
	if (data == 0)
	{
		return 0U;
	}
	if ((data->serial_number_length > PRODUCT_ID_LENGTH_MAX) ||
		(data->hardware_version_length > PRODUCT_ID_LENGTH_MAX) ||
		(data->software_version_length > PRODUCT_ID_LENGTH_MAX))
	{
		return 0U;
	}
	return 1U;
}

static void ProductionID_BuildStorageData(STORAGE_PRODUCT_ID_DATA *data)
{
	memcpy(data->serial_number, ProductionInfor.BMS_SerialNumber, PRODUCT_ID_LENGTH_MAX);
	memcpy(data->hardware_version, ProductionInfor.BMS_HardWareVersion, PRODUCT_ID_LENGTH_MAX);
	memcpy(data->software_version, ProductionInfor.BMS_SoftWareVersion, PRODUCT_ID_LENGTH_MAX);
	data->serial_number_length = ProductionInfor.BMS_SerialNumberLength;
	data->hardware_version_length = ProductionInfor.BMS_HardWareVersionLength;
	data->software_version_length = ProductionInfor.BMS_SoftWareVersionLength;
}

static void ProductionID_ApplyStorageData(const STORAGE_PRODUCT_ID_DATA *data)
{
	memcpy(ProductionInfor.BMS_SerialNumber, data->serial_number, PRODUCT_ID_LENGTH_MAX);
	memcpy(ProductionInfor.BMS_HardWareVersion, data->hardware_version, PRODUCT_ID_LENGTH_MAX);
	memcpy(ProductionInfor.BMS_SoftWareVersion, data->software_version, PRODUCT_ID_LENGTH_MAX);
	ProductionInfor.BMS_SerialNumberLength = data->serial_number_length;
	ProductionInfor.BMS_HardWareVersionLength = data->hardware_version_length;
	ProductionInfor.BMS_SoftWareVersionLength = data->software_version_length;
}

void InitProID(void)
{
	STORAGE_PRODUCT_ID_DATA data;

	if ((Storage_LoadProductIdData(&data) != 0U) &&
		(ProductionID_DataIsValid(&data) != 0U))
	{
		memset(&ProductionInfor, 0, sizeof(ProductionInfor));
		ProductionID_ApplyStorageData(&data);
		return;
	}

	InitProID_DefaultData();
	ProductionID_BuildStorageData(&data);
	(void)Storage_SaveProductIdData(&data);
}

UINT8 ProductionID_UpdateField(UINT8 field, const UINT8 *data, UINT16 length)
{
	STORAGE_PRODUCT_ID_DATA candidate;
	UINT8 *target;
	UINT16 *target_length;

	if ((data == 0) || (length > PRODUCT_ID_LENGTH_MAX))
	{
		return 0U;
	}

	ProductionID_BuildStorageData(&candidate);
	switch (field)
	{
	case PRODUCTION_ID_FIELD_SERIAL_NUMBER:
		target = candidate.serial_number;
		target_length = &candidate.serial_number_length;
		break;
	case PRODUCTION_ID_FIELD_HARDWARE_VERSION:
		target = candidate.hardware_version;
		target_length = &candidate.hardware_version_length;
		break;
	case PRODUCTION_ID_FIELD_SOFTWARE_VERSION:
		target = candidate.software_version;
		target_length = &candidate.software_version_length;
		break;
	default:
		return 0U;
	}

	memset(target, 0, PRODUCT_ID_LENGTH_MAX);
	memcpy(target, data, length);
	*target_length = length;
	if (ProductionID_DataIsValid(&candidate) == 0U)
	{
		return 0U;
	}

	/* Persist first so a failed write never changes the live identity. */
	if (Storage_SaveProductIdData(&candidate) == 0U)
	{
		return 0U;
	}
	ProductionID_ApplyStorageData(&candidate);
	return 1U;
}

void WriteProID_Default(void)
{
	InitProID_DefaultData();
}

void App_ProID_Deal(void)
{
	/* Production ID is initialized during boot; keep this hook for runtime heartbeat. */
}
