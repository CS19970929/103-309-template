#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

typedef uint8_t UINT8;
typedef uint16_t UINT16;
typedef int16_t INT16;
typedef uint32_t UINT32;
typedef struct { uint32_t CRL, CRH, IDR, ODR; } GPIO_TypeDef;
typedef struct { int unused; } USART_TypeDef;
static GPIO_TypeDef ports[5];
static USART_TypeDef usarts[2];
#define GPIOA (&ports[0])
#define GPIOB (&ports[1])
#define GPIOC (&ports[2])
#define GPIOD (&ports[3])
#define GPIOE (&ports[4])
#define USART1 (&usarts[0])
#define USART2 (&usarts[1])
typedef enum { DISABLE, ENABLE } FunctionalState;
typedef enum { Bit_RESET, Bit_SET } BitAction;
typedef enum { GPIO_Mode_AIN, GPIO_Mode_IN_FLOATING, GPIO_Mode_Out_PP,
               GPIO_Mode_AF_PP, GPIO_Mode_IPU } GPIOMode_TypeDef;
typedef enum { GPIO_Speed_2MHz, GPIO_Speed_10MHz } GPIOSpeed_TypeDef;
typedef struct { uint16_t GPIO_Pin; GPIOMode_TypeDef GPIO_Mode; GPIOSpeed_TypeDef GPIO_Speed; } GPIO_InitTypeDef;
typedef enum { EXTI_Trigger_Falling, EXTI_Trigger_Rising, EXTI_Trigger_Rising_Falling } EXTITrigger_TypeDef;
typedef struct { uint32_t EXTI_Line; int EXTI_Mode; EXTITrigger_TypeDef EXTI_Trigger; FunctionalState EXTI_LineCmd; } EXTI_InitTypeDef;
typedef struct { uint8_t NVIC_IRQChannel, NVIC_IRQChannelPreemptionPriority,
    NVIC_IRQChannelSubPriority; FunctionalState NVIC_IRQChannelCmd; } NVIC_InitTypeDef;
typedef struct { uint16_t CAN_Prescaler; uint8_t CAN_Mode, CAN_SJW, CAN_BS1, CAN_BS2;
    FunctionalState CAN_TTCM, CAN_ABOM, CAN_AWUM, CAN_NART, CAN_RFLM, CAN_TXFP; } CAN_InitTypeDef;
#define GPIO_Pin_All 0xFFFFU
#define GPIO_PortSourceGPIOA 0
#define GPIO_PortSourceGPIOB 1
#define GPIO_PortSourceGPIOC 2
#define GPIO_PortSourceGPIOD 3
#define GPIO_PortSourceGPIOE 4
#define RCC_APB2Periph_GPIOA 1U
#define RCC_APB2Periph_GPIOB 2U
#define RCC_APB2Periph_GPIOC 4U
#define RCC_APB2Periph_GPIOD 8U
#define RCC_APB2Periph_GPIOE 16U
#define RCC_APB2Periph_AFIO 32U
#define RCC_APB2Periph_USART1 64U
#define RCC_APB1Periph_USART2 128U
#define GPIO_Remap_USART1 1U
#define CAN_Mode_Normal 0
#define CAN_SJW_1tq 0
#define CAN_BS1_5tq 4
#define CAN_BS2_2tq 1
#define ADC_Channel_1 1
#define ADC_SampleTime_239Cycles5 7
#define IWDG_Prescaler_64 4
#define IWDG_Prescaler_256 6
#define EXTI_Mode_Interrupt 0
#define EXTI_Line13 (1U << 13)
#define EXTI_Line17 (1U << 17)
enum { EXTI0_IRQn, EXTI3_IRQn, EXTI9_5_IRQn, EXTI15_10_IRQn,
       USART1_IRQn, USART2_IRQn, USB_LP_CAN1_RX0_IRQn, TIM3_IRQn };
enum { NORMAL_MODE, HICCUP_MODE, DEEP_MODE };
#if TEST_UART1_WAKE
#define UART1_WAKEUP_ENABLE
#endif
#if TEST_UART1_ROLE
#define _COMMOM_UPPER_SCI1
#endif
#if TEST_UART2_WAKE
#define RS485_WAKEUP_ENABLE
#endif
#if TEST_UART2_ROLE
#define _COMMOM_UPPER_SCI2
#endif
#if TEST_RTC
#define __FUNC_RTC__
#endif

/* BOARD_DECLARATIONS */
/* BOARD_TABLES */

typedef struct { uint8_t time_enter_rtc; bool power_on; } Time_T;
static struct { int mode; } g_stLowPowerRtcStatus;
static struct { int kind, port; uint16_t pins; int value; } events[256];
static unsigned event_count, stop_phase;
static int modes[5][16], speeds[5][16];
static unsigned irq_count;
static uint32_t exti_enabled, cleared_pending;
static int exti_port[16], exti_trigger[16];
static uint32_t output_bits[5][16], input_bits[5][16];

static int port_index(GPIO_TypeDef *gpio) { assert(gpio >= ports && gpio < ports+5); return (int)(gpio-ports); }
static void record(int kind, int port, uint16_t pins, int value)
{
    assert(event_count < sizeof(events)/sizeof(events[0]));
    events[event_count].kind=kind; events[event_count].port=port;
    events[event_count].pins=pins; events[event_count++].value=value;
}
static void RCC_APB2PeriphClockCmd(uint32_t clocks, FunctionalState state) { assert(clocks && state==ENABLE); }
static void GPIO_WriteBit(GPIO_TypeDef *gpio, uint16_t pin, BitAction level)
{
    record(1,port_index(gpio),pin,level);
    if (level==Bit_SET) gpio->ODR |= pin; else gpio->ODR &= ~pin;
}
static void GPIO_SetBits(GPIO_TypeDef *gpio, uint16_t pins) { GPIO_WriteBit(gpio,pins,Bit_SET); }
static void GPIO_ResetBits(GPIO_TypeDef *gpio, uint16_t pins) { GPIO_WriteBit(gpio,pins,Bit_RESET); }
static void GPIO_Init(GPIO_TypeDef *gpio, GPIO_InitTypeDef *config)
{
    unsigned bit;
    int port=port_index(gpio);
    assert(config->GPIO_Pin);
    record(2,port,config->GPIO_Pin,config->GPIO_Mode);
    for (bit=0; bit<16; ++bit) if (config->GPIO_Pin & (1U<<bit)) {
        modes[port][bit]=config->GPIO_Mode;
        speeds[port][bit]=config->GPIO_Speed;
        if (config->GPIO_Mode==GPIO_Mode_IPU) gpio->ODR |= 1U<<bit;
    }
}
static void GPIO_EXTILineConfig(uint8_t port, uint8_t pin) { assert(port<5 && pin<16); exti_port[pin]=port; }
static void EXTI_Init(EXTI_InitTypeDef *config)
{
    unsigned bit;
    if (config->EXTI_LineCmd==ENABLE) {
        exti_enabled |= config->EXTI_Line;
        for (bit=0; bit<16; ++bit) if (config->EXTI_Line & (1U<<bit)) exti_trigger[bit]=config->EXTI_Trigger;
    } else exti_enabled &= ~config->EXTI_Line;
}
static void NVIC_Init(NVIC_InitTypeDef *config) { assert(config->NVIC_IRQChannelCmd==ENABLE); ++irq_count; }
static void EXTI_ClearITPendingBit(uint32_t lines) { cleared_pending |= lines; }
static void NVIC_ClearPendingIRQ(int irq) { (void)irq; }
static void jtag_disableAndConfIO(void) {}
static void Sci_StopForLowPower(void) { assert(stop_phase++==0); }
static void Fault_PauseMeasurements(void) { assert(stop_phase++==1); }
static void ADC_StopForLowPower(void) { assert(stop_phase++==2); }
void LowPower_DisableUartWakeup(void);

/* BOARD_INITIALIZATION */

/* Host bit-band substitute keeps lvalue semantics for the real pin macros. */
static uint32_t *bit_address(uint32_t address, unsigned bit)
{
    unsigned port;
    assert(bit<16);
    for (port=0; port<5; ++port) {
        if (address==(uint32_t)(uintptr_t)&ports[port].ODR) return &output_bits[port][bit];
        if (address==(uint32_t)(uintptr_t)&ports[port].IDR) return &input_bits[port][bit];
    }
    assert(0); return 0;
}
#define BIT_ADDR(address,bit) (*bit_address((uint32_t)(address),(bit)))
/* I2C_PIN_OPERATIONS */

static void reset_case(void)
{
    unsigned port,bit;
    memset(ports,0,sizeof(ports));
    memset(exti_port,-1,sizeof(exti_port));
    memset(exti_trigger,-1,sizeof(exti_trigger));
    for (port=0; port<5; ++port) for (bit=0; bit<16; ++bit) {
        modes[port][bit]=GPIO_Mode_AF_PP; speeds[port][bit]=GPIO_Speed_10MHz;
    }
    event_count=stop_phase=irq_count=0;
    exti_enabled=cleared_pending=0;
}

static void test_board_contract(void)
{
    unsigned i,j;
    const unsigned expected_port[]={1,1,0,0,0,0,1,1,0,1,1,1,1,0,0,0,0,1,0,1,1};
    const unsigned expected_pin[] ={0,14,0,9,7,15,3,4,4,5,15,6,7,2,3,11,12,12,1,8,9};
    assert(BOARD_IO_COUNT==21);
    for (i=0; i<BOARD_IO_COUNT; ++i) {
        assert(g_board_io[i].gpio==&ports[expected_port[i]]);
        assert(g_board_io[i].pin_source==expected_pin[i]);
        for (j=0; j<i; ++j) assert(g_board_io[j].gpio!=g_board_io[i].gpio ||
                                  g_board_io[j].pin_source!=g_board_io[i].pin_source);
    }
    assert(g_board_adc_inputs[ADC_INPUT_MOS].channel==1);
    assert(g_board_adc_inputs[ADC_INPUT_MOS].sample_time==ADC_SampleTime_239Cycles5);
    assert(g_board_uart1.usart==USART1 && g_board_uart1.apb2 && g_board_uart1.remap==GPIO_Remap_USART1);
    assert(g_board_uart2.usart==USART2 && !g_board_uart2.apb2 && !g_board_uart2.remap);
    assert(g_board_uart1.irq.channel==USART1_IRQn && g_board_uart2.irq.channel==USART2_IRQn);
    assert(g_board_can.init.CAN_Prescaler==2 && g_board_can.init.CAN_BS1==CAN_BS1_5tq &&
           g_board_can.init.CAN_BS2==CAN_BS2_2tq && g_board_can.init.CAN_SJW==CAN_SJW_1tq);
    assert(g_board_can.init.CAN_ABOM==ENABLE && g_board_can.init.CAN_NART==ENABLE &&
           g_board_can.init.CAN_AWUM==DISABLE && g_board_can.power_on_level==Bit_RESET &&
           g_board_can.power_off_level==Bit_SET);
    assert(g_board_timer3.counter_hz/(g_board_timer3.period+1U)==100U);
    assert(g_board_timer3.irq.preemption_priority==0 && g_board_timer3.irq.sub_priority==3);
#if TEST_RTC
    assert(g_board_iwdg.prescaler==IWDG_Prescaler_256 && g_board_iwdg.reload==4095);
#else
    assert(g_board_iwdg.prescaler==IWDG_Prescaler_64 && g_board_iwdg.reload==800);
#endif
}

static void test_boot_and_resume(void)
{
    unsigned i,first_power_mode=256,preloads=0;
    reset_case();
    GPIOB->ODR=0xFFFF; GPIOA->ODR=0;
    InitIO();
    assert(modes[1][0]==GPIO_Mode_Out_PP && (GPIOB->ODR&1U));
    assert(modes[1][14]==GPIO_Mode_Out_PP && !(GPIOB->ODR&(1U<<14)));
    assert((GPIOA->ODR&(1U<<15)) && !(GPIOA->ODR&(1U<<4)));
    assert(!(GPIOB->ODR&((1U<<3)|(1U<<4))) && (GPIOB->ODR&(1U<<5)));
    for (i=0; i<event_count; ++i) {
        unsigned id;
        for (id=0; id<BOARD_IO_COUNT; ++id) if (g_board_io[id].init_group==BOARD_INIT_POWER &&
            events[i].port==port_index(g_board_io[id].gpio) && (events[i].pins&BOARD_IO_PIN(id))) {
            if (events[i].kind==2 && first_power_mode==256) first_power_mode=i;
            if (events[i].kind==1) { assert(first_power_mode==256); ++preloads; }
        }
    }
    assert(preloads==5 && first_power_mode<event_count);
    /* STOP wake must never reapply boot CTL=0, VPRO or LED mode/level. */
    GPIOB->ODR=(1U<<14); modes[1][0]=GPIO_Mode_AF_PP; modes[1][15]=GPIO_Mode_IN_FLOATING;
    event_count=0;
    InitIO_rtc();
    assert((GPIOB->ODR&(1U<<14)) && !(GPIOB->ODR&1U));
    assert(modes[1][0]==GPIO_Mode_AF_PP && modes[1][15]==GPIO_Mode_IN_FLOATING);
    for (i=0; i<event_count; ++i) assert(events[i].port!=1 ||
        !(events[i].pins&((1U<<0)|(1U<<14)|(1U<<15))));
}

static void test_stop_retention(void)
{
    unsigned port,bit,mode;
    for (mode=0; mode<3; ++mode) {
        reset_case();
        GPIOB->ODR=0xA55A;
        g_stLowPowerRtcStatus.mode=mode;
        IOstatus_RTCMode();
        assert(stop_phase==3 && GPIOB->ODR==0xA55A);
        for (port=0; port<5; ++port) for (bit=0; bit<16; ++bit) {
            int keep=port==1 && (bit==0 || bit==14 || bit==15 || (bit==4 && mode!=NORMAL_MODE));
            assert(modes[port][bit]==(keep ? GPIO_Mode_AF_PP : GPIO_Mode_AIN));
        }
    }
    reset_case(); IOstatus_DeepMode(); assert(stop_phase==3);
    for (port=0; port<5; ++port) for (bit=0; bit<16; ++bit) assert(modes[port][bit]==GPIO_Mode_AIN);
}

static void test_wakeup(void)
{
    uint32_t expected=(1U<<9)|(1U<<12);
    reset_case(); InitWakeUp_Base();
    assert(exti_enabled==((1U<<0)|(1U<<9)) && irq_count==2);
    assert(exti_port[0]==0 && exti_trigger[0]==EXTI_Trigger_Rising);
    assert(exti_port[9]==0 && exti_trigger[9]==EXTI_Trigger_Rising_Falling);
    reset_case(); InitWakeUp_RTCMode();
#if TEST_UART1_WAKE && TEST_UART1_ROLE
    expected |= 1U<<7;
    assert(exti_port[7]==1 && exti_trigger[7]==EXTI_Trigger_Falling);
#endif
#if TEST_UART2_WAKE && TEST_UART2_ROLE
    expected |= 1U<<3;
    assert(exti_port[3]==0 && exti_trigger[3]==EXTI_Trigger_Falling);
#endif
    assert(exti_enabled==expected);
    assert(exti_port[12]==1 && exti_trigger[12]==EXTI_Trigger_Rising);
    LowPower_DisableWakeupExti(); assert(exti_enabled==0);
    assert((cleared_pending&((1U<<0)|(1U<<3)|(1U<<9)|(1U<<12)|(1U<<13)))==
           ((1U<<0)|(1U<<3)|(1U<<9)|(1U<<12)|(1U<<13)));
}

static void test_peripheral_pins(void)
{
    reset_case();
    Board_InitIoGroup(BOARD_INIT_UART1); Board_InitIoGroup(BOARD_INIT_UART2);
    assert(modes[1][6]==GPIO_Mode_AF_PP && speeds[1][6]==GPIO_Speed_10MHz);
    assert(modes[1][7]==GPIO_Mode_IN_FLOATING);
    assert(modes[0][2]==GPIO_Mode_AF_PP && modes[0][3]==GPIO_Mode_IN_FLOATING);
    Board_InitIoGroup(BOARD_INIT_CAN);
    assert(modes[0][11]==GPIO_Mode_IPU && (GPIOA->ODR&(1U<<11)));
    assert(modes[0][12]==GPIO_Mode_AF_PP && speeds[0][12]==GPIO_Speed_2MHz);
    Board_InitIoGroup(BOARD_INIT_ADC); assert(modes[0][1]==GPIO_Mode_AIN);
    event_count=0;
    Board_InitIoGroup(BOARD_INIT_AFE_I2C);
    assert((GPIOB->ODR&0x300U)==0x300U);
    assert(events[event_count-1].kind==1 && events[event_count-1].pins==0x300U);
    /* CRH updates affect exactly PB8 or PB9; adjacent control pins survive. */
    GPIOB->CRH=0xA5A5A5A5;
    TWI_CLK_OUT; assert(GPIOB->CRH==0xA5A5A5A3);
    TWI_DAT_IN; assert(GPIOB->CRH==0xA5A5A583);
    TWI_CLK_IN; assert(GPIOB->CRH==0xA5A5A588);
    TWI_DAT_OUT; assert(GPIOB->CRH==0xA5A5A538);
    TWI_CLK_HIGH; TWI_DAT_LOW;
    assert(output_bits[1][8]==1 && output_bits[1][9]==0);
    input_bits[1][8]=1; input_bits[1][9]=0;
    assert(TWI_RD_CLK==1 && TWI_RD_DAT==0);
}

int main(void)
{
    test_board_contract(); test_boot_and_resume(); test_stop_retention();
    test_wakeup(); test_peripheral_pins();
    puts("PASS board: 21 pins, power preload, STOP retention/restore, wake gating, peripheral/I2C pins");
    return 0;
}
