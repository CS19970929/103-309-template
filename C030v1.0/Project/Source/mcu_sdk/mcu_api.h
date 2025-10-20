/**********************************Copyright (c)**********************************
**                       版权所有 (C), 2015-2030, 涂鸦科技
**
**                             http://www.tuya.com
**
*********************************************************************************/
/**
 * @file    mcu_api.h
 * @author  涂鸦综合协议开发组
 * @version v1.0.2
 * @date    2021.6.2
 * @brief   用户需要主动调用的函数都在该文件内
 */

/****************************** 免责声明 ！！！ *******************************
由于MCU类型和编译环境多种多样，所以此代码仅供参考，用户请自行把控最终代码质量，
涂鸦不对MCU功能结果负责。
******************************************************************************/

#ifndef __MCU_API_H_
#define __MCU_API_H_

#include "tuya_type.h"
#include "protocol.h"

#ifdef MCU_API_GLOBAL
  #define MCU_API_EXTERN
#else
  #define MCU_API_EXTERN   extern
#endif

/**
 * @brief  hex转bcd
 * @param[in] {Value_H} 高字节
 * @param[in] {Value_L} 低字节
 * @return 转换完成后数据
 */
u8 hex_to_bcd(u8 Value_H,u8 Value_L);

/**
 * @brief  求字符串长度
 * @param[in] {str} 字符串地址
 * @return 数据长度
 */
u32 my_strlen(u8 *str);

/**
 * @brief  把src所指内存区域的前count个字节设置成字符c
 * @param[out] {src} 待设置的内存首地址
 * @param[in] {ch} 设置的字符
 * @param[in] {count} 设置的内存长度
 * @return 待设置的内存首地址
 */
void *my_memset(void *src,u8 ch,u16 count);

/**
 * @brief  内存拷贝
 * @param[out] {dest} 目标地址
 * @param[in] {src} 源地址
 * @param[in] {count} 拷贝数据个数
 * @return 数据处理完后的源地址
 */
void *my_memcpy(void *dest, const void *src, u16 count);

/**
 * @brief  字符串拷贝
 * @param[in] {dest} 目标地址
 * @param[in] {src} 源地址
 * @return 数据处理完后的源地址
 */
i8 *my_strcpy(i8 *dest, const i8 *src);

/**
 * @brief  字符串比较
 * @param[in] {s1} 字符串 1
 * @param[in] {s2} 字符串 2
 * @return 大小比较值
 * -         0:s1=s2
 * -         <0:s1<s2
 * -         >0:s1>s2
 */
i32 my_strcmp(i8 *s1 , i8 *s2);

/**
 * @brief  将int类型拆分四个字节
 * @param[in] {number} 4字节原数据
 * @param[out] {value} 处理完成后4字节数据
 * @return Null
 */
void int_to_byte(u32 number,u8 value[4]);

/**
 * @brief  将4字节合并为1个32bit变量
 * @param[in] {value} 4字节数组
 * @return 合并完成后的32bit变量
 */
u32 byte_to_int(const u8 value[4]);

/**
 * @brief  raw型dp数据上传
 * @param[in] {dpid} dpid号
 * @param[in] {value} 当前dp值指针
 * @param[in] {len} 数据长度
 * @return Null
 * @note   Null
 */
u8 mcu_dp_raw_update(u8 dpid,const u8 value[],u16 len);

/**
 * @brief  bool型dp数据上传
 * @param[in] {dpid} dpid号
 * @param[in] {value} 当前dp值
 * @return Null
 * @note   Null
 */
u8 mcu_dp_bool_update(u8 dpid,u8 value);

/**
 * @brief  value型dp数据上传
 * @param[in] {dpid} dpid号
 * @param[in] {value} 当前dp值
 * @return Null
 * @note   Null
 */
u8 mcu_dp_value_update(u8 dpid,u32 value);

/**
 * @brief  string型dp数据上传
 * @param[in] {dpid} dpid号
 * @param[in] {value} 当前dp值指针
 * @param[in] {len} 数据长度
 * @return Null
 * @note   Null
 */
u8 mcu_dp_string_update(u8 dpid,const u8 value[],u16 len);

/**
 * @brief  enum型dp数据上传
 * @param[in] {dpid} dpid号
 * @param[in] {value} 当前dp值
 * @return Null
 * @note   Null
 */
u8 mcu_dp_enum_update(u8 dpid,u8 value);

/**
 * @brief  fault型dp数据上传
 * @param[in] {dpid} dpid号
 * @param[in] {value} 当前dp值
 * @return Null
 * @note   Null
 */
u8 mcu_dp_fault_update(u8 dpid,u32 value);

#ifdef MCU_DP_UPLOAD_SYN
/**
 * @brief  raw型dp数据同步上传
 * @param[in] {dpid} dpid号
 * @param[in] {value} 当前dp值指针
 * @param[in] {len} 数据长度
 * @return Null
 * @note   Null
 */
u8 mcu_dp_raw_update_syn(u8 dpid,const u8 value[],u16 len);

/**
 * @brief  bool型dp数据同步上传
 * @param[in] {dpid} dpid号
 * @param[in] {value} 当前dp值指针
 * @return Null
 * @note   Null
 */
u8 mcu_dp_bool_update_syn(u8 dpid,u8 value);

/**
 * @brief  value型dp数据同步上传
 * @param[in] {dpid} dpid号
 * @param[in] {value} 当前dp值指针
 * @return Null
 * @note   Null
 */
u8 mcu_dp_value_update_syn(u8 dpid,u32 value);

/**
 * @brief  string型dp数据同步上传
 * @param[in] {dpid} dpid号
 * @param[in] {value} 当前dp值指针
 * @param[in] {len} 数据长度
 * @return Null
 * @note   Null
 */
u8 mcu_dp_string_update_syn(u8 dpid,const u8 value[],u16 len);

/**
 * @brief  enum型dp数据同步上传
 * @param[in] {dpid} dpid号
 * @param[in] {value} 当前dp值指针
 * @return Null
 * @note   Null
 */
u8 mcu_dp_enum_update_syn(u8 dpid,u8 value);

/**
 * @brief  fault型dp数据同步上传
 * @param[in] {dpid} dpid号
 * @param[in] {value} 当前dp值指针
 * @return Null
 * @note   Null
 */
u8 mcu_dp_fault_update_syn(u8 dpid,u32 value);
#endif
#ifdef MCU_DP_UPLOAD_SYN_WITH_TIMESTAMP
/*
@brief 记录型raw类型数据同步上报
@param[in] {dpid} dpid号
@param[in] {value} 数据指针
@param[in] {len} 数据长度
@param[in] {timestamp} 时间戳 (Data[0]: 是否带本地时间标志位
	bit: 0表示这条数据不带MCU给的时间，后面的时间模块认为数据无效不处理
	1表示后面的时间数据有效，时间数据为设备所在的当地时间；
	2表示后面的时间数据有效，时间数据为格林时间；
	 Data[1]为年份,  0x00表示2000年
	 Data[2]为月份，从1开始到12结束
	 Data[3]为日期，从1开始到31结束
	 Data[4]为时钟，从0开始到23结束
	 Data[5]为分钟，从0开始到59结束
	 Data[6]为秒钟，从0开始到59结束  
@return Null
@note Null
*/
u8 mcu_dp_raw_update_syn_timestamp(u8 dpid,const u8 value[],u16 len,TIMESTAMP_T timestamp);
/**
 * @brief  记录型bool型dp数据同步上传
 * @param[in] {dpid} dpid号
 * @param[in] {value} 当前dp值指针
 * @param[in] {timestamp}时间
 * @return Null
 * @note   Null
 */
u8 mcu_dp_bool_update_syn_timestamp(u8 dpid,u8 value,TIMESTAMP_T timestamp);

/**
 * @brief  记录型value型dp数据同步上传
 * @param[in] {dpid} dpid号
 * @param[in] {value} 当前dp值指针
 * @param[in] {timestamp}时间
 * @return Null
 * @note   Null
 */
u8 mcu_dp_value_update_syn_timestamp(u8 dpid,u32 value,TIMESTAMP_T timestamp);

/**
 * @brief  记录型string型dp数据同步上传
 * @param[in] {dpid} dpid号
 * @param[in] {value} 当前dp值指针
 * @param[in] {len} 数据长度
 * @param[in] {timestamp}时间
 * @return Null
 * @note   Null
 */
u8 mcu_dp_string_update_syn_timestamp(u8 dpid,const u8 value[],u16 len,TIMESTAMP_T timestamp)

/**
 * @brief  记录型enum型dp数据同步上传
 * @param[in] {dpid} dpid号
 * @param[in] {value} 当前dp值指针
 * @param[in] {timestamp}时间
 * @return Null
 * @note   Null
 */
u8 mcu_dp_enum_update_syn_timestamp(u8 dpid,u8 value,TIMESTAMP_T timestamp);

/**
 * @brief  记录型fault型dp数据同步上传
 * @param[in] {dpid} dpid号
 * @param[in] {value} 当前dp值指针
 * @param[in] {timestamp}时间
 * @return Null
 * @note   Null
 */
u8 mcu_dp_fault_update_syn_timestamp(u8 dpid,u32 value,TIMESTAMP_T timestamp);
#endif

/**
 * @brief  mcu获取bool型下发dp值
 * @param[in] {value} dp数据缓冲区地址
 * @param[in] {len} dp数据长度
 * @return 当前dp值
 * @note   Null
 */
u8 mcu_get_dp_download_bool(const u8 value[],u16 len);

/**
 * @brief  mcu获取enum型下发dp值
 * @param[in] {value} dp数据缓冲区地址
 * @param[in] {len} dp数据长度
 * @return 当前dp值
 * @note   Null
 */
u8 mcu_get_dp_download_enum(const u8 value[],u16 len);

/**
 * @brief  mcu获取value型下发dp值
 * @param[in] {value} dp数据缓冲区地址
 * @param[in] {len} dp数据长度
 * @return 当前dp值
 * @note   Null
 */
u32 mcu_get_dp_download_value(const u8 value[],u16 len);

/**
 * @brief  串口接收数据暂存处理
 * @param[in] {value} 串口收到的1字节数据
 * @return Null
 * @note   在MCU串口处理函数中调用该函数,并将接收到的数据作为参数传入
 */
void uart_receive_input(u8 value);

/**
 * @brief  串口接收多个字节数据暂存处理
 * @param[in] {value} 串口要接收的数据的源地址
 * @param[in] {data_len} 串口要接收的数据的数据长度
 * @return Null
 * @note   如需要支持一次多字节缓存，可调用该函数
 */
void uart_receive_buff_input(u8 value[], u16 data_len);

/**
 * @brief  蜂窝设备串口数据处理服务
 * @param  Null
 * @return Null
 * @note   在MCU主函数while循环中调用该函数
 */
void cellular_uart_service(void);

/**
 * @brief  协议串口初始化函数
 * @param  Null
 * @return Null
 * @note   在MCU初始化代码中调用该函数
 */
void cellular_protocol_init(void);

#ifndef CELLULAR_CONTROL_SELF_MODE
/**
 * @brief  MCU获取复位蜂窝设备成功标志
 * @param  Null
 * @return 复位标志
 * -           0(RESET_CELLULAR_ERROR):失败
 * -           1(RESET_CELLULAR_SUCCESS):成功
 * @note   1:MCU主动调用mcu_reset_cellular()后调用该函数获取复位状态
 *         2:如果为模块自处理模式,MCU无须调用该函数
 */
u8 mcu_get_reset_cellular_flag(void);

/**
 * @brief  MCU主动重置蜂窝设备激活状态
 * @param  Null
 * @return Null
 * @note   1:MCU主动调用,通过mcu_get_reset_celluluar_flag()函数获取重置蜂窝设备是否成功
 *         2:如果为模块自处理模式,MCU无须调用该函数
 */
void mcu_reset_cellular(void);

/**
 * @brief  获取设置蜂窝设备工作模式状态成功标志
 * @param  Null
 * @return 蜂窝mode flag
 * -           0(SET_CELLULARCONFIG_ERROR):失败
 * -           1(SET_CELLULARCONFIG_SUCCESS):成功
 * @note   1:MCU主动调用mcu_set_cellular_mode()后调用该函数获取复位状态
 *         2:如果为模块自处理模式,MCU无须调用该函数
 */
u8 mcu_get_cellular_mode_flag(void);

/**
 * @brief  MCU设置蜂窝设备工作模式
 * @param[in] {mode} 进入的模式
 * @ref        1:全功能模式
 * @ref        4:飞行模式
 * @return Null
 * @note   1:MCU主动调用
 *         2:成功后,可判断set_cellularmode_flag是否为TRUE;TRUE表示为设置工作模式成功
 *         3:如果为模块自处理模式,MCU无须调用该函数
 */
void mcu_set_cellular_mode(u8 mode);

/**
 * @brief  MCU主动获取当前蜂窝设备工作状态
 * @param  Null
 * @return cellular work state
 * -          0: SIM卡未连接
 * -          1: 搜索网络中
 * -          2: 已成功注册未联网
 * -          3: 联网成功并获取到IP
 * -          4: 已连接到云端
 * -          0xff: 未知状态
 * @note   如果为模块自处理模式,MCU无须调用该函数
 */
u8 mcu_get_cellular_work_state(void);
#endif

#ifdef SUPPORT_GREEN_TIME
/**
 * @brief  MCU获取格林时间
 * @param  Null
 * @return Null
 * @note   MCU需要自行调用该功能
 */
void mcu_get_green_time(void);
#endif

#ifdef SUPPORT_MCU_RTC_CHECK
/**
 * @brief  MCU获取系统时间,用于校对本地时钟
 * @param  Null
 * @return Null
 * @note   MCU主动调用完成后在mcu_write_rtctime函数内校对rtc时钟
 */
void mcu_get_system_time(void);
#endif

#ifdef CELLULAR_HEARTSTOP_ENABLE
/**
 * @brief  通知蜂窝模组关闭心跳
 * @param  Null
 * @return Null
 * @note   MCU需要自行调用该功能
 */
void cellular_heart_stop(void);
#endif

#ifdef GET_CELLULAR_STATUS_ENABLE
/**
 * @brief  获取当前蜂窝模组联网状态
 * @param  Null
 * @return Null
 * @note   MCU需要自行调用该功能
 */
void mcu_get_cellular_connect_status(void);
#endif

#ifdef GET_MODULE_MAC_ENABLE
/**
 * @brief  获取模块MAC
 * @param  Null
 * @return Null
 * @note   MCU需要自行调用该功能
 */
void mcu_get_module_mac(void);
#endif

#ifdef GET_MODULE_REMAIN_MEMORY_ENABLE
/**
 * @brief  获取 蜂窝设备 模块剩余内存
 * @param  Null
 * @return Null
 * @note   MCU需要自行调用该功能
 */
void get_module_remain_memory(void);
#endif

#ifdef GET_CELLULAR_RSSI_ENABLE
/**
 * @brief  获取当前蜂窝设备信号强度
 * @param  Null
 * @return Null
 * @note   MCU需要自行调用该功能
 */
void mcu_get_cellular_rssi(void);
#endif

#ifdef CELLULAR_SERVICE_ENABLE
/**
 * @brief  获取蜂窝设备的工作模式
 * @param  Null
 * @return Null
 * @note   MCU需要自行实现该功能
 */
void mcu_get_cellular_work_mode(void);

/**
 * @brief  获取国际移动用户识别码
 * @param  Null
 * @return Null
 * @note   MCU需要自行实现该功能
 */
void mcu_get_IMSI(void);

/**
 * @brief  获取 SIM 卡识别码
 * @param  Null
 * @return Null
 * @note   MCU需要自行实现该功能
 */
void mcu_get_IMCI(void);

/**
 * @brief  获取设备 IMEI
 * @param  Null
 * @return Null
 * @note   MCU需要自行实现该功能
 */
void mcu_get_IMEI(void);

#ifdef GNSS_SERIVCE_ENABLE
typedef struct
{
	float  	lat;
	float 	lng;
	u8 		snr;
	u8  	satellites_num;
	u16  	speed;
	u16     course;
	u16 	hdop;
	u16 	altitude;
}gps_info_struct_t;
/**
 * @brief   通过模组获取到经纬度信息
 * @param  Null
 * @return Null
 * @note   MCU需要自行实现该功能
 */
void mcu_get_gnss_location(void);
/**
 * @brief  通过模组获取的当前的GNSS设备的信号强度
 * @param  Null
 * @return Null
 * @note   MCU需要自行实现该功能
 */
void mcu_get_gnss_rssi(void);

/**
 * @brief  通过模组获取到当前设备的速度
 * @param  Null
 * @return Null
 * @note   MCU需要自行实现该功能
 */
void mcu_get_gnss_speed(void);

/**
 * @brief  获取GNSS当前航向角
 * @param  Null
 * @return Null
 * @note   MCU需要自行实现该功能
 */
void mcu_get_gnss_course(void);

/**
 * @brief  获取GNSS当前水平精度因子
 * @param  Null
 * @return Null
 * @note   MCU需要自行实现该功能
 */
void mcu_get_gnss_hdop(void);

/**
 * @brief  获取GNSS当前海拔
 * @param  [in] {enable} true:open,false:close
 * @return Null
 * @note   MCU需要自行实现该功能
 */
void mcu_get_gnss_altitude(void);

/**
 * @brief  控制GNSS过滤功能
 * @param  {in} enable: 0:disable filter 1:enable filter
 * @return Null
 * @note   MCU需要自行实现该功能
 */
void mcu_ctl_gnss_filter(bool_t enable);

/**
 * @brief  设置GNSS过滤参数
 * @param  {in} filter_value : 0~99.9
 * @return Null
 * @note   MCU需要自行实现该功能
 */
void mcu_set_gnss_filter(float filter_value);

/**
 * @brief  获取GNSS当前信息
 * @param  Null
 * @return Null
 * @note   MCU需要自行实现该功能
 */
void mcu_get_gnss_info(void);

/**
 * @brief  控制GNSS设备，启动、关闭，以及定位跟踪模式（GPS+BD是默认的）
 * @param  [in] {enable} true:启动,false:关闭
 * @param  [in] {mode} GNSS 定位跟踪模式
 * @return Null
 * @note   MCU需要自行实现该功能
 */
void mcu_control_gnss(bool_t enable,GNSS_ATTACH_MODE_e mode);

/**
 * @brief  复位 GNSS 设备
 * @param  Null
 * @return Null
 * @note   MCU需要自行实现该功能
 */
void mcu_reset_gnss(u8 pin,bool_t level);

/**
 * @brief  启动或者关闭WIFI定位
 * @param  [in] {enable} true:启动,false:停止
 * @return Null
 * @note   MCU需要自行实现该功能
 */
void mcu_control_wifi_location(bool_t enable);

/**
 * @brief  获取WIFI定位信息
 * @param  Null
 * @return Null
 * @note   MCU需要自行实现该功能
 */
void mcu_get_wifi_location(void);

/**
 * @brief  获取LBS定位信息
 * @param  Null
 * @return Null
 * @note   MCU需要自行实现该功能
 */
void mcu_get_lbs_location(void);

/**
 * @brief  mcu设置gnss纬经度上报
 * @param  Null
 * @return Null
 * @note   MCU需要自行实现该功能
 */
void mcu_set_gnss_lat_lg_location(void);

/**
 * @brief  mcu设置gnss自动上报
 * @param  Null
 * @return Null
 * @note   MCU需要自行实现该功能
 */
void mcu_set_cellular_auto_rpt_gps(u16 period, u8 dpid);

/**
 * @brief  mcu设置wifi自动上报
 * @param  Null
 * @return Null
 * @note   MCU需要自行实现该功能
 */
void mcu_set_cellular_auto_rpt_wifi(u16 period, u8 dpid);

/**
 * @brief  mcu设置lbs自动上报
 * @param  Null
 * @return Null
 * @note   MCU需要自行实现该功能
 */
void mcu_set_cellular_auto_rpt_lbs(u16 period, u8 dpid);

/**
 * @brief  mcu获取各定位功能是否开启
 * @param  Null
 * @return Null
 * @note   MCU需要自行实现该功能
 */
void mcu_get_cellular_location_track_status(void);

#if (CELLULAR_MODULE_TYPE == 1)
/**
 * @brief  控制gnss设备主电源开启和关闭
 * @param  [in] {enable} true:启动,false:停止
 * @return Null
 * @note   MCU需要自行实现该功能
 */
void mcu_control_gnss_power(bool_t enable);

/**
 * @brief  获取gnss设备电源状态
 * @param  Null
 * @return Null
 * @note   MCU需要自行实现该功能
 */
void mcu_get_gnss_power_status(void);

#endif

/**
 * @brief  Control the agnss service
 * @param  [in] {enable} true:Start,false:Stop
 * @return Null
 * @note   The MCU needs to implement this function itself
 */
void mcu_ctl_agnss(bool_t enable);
#endif

#ifdef SUPPORT_CALL
/**
 * @brief  通知蜂窝模组，收到来电
 * @param  Null
 * @return Null
 * @note   MCU需要自行实现该功能
 */
void mcu_control_phone_callin_rsp(void);

/**
 * @brief  控制蜂窝模组电话呼出
 * @param  [in] {phone_num} 手机号码
 * @param  [in] {phone_num_len} 手机号码长度
 * @return Null
 * @note   MCU需要自行实现该功能
 */
void mcu_control_phone_callout(u8 *phone_num,u32 phone_num_len);

/**
 * @brief  控制蜂窝模组电话接听
 * @param  Null
 * @return Null
 * @note   MCU需要自行实现该功能
 */
void mcu_control_phone_anwser(void);

/**
 * @brief  控制蜂窝模组电话挂断
 * @param  Null
 * @return Null
 * @note   MCU需要自行实现该功能
 */
void mcu_control_phone_hungup(void);

/**
 * @brief  获取蜂窝模组电话状态
 * @param  Null
 * @return Null
 * @note   MCU需要自行实现该功能
 */
void mcu_get_phone_status(void);

/**
 * @brief  发送dtmf信息
 * @param  [in] {dtmf_data} dtmf音频值
 * @return Null
 * @note   MCU需要自行实现该功能
 */
void mcu_send_dtmf(u8 dtmf_data);

/**
 * @brief  DTMF监听控制
 * @param  [in] {enable} 0：关闭监听，1：打开监听
 * @return Null
 * @note   MCU需要自行实现该功能
 */
void mcu_ctl_dtmf_monitor(u8 enable);
#endif

#ifdef SUPPORT_SMS
/**
 * @brief  控制接收短信是否成功
 * @param  [in] {result} true:成功,false:失败
 * @return Null
 * @note   MCU需要自行实现该功能
 */
void mcu_control_recv_sms_rsp(bool_t result);

/**
 * @brief  控制蜂窝模组发送短信
 * @param  [in] {sms_ctx} 短信内容
 * @param  [in] {phone_num} 手机号
 * @return Null
 * @note   MCU需要自行实现该功能
 */
void mcu_control_send_sms(u8 *sms_ctx, u8 *phone_num);
#endif

/**
 * @brief  获取电池电量
 * @param  Null
 * @return Null
 * @note   MCU需要自行实现该功能
 */
void mcu_get_battery(void);

/**
 * @brief  获取蜂窝模组充电状态
 * @param  Null
 * @return Null
 * @note   MCU需要自行实现该功能
 */
void mcu_get_charging_status(void);

/**
 * @brief  控制蜂窝模组音量设置
 * @param  [in] {cmd} 1:本地音量 2:通话音量
 * @param  [in] {vol} 十进制：0-100
 * @return Null
 * @note   MCU需要自行实现该功能
 */
void mcu_control_set_volume(u8 cmd, u8 vol);

/**
 * @brief  控制蜂窝模组sd卡中音频播放
 * @param  [in] {port} 0:播放到本地喇叭 1:播放到通话中的上行链路
 * @param  [in] {play_cmd} 0:停止 1:播放 2:暂停 3:恢复
 * @param  [in] {format} 播放格式：1:pcm 2:wav pcm 3:mp3 4:amr-nb 5:amr-wb
 * @param  [in] {addr} 音频存储地址
 * @param  [in] {addr_len} 音频存储地址长度
 * @return Null
 * @note   MCU需要自行实现该功能
 */
void mcu_control_audio_play(u8 port,
                            u8 play_cmd,
                            u8 format,
                            u8 *addr,
                            u8 addr_len);

/**
 * @brief  获取本地音频播放状态
 * @param  Null
 * @return Null
 * @note   MCU需要自行实现该功能
 */
void mcu_get_audio_play_status(void);

/**
 * @brief  设置低电量关机功能开启或关闭
 * @param  Null
 * @return Null
 * @note   MCU需要自行实现该功能
 */
void mcu_ctrl_low_vol_power(bool_t enable);

/**
 * @brief  设置蓝牙连接功能开启或关闭
 * @param  Null
 * @return Null
 * @note   MCU需要自行实现该功能`
 */
void mcu_ctrl_ble_conn(bool_t enable);

/**
 * @brief  设置VoLTE功能开启或关闭
 * @param  [in] {enable} 0:关闭 1:打开
 * @return Null
 * @note   MCU需要自行实现该功能
 */
void mcu_ctrl_volte(bool_t enable);

/**
 * @brief  启动或者关闭模块的静音模式
 * @param  [in] {type} 0: sms, 1: call in
 * @param  [in] {enable} 0: 打开, 1: 关闭
 * @return Null
 * @note   MCU需要自行实现该功能
 */
void mcu_ctrl_mute(u8 type,bool_t enable);

/**
 * @brief  设置模块无法驻网成功的连续时间后重启
 * @param  [in] {duration} 连续无法驻网时间，单位秒
 * @return Null
 * @note   MCU需要自行实现该功能
 */
void mcu_set_offline_duraion(u16 duration);
#endif

#ifdef LOCK_SERIVCE_ENABLE
/**
 * @brief  MCU获取unix时区
 * @param  Null
 * @return Null
 * @note   MCU需要自行调用该功能
 */
void mcu_get_unix_time_zone(void);

/**
 * @brief  MCU请求获取云端临时密码(带schedule列表)
 * @param  Null
 * @return Null
 * @note   MCU主动调用完成后在 get_schedule_temp_pass_handle 函数内可获取结果
 */
void mcu_get_schedule_temp_pass(void);

/**
 * @brief  设置密码进制服务
 * @param[in] {pswd_num} 连续的密码数字按键范围
 * @param[in] {start} 密码数字开始的数值
 * @return Null
 * @note   MCU需要自行调用后，可在get_set_psw_base_result函数中对结果进行处理
 */
void mcu_set_pswd_base(u8 pswd_num,u8 start);

#ifdef OFFLINE_DYN_PW_ENABLE
/**
 * @brief  离线动态密码
 * @param[in] {green_time} 格林时间
                green_time[0]为年份，0x00 表示2000 年
                green_time[1]为月份，从 1 开始到12 结束
                green_time[2]为日期，从 1 开始到31 结束
                green_time[3]为时钟，从 0 开始到23 结束
                green_time[4]为分钟，从 0 开始到59 结束
                green_time[5]为秒钟，从 0 开始到59 结束
 * @param[in] {pw} 离线动态密码
 * @param[in] {pw_len} 离线动态密码长度
 * @return Null
 * @note   MCU需要自行调用后，可在get_offline_dynamic_pswd_result函数中对结果进行处理
 */
void mcu_set_offline_dynamic_pswd(u8 green_time[],u8 pw[],u8 pw_len);
#endif
#endif

/**
 * @brief  获取模组版本信息
 * @return Null
 * @note   MCU需要自行实现该功能
 */
void mcu_get_cellular_ver(void);

/**
 * @brief  蜂窝网络类型读取
 * @return Null
 * @note   MCU需要自行实现该功能
 */
void mcu_get_cellular_nettype(void);

/**
 * @brief  打开扩展服务的重置通知
 * @param  Null
 * @return Null
 * @note   MCU需要自行调用该功能
 */
void mcu_open_expand_reset(void);

/**
 * @brief  对模组进行软件重启
 * @return Null
 * @note   MCU需要自行实现该功能
 */
void mcu_ctrl_cellular_reset(void);

/**
 * @brief  模组外部唤醒管脚变化时进行配置。
 * @param  [in] {gpio} GPIO编号，具体和硬件和模组相关
 * @param  [in] {low_level_time} 低电平持续时间（10ms单位，最大250ms)(部分模组无法实现10ms，以实际结果为准)
 * @return Null
 * @note   MCU需要自行实现该功能
 */
void mcu_set_cellular_wakeup_gpio(u8 gpio,u8 low_level_time);

/**
 * @brief  SIM卡热插拔使能控制
 * @param  [in] {enable} 1使能；0关闭
 * @return Null
 * @note   MCU需要自行实现该功能
 */
void mcu_switch_sim_hotplug(u8 enable);

/**
 * @brief  设置/查询通话MIC增益
 * @param  [in] {ctrl} 0 -- 查询; 1 -- 设置
 * @param  [in] {gain} 设置值  0~30（0静音）
 * @return Null
 * @note   MCU需要自行实现该功能
 */
void mcu_contrl_cellular_mic_gain(u8 ctrl,u8 gain);

/**
 * @brief  设置/查询侧音增益
 * @param  [in] {ctrl} 0 -- 查询; 1 -- 设置
 * @param  [in] {gain} 设置值  0~99（0静音）
 * @return Null
 * @note   MCU需要自行实现该功能
 */
void mcu_contrl_cellular_side_gain(u8 ctrl,u8 gain);

#ifdef SET_RRC_ENABLE
/**
 * @brief  控制模组RRC
 * @param  [in] ctrl, 0:get rrc value, 1:set rrc value
 * @param  [in] rrc value
 * @return Null
 * @note   MCU需要自行实现该功能
 */
void mcu_ctl_cellular_rrc(u8 ctrl, u8 rrc);
#endif

#ifdef GET_CELLULAR_PLMN_ENABLE
/**
 * @brief  查询当前的PLMN
 * @return Null
 * @note   MCU需要自行实现该功能
 */
void mcu_get_cellular_plmn(void);
#endif

#ifdef SUPPORT_MEM_AUDIO
typedef struct 
{
	u8 	format;				//audio format
	u8  *data;				//audio data
	u16 len;				//current audio data length, max is 1024 bytes
	int total;				//audio file length, max is 16 * 1024 bytes
	int offset;				//current audio data offset
}mem_audio_data_struct;

/**
 * @brief  内存音频控制
 * @param  [in] {ctrl} 1 -- 添加; 2 -- 删除; 3 -- 播放; 4 -- 停止; 5 -- 查询
 * @param  [in] {audio_id} 音频id
 * @param  [in] {port} 1 -- 喇叭; 2 -- 上行链路
 * @param  [in] {audio_data} 音频数据结构体
 * @return Null
 * @note   MCU需要自行实现该功能
            并非所有模块都支持此功能，有关详细信息，请咨询产品经理。
 */
void mcu_contrl_cellular_mem_audio(u8 ctrl, u8 audio_id, u8 port, mem_audio_data_struct *audio_data);
#endif

#ifdef SUPPORT_ONLINE_TTS
/**
 * @brief  在线tts控制
 * @param  [in] {ctrl} 1 -- 播放; 2 -- 查询; 
 * @param  [in] {context} tts文本，仅支持utf-8编码
 * @param  [in] {req_timeout} 请求超时时间
 * @return Null
 * @note   MCU需要自行实现该功能
            并非所有模块都支持此功能，有关详细信息，请咨询产品经理。
 */
void mcu_contrl_cellular_tts_audio(u8 ctrl,char *context,u8 req_timeout);
#endif

#ifdef SUPPORT_ADC_CTL
/**
 * @brief  adc初始化
 * @return Null
 * @note   MCU需要自行实现该功能
 */
void mcu_control_cellular_adc_init(u8 port, u16 reference_voltage);
/**
 * @brief  adc读取
 * @param  [in] {read_count} 一次读取个数
 * @return Null
 * @note   MCU需要自行实现该功能
 */
void mcu_control_cellular_adc_read(u8 port, u8 read_count);
/**
 * @brief  adc间隔读取
 * @param  [in] {read_count} 一次读取个数
 * @return Null
 * @note   MCU需要自行实现该功能
 */
void mcu_control_cellular_adc_interval(u8 port, u16 interval, u8 read_count);
/**
 * @brief  adc 去初始化
 * @return Null
 * @note   MCU需要自行实现该功能
 */
void mcu_control_cellular_adc_deinit(u8 port);
#endif

#ifdef SUPPORT_GPIO_CTL
typedef struct 
{	
	u8 	dir;			//gpio方向, 0:输入 1:输出
	u8 	mode;			//0:上拉 1:下拉 2:高阻 4:推挽, 0-3 输入用, 4 输出用  
	u8  level;			//0:低电平 1:高电平
}gpio_init_struct_t;
/**
 * @brief  gpio初始化
 * @param  [in] {para} 初始化参数
 * @return Null
 * @note   MCU需要自行实现该功能
 */
void mcu_control_cellular_gpio_init(u8 pin, gpio_init_struct_t *para);
/**
 * @brief  gpio去初始化
 * @return Null
 * @note   MCU需要自行实现该功能
 */
void mcu_control_cellular_gpio_deinit(u8 pin);
/**
 * @brief  gpio写
 * @return Null
 * @note   MCU需要自行实现该功能
 */
void mcu_control_cellular_gpio_write(u8 pin, u8 level);
/**
 * @brief  gpio读
 * @return Null
 * @note   MCU需要自行实现该功能
 */
void mcu_control_cellular_gpio_read(u8 pin);
/**
 * @brief  gpio定时写
 * @return Null
 * @note   MCU需要自行实现该功能
 */
void mcu_control_cellular_gpio_write_interval(u8 pin, u8 start_level, u16 high_interval, u16 low_interval);
#endif

#endif
