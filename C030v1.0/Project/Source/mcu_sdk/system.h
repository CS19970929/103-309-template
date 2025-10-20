/**********************************Copyright (c)**********************************
**                       版权所有 (C), 2015-2030, 涂鸦科技
**
**                             http://www.tuya.com
**
*********************************************************************************/
/**
 * @file    system.h
 * @author  涂鸦综合协议开发组
 * @version v1.0.2
 * @date    2021.6.2
 * @brief   串口数据处理，用户无需关心该文件实现内容
 */

/****************************** 免责声明 ！！！ *******************************
由于MCU类型和编译环境多种多样，所以此代码仅供参考，用户请自行把控最终代码质量，
涂鸦不对MCU功能结果负责。
******************************************************************************/

#ifndef __SYSTEM_H_
#define __SYSTEM_H_

#include "tuya_type.h"

#ifdef SYSTEM_GLOBAL
    #define SYSTEM_EXTERN
#else
    #define SYSTEM_EXTERN   extern
#endif

//=============================================================================
//Byte order of the frame
//=============================================================================
#define         HEAD_FIRST                      0
#define         HEAD_SECOND                     1
#define         PROTOCOL_VERSION                2
#define         FRAME_TYPE                      3
#define         LENGTH_HIGH                     4
#define         LENGTH_LOW                      5
#define         DATA_START                      6

//=============================================================================
//Data frame type
//=============================================================================
#define         HEAT_BEAT_CMD                   0                               //心跳包
#define         PRODUCT_INFO_CMD                1                               //产品信息
#define         WORK_MODE_CMD                   2                               //查询MCU 设定的模块工作模式
#define         CELLULAR_STATE_CMD              3                               //报告设备联网状态
#define         DEACTIVE_CELLULAR_NET           4                               //重置蜂窝网络
#define         SET_CELLULAR_WORK_MODE          5                               //设置模组工作模式
#define         DATA_QUERT_CMD                  6                               //命令下发
#define         STATE_UPLOAD_CMD                7                               //状态上报
#define         STATE_QUERY_CMD                 8                               //状态查询
#define         UPDATE_START_CMD                0x0a                            //升级开始
#define         UPDATE_TRANS_CMD                0x0b                            //升级传输
#define         GET_ONLINE_TIME_CMD             0x0c                            //获取系统时间(格林威治时间)
#define         GET_FEATURE_TEST_CMD            0x0e
#define         GET_MODULE_REMAIN_MEMORY_CMD    0x0f                            //获取模块内存
#define         GET_SCHEDULE_PASS               0x14                            //获取临时密码(带schedule列表)
#define         GET_OFFLINE_PASS                0x16                            //获取离线动态密码
#define         SET_PSW_BASE                    0x17                            //设置密码进制服务
#define         GET_UNIX_TIME_ZONE              0x1b                            //获取unix时区信息
#define         GET_LOCAL_TIME_CMD              0x1c                            //获取本地时间
#define         WEATHER_OPEN_CMD                0x20                            //打开天气
#define         WEATHER_DATA_CMD                0x21                            //天气数据
#define         STATE_UPLOAD_SYN_CMD            0x22                            //状态上报（同步）
#define         STATE_UPLOAD_SYN_RESP_CMD       0x23                            //状态上报回复（同步）
#define         GET_CELLULAR_RSSI               0x24                            //获取当前蜂窝设备信号强度
#define         HEAT_BEAT_STOP                  0x25                            //关闭蜂窝模组心跳
#define         DATA_RPT_WITH_TIME_CMD          0x26                            //记录类型DP上报
#define         GET_CELLULAR_STATE_CMD          0x2b                            //获取当前蜂窝设备联网状态
#define         GET_MAC_CMD                     0x2d                            //获取蜂窝模块的蓝牙mac
#define         EXPAND_SERVICE_CMD              0x34                            //扩展服务
#define         BLE_X_SERVICE_CMD               0xd0                            //BLE+CAT.1模式服务命令集
#define         GET_CELLULAR_CMD                0x71                            //获取蜂窝服务相关命令
#define         SET_CELLULAR_CMD                0x72                            //设置蜂窝服务相关命令

//=============================================================================
//子命令
//=============================================================================
#ifdef CELLULAR_SERVICE_ENABLE                                                   //蜂窝服务相关命令
#define MAX_AP_COUNT                            10  //由于WIFI定位的DP点是STRING类型，最大只支持255字节，所以按照10个AP信息一组上传

typedef enum {
    GET_CELLULAR_WORK_MODE = 0x01,                                              //获取蜂窝设备的工作模式
    GET_CELLULAR_IMSI = 0x02,                                                   //获取国际移动用户识别码
    GET_CELLULAR_IMCI = 0x03,                                                   //获取 SIM 卡识别码
    GET_CELLULAR_IMEI = 0x04,                                                   //获取设备 IMEI
#ifdef GNSS_SERIVCE_ENABLE
    GET_CELLULAR_GPS_LOCATION = 0x10,                                           //获取GNSS的经纬度信息
    GET_CELLULAR_GPS_SNR = 0x11,                                                //获取GNSS设备信号强度
    GET_CELLULAR_GPS_SPEED = 0x12,                                              //获取GNSS当前速度(100m/H)
    GET_CELLULAR_GPS_COURSE = 0x13,                                             //获取GNSS当前航向角 (0~359.9)*10
    GET_CELLULAR_GPS_HDOP = 0x16,                                               //获取GNSS当前水平精度因子
    GET_CELLULAR_GPS_ALTITUDE = 0x17,                                           //获取GNSS当前海拔
    GET_CELLULAR_GPS_INFO = 0x1A,                                               //获取GNSS信息
    GET_CELLULAR_WIFI_LOCATION = 0x20,                                          //获取WIFI定位信息
    GET_CELLULAR_LBS_LOCATION = 0x21,                                           //获取基站定位信息
    GET_CELLULAR_GNSS_POWER_STATUS = 0x22,                                      //获取GNSS电源状态
#endif
    CONTRL_CELLULAR_PHONE = 0x23,                                               //控制蜂窝设备电话业务
    CONTRL_CELLULAR_SMS   = 0X24,                                               //控制蜂窝设备短信业务
    GET_CELLULAR_VBAT_VOL = 0x25,                                               //获取电池电量
    GET_CELLULAR_VBAT_STATUS = 0x26,                                            //获取充电状态
    CONTRL_CELLULAR_AUDIO_PLAY = 0x27,                                          //控制蜂窝设备音频播放
    GET_CELLULAR_VOICE_PLAY_STATUS = 0x28,                                      //获取蜂窝设备音频播放状态
#ifdef GNSS_SERIVCE_ENABLE
    GET_CELLULAR_GPS_LOCATION_LAT_LG= 0x29,                                     //获取gps纬经度上报信息
#endif
    GET_CELLULAR_VOICE_PLAY_FINISH  = 0x2a,                                     //自动上报音频播放完整状态
#ifdef GNSS_SERIVCE_ENABLE
    GET_CELLULAR_LOCATION_TRACK_STATUS = 0x30,                                  //获取gps、wifi、lbs定位功能开启状态
#endif
    GET_CELLULAR_VER_INFO   	= 0x41,                                         //获取模组版本
    GET_CELLULAR_NET_TYPE   	= 0x43,                                         //获取模组的当前的网络类型
    CONTRL_CELLULAR_ONLINE_TTS 	= 0x45,                                         //在线tts功能
    CONTRL_CELLULAR_MIC_GAIN 	= 0x46,                                         //设置/查询通话MIC增益
    CONTRL_CELLULAR_SIDE_GAIN 	= 0X47,                                         //设置/查询侧音增益
    GET_CELLULAR_PLMN          	= 0X48,                                         //获取当前蜂窝网络的PLMN
    CONTRL_CELLULAR_MEM_PLAY  	= 0X49, 										//内存播放音频功能
    SET_CELLULAR_RRC           	= 0X4A,                                         //设置RRC
#ifdef GNSS_SERIVCE_ENABLE
    CTL_CELLULAR_GPS_FILTER = 0x18,                                             //控制gnss过滤 
    SET_CELLULAR_GPS_FILTER = 0x19,                                             //设置gnss过滤 
    SET_CELLULAR_CTRL_GNSS = 0x81,                                              //设置GNSS服务
    SET_CELLULAR_CTRL_WIFI = 0x82,                                              //设置WIFI定位服务
    SET_CELLULAR_RESET_GNSS = 0x83,                                             //GNSS设备复位
    SET_CELLULAR_GNSS_POWER = 0x85,                                             //控制GNSS主电源的开启关闭
    SET_CELLULAR_AGNSS = 0x86,                                                  //AGNSS功能控制
    SET_CELLULAR_AUTO_RPT_GPS = 0x90,                                           //设置gps自动上报周期
    SET_CELLULAR_AUTO_RPT_WIFI = 0x91,                                          //设置wifi自动上报周期
    SET_CELLULAR_AUTO_RPT_LBS = 0x92,                                           //设置lbs自动上报周期
#endif
    SET_CELLULAR_VOLUME = 0x87,                                                 //设置音量
    SET_CELLULAR_CTRL_LOW_VOL_POWER = 0x88,                                     //启动或关闭设备低电压关机功能
    SET_CELLULAR_CTRL_BLE_CONN = 0x89,                                          //启动或关闭蓝牙控制
#ifdef SUPPORT_SHORTURL_GET
    SET_CELLULAR_SEND_SHORTURL = 0x93,                                          //获取短链接地址,该功能一般使用在有屏的设备且需要在屏幕上显示绑定二维码.
#endif
    SET_CELLULAR_CTRL_VOLTE = 0x94,                                             //设置VoLTE功能打开或关闭
    SET_CELLULAR_DTMF_DATA          = 0x95,                                     //发送DTMF指令(0x72 95)
	SET_CELLULAR_OFFLINE_TIME 		= 0x96,                                     //设置蜂窝模组连续无法驻网的时间后重启
    SET_CELLULAR_CTRL_MUTE          = 0x97,                                     //设置模组静音或非静音
    SET_CELLULAR_RESET_SELF         = 0x98,                            			//蜂窝模组重启
    SET_CELLULAR_DTMF_CTRL           = 0x99,                            		//DTMF监听功能使能
    RPT_CELLULAR_NET_WARNING        = 0X9A,                                     //蜂窝告警事件上报
    SET_CELLULAR_WAKE_MCU_GPIO      = 0X9B,                                     //外部唤醒管脚配置
    REQ_CELLULAR_MCU_REBOOT         = 0X9D,                                     //通知MCU主动重启设备
    SET_CELLULAR_HOTPLUG            = 0x9e,                                     //SIM卡热插拔使能控制(0x72 9e)
    SET_CELLULAR_ADC                = 0xA0,                                     //ADC控制                 
    SET_CELLULAR_GPIO               = 0xA1,                                     //GPIO控制
}CELLULAR_SUB_CMD;
#endif

#ifdef GNSS_SERIVCE_ENABLE
//=============================================================================
//GNSS的定位跟踪模式
//=============================================================================
typedef enum{
    GNSS_ATTACH_MODE_GPS_BD,                                                     //GPS+BEIDOU定位模式
    GNSS_ATTACH_MODE_GPS_GL,                                                     //GPS+GLONASS定位模式
    GNSS_ATTACH_MODE_GPS,                                                        //GPS定位模式
    GNSS_ATTACH_MODE_BD,                                                         //北斗定位模式
    GNSS_ATTACH_MODE_GL,                                                         //GLONASS定位模式
    GNSS_ATTACH_MODE_GPS_GA,                                                     //GPS+伽利略定位模式
}GNSS_ATTACH_MODE_e;
#endif


/*模块拓展服务子命令*/
typedef enum {
    EXPAND_OPEN_RESET_SEVER            = 0x04, //打开模块重置状态通知
    EXPAND_UP_RESET_DATA               = 0x05, //模块重置状态通知
    EXPAND_FOTA_NOTIFY                 = 0x09,
} TY_UART_EXPAND_SERVICE_CMD_E;

//=============================================================================
#define MCU_RX_VER              0x00                                            //模块发送帧协议版本号
#define MCU_TX_VER              0x03                                            //MCU 发送帧协议版本号(默认)
#define PROTOCOL_HEAD           0x07                                            //固定协议头长度
#define FRAME_FIRST             0x55                                            //帧头第一字节
#define FRAME_SECOND            0xaa                                            //帧头第二字节
//=============================================================================
SYSTEM_EXTERN volatile u8 cellular_data_process_buf[PROTOCOL_HEAD + CELLULAR_DATA_PROCESS_LMT];     //串口数据处理缓存
SYSTEM_EXTERN volatile u8 cellular_uart_rx_buf[PROTOCOL_HEAD + CELLULAR_UART_RECV_BUF_LMT];         //串口接收缓存
SYSTEM_EXTERN volatile u8 cellular_uart_tx_buf[PROTOCOL_HEAD + CELLULAR_UART_SEND_BUF_LMT];        //串口发送缓存
//
SYSTEM_EXTERN volatile u8 *rx_buf_in;
SYSTEM_EXTERN volatile u8 *rx_buf_out;

SYSTEM_EXTERN volatile u8 stop_update_flag;                                                 //ENABLE:停止一切数据上传  DISABLE:恢复一切数据上传
#ifdef LOCK_SERIVCE_ENABLE
SYSTEM_EXTERN volatile u8 is_setpswd_base;                                                 //TRUE:设置过密码进制服务  FALSE:没有设置过密码进制服务
#endif
#ifndef CELLULAR_CONTROL_SELF_MODE
SYSTEM_EXTERN volatile u8 reset_cellular_flag;                                                  //重置cellular标志(TRUE:成功/FALSE:失败)
SYSTEM_EXTERN volatile u8 set_cellularmode_flag;                                                //设置cellular工作模式标志(TRUE:成功/FALSE:失败)
SYSTEM_EXTERN volatile u8 cellular_work_state;                                                  //cellular模块当前工作状态
#endif

#ifdef SUPPORT_MCU_FIRM_UPDATE
SYSTEM_EXTERN volatile u16 firm_size;                                            	//升级包一包的大小
SYSTEM_EXTERN volatile u32 firm_length;                                           	//MCU升级文件长度
SYSTEM_EXTERN volatile u8 firm_update_flag;                                  		//MCU升级标志
#endif

/**
 * @brief  写cellular_uart字节
 * @param[in] {dest} 缓存区其实地址
 * @param[in] {byte} 写入字节值
 * @return 写入完成后的总长度
 */
u16 set_cellular_uart_byte(u16 dest, u8 byte);

/**
 * @brief  写cellular_uart_buffer
 * @param[in] {dest} 目标地址
 * @param[in] {src} 源地址
 * @param[in] {len} 数据长度
 * @return 写入结束的缓存地址
 */
u16 set_cellular_uart_buffer(u16 dest, const u8 *src, u16 len);

/**
 * @brief  计算校验和
 * @param[in] {pack} 数据源指针
 * @param[in] {pack_len} 计算校验和长度
 * @return 校验和
 */
u8 get_check_sum(u8 *pack, u16 pack_len);

/**
 * @brief  向cellular串口发送一帧数据
 * @param[in] {fr_type} 帧类型
 * @param[in] {fr_ver} 帧版本
 * @param[in] {len} 数据长度
 * @return Null
 */
void cellular_uart_write_frame(u8 fr_type, u8 fr_ver, u16 len);

/**
 * @brief  数据帧处理
 * @param[in] {offset} 数据起始位
 * @return Null
 */
void data_handle(u16 offset);

/**
 * @brief  判断串口接收缓存中是否有数据
 * @param  Null
 * @return 是否有数据
 */
u8 with_data_rxbuff(void);

/**
 * @brief  读取队列1字节数据
 * @param  Null
 * @return Read the data
 */
u8 take_byte_rxbuff(void);
/**
 * @brief  获取设置进制服务的标志
 * @param  Null
 * @return Read the data
 */
u8 get_setpassword_base_flag(void);
#endif
