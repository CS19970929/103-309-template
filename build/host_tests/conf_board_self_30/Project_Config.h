#ifndef PROJECT_CONFIG_H
#define PROJECT_CONFIG_H

/* Keil uVision: right click this file and select "Configuration Wizard". */

// <<< Use Configuration Wizard in Context Menu >>>

// <h>Ŀӻ

// <h>λ
// <o> λ
//   <0=>  Release
//   <1=>  Debug
//   <2=> / Factory/Test
// <i> ;ѡʹ ReleaseSOC עԱʹ Factory/Test
#ifndef PROJECT_CFG_BUILD_PROFILE
#define PROJECT_CFG_BUILD_PROFILE 0
#endif
// </h>

// <h>Ʒ
// <o> EEPROM ʼ <0x0000-0xFFFF>
// <i> Change this value only when field parameters must be reinitialized.
// <i> EEPROM ʼǡֻҪǿֳʱ޸ġ
#define PROJECT_CFG_EEPROM_VALUE_BEGIN_FLAG 0x2445

// <o> ذ
//   <0=>  BAT_MASTER20A
//   <1=> Ӱ BAT_SLAVE40A
// <i> ذ͡ӰĬϵȼЭݺͲֲƷá
#define PROJECT_CFG_BAT_TYPE 1

// <o> оϵ
//   <0=> Ԫ TERNARYLI
//   <1=>  LIFEPO
// <i> оϵӰѹƽ̨SOC ͱѡ
#define PROJECT_CFG_BAT_CHEMISTRY 1

// <q> Allow host runtime SOC table writes
// <i> Default disabled. When disabled, SOC table is selected by PROJECT_CFG_BAT_CHEMISTRY at compile time; 0x2200 writes are rejected.
#define PROJECT_CFG_SOC_RUNTIME_TABLE_ENABLE 0

// <q> Enable host write registers
// <i> Default enabled. PC UI writes protection and other parameters through comm tool/CAN; board still checks address and range.
#define PROJECT_CFG_HOST_WRITE_ENABLE 1

// <o> ̼ <0-99>
// <i> ̼ݣλʾ
#define PROJECT_CFG_FD_YEAR 26

// <o> ̼· <1-12>
// <i> ̼·ݣڰ汾Ϣϱ
#define PROJECT_CFG_FD_MONTH 5

// <o> ̼ <1-31>
// <i> ̼ڣڰ汾Ϣϱ
#define PROJECT_CFG_FD_DAY 9

// <o> ̼汾 <0-65535>
// <i> ̼汾ţλԺֳʶ
#define PROJECT_CFG_VERSION 5

// <o> λ
//   <0=> 80A  CURR_80A
//   <1=> 100A  CURR_100A
//   <2=> 150A  CURR_150A
//   <3=> 200A  CURR_200A
//   <4=> 250A  CURR_250A
// <i> λӰ챣ʾá
#define PROJECT_CFG_LEVEL_CURR 2

// <o> AFE 
//   <0=> bq76xx AFE
//   <1=> sh36xx AFE
// <i> AFE оƬ͡Ӳʵʺͺһ¡
#define PROJECT_CFG_AFE_TYPE 1
// </h>

// <h>ܿ
// <q> ʹ IWDG Ź
// <i> ʹܶŹ鿪ֹܷɺӦ
#define PROJECT_CFG_WDOG_ENABLE 1

// <q> ʹ RTC 
// <i> ʹ RTCڵ͹ļʱ߻Ѻ SOC 
#define PROJECT_CFG_RTC_ENABLE 1

// <q> ʹܸƳ·ָ
// <i> ʹܸƳ·ָ߼ƷԾ
#define PROJECT_CFG_LOAD_REMOVE_SHORT_ENABLE 0

// <q> ʹ UART1 
// <i>  UART1 ϵͳλ UART1 ʱ
#define PROJECT_CFG_UART1_WAKEUP_ENABLE 1

// <q> ʹ UART2 
// <i>  UART2 ϵͳδʹ UART2 ʱֹرա
#define PROJECT_CFG_UART2_WAKEUP_ENABLE 0

// <q> ʹ RS485 
// <i>  RS485 ͨŻϵͳҪߺλʱ
#define PROJECT_CFG_RS485_WAKEUP_ENABLE 1

// <q> ʹܵڶ·
// <i> ʹܵڶ·ֻӲͲ֧ʱ
#define PROJECT_CFG_SECOND_CURR_PROTECT_ENABLE 0

// <q> ʹ
// <i> ʹֹµĵƫ
#define PROJECT_CFG_VIRTUAL_CURRENT_ENABLE 1

// <q> ʹ DI ϵͳ
// <i> ʹ DI ϵͳ롣Ҫⲿϵͳʱ
#define PROJECT_CFG_DI_SWITCH_SYS_ONOFF_ENABLE 0

// <q> ʹ DI ŵ翪
// <i> ʹ DI ŵ翪롣ҪⲿƷŵʱ
#define PROJECT_CFG_DI_SWITCH_DSG_ONOFF_ENABLE 0

// <q> ʹ DI 
// <i> ʹ DI 롣ڰػ
#define PROJECT_CFG_DI_SWITCH_LONGKEY_ONOFF_ENABLE 1

// <q> ʹܾ LED ܱ
// <i> ʹܾ LED ܱǡ LedBar ͨ
#define PROJECT_CFG_LED_FUNC_ENABLE 0

// <q> ʹܵԴ
// <i> ʹܵԴ롣رա
#define PROJECT_CFG_DEBUG_CODE_ENABLE 0

// <q> Enable Flash boot diagnostic print
// <i> Debug/Factory only. Release keeps this disabled to avoid pulling printf into the image.
#ifndef PROJECT_CFG_FLASH_BOOT_PRINT_ENABLE
#define PROJECT_CFG_FLASH_BOOT_PRINT_ENABLE 0
#endif

// <q> Export Keil Watch debug observation entries
// <i> Debug only. Release build must keep this disabled. Exports g_dbg_* watch pointers.
#ifndef PROJECT_CFG_DEBUG_WATCH_ENABLE
#define PROJECT_CFG_DEBUG_WATCH_ENABLE 0
#endif

// <q> 
// <i> ߡֹͨرգߡ
#define PROJECT_CFG_SLEEP_WITH_CURRENT_ENABLE 0

// <q> ʹ IAP ת֧
// <i> ʹ IAP ת֧֡ bootloader  App 뿪
#define PROJECT_CFG_IAP_ENABLE 1

// <q> ʹܳϻģʽ
// <i> ״γڼ򿪳ŵܣɺд Flash ־Զϻ
#define PROJECT_CFG_FACTORY_AGING_ENABLE 1

// <o> ϻʱ <1-604800>
// <i> Ĭ 259200 룬 3 졣ֻۼ MCU ̬ TIM3 tickRTC/STOP ʱ䲻롣
#define PROJECT_CFG_FACTORY_AGING_DURATION_SECONDS 259200
// </h>

// <h>ڽɫ
// <o> SCI1 ɫ
//   <0=> 
//   <1=> ͨλ
//   <2=> ͻ
//   <3=> LCD
// <i> SCI1 ͨŽɫǰͨλЭ顣
#define PROJECT_CFG_SCI1_ROLE 1

// <o> SCI2 ɫ
//   <0=> 
//   <1=> ͨλ
//   <2=> ͻ
//   <3=> LCD
// <i> SCI2 ͨŽɫδʱ Disabled
#define PROJECT_CFG_SCI2_ROLE 0

// <o> SCI3 ɫ
//   <0=> 
//   <1=> ͨλ
//   <2=> ͻ
//   <3=> LCD
// <i> SCI3 ͨŽɫδʱ Disabled
#define PROJECT_CFG_SCI3_ROLE 0
// </h>

// <h>Flash 64K 洢
// <q> ʹƻԿٲ
// <i> ʹ Flash  64K ƻԿٲԡֻʵ̼
#define PROJECT_CFG_FLASH64K_QUICK_TEST_ENABLE 0

// <o> ٲѭ <1-65535>
// <i> ƻԿٲѭģʽʹá
#define PROJECT_CFG_FLASH64K_QUICK_TEST_CYCLES 96

// <q> ʹ App д洢
// <i> ʹ App ڼ洢ԡ֤ SOC/AFE 洢ȶԡ
#define PROJECT_CFG_FLASH64K_USE_TEST_ENABLE 0

// <o> App Դӡڣ <1-65535>
// <i> App д洢־ӡڣλ롣
#define PROJECT_CFG_FLASH64K_USE_TEST_PRINT_PERIOD_SEC 10

// <q> ʹ App Լ
// <i> ʹд洢Լ١ֻ̨֤ʱ䡣
#define PROJECT_CFG_FLASH64K_USE_TEST_ACCEL_ENABLE 0

// <o>  SOC ڣ <1-65535>
// <i> ٲʱ SOC ձڣλ롣
#define PROJECT_CFG_FLASH64K_USE_TEST_ACCEL_SOC_PERIOD_SEC 1

// <o>  AFE ڣ <1-65535>
// <i> ٲʱ AFE ڣλ롣
#define PROJECT_CFG_FLASH64K_USE_TEST_ACCEL_AFE_PERIOD_SEC 30
// </h>

// <h>Flash ־ĥ𱣻
// <o> ͬ¼ظС <0-86400>
// <i> ƹ϶µ¼־д FlashĬ 3600 룻0 ʾرƵ˯¼ޡ
#define PROJECT_CFG_LOG_RECORD_REPEAT_MIN_INTERVAL_SEC 3600
// </h>

// <h>SOC У׼
// <o> ȷСѹmV <0-500>
// <i> ͨȷѹԽԽ׵ 100%
#define PROJECT_CFG_SOC_FULL_CONFIRM_MIN_CELL_MARGIN_MV 80

// <o> ȷѹmV <0-1000>
// <i> ȷѹԽСԽء
#define PROJECT_CFG_SOC_FULL_CONFIRM_MAX_CELL_DELTA_MV 120

// <o> ͨȷʱ䣬 <1-600>
// <i> ͨҪʱ䡣ԽԽȣԽ̵ 100% Խ졣
#define PROJECT_CFG_SOC_FULL_CONFIRM_SECONDS 15

// <o> ȷʱ䣬 <1-600>
// <i> Ҫʱ䡣벻ͨȷʱ䡣
#define PROJECT_CFG_SOC_FULL_CONFIRM_FAST_SECONDS 5

// <o> ͨȷ SOCٷֱ <0-100>
// <i> ͨȷҪڲ SOCڱе͵ 100%
#define PROJECT_CFG_SOC_FULL_CONFIRM_MIN_SOC_PERCENT 95

// <o> ȷϵѹmV <0-500>
// <i> ȷѹԽԽ׿ٵ 100%
#define PROJECT_CFG_SOC_FULL_CONFIRM_FAST_MARGIN_MV 30

// <o> SOC calibration min valid cell voltage, mV <1000-3500>
#define PROJECT_CFG_SOC_CALIBRATION_MIN_CELL_VALID_MV 2000

// <o> У׼ЧѹmV <3600-6000>
// <i> SOC У׼ʹõߵѹڸֵΪ쳣
#define PROJECT_CFG_SOC_CALIBRATION_MAX_CELL_VALID_MV 5000

// <o> У׼ѹmV <0-3000>
// <i> SOC У׼ѹԽСԽܹ쳣
#define PROJECT_CFG_SOC_CALIBRATION_MAX_CELL_DELTA_MV 1000

// <q> ʱֹ SOC У׼
// <i> ʱǷֹ SOC ԶУ׼ء
#define PROJECT_CFG_SOC_CALIBRATION_BLOCK_PROTECTION_FAULT 0

// <q> ϵͳʱֹ SOC У׼
// <i> AFE/ADC//¶ȵϵͳ쳣ʱǷֹ SOC ԶУ׼
#define PROJECT_CFG_SOC_CALIBRATION_BLOCK_SYSTEM_FAULT 0

// <o> SOC sag holdoff seconds <0-1800>
#define PROJECT_CFG_SOC_SAG_HOLDOFF_SECONDS 30

// <o> ѹӳУ׼mV <0-500>
// <i> ѹӳڼ䣬 V0 ӸĩУ׼
#define PROJECT_CFG_SOC_SAG_ALLOW_OFFSET_MV 50

// <o>  OCV ȴʱ䣬 <60-43200>
// <i> öú OCV С SOCԪͨ 30 
#define PROJECT_CFG_SOC_REST_OCV_SECONDS 1800

// <o> ȶ̿ʱ䣬 <60-7200>
// <i> ѹȶٶú¼ OCV deferred targetСȡ
#define PROJECT_CFG_SOC_REST_STABLE_MIN_SECONDS 300

// <o> ȶ/ŵ OCV ֵģ <60-7200>
// <i> ﵽȶںÿˢ» 1% OCV 
#define PROJECT_CFG_SOC_REST_TARGET_STEP_SECONDS 600

// <o> õ OCV ޽ģ <60-43200>
// <i> ʱ侲 OCV Ŀڲ SOC ʱÿ 1%
#define PROJECT_CFG_SOC_REST_DOWN_STEP_SECONDS 1800

// <o> SOC auto calibration max step percent <1-10>
#define PROJECT_CFG_SOC_CALIBRATION_STEP_PERCENT 1

// <o> ԺĵmA <0-1000>
// <i> SOC ʱ۳ BMS ĵ硣Ĭ 30mAʵ޸ġ
#define PROJECT_CFG_SOC_BOARD_SELF_CONSUMPTION_MA 30

// <o> ѹβУ׼ʼƫƣmV <0-1000>
// <i> VCellMin <= V0 + ƫʱѹβαСɼ 20% 졣
#define PROJECT_CFG_SOC_EMPTY_TAIL_START_OFFSET_MV 400

// <o> ѹβĿ̧߰ٷֱ <0-30>
// <i> ֻ V0 ϵβαõ͵ʾأV0 ǿ 0 
#define PROJECT_CFG_SOC_EMPTY_TAIL_SOFT_TARGET_LIFT_PERCENT 0

// <o> ѹβٶȱٷֱ <25-400>
// <i> ֻ V0 ϵβα100 ΪĬϣС졣
#define PROJECT_CFG_SOC_EMPTY_TAIL_SOFT_TICK_SCALE_PERCENT 100

// <o> ͨʾٶȣ/1% <1-60>
#define PROJECT_CFG_SOC_DISPLAY_NORMAL_SECONDS 5

// <o> ʾٶȣ/1% <1-60>
#define PROJECT_CFG_SOC_DISPLAY_CHG_SECONDS 5

// <o> ѹʾ½ٶȣ/1% <1-60>
#define PROJECT_CFG_SOC_DISPLAY_LOW_SECONDS 1

// <o> ѹʾٱ߽磬mV <0-500>
// <i> VCellMin <= V0 + ƫʱʾ½ʹõѹٶȡ
#define PROJECT_CFG_SOC_DISPLAY_LOW_OFFSET_MV 50

// <o> յʾ߽磬 V0  mV <0-500>
// <i> VCellMin <= V0 - ֵʱʾÿ 200ms tick ½ 1%
#define PROJECT_CFG_SOC_DISPLAY_EMPTY_FAST_BELOW_V0_MV 50
// </h>

// <h>SOC ģʽ
// <q> ʹ SOC עģʽ
// <i> SOC עʽڡ뱣ֹرգֻ Factory/Test ̼
#ifndef PROJECT_CFG_SOC_TEST_MODE_ENABLE
#define PROJECT_CFG_SOC_TEST_MODE_ENABLE 0
#endif

// <o> SOC ע tick  <1-1000>
// <i>  0x2500 עģٸ 200ms tickƲԼǿȡ
#ifndef PROJECT_CFG_SOC_TEST_ACCEL_TICKS_MAX
#define PROJECT_CFG_SOC_TEST_ACCEL_TICKS_MAX 300
#endif
// </h>

// <h>
// <q> ʹ GPIO Charlieplexing 
// <i> ʹ GPIO Charlieplexing Ӳӷʽһ¡

// <q> ʹܵ SOC 
// <i> ǰݵ SOC ʾ״̬Ѻڻָʾ顣
#define PROJECT_CFG_LEDBAR_SLEEP_ENABLE 1

// <q> ʹܳ GPIO ת
// <i>  GPIO תԿءӲ/ʱ
#define PROJECT_CFG_LEDBAR_LONG_PRESS_GPIO_TOGGLE_TEST 0

// <q> LedBar test always-on display
// <i> Keeps SOC display on during Debug/Factory testing. Release build must keep this disabled.
#ifndef PROJECT_CFG_LEDBAR_TEST_ALWAYS_ON
#define PROJECT_CFG_LEDBAR_TEST_ALWAYS_ON 0
#endif

// <o> SOC ʾʱ䣬10ms tick <1-65535>
// <i> SOC ʾʱ䣬λ 10ms500 ʾ 5 롣
#define PROJECT_CFG_LEDBAR_SOC_DISPLAY_10MS 500

// <o> Sleep wakeup SOC display time, 10ms tick <1-65535>
// <i> After MCU reset from sleep wakeup, keep SOC display for 10s by default.
#define PROJECT_CFG_LEDBAR_WAKEUP_DISPLAY_10MS 1000

// <o> LedBar scan timer period, 100kHz tick <1-1000>
// <i> GPIO Charlieplexing only. 50 means 0.5ms per route.
#define PROJECT_CFG_LEDBAR_SCAN_TIMER_100KHZ_TICKS 50

// <o> MCU_WK Ч˲10ms tick <0-255>
// <i> MCU_WK Ч˲ʱ䣬λ 10ms
#define PROJECT_CFG_LEDBAR_MCU_WK_ON_FILTER_10MS 3

// <o> MCU_WK Ч˲10ms tick <0-255>
// <i> MCU_WK Ч˲ʱ䣬λ 10ms
#define PROJECT_CFG_LEDBAR_MCU_WK_OFF_FILTER_10MS 3

// <o> ˲100ms tick <0-255>
// <i> Ч˲ʱ䣬λ 100ms
#define PROJECT_CFG_LEDBAR_CHARGE_ON_FILTER_100MS 3

// <o> Ͽ˲100ms tick <0-255>
// <i> γЧ˲ʱ䣬λ 100ms
#define PROJECT_CFG_LEDBAR_CHARGE_OFF_FILTER_100MS 5
// </h>

// <h>
// <q> ʹ
// <i> ʹԡֻҪֳ̼ʱ
#define PROJECT_CFG_UPGRADE_PARAM_POLICY_ENABLE 1

// <o> ԰汾 <0x0000-0xFFFE>
// <i> ԰汾ݱ仯ʱظ©ִС
#define PROJECT_CFG_UPGRADE_PARAM_POLICY_VERSION 0x0003

// <q>  AFE 
// <i> ʱ AFE ֻ AFE Ĭϲ븲ֳֵʱ
#define PROJECT_CFG_UPGRADE_PARAM_RESET_AFE 1

// <q> ñ
// <i> ʱñӰ챣ֵ
#define PROJECT_CFG_UPGRADE_PARAM_RESET_PROTECT 0

// <q> þ⿪ѹ
// <i> ʱþ⿪ѹֳɰ汾
#define PROJECT_CFG_UPGRADE_PARAM_RESET_BALANCE_OPEN_VOLTAGE 0

// <q>  SOC 
// <i> ʱ SOC OCV о߻Ĭϱ仯ʱ
#define PROJECT_CFG_UPGRADE_PARAM_RESET_SOC_TABLE 0

// <q>  SOC 
// <i> ʱ SOC ѹêáӰûɼ
#define PROJECT_CFG_UPGRADE_PARAM_RESET_SOC_CONFIG 0

// <q>  SOC 
// <i> ʱ SOC ա״° OCV  SOC
#define PROJECT_CFG_UPGRADE_PARAM_RESET_SOC_SNAPSHOT 0

// <q> ¼¼
// <i> ʱ¼¼ۺҪʷʱرա
#define PROJECT_CFG_UPGRADE_PARAM_RESET_EVENT_RECORD 0

// <q> ǿظִвԣ
// <i> ǿظִԡֻڲԣرա
#define PROJECT_CFG_UPGRADE_PARAM_FORCE_REAPPLY 0
// </h>

// </h>

// <<< end of configuration section >>>

#endif
