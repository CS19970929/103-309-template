#include "main.h"
#include "SerialProtocolMux.h"
#include "LegacyModbusProtocol.h"
#include "LxPowerProtocol.h"
#include "LxPowerProtocolPort.h"

#include <string.h>

/*
 * UART 公共传输层 + 协议分发层。
 *
 * 帧首字节天然不冲突：
 *   0x00 / 0x01 -> 原 CommonUpper / Modbus RTU
 *   0x5A / 0xDD -> LX V0.9
 *
 * ISR 仅负责字节收发和完整帧提交；协议处理在 SerialProtocolMux_Process()
 * 中执行，避免在中断中运行 Flash、AFE、MOS 等业务。
 */

#define SERIAL_USART_ERROR_FLAGS ((UINT16)(USART_SR_ORE | USART_SR_NE | USART_SR_FE | USART_SR_PE))

#if (PROJECT_CFG_SERIES_NUM != 7)
#error "LX V0.9 C073 product must be fixed to 7S"
#endif

#if ((PROJECT_CFG_SCI1_BAUDRATE != 9600) && \
     (PROJECT_CFG_SCI1_BAUDRATE != 19200) && \
     (PROJECT_CFG_SCI1_BAUDRATE != 115200))
#error "Unsupported PROJECT_CFG_SCI1_BAUDRATE"
#endif

#if ((PROJECT_CFG_SCI2_BAUDRATE != 9600) && \
     (PROJECT_CFG_SCI2_BAUDRATE != 19200) && \
     (PROJECT_CFG_SCI2_BAUDRATE != 115200))
#error "Unsupported PROJECT_CFG_SCI2_BAUDRATE"
#endif

typedef enum
{
    SERIAL_PROTOCOL_NONE = 0,
    SERIAL_PROTOCOL_LEGACY_MODBUS,
    SERIAL_PROTOCOL_LX_V09
} SERIAL_PROTOCOL_KIND;

typedef struct
{
    USART_TypeDef *usart;
    LEGACY_MODBUS_PROTOCOL_CONTEXT legacy;
    LX_POWER_PROTOCOL_CONTEXT lx;
    SERIAL_PROTOCOL_KIND active_protocol;
    volatile uint8_t frame_pending;
    volatile uint8_t tx_active;
    volatile uint16_t tx_index;
    volatile uint16_t tx_length;
    uint8_t * volatile tx_buffer;
    volatile uint16_t error_count;
} SERIAL_PROTOCOL_PORT;

static SERIAL_PROTOCOL_PORT s_serial1;
static SERIAL_PROTOCOL_PORT s_serial2;

static void Serial_ResetProtocol(SERIAL_PROTOCOL_PORT *port)
{
    LegacyModbusProtocol_Reset(&port->legacy);
    LxPowerProtocol_Reset(&port->lx);
    port->active_protocol = SERIAL_PROTOCOL_NONE;
}

static uint8_t Serial_StartProtocolFromByte(SERIAL_PROTOCOL_PORT *port, uint8_t data)
{
    if (LegacyModbusProtocol_IsStartByte(data))
    {
        port->active_protocol = SERIAL_PROTOCOL_LEGACY_MODBUS;
        return LegacyModbusProtocol_Feed(&port->legacy, data);
    }

    if (LxPowerProtocol_IsStartByte(data))
    {
        port->active_protocol = SERIAL_PROTOCOL_LX_V09;
        return LxPowerProtocol_Feed(&port->lx, data);
    }

    port->active_protocol = SERIAL_PROTOCOL_NONE;
    return 0U;
}

static uint8_t Serial_FeedProtocol(SERIAL_PROTOCOL_PORT *port, uint8_t data)
{
    uint8_t complete = 0U;

    if (port->active_protocol == SERIAL_PROTOCOL_NONE)
    {
        return Serial_StartProtocolFromByte(port, data);
    }

    if (port->active_protocol == SERIAL_PROTOCOL_LEGACY_MODBUS)
    {
        complete = LegacyModbusProtocol_Feed(&port->legacy, data);
        if ((!complete) && (!LegacyModbusProtocol_IsBusy(&port->legacy)))
        {
            /*
             * 旧 parser 判定失败后，允许把当前字节重新视为 LX 帧头，
             * 这样前导噪声不会让另一个协议必须等到下一次 IDLE 才恢复。
             */
            port->active_protocol = SERIAL_PROTOCOL_NONE;
            if (LxPowerProtocol_IsStartByte(data))
            {
                complete = Serial_StartProtocolFromByte(port, data);
            }
        }
    }
    else if (port->active_protocol == SERIAL_PROTOCOL_LX_V09)
    {
        complete = LxPowerProtocol_Feed(&port->lx, data);
        if ((!complete) && (!LxPowerProtocol_IsBusy(&port->lx)))
        {
            port->active_protocol = SERIAL_PROTOCOL_NONE;
            if (LegacyModbusProtocol_IsStartByte(data))
            {
                complete = Serial_StartProtocolFromByte(port, data);
            }
        }
    }
    else
    {
        Serial_ResetProtocol(port);
    }

    return complete;
}

static void Serial_OnRxIdle(SERIAL_PROTOCOL_PORT *port)
{
    if (port->active_protocol == SERIAL_PROTOCOL_LEGACY_MODBUS)
    {
        LegacyModbusProtocol_OnRxIdle(&port->legacy);
    }
    else if (port->active_protocol == SERIAL_PROTOCOL_LX_V09)
    {
        LxPowerProtocol_OnRxIdle(&port->lx);
    }

    port->active_protocol = SERIAL_PROTOCOL_NONE;
}

static void Serial_ArmReceiver(SERIAL_PROTOCOL_PORT *port)
{
    volatile UINT16 dummy;

    port->frame_pending = 0U;
    port->tx_active = 0U;
    port->tx_index = 0U;
    port->tx_length = 0U;
    port->tx_buffer = 0;

    dummy = port->usart->SR;
    dummy = port->usart->DR;
    (void)dummy;

    port->usart->CR1 |= (USART_CR1_RE | USART_CR1_RXNEIE | USART_CR1_IDLEIE);
    port->usart->CR1 &= (UINT16)~(USART_CR1_TXEIE | USART_CR1_TCIE);
}

static void Serial_Abort(SERIAL_PROTOCOL_PORT *port)
{
    Serial_ResetProtocol(port);
    Serial_ArmReceiver(port);
}

static void Serial_StartTx(SERIAL_PROTOCOL_PORT *port)
{
    port->tx_index = 0U;
    port->tx_active = 1U;
    port->usart->CR1 &= (UINT16)~(USART_CR1_RE |
                                         USART_CR1_RXNEIE |
                                         USART_CR1_IDLEIE |
                                         USART_CR1_TCIE);
    port->usart->CR1 |= (USART_CR1_TE | USART_CR1_TXEIE);
}

static void Serial_FinishTx(SERIAL_PROTOCOL_PORT *port)
{
    USART_ClearFlag(port->usart, USART_FLAG_TC);
    port->usart->CR1 &= (UINT16)~USART_CR1_TCIE;

    /* 保留原 CommonUpper：应答发完后再提交 IAP/Flash 更新标志。 */
    if (u8FlashUpdateE2PROM != 0U)
    {
        u8FlashUpdateE2PROM = 0U;
        u8FlashUpdateFlag = 1U;
    }

    Serial_ResetProtocol(port);
    Serial_ArmReceiver(port);
}

static void Serial_ProcessPort(SERIAL_PROTOCOL_PORT *port)
{
    uint8_t pending;
    UINT32 primask;

    primask = __get_PRIMASK();
    __disable_irq();
    pending = port->frame_pending;
    port->frame_pending = 0U;
    if (primask == 0U)
    {
        __enable_irq();
    }

    if (pending == 0U)
    {
        return;
    }

    port->tx_buffer = 0;
    port->tx_length = 0U;

    if (port->active_protocol == SERIAL_PROTOCOL_LEGACY_MODBUS)
    {
        LegacyModbusProtocol_Process(&port->legacy);
        port->tx_buffer = LegacyModbusProtocol_GetTxBuffer(&port->legacy);
        port->tx_length = LegacyModbusProtocol_GetTxLength(&port->legacy);
    }
    else if (port->active_protocol == SERIAL_PROTOCOL_LX_V09)
    {
        LxPowerProtocol_Process(&port->lx);
        port->tx_buffer = LxPowerProtocol_GetTxBuffer(&port->lx);
        port->tx_length = LxPowerProtocol_GetTxLength(&port->lx);
    }

    if ((port->tx_buffer != 0) && (port->tx_length != 0U))
    {
        Serial_StartTx(port);
    }
    else
    {
        Serial_Abort(port);
    }
}

static void Serial_IRQHandler(SERIAL_PROTOCOL_PORT *port)
{
    UINT16 status;
    volatile UINT16 dummy;

    status = port->usart->SR;
    if ((status & SERIAL_USART_ERROR_FLAGS) != 0U)
    {
        dummy = port->usart->DR;
        (void)dummy;
        ++port->error_count;
        Serial_Abort(port);
        return;
    }

    if (((status & USART_SR_RXNE) != 0U) &&
        ((port->usart->CR1 & USART_CR1_RXNEIE) != 0U))
    {
        uint8_t data;

        SleepDeal_RecordExternalComm();
        data = (uint8_t)port->usart->DR;
        if (Serial_FeedProtocol(port, data))
        {
            port->frame_pending = 1U;
            port->usart->CR1 &= (UINT16)~(USART_CR1_RE |
                                                  USART_CR1_RXNEIE |
                                                  USART_CR1_IDLEIE);
        }
    }

    status = port->usart->SR;
    if (((status & USART_SR_IDLE) != 0U) &&
        ((port->usart->CR1 & USART_CR1_IDLEIE) != 0U))
    {
        dummy = port->usart->DR;
        (void)dummy;
        if ((port->frame_pending == 0U) && (port->tx_active == 0U))
        {
            Serial_OnRxIdle(port);
        }
    }

    if (((port->usart->SR & USART_SR_TXE) != 0U) &&
        ((port->usart->CR1 & USART_CR1_TXEIE) != 0U))
    {
        if ((port->tx_buffer == 0) || (port->tx_index >= port->tx_length))
        {
            port->usart->CR1 &= (UINT16)~USART_CR1_TXEIE;
            port->usart->CR1 |= USART_CR1_TCIE;
        }
        else
        {
            port->usart->DR = port->tx_buffer[port->tx_index++];
            if (port->tx_index >= port->tx_length)
            {
                port->usart->CR1 &= (UINT16)~USART_CR1_TXEIE;
                port->usart->CR1 |= USART_CR1_TCIE;
            }
        }
    }

    if (((port->usart->SR & USART_SR_TC) != 0U) &&
        ((port->usart->CR1 & USART_CR1_TCIE) != 0U))
    {
        Serial_FinishTx(port);
    }
}

static void Serial_InitPort(SERIAL_PROTOCOL_PORT *port,
                            USART_TypeDef *usart,
                            IRQn_Type irq,
                            uint8_t apb2,
                            UINT32 usart_clock,
                            GPIO_TypeDef *tx_gpio,
                            UINT16 tx_pin,
                            GPIO_TypeDef *rx_gpio,
                            UINT16 rx_pin,
                            UINT32 remap,
                            UINT32 baudrate)
{
    GPIO_InitTypeDef gpio;
    USART_InitTypeDef serial;
    NVIC_InitTypeDef nvic;

    memset(port, 0, sizeof(*port));
    port->usart = usart;
    LegacyModbusProtocol_Init(&port->legacy);
    LxPowerProtocol_Init(&port->lx, &g_lx_power_protocol_adapter);
    port->active_protocol = SERIAL_PROTOCOL_NONE;

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_AFIO |
                           RCC_APB2Periph_GPIOA |
                           RCC_APB2Periph_GPIOB,
                           ENABLE);
    if (apb2)
    {
        RCC_APB2PeriphClockCmd(usart_clock, ENABLE);
    }
    else
    {
        RCC_APB1PeriphClockCmd(usart_clock, ENABLE);
    }

    if (remap != 0U)
    {
        GPIO_PinRemapConfig(remap, ENABLE);
    }

    USART_Cmd(usart, DISABLE);
    USART_DeInit(usart);

    gpio.GPIO_Pin = tx_pin;
    gpio.GPIO_Speed = GPIO_Speed_10MHz;
    gpio.GPIO_Mode = GPIO_Mode_AF_PP;
    GPIO_Init(tx_gpio, &gpio);

    gpio.GPIO_Pin = rx_pin;
    gpio.GPIO_Mode = GPIO_Mode_IN_FLOATING;
    GPIO_Init(rx_gpio, &gpio);

    serial.USART_BaudRate = baudrate;
    serial.USART_WordLength = USART_WordLength_8b;
    serial.USART_StopBits = USART_StopBits_1;
    serial.USART_Parity = USART_Parity_No;
    serial.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
    serial.USART_Mode = USART_Mode_Rx | USART_Mode_Tx;
    USART_Init(usart, &serial);

    nvic.NVIC_IRQChannel = irq;
    nvic.NVIC_IRQChannelPreemptionPriority = 3U;
    nvic.NVIC_IRQChannelSubPriority = 3U;
    nvic.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&nvic);

    usart->CR3 |= USART_CR3_EIE;
    USART_ITConfig(usart, USART_IT_RXNE, ENABLE);
    USART_ITConfig(usart, USART_IT_IDLE, ENABLE);
    USART_ITConfig(usart, USART_IT_TXE, DISABLE);
    USART_ITConfig(usart, USART_IT_TC, DISABLE);
    USART_Cmd(usart, ENABLE);

    Serial_ArmReceiver(port);
}

void SerialProtocolMux_Init(void)
{
    Serial_InitPort(&s_serial1,
                    USART1,
                    USART1_IRQn,
                    1U,
                    RCC_APB2Periph_USART1,
                    GPIOB,
                    GPIO_Pin_6,
                    GPIOB,
                    GPIO_Pin_7,
                    GPIO_Remap_USART1,
                    PROJECT_CFG_SCI1_BAUDRATE);

    Serial_InitPort(&s_serial2,
                    USART2,
                    USART2_IRQn,
                    0U,
                    RCC_APB1Periph_USART2,
                    GPIOA,
                    GPIO_Pin_2,
                    GPIOA,
                    GPIO_Pin_3,
                    0U,
                    PROJECT_CFG_SCI2_BAUDRATE);
}

void SerialProtocolMux_Process(void)
{
    /* 两个 UART 共用同一组 BMS/MOS 状态，只需服务一次软件 MOS 仲裁。 */
    LxPowerProtocol_Service(&s_serial1.lx);
    Serial_ProcessPort(&s_serial1);
    Serial_ProcessPort(&s_serial2);
}

void SerialProtocolMux_USART1_IRQHandler(void)
{
    Serial_IRQHandler(&s_serial1);
}

void SerialProtocolMux_USART2_IRQHandler(void)
{
    Serial_IRQHandler(&s_serial2);
}

uint8_t SerialProtocolMux_IsAnyPortBusy(void)
{
    uint8_t port1_busy;
    uint8_t port2_busy;

    port1_busy = (uint8_t)((s_serial1.frame_pending != 0U) ||
                           (s_serial1.tx_active != 0U) ||
                           (s_serial1.active_protocol != SERIAL_PROTOCOL_NONE));
    port2_busy = (uint8_t)((s_serial2.frame_pending != 0U) ||
                           (s_serial2.tx_active != 0U) ||
                           (s_serial2.active_protocol != SERIAL_PROTOCOL_NONE));
    return (uint8_t)(port1_busy || port2_busy);
}
