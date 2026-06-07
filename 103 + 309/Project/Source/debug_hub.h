#ifndef DEBUG_HUB_H
#define DEBUG_HUB_H

#include <stdint.h>

#define DBG_HUB_MAGIC          ((uint32_t)0x44424748U)
#define DBG_HUB_VERSION        ((uint16_t)1U)
#define DBG_HUB_IRQ_SLOTS      32U
#define DBG_HUB_EXTI_LINES     16U
#define DBG_HUB_GPIO_PORTS     4U
#define DBG_HUB_DMA1_CHANNELS  7U
#define DBG_HUB_ADC_KEYS       8U
#define DBG_HUB_BKP_WORDS      16U

enum DBG_HubUsartPort {
	DBG_HUB_USART1 = 1,
	DBG_HUB_USART2 = 2,
	DBG_HUB_USART3 = 3
};

enum DBG_HubIwdgFeedSource {
	DBG_HUB_IWDG_FEED_INIT = 1,
	DBG_HUB_IWDG_FEED_RUNTIME = 2,
	DBG_HUB_IWDG_FEED_DELAY = 3,
	DBG_HUB_IWDG_FEED_SLEEP = 4
};

typedef struct DBG_HubCtrlTag {
	uint32_t magic;
	uint16_t version;
	uint16_t size;
	uint8_t enable;
	uint8_t freeze;
	uint8_t capture_once;
	uint8_t clear_counters;
	uint8_t fast_period_10ms;
	uint8_t slow_period_10ms;
	uint8_t i2c_sr_auto_read;
	uint8_t reserved0;
	uint32_t init_count;
	uint32_t task_count;
	uint32_t fast_capture_count;
	uint32_t slow_capture_count;
	uint32_t manual_capture_count;
	uint32_t clear_count;
	uint32_t last_task_tick_10ms;
	uint32_t last_fast_tick_10ms;
	uint32_t last_slow_tick_10ms;
} DBG_HubCtrl_t;

typedef struct DBG_HubCoreTag {
	uint32_t scb_icsr;
	uint32_t scb_aircr;
	uint32_t scb_scr;
	uint32_t scb_ccr;
	uint32_t scb_shcsr;
	uint32_t scb_cfsr;
	uint32_t scb_hfsr;
	uint32_t scb_dfsr;
	uint32_t scb_afsr;
	uint32_t primask;
	uint32_t basepri;
	uint32_t faultmask;
	uint16_t current_exception;
	uint8_t in_isr;
	uint8_t interrupts_masked;
} DBG_HubCore_t;

typedef struct DBG_HubSystickTag {
	uint32_t ctrl;
	uint32_t load;
	uint32_t val;
	uint32_t calib;
} DBG_HubSystick_t;

typedef struct DBG_HubClockTag {
	uint32_t sysclk_hz;
	uint32_t hclk_hz;
	uint32_t pclk1_hz;
	uint32_t pclk2_hz;
	uint8_t sysclk_src;
	uint8_t hse_ready;
	uint8_t hsi_ready;
	uint8_t pll_ready;
	uint8_t lsi_ready;
	uint8_t lse_ready;
	uint8_t gpioa_en;
	uint8_t gpiob_en;
	uint8_t gpioc_en;
	uint8_t gpiod_en;
	uint8_t dma1_en;
	uint8_t adc1_en;
	uint8_t usart1_en;
	uint8_t usart2_en;
	uint8_t i2c1_en;
	uint8_t can1_en;
	uint8_t rtc_en;
	uint8_t pwr_en;
	uint8_t bkp_en;
} DBG_HubClock_t;

typedef struct DBG_HubRccTag {
	uint32_t cr;
	uint32_t cfgr;
	uint32_t cir;
	uint32_t apb2rstr;
	uint32_t apb1rstr;
	uint32_t ahbenr;
	uint32_t apb2enr;
	uint32_t apb1enr;
	uint32_t bdcr;
	uint32_t csr;
	uint32_t ahbrstr;
	uint32_t cfgr2;
	uint32_t cfgr3;
	uint32_t cr2;
} DBG_HubRcc_t;

typedef struct DBG_HubResetTag {
	uint32_t raw_rcc_csr;
	uint8_t pin;
	uint8_t por;
	uint8_t software;
	uint8_t iwdg;
	uint8_t wwdg;
	uint8_t low_power;
	uint8_t option_byte;
	uint8_t backup_domain;
} DBG_HubReset_t;

typedef struct DBG_HubNvicTag {
	uint32_t iser[2];
	uint32_t ispr[2];
	uint32_t iabr[2];
	uint32_t irq_enter_count[DBG_HUB_IRQ_SLOTS];
	uint32_t irq_last_tick_10ms[DBG_HUB_IRQ_SLOTS];
	uint16_t last_irq_id;
	uint16_t last_vectactive;
	uint8_t current_phase;
	uint8_t last_phase;
	uint8_t event_count;
	uint8_t reserved0;
} DBG_HubNvic_t;

typedef struct DBG_HubExtiTag {
	uint32_t imr;
	uint32_t emr;
	uint32_t rtsr;
	uint32_t ftsr;
	uint32_t pr;
	uint32_t line_trigger_count[DBG_HUB_EXTI_LINES];
	uint32_t spurious_count;
} DBG_HubExti_t;

typedef struct DBG_HubGpioPortTag {
	uint32_t crl;
	uint32_t crh;
	uint32_t moder;
	uint32_t otyper;
	uint32_t ospeedr;
	uint32_t pupdr;
	uint32_t idr;
	uint32_t odr;
	uint32_t afr[2];
	uint32_t lckr;
} DBG_HubGpioPort_t;

typedef struct DBG_HubGpioNamedTag {
	uint8_t key_raw;
	uint8_t key_pressed;
	uint8_t charger_det_raw;
	uint8_t charger_connected;
	uint8_t load_det_raw;
	uint8_t load_present;
	uint8_t afe_alert_raw;
	uint8_t afe_alert_valid;
	uint8_t chg_mos;
	uint8_t dsg_mos;
	uint8_t pre_mos;
	uint8_t rs485_dir;
	uint8_t mcc_c_gpio;
	uint8_t cmnt_en_gpio;
	uint8_t dc_en_gpio;
	uint8_t afe_ctl_gpio;
} DBG_HubGpioNamed_t;

typedef struct DBG_HubGpioTag {
	DBG_HubGpioPort_t porta;
	DBG_HubGpioPort_t portb;
	DBG_HubGpioPort_t portc;
	DBG_HubGpioPort_t portd;
	DBG_HubGpioNamed_t named;
} DBG_HubGpio_t;

typedef struct DBG_HubUsartTag {
	uint32_t sr;
	uint32_t brr;
	uint32_t cr1;
	uint32_t cr2;
	uint32_t cr3;
	uint8_t rxne;
	uint8_t txe;
	uint8_t tc;
	uint8_t idle;
	uint8_t ore;
	uint8_t fe;
	uint8_t ne;
	uint8_t pe;
	uint32_t irq_count;
	uint32_t rx_count;
	uint32_t tx_count;
	uint32_t idle_count;
	uint32_t ore_count;
	uint32_t fe_count;
	uint32_t ne_count;
	uint32_t pe_count;
	uint32_t last_event_tick_10ms;
} DBG_HubUsart_t;

typedef struct DBG_HubDmaChannelTag {
	uint32_t ccr;
	uint32_t cndtr;
	uint32_t cpar;
	uint32_t cmar;
} DBG_HubDmaChannel_t;

typedef struct DBG_HubDmaTag {
	uint32_t isr;
	DBG_HubDmaChannel_t ch[DBG_HUB_DMA1_CHANNELS];
	uint8_t adc_dma_channel;
	uint8_t uart_rx_dma_channel;
	uint8_t uart_tx_dma_channel;
	uint8_t reserved0;
} DBG_HubDma_t;

typedef struct DBG_HubAdcTag {
	uint32_t sr;
	uint32_t cr1;
	uint32_t cr2;
	uint32_t smpr1;
	uint32_t smpr2;
	uint32_t sqr1;
	uint32_t sqr2;
	uint32_t sqr3;
	uint32_t sample_count;
	uint32_t last_sample_tick_10ms;
	uint16_t raw[DBG_HUB_ADC_KEYS];
	int32_t value[DBG_HUB_ADC_KEYS];
	uint32_t vbat_mv;
	uint16_t typec_current_ma;
	uint8_t ready;
	uint8_t dr_read_skipped;
} DBG_HubAdc_t;

typedef struct DBG_HubI2cTag {
	uint32_t cr1;
	uint32_t cr2;
	uint32_t sr1;
	uint32_t sr2;
	uint32_t ccr;
	uint32_t trise;
	uint8_t state;
	uint8_t busy;
	uint8_t sr_read_skipped;
	uint8_t recovery_count;
	uint32_t ack_fail_count;
	uint32_t busy_timeout_count;
	uint32_t clock_stretch_timeout_count;
	uint32_t last_event_tick_10ms;
} DBG_HubI2c_t;

typedef struct DBG_HubCanFrameTag {
	uint32_t id;
	uint8_t ide;
	uint8_t rtr;
	uint8_t dlc;
	uint8_t reserved0;
	uint8_t data[8];
	uint32_t tick_10ms;
} DBG_HubCanFrame_t;

typedef struct DBG_HubCanTag {
	uint32_t mcr;
	uint32_t msr;
	uint32_t tsr;
	uint32_t rf0r;
	uint32_t rf1r;
	uint32_t ier;
	uint32_t esr;
	uint32_t btr;
	uint8_t bus_off;
	uint8_t error_passive;
	uint8_t error_warning;
	uint8_t lec;
	uint8_t tec;
	uint8_t rec;
	uint8_t fifo0_pending;
	uint8_t fifo1_pending;
	uint32_t rx_count;
	uint32_t tx_count;
	DBG_HubCanFrame_t last_rx;
	DBG_HubCanFrame_t last_tx;
} DBG_HubCan_t;

typedef struct DBG_HubRtcTag {
	uint32_t crh;
	uint32_t crl;
	uint32_t prlh;
	uint32_t prll;
	uint32_t divh;
	uint32_t divl;
	uint32_t cnth;
	uint32_t cntl;
	uint32_t alrh;
	uint32_t alrl;
	uint32_t tr;
	uint32_t dr;
	uint32_t cr;
	uint32_t isr;
	uint32_t prer;
	uint32_t wutr;
	uint32_t counter;
	uint8_t running;
	uint8_t wake_flag;
	uint8_t alarm_flag;
	uint8_t sync_ready;
} DBG_HubRtc_t;

typedef struct DBG_HubPwrTag {
	uint32_t cr;
	uint32_t csr;
	uint8_t dbp;
	uint8_t wuf;
	uint8_t sbf;
	uint8_t regulator_stop_ready;
} DBG_HubPwr_t;

typedef struct DBG_HubBkpTag {
	uint16_t raw[DBG_HUB_BKP_WORDS];
	uint16_t magic;
	uint16_t boot_flag;
	uint16_t boot_flag_inv;
	uint16_t sleep_flag;
	uint16_t fault_flag;
	uint16_t fault_flag_inv;
	uint16_t last_wakeup_source;
	uint8_t boot_flag_valid;
	uint8_t fault_flag_valid;
} DBG_HubBkp_t;

typedef struct DBG_HubIwdgTag {
	uint32_t pr;
	uint32_t rlr;
	uint32_t sr;
	uint32_t feed_count;
	uint32_t last_feed_tick_10ms;
	uint32_t last_feed_gap_10ms;
	uint32_t max_feed_gap_10ms;
	uint8_t last_feed_source;
	uint8_t reset_flag;
	uint8_t enabled_by_config;
	uint8_t reserved0;
} DBG_HubIwdg_t;

typedef struct DBG_HubMcuTag {
	DBG_HubCore_t core;
	DBG_HubSystick_t systick;
	DBG_HubClock_t clock;
	DBG_HubRcc_t rcc;
	DBG_HubReset_t reset;
	DBG_HubNvic_t nvic;
	DBG_HubExti_t exti;
	DBG_HubGpio_t gpio;
	DBG_HubUsart_t usart1;
	DBG_HubUsart_t usart2;
	DBG_HubDma_t dma;
	DBG_HubAdc_t adc;
	DBG_HubI2c_t i2c1;
	DBG_HubCan_t can;
	DBG_HubRtc_t rtc;
	DBG_HubPwr_t pwr;
	DBG_HubBkp_t bkp;
	DBG_HubIwdg_t iwdg;
} DBG_HubMcu_t;

typedef struct DBG_HubBmsTag {
	uint32_t system_status;
	uint32_t feature_mask;
	uint32_t sys_error_word0;
	uint32_t sys_error_word1;
	uint32_t main_cycle;
	uint16_t series_num;
	uint16_t pack_mv_x100;
	uint16_t cell_min_mv;
	uint16_t cell_max_mv;
	uint16_t cell_delta_mv;
	uint16_t temp_max;
	uint16_t temp_min;
	uint16_t ichg_a10;
	uint16_t idsg_a10;
	uint8_t adc_ready;
	uint8_t afe_ready;
	uint8_t flash_busy;
	uint8_t aging_state;
	uint32_t aging_remaining_sec;
} DBG_HubBms_t;

typedef struct DBG_HubProtectTag {
	uint16_t fault_first;
	uint16_t fault_second;
	uint16_t fault_third;
	uint16_t mdl_fault_first;
	uint16_t mdl_fault_second;
	uint16_t mdl_fault_third;
	uint16_t cov_first_mv;
	uint16_t cuv_first_mv;
	uint16_t bov_first_v10;
	uint16_t buv_first_v10;
	uint16_t ichg_ocp_first_a10;
	uint16_t idsg_ocp_first_a10;
	uint16_t tchg_otp_first;
	uint16_t tdsg_otp_first;
	uint16_t tmos_otp_first;
	uint16_t vdelta_ovp_first_mv;
} DBG_HubProtect_t;

typedef struct DBG_HubMosTag {
	uint8_t sw_chg;
	uint8_t sw_dsg;
	uint8_t afe_chg_fet;
	uint8_t afe_dsg_fet;
	uint8_t afe_pchg_fet;
	uint8_t mtp_chgmos;
	uint8_t mtp_dsgmos;
	uint8_t mtp_pchmos;
	uint8_t mcc_c_gpio;
	uint8_t rf_en_gpio;
	uint8_t dc_en_gpio;
	uint8_t boost_en_gpio;
} DBG_HubMos_t;

typedef struct DBG_HubSocTag {
	uint16_t soc_pct;
	uint16_t soh_pct;
	uint16_t cap_now_ah100;
	uint16_t cap_full_ah100;
	uint16_t cap_factory_ah100;
	uint16_t cycle_times;
	uint16_t vcell_min_mv;
	uint16_t vcell_max_mv;
	uint16_t vcell_total_v100;
	uint16_t ichg_a10;
	uint16_t idsg_a10;
	uint16_t typec_current_ma;
	uint32_t vbat_mv;
} DBG_HubSoc_t;

typedef struct DBG_HubAfeTag {
	uint8_t bstatus1;
	uint8_t bstatus2;
	uint8_t bstatus3;
	uint8_t mtp_conf;
	uint16_t current_raw;
	uint16_t cell_min_mv;
	uint16_t cell_max_mv;
	uint16_t pec_error_count;
	uint32_t afe_seq;
	uint8_t current_zero_state;
	uint8_t current_zero_ready;
	uint8_t startup_cold_boot;
	uint8_t reserved0;
} DBG_HubAfe_t;

typedef struct DBG_HubCommTag {
	uint32_t can_rx_count;
	uint32_t can_tx_count;
	uint32_t sci1_irq_count;
	uint32_t sci2_irq_count;
	uint32_t sci3_irq_count;
	uint16_t sci1_error_count;
	uint16_t sci2_error_count;
	uint16_t sci3_error_count;
	uint8_t flash_update_flag;
	uint8_t flash_update_e2prom;
	uint8_t can_busy;
	uint8_t can_bus_off;
} DBG_HubComm_t;

typedef struct DBG_HubSleepTag {
	uint8_t mode;
	uint8_t rtc_wake;
	uint8_t boot_from_sleep;
	uint8_t boot_charger_wake;
	uint32_t block_reason;
	uint32_t idle_count;
	uint32_t sleep_seconds;
	uint32_t last_sleep_seconds;
	uint32_t cycles;
	uint32_t rtc_sleep_count;
	uint8_t last_wakeup_source;
	uint8_t charger_active;
	uint8_t key_active;
	uint8_t load_active;
} DBG_HubSleep_t;

typedef struct DBG_HubFaultTag {
	uint16_t cortex_fault_reason;
	uint16_t cortex_fault_reason_inv;
	uint8_t cortex_fault_valid;
	uint8_t hardfault_count;
	uint8_t memfault_count;
	uint8_t busfault_count;
	uint8_t usagefault_count;
	uint32_t scb_cfsr;
	uint32_t scb_hfsr;
	uint32_t scb_dfsr;
	uint32_t scb_afsr;
	uint16_t system_error_flags[12];
} DBG_HubFault_t;

typedef struct DBG_HubTag {
	DBG_HubCtrl_t ctrl;
	DBG_HubMcu_t mcu;
	DBG_HubBms_t bms;
	DBG_HubProtect_t protect;
	DBG_HubMos_t mos;
	DBG_HubSoc_t soc;
	DBG_HubAfe_t afe;
	DBG_HubComm_t comm;
	DBG_HubSleep_t sleep;
	DBG_HubFault_t fault;
} DBG_Hub_t;

extern volatile DBG_Hub_t g_dbg;

void DBG_Init(void);
void DBG_Task(void);
void DBG_CaptureMcuFast(void);
void DBG_CaptureMcuSlow(void);
void DBG_CaptureMcuAll(void);
void DBG_CaptureClock(void);
void DBG_CaptureRcc(void);
void DBG_CaptureGpio(void);
void DBG_CaptureNvic(void);
void DBG_CaptureExti(void);
void DBG_CaptureRtcBkp(void);
void DBG_ClearCounters(void);

void DBG_RecordUsartRx(uint8_t port);
void DBG_RecordUsartTx(uint8_t port);
void DBG_RecordUsartIdle(uint8_t port);
void DBG_RecordUsartError(uint8_t port, uint16_t sr);
void DBG_RecordCanRxFrame(uint32_t id, uint8_t ide, uint8_t rtr, uint8_t dlc, const uint8_t data[8]);
void DBG_RecordCanTxFrame(uint32_t id, uint8_t ide, uint8_t rtr, uint8_t dlc, const uint8_t data[8]);
void DBG_RecordAdcSample(void);
void DBG_RecordIwdgFeed(uint8_t source);
void DBG_RecordI2c1Status(uint16_t sr1, uint16_t sr2, uint8_t state, uint8_t event);

#endif /* DEBUG_HUB_H */
