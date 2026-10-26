/* Host-only peripherals and independent assertions. Not part of the firmware. */
typedef struct { UINT16 SR, DR, CR1, CR3; } USART_TypeDef;
typedef struct { int unused; } GPIO_TypeDef;
typedef struct { UINT32 GPIO_Pin,GPIO_Mode,GPIO_Speed; } GPIO_InitTypeDef;
typedef struct { UINT32 USART_BaudRate,USART_Mode; } USART_InitTypeDef;
typedef struct { UINT32 NVIC_IRQChannel,NVIC_IRQChannelPreemptionPriority,
    NVIC_IRQChannelSubPriority,NVIC_IRQChannelCmd; } NVIC_InitTypeDef;
typedef int IRQn_Type;
#include <stdbool.h>
static USART_TypeDef uart1,uart2,uart3;
static GPIO_TypeDef gpio_a,gpio_b,gpio_d;
#define USART1 (&uart1)
#define USART2 (&uart2)
#define USART3 (&uart3)
#define GPIOA (&gpio_a)
#define GPIOB (&gpio_b)
#define GPIOD (&gpio_d)
#define USART1_IRQn 1
#define USART2_IRQn 2
#define USART3_IRQn 3
#define USART_SR_ORE 1
#define USART_SR_NE 2
#define USART_SR_FE 4
#define USART_SR_PE 8
#define USART_SR_RXNE 16
#define USART_SR_TXE 32
#define USART_SR_TC 64
#define USART_SR_IDLE 128
#define USART_CR1_RE 1
#define USART_CR1_TE 2
#define USART_CR1_RXNEIE 4
#define USART_CR1_IDLEIE 8
#define USART_CR1_TXEIE 16
#define USART_CR1_TCIE 32
#define USART_CR3_EIE 1
#define USART_FLAG_TC USART_SR_TC
#define RCC_APB2Periph_USART1 1
#define RCC_APB1Periph_USART2 2
#define RCC_APB1Periph_USART3 3
#define RCC_APB2Periph_AFIO 4
#define RCC_APB2Periph_GPIOA 8
#define RCC_APB2Periph_GPIOB 16
#define RCC_APB2Periph_GPIOD 32
#define GPIO_Pin_2 2
#define GPIO_Pin_3 3
#define GPIO_Pin_6 6
#define GPIO_Pin_7 7
#define GPIO_Pin_8 8
#define GPIO_Pin_9 9
#define GPIO_Remap_USART1 1
#define GPIO_FullRemap_USART3 2
#define GPIO_Mode_AF_PP 1
#define GPIO_Mode_IN_FLOATING 2
#define GPIO_Speed_10MHz 10
#define USART_Mode_Rx 1
#define USART_Mode_Tx 2
#define ENABLE 1
#define DISABLE 0
#define _COMMOM_UPPER_SCI1
#define _COMMOM_UPPER_SCI2
#define _COMMOM_UPPER_SCI3
#define PROJECT_CFG_HOST_WRITE_ENABLE 1
#define PROJECT_CFG_IRQ_DEBUG_ENABLE 1
#include "IrqDebug.h"
static struct { volatile IRQ_DEBUG irq; UINT32 rtc_sec_cnt; } sys_time;
static UINT32 clock_us;
static UINT32 clock_step;
static UINT8 primask, u8FlashUpdateFlag, host_off;
static int comm_activity, host_apply_ok=1;
static UINT32 SysTime_GetMicroseconds(void) { clock_us+=clock_step; return clock_us; }
static UINT32 __get_PRIMASK(void) { return primask; }
static void __disable_irq(void) { primask=1; }
static void __enable_irq(void) { primask=0; }
static void RCC_APB2PeriphClockCmd(UINT32 p,int on) {}
static void RCC_APB1PeriphClockCmd(UINT32 p,int on) {}
static void USART_Cmd(USART_TypeDef *p,int on) {}
static void USART_StructInit(USART_InitTypeDef *p) { memset(p,0,sizeof(*p)); }
static void USART_Init(USART_TypeDef *p,USART_InitTypeDef *cfg) { assert(cfg->USART_BaudRate); }
static void USART_ClearFlag(USART_TypeDef *p,UINT16 flag) { p->SR &= ~flag; }
static void GPIO_StructInit(GPIO_InitTypeDef *p) { memset(p,0,sizeof(*p)); }
static void GPIO_Init(GPIO_TypeDef *p,GPIO_InitTypeDef *cfg) {}
static void GPIO_PinRemapConfig(UINT32 p,int on) {}
static void NVIC_Init(NVIC_InitTypeDef *p) {}
static UINT8 Fault_SetHostMosOff(UINT8 mask) { host_off=mask; return (UINT8)host_apply_ok; }

#define TEMP_NUM 10
/* The actual report/fault declarations are inserted here by the runner. */
