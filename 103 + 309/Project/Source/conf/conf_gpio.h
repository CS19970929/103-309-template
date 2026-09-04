#ifndef CONF_GPIO_H
#define CONF_GPIO_H

#include "System_Init.h"
#define __STM32F1__
#include "stm32f10x.h"

/*
 * SH3673520 initial board map.
 * Based on codex/afe-spi-refactor-debug but conflicting aliases were removed:
 * PA4..PA7 are exclusively SPI1 and PA6/PA7 are NOT ALARM/MODE GPIOs.
 */
#define GPIO_CS_SPI                 GPIOA
#define PIN_CS_SPI                  GPIO_Pin_4
#define GPIO_SCLK_SPI               GPIOA
#define PIN_SCLK_SPI                GPIO_Pin_5
#define GPIO_MISO_SPI               GPIOA
#define PIN_MISO_SPI                GPIO_Pin_6
#define GPIO_MOSI_SPI               GPIOA
#define PIN_MOSI_SPI                GPIO_Pin_7

#define GPIO_AFE1_SHIP              GPIOA
#define PIN_AFE1_SHIP               GPIO_Pin_10
#define GPIO_AFE1_CTL               GPIOB
#define PIN_AFE1_CTL                GPIO_Pin_14
#define GPIO_AFE1_PRO_EN            GPIOB
#define PIN_AFE1_PRO_EN             GPIO_Pin_0

/* Reference-board charge/discharge detect inputs. */
#define GPIO_DSG_DET                GPIOA
#define PIN_DSG_DET                 GPIO_Pin_8
#define GPIO_CHG_DET                GPIOA
#define PIN_CHG_DET                 GPIO_Pin_9

/* Reference-board wake/key wiring. */
#define GPIO_INT_WK_MCU             GPIOA
#define PIN_INT_WK_MCU              GPIO_Pin_0
#define GPIO_CHG_IN                 GPIO_INT_WK_MCU /* legacy wake-source name */
#define PIN_CHG_IN                  PIN_INT_WK_MCU
#define GPIO_SW                     GPIOB
#define PIN_SW                      GPIO_Pin_5
#define GPIO_INT_WK_CMNT            GPIOB
#define PIN_INT_WK_CMNT             GPIO_Pin_12

/* Power/control rails retained from the SH3673520 reference branch. */
#define GPIO_M_STB                  GPIOA
#define PIN_M_STB                   GPIO_Pin_15
#define GPIO_AD_EN                  GPIOB
#define PIN_AD_EN                   GPIO_Pin_3
#define GPIO_CMNT_EN                GPIOB
#define PIN_CMNT_EN                 GPIO_Pin_4

/* Legacy aliases kept for source compatibility only. Do not drive these as
 * independent rails: PB4 is CMNT_EN and PB5 is the key input on this board. */
#define GPIO_BLE_EN                 GPIOB
#define PIN_BLE_EN                  GPIO_Pin_4
#define GPIO_SW_EN                  GPIOB
#define PIN_SW_EN                   GPIO_Pin_5

#define GPIO_DBG_LED                GPIOB
#define PIN_DBG_LED                 GPIO_Pin_15
#define MCUO_DEBUG_LED1             PBout(15)

#define GPIO_SCI1_TX                GPIOB
#define PIN_SCI1_TX                 GPIO_Pin_6
#define GPIO_SCI1_RX                GPIOB
#define PIN_SCI1_RX                 GPIO_Pin_7

#define GPIO_ADC_VBUS               GPIOA
#define PIN_ADC_VBUS                GPIO_Pin_1
#define GPIO_ADC_NMOS               GPIOB
#define PIN_ADC_NMOS                GPIO_Pin_1

#define MCUO_AFE_SHIP               PAout(10)
#define MCUO_AFE_VPRO               PBout(0)
#define MCUO_AFE_CTLC               PBout(14)
#define MCUO_DRV_CMNT               PCout(12)
#define MCUO_PWSV_CTR               PCout(13)
#define MCUO_PWSV_STB               PDout(2)

/* ALARM is intentionally not assigned. The reference branch used PA6, which
 * collides with SPI1 MISO. FLAG polling is the mandatory protection path. */

#endif /* CONF_GPIO_H */
