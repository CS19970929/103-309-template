#ifndef CAN_HDX_H
#define CAN_HDX_H


#define CAN_ADRESS_STD_ID				0x00
#define CANID_RX_COMMON_MSG_FILTER   	((UINT16)0x0000|((UINT16)CAN_ADRESS_STD_ID<<7))
//#define CANID_RX_COMMON_MSG_FILTER   	((UINT16)0x0000)

//单机版，11位的标准帧，高位的4位必须匹配为0bxxxx x000 0xxx xxxx
#define CANID_RX_COMMON_MSG_MASK      	((UINT16)0x0780)



struct CAN_ERROR_SNAPSHOT {
	UINT8 u8LastErrorCode;
	UINT8 u8ReceiveErrorCounter;
	UINT8 u8TransmitErrorCounter;
	UINT8 u8ErrorWarning;
	UINT8 u8ErrorPassive;
	UINT8 u8BusOff;
	UINT16 u16AckErrorCnt;
	UINT16 u16TxFailedCnt;
	UINT16 u16TxTimeoutCnt;
	UINT16 u16TxAbortCnt;
	UINT16 u16TxNoMailboxCnt;
	UINT16 u16BusOffCnt;
};

extern volatile struct CAN_ERROR_SNAPSHOT g_stCanErrorSnapshot;

struct CAN_LOW_POWER_STATUS {
	UINT8 u8PowerState;
	UINT8 u8BusActive;
	UINT8 u8NoAckCnt;
	UINT8 u8ProbeActive;
	UINT8 u8TxMailbox;
	UINT8 u8RtcServiceActive;
	UINT8 u8LastRtcWakeTxAcked;
	UINT8 u8LastRtcWakeTimeout;
	UINT8 u8PeripheralSleepRequested;
	UINT16 u16PendingMask;
	UINT16 u16RtcWakeServiceCnt;
	UINT16 u16PrepareSleepCnt;
	UINT16 u16PeripheralSleepCnt;
	UINT16 u16PeripheralWakeCnt;
	UINT32 u32LogicalTick;
	UINT32 u32LastRtcElapsedSeconds;
};

extern volatile struct CAN_LOW_POWER_STATUS g_stCanLowPowerStatus;


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
		UINT8 b1Status_MOS_CHG      :1;		//充电MOS管功能状态
		UINT8 b1Status_MOS_DSG      :1;		//放电MOS管功能状态
		UINT8 b1Status_MOS_PRE      :1;		//预充MOS管功能状态
		UINT8 b1Status_Relay_CHG    :1;		//分口充电继电器功能状态
		
		UINT8 b1Status_Relay_DSG    :1;		//分口放电继电器功能状态
		UINT8 b1Status_Relay_PRE    :1;		//预充继电器功能状态
		UINT8 b1Status_Heat			:1; 	//
		UINT8 b1Status_Cool			:1; 	//
		
		UINT8 b1Status_Relay_MAIN   :1;		//同口主继电器功能状态
		UINT8 b1Status_Res   		:7;		//同口主继电器功能状态
     }bits;
};


void InitCan(void);
void App_Can(void);
UINT8 Can_HDX_Transmit(CanTxMsg *Msg);
UINT8 Can_IsBusy(void);
void Can_PrepareSleep(void);
UINT8 Can_IsBusActive(void);
UINT32 Can_GetIdleRtcPeriodSeconds(void);
void Can_RtcWakeService(UINT32 elapsed_seconds);

#endif
