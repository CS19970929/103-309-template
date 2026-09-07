#ifndef CONF_GPIO_H
#define CONF_GPIO_H

#include "stm32f10x.h"

/* codex/afe-spi-refactor-debug (78b4f5f): active board signals only.
 * PA6/PA7 belong exclusively to SPI. PB14 is M_CCC (charge control).
 * No SHIP/PRO_EN, BLE/SW_EN aliases or unconnected ADC/legacy power GPIOs. */
#define GPIO_CS_SPI       GPIOA
#define PIN_CS_SPI        GPIO_Pin_4
#define GPIO_SCLK_SPI     GPIOA
#define PIN_SCLK_SPI      GPIO_Pin_5
#define GPIO_MISO_SPI     GPIOA
#define PIN_MISO_SPI      GPIO_Pin_6
#define GPIO_MOSI_SPI     GPIOA
#define PIN_MOSI_SPI      GPIO_Pin_7
#define GPIO_M_CCC        GPIOB
#define PIN_M_CCC         GPIO_Pin_14

#define GPIO_INT_WK_MCU   GPIOA
#define PIN_INT_WK_MCU    GPIO_Pin_0
#define GPIO_KEY1         GPIOB
#define PIN_KEY1          GPIO_Pin_5
#define GPIO_INT_WK_CMNT  GPIOB
#define PIN_INT_WK_CMNT   GPIO_Pin_12

#define GPIO_M_STB        GPIOA
#define PIN_M_STB         GPIO_Pin_15
#define GPIO_AD_EN        GPIOB
#define PIN_AD_EN         GPIO_Pin_3
#define GPIO_CMNT_EN      GPIOB
#define PIN_CMNT_EN       GPIO_Pin_4
#define GPIO_DBG_LED      GPIOB
#define PIN_DBG_LED       GPIO_Pin_15

#define GPIO_SCI1_TX      GPIOB
#define PIN_SCI1_TX       GPIO_Pin_6
#define GPIO_SCI1_RX      GPIOB
#define PIN_SCI1_RX       GPIO_Pin_7
#define GPIO_AD_TTC_MOS1  GPIOA
#define PIN_AD_TTC_MOS1   GPIO_Pin_1

#define GPIO_SCI2_TX      GPIOA
#define PIN_SCI2_TX       GPIO_Pin_2
#define GPIO_SCI2_RX      GPIOA
#define PIN_SCI2_RX       GPIO_Pin_3

#endif
