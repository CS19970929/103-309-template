/*
*********************************************************************************************************
*
*	模块名称 : I2C总线驱动模块
*	文件名称 : bsp_i2c_gpio.h
*	版    本 : V1.0
*	说    明 : 头文件。
*
*	Copyright (C), 2012-2013, 安富莱电子 www.armfly.com
*
*********************************************************************************************************
*/

#ifndef _BSP_I2C_GPIO1_H
#define _BSP_I2C_GPIO1_H

#define I2C_WR	0		/* 写控制bit */
#define I2C_RD	1		/* 读控制bit */

// void bsp_InitI2C(GPIO_TypeDef* GPIOx, uint16_t _pin_scl, uint16_t _pin_sda)
// void bsp_InitI2C(void);
void i2c_Start1(void);
void i2c_Stop1(void);
void i2c_SendByte1(uint8_t _ucByte);
uint8_t i2c_ReadByte1(void);
uint8_t i2c_WaitAck1(void);
void i2c_Ack1(void);
void i2c_NAck1(void);
// uint8_t i2c_CheckDevice(uint8_t _Address);

#endif
