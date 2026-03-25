#ifndef CAN_HDX_H
#define CAN_HDX_H

#define CMD_SET_OUTPUT_A (0x55)
#define CMD_SET_OUTPUT_B (0x66)
#define CMD_RSP_OUTPUT_OK (0x33)
#define CMD_RSP_OUTPUT_FAIL (0x66)
#define CMD_NULL (0x88)

//����ID
#define CANID_TX_Test      	((UINT32)0x001)

#define CANID_CHECK_0x00   ((UINT16)0x00)
#define CANID_CHECK_0x01   ((UINT16)0x01)
#define CANID_CHECK_0x02   ((UINT16)0x02)
#define CANID_CHECK_0x03   ((UINT16)0x03)
#define CANID_CHECK_0x04   ((UINT16)0x04)
#define CANID_CHECK_0x05   ((UINT16)0x05)
#define CANID_CHECK_0x06   ((UINT16)0x06)
#define CANID_CHECK_0x07   ((UINT16)0x07)
#define CANID_CHECK_0x08   ((UINT16)0x08)
#define CANID_CHECK_0x09   ((UINT16)0x09)
#define CANID_CHECK_0x0A   ((UINT16)0x0A)
#define CANID_CHECK_0x0B   ((UINT16)0x0B)
#define CANID_CHECK_0x0C   ((UINT16)0x0C)
#define CANID_CHECK_0x0D   ((UINT16)0x0D)
#define CANID_CHECK_0x0E   ((UINT16)0x0E)
#define CANID_CHECK_0x0F   ((UINT16)0x0F)
#define CANID_CHECK_0x10   ((UINT16)0x10)
#define CANID_CHECK_0x11   ((UINT16)0x11)



#define CAN_ADRESS_STD_ID				0x00
#define CANID_RX_COMMON_MSG_FILTER   	((UINT16)0x0000|((UINT16)CAN_ADRESS_STD_ID<<7))
//#define CANID_RX_COMMON_MSG_FILTER   	((UINT16)0x0000)

//�����棬11λ�ı�׼֡����λ��4λ����ƥ��Ϊ0bxxxx x000 0xxx xxxx
#define CANID_RX_COMMON_MSG_MASK      	((UINT16)0x0780)


union CanTxType_Status {
    UINT32   all;
    struct CanTxType_StatusBit {
	    UINT8 b1CanTx_Test     	:1;
		UINT8 b1CanTx_0x00		:1;
		UINT8 b1CanTx_0x01		:1;
    	UINT8 b1CanTx_0x02		:1;
		
        UINT8 b1CanTx_0x03     	:1;
        UINT8 b1CanTx_0x04     	:1;
        UINT8 b1CanTx_0x05     	:1;
        UINT8 b1CanTx_0x06     	:1;
		
        UINT8 b1CanTx_0x07     	:1;
        UINT8 b1CanTx_0x08    	:1;
        UINT8 b1CanTx_0x09    	:1;
        UINT8 b1CanTx_0x0A    	:1;
		
        UINT8 b1CanTx_0x0B    	:1;
        UINT8 b1CanTx_0x0C     	:1;
        UINT8 b1CanTx_0x0D    	:1;
        UINT8 b1CanTx_0x0E    	:1;

        UINT8 b1CanTx_0x0F    	:1;
        UINT8 b1CanTx_0x10     	:1;
        UINT8 b1CanTx_0x11    	:1;
        UINT8 b1CanTx_0x12    	:1;
		
        UINT8 b1CanTx_Res1      :4;
		UINT8 b1CanTx_Res2      :8;
     }bits;	
};


union Can_Status {
    UINT8   all;
    struct Can_StatusBit {
		UINT8 b1Can_BusOFF			:1;
		UINT8 b1Can_Received 		:1;
		UINT8 b1Can_Send            :1;
		UINT8 b1Can_Fault           :1;

		UINT8 b1Can_BusOFF_TestSd	:1;
		UINT8 b1Rcved				:3;
     }bits;
};


union MDLREPORTFAULT_REG {
    UINT16 all;
    struct MDLREPORTCHGFAULT_BITS {
		UINT8 b1CellOvp 		:1; 	//
		UINT8 b1CellUvp			:1; 	//
		UINT8 b1BatOvp			:1; 	//
		UINT8 b1BatUvp			:1; 	//
		
		UINT8 b1CellChgOtp		:1; 	//
		UINT8 b1CellChgUtp		:1; 	//
		UINT8 b1CellDischgOtp	:1; 	//
		UINT8 b1CellDischgUtp	:1; 	//
		
		UINT8 b1IchgOcp 		:1; 	//
		UINT8 b1IdischgOcp		:1; 	//
		UINT8 b1CBC_Err			:1; 	//
		UINT8 b1AFE_Err			:1; 	//
		
		UINT8 b1Soft_Lock_MOS	:1; 	//
		UINT8 b1VcellDeltaBig 	:1; 	//
		UINT8 b1SocLow 			:1; 	//
		UINT8 b1Charger_Online	:1; 	//
     }bits;
};


union WARNING_REG {
    UINT16 all;
    struct WARNING_BITS {
		UINT8 b1CellOvp 		:1; 	//
		UINT8 b1CellUvp			:1; 	//
		UINT8 b1BatOvp			:1; 	//
		UINT8 b1BatUvp			:1; 	//
		
		UINT8 b1CellChgOtp		:1; 	//
		UINT8 b1CellChgUtp		:1; 	//
		UINT8 b1CellDischgOtp	:1; 	//
		UINT8 b1CellDischgUtp	:1; 	//
		
		UINT8 b1IchgOcp 		:1; 	//
		UINT8 b1IdischgOcp		:1; 	//
		UINT8 b1Rec1			:1; 	//
		UINT8 b1VcellDeltaBig	:1; 	//
		
		UINT8 b1TempDeltaBig	:1; 	//
		UINT8 b1SocLow 			:1; 	//
		UINT8 b1TmosOtp 		:1; 	//
		UINT8 b1Rec2			:1; 	//
     }bits;
};


union SYS_LOSE_REG {
    UINT16 all;
    struct SYS_LOSE_BITS {
		UINT8 b1CellOvp_Err 	:1; 	//
		UINT8 b1CellUvp_Err		:1; 	//
		UINT8 b1MOS_Err			:1; 	//
		UINT8 b1Relay_Err		:1; 	//
		
		UINT8 b1AFE_Err			:1; 	//
		UINT8 b1Sys_Err			:1; 	//
		UINT8 b1Lifetime_Err	:1; 	//
		UINT8 b1Rec1			:1; 	//
		
		UINT8 b1Rec2			:8; 	//
     }bits;
};


union MOS_RELAY_REG {
    UINT16 all;
    struct MOS_RELAY_BITS {
		UINT8 b1Status_MOS_CHG      :1;		//���MOS�ܹ���״̬
		UINT8 b1Status_MOS_DSG      :1;		//�ŵ�MOS�ܹ���״̬
		UINT8 b1Status_MOS_PRE      :1;		//Ԥ��MOS�ܹ���״̬
		UINT8 b1Status_Relay_CHG    :1;		//�ֿڳ��̵�������״̬
		
		UINT8 b1Status_Relay_DSG    :1;		//�ֿڷŵ�̵�������״̬
		UINT8 b1Status_Relay_PRE    :1;		//Ԥ��̵�������״̬
		UINT8 b1Status_Heat			:1; 	//
		UINT8 b1Status_Cool			:1; 	//
		
		UINT8 b1Status_Relay_MAIN   :1;		//ͬ�����̵�������״̬
		UINT8 b1Status_Res   		:7;		//ͬ�����̵�������״̬
     }bits;
};

union FAULT{
    UINT16 all;
    struct Fault_Flag{
	UINT8 b1CellOvp			:1;   	//
	UINT8 b1CellUvp			:1;   	//
	UINT8 b1BatOvp			:1;   	//
	UINT8 b1BatUvp			:1;   	//
	
	UINT8 b1IchgOcp			:1;   	//
	UINT8 b1IdischgOcp		:1;   	//
	UINT8 b1CellChgOtp		:1;   	//
	UINT8 b1CellDischgOtp 	:1;   	//

	UINT8 b1CellChgUtp		:1;   	//
	UINT8 b1CellDischgUtp 	:1;   	//
	UINT8 b1VcellDeltaBig	:1;   	//
	UINT8 b1TempDeltaBig 	:1;   	//这个没有，Res可用

	UINT8 b1SocLow			:1;   	//
	UINT8 b1TmosOtp			:1;   	//
	UINT8 e2p_err	 		:1;   	//
	UINT8 afe_short  		:1;   	//
     }bits;	
};


enum OUTPUT_STATUS
{
	CHG_DSG_ON = 0,
	CHG_ON_DSG_OFF,
	CHG_OFF_DSG_ON,
	CHG_DSG_OFF,

};
//todo 定义fault,battary ok
struct battery_info
{
	uint16_t vcellmin;
	uint16_t soc;
	// uint16_t fault;
	union FAULT fault;
	enum OUTPUT_STATUS   output;
};


enum two_battery_state
{
	s_idle = 0,
	s_master_output,
	s_slave_output,

};

// static struct battery_state
struct battery_state
{
	bool master_online;
	bool slave_online;
	uint8_t comm_cnt;
	struct battery_info master_info;
	struct battery_info slave_info;
	//todo 两者都维护bat_output_num变量，有问题 !!!!得修改逻辑
	uint8_t cmd;
	enum two_battery_state state_master;
	enum two_battery_state state_slave;
	bool  sleep;
	uint16_t try_times_1ms;
	bool battary_ok;

} ;

#define CANID_EXPAND_0    ((UINT32)0x141F0200)
#define CANID_EXPAND_1    ((UINT32)0x141F0201)
#define CANID_EXPAND_2    ((UINT32)0x141F0202)
#define CANID_EXPAND_3    ((UINT32)0x141F0203)
#define CANID_EXPAND_4    ((UINT32)0x141F0204)
#define CANID_EXPAND_5    ((UINT32)0x141F0205)
#define CANID_EXPAND_6    ((UINT32)0x141F0206)
#define CANID_EXPAND_7    ((UINT32)0x141F0207)

/* 电池通信协议常量定义 */
#define BATTERY_CAN_ID            0x14 /* 电池节点ID */
#define BROADCAST_CAN_ID          0x1F /* 广播节点ID */

/* 电池通信Index值 */
#define BATTERY_INDEX_VOLTAGE     0x00 /* 实时电压 */
#define BATTERY_INDEX_CURRENT     0x00 /* 实时电流 */
#define BATTERY_INDEX_CAPACITY    0x01 /* 实时容量 */
#define BATTERY_INDEX_DESIGN_CAP  0x01 /* 设计容量 */
#define BATTERY_INDEX_CHARGE_STAT 0x02 /* 充电状态 */
#define BATTERY_INDEX_SOC         0x02 /* 电量百分比 */
#define BATTERY_INDEX_TEMP        0x02 /* 电池温度 */
#define BATTERY_INDEX_CHARGE_TIME 0x02 /* 剩余充电时间 */
#define BATTERY_INDEX_HEALTH      0x03 /* 健康状态 */
#define BATTERY_INDEX_CYCLE_COUNT 0x03 /* 循环次数 */
#define BATTERY_INDEX_VERSION     0x04 /* 协议版本号 */
#define BATTERY_INDEX_SW_VERSION  0x04 /* 软件版本号 */
#define BATTERY_INDEX_WORK_STATE  0x05 /* 工作状态 */
#define BATTERY_INDEX_FAULT_STATE 0x05 /* 异常状态 */
#define BATTERY_INDEX_FULL_CAP    0x05 /* 完全充电容量 */
#define BATTERY_INDEX_CURR_CAP    0x05 /* 当前剩余容量 */
#define BATTERY_INDEX_DESIGN_CAP2 0x05 /* 电池设计容量 */
#define BATTERY_INDEX_UID         0x06 /* 设备唯一标识码 */
#define BATTERY_INDEX_VENDOR_CODE 0x07 /* 厂商代码 */

/* 电池工作状态位定义 */
#define WORK_STATE_DISCHARGE_MOS_CLOSE  0x01 /* 0:放电 MOS关闭 */
#define WORK_STATE_DISCHARGE_MOS_OPEN   0x02 /* 1:放电 MOS打开 */
#define WORK_STATE_CHARGE_MOS_CLOSE     0x04 /* 0:充电 MOS关闭 */
#define WORK_STATE_CHARGE_MOS_OPEN      0x08 /* 1:充电 MOS打开 */
#define WORK_STATE_CHARGER_DISCONNECTED 0x10 /* 0:充电器没有连接 */
#define WORK_STATE_CHARGER_CONNECTED    0x20 /* 1:充电器连接 */
#define WORK_STATE_BATTERY_CHARGING     0x40 /* 0:电池没有充电 */
#define WORK_STATE_BATTERY_DISCHARGING  0x80 /* 1:电池充电 */

/* 电池异常状态位定义 */
#define FAULT_STATE_CHARGE_OVER_CURRENT 0x01 /* 0:没有充电过流保护 */
#define FAULT_STATE_DISCHARGE_OVER_CURRENT 0x02 /* 1:充电过流保护 */
#define FAULT_STATE_OVER_VOLTAGE        0x04 /* 0:没有过压保护 */
#define FAULT_STATE_UNDER_VOLTAGE       0x08 /* 1:过压保护 */
#define FAULT_STATE_OVER_TEMPERATURE    0x10 /* 0:没有欠压保护 */
#define FAULT_STATE_UNDER_TEMPERATURE   0x20 /* 1:欠压保护 */

void InitCan(void);
void App_Can(void);
void App_CanTest(void);
void feidao_logi(void);

#endif
