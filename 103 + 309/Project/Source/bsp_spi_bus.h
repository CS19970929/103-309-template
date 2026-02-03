/*
*********************************************************************************************************
*
*	ģ������ : SPI��������
*	�ļ����� : bsp_spi_bus.h
*	��    �� : V1.0
*	˵    �� : ͷ�ļ�
*
*	Copyright (C), 2014-2015, ���������� www.armfly.com
*
*********************************************************************************************************
*/

#ifndef __BSP_SPI_BUS_H
#define __BSP_SPI_BUS_H

#include "stdint.h"

#define SOFT_SPI		/* ������б�ʾʹ��GPIOģ��SPI�ӿ� */
// #define HARD_SPI		/* ������б�ʾʹ��CPU��Ӳ��SPI�ӿ� */

/*
	安富莱STM32-V5 开发板口线分配
	PB3/SPI3_SCK/SPI1_SCK
	PB4/SPI3_MISO/SPI1_MISO
	PB5/SPI3_MOSI/SPI1_MOSI

	STM32硬件SPI接口 = SPI3 或者 SPI1
	由于SPI1的时钟源是84M, SPI3的时钟源是42M。为了获得更快的速度，软件上选择SPI1。
*/
#ifdef SOFT_SPI		/* 软件SPI */
	/* 定义GPIO端口 */
	#define RCC_SCK 	RCC_APB2Periph_GPIOA
	#define PORT_SCK	GPIO_SCLK_SPI
	#define PIN_SCK		PIN_SCLK_SPI

	#define RCC_MOSI 	RCC_APB2Periph_GPIOA
	#define PORT_MOSI	GPIO_MOSI_SPI
	#define PIN_MOSI	PIN_MOSI_SPI

	#define RCC_MISO 	RCC_APB2Periph_GPIOA
	#define PORT_MISO	GPIO_MISO_SPI
	#define PIN_MISO	PIN_MISO_SPI

	// #define SCK_0()		PORT_SCK->BSRR = PIN_SCK
	// #define SCK_1()		PORT_SCK->BRR = PIN_SCK
	#define SCK_0()		PORT_SCK->BRR = PIN_SCK
	#define SCK_1()		PORT_SCK->BSRR = PIN_SCK

#if 1
	#define MOSI_0()	PORT_MOSI->BSRR = PIN_MOSI
	#define MOSI_1()	PORT_MOSI->BRR = PIN_MOSI
#else
	#define MOSI_0()	PORT_MOSI->BRR = PIN_MOSI
	#define MOSI_1()	PORT_MOSI->BSRR = PIN_MOSI
#endif

	#define MISO_IS_HIGH()	(GPIO_ReadInputDataBit(PORT_MISO, PIN_MISO) == Bit_SET)
#endif

/*
	��SPIʱ�������2��Ƶ����֧�ֲ���Ƶ��
	�����SPI1��2��ƵʱSCKʱ�� = 42M��4��ƵʱSCKʱ�� = 21M
	�����SPI3, 2��ƵʱSCKʱ�� = 21M
*/
#define SPI_SPEED_42M		SPI_BaudRatePrescaler_2
#define SPI_SPEED_21M		SPI_BaudRatePrescaler_4
#define SPI_SPEED_5_2M		SPI_BaudRatePrescaler_8
#define SPI_SPEED_2_6M		SPI_BaudRatePrescaler_16
#define SPI_SPEED_1_3M		SPI_BaudRatePrescaler_32
#define SPI_SPEED_0_6M		SPI_BaudRatePrescaler_64

void bsp_InitSPIBus(void);

void bsp_spiWrite0(uint8_t _ucByte);
uint8_t bsp_spiRead0(void);

void bsp_spiWrite1(uint8_t _ucByte);
uint8_t bsp_spiRead1(void);

uint8_t bsp_SpiBusBusy(void);

void bsp_SPI_Init(uint16_t _cr1);

void bsp_SpiBusEnter(void);
void bsp_SpiBusExit(void);
uint8_t bsp_SpiBusBusy(void);
void bsp_SetSpiSck(uint8_t _data);

#endif

/***************************** ���������� www.armfly.com (END OF FILE) *********************************/
