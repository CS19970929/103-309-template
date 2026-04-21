#ifndef CONF_GPIO_H
#define CONF_GPIO_H

#include "System_Init.h"

//#define __STM32F0__
#define __STM32F1__

#ifdef __STM32F0__
#include "stm32f0xx.h"
#endif // __STM32F0__
#ifdef __STM32F1__
#include "stm32f10x.h"
#endif // __STM32F1__


/**********************************************/
#define GPIO_INT_WK_MCU         GPIOA
#define PIN_INT_WK_MCU          GPIO_Pin_0


#define GPIO_SW_EN        GPIOB
#define PIN_SW_EN         GPIO_Pin_5


#define GPIO_AFE1_SHIP        GPIOA
#define PIN_AFE1_SHIP         GPIO_Pin_10

#define GPIO_AFE1_ALM        GPIOA
#define PIN_AFE1_ALM         GPIO_Pin_6

#define GPIO_AFE1_MODE        GPIOA
#define PIN_AFE1_MODE         GPIO_Pin_7



#define MCUO_DEBUG_LED1 	PBout(15)		//LED1

#define MCUO_DRV_CMNT		PCout(12)		//
//��Դģ��
#define MCUO_PWSV_STB		PDout(2)		//

#define MCUO_AFE_SHIP 		PAout(10)		//AFE_SHIP
#define MCUO_AFE_MODE 		PAout(7)		//AFE_MODE
#define MCUO_AFE_VPRO 		PBout(0)		//AFE_VPRO
#define MCUO_AFE_CTLC 		PBout(14)		//��������


#define GPIO_AD_TTC_MOS1             GPIOA 
#define PIN_AD_TTC_MOS1              GPIO_Pin_1

#define GPIO_SCI1_TX	     GPIOB
#define PIN_SCI1_TX	     GPIO_Pin_6

#define GPIO_SCI1_RX	     GPIOB
#define PIN_SCI1_RX	     GPIO_Pin_7


/**********************************************/
#define GPIO_CHG_IN         GPIOA
#define PIN_CHG_IN          GPIO_Pin_0

#define GPIO_INT_WK_CMNT         GPIOB
#define PIN_INT_WK_CMNT          GPIO_Pin_12

#define GPIO_MCC_C         GPIOA
#define PIN_MCC_C          GPIO_Pin_8

#define GPIO_MCU_WK         GPIOB
#define PIN_MCU_WK          GPIO_Pin_13

#define GPIO_SW         GPIOA
#define PIN_SW          GPIO_Pin_9

#define GPIO_AFE1_CTL        GPIOB
#define PIN_AFE1_CTL         GPIO_Pin_14

#define GPIO_DC_EN        GPIOA
#define PIN_DC_EN         GPIO_Pin_10

#define GPIO_DBG_LED        GPIOB
#define PIN_DBG_LED         GPIO_Pin_15

#define GPIO_SPI_MOSI        GPIOA
#define PIN_SPI_MOSI         GPIO_Pin_6

#define GPIO_LED595_DATA     GPIO_SPI_MOSI
#define PIN_LED595_DATA      PIN_SPI_MOSI

#define GPIO_RF_EN        GPIOA
#define PIN_RF_EN         GPIO_Pin_7

#define GPIO_AFE1_PRO_EN        GPIOB
#define PIN_AFE1_PRO_EN         GPIO_Pin_0

#define GPIO_ADC_VBUS        GPIOA
#define PIN_ADC_VBUS         GPIO_Pin_1

#define GPIO_SPI1_NSS        GPIOA
#define PIN_SPI1_NSS         GPIO_Pin_4

#define GPIO_LED595_LATCH    GPIO_SPI1_NSS
#define PIN_LED595_LATCH     PIN_SPI1_NSS

#define GPIO_SPI1_SCK        GPIOA
#define PIN_SPI1_SCK         GPIO_Pin_5

#define GPIO_LED595_CLK      GPIO_SPI1_SCK
#define PIN_LED595_CLK       PIN_SPI1_SCK

#define GPIO_ADC_NMOS        GPIOB
#define PIN_ADC_NMOS         GPIO_Pin_1

#define GPIO_ADC_CUR        GPIOA
#define PIN_ADC_CUR         GPIO_Pin_2

#define GPIO_2727_EN        GPIOA
#define PIN_2737_EN         GPIO_Pin_3

#define GPIO_M_STB          GPIOA
#define PIN_M_STB           GPIO_Pin_15

#define GPIO_AD_EN        GPIOB
#define PIN_AD_EN         GPIO_Pin_3

#define GPIO_CMNT_EN        GPIOB
#define PIN_CMNT_EN         GPIO_Pin_4

#define GPIO_ADC_BUS_EN        GPIOB
#define PIN_ADC_BUS_EN         GPIO_Pin_5

#endif

