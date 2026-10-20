"""Real UART byte parser/IRQ/CRC wake guard; hardware is stubbed, protocol is not."""
from test_current_calibration import function_source
from test_sleep import read
from test_low_power_policy import run_programs

STUBS = r'''
#include <stdint.h>
#include <stdio.h>
#include <string.h>
typedef uint8_t UINT8;
typedef uint16_t UINT16;
typedef uint32_t UINT32;
#define _COMMOM_UPPER_SCI1
#define RS485_MAX_BUFFER_SIZE 251
#define RS485_SLAVE_ADDR 1
#define RS485_BROADCAST_ADDR 0
#define RS485_CMD_READ_REGS 3
#define RS485_CMD_WRITE_REG 6
#define RS485_CMD_WRITE_REGS 16
#define RS485_STA_IDLE 0
#define RS485_STA_RX_COMPLETE 1
#define USART_CR1_RE 4
#define USART_CR1_RXNEIE 32
#define USART_CR3_EIE 1
#define USART_SR_RXNE 32
#define USART_SR_ORE 8
#define USART_SR_NE 4
#define USART_SR_FE 2
#define USART_SR_PE 1
#define USART_IT_RXNE 32
#define USART1_IRQn 0
#define ENABLE 1
#define DISABLE 0
#define PIN_SCI1_TX 64
#define GPIO_Mode_AF_PP 0
#define GPIO_Speed_10MHz 0
#define GPIO_SCI1_TX 0
typedef struct { unsigned GPIO_Pin,GPIO_Mode,GPIO_Speed; } GPIO_InitTypeDef;
struct RS485MSG { uint8_t ptr_no,csr,enRs485CmdType,u16Buffer[251]; };
static struct RS485MSG g_stCurrentMsgPtr_SCI1;
static struct { uint32_t CR1,CR3,SR,DR; } usart;
#define USART1 (&usart)
static uint8_t s_uart1_preserve_frame,s_common_usarts_stopped;
static uint16_t gu16_CommuErrCnt_SCI1;
static uint32_t mask,now,s_sci1_last_rx_ms;
static uint32_t __get_PRIMASK(void) { return mask; }
static void __disable_irq(void) { mask=1; }
static void __set_PRIMASK(uint32_t value) { mask=value; }
static void NVIC_DisableIRQ(unsigned x) {(void)x;}
static void NVIC_EnableIRQ(unsigned x) {(void)x;}
static void NVIC_ClearPendingIRQ(unsigned x) {(void)x;}
static void GPIO_Init(unsigned x,GPIO_InitTypeDef *p) {(void)x;(void)p;}
static void USART_Cmd(void *x,int y) {(void)x;(void)y;}
static void USART_ITConfig(void *x,int y,int z) {(void)x;(void)y;(void)z;}
static void Sci_DataInit(struct RS485MSG *p) { memset(p,0,sizeof(*p)); }
static uint32_t System_GetUptimeMs(void) { return now; }
static void Sci_ExpirePartialRx(struct RS485MSG *p,volatile uint32_t *last) {
 if(p->csr==0 && (uint32_t)(now-*last)>=100U) p->ptr_no=0;
}
'''

TESTS = r'''
static void feed(const uint8_t *bytes,unsigned count) {
 for(unsigned i=0;i<count;i++) {
  usart.SR=USART_SR_RXNE;usart.DR=bytes[i];
  if(!Sci1_CommonUpper_IRQHandler(&g_stCurrentMsgPtr_SCI1)) puts("unexpected IRQ rejection");
 }
}
static void crc(uint8_t *p,unsigned n) {
 uint16_t value=Sci_CRC16RTU(p,(uint8_t)(n-2));p[n-2]=(uint8_t)value;p[n-1]=(uint8_t)(value>>8);
}
int main(void) {
 uint8_t read_request[8]={1,3,0,0,0,1,0,0};
 uint8_t write_request[11]={1,16,0x23,0,0,1,2,0x12,0x34,0,0};
 crc(read_request,8);crc(write_request,11);
 Sci_LowPowerUart1Begin();
 for(unsigned i=0;i<100;i++) if(Sci_LowPowerUart1FrameReady()) return 1;
 usart.SR=USART_SR_RXNE|USART_SR_FE;usart.DR=0;
 if(Sci1_CommonUpper_IRQHandler(&g_stCurrentMsgPtr_SCI1)) return 2;
 if(Sci_LowPowerUart1FrameReady() || !gu16_CommuErrCnt_SCI1) return 3;
 feed(read_request,8);
 if(!Sci_LowPowerUart1FrameReady()) return 4;
 Sci_LowPowerUart1End(1);
 if(!Sci_LowPowerUart1Preserve() || gu16_CommuErrCnt_SCI1) return 5;
 if(memcmp(g_stCurrentMsgPtr_SCI1.u16Buffer,read_request,8)) return 6;
 Sci_LowPowerResume();if(Sci_LowPowerUart1Preserve()) return 7;
 Sci_LowPowerUart1Begin(); read_request[7]^=1;feed(read_request,8);
 if(Sci_LowPowerUart1FrameReady()) return 8;
 feed(write_request,11);if(!Sci_LowPowerUart1FrameReady()) return 9;
 Sci_LowPowerUart1End(0);if(g_stCurrentMsgPtr_SCI1.ptr_no || g_stCurrentMsgPtr_SCI1.csr) return 10;
 for(unsigned len=0;len<256;len++){
  memset(&g_stCurrentMsgPtr_SCI1,0,sizeof(g_stCurrentMsgPtr_SCI1));
  g_stCurrentMsgPtr_SCI1.csr=RS485_STA_RX_COMPLETE;g_stCurrentMsgPtr_SCI1.ptr_no=(uint8_t)len;
  if(Sci_LowPowerUart1FrameReady()) return 11;
 }
 if(Sci_LowPowerUart1TimeoutMs()>3000U) return 12;
 puts("PASS: idle/noise/errors/bad CRC rejected; valid requests preserved; lengths 0..255 bounded");return 0;
}
'''

if __name__ == "__main__":
    sci = read("Sci_Upper.c")
    functions = ["Sci1_CommonUpper_RxByte", "Sci1_CommonUpper_IRQHandler", "Sci_LowPowerUart1Begin",
                 "Sci_LowPowerUart1FrameReady", "Sci_LowPowerUart1End", "Sci_LowPowerUart1Preserve",
                 "Sci_LowPowerUart1TimeoutMs", "Sci_LowPowerResume"]
    run_programs({"uart": STUBS + function_source(read("PubFunc.c"), "Sci_CRC16RTU")
                  + "\n".join(function_source(sci, name) for name in functions) + TESTS})
