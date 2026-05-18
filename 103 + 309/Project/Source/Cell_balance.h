#ifndef CELL_BALANCE_H
#define CELL_BALANCE_H

enum BALANCE_STATE_E {
	BALANCE_ST_INIT = 0,
	BALANCE_ST_MONITOR,
	BALANCE_ST_ODD_ON,
	BALANCE_ST_EVEN_ON,
	BALANCE_ST_OFF,
};

extern enum BALANCE_STATE_E g_enBalanceState;
extern UINT16 g_u16CBnFLAG_ToUpper;
extern UINT8 g_u8CBn_StatusFlag;
extern UINT8 g_u8CBn_AFECloseFlag;

void App_CellBalance(void);

#endif
