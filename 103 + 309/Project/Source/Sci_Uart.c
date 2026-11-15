#include "main.h"
#include "Sci_Protocol.h"

#define SCI_USART_ERRORS (USART_SR_ORE | USART_SR_NE | USART_SR_FE | USART_SR_PE)
#define SCI_CUSTOMER_BYTE_TIMEOUT_US 20000U
#define SCI_TX_MARGIN_US 20000U
#define SCI_CLOCK_STEP_US 10U

enum SCI_PROTOCOL { SCI_NONE, SCI_MODBUS, SCI_CUSTOMER, SCI_DISCARD };
struct SCI_PORT {
    USART_TypeDef *usart;
    UINT32 baud;
    struct RS485MSG message;
    volatile UINT32 last_rx_us;
    UINT32 tx_started_us;
    volatile UINT16 tx_index, tx_length;
    volatile UINT8 protocol, frame_pending;
    /* Diagnostic-only counters: never affect safety or wire protocol. */
    volatile UINT16 rx_errors, bad_frames, tx_timeouts, last_sr_error;
};
static struct SCI_PORT s_port1 = {USART1, UART1_BAUD};
static UINT8 s_baud_change_pending;
#ifdef _COMMOM_UPPER_SCI2
static struct SCI_PORT s_port2 = {USART2, UART2_BAUD};
#endif

static UINT32 Sci_CharacterUs(const struct SCI_PORT *port)
{
    return (10000000U+port->baud-1U)/port->baud; /* Existing 8N1: ten bits. */
}

static UINT32 Sci_FrameGapUs(const struct SCI_PORT *port)
{
    return port->baud > 19200U ? 1750U : (35000000U+port->baud-1U)/port->baud;
}

static UINT32 Sci_ByteGapUs(const struct SCI_PORT *port)
{
    return port->baud > 19200U ? 750U : (15000000U+port->baud-1U)/port->baud;
}

static void Sci_ArmReceiver(struct SCI_PORT *port)
{
    Sci_ResetMessage(&port->message);
    port->frame_pending=0U;
    port->protocol=SCI_NONE;
    port->tx_index=0U;
    port->tx_length=0U;
    port->usart->CR1 &= (UINT16)~(USART_CR1_TXEIE | USART_CR1_TCIE | USART_CR1_IDLEIE);
    port->usart->CR1 |= USART_CR1_RE | USART_CR1_RXNEIE;
}

/* Called only with RXNEIE enabled. ISR performs framing, never CRC/business. */
static void Sci_ReceiveByte(struct SCI_PORT *port, UINT8 data, UINT32 now)
{
    UINT32 elapsed=now-port->last_rx_us;
    UINT32 char_us=Sci_CharacterUs(port);
    if (port->protocol != SCI_NONE)
    {
        if (elapsed >= (port->protocol == SCI_CUSTOMER ?
            SCI_CUSTOMER_BYTE_TIMEOUT_US : Sci_FrameGapUs(port))+char_us+SCI_CLOCK_STEP_US)
        {
            Sci_ResetMessage(&port->message);
            port->frame_pending=0U;
            port->protocol=SCI_NONE;
        }
        else if (port->frame_pending || (port->protocol == SCI_MODBUS &&
            elapsed > Sci_ByteGapUs(port)+char_us+SCI_CLOCK_STEP_US))
        {
            /* Additional bytes invalidate a completed RTU frame until silence. */
            Sci_ResetMessage(&port->message);
            port->frame_pending=0U;
            port->protocol=SCI_DISCARD;
        }
    }
    port->last_rx_us=now;
    if (port->protocol == SCI_NONE)
    {
        if (data == RS485_SLAVE_ADDR || data == RS485_BROADCAST_ADDR)
            port->protocol=SCI_MODBUS;
        else if (data == 0x5AU || data == 0xDDU)
            port->protocol=SCI_CUSTOMER;
        else
            port->protocol=SCI_DISCARD;
    }
    if (port->protocol == SCI_MODBUS)
        port->frame_pending=Sci_ModbusFeed(&port->message,data);
    else if (port->protocol == SCI_CUSTOMER)
        port->frame_pending=Sci_CustomerFeed(&port->message,data);
    if (port->protocol != SCI_DISCARD && port->message.ptr_no == 0U)
        port->protocol=SCI_DISCARD;
}

static void Sci_PortIRQ(struct SCI_PORT *port)
{
    UINT16 status=(UINT16)port->usart->SR;
    if (status & SCI_USART_ERRORS)
    {
        port->last_sr_error=(UINT16)(status & SCI_USART_ERRORS);
        if (port->rx_errors != 0xFFFFU) ++port->rx_errors;
        volatile UINT16 dummy=(UINT16)port->usart->DR; /* SR then DR clears errors. */
        (void)dummy;
        Sci_ArmReceiver(port);
        port->protocol=SCI_DISCARD;
        port->last_rx_us=SysTime_GetMicroseconds();
        return;
    }
    if ((status & USART_SR_RXNE) && (port->usart->CR1 & USART_CR1_RXNEIE))
    {
        UINT8 data=(UINT8)port->usart->DR;
        Sci_ReceiveByte(port,data,SysTime_GetMicroseconds());
    }
    if ((port->usart->SR & USART_SR_TXE) && (port->usart->CR1 & USART_CR1_TXEIE))
    {
        if (port->tx_index < port->tx_length)
            port->usart->DR=port->message.u16Buffer[port->tx_index++];
        if (port->tx_index >= port->tx_length)
        {
            port->usart->CR1 &= (UINT16)~USART_CR1_TXEIE;
            port->usart->CR1 |= USART_CR1_TCIE;
        }
    }
    if ((port->usart->SR & USART_SR_TC) && (port->usart->CR1 & USART_CR1_TCIE))
    {
        USART_ClearFlag(port->usart,USART_FLAG_TC);
        if (port->message.reset_after_tx)
            u8FlashUpdateFlag=1U;
        Sci_ArmReceiver(port);
        port->last_rx_us=SysTime_GetMicroseconds();
    }
}

/* A complete valid frame is processed only in the RUN main loop. */
static UINT8 Sci_ServicePort(struct SCI_PORT *port)
{
    UINT32 now, elapsed;
    UINT32 primask=__get_PRIMASK();
    UINT8 ready=0U, valid;
    __disable_irq();
    now=SysTime_GetMicroseconds();
    elapsed=now-port->last_rx_us;
    if (port->tx_length != 0U)
    {
        if ((UINT32)(now-port->tx_started_us) >
            (UINT32)port->tx_length*Sci_CharacterUs(port)+SCI_TX_MARGIN_US)
        {
            if (port->tx_timeouts != 0xFFFFU) ++port->tx_timeouts;
            Sci_ArmReceiver(port); /* Never reset or upgrade after a TX timeout. */
        }
    }
    else if (!(port->usart->SR & USART_SR_RXNE))
    {
        if (port->frame_pending &&
            (port->protocol != SCI_MODBUS || elapsed >= Sci_FrameGapUs(port)+SCI_CLOCK_STEP_US))
        {
            port->usart->CR1 &= (UINT16)~(USART_CR1_RE | USART_CR1_RXNEIE);
            port->frame_pending=0U;
            ready=1U;
        }
        else if ((port->protocol == SCI_DISCARD && elapsed >= Sci_FrameGapUs(port)) ||
            (port->message.ptr_no != 0U && !port->frame_pending && elapsed >
                (port->protocol == SCI_MODBUS ? Sci_ByteGapUs(port)+SCI_CLOCK_STEP_US : SCI_CUSTOMER_BYTE_TIMEOUT_US)))
        {
            Sci_ArmReceiver(port);
            port->protocol=SCI_DISCARD;
            if (elapsed >= Sci_FrameGapUs(port)) port->protocol=SCI_NONE;
        }
    }
    if (primask == 0U) __enable_irq();
    if (!ready)
        return 0U;
    valid=(port->protocol == SCI_MODBUS) ? Sci_ModbusFrameValid(&port->message) :
        Sci_CustomerFrameValid(&port->message);
    if (!valid)
    {
        if (port->bad_frames != 0xFFFFU) ++port->bad_frames;
        Sci_ArmReceiver(port);
        port->protocol=SCI_DISCARD; /* Do not execute a nested prefix in bad data. */
        return 0U;
    }
    SleepDeal_RecordExternalComm();
    if (port->protocol == SCI_MODBUS)
        Sci_ModbusProcessFrame(&port->message);
    else
        Sci_CustomerProcessFrame(&port->message);
    if (port->message.AckLenth == 0U || port->message.AckLenth > RS485_MAX_BUFFER_SIZE)
        Sci_ArmReceiver(port);
    else
    {
        port->tx_started_us=SysTime_GetMicroseconds();
        port->tx_index=0U;
        port->tx_length=port->message.AckLenth;
        port->usart->CR1 &= (UINT16)~USART_CR1_TCIE;
        port->usart->CR1 |= USART_CR1_TE | USART_CR1_TXEIE;
    }
    return 1U;
}

static void Sci_InitPort(struct SCI_PORT *port, const BoardUartConfig *config)
{
    USART_InitTypeDef uart;
    volatile UINT16 dummy;
    port->usart=config->usart;
    if (config->apb2) RCC_APB2PeriphClockCmd(config->clock,ENABLE);
    else RCC_APB1PeriphClockCmd(config->clock,ENABLE);
    USART_Cmd(port->usart,DISABLE);
    port->usart->CR1=0U;
    dummy=(UINT16)port->usart->SR;
    dummy=(UINT16)port->usart->DR;
    (void)dummy;
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_AFIO | RCC_APB2Periph_GPIOA |
        RCC_APB2Periph_GPIOB | RCC_APB2Periph_GPIOD,ENABLE);
    if (config->remap) GPIO_PinRemapConfig(config->remap,ENABLE);
    Board_InitIoGroup(config->io_group);
    USART_StructInit(&uart);
    uart.USART_BaudRate=port->baud;
    uart.USART_Mode=USART_Mode_Rx | USART_Mode_Tx;
    USART_Init(port->usart,&uart);
    port->usart->CR3 |= USART_CR3_EIE;
    /* All protocol state is initialized before enabling USART/NVIC. */
    Sci_ArmReceiver(port);
    port->last_rx_us=SysTime_GetMicroseconds();
    Board_EnableIrq(&config->irq);
    USART_Cmd(port->usart,ENABLE);
}

void InitUSART_CommonUpper(void)
{
    UINT16 mode=StorageFlash_GetBaudMode();
    UINT32 baud=(mode==3U) ? 115200U : (mode==2U ? 19200U : 9600U);
    s_port1.baud=baud;
#ifdef _COMMOM_UPPER_SCI2
    s_port2.baud=baud;
#endif
#ifdef _COMMOM_UPPER_SCI1
    Sci_InitPort(&s_port1,&g_board_uart1);
#endif
#ifdef _COMMOM_UPPER_SCI2
    Sci_InitPort(&s_port2,&g_board_uart2);
#endif
}

void Sci1_CommonUpper_IRQHandler(void) { Sci_PortIRQ(&s_port1); }
void Sci2_CommonUpper_IRQHandler(void)
{
#ifdef _COMMOM_UPPER_SCI2
    Sci_PortIRQ(&s_port2);
#endif
}

static UINT8 Sci_PortBusy(const struct SCI_PORT *port)
{
    return (UINT8)(port->protocol == SCI_MODBUS || port->protocol == SCI_CUSTOMER ||
        port->tx_length || port->frame_pending);
}

static UINT8 Sci_PortsBusy(void)
{
    UINT8 busy=0U;
#ifdef _COMMOM_UPPER_SCI1
    busy |= Sci_PortBusy(&s_port1);
#endif
#ifdef _COMMOM_UPPER_SCI2
    busy |= Sci_PortBusy(&s_port2);
#endif
    return busy;
}

UINT8 Sci_IsAnyPortBusy(void)
{
    return (UINT8)(s_baud_change_pending || Sci_PortsBusy());
}


UINT8 Sci_RequestBaudMode(UINT16 mode)
{
    if (s_baud_change_pending || !StorageFlash_SetBaudMode(mode)) return 0U;
    s_baud_change_pending=1U;
    return 1U;
}


static void Sci_StopPort(struct SCI_PORT *port)
{
    volatile UINT16 dummy;
    port->usart->CR1 &= (UINT16)~(USART_CR1_RE | USART_CR1_RXNEIE |
        USART_CR1_IDLEIE | USART_CR1_TXEIE | USART_CR1_TCIE);
    USART_Cmd(port->usart,DISABLE);
    dummy=(UINT16)port->usart->SR;
    dummy=(UINT16)port->usart->DR;
    (void)dummy;
    Sci_ResetMessage(&port->message);
    port->protocol=SCI_NONE;
    port->frame_pending=0U;
    port->tx_length=0U;
}

void Sci_StopForLowPower(void)
{
#ifdef _COMMOM_UPPER_SCI1
    Sci_StopPort(&s_port1);
#endif
#ifdef _COMMOM_UPPER_SCI2
    Sci_StopPort(&s_port2);
#endif
}

void App_CommonUpper(void)
{
#ifdef _COMMOM_UPPER_SCI1
    Sci_ServicePort(&s_port1);
#endif
#ifdef _COMMOM_UPPER_SCI2
    Sci_ServicePort(&s_port2);
#endif
    if (s_baud_change_pending) {
        UINT32 primask=__get_PRIMASK();
        __disable_irq();
        /* TC has returned the old-rate ACK buffer before all ports go idle.
         * A broadcast has no ACK; a failed TX switches after its bounded timeout. */
        if (!Sci_PortsBusy()) {
            InitUSART_CommonUpper();
            s_baud_change_pending=0U;
        }
        if (!primask) __enable_irq();
    }
}
