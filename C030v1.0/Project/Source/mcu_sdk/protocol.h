/**********************************Copyright (c)**********************************
**                       版权所有 (C), 2015-2030, 涂鸦科技
**
**                             http://www.tuya.com
**
*********************************************************************************/
/**
 * @file    protocol.h
 * @author  涂鸦综合协议开发组
 * @version v1.0.2
 * @date    2021.6.2
 * @brief
 *                       *******非常重要，一定要看哦！！！********
 *          1. 用户在此文件中实现数据下发/上报功能
 *          2. DP的ID/TYPE及数据处理函数都需要用户按照实际定义实现
 *          3. 当开始某些宏定义后需要用户实现代码的函数内部有#err提示,完成函数后请删除该#err
 */

/****************************** 免责声明 ！！！ *******************************
由于MCU类型和编译环境多种多样，所以此代码仅供参考，用户请自行把控最终代码质量，
涂鸦不对MCU功能结果负责。
******************************************************************************/

#ifndef __PROTOCOL_H_
#define __PROTOCOL_H_

#include "tuya_type.h"

/******************************************************************************
                           用户相关信息配置
******************************************************************************/

/******************************************************************************
                       1:修改产品信息
******************************************************************************/
#define PRODUCT_KEY "h2ari7fs5brq6qyk"    //开发平台创建产品后生成的16位字符产品唯一标识

#define MCU_VER "1.0.0"         //用户的软件版本,用于MCU固件升级,MCU升级版本需修改

/*  模组功耗  */
#define MODULE_POWER        "0"         //正常功耗
//#define MODULE_POWER        "1"         //低功耗
/* apn设置(可选项) */
//#define APN_VER             "cniot"

/* 心跳设置(可选项) */
//#define MHT_INTERNAL        "300"       //客户根据需要修改该值,默认心跳间隔300s

/* 短链接获取(可选项) */
//#define QR_ENABLE           "0"		//模组拉取配网二维码，该二维码会和生产工具打印的二维码冲突，谨慎使用

/* Slave 模式 */
// #define SLAVE_SET             "1"

/* DP CACHE功能 */
// #define DP_CACHE                "1"

/* 设置运营商ID */
// #define SET_COPS                    "46000"

/* DP NOTICE ACK */
// #define DP_ACK                  "1"

// #define ONLINE_TTS           "1"

/*预发环境设置*/
// #define PRE_ENV          "1"

/*GPS类型*/
// #define GPS_TYPE            "allystar"    //You should set this,if the gps type is allystar and you want to use agnss function, UFirebird needn't set this

/*网络类型*/
// #define NET_TYPE          "4"   //0:Auto 1:GSM 2:LTE 4:CATM 5:NB-IoT (only Support MA510)

/* ota protocol version 1*/
//#define MCU_OTA_PROTOCL_V1            "1"

/* 是否支持除MCU外的其他设备的OTA升级 */
#ifdef SUPPORT_EXT_OTA
//#define EX_DEVICE1_OTA_CH           "11"
//#define EX_DEVICE1_OTA_CH_VER        "0.0.2"
//#define EX_DEVICE2_OTA_CH           "12"
//#define EX_DEVICE2_OTA_CH_VER        "0.0.3"
#endif
/******************************************************************************
                          2:MCU是否需要支固件升级
如需要支持MCU固件升级,请开启该宏
MCU可调用mcu_api.c文件内的mcu_firm_update_query()函数获取当前MCU固件更新情况
                        ********WARNING!!!**********
当前接收缓冲区为关闭固件更新功能的大小,固件升级包可选择，默认256字节大小
如需要开启该功能,串口接收缓冲区会变大
******************************************************************************/
//#define         SUPPORT_MCU_FIRM_UPDATE                 //开启MCU固件升级功能(默认关闭)
/*  Firmware package size selection  */
#ifdef SUPPORT_MCU_FIRM_UPDATE
#define PACKAGE_SIZE                   0        //包大小为256字节
//#define PACKAGE_SIZE                   1        //包大小为512字节
//#define PACKAGE_SIZE                   2        //包大小为1024字节
#endif

/******************************************************************************
                         3:定义收发缓存:
                    如当前使用MCU的RAM不够,可修改为24
******************************************************************************/
//#define SUPPORT_SHORTURL_GET                1             //是否支持短链接获取功能
#ifndef SUPPORT_MCU_FIRM_UPDATE
#define CELLULAR_UART_RECV_BUF_LMT          16              //串口数据接收缓存区大小,如MCU的RAM不够,可缩小
#ifdef SUPPORT_WIFI_LOCATION
#define CELLULAR_DATA_PROCESS_LMT          700
#elif SUPPORT_SHORTURL_GET
#define CELLULAR_DATA_PROCESS_LMT          48
#else
#define CELLULAR_DATA_PROCESS_LMT           24              //串口数据处理缓存区大小,根据用户DP数据大小量定,必须大于24
#endif
#else
#define CELLULAR_UART_RECV_BUF_LMT          128             //串口数据接收缓存区大小,如MCU的RAM不够,可缩小
//请在此处选择合适的串口数据处理缓存大小（根据上面MCU固件升级包选择的大小和是否开启天气服务来选择开启多大的缓存）
#define CELLULAR_DATA_PROCESS_LMT           300             //串口数据处理缓存大小,如需MCU固件升级,若单包大小选择256,
                                                            //则缓存必须大于260,若开启天气服务,则需要更大
//#define CELLULAR_DATA_PROCESS_LMT           600           //串口数据处理缓存大小,如需MCU固件升级,若单包大小选择512,
                                                            //则缓存必须大于520,若开启天气服务,则需要更大
//#define CELLULAR_DATA_PROCESS_LMT           1200          //串口数据处理缓存大小,如需MCU固件升级,若单包大小选择1024,
                                                            //则缓存必须大于1030,若开启天气服务,则需要更大
#endif

#define CELLULAR_UART_SEND_BUF_LMT         48              //根据用户DP数据大小量定,必须大于48
#ifdef SUPPORT_WIFI_LOCATION
#define CELLULAR_UART_SEND_BUF_LMT         256
#endif
/******************************************************************************
                        4:定义模块工作方式
模块自处理:
          蜂窝指示灯和蜂窝复位按钮接在蜂窝模块上(开启CELLULAR_CONTROL_SELF_MODEE宏)
          并正确定义WF_STATE_KEY和WF_RESET_KEY
MCU自处理:
          蜂窝指示灯和蜂窝复位按钮接在MCU上(关闭CELLULAR_CONTROL_SELF_MODEE宏)
          MCU在需要处理复位蜂窝设备的地方调用mcu_api.c文件内的mcu_reset_cellular()函数,并可调用mcu_get_reset_cellular_flag()函数返回复位cellular结果
          或调用设置蜂窝设备模式mcu_api.c文件内的mcu_set_cellular_mode(CELLULAR_CONFIG_E mode)函数,并可调用mcu_get_cellular_work_state()函数返回设置cellular结果
******************************************************************************/
//#define         CELLULAR_CONTROL_SELF_MODE                       //蜂窝自处理按键及LED指示灯;
                                                                   //如为MCU外界按键/LED指示灯请关闭该宏
#ifdef          CELLULAR_CONTROL_SELF_MODE                      //模块自处理
  #define     CELLULAR_STATE_KEY            0                    //蜂窝模块状态指示灯固定为NET_MODE灯，所以这个宏配置随意
  #define     CELLULAR_RESERT_KEY           21                   //蜂窝模块重置按键，请根据实际GPIO管脚设置
#endif

/******************************************************************************
                      MCU是否需要支持校时功能
如需要请开启该宏,并在Protocol.c文件内mcu_write_rtctime实现代码
mcu_write_rtctime内部有#err提示,完成函数后请删除该#err
mcu在cellular模块正确联网后可调用mcu_get_system_time()函数发起校时功能
******************************************************************************/
//#define         SUPPORT_MCU_RTC_CHECK                //开启校时功能

/******************************************************************************
                      是否开启天气功能
如需要请开启该宏,并在protocol.c文件内weather_open_return_handle与weather_data_user_handle两个用户处理函数中实现显示等代码
此两函数内#err提示,完成函数后请删除该#err
开启天气功能，串口数据缓存区的大小要开大一些
******************************************************************************/
//#define         WEATHER_ENABLE                  //打开天气功能
#ifdef          WEATHER_ENABLE
/*  在protocol.c文件中weather_choose数组中可调整，然后将打开服务的类型数目写到此宏定义  */
#define         WEATHER_CHOOSE_CNT              4   //选择的需要天气服务类型的数目
/*  在打开天气服务时，可以设置此宏定义选择天气预报的天数，1表示当天天气(不需要预报可设置为1)，最多为7天(不可以设置成0或大于7)  */
#define         WEATHER_FORECAST_DAYS_NUM       1   //设置天气预报的天数
#endif

/******************************************************************************
                      是否开启蜂窝模组心跳关闭功能
如需要请开启该宏,调用mcu_api.c文件内cellular_heart_stop函数即可停止心跳
******************************************************************************/
//#define         CELLULAR_HEARTSTOP_ENABLE           //开启心跳停止功能

/******************************************************************************
                      MCU是否需要开启获取当前蜂窝设备联网状态功能
如需要请开启该宏,并且mcu在需要获取当前蜂窝联网状态处调用mcu_api.c文件内mcu_get_cellular_connect_status
并在protocol.c文件cellular_test_result函数内查看结果,
cellular_test_result内部有#err提示,完成函数后请删除该#err
******************************************************************************/
//#define         GET_CELLULAR_STATUS_ENABLE                  //开启获取当前CELLULAR联网状态功能

/******************************************************************************
                      MCU是否需要开启获取模块mac地址功能
如需要请开启该宏,并且mcu在需要获取模块的蓝牙mac地址处调用mcu_api.c文件内mcu_get_module_mac
并在protocol.c文件mcu_get_mac函数内查看结果,
mcu_get_mac内部有#err提示,完成函数后请删除该#err
******************************************************************************/
//#define         GET_MODULE_MAC_ENABLE                   //开启获取模块mac地址功能

/******************************************************************************
                      MCU是否需要支持获取格林时间功能
如需要请开启该宏,并且mcu在需要获取格林时间处调用mcu_api.c文件内mcu_get_green_time
并在protocol.c文件mcu_get_greentime函数内查看结果,
mcu_get_greentime内部有#err提示,完成函数后请删除该#err
mcu在蜂窝模块正确联网后可调用mcu_get_green_time()函数发起校时功能
******************************************************************************/
//#define         SUPPORT_GREEN_TIME                //开启格林时间功能

/******************************************************************************
                      MCU是否需要支持获取模块内存
如需要请开启该宏,并且mcu在需要获取模块内存处调用mcu_api.c文件内get_module_remain_memory
并在protocol.c文件remain_memory_result函数内查看结果,
remain_memory_result内部有#err提示,完成函数后请删除该#err
******************************************************************************/
//#define         GET_MODULE_REMAIN_MEMORY_ENABLE        //开启获取模块内存

/******************************************************************************
                      MCU是否需要开启同步状态上报功能
1) 此命令为同步指令，MCU 数据状态上报后，需要等待模块返回结果；
2) 每次发送模组都会有响应，蜂窝 模组未响应前不可多次上报；
3) 网络不好，数据难以及时上报时，模块会在 5 后返回失败，MCU 需要等待大于 5 秒。
******************************************************************************/
//#define         MCU_DP_UPLOAD_SYN                   //开启同步状态上报功能

/******************************************************************************
                      MCU是否需要开启同步记录类型DP上报功能
1) 此命令为同步指令，MCU 数据状态上报后，需要等待模块返回结果；
2) 每次发送模组都会有响应，蜂窝 模组未响应前不可多次上报；
3) 网络不好，数据难以及时上报时，模块会在 5 后返回失败，MCU 需要等待大于 5 秒。
******************************************************************************/
// #define MCU_DP_UPLOAD_SYN_WITH_TIMESTAMP       //开启同步记录类型DP上报

/******************************************************************************
                      MCU是否需要支持获取当前蜂窝信号强度
如需要请开启该宏,并且mcu在需要蜂窝功能测试处调用mcu_api.c文件内mcu_get_cellular_rssi
并在protocol.c文件get_cellular_rssi_result函数内查看测试结果,
get_cellular_rssi_result内部有#err提示,完成函数后请删除该#err
******************************************************************************/
//#define         GET_CELLULAR_RSSI_ENABLE                //开启获取当前蜂窝信号强度

/******************************************************************************
                      MCU是否需要支持当前蜂窝设备自检
如需要请开启该宏,并且mcu在需要蜂窝功能测试处调用mcu_api.c文件内mcu_set_feature_test
并在protocol.c文件get_feature_test_status函数内查看测试结果,
get_feature_test_result内部有#err提示,完成函数后请删除该#err
******************************************************************************/
//#define         SET_FEATURE_TEST_ENABLE                 //开启模组自检功能

/******************************************************************************
                      MCU是否需要支持蜂窝设备相关命令
如需要请开启该宏,并且mcu在需要蜂窝设备相关处调用mcu_api.c文件内mcu_get_cellular_rssi
并在protocol.c文件get_router_rssi_result函数内查看测试结果,
get_cellular_rssi_result内部有#err提示,完成函数后请删除该#err
******************************************************************************/
#define CELLULAR_SERVICE_ENABLE                //开启蜂窝服务

//#define GNSS_SERIVCE_ENABLE                   //开启地理定位服务

#define CELLULAR_MODULE_TYPE                   0//蜂窝模组类型配置0：其他，1：lz211
/******************************************************************************
                      MCU是否需要支持门锁类功能
如需要请开启该宏,并且mcu在需要蜂窝功能测试处调用mcu_api.c文件内相关接口函数
并在protocol.c文件相关接口函数内查看测试结果,
如果函数内部有#err提示,完成函数后请删除该#err
******************************************************************************/
//#define         LOCK_SERIVCE_ENABLE                //开启门锁类功能
//#define         OFFLINE_DYN_PW_ENABLE              //是否支持离线动态密码
//#define         DECODE_MAX_LEN                   16//离线动态密码解密后的最大长度
/******************************************************************************
                      MCU是否需要支持心跳设置及短链接获取功能
如需要请开启该宏,并且mcu在需要蜂窝功能测试处调用mcu_api.c文件内相关接口函数
并在protocol.c文件相关接口函数内查看测试结果,
如果函数内部有#err提示,完成函数后请删除该#err
******************************************************************************/
// #define         SET_ALIVE_SERIVCE_ENABLE            //是否支持心跳设置功能,该心跳为模组与IOT平台间的心跳
// #define         SET_APN_SERVICE_ENABLE              //是否支持apn设置功能
// #define         GET_CELLULAR_PLMN_ENABLE            //
// #define         SUPPORT_ONLINE_TTS                  //是否支持在线tts
// #define         SUPPORT_MEM_AUDIO                   //是否支持内存音频
// #define         SUPPORT_CALL                        //是否支持电话功能
// #define         SUPPORT_SMS                         //是否支持短信功能
// #define         SUPPORT_GPIO_CTL                    //是否支持gpio控制
// #define         SUPPORT_ADC_CTL                     //是否支持adc控制

/******************************************************************************
                        1:dp数据点序列号重新定义
          **此为自动生成代码,如在开发平台有相关修改请重新下载MCU_SDK**
******************************************************************************/
//软件硬件版本(只上报)
//备注:硬件：F1   软件：00  数据=0xF100
#define DPID_VER 101
//SOC(只上报)
//备注:数据:1=0.01%
//
#define DPID_SOC 102
//满容量(只上报)
//备注:数据:1=0.01AH
//
#define DPID_FULL_CAP 103
//剩余容量(只上报)
//备注:数据:1=0.01AH
//
#define DPID_REMAINING_CAP 104
//充电时间(只上报)
//备注:
#define DPID_CHARGING_TIME 105
//放电时间(只上报)
//备注:
#define DPID_DISCHARGE_TIME 106
//总电压(只上报)
//备注:
#define DPID_VOL 107
//充放电电流(只上报)
//备注:数据:1=0.01A，取值范围为 ‌-32768 到 32767‌
#define DPID_CUR 108
//电池温度(只上报)
//备注:
#define DPID_TEMP 109
//循环次数(只上报)
//备注:
#define DPID_CYCLES 110
//健康状态(只上报)
//备注:
#define DPID_SOH 111
//故障代码(只上报)
//备注:bit1：单节过压
//bit2：单节欠压
//bit3：环境过温
//bit4：环境低温
//bit5：充电过温
//bit6：充电低温
//bit7：放电过温
//bit8：放电低温
//bit9：充电过流
//bit10：放电过流
//bit11：短路保护
//bit12：充满状态
//bit13：AFE丢失
//bit14：电流异常
//bit15：预留
//bit16：预留
#define DPID_FAULT_CODE 112
//第1节电压(只上报)
//备注:
#define DPID_NUM_1_VOL 113
//第2节电压(只上报)
//备注:
#define DPID_NUM_2_VOL 114
//第3节电压(只上报)
//备注:
#define DPID_NUM_3_VOL 115
//第4节电压(只上报)
//备注:
#define DPID_NUM_4_VOL 116
//第5节电压(只上报)
//备注:
#define DPID_NUM_5_VOL 117
//第6节电压(只上报)
//备注:
#define DPID_NUM_6_VOL 118
//第7节电压(只上报)
//备注:
#define DPID_NUM_7_VOL 119
//第8节电压(只上报)
//备注:
#define DPID_NUM_8_VOL 120
//第9节电压(只上报)
//备注:
#define DPID_NUM_9_VOL 121
//第10节电压(只上报)
//备注:
#define DPID_NUM_10_VOL 122
//第11节电压(只上报)
//备注:
#define DPID_NUM_11_VOL 123
//第12节电压(只上报)
//备注:
#define DPID_NUM_12_VOL 124



/**
 * @brief  串口发送数据
 * @param[in] {value} 串口要发送的1字节数据
 * @return Null
 */
void uart_transmit_output(u8 value);

/**
 * @brief  系统所有dp点信息上传,实现APP和muc数据同步
 * @param  Null
 * @return Null
 * @note   MCU必须实现该函数内数据上报功能
 */
void all_data_update(void);

/**
 * @brief  dp下发处理函数
 * @param[in] {dpid} dpid 序号
 * @param[in] {value} dp数据缓冲区地址
 * @param[in] {length} dp数据长度
 * @return dp处理结果
 * -           0(ERROR): 失败
 * -           1(SUCCESS): 成功
 * @note   该函数用户不能修改
 */
u8 dp_download_handle(u8 dpid,const u8 value[], u16 length);

/**
 * @brief  获取所有dp命令总和
 * @param[in] Null
 * @return 下发命令总和
 * @note   该函数用户不能修改
 */
u8 get_download_cmd_total(void);

#ifdef SUPPORT_MCU_FIRM_UPDATE
/**
 * @brief  升级包大小选择
 * @param[in] {package_sz} 升级包大小
 * @ref           0x00: 256byte (默认)
 * @ref           0x01: 512byte
 * @ref           0x02: 1024byte
 * @return Null
 * @note   MCU需要自行实现该功能
 */
void upgrade_package_choose(u8 package_sz);

/**
 * @brief  MCU进入固件升级模式
 * @param[in] {value} 固件缓冲区
 * @param[in] {position} 当前数据包在于固件位置
 * @param[in] {length} 当前固件包长度(固件包长度为0时,表示固件包发送完成)
 * @return Null
 * @note   MCU需要自行实现该功能
 */
u8 mcu_firm_update_handle(const u8 value[],u32 position,u16 length);
#endif

#ifdef SUPPORT_GREEN_TIME
/**
 * @brief  获取到的格林时间
 * @param[in] {time} 获取到的格林时间数据
 * @return Null
 * @note   MCU需要自行实现该功能
 */
void mcu_get_greentime(u8 time[]);
#endif

#ifdef SUPPORT_MCU_RTC_CHECK
/**
 * @brief  MCU校对本地RTC时钟
 * @param[in] {time} 获取到的格林时间数据
 * @return Null
 * @note   MCU需要自行实现该功能
 */
void mcu_write_rtctime(u8 time[]);
#endif

#ifdef WEATHER_ENABLE
/**
* @brief  mcu打开天气服务
 * @param  Null
 * @return Null
 */
void mcu_open_weather(void);

/**
 * @brief  打开天气功能返回用户自处理函数
 * @param[in] {res} 打开天气功能返回结果
 * @ref       0: 失败
 * @ref       1: 成功
 * @param[in] {err} 错误码
 * @return Null
 * @note   MCU需要自行实现该功能
 */
void weather_open_return_handle(u8 res, u8 err);

/**
 * @brief  天气数据用户自处理函数
 * @param[in] {name} 参数名
 * @param[in] {type} 参数类型
 * @ref       0: int 型
 * @ref       1: string 型
 * @param[in] {data} 参数值的地址
 * @param[in] {day} 哪一天的天气  0:表示当天 取值范围: 0~6
 * @ref       0: 今天
 * @ref       1: 明天
 * @return Null
 * @note   MCU需要自行实现该功能
 */
void weather_data_user_handle(i8 *name, u8 type, const u8 *data, i8 day);
#endif

#ifdef MCU_DP_UPLOAD_SYN
/**
 * @brief  状态同步上报结果
 * @param[in] {result} 结果
 * @ref       0: 失败
 * @ref       1: 成功
 * @return Null
 * @note   MCU需要自行实现该功能
 */
void get_upload_syn_result(u8 result);
#endif

#ifdef MCU_DP_UPLOAD_SYN_WITH_TIMESTAMP
/**
 * @brief  记录型状态同步上报结果
 * @param[in] {result} 结果
 * @ref       0: 失败
 * @ref       1: 成功
 * @return Null
 * @note   MCU需要自行实现该功能
 */
void get_upload_syn_timestamp_result(u8 result)
#endif

#ifdef GET_CELLULAR_STATUS_ENABLE
/**
 * @brief  获取 蜂窝设备的联网 状态结果
 * @param[in] {result} 指示 蜂窝设备 工作状态
 * @ref       0(状态1): SIM卡未连接
 * @ref       1(状态2): 搜索网络中
 * @ref       2(状态3): 已成功注册未联网
 * @ref       3(状态4): 联网成功并获取到IP
 * @ref       4(状态5): 已连接到云端
 * @ref       5(状态6): SIM卡拒绝注册
 * @ref       0xff: 未知状态
 * @return Null
 * @note   MCU需要自行实现该功能
 */
void get_cellular_status(u8 result);
#endif

#ifdef GET_MODULE_MAC_ENABLE
/**
 * @brief  获取模块mac结果
 * @param[in] {mac} 模块 MAC 数据
 * @ref       mac[0]: 为是否获取mac成功标志，0x00 表示成功，0x01 表示失败
 * @ref       mac[1]~mac[6]: 当获取 MAC地址标志位如果mac[0]为成功，则表示模块有效的MAC地址
 * @return Null
 * @note   MCU需要自行实现该功能
 */
void mcu_get_mac(u8 mac[]);
#endif

#ifdef GET_MODULE_REMAIN_MEMORY_ENABLE
/**
 * @brief  获取模块内存
 * @param[in] {module_memory} 获取模块内存的大小
 * @return Null
 * @note   MCU需要自行实现该功能
 */
void remain_memory_result(u32 module_memory);
#endif

#ifdef GET_CELLULAR_RSSI_ENABLE
/**
 * @brief  获取当前蜂窝设备信号强度
 * @param[in] {rssi} 获取信号强度结果
 * @return Null
 * @note   MCU需要自行实现该功能
 */
void get_cellular_rssi_result(u8 rssi);
#endif

#ifdef CELLULAR_SERVICE_ENABLE


/**
 * @brief  蜂窝模组服务处理
 * @param[in] {p_data} 数据缓冲区
 * @param[in] {data_len} 数据长度
 * @return Null
 * @note   MCU需要自行实现该功能
 */
void cellular_service_process(u8 cell_cmd,u8 p_data[], u16 data_len);
#endif
#ifdef SET_FEATURE_TEST_ENABLE
/**
 * @brief  获取模组的自检状态
 * @param[in] {sim_st} SIM卡状态.1:tree,0:fail
 * @param[in] {auth} 是否经过涂鸦授权产测.1:tree,0:fail
 * @param[in] {rf} RF射频是否经过校准. 1:校准完成,0:未校准
 * @param[in] {signal} 获取当前设备的信号强度 0~31
 * @return Null
 * @note   MCU needs to implement this function by itself
 */
void get_feature_test_result(u8 sim_st,u8 auth,u8 rf,u8 signal);
#endif

#ifdef LOCK_SERIVCE_ENABLE
/**
 * @brief  获取到的Unix时间
 * @param[in] {time} 获取到的Unix时间数据
 * @return Null
 * @note   MCU需要自行实现该功能
 */
void get_unix_time_zone(u8 time[]);

/**
 * @brief  获取密码进制服务设置结果
 * @param[in] {time} 获取到的设置密码进制服务返回的状态
 * @return Null
 * @note   MCU需要自行实现该功能
 */
void get_set_psw_base_result(u8 result);

/**
 * @brief  schedule列表数据处理
 * @param[in] {schedule_num} schedule列表数量
 * @param[in] {schedule_data} schedule列表数据
 * @return Null
 * @note   MCU调用 schedule_temp_pass_data 成功后，该函数内可对 schedule 列表数据进行处理
 */
static void schedule_data_process(u8 schedule_num, const u8 schedule_data[]);

/**
 * @brief  MCU请求带schedule列表临时密码处理函数
 * @param[in] {succ_flag} 请求标志(1:成功;0:失败)
 * @param[in] {pass_ser} 当前组密码编号(实际编号须+900)
 * @param[in] {pass_valcnt} 当前组密码有效次数(0:不限次数;1:一次性)
 * @param[in] {pass_state} 密码当前状态(0:有效;1:被删除无效)
 * @param[in] {gl_start} 密码生效日期格林时间(从低到高6位，分别为年月日时分秒)
 * @param[in] {gl_end} 密码失效日期格林时间(从低到高6位，分别为年月日时分秒)
 * @param[in] {pass} 临时密码数据(ascll码表示，长度pass_len)
 * @param[in] {pass_len} 临时密码数据长度
 * @param[in] {schedule_num} schedule列表数量
 * @param[in] {schedule_data} schedule列表数据
 * @return Null
 * @note   MCU主动调用 mcu_get_mul_temp_pass 成功后，该函数内可多次分别获取每组的临时密码与有效期限
 */
static void schedule_temp_pass_data(u8 succ_flag, u8 pass_ser,
                               u8 pass_valcnt, u8 pass_state,
                               const u8 gl_start[], const u8 gl_end[],
                               const u8 pass[], u8 pass_len,
                               u8 schedule_num, const u8 schedule_data[]);

/**
 * @brief  MCU请求临时密码(带schedule列表)返回
 * @param[in] {data} 返回数据
 * @return Null
 * @note   Null
 */
void get_schedule_temp_pass_handle(const u8 data[]);

#ifdef OFFLINE_DYN_PW_ENABLE
/**
 * @brief  离线动态密码结果
 * @param[in] {result_data} 结果数据
 * @return Null
 * @note   MCU需要先自行调用mcu_set_offline_dynamic_pswd函数后，在此函数对接收的结果进行处理
 */
void get_offline_dynamic_pswd_result(u8 result_data[]);
#endif
#endif

/**
 * @brief 扩展服务处理
 * @param[in] {p_data} 数据
 * @return Null
 */
void expand_service_process(u8 p_data[]);

#endif
