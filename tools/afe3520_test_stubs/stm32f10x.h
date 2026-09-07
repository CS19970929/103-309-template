#ifndef AFE_HOST_STM32_H
#define AFE_HOST_STM32_H
#include <stdint.h>
typedef struct { uint16_t odr; } GPIO_TypeDef;
extern GPIO_TypeDef host_gpioa, host_gpiob;
#define GPIOA (&host_gpioa)
#define GPIOB (&host_gpiob)
#define GPIO_Pin_0 0x0001U
#define GPIO_Pin_1 0x0002U
#define GPIO_Pin_3 0x0008U
#define GPIO_Pin_4 0x0010U
#define GPIO_Pin_5 0x0020U
#define GPIO_Pin_6 0x0040U
#define GPIO_Pin_7 0x0080U
#define GPIO_Pin_8 0x0100U
#define GPIO_Pin_9 0x0200U
#define GPIO_Pin_12 0x1000U
#define GPIO_Pin_14 0x4000U
#define GPIO_Pin_15 0x8000U
typedef enum { Bit_RESET, Bit_SET } BitAction;
typedef struct { uint16_t GPIO_Pin; int GPIO_Speed, GPIO_Mode; } GPIO_InitTypeDef;
#define GPIO_Mode_Out_PP 1
#define GPIO_Mode_IN_FLOATING 2
#define GPIO_Speed_2MHz 2
#define RCC_APB2Periph_GPIOA 1
#define ENABLE 1
#define __NOP() ((void)0)
void GPIO_SetBits(GPIO_TypeDef *, uint16_t);
void GPIO_ResetBits(GPIO_TypeDef *, uint16_t);
void GPIO_WriteBit(GPIO_TypeDef *, uint16_t, BitAction);
uint8_t GPIO_ReadInputDataBit(GPIO_TypeDef *, uint16_t);
uint8_t GPIO_ReadOutputDataBit(GPIO_TypeDef *, uint16_t);
void GPIO_Init(GPIO_TypeDef *, GPIO_InitTypeDef *);
void RCC_APB2PeriphClockCmd(int, int);
#endif
