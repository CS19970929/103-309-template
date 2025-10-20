/**********************************Copyright (c)**********************************
**                       版权所有 (C), 2015-2030, 涂鸦科技
**
**                             http://www.tuya.com
**
*********************************************************************************
**
 * @file    system.c
 * @author  涂鸦综合协议开发组
 * @version v1.0.2
 * @date    2021.6.2
 * @brief   串口数据处理，用户无需关心该文件实现内容
**/
/****************************** 免责声明 ！！！ *******************************
由于MCU类型和编译环境多种多样，所以此代码仅供参考，用户请自行把控最终代码质量，
涂鸦不对MCU功能结果负责。
******************************************************************************/

#define SYSTEM_GLOBAL

#include "cellular.h"

extern const DOWNLOAD_CMD_S download_cmd[];
#ifdef LOCK_SERIVCE_ENABLE
/**
 * @brief  获取是否设置过密码进制服务的标志
 * @param  无
 * @return 是否设置过密码进制服务的标志
 */
u8 get_setpassword_base_flag(void)
{
    return is_setpswd_base;
}
#endif
/**
 * @brief  写cellular_uart字节
 * @param[in] {dest} 缓存区其实地址
 * @param[in] {byte} 写入字节值
 * @return 写入完成后的总长度
 */
u16 set_cellular_uart_byte(u16 dest, u8 byte)
{
    u8 *obj = (u8 *)cellular_uart_tx_buf + DATA_START + dest;

    *obj = byte;
    dest += 1;

    return dest;
}

/**
 * @brief  写cellular_uart_buffer
 * @param[in] {dest} 目标地址
 * @param[in] {src} 源地址
 * @param[in] {len} 数据长度
 * @return 写入结束的缓存地址
 */
u16 set_cellular_uart_buffer(u16 dest, const u8 *src, u16 len)
{
    u8 *obj = (u8 *)cellular_uart_tx_buf + DATA_START + dest;

    my_memcpy(obj,src,len);

    dest += len;
    return dest;
}

/**
 * @brief  计算校验和
 * @param[in] {pack} 数据源指针
 * @param[in] {pack_len} 计算校验和长度
 * @return 校验和
 */
u8 get_check_sum(u8 *pack, u16 pack_len)
{
    u16 i;
    u8 check_sum = 0;

    for(i = 0; i < pack_len; i ++) {
        check_sum += *pack ++;
    }

    return check_sum;
}

/**
 * @brief  串口发送一段数据
 * @param[in] {in} 发送缓存指针
 * @param[in] {len} 数据发送长度
 * @return Null
 */
static void cellular_uart_write_data(u8 *in, u16 len)
{
    if((NULL == in) || (0 == len)) {
        return;
    }
    while(len --) {
        uart_transmit_output(*in);
        in ++;
    }
}

/**
 * @brief  向cellular串口发送一帧数据
 * @param[in] {fr_type} 帧类型
 * @param[in] {fr_ver} 帧版本
 * @param[in] {len} 数据长度
 * @return Null
 */
void cellular_uart_write_frame(u8 fr_type, u8 fr_ver, u16 len)
{
    u8 check_sum = 0;

    cellular_uart_tx_buf[HEAD_FIRST] = FRAME_FIRST;
    cellular_uart_tx_buf[HEAD_SECOND] = FRAME_SECOND;
    cellular_uart_tx_buf[PROTOCOL_VERSION] = fr_ver;
    cellular_uart_tx_buf[FRAME_TYPE] = fr_type;
    cellular_uart_tx_buf[LENGTH_HIGH] = len >> 8;
    cellular_uart_tx_buf[LENGTH_LOW] = len & 0xff;

    len += PROTOCOL_HEAD;
    check_sum = get_check_sum((u8 *)cellular_uart_tx_buf, len - 1);
    cellular_uart_tx_buf[len - 1] = check_sum;

    cellular_uart_write_data((u8 *)cellular_uart_tx_buf, len);
}

/**
 * @brief  心跳包检测
 * @param  Null
 * @return Null
 */
static void heat_beat_check(void)
{
    u8 length = 0;
    static u8 mcu_reset_state = FALSE;

    if(FALSE == mcu_reset_state) {
        length = set_cellular_uart_byte(length, FALSE);
        mcu_reset_state = TRUE;
    }else {
        length = set_cellular_uart_byte(length, TRUE);
    }

    cellular_uart_write_frame(HEAT_BEAT_CMD, MCU_TX_VER, length);
}

#ifdef DP_ACK
static void dp_ack_respond(u8 result)
{
    u8 length = 0;
    length = set_cellular_uart_byte(length, result);
    cellular_uart_write_frame(DATA_QUERT_CMD, MCU_TX_VER, length);
}
#endif

/**
 * @brief  产品信息上传
 * @param  Null
 * @return Null
 */
static void product_info_update(void)
{
    u8 length = 0;
    memset((void *)cellular_uart_tx_buf,0,sizeof(cellular_uart_tx_buf));
    length = set_cellular_uart_buffer(length, "{\"p\":\"", my_strlen("{\"p\":\""));
    length = set_cellular_uart_buffer(length,(u8 *)PRODUCT_KEY,my_strlen((u8 *)PRODUCT_KEY));
    length = set_cellular_uart_buffer(length, "\",\"v\":\"", my_strlen("\",\"v\":\""));
    length = set_cellular_uart_buffer(length,(u8 *)MCU_VER,my_strlen((u8 *)MCU_VER));
#ifdef SET_APN_SERVICE_ENABLE
    length = set_cellular_uart_buffer(length, "\",\"apn\":\"", my_strlen("\",\"apn\":\""));
    length = set_cellular_uart_buffer(length,(u8 *)APN_VER,my_strlen((u8 *)APN_VER));
#endif
    length = set_cellular_uart_buffer(length, "\",\"m\":", my_strlen("\",\"m\":"));
    length = set_cellular_uart_buffer(length, (u8 *)MODULE_POWER, my_strlen((u8 *)MODULE_POWER));
#ifdef SET_ALIVE_SERIVCE_ENABLE
    length = set_cellular_uart_buffer(length, ",\"mht\":", my_strlen(",\"mht\":"));
    length = set_cellular_uart_buffer(length,(u8 *)MHT_INTERNAL,my_strlen((u8 *)MHT_INTERNAL));
#endif
#ifdef SUPPORT_SHORTURL_GET
    length = set_cellular_uart_buffer(length, ",\"qr\":", my_strlen(",\"qr\":"));
    length = set_cellular_uart_buffer(length,(u8 *)QR_ENABLE,my_strlen((u8 *)QR_ENABLE));
#endif

#ifdef SLAVE_SET
    length = set_cellular_uart_buffer(length, ",\"slave\":", my_strlen(",\"slave\":"));
    length = set_cellular_uart_buffer(length,(u8 *)SLAVE_SET,my_strlen((u8 *)SLAVE_SET));
#endif

#ifdef DP_CACHE
    length = set_cellular_uart_buffer(length, ",\"dp_cache\":", my_strlen(",\"dp_cache\":"));
    length = set_cellular_uart_buffer(length,(u8 *)DP_CACHE,my_strlen((u8 *)DP_CACHE));
#endif

#ifdef SET_COPS
    length = set_cellular_uart_buffer(length, ",\"cops\":\"", my_strlen(",\"cops\":\""));
    length = set_cellular_uart_buffer(length,(u8 *)SET_COPS,my_strlen((u8 *)SET_COPS));
    length = set_cellular_uart_buffer(length, "\"", my_strlen("\""));
#endif

#ifdef DP_ACK
    length = set_cellular_uart_buffer(length, ",\"dp_ack\":", my_strlen(",\"dp_ack\":"));
    length = set_cellular_uart_buffer(length,(u8 *)DP_ACK,my_strlen((u8 *)DP_ACK));
#endif

#ifdef PRE_ENV
    length = set_cellular_uart_buffer(length, ",\"env_pre\":", my_strlen(",\"env_pre\":"));
    length = set_cellular_uart_buffer(length,(u8 *)PRE_ENV,my_strlen((u8 *)PRE_ENV));
#endif

#ifdef GPS_TYPE
    length = set_cellular_uart_buffer(length, ",\"gps\":", my_strlen(",\"gps\":"));
    length = set_cellular_uart_buffer(length,(u8 *)GPS_TYPE,my_strlen((u8 *)GPS_TYPE));
#endif

#ifdef NET_TYPE
    length = set_cellular_uart_buffer(length, ",\"net_type\":", my_strlen(",\"net_type\":"));
    length = set_cellular_uart_buffer(length,(u8 *)NET_TYPE,my_strlen((u8 *)NET_TYPE));
#endif

#ifdef MCU_OTA_PROTOCL_V1
    length = set_cellular_uart_buffer(length, ",\"U\":", my_strlen(",\"U\":"));
    length = set_cellular_uart_buffer(length,(u8 *)MCU_OTA_PROTOCL_V1,my_strlen((u8 *)MCU_OTA_PROTOCL_V1));
#endif
#ifdef SUPPORT_ONLINE_TTS 
    u8 tts_value = 1;
    length = set_cellular_uart_buffer(length, ",\"online_tts\":", my_strlen(",\"online_tts\":"));
    length = set_cellular_uart_buffer(length,&tts_value, 1);
#endif 
#ifdef SUPPORT_EXT_OTA
    length = set_cellular_uart_buffer(length, ",\"ext_ota\":[", my_strlen(",\"ext_ota\":["));
    length = set_cellular_uart_buffer(length, "{", my_strlen("{"));
    length = set_cellular_uart_buffer(length, "\"ch\":", my_strlen("\"ch\":"));
    length = set_cellular_uart_buffer(length,(u8 *)EX_DEVICE1_OTA_CH,my_strlen((u8 *)EX_DEVICE1_OTA_CH));
    length = set_cellular_uart_buffer(length, ",\"v\":\"", my_strlen(",\"v\":\""));
    length = set_cellular_uart_buffer(length,(u8 *)EX_DEVICE1_OTA_CH_VER,my_strlen((u8 *)EX_DEVICE1_OTA_CH_VER));
    length = set_cellular_uart_buffer(length, "\"}", my_strlen("\"}"));

    length = set_cellular_uart_buffer(length, ",{", my_strlen(",{"));
    length = set_cellular_uart_buffer(length, "\"ch\":", my_strlen("\"ch\":"));
    length = set_cellular_uart_buffer(length,(u8 *)EX_DEVICE2_OTA_CH,my_strlen((u8 *)EX_DEVICE2_OTA_CH));
    length = set_cellular_uart_buffer(length, ",\"v\":\"", my_strlen(",\"v\":\""));
    length = set_cellular_uart_buffer(length,(u8 *)EX_DEVICE2_OTA_CH_VER,my_strlen((u8 *)EX_DEVICE2_OTA_CH_VER));
    length = set_cellular_uart_buffer(length, "\"}", my_strlen("\"}"));
    length = set_cellular_uart_buffer(length, "]", my_strlen("]"));
#endif
    length = set_cellular_uart_buffer(length, "}", my_strlen("}"));

    cellular_uart_write_frame(PRODUCT_INFO_CMD, MCU_TX_VER, length);
}

/**
 * @brief  mcu查询mcu和cellular的工作模式
 * @param  Null
 * @return Null
 */
static void get_mcu_cellular_mode(void)
{
    u8 length = 0;

#ifdef CELLULAR_CONTROL_SELF_MODE                                   //模块自处理
    length = set_cellular_uart_byte(length, WF_STATE_KEY);
    length = set_cellular_uart_byte(length, WF_RESERT_KEY);
#else
    //No need to process data
#endif

    cellular_uart_write_frame(WORK_MODE_CMD, MCU_TX_VER, length);
}

/**
 * @brief  获取制定DPID在数组中的序号
 * @param[in] {dpid} dpid
 * @return dp序号
 */
static u8 get_dowmload_dpid_index(u8 dpid)
{
    u8 index;
    u8 total = get_download_cmd_total();

    for(index = 0; index < total; index ++) {
        if(download_cmd[index].dp_id == dpid) {
            break;
        }
    }

    return index;
}

/**
 * @brief  下发数据处理
 * @param[in] {value} 下发数据源指针
 * @return 返回数据处理结果
 */
static u8 data_point_handle(const u8 value[])
{
    u8 dp_id,index;
    u8 dp_type;
    u8 ret;
    u16 dp_len;

    dp_id = value[0];
    dp_type = value[1];
    dp_len = value[2] * 0x100;
    dp_len += value[3];

    index = get_dowmload_dpid_index(dp_id);

    if(dp_type != download_cmd[index].dp_type) {
        //错误提示
        return FALSE;
    }else {
        ret = dp_download_handle(dp_id,value + 4,dp_len);
    }

    return ret;
}

#ifdef WEATHER_ENABLE
/**
 * @brief  天气数据解析
 * @param[in] {p_data} 接收数据指针
 * @param[in] {data_len} 接收数据长度
 * @return Null
 */
static void weather_data_raw_handle(const u8 p_data[], u16 data_len)
{
    i32 i = 1;
    i32 can_len = 0;
    i8 can[15] = {0};
    i8 day = 0;
    i32 type1 = 0;
    u8 value_string[100] = {0};
    i32 val_cnt = 0;
    i32 val_len = 0;

    if(p_data[0] != 1 || data_len < 1) {
        //接收失败
    }else {
        if(data_len < 4) {
            //数据为空
        }

        while (i < data_len) {
            can_len = p_data[i];

            my_memset(can, '\0', 15);
            my_memcpy(can, p_data + i + 1, can_len - 2);

            day = p_data[i + can_len] - '0';

            type1 = p_data[i + 1 + can_len];
            if(type1 != 0 && type1 != 1) {
                return;
            }

            my_memset(value_string, '\0', 100);
            val_cnt = i + 1 + can_len + 1;
            val_len = p_data[val_cnt];
            if (type1 == 0) { //int32
                weather_data_user_handle(can+2, type1, p_data+val_cnt+1, day);
            }
            else if(type1 == 1) { //string
                my_memcpy(value_string, p_data + val_cnt + 1, val_len);
                weather_data_user_handle(can+2, type1, value_string, day);
            }

            i += 1 + can_len + 1 + 1 + val_len;
        }

        cellular_uart_write_frame(WEATHER_DATA_CMD, 0, 0);
    }
}
#endif


/**
 * @brief  和蜂窝模组的通讯检测，如果和蜂窝模组在未停止心跳的情况下，有45秒没有接收到蜂窝的数据需要考虑重启蜂窝模组
 * @return Null
 */
static void cellular_heatbeat_check_timer(void)
{
	//#error "请自行完成相关代码,并删除该行"
}

// 接收到蜂窝的数据后，对检测定时器进行复位，定时器重新计时
static void cellular_heatbeat_reset_timer(void)
{
	//#error "请自行完成相关代码,并删除该行"
}

/**
 * @brief  数据帧处理
 * @param[in] {offset} 数据起始位
 * @return Null
 */
void data_handle(u16 offset)
{
#ifdef SUPPORT_MCU_FIRM_UPDATE
    u8 *firmware_addr = NULL;
    u32 dp_len;
    u8 firm_flag;                                                    //升级包大小标志
#else
    u16 dp_len;
#endif

#ifdef WEATHER_ENABLE
    static u8 isWoSend = 0;                                          //是否已经打开过天气数据, 0:否  1:是
#endif

    u8 ret;
    u16 i,total_len;
    u8 cmd_type = cellular_data_process_buf[offset + FRAME_TYPE];
    u8 result;
    u8 protocl_ver = cellular_data_process_buf[offset+PROTOCOL_VERSION];

    cellular_heatbeat_reset_timer();
    switch(cmd_type)
    {
        case HEAT_BEAT_CMD:                                     //心跳包
            heat_beat_check();
        break;

        case PRODUCT_INFO_CMD:                                  //产品信息
            product_info_update();
        break;

        case WORK_MODE_CMD:                                     //查询MCU设定的模块工作模式
            get_mcu_cellular_mode();
        break;

#ifndef CELLULAR_CONTROL_SELF_MODE
        case CELLULAR_STATE_CMD:                                    //蜂窝设备工作状态
            cellular_work_state = cellular_data_process_buf[offset + DATA_START];
            cellular_uart_write_frame(CELLULAR_STATE_CMD, MCU_TX_VER, 0);
#ifdef WEATHER_ENABLE
            if((cellular_work_state == CONNECTED_GET_IP || cellular_work_state == DEVICE_WAIT_ACTIVE || cellular_work_state == CONNECTED_CLOUD) && isWoSend == 0) { //当模组连接涂鸦云成功，打开天气数据且仅一次
                mcu_open_weather();
                isWoSend = 1;
            }
#endif
        break;

        case DEACTIVE_CELLULAR_NET:                                    //重置模组的激活状态(返回成功)
            reset_cellular_flag = RESET_CELLULAR_SUCCESS;
        break;

        case SET_CELLULAR_WORK_MODE:                                 //设置模组工作模式
            set_cellularmode_flag = SET_CELLULARCONFIG_SUCCESS;
        break;
#endif

        case DATA_QUERT_CMD:                                    //命令下发
            total_len = (cellular_data_process_buf[offset + LENGTH_HIGH] << 8) | cellular_data_process_buf[offset + LENGTH_LOW];

            for(i = 0;i < total_len; ) {
                dp_len = cellular_data_process_buf[offset + DATA_START + i + 2] * 0x100;
                dp_len += cellular_data_process_buf[offset + DATA_START + i + 3];
                //
                ret = data_point_handle((u8 *)cellular_data_process_buf + offset + DATA_START + i);

                if(SUCCESS == ret) {
                    //成功提示
                }else {
                    //错误提示
                }

                i += (dp_len + 4);
            }
            #ifdef DP_ACK
                dp_ack_respond(1);
            #endif
        break;

        case STATE_QUERY_CMD:                                   //状态查询
            all_data_update();
        break;

#ifdef SUPPORT_MCU_FIRM_UPDATE
        case UPDATE_START_CMD:                                  //升级开始
            //获取升级包大小全局变量
            firm_flag = PACKAGE_SIZE;
            if(firm_flag == 0) {
                firm_size = 256;
            }else if(firm_flag == 1) {
                firm_size = 512;
            }else if(firm_flag == 2) {
                firm_size = 1024;
            }

            firm_length = cellular_data_process_buf[offset + DATA_START];
            firm_length <<= 8;
            firm_length |= cellular_data_process_buf[offset + DATA_START + 1];
            firm_length <<= 8;
            firm_length |= cellular_data_process_buf[offset + DATA_START + 2];
            firm_length <<= 8;
            firm_length |= cellular_data_process_buf[offset + DATA_START + 3];
#ifdef MCU_OTA_PROTOCL_V1
            // 协议版本1

            if (protocl_ver == 1) {
                char md5[32]={0};
                u8   channel;
                char ver[10]={0};
                // MD5可以不使用，因为模组会计算下载的数据的MD5和云端计算的MD5进行比较
                // memcpy(md5,&cellular_data_process_buf[offset + DATA_START + 4],32);
                // 可以通过channel来判断当前的OTA文件的归属
                channel = cellular_data_process_buf[offset + DATA_START + 36];
                snprintf(ver, sizeof(ver),"%02d.%02d.%02d",cellular_data_process_buf[offset + DATA_START + 37],cellular_data_process_buf[offset + DATA_START + 38],
                        cellular_data_process_buf[offset + DATA_START + 39]);
            }
#endif
            upgrade_package_choose(PACKAGE_SIZE);
            firm_update_flag = UPDATE_START_CMD;
        break;

        case UPDATE_TRANS_CMD:                                  //升级传输
            if(firm_update_flag == UPDATE_START_CMD) {
#ifdef MCU_OTA_PROTOCL_V1 
                u8 firm_data_head_len = 5;
#else
                u8 firm_data_head_len = 4;
#endif
                //Stop all data reporting
                stop_update_flag = ENABLE;

                total_len = (cellular_data_process_buf[offset + LENGTH_HIGH] << 8) | cellular_data_process_buf[offset + LENGTH_LOW];
                firmware_addr = (u8 *)cellular_data_process_buf;

#ifdef MCU_OTA_PROTOCL_V1
                u8 ota_channel = cellular_data_process_buf[offset + DATA_START];
                dp_len = cellular_data_process_buf[offset + DATA_START + 1];
                dp_len <<= 8;
                dp_len |= cellular_data_process_buf[offset + DATA_START + 2];
                dp_len <<= 8;
                dp_len |= cellular_data_process_buf[offset + DATA_START + 3];
                dp_len <<= 8;
                dp_len |= cellular_data_process_buf[offset + DATA_START + 4];
#else
                dp_len = cellular_data_process_buf[offset + DATA_START];
                dp_len <<= 8;
                dp_len |= cellular_data_process_buf[offset + DATA_START + 1];
                dp_len <<= 8;
                dp_len |= cellular_data_process_buf[offset + DATA_START + 2];
                dp_len <<= 8;
                dp_len |= cellular_data_process_buf[offset + DATA_START + 3];
#endif
                firmware_addr += (offset + DATA_START + firm_data_head_len);
                if((total_len == firm_data_head_len) && (dp_len == firm_length)) {
                    //The last package
                    ret = mcu_firm_update_handle(firmware_addr,dp_len,0);
                    firm_update_flag = 0;
                }else if((total_len - firm_data_head_len) <= firm_size) {
                    ret = mcu_firm_update_handle(firmware_addr,dp_len,total_len - firm_data_head_len);
                }else {
                    firm_update_flag = 0;
                    ret = ERROR;
                }

                if(ret == SUCCESS) {
                    cellular_uart_write_frame(UPDATE_TRANS_CMD, MCU_TX_VER, 0);
                }
                //恢复一切数据上报
                stop_update_flag = DISABLE;
            }
        break;
#endif

#ifdef SUPPORT_GREEN_TIME
        case GET_ONLINE_TIME_CMD:                               //获取格林时间
            mcu_get_greentime((u8 *)(cellular_data_process_buf + offset + DATA_START));
        break;
#endif

#ifdef SUPPORT_MCU_RTC_CHECK
        case GET_LOCAL_TIME_CMD:                               //获取本地时间
            mcu_write_rtctime((u8 *)(cellular_data_process_buf + offset + DATA_START));
        break;
#endif

#ifdef WEATHER_ENABLE
        case WEATHER_OPEN_CMD:                                  //打开天气服务返回
            weather_open_return_handle(cellular_data_process_buf[offset + DATA_START], cellular_data_process_buf[offset + DATA_START + 1]);
        break;

        case WEATHER_DATA_CMD:                                  //天气数据下发
            total_len = (cellular_data_process_buf[offset + LENGTH_HIGH] << 8) | cellular_data_process_buf[offset + LENGTH_LOW];
            weather_data_raw_handle((u8 *)cellular_data_process_buf + offset + DATA_START, total_len);
        break;
#endif

#ifdef MCU_DP_UPLOAD_SYN
        case STATE_UPLOAD_SYN_RESP_CMD:                              //状态上报（同步）
            get_upload_syn_result(cellular_data_process_buf[offset + DATA_START]);
        break;
#endif

#ifdef MCU_DP_UPLOAD_SYN_WITH_TIMESTAMP
        case DATA_RPT_WITH_TIME_CMD:                              //状态上报（同步）
            get_upload_syn_timestamp_result(cellular_data_process_buf[offset + DATA_START]);
        break;
#endif
#ifdef GET_MODULE_MAC_ENABLE
        case GET_MAC_CMD:                                       //获取模块mac
            mcu_get_mac((u8 *)(cellular_data_process_buf + offset + DATA_START));
        break;
#endif
        case EXPAND_SERVICE_CMD:
            expand_service_process((u8 *)(cellular_data_process_buf + offset + DATA_START));
        break;
#ifdef GET_CELLULAR_STATUS_ENABLE
        case GET_CELLULAR_STATE_CMD:                               //获取当前蜂窝联网状态
            result = cellular_data_process_buf[offset + DATA_START];
            get_cellular_status(result);
        break;
#endif

#ifdef GET_MODULE_REMAIN_MEMORY_ENABLE
        case GET_MODULE_REMAIN_MEMORY_CMD:                      //获取模块内存
            remain_memory_result(cellular_data_process_buf + offset + DATA_START);
        break;
#endif

#ifdef SET_FEATURE_TEST_ENABLE
        case GET_FEATURE_TEST_CMD:{

            get_feature_test_result(cellular_data_process_buf[offset + DATA_START],
                                    cellular_data_process_buf[offset + DATA_START+1],
                                    cellular_data_process_buf[offset + DATA_START+2],
                                    cellular_data_process_buf[offset + DATA_START+3]);
        }

        break;
#endif

#ifdef CELLULAR_SERVICE_ENABLE
#ifdef GET_CELLULAR_RSSI_ENABLE
        case GET_CELLULAR_RSSI:                                //获取当前蜂窝信号强度
            get_cellular_rssi_result(cellular_data_process_buf[offset + DATA_START]);
        break;
#endif
        case GET_CELLULAR_CMD:                                //蜂窝设备服务相关命令
        case SET_CELLULAR_CMD:
            total_len = (cellular_data_process_buf[offset + LENGTH_HIGH] << 8) | cellular_data_process_buf[offset + LENGTH_LOW];
            cellular_service_process(cmd_type,(u8 *)cellular_data_process_buf + offset + DATA_START, total_len);
        break;
#endif
#ifdef LOCK_SERIVCE_ENABLE
        case GET_SCHEDULE_PASS:
        get_schedule_temp_pass_handle((u8 *)cellular_data_process_buf + offset + DATA_START);
        break;
        case GET_OFFLINE_PASS:
        get_offline_dynamic_pswd_result((u8 *)cellular_data_process_buf + offset + DATA_START);
        break;
        case SET_PSW_BASE:
        get_set_psw_base_result(cellular_data_process_buf[offset + DATA_START]);
        break;
        case GET_UNIX_TIME_ZONE:
        get_unix_time_zone((u8 *)cellular_data_process_buf + offset + DATA_START);
        break;
#endif
        default:break;
    }
}

/**
 * @brief  判断串口接收缓存中是否有数据
 * @param  Null
 * @return 是否有数据
 */
u8 with_data_rxbuff(void)
{
    if(rx_buf_in != rx_buf_out)
        return 1;
    else
        return 0;
}

/**
 * @brief  读取队列1字节数据
 * @param  Null
 * @return Read the data
 */
u8 take_byte_rxbuff(void)
{
    u8 value;

    if(rx_buf_out != rx_buf_in) {
        //有数据
        if(rx_buf_out >= (u8 *)(cellular_uart_rx_buf + sizeof(cellular_uart_rx_buf))) {
            //数据已经到末尾
            rx_buf_out = (u8 *)(cellular_uart_rx_buf);
        }

        value = *rx_buf_out ++;
    }

    return value;
}
