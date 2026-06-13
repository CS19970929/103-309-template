/**
 * @file SocConfig.h
 * @brief SOC模块配置参数集中定义
 * 
 * 所有SOC相关的配置参数统一在此文件管理，
 * 方便维护和修改，避免参数分散在多个文件中。
 */

#ifndef SOC_CONFIG_H
#define SOC_CONFIG_H

#include "Project_Config.h"

/*==========================================================================
 * 一、积分参数
 *==========================================================================*/

/** @brief 积分周期(ms)，每200ms执行一次库仑计积分 */
#define SOC_CFG_TICK_MS                     ((UINT32)200U)

/** @brief 每秒的积分tick数 = 1000ms / 200ms = 5 */
#define SOC_CFG_TICKS_PER_SECOND            ((UINT16)5U)

/** @brief 电流活跃阈值(A×10)，低于此值视为静置 */
#define SOC_CFG_CURRENT_ACTIVE_A10          ((UINT16)2U)

/** @brief mA到A×10的换算系数 */
#define SOC_CFG_MA_PER_A10                  ((int32_t)100)

/** @brief mA·ms到A×10·s的换算系数 */
#define SOC_CFG_MAMS_PER_AS10               ((UINT32)100000U)

/** @brief 板级自耗电(mA)，积分时减去此值 */
#define SOC_CFG_BOARD_SELF_CONSUMPTION_MA   ((UINT16)PROJECT_CFG_SOC_BOARD_SELF_CONSUMPTION_MA)

/*==========================================================================
 * 二、容量与SOH参数
 *==========================================================================*/

/** @brief SOH最低值(%)，低于此值不再衰减 */
#define SOC_CFG_SOH_MIN                     ((UINT8)80U)

/** @brief SOH衰减步长(循环次数)，每100个循环SOH-1% */
#define SOC_CFG_SOH_CYCLE_STEP              ((UINT16)100U)

/*==========================================================================
 * 三、满充校准参数
 *==========================================================================*/

/** @brief 满充校准持续时间(秒) */
#define SOC_CFG_FULL_SECONDS                ((UINT16)PROJECT_CFG_SOC_FULL_CONFIRM_SECONDS)

/** @brief 满充校准最低VCellMax(mV) */
#define SOC_CFG_FULL_CONFIRM_MIN_VMAX_MV    ((UINT16)4180U)

/** @brief 满充校准最低VCellMin裕量(mV) */
#define SOC_CFG_FULL_MIN_MARGIN_MV          ((UINT16)PROJECT_CFG_SOC_FULL_CONFIRM_MIN_CELL_MARGIN_MV)

/** @brief 满充校准最大压差(mV) */
#define SOC_CFG_FULL_MAX_DELTA_MV           ((UINT16)PROJECT_CFG_SOC_FULL_CONFIRM_MAX_CELL_DELTA_MV)

/*==========================================================================
 * 四、低电量尾部修正参数
 *==========================================================================*/

/** @brief 轻载电流分档系数 */
#define SOC_CFG_EMPTY_CUR_LIGHT_DIVIDER     ((UINT16)5U)

/** @brief 中载电流分档系数 */
#define SOC_CFG_EMPTY_CUR_MID_DIVIDER       ((UINT16)2U)

/** @brief 低电量尾部修正起始偏移(mV) */
#define SOC_CFG_EMPTY_TAIL_START_OFFSET_MV  ((UINT16)PROJECT_CFG_SOC_EMPTY_TAIL_START_OFFSET_MV)

/*==========================================================================
 * 五、Sag Hold参数
 *==========================================================================*/

/** @brief Sag Hold保持时间(秒)，大电流放电后禁止OCV校准的等待期 */
#define SOC_CFG_SAG_HOLDOFF_SECONDS         ((UINT16)PROJECT_CFG_SOC_SAG_HOLDOFF_SECONDS)

/** @brief Sag Hold期间的电压偏移(mV)，低于此值时允许校准 */
#define SOC_CFG_SAG_ALLOW_OFFSET_MV         ((int16_t)PROJECT_CFG_SOC_SAG_ALLOW_OFFSET_MV)

/*==========================================================================
 * 六、OCV校准参数
 *==========================================================================*/

/** @brief 静置OCV校准所需秒数 */
#define SOC_CFG_REST_OCV_SECONDS            ((UINT32)PROJECT_CFG_SOC_REST_OCV_SECONDS)

/** @brief OCV步进间隔(秒) */
#define SOC_CFG_LONG_REST_DOWN_STEP_SECONDS ((UINT32)PROJECT_CFG_SOC_REST_DOWN_STEP_SECONDS)

/** @brief OCV每次步进的百分点数 */
#define SOC_CFG_CAL_STEP                    ((UINT8)PROJECT_CFG_SOC_CALIBRATION_STEP_PERCENT)

/*==========================================================================
 * 七、电压有效性参数
 *==========================================================================*/

/** @brief 电压有效最低值(mV) */
#define SOC_CFG_VALID_MIN_MV                ((UINT16)PROJECT_CFG_SOC_CALIBRATION_MIN_CELL_VALID_MV)

/** @brief 电压有效最高值(mV) */
#define SOC_CFG_VALID_MAX_MV                ((UINT16)PROJECT_CFG_SOC_CALIBRATION_MAX_CELL_VALID_MV)

/** @brief 校准允许的最大压差(mV) */
#define SOC_CFG_VALID_MAX_DELTA_MV          ((UINT16)300)

/** @brief 静置稳定性检查最大压差(mV) */
#define SOC_CFG_REST_MAX_DELTA_MV           ((UINT16)200U)

/** @brief 电压稳定性判定阈值(mV)，波动≤此值视为稳定 */
#define SOC_CFG_REST_STABLE_DELTA_MV        ((UINT16)30U)

/** @brief 开机后Sag Hold保持时间(秒) */
#define SOC_CFG_REBOUND_BOOT_HOLDOFF_SECONDS ((UINT32)300U)

/*==========================================================================
 * 八、内部常量
 *==========================================================================*/

/** @brief Sag Hold标志位 */
#define SOC_CFG_SNAPSHOT_FLAG_REBOUND_HOLD  ((UINT16)0x0001U)

#endif /* SOC_CONFIG_H */
