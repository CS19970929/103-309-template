#ifndef CONF_GPIO_H
#define CONF_GPIO_H

#include "System_Init.h"
#include "Board_Config.h"

/* Compatibility names only. Edit the physical IO rows in Board_Config.c. */
#define GPIO_CHG_IN       BOARD_IO_GPIO(BOARD_IO_CHG_IN)
#define PIN_CHG_IN        BOARD_IO_PIN(BOARD_IO_CHG_IN)
#define GPIO_INT_WK_CMNT  BOARD_IO_GPIO(BOARD_IO_CMNT_WAKE)
#define PIN_INT_WK_CMNT   BOARD_IO_PIN(BOARD_IO_CMNT_WAKE)
#define GPIO_SW           BOARD_IO_GPIO(BOARD_IO_SWITCH)
#define PIN_SW            BOARD_IO_PIN(BOARD_IO_SWITCH)
#define PIN_AFE1_CTL      BOARD_IO_PIN(BOARD_IO_AFE_CTL)
#define PIN_AFE1_PRO_EN   BOARD_IO_PIN(BOARD_IO_AFE_PRO)
#define GPIO_DBG_LED      BOARD_IO_GPIO(BOARD_IO_DEBUG_LED)
#define PIN_DBG_LED       BOARD_IO_PIN(BOARD_IO_DEBUG_LED)
#define GPIO_RF_EN        BOARD_IO_GPIO(BOARD_IO_RF_EN)
#define PIN_RF_EN         BOARD_IO_PIN(BOARD_IO_RF_EN)
#define GPIO_CMNT_EN      BOARD_IO_GPIO(BOARD_IO_CMNT_EN)
#define PIN_CMNT_EN       BOARD_IO_PIN(BOARD_IO_CMNT_EN)
#define GPIO_ADC_NMOS     BOARD_IO_GPIO(BOARD_IO_MOS_NTC)
#define PIN_ADC_NMOS      BOARD_IO_PIN(BOARD_IO_MOS_NTC)
#define GPIO_M_STB        BOARD_IO_GPIO(BOARD_IO_M_STB)
#define PIN_M_STB         BOARD_IO_PIN(BOARD_IO_M_STB)
#define GPIO_AD_EN        BOARD_IO_GPIO(BOARD_IO_AD_EN)
#define PIN_AD_EN         BOARD_IO_PIN(BOARD_IO_AD_EN)
#define GPIO_BLE_EN       BOARD_IO_GPIO(BOARD_IO_BLE_EN)
#define PIN_BLE_EN        BOARD_IO_PIN(BOARD_IO_BLE_EN)
#define GPIO_SW_EN        BOARD_IO_GPIO(BOARD_IO_SW_EN)
#define PIN_SW_EN         BOARD_IO_PIN(BOARD_IO_SW_EN)

/* Keep bit-band lvalue semantics used by the existing business code. */
#define BOARD_IO_OUTPUT(id) BIT_ADDR((uint32_t)&BOARD_IO_GPIO(id)->ODR, g_board_io[(id)].pin_source)
#define MCUO_DEBUG_LED1    BOARD_IO_OUTPUT(BOARD_IO_DEBUG_LED)
#define MCUO_AFE_VPRO      BOARD_IO_OUTPUT(BOARD_IO_AFE_PRO)
#define MCUO_AFE_CTLC      BOARD_IO_OUTPUT(BOARD_IO_AFE_CTL)
#define MCUI_ENI_DI1       BIT_ADDR((uint32_t)&GPIO_SW->IDR, g_board_io[BOARD_IO_SWITCH].pin_source)

#endif
