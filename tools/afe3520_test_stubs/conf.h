#ifndef AFE_HOST_CONF_H
#define AFE_HOST_CONF_H
#include <stdint.h>
#include <stdbool.h>
#include "stm32f10x.h"
#include "conf/conf_gpio.h"
typedef uint8_t UINT8;
typedef uint16_t UINT16;
typedef uint32_t UINT32;
typedef int16_t INT16;
typedef int32_t INT32;
typedef enum { GPIO_CHG, GPIO_DSG, GPIO_PreCHG, GPIO_MAIN } GPIO_Type;
struct RS485MSG;
#define CS_Res 2
#define CS_Res_Num 2
#define LIFEPO
#define AFE_OCC1 120
#define AFE_OCC2 120
#define AFE_ODC1 150
#define AFE_ODC2 150
#define CBC_Cur_DSG 50
#define CBC_DelayT 128
#endif
