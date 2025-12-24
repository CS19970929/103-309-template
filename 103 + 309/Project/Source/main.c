#include "main.h"
#include "uavcan.equipment.power.BatteryInfo.h"

#define PREFERRED_NODE_ID 73
/*
  in this example we will use dynamic node allocation if MY_NODE_ID is zero
 */
#define MY_NODE_ID 0

#define BATTERY_MANUFACTURER_NAME "Example Battery Co."

enum uavcan_protocol_param_Value_type_t
{

	UAVCAN_PROTOCOL_PARAM_VALUE_EMPTY,

	UAVCAN_PROTOCOL_PARAM_VALUE_INTEGER_VALUE,

	UAVCAN_PROTOCOL_PARAM_VALUE_REAL_VALUE,

	UAVCAN_PROTOCOL_PARAM_VALUE_BOOLEAN_VALUE,

	UAVCAN_PROTOCOL_PARAM_VALUE_STRING_VALUE,

};

struct uavcan_protocol_NodeStatus {

#if defined(__cplusplus) && defined(DRONECAN_CXX_WRAPPERS)
    using cxx_iface = uavcan_protocol_NodeStatus_cxx_iface;
#endif




    uint32_t uptime_sec;



    uint8_t health;



    uint8_t mode;



    uint8_t sub_mode;



    uint16_t vendor_specific_status_code;



};
/*
  keep the state of the battery
 */
static struct battery_state
{
	float current;
	float voltage;
	float temperature_K;
	float remaining_capacity;
	float total_capacity_Ah;
	float consumed_Ah;
} battery;

/*
  state of user settings. This will be saved in settings.dat. On a
  real device a better storage system will be needed
  For simplicity we store all parameters as floats in this example
 */
static struct
{
	float can_node;
	float battery_index;
	float telem_rate;
} settings;

/*
  a set of parameters to present to the user. In this example we don't
  actually save parameters, this is just to show how to handle the
  parameter protocol
 */
static struct parameter
{
	char *name;
	enum uavcan_protocol_param_Value_type_t type;
	float *value;
	float min_value;
	float max_value;
} parameters[] = {
	// add any parameters you want users to be able to set
	{"CAN_NODE", UAVCAN_PROTOCOL_PARAM_VALUE_INTEGER_VALUE, &settings.can_node, 0, 127},		  // CAN node ID
	{"BATTERY_INDEX", UAVCAN_PROTOCOL_PARAM_VALUE_INTEGER_VALUE, &settings.battery_index, 0, 32}, // index in RawCommand
	{"TELEM_RATE", UAVCAN_PROTOCOL_PARAM_VALUE_INTEGER_VALUE, &settings.telem_rate, 0, 32},		  // index in RawCommand
};

// some convenience macros
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define ARRAY_SIZE(x) (sizeof(x) / sizeof(x[0]))

/*
  hold our node status as a static variable. It will be updated on any errors
 */
static struct uavcan_protocol_NodeStatus node_status;

UINT8 SeriesNum = 16;

// 不同串数维护的表格
// 中颖
const unsigned char SeriesSelect_AFE1[16][16] = {
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},	   // 1串
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},	   // 2串
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},	   // 3
	{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},	   // 4
	{0, 1, 2, 3, 4, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},	   // 5
	{0, 1, 2, 3, 4, 5, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},	   // 6
	{0, 1, 2, 3, 4, 5, 6, 0, 0, 0, 0, 0, 0, 0, 0, 0},	   // 7
	{0, 1, 2, 3, 4, 5, 6, 7, 0, 0, 0, 0, 0, 0, 0, 0},	   // 8
	{0, 1, 2, 3, 4, 5, 6, 7, 8, 0, 0, 0, 0, 0, 0, 0},	   // 9
	{0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 0, 0, 0, 0, 0, 0},	   // 10
	{0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 0, 0, 0, 0, 0},	   // 11
	{0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 0, 0, 0, 0},	   // 12
	{0, 1, 2, 3, 4, 5, 6, 7, 9, 9, 10, 11, 12, 0, 0, 0},   // 13
	{0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 0, 0},  // 14
	{0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 0}, // 15
	{0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15} // 16
};

void InitVar(void);
void InitDevice(void);
void InitSci(void);
void App_Sci(void);
void InitSystemWakeUp(void);

int main(void)
{
	InitDevice(); // 初始化外设
	InitVar();	  // 初始化变量
	while (1)
	{
#if (defined _DEBUG_CODE)
#else
		App_SysTime();
		App_AFEGet();

		App_Sci();
		App_AnlogCal();
		App_E2promDeal();
		App_CellBalance();
		App_Can();
		// App_SleepDeal(); // 关闭这个功能的话，在InitVar()中System_OnOFF_Func相关置零，或者直接屏蔽
		sleep();
		App_SOC();

#ifdef __FUNC__HEAT__
		App_Heat_Cool_Ctrl();
#endif
		// APP_LedBar();
		App_FlashUpdate();
		App_LogRecord();
		App_ProID_Deal();
		// __delay_ms(1000);
		test_dronecan();

#ifdef wdog_enable
		Feed_IWatchDog;

#endif
#endif
	}
}

void InitDevice(void)
{
	SystemInit(); // HSE默认倍频到72MHz，如果没HSE切回HSI怎么处理目前还没了解

#if (defined _DEBUG_CODE)
	InitDelay();
#else
	InitDelay();

	IsSleepStartUp();

	InitIO();
#ifdef ELOG_OUTPUT_ENABLE
	InitUSART_CommonUpper();
	elogInit();
#endif

	jtag_disableAndConfIO();

	InitNVIC();
	InitSystemWakeUp();
	InitE2PROM(); // 决定把这个放在前面，优先级提高，因为客户串口初始化，有可能要读其自己的数据
	InitAFE1();
	InitCan();
	InitADC();
	InitSci();

#ifdef __FUNC__HEAT__
	InitHeat_Cool();
#endif

	InitMosRelay_DOx();
	InitData_SOC(); // 必须放在读完eeprom数据后面
	extern void LedBar_Init(void);
	LedBar_Init();

	//???充电
	Driver_Element.DriverForceExt.bits.b2_Force_MOS_DSG = FORCE_CLOSE_MODE;
	Driver_Element.DriverForceExt.bits.b2_Force_MOS_CHG = FORCE_CLOSE_MODE;

#ifdef wdog_enable
	Init_IWDG();
#endif // !1
	dronecan_init();

	InitTimer();

#endif
}

void InitVar(void)
{
	// SystemMonitorResetData_EEPROM();							//这个函数的初始化默认需求功能修改了，要修改EEPROM的上电标志位
	InitSystemMonitorData_EEPROM();
	SeriesNum = OtherElement.u16Sys_SeriesNum;
	g_u32CS_Res_AFE = ((UINT32)OtherElement.u16Sys_CS_Res_Num * 1000) / OtherElement.u16Sys_CS_Res;

	SystemStatus.bits.b1StartUpBMS = 0; // 去掉开机时序
	SystemStatus.bits.b1Status_ToSleep = 1;

	// SystemStatus.bits.b4Status_ProjectVer = 1;
	LogRecord_Flag.bits.Log_StartUp = 1;
}

void InitSystemWakeUp(void)
{
}

void InitSci(void)
{
	InitUSART_CommonUpper();
}

void App_Sci(void)
{
	App_CommonUpper();
}

static CanardInstance canard;
static uint8_t memory_pool[200];

static void onTransferReceived(CanardInstance *ins, CanardRxTransfer *transfer)
{
#if 0
    // switch on data type ID to pass to the right handler function
    if (transfer->transfer_type == CanardTransferTypeRequest) {
        // check if we want to handle a specific service request
        switch (transfer->data_type_id) {
        case UAVCAN_PROTOCOL_GETNODEINFO_ID: {
            handle_GetNodeInfo(ins, transfer);
            break;
        }
        case UAVCAN_PROTOCOL_PARAM_GETSET_ID: {
            handle_param_GetSet(ins, transfer);
            break;
        }
        case UAVCAN_PROTOCOL_PARAM_EXECUTEOPCODE_ID: {
            handle_param_ExecuteOpcode(ins, transfer);
            break;
        }
        case UAVCAN_PROTOCOL_RESTARTNODE_ID: {
            handle_RestartNode(ins, transfer);
            break;
        }
        }
    }
    if (transfer->transfer_type == CanardTransferTypeBroadcast) {
        // check if we want to handle a specific broadcast message
        switch (transfer->data_type_id) {
        case UAVCAN_PROTOCOL_DYNAMIC_NODE_ID_ALLOCATION_ID: {
            handle_DNA_Allocation(ins, transfer);
            break;
        }
        }
    }
#endif
}

static bool shouldAcceptTransfer(const CanardInstance *ins,
								 uint64_t *out_data_type_signature,
								 uint16_t data_type_id,
								 CanardTransferType transfer_type,
								 uint8_t source_node_id)
{
#if 0
    if (transfer_type == CanardTransferTypeRequest) {
        // check if we want to handle a specific service request
        switch (data_type_id) {
        case UAVCAN_PROTOCOL_GETNODEINFO_ID: {
            *out_data_type_signature = UAVCAN_PROTOCOL_GETNODEINFO_REQUEST_SIGNATURE;
            return true;
        }
        case UAVCAN_PROTOCOL_PARAM_GETSET_ID: {
            *out_data_type_signature = UAVCAN_PROTOCOL_PARAM_GETSET_SIGNATURE;
            return true;
        }
        case UAVCAN_PROTOCOL_PARAM_EXECUTEOPCODE_ID: {
            *out_data_type_signature = UAVCAN_PROTOCOL_PARAM_EXECUTEOPCODE_SIGNATURE;
            return true;
        }
        case UAVCAN_PROTOCOL_RESTARTNODE_ID: {
            *out_data_type_signature = UAVCAN_PROTOCOL_RESTARTNODE_SIGNATURE;
            return true;
        }
        }
    }
    if (transfer_type == CanardTransferTypeBroadcast) {
        // see if we want to handle a specific broadcast packet
        switch (data_type_id) {
        case UAVCAN_PROTOCOL_DYNAMIC_NODE_ID_ALLOCATION_ID: {
            *out_data_type_signature = UAVCAN_PROTOCOL_DYNAMIC_NODE_ID_ALLOCATION_SIGNATURE;
            return true;
        }
        }
    }
    // we don't want any other messages
    return false;
#endif
}

static void send_BatteryInfo(void)
{
	struct uavcan_equipment_power_BatteryInfo pkt;
	memset(&pkt, 0, sizeof(pkt));
	uint8_t buffer[UAVCAN_EQUIPMENT_POWER_BATTERYINFO_MAX_SIZE];

	// make up some synthetic status data
	pkt.temperature = battery.temperature_K;
	pkt.voltage = battery.voltage;
	pkt.current = battery.current;

	/*
	  Note!! fill in all remaining fields from the DSDL
	 */

	pkt.battery_id = settings.battery_index;
	pkt.model_instance_id = 0;
	pkt.model_name.len = strlen(BATTERY_MANUFACTURER_NAME);
	strncpy((char *)pkt.model_name.data, BATTERY_MANUFACTURER_NAME, sizeof(pkt.model_name.data));

	uint32_t len = uavcan_equipment_power_BatteryInfo_encode(&pkt, buffer);

	// we need a static variable for the transfer ID. This is
	// incremeneted on each transfer, allowing for detection of packet
	// loss
	static uint8_t transfer_id;

	canardBroadcast(&canard,
					UAVCAN_EQUIPMENT_POWER_BATTERYINFO_SIGNATURE,
					UAVCAN_EQUIPMENT_POWER_BATTERYINFO_ID,
					&transfer_id,
					CANARD_TRANSFER_PRIORITY_LOW,
					buffer,
					len);
}

void dronecan_init(void)
{
	settings.can_node = MY_NODE_ID;
	settings.battery_index = 0;
	settings.telem_rate = 10;

	// load_settings();

	/*
	 Initializing the Libcanard instance.
	 */
	canardInit(&canard,
			   memory_pool,
			   sizeof(memory_pool),
			   onTransferReceived,
			   shouldAcceptTransfer,
			   NULL);

	if (settings.can_node > 0)
	{
		canardSetLocalNodeID(&canard, settings.can_node);
	}
	else
	{
		printf("Waiting for DNA node allocation\n");
	}
}

int test_dronecan(void)
{
	if(!g_st_SysTimeFlag.bits.b1Sys1000msFlag1)
		return;

	// while (true)
	{
		{
			// process1HzTasks(ts);
		}
		{
			send_BatteryInfo();
			// battery_update(dt_us * 1.0e-6);
		}
	}

	return 0;
}