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
#define DISABLE 0
#define GPIO_Mode_AF_PP 3
#define GPIO_Remap_SPI1 1
#define RCC_APB2Periph_AFIO 2
#define RCC_APB2Periph_SPI1 4
typedef enum { RESET, SET } FlagStatus;
typedef struct { unsigned enabled; } SPI_TypeDef;
extern SPI_TypeDef host_spi;
#define SPI1 (&host_spi)
typedef struct {
    unsigned SPI_Direction, SPI_Mode, SPI_DataSize, SPI_CPOL, SPI_CPHA;
    unsigned SPI_NSS, SPI_BaudRatePrescaler, SPI_FirstBit;
} SPI_InitTypeDef;
#define SPI_Direction_2Lines_FullDuplex 1
#define SPI_Mode_Master 2
#define SPI_DataSize_8b 3
#define SPI_CPOL_High 4
#define SPI_CPHA_2Edge 5
#define SPI_NSS_Soft 6
#define SPI_BaudRatePrescaler_2 2
#define SPI_BaudRatePrescaler_4 4
#define SPI_BaudRatePrescaler_8 8
#define SPI_BaudRatePrescaler_16 16
#define SPI_BaudRatePrescaler_32 32
#define SPI_BaudRatePrescaler_64 64
#define SPI_BaudRatePrescaler_128 128
#define SPI_BaudRatePrescaler_256 256
#define SPI_FirstBit_MSB 7
#define SPI_NSSInternalSoft_Set 8
#define SPI_I2S_FLAG_TXE 1
#define SPI_I2S_FLAG_RXNE 2
#define SPI_I2S_FLAG_BSY 3
#define SPI_I2S_FLAG_OVR 4
#define SPI_FLAG_MODF 5
void GPIO_PinRemapConfig(unsigned, int);
void SPI_I2S_DeInit(SPI_TypeDef *);
void SPI_StructInit(SPI_InitTypeDef *);
void SPI_Init(SPI_TypeDef *, SPI_InitTypeDef *);
void SPI_Cmd(SPI_TypeDef *, int);
void SPI_NSSInternalSoftwareConfig(SPI_TypeDef *, unsigned);
FlagStatus SPI_I2S_GetFlagStatus(SPI_TypeDef *, uint16_t);
void SPI_I2S_SendData(SPI_TypeDef *, uint16_t);
uint16_t SPI_I2S_ReceiveData(SPI_TypeDef *);
typedef struct { uint32_t SYSCLK_Frequency, HCLK_Frequency, PCLK1_Frequency, PCLK2_Frequency; } RCC_ClocksTypeDef;
void RCC_GetClocksFreq(RCC_ClocksTypeDef *);
#define RCC_APB1Periph_TIM4 8
#define TIM4 4
#define TIM_PSCReloadMode_Immediate 1
void RCC_APB1PeriphClockCmd(int,int);
void TIM_DeInit(int);
void TIM_SetAutoreload(int,uint16_t);
void TIM_PrescalerConfig(int,uint16_t,int);
void TIM_SetCounter(int,uint16_t);
uint16_t TIM_GetCounter(int);
void TIM_Cmd(int,int);
#endif
