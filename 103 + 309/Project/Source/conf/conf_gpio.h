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
#define MCUO_DEBUG_LED1 	PBout(15)		//LED1

#define MCUO_AFE_VPRO 		PBout(0)		//AFE_VPRO
#define MCUO_AFE_CTLC 		PBout(14)		//��������


#define GPIO_SCI1_TX	     GPIOB
#define PIN_SCI1_TX	     GPIO_Pin_6

#define GPIO_SCI1_RX	     GPIOB
#define PIN_SCI1_RX	     GPIO_Pin_7
/**********************************************/
//todo 注意rtc休眠唤醒前后，外设、io状态，power
#define GPIO_CHG_IN                 GPIOA                  //5v充电识别信号；充电唤醒？什么沿；充电关放电；
#define PIN_CHG_IN                  GPIO_Pin_0

#define GPIO_INT_WK_CMNT            GPIOB                  //todo can唤醒
#define PIN_INT_WK_CMNT             GPIO_Pin_12

#define GPIO_MCC_C                  GPIOA                   //mcu控制的充电管。双重保护？
#define PIN_MCC_C                   GPIO_Pin_8

#define GPIO_MCU_WK                 GPIOB                   //typeC芯片唤醒mcu？？，高电平代表typec有负载？
#define PIN_MCU_WK                  GPIO_Pin_13

#define GPIO_SW                     GPIOA
#define PIN_SW                      GPIO_Pin_9

#define GPIO_AFE1_CTL        GPIOB
#define PIN_AFE1_CTL         GPIO_Pin_14

#define GPIO_DC_EN                  GPIOA                   //(power)todo tyepc供电
#define PIN_DC_EN                   GPIO_Pin_10

#define GPIO_DBG_LED        GPIOB
#define PIN_DBG_LED         GPIO_Pin_15

#define GPIO_SPI_MOSI        GPIOA                          //todo 实际对应led控制
#define PIN_SPI_MOSI         GPIO_Pin_6

#define GPIO_RF_EN                  GPIOA                   //todo !!!熔断保险丝控制io
#define PIN_RF_EN                   GPIO_Pin_7

#define GPIO_AFE1_PRO_EN        GPIOB
#define PIN_AFE1_PRO_EN         GPIO_Pin_0

#define GPIO_ADC_VBUS               GPIOA                   //adc输入采样，总压
#define PIN_ADC_VBUS                GPIO_Pin_1

#define GPIO_SPI1_NSS        GPIOA
#define PIN_SPI1_NSS         GPIO_Pin_4

#define GPIO_SPI1_SCK        GPIOA
#define PIN_SPI1_SCK         GPIO_Pin_5

#define GPIO_ADC_NMOS        GPIOB
#define PIN_ADC_NMOS         GPIO_Pin_1

#define GPIO_ADC_CUR        GPIOA
#define PIN_ADC_CUR         GPIO_Pin_2
// #define GPIO_ADC_NTC        GPIOA
// #define PIN_ADC_NTC         GPIO_Pin_3

#define GPIO_2727_EN                GPIOA                   //(power)
#define PIN_2737_EN                 GPIO_Pin_3

#define GPIO_SEG_EN          GPIOB
#define PIN_SEG_EN           GPIO_Pin_10

#define GPIO_M_STB                  GPIOA                   //(power)
#define PIN_M_STB                   GPIO_Pin_15

#define GPIO_AD_EN                  GPIOB                   //todo (power) ???
#define PIN_AD_EN                   GPIO_Pin_3

#define GPIO_CMNT_EN                GPIOB                   //(power) can供电？？？
#define PIN_CMNT_EN                 GPIO_Pin_4

#define GPIO_ADC_BUS_EN             GPIOB                   //(power)
#define PIN_ADC_BUS_EN              GPIO_Pin_5

#endif
