#ifndef IO_CONTROL_H
#define IO_CONTROL_H

// #include "IODrivers_030.h"
#include "IODrivers.h"

bool is_charger_online(void);
bool is_load_online(void);
// inline bool is_charger_online(void)
// {
// 	if (0 == GPIO_ReadInputDataBit(GPIO_CHG_DET, PIN_CHG_DET))
// 		return true;

// 	return false;
// }
// inline bool is_load_online(void)
// {
// 	if (0 == GPIO_ReadInputDataBit(GPIO_DSG_DET, PIN_DSG_DET))
// 		return true;

// 	return false;
// }

enum system_status
{
	S_DSG = 0,
	S_CHG,
	S_CHARGESIG,
	S_STARTUP,
	S_IDLE,
	S_PRECHG,
};

extern enum system_status bms_status;

/*
 * Split-port charger control.
 *
 * Drivers_External_Ctrl() is called after every successful 200 ms AFE update.
 * Voltage units follow stCell_Info:
 *   pack voltage: 0.01 V, cell voltage: mV, current: 0.1 A.
 */
#define CHARGE_CTRL_PERIOD_MS                 ((UINT32)200u)
#define CHARGE_CTRL_DET_CONFIRM_MS            ((UINT32)1000u)
#define CHARGE_CTRL_PROBE_IDLE_MS             ((UINT32)(5u * 60u * 1000u))
#define CHARGE_CTRL_PROBE_SETTLE_MS           ((UINT32)1000u)
#define CHARGE_CTRL_PROBE_SAMPLE_MS           ((UINT32)1000u)
#define CHARGE_CTRL_FULL_TAPER_CONFIRM_MS     ((UINT32)(60u * 1000u))
#define CHARGE_CTRL_FULL_VOLT_CONFIRM_MS      ((UINT32)(10u * 60u * 1000u))
#define CHARGE_CTRL_RECHARGE_CONFIRM_MS       ((UINT32)(60u * 1000u))

#define CHARGE_CTRL_DET_CONFIRM_CNT           ((UINT16)(CHARGE_CTRL_DET_CONFIRM_MS / CHARGE_CTRL_PERIOD_MS))
#define CHARGE_CTRL_PROBE_IDLE_CNT            ((UINT16)(CHARGE_CTRL_PROBE_IDLE_MS / CHARGE_CTRL_PERIOD_MS))
#define CHARGE_CTRL_PROBE_SETTLE_CNT          ((UINT16)(CHARGE_CTRL_PROBE_SETTLE_MS / CHARGE_CTRL_PERIOD_MS))
#define CHARGE_CTRL_PROBE_SAMPLE_CNT          ((UINT16)(CHARGE_CTRL_PROBE_SAMPLE_MS / CHARGE_CTRL_PERIOD_MS))
#define CHARGE_CTRL_FULL_TAPER_CONFIRM_CNT    ((UINT16)(CHARGE_CTRL_FULL_TAPER_CONFIRM_MS / CHARGE_CTRL_PERIOD_MS))
#define CHARGE_CTRL_FULL_VOLT_CONFIRM_CNT     ((UINT16)(CHARGE_CTRL_FULL_VOLT_CONFIRM_MS / CHARGE_CTRL_PERIOD_MS))
#define CHARGE_CTRL_RECHARGE_CONFIRM_CNT      ((UINT16)(CHARGE_CTRL_RECHARGE_CONFIRM_MS / CHARGE_CTRL_PERIOD_MS))

#define CHARGE_CTRL_TARGET_SERIES             ((UINT8)19u)
#define CHARGE_CTRL_CHARGER_PACK_CV           ((UINT16)7980u)
#define CHARGE_CTRL_FULL_PACK_CV              ((UINT16)7900u)
#define CHARGE_CTRL_FULL_CELL_MV              ((UINT16)4180u)
#define CHARGE_CTRL_FULL_TAPER_CURRENT_A10    ((UINT16)10u)
#define CHARGE_CTRL_CHARGE_EVIDENCE_A10       ((UINT16)5u)
#define CHARGE_CTRL_PROBE_MAX_PACK_CV         ((UINT16)7800u)
#define CHARGE_CTRL_RECHARGE_PACK_CV          ((UINT16)7700u)
#define CHARGE_CTRL_RECHARGE_CELL_MV          ((UINT16)4050u)

typedef enum CHARGE_CTRL_STATE
{
	CHARGE_CTRL_WAIT_CHARGER = 0,
	CHARGE_CTRL_CHARGING,
	CHARGE_CTRL_PROBE_OFF,
	CHARGE_CTRL_PROBE_SAMPLE,
	CHARGE_CTRL_FULL_HOLD,
	CHARGE_CTRL_FAULT_HOLD
} ChargeCtrlState;

typedef enum CHARGER_PRESENCE
{
	CHARGER_PRESENCE_UNKNOWN = 0,
	CHARGER_PRESENCE_ABSENT,
	CHARGER_PRESENCE_PRESENT
} ChargerPresence;

typedef enum CHARGE_CLOSE_REASON
{
	CHARGE_CLOSE_NONE = 0,
	CHARGE_CLOSE_NO_CHARGER,
	CHARGE_CLOSE_PROBE,
	CHARGE_CLOSE_FULL,
	CHARGE_CLOSE_PROTECTION,
	CHARGE_CLOSE_DATA_INVALID,
	CHARGE_CLOSE_AFE_COMM
} ChargeCloseReason;

typedef struct CHARGE_CTRL_DIAG
{
	ChargeCtrlState state;
	ChargerPresence presence;
	ChargeCloseReason close_reason;
	UINT8 charge_request;
	UINT8 chg_det_low;
	UINT16 det_low_cnt;
	UINT16 det_high_cnt;
	UINT16 no_charge_cnt;
	UINT16 full_taper_cnt;
	UINT16 full_voltage_cnt;
	UINT16 recharge_cnt;
} ChargeCtrlDiag;

extern volatile ChargeCtrlDiag g_charge_ctrl_diag;
void ChargeCtrl_ForceOff(ChargeCloseReason reason);

typedef enum _IO_STATUS {
OPEN = 1, CLOSE = 0
}IO_STATUS;


typedef enum _FUNC_STATUS {
CONT = 0, RECOVER = 1
}FUNC_STATUS;


enum RELAY_CTRL_STATUS {
	RELAY_PRE_DET,	
	RELAY_PRE_OPEN,
	RELAY_PRE_CLOSE,
	RELAY_MAIN_OPEN,
	RELAY_MAIN_CLOSE,
	RELAY_ALL_CLOSE
};


enum MOS_CTRL_STATUS {
	MOS_PRE_DET,	
	MOS_PRE_OPEN,
	MOS_PRE_CLOSE,
	MOS_MAIN_OPEN,
	MOS_MAIN_CLOSE,
	MOS_ALL_CLOSE
};


union Switch_OnOFF_Function {
    UINT32 all;
    struct Switch_OnOFF_Ctrl {
		UINT8 b1Switch1			:1;
		UINT8 b1Switch2			:1;
		UINT8 b1Switch3			:1;
		UINT8 b1Switch4			:1;
		UINT8 b1Switch5			:1;
		UINT8 b1Switch6			:1;
		UINT8 b1Switch7			:1;
		UINT8 b1Switch8			:1;

		UINT8 b1Switch9			:1;
		UINT8 b1Switch10		:1;
		UINT8 b1Switch11		:1;
		UINT8 b1Switch12		:1;
		UINT8 b1Switch13		:1;
		UINT8 b1Switch14		:1;
		UINT8 b1Switch15		:1;
		UINT8 b1Switch16		:1;

		UINT8 b1Switch17		:1;
		UINT8 b1Switch18		:1;
		UINT8 b1Switch19		:1;
		UINT8 b1Switch20		:1;
		UINT8 b1Switch21		:1;
		UINT8 b1Switch22		:1;
		UINT8 b1Switch23		:1;
		UINT8 b1Switch24		:1;

		UINT8 b1Switch25		:1;
		UINT8 b1Switch26		:1;
		UINT8 b1Switch27		:1;
		UINT8 b1Switch28		:1;
		UINT8 b1Switch29		:1;
		UINT8 b1Switch30		:1;
		UINT8 b1Switch31		:1;
		UINT8 b1Switch32		:1;
    }bits;
};


#define PreRelayCloseT 			10			//�����Ӵ�����Ԥ��̵����ر�ʱ��
#define PreDsgMOSCloseT 		10			//�����ŵ�ܺ�Ԥ��ŵ�ܹر�ʱ��


/*
��������ѡһ��
*/
#define _MOS_SAME_DOOR_NO_PRECHG
// #define _MOS_SAME_DOOR_HAVE_PRECHG
//#define _MOS_BOOTSTRAP_CIR

//#define _RELAY_SAME_DOOR_NO_PRECHG
//#define _RELAY_SAME_DOOR_HAVE_PRECHG
//#define _RELAY_DIFF_DOOR_NO_PRECHG
//#define _RELAY_DIFF_DOOR_HAVE_PRECHG


#if (defined _MOS_SAME_DOOR_NO_PRECHG)||(defined _MOS_SAME_DOOR_HAVE_PRECHG)||(defined _MOS_OTHER)||(defined _MOS_BOOTSTRAP_CIR)
#define _MOS
#else
#define _RELAY
#endif



//extern enum RELAY_CTRL_STATUS RelayCtrl_Command;
extern volatile union Switch_OnOFF_Function Switch_OnOFF_Func;
extern UINT8 gu8_DsgFirstOpen_Flag;


void InitMosRelay_DOx(void);
void InitData_Drivers(void);
void App_MOS_Relay_Ctrl(void);

#endif	/* IO_CONTROL_H */

