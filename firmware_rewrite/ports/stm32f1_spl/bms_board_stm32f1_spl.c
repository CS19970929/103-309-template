#include "bms_port_stm32f1_spl.h"

#include "stm32f10x_adc.h"
#include "stm32f10x_can.h"
#include "stm32f10x_exti.h"
#include "stm32f10x_flash.h"
#include "stm32f10x_gpio.h"
#include "stm32f10x_pwr.h"
#include "stm32f10x_rcc.h"
#include "stm32f10x_rtc.h"
#include "stm32f10x_usart.h"
#include "misc.h"

#include <string.h>

#define BMS_BOARD_SLAVE_ADDR 1u
#define BMS_BOARD_ADC_MAX 4095u
#define BMS_BOARD_ADC_VREF_MV 3300u
#define BMS_BOARD_VBUS_SCALE 160u
#define BMS_BOARD_CURRENT_ZERO_MV 1650u
#define BMS_BOARD_CURRENT_MV_PER_A10 4u
#define BMS_BOARD_TEMP_DEFAULT_C 25u
#define BMS_BOARD_FLASH_PAGE_SIZE 2048u
#define BMS_BOARD_CAN_STATUS_ID 0x14F80200ul
#define BMS_BOARD_IAP_FLAG_ADDR 0x0801F800ul
#define BMS_BOARD_IAP_FLAG_VALUE 0xA55A00ABul
#define BMS_BOARD_UART_RX_LIMIT 128u
#define BMS_BOARD_RTC_LSE_PRESCALER 32767ul
#define BMS_BOARD_RTC_LSI_PRESCALER 39999ul
#define BMS_BOARD_RTC_CLOCK_TIMEOUT 1000000ul
#define BMS_BOARD_MODBUS_RESPONSE_LIMIT 160u

#define BMS_GPIO_ADC_VBUS GPIOA
#define BMS_PIN_ADC_VBUS GPIO_Pin_1
#define BMS_GPIO_ADC_CUR GPIOA
#define BMS_PIN_ADC_CUR GPIO_Pin_2
#define BMS_GPIO_ADC_NMOS GPIOB
#define BMS_PIN_ADC_NMOS GPIO_Pin_1
#define BMS_GPIO_CMNT_EN GPIOB
#define BMS_PIN_CMNT_EN GPIO_Pin_4
#define BMS_GPIO_MCU_WK GPIOB
#define BMS_PIN_MCU_WK GPIO_Pin_13
#define BMS_GPIO_SEG_EN GPIOB
#define BMS_PIN_SEG_EN GPIO_Pin_10
#define BMS_GPIO_DBG_LED GPIOB
#define BMS_PIN_DBG_LED GPIO_Pin_15
#define BMS_GPIO_AFE_CTL GPIOB
#define BMS_PIN_AFE_CTL GPIO_Pin_14
#define BMS_GPIO_AFE_PRO GPIOB
#define BMS_PIN_AFE_PRO GPIO_Pin_0

static volatile uint32_t s_ms_tick;
static uint8_t s_uart_rx[BMS_BOARD_UART_RX_LIMIT];
static uint8_t s_uart_rx_len;
static bms_soc_report_t s_last_report;
static bool s_rtc_ready;

static bool read_holding_words(const bms_app_t *app, uint16_t address, uint16_t count, uint16_t *words)
{
    uint16_t i;

    if (app == NULL || words == NULL || count == 0u) {
        return false;
    }

    if (address == 0xD000u && count <= BMS_RO_D000_WORDS) {
        return bms_comm_read_d000(app, words, BMS_RO_D000_WORDS);
    }

    if (address == BMS_ADDR_SOC_TEST_STATUS && count <= BMS_RO_D300_WORDS) {
        return bms_comm_read_d300(app, words, BMS_RO_D300_WORDS);
    }

    if (address >= BMS_ADDR_SOC_PARAM_START &&
        address <= BMS_ADDR_SOC_PARAM_END &&
        count <= (uint16_t)(BMS_ADDR_SOC_PARAM_END - address + 1u)) {
        for (i = 0u; i < count; ++i) {
            switch ((uint16_t)(address + i)) {
            case 0x2318u:
                words[i] = app->config.capacity_ah10;
                break;
            case 0x2319u:
                words[i] = app->config.cycle_count;
                break;
            case 0x231Au:
                words[i] = app->config.full_cell_mv;
                break;
            case 0x231Bu:
                words[i] = app->config.empty_cell_mv;
                break;
            default:
                return false;
            }
        }
        return true;
    }

    if (address >= BMS_ADDR_SOC_TABLE_START &&
        address <= BMS_ADDR_SOC_TABLE_END &&
        count <= (uint16_t)(BMS_ADDR_SOC_TABLE_END - address + 1u)) {
        for (i = 0u; i < count; ++i) {
            uint16_t word_index = (uint16_t)(address + i - BMS_ADDR_SOC_TABLE_START);
            uint16_t point = (uint16_t)(word_index / 2u);
            if (point >= BMS_SOC_TABLE_POINTS) {
                return false;
            }
            words[i] = (word_index & 1u) == 0u ?
                       app->soc.ocv_table[point].voltage_mv :
                       app->soc.ocv_table[point].soc_percent;
        }
        return true;
    }

    return false;
}

static uint16_t read_adc_channel(uint8_t channel)
{
    ADC_RegularChannelConfig(ADC1, channel, 1u, ADC_SampleTime_239Cycles5);
    ADC_SoftwareStartConvCmd(ADC1, ENABLE);
    while (ADC_GetFlagStatus(ADC1, ADC_FLAG_EOC) == RESET) {
    }
    return ADC_GetConversionValue(ADC1);
}

static uint16_t adc_to_mv(uint16_t adc)
{
    return (uint16_t)(((uint32_t)adc * BMS_BOARD_ADC_VREF_MV) / BMS_BOARD_ADC_MAX);
}

static void gpio_set(GPIO_TypeDef *port, uint16_t pin, bool high)
{
    GPIO_WriteBit(port, pin, high ? Bit_SET : Bit_RESET);
}

static uint16_t modbus_crc16(const uint8_t *data, uint16_t len)
{
    uint16_t crc = 0xFFFFu;
    uint16_t i;
    uint8_t bit;

    for (i = 0u; i < len; ++i) {
        crc ^= data[i];
        for (bit = 0u; bit < 8u; ++bit) {
            if ((crc & 1u) != 0u) {
                crc = (uint16_t)((crc >> 1) ^ 0xA001u);
            } else {
                crc >>= 1;
            }
        }
    }
    return crc;
}

static void uart_send_byte(uint8_t data)
{
    while (USART_GetFlagStatus(USART1, USART_FLAG_TXE) == RESET) {
    }
    USART_SendData(USART1, data);
}

static void uart_send(const uint8_t *data, uint16_t len)
{
    uint16_t i;

    for (i = 0u; i < len; ++i) {
        uart_send_byte(data[i]);
    }
    while (USART_GetFlagStatus(USART1, USART_FLAG_TC) == RESET) {
    }
}

static void modbus_send(uint8_t *frame, uint16_t len_without_crc)
{
    uint16_t crc = modbus_crc16(frame, len_without_crc);

    frame[len_without_crc] = (uint8_t)(crc & 0xFFu);
    frame[len_without_crc + 1u] = (uint8_t)(crc >> 8);
    uart_send(frame, (uint16_t)(len_without_crc + 2u));
}

static bool flash_write_page(uint32_t address, const void *data, uint16_t len)
{
    const uint8_t *bytes = (const uint8_t *)data;
    uint16_t offset;

    if ((address < BMS_FLASH_STORAGE_START) || (len > BMS_BOARD_FLASH_PAGE_SIZE)) {
        return false;
    }

    FLASH_Unlock();
    if (FLASH_ErasePage(address) != FLASH_COMPLETE) {
        FLASH_Lock();
        return false;
    }

    for (offset = 0u; offset < len; offset += 2u) {
        uint16_t halfword = bytes[offset];
        if ((uint16_t)(offset + 1u) < len) {
            halfword |= (uint16_t)((uint16_t)bytes[offset + 1u] << 8);
        } else {
            halfword |= 0xFF00u;
        }
        if (FLASH_ProgramHalfWord(address + offset, halfword) != FLASH_COMPLETE) {
            FLASH_Lock();
            return false;
        }
    }

    FLASH_Lock();
    return memcmp((const void *)address, data, len) == 0;
}

static bool wait_rcc_flag(uint8_t flag, uint32_t timeout)
{
    while (timeout > 0u) {
        if (RCC_GetFlagStatus(flag) != RESET) {
            return true;
        }
        --timeout;
    }
    return false;
}

static bool init_rtc_clock(void)
{
    uint32_t prescaler = BMS_BOARD_RTC_LSE_PRESCALER;
    uint32_t source = RCC_RTCCLKSource_LSE;

    if ((RCC->BDCR & RCC_BDCR_RTCEN) != 0u) {
        RTC_WaitForSynchro();
        RTC_WaitForLastTask();
        return true;
    }

    RCC_LSEConfig(RCC_LSE_ON);
    if (!wait_rcc_flag(RCC_FLAG_LSERDY, BMS_BOARD_RTC_CLOCK_TIMEOUT)) {
        RCC_LSEConfig(RCC_LSE_OFF);
        RCC_LSICmd(ENABLE);
        if (!wait_rcc_flag(RCC_FLAG_LSIRDY, BMS_BOARD_RTC_CLOCK_TIMEOUT)) {
            return false;
        }
        source = RCC_RTCCLKSource_LSI;
        prescaler = BMS_BOARD_RTC_LSI_PRESCALER;
    }

    RCC_RTCCLKConfig(source);
    RCC_RTCCLKCmd(ENABLE);
    RTC_WaitForSynchro();
    RTC_WaitForLastTask();
    RTC_SetPrescaler(prescaler);
    RTC_WaitForLastTask();
    RTC_SetCounter(0u);
    RTC_WaitForLastTask();
    return true;
}

static const bms_snapshot_t *slot_a(void)
{
    return (const bms_snapshot_t *)BMS_FLASH_SOC_SLOT_A;
}

static const bms_snapshot_t *slot_b(void)
{
    return (const bms_snapshot_t *)BMS_FLASH_SOC_SLOT_B;
}

static uint32_t choose_snapshot_slot(void)
{
    const bool a_valid = bms_storage_validate(slot_a());
    const bool b_valid = bms_storage_validate(slot_b());

    if (!a_valid) {
        return BMS_FLASH_SOC_SLOT_A;
    }
    if (!b_valid) {
        return BMS_FLASH_SOC_SLOT_B;
    }
    return slot_a()->sequence <= slot_b()->sequence ? BMS_FLASH_SOC_SLOT_A : BMS_FLASH_SOC_SLOT_B;
}

static void init_gpio(void)
{
    GPIO_InitTypeDef gpio;

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_AFIO |
                           RCC_APB2Periph_GPIOA |
                           RCC_APB2Periph_GPIOB, ENABLE);
    GPIO_PinRemapConfig(GPIO_Remap_SWJ_JTAGDisable, ENABLE);
    GPIO_PinRemapConfig(GPIO_Remap_USART1, ENABLE);

    gpio.GPIO_Speed = GPIO_Speed_50MHz;
    gpio.GPIO_Mode = GPIO_Mode_AIN;
    gpio.GPIO_Pin = BMS_PIN_ADC_VBUS | BMS_PIN_ADC_CUR;
    GPIO_Init(BMS_GPIO_ADC_VBUS, &gpio);
    gpio.GPIO_Pin = BMS_PIN_ADC_NMOS;
    GPIO_Init(BMS_GPIO_ADC_NMOS, &gpio);

    gpio.GPIO_Mode = GPIO_Mode_IPD;
    gpio.GPIO_Pin = BMS_PIN_MCU_WK;
    GPIO_Init(BMS_GPIO_MCU_WK, &gpio);

    gpio.GPIO_Mode = GPIO_Mode_Out_PP;
    gpio.GPIO_Pin = BMS_PIN_CMNT_EN | BMS_PIN_SEG_EN | BMS_PIN_DBG_LED |
                    BMS_PIN_AFE_CTL | BMS_PIN_AFE_PRO;
    GPIO_Init(GPIOB, &gpio);

    gpio.GPIO_Mode = GPIO_Mode_AF_PP;
    gpio.GPIO_Pin = GPIO_Pin_6;
    GPIO_Init(GPIOB, &gpio);
    gpio.GPIO_Mode = GPIO_Mode_IN_FLOATING;
    gpio.GPIO_Pin = GPIO_Pin_7;
    GPIO_Init(GPIOB, &gpio);

    gpio.GPIO_Mode = GPIO_Mode_IPU;
    gpio.GPIO_Pin = GPIO_Pin_11;
    GPIO_Init(GPIOA, &gpio);
    gpio.GPIO_Mode = GPIO_Mode_AF_PP;
    gpio.GPIO_Pin = GPIO_Pin_12;
    GPIO_Init(GPIOA, &gpio);

    gpio_set(BMS_GPIO_CMNT_EN, BMS_PIN_CMNT_EN, true);
    gpio_set(BMS_GPIO_SEG_EN, BMS_PIN_SEG_EN, false);
    gpio_set(BMS_GPIO_DBG_LED, BMS_PIN_DBG_LED, false);
    gpio_set(BMS_GPIO_AFE_CTL, BMS_PIN_AFE_CTL, false);
    gpio_set(BMS_GPIO_AFE_PRO, BMS_PIN_AFE_PRO, true);
}

static void init_adc(void)
{
    ADC_InitTypeDef adc;

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_ADC1, ENABLE);
    RCC_ADCCLKConfig(RCC_PCLK2_Div6);
    ADC_DeInit(ADC1);
    adc.ADC_Mode = ADC_Mode_Independent;
    adc.ADC_ScanConvMode = DISABLE;
    adc.ADC_ContinuousConvMode = DISABLE;
    adc.ADC_ExternalTrigConv = ADC_ExternalTrigConv_None;
    adc.ADC_DataAlign = ADC_DataAlign_Right;
    adc.ADC_NbrOfChannel = 1u;
    ADC_Init(ADC1, &adc);
    ADC_Cmd(ADC1, ENABLE);
    ADC_ResetCalibration(ADC1);
    while (ADC_GetResetCalibrationStatus(ADC1) != RESET) {
    }
    ADC_StartCalibration(ADC1);
    while (ADC_GetCalibrationStatus(ADC1) != RESET) {
    }
}

static void init_usart(void)
{
    USART_InitTypeDef usart;
    NVIC_InitTypeDef nvic;

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_USART1, ENABLE);
    USART_DeInit(USART1);
    usart.USART_BaudRate = 19200u;
    usart.USART_WordLength = USART_WordLength_8b;
    usart.USART_StopBits = USART_StopBits_1;
    usart.USART_Parity = USART_Parity_No;
    usart.USART_Mode = USART_Mode_Rx | USART_Mode_Tx;
    usart.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
    USART_Init(USART1, &usart);
    USART_ITConfig(USART1, USART_IT_RXNE, ENABLE);

    nvic.NVIC_IRQChannel = USART1_IRQn;
    nvic.NVIC_IRQChannelPreemptionPriority = 2u;
    nvic.NVIC_IRQChannelSubPriority = 0u;
    nvic.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&nvic);
    USART_Cmd(USART1, ENABLE);
}

static void init_can(void)
{
    CAN_InitTypeDef can;
    CAN_FilterInitTypeDef filter;

    RCC_APB1PeriphClockCmd(RCC_APB1Periph_CAN1, ENABLE);
    CAN_DeInit(CAN1);
    CAN_StructInit(&can);
    can.CAN_TTCM = DISABLE;
    can.CAN_ABOM = ENABLE;
    can.CAN_AWUM = ENABLE;
    can.CAN_NART = ENABLE;
    can.CAN_RFLM = DISABLE;
    can.CAN_TXFP = DISABLE;
    can.CAN_Mode = CAN_Mode_Normal;
    can.CAN_SJW = CAN_SJW_1tq;
    can.CAN_BS1 = CAN_BS1_13tq;
    can.CAN_BS2 = CAN_BS2_2tq;
    can.CAN_Prescaler = 2u;
    (void)CAN_Init(CAN1, &can);

    filter.CAN_FilterNumber = 0u;
    filter.CAN_FilterMode = CAN_FilterMode_IdMask;
    filter.CAN_FilterScale = CAN_FilterScale_32bit;
    filter.CAN_FilterIdHigh = 0u;
    filter.CAN_FilterIdLow = 0u;
    filter.CAN_FilterMaskIdHigh = 0u;
    filter.CAN_FilterMaskIdLow = 0u;
    filter.CAN_FilterFIFOAssignment = CAN_FIFO0;
    filter.CAN_FilterActivation = ENABLE;
    CAN_FilterInit(&filter);
}

static void init_rtc(void)
{
    EXTI_InitTypeDef exti;
    NVIC_InitTypeDef nvic;

    RCC_APB1PeriphClockCmd(RCC_APB1Periph_PWR | RCC_APB1Periph_BKP, ENABLE);
    PWR_BackupAccessCmd(ENABLE);

    s_rtc_ready = init_rtc_clock();
    if (!s_rtc_ready) {
        return;
    }

    RTC_ITConfig(RTC_IT_SEC, DISABLE);
    RTC_ITConfig(RTC_IT_ALR, DISABLE);
    RTC_ClearITPendingBit(RTC_IT_ALR);
    RTC_ClearFlag(RTC_FLAG_ALR);
    RTC_WaitForLastTask();

    EXTI_ClearITPendingBit(EXTI_Line17);
    exti.EXTI_Line = EXTI_Line17;
    exti.EXTI_Mode = EXTI_Mode_Interrupt;
    exti.EXTI_Trigger = EXTI_Trigger_Rising;
    exti.EXTI_LineCmd = ENABLE;
    EXTI_Init(&exti);

    nvic.NVIC_IRQChannel = RTCAlarm_IRQn;
    nvic.NVIC_IRQChannelPreemptionPriority = 2u;
    nvic.NVIC_IRQChannelSubPriority = 1u;
    nvic.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&nvic);
}

void bms_stm32f1_board_init(void)
{
    init_gpio();
    init_adc();
    init_usart();
    init_can();
    init_rtc();
    SysTick_Config(SystemCoreClock / 1000u);
}

bool bms_stm32f1_board_read_sample(bms_sample_t *sample)
{
    uint16_t vbus_mv;
    uint16_t cur_mv;
    uint16_t pack_mv;
    int32_t current_delta_mv;

    if (sample == NULL) {
        return false;
    }

    *sample = bms_sample_default();
    vbus_mv = adc_to_mv(read_adc_channel(ADC_Channel_1));
    cur_mv = adc_to_mv(read_adc_channel(ADC_Channel_2));
    pack_mv = (uint16_t)((uint32_t)vbus_mv * BMS_BOARD_VBUS_SCALE);
    sample->pack_mv = pack_mv;
    sample->cell_count = 10u;
    sample->vcell_min_mv = (uint16_t)(pack_mv / sample->cell_count);
    sample->vcell_max_mv = sample->vcell_min_mv;
    sample->vcell_delta_mv = 0u;
    sample->vcell[0] = sample->vcell_min_mv;
    sample->temp_max_c = BMS_BOARD_TEMP_DEFAULT_C;
    sample->temp_min_c = BMS_BOARD_TEMP_DEFAULT_C;
    sample->mcu_wake = GPIO_ReadInputDataBit(BMS_GPIO_MCU_WK, BMS_PIN_MCU_WK) == Bit_SET;

    current_delta_mv = (int32_t)cur_mv - (int32_t)BMS_BOARD_CURRENT_ZERO_MV;
    if (current_delta_mv > 0) {
        sample->ichg_a10 = (uint16_t)(current_delta_mv / BMS_BOARD_CURRENT_MV_PER_A10);
    } else {
        sample->idsg_a10 = (uint16_t)((-current_delta_mv) / BMS_BOARD_CURRENT_MV_PER_A10);
    }
    sample->charger_present = sample->ichg_a10 > 0u;
    return true;
}

bool bms_stm32f1_board_save_snapshot(const bms_snapshot_t *snapshot)
{
    if (snapshot == NULL) {
        return false;
    }
    return flash_write_page(choose_snapshot_slot(), snapshot, sizeof(*snapshot));
}

bool bms_stm32f1_board_load_snapshot(bms_snapshot_t *snapshot)
{
    const bool a_valid = bms_storage_validate(slot_a());
    const bool b_valid = bms_storage_validate(slot_b());

    if (snapshot == NULL || (!a_valid && !b_valid)) {
        return false;
    }

    if (a_valid && (!b_valid || slot_a()->sequence >= slot_b()->sequence)) {
        *snapshot = *slot_a();
    } else {
        *snapshot = *slot_b();
    }
    return true;
}

bool bms_stm32f1_board_can_send_probe(void)
{
    return bms_stm32f1_board_can_send_status(&s_last_report);
}

bool bms_stm32f1_board_can_send_status(const bms_soc_report_t *report)
{
    CanTxMsg msg;
    uint8_t mailbox;

    if (report != NULL) {
        s_last_report = *report;
    }

    gpio_set(BMS_GPIO_CMNT_EN, BMS_PIN_CMNT_EN, false);
    memset(&msg, 0, sizeof(msg));
    msg.IDE = CAN_ID_EXT;
    msg.RTR = CAN_RTR_DATA;
    msg.ExtId = BMS_BOARD_CAN_STATUS_ID;
    msg.DLC = 8u;
    msg.Data[0] = s_last_report.display_soc;
    msg.Data[1] = s_last_report.soh;
    msg.Data[2] = (uint8_t)(s_last_report.capacity_now_ah100 >> 8);
    msg.Data[3] = (uint8_t)s_last_report.capacity_now_ah100;
    msg.Data[4] = (uint8_t)(s_last_report.cycle_count >> 8);
    msg.Data[5] = (uint8_t)s_last_report.cycle_count;
    msg.Data[6] = (uint8_t)s_last_report.mode;
    msg.Data[7] = s_last_report.full_anchor ? 1u : 0u;
    mailbox = CAN_Transmit(CAN1, &msg);
    if (mailbox == CAN_TxStatus_NoMailBox) {
        gpio_set(BMS_GPIO_CMNT_EN, BMS_PIN_CMNT_EN, true);
        return false;
    }
    return true;
}

void bms_stm32f1_board_set_charge_mos(bool on)
{
    gpio_set(BMS_GPIO_AFE_CTL, BMS_PIN_AFE_CTL, on);
}

void bms_stm32f1_board_set_discharge_mos(bool on)
{
    gpio_set(BMS_GPIO_AFE_PRO, BMS_PIN_AFE_PRO, on);
}

void bms_stm32f1_board_set_display(bool on, uint8_t soc, bool charge_icon)
{
    (void)soc;
    gpio_set(BMS_GPIO_SEG_EN, BMS_PIN_SEG_EN, on);
    gpio_set(BMS_GPIO_DBG_LED, BMS_PIN_DBG_LED, charge_icon);
}

void bms_stm32f1_board_enter_rtc_stop(uint16_t seconds)
{
    uint32_t wake_seconds = seconds == 0u ? 1u : seconds;

    gpio_set(BMS_GPIO_CMNT_EN, BMS_PIN_CMNT_EN, true);
    if (!s_rtc_ready) {
        return;
    }

    RCC_APB1PeriphClockCmd(RCC_APB1Periph_PWR, ENABLE);
    RTC_ITConfig(RTC_IT_SEC, DISABLE);
    RTC_ITConfig(RTC_IT_ALR, DISABLE);
    RTC_ClearITPendingBit(RTC_IT_ALR);
    RTC_ClearFlag(RTC_FLAG_ALR);
    RTC_WaitForLastTask();
    EXTI_ClearITPendingBit(EXTI_Line17);
    RTC_SetAlarm(RTC_GetCounter() + wake_seconds);
    RTC_WaitForLastTask();
    RTC_ITConfig(RTC_IT_ALR, ENABLE);
    RTC_WaitForLastTask();
    PWR_ClearFlag(PWR_FLAG_WU);

    PWR_EnterSTOPMode(PWR_Regulator_LowPower, PWR_STOPEntry_WFI);
    SystemCoreClockUpdate();
    SysTick_Config(SystemCoreClock / 1000u);
}

void bms_stm32f1_board_request_iap_reset(void)
{
    uint32_t flag = BMS_BOARD_IAP_FLAG_VALUE;

    (void)flash_write_page(BMS_BOARD_IAP_FLAG_ADDR, &flag, sizeof(flag));
    NVIC_SystemReset();
}

bool bms_stm32f1_board_wait_tick(uint32_t tick_ms)
{
    uint32_t start = s_ms_tick;

    while ((uint32_t)(s_ms_tick - start) < tick_ms) {
    }
    return true;
}

void bms_stm32f1_board_poll(bms_app_t *app)
{
    uint16_t crc;
    uint8_t fn;
    uint16_t address;
    uint16_t count;
    uint8_t response[BMS_BOARD_MODBUS_RESPONSE_LIMIT];
    uint16_t words[BMS_RO_D000_WORDS];
    uint16_t write_words[64];
    uint16_t i;
    uint8_t byte_count;

    if (app == NULL || s_uart_rx_len < 8u || s_uart_rx[0] != BMS_BOARD_SLAVE_ADDR) {
        return;
    }

    crc = (uint16_t)s_uart_rx[s_uart_rx_len - 2u] |
          (uint16_t)((uint16_t)s_uart_rx[s_uart_rx_len - 1u] << 8);
    if (crc != modbus_crc16(s_uart_rx, (uint16_t)(s_uart_rx_len - 2u))) {
        s_uart_rx_len = 0u;
        return;
    }

    fn = s_uart_rx[1];
    address = (uint16_t)(((uint16_t)s_uart_rx[2] << 8) | s_uart_rx[3]);
    count = (uint16_t)(((uint16_t)s_uart_rx[4] << 8) | s_uart_rx[5]);

    if (fn == 0x03u && count <= BMS_RO_D000_WORDS &&
        read_holding_words(app, address, count, words)) {
        response[0] = BMS_BOARD_SLAVE_ADDR;
        response[1] = fn;
        response[2] = (uint8_t)(count * 2u);
        for (i = 0u; i < count; ++i) {
            response[3u + i * 2u] = (uint8_t)(words[i] >> 8);
            response[4u + i * 2u] = (uint8_t)words[i];
        }
        modbus_send(response, (uint16_t)(3u + count * 2u));
    } else if (fn == 0x06u && bms_comm_write_single(app, address, count)) {
        modbus_send(s_uart_rx, 6u);
    } else if (fn == 0x10u && count <= 64u && s_uart_rx_len >= (uint8_t)(9u + count * 2u)) {
        byte_count = s_uart_rx[6];
        if (byte_count == (uint8_t)(count * 2u)) {
            for (i = 0u; i < count; ++i) {
                write_words[i] = (uint16_t)(((uint16_t)s_uart_rx[7u + i * 2u] << 8) |
                                            s_uart_rx[8u + i * 2u]);
            }
            if (bms_comm_write_block(app, address, write_words, count)) {
                response[0] = BMS_BOARD_SLAVE_ADDR;
                response[1] = fn;
                response[2] = (uint8_t)(address >> 8);
                response[3] = (uint8_t)address;
                response[4] = (uint8_t)(count >> 8);
                response[5] = (uint8_t)count;
                modbus_send(response, 6u);
            }
        }
    }

    s_uart_rx_len = 0u;
}

void bms_stm32f1_tick_isr(void)
{
    ++s_ms_tick;
}

void bms_stm32f1_can_rx_isr(void)
{
    CanRxMsg msg;

    if (CAN_MessagePending(CAN1, CAN_FIFO0) > 0u) {
        CAN_Receive(CAN1, CAN_FIFO0, &msg);
    }
}

void bms_stm32f1_usart_rx_isr(void)
{
    if (USART_GetITStatus(USART1, USART_IT_RXNE) != RESET) {
        uint8_t byte = (uint8_t)USART_ReceiveData(USART1);
        if (s_uart_rx_len < BMS_BOARD_UART_RX_LIMIT) {
            s_uart_rx[s_uart_rx_len++] = byte;
        } else {
            s_uart_rx_len = 0u;
        }
    }
}
