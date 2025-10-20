/**********************************Copyright (c)**********************************
**                       版权所有 (C), 2015-2030, 涂鸦科技
**
**                             http://www.tuya.com
**
*********************************************************************************/
/**
 * @file    mcu_api.c
 * @author  涂鸦综合协议开发组
 * @version v1.0.2
 * @date    2021.6.2
 * @brief   用户需要主动调用的函数都在该文件内
 */

/****************************** 免责声明 ！！！ *******************************
由于MCU类型和编译环境多种多样，所以此代码仅供参考，用户请自行把控最终代码质量，
涂鸦不对MCU功能结果负责。
******************************************************************************/

#define MCU_API_GLOBAL

#include "cellular.h"

extern CELLULAR_CALL_STATUS_E s_phone_call_status;

/**
 * @brief  hex转bcd
 * @param[in] {Value_H} 高字节
 * @param[in] {Value_L} 低字节
 * @return 转换完成后数据
 */
u8 hex_to_bcd(u8 Value_H,u8 Value_L)
{
    u8 bcd_value;

    if((Value_H >= '0') && (Value_H <= '9'))
        Value_H -= '0';
    else if((Value_H >= 'A') && (Value_H <= 'F'))
        Value_H = Value_H - 'A' + 10;
    else if((Value_H >= 'a') && (Value_H <= 'f'))
        Value_H = Value_H - 'a' + 10;

    bcd_value = Value_H & 0x0f;

    bcd_value <<= 4;
    if((Value_L >= '0') && (Value_L <= '9'))
        Value_L -= '0';
    else if((Value_L >= 'A') && (Value_L <= 'F'))
        Value_L = Value_L - 'a' + 10;
    else if((Value_L >= 'a') && (Value_L <= 'f'))
        Value_L = Value_L - 'a' + 10;

    bcd_value |= Value_L & 0x0f;

    return bcd_value;
}

/**
 * @brief  求字符串长度
 * @param[in] {str} 字符串地址
 * @return 数据长度
 */
u32 my_strlen(u8 *str)
{
    u32 len = 0;
    if(str == NULL) {
        return 0;
    }

    for(len = 0; *str ++ != '\0'; ) {
        len ++;
    }

    return len;
}

/**
 * @brief  把src所指内存区域的前count个字节设置成字符c
 * @param[out] {src} 待设置的内存首地址
 * @param[in] {ch} 设置的字符
 * @param[in] {count} 设置的内存长度
 * @return 待设置的内存首地址
 */
void *my_memset(void *src,u8 ch,u16 count)
{
    u8 *tmp = (u8 *)src;

    if(src == NULL) {
        return NULL;
    }

    while(count --) {
        *tmp ++ = ch;
    }

    return src;
}

/**
 * @brief  内存拷贝
 * @param[out] {dest} 目标地址
 * @param[in] {src} 源地址
 * @param[in] {count} 拷贝数据个数
 * @return 数据处理完后的源地址
 */
void *my_memcpy(void *dest, const void *src, u16 count)
{
    u8 *pdest = (u8 *)dest;
    const u8 *psrc  = (const u8 *)src;
    u16 i;

    if(dest == NULL || src == NULL) {
        return NULL;
    }

    if((pdest <= psrc) || (pdest > psrc + count)) {
        for(i = 0; i < count; i ++) {
            pdest[i] = psrc[i];
        }
    }else {
        for(i = count; i > 0; i --) {
            pdest[i - 1] = psrc[i - 1];
        }
    }

    return dest;
}

/**
 * @brief  字符串拷贝
 * @param[in] {dest} 目标地址
 * @param[in] {src} 源地址
 * @return 数据处理完后的源地址
 */
i8 *my_strcpy(i8 *dest, const i8 *src)
{
    if((NULL == dest) || (NULL == src)) {
        return NULL;
    }

    i8 *p = dest;
    while(*src!='\0') {
        *dest++ = *src++;
    }
    *dest = '\0';
    return p;
}

/**
 * @brief  字符串比较
 * @param[in] {s1} 字符串 1
 * @param[in] {s2} 字符串 2
 * @return 大小比较值
 * -         0:s1=s2
 * -         <0:s1<s2
 * -         >0:s1>s2
 */
i32 my_strcmp(i8 *s1 , i8 *s2)
{
    while( *s1 && *s2 && *s1 == *s2 ) {
        s1++;
        s2++;
    }
    return *s1 - *s2;
}

/**
 * @brief  将int类型拆分四个字节
 * @param[in] {number} 4字节原数据
 * @param[out] {value} 处理完成后4字节数据
 * @return Null
 */
void int_to_byte(u32 number,u8 value[4])
{
    value[0] = number >> 24;
    value[1] = number >> 16;
    value[2] = number >> 8;
    value[3] = number & 0xff;
}

/**
 * @brief  将4字节合并为1个32bit变量
 * @param[in] {value} 4字节数组
 * @return 合并完成后的32bit变量
 */
u32 byte_to_int(const u8 value[4])
{
    u32 nubmer = 0;

    nubmer = (u32)value[0];
    nubmer <<= 8;
    nubmer |= (u32)value[1];
    nubmer <<= 8;
    nubmer |= (u32)value[2];
    nubmer <<= 8;
    nubmer |= (u32)value[3];

    return nubmer;
}

/**
 * @brief  raw型dp数据上传
 * @param[in] {dpid} dpid号
 * @param[in] {value} 当前dp值指针
 * @param[in] {len} 数据长度
 * @return Null
 * @note   Null
 */
u8 mcu_dp_raw_update(u8 dpid,const u8 value[],u16 len)
{
    u16 send_len = 0;

    if(stop_update_flag == ENABLE)
        return SUCCESS;
    //
    send_len = set_cellular_uart_byte(send_len,dpid);
    send_len = set_cellular_uart_byte(send_len,DP_TYPE_RAW);
    //
    send_len = set_cellular_uart_byte(send_len,len / 0x100);
    send_len = set_cellular_uart_byte(send_len,len % 0x100);
    //
    send_len = set_cellular_uart_buffer(send_len,(u8 *)value,len);

    cellular_uart_write_frame(STATE_UPLOAD_CMD,MCU_TX_VER,send_len);

    return SUCCESS;
}

/**
 * @brief  bool型dp数据上传
 * @param[in] {dpid} dpid号
 * @param[in] {value} 当前dp值
 * @return Null
 * @note   Null
 */
u8 mcu_dp_bool_update(u8 dpid,u8 value)
{
    u16 send_len = 0;

    if(stop_update_flag == ENABLE)
        return SUCCESS;

    send_len = set_cellular_uart_byte(send_len,dpid);
    send_len = set_cellular_uart_byte(send_len,DP_TYPE_BOOL);
    //
    send_len = set_cellular_uart_byte(send_len,0);
    send_len = set_cellular_uart_byte(send_len,1);
    //
    if(value == FALSE) {
        send_len = set_cellular_uart_byte(send_len,FALSE);
    }else {
        send_len = set_cellular_uart_byte(send_len,1);
    }

    cellular_uart_write_frame(STATE_UPLOAD_CMD, MCU_TX_VER, send_len);

    return SUCCESS;
}

/**
 * @brief  value型dp数据上传
 * @param[in] {dpid} dpid号
 * @param[in] {value} 当前dp值
 * @return Null
 * @note   Null
 */
u8 mcu_dp_value_update(u8 dpid,u32 value)
{
    u16 send_len = 0;

    if(stop_update_flag == ENABLE)
        return SUCCESS;

    send_len = set_cellular_uart_byte(send_len,dpid);
    send_len = set_cellular_uart_byte(send_len,DP_TYPE_VALUE);
    //
    send_len = set_cellular_uart_byte(send_len,0);
    send_len = set_cellular_uart_byte(send_len,4);
    //
    send_len = set_cellular_uart_byte(send_len,value >> 24);
    send_len = set_cellular_uart_byte(send_len,value >> 16);
    send_len = set_cellular_uart_byte(send_len,value >> 8);
    send_len = set_cellular_uart_byte(send_len,value & 0xff);

    cellular_uart_write_frame(STATE_UPLOAD_CMD,MCU_TX_VER,send_len);

    return SUCCESS;
}

/**
 * @brief  string型dp数据上传
 * @param[in] {dpid} dpid号
 * @param[in] {value} 当前dp值指针
 * @param[in] {len} 数据长度
 * @return Null
 * @note   Null
 */
u8 mcu_dp_string_update(u8 dpid,const u8 value[],u16 len)
{
    u16 send_len = 0;

    if(stop_update_flag == ENABLE)
        return SUCCESS;
    //
    send_len = set_cellular_uart_byte(send_len,dpid);
    send_len = set_cellular_uart_byte(send_len,DP_TYPE_STRING);
    //
    send_len = set_cellular_uart_byte(send_len,len / 0x100);
    send_len = set_cellular_uart_byte(send_len,len % 0x100);
    //
    send_len = set_cellular_uart_buffer(send_len,(u8 *)value,len);

    cellular_uart_write_frame(STATE_UPLOAD_CMD,MCU_TX_VER,send_len);

    return SUCCESS;
}

/**
 * @brief  enum型dp数据上传
 * @param[in] {dpid} dpid号
 * @param[in] {value} 当前dp值
 * @return Null
 * @note   Null
 */
u8 mcu_dp_enum_update(u8 dpid,u8 value)
{
    u16 send_len = 0;

    if(stop_update_flag == ENABLE)
        return SUCCESS;

    send_len = set_cellular_uart_byte(send_len,dpid);
    send_len = set_cellular_uart_byte(send_len,DP_TYPE_ENUM);
    //
    send_len = set_cellular_uart_byte(send_len,0);
    send_len = set_cellular_uart_byte(send_len,1);
    //
    send_len = set_cellular_uart_byte(send_len,value);

    cellular_uart_write_frame(STATE_UPLOAD_CMD,MCU_TX_VER,send_len);

    return SUCCESS;
}

/**
 * @brief  fault型dp数据上传
 * @param[in] {dpid} dpid号
 * @param[in] {value} 当前dp值
 * @return Null
 * @note   Null
 */
u8 mcu_dp_fault_update(u8 dpid,u32 value)
{
    u16 send_len = 0;

    if(stop_update_flag == ENABLE)
        return SUCCESS;

    send_len = set_cellular_uart_byte(send_len,dpid);
    send_len = set_cellular_uart_byte(send_len,DP_TYPE_BITMAP);
    //
    send_len = set_cellular_uart_byte(send_len,0);

    if((value | 0xff) == 0xff) {
        send_len = set_cellular_uart_byte(send_len,1);
        send_len = set_cellular_uart_byte(send_len,value);
    }else if((value | 0xffff) == 0xffff) {
        send_len = set_cellular_uart_byte(send_len,2);
        send_len = set_cellular_uart_byte(send_len,value >> 8);
        send_len = set_cellular_uart_byte(send_len,value & 0xff);
    }else {
        send_len = set_cellular_uart_byte(send_len,4);
        send_len = set_cellular_uart_byte(send_len,value >> 24);
        send_len = set_cellular_uart_byte(send_len,value >> 16);
        send_len = set_cellular_uart_byte(send_len,value >> 8);
        send_len = set_cellular_uart_byte(send_len,value & 0xff);
    }

    cellular_uart_write_frame(STATE_UPLOAD_CMD, MCU_TX_VER, send_len);

    return SUCCESS;
}

#ifdef MCU_DP_UPLOAD_SYN
/**
 * @brief  raw型dp数据同步上传
 * @param[in] {dpid} dpid号
 * @param[in] {value} 当前dp值指针
 * @param[in] {len} 数据长度
 * @return Null
 * @note   Null
 */
u8 mcu_dp_raw_update_syn(u8 dpid,const u8 value[],u16 len)
{
    u16 send_len = 0;

    if(stop_update_flag == ENABLE)
        return SUCCESS;
    //
    send_len = set_cellular_uart_byte(send_len,dpid);
    send_len = set_cellular_uart_byte(send_len,DP_TYPE_RAW);
    //
    send_len = set_cellular_uart_byte(send_len,len / 0x100);
    send_len = set_cellular_uart_byte(send_len,len % 0x100);
    //
    send_len = set_cellular_uart_buffer(send_len,(u8 *)value,len);

    cellular_uart_write_frame(STATE_UPLOAD_SYN_CMD,MCU_TX_VER,send_len);

    return SUCCESS;
}

/**
 * @brief  bool型dp数据同步上传
 * @param[in] {dpid} dpid号
 * @param[in] {value} 当前dp值指针
 * @return Null
 * @note   Null
 */
u8 mcu_dp_bool_update_syn(u8 dpid,u8 value)
{
    u16 send_len = 0;

    if(stop_update_flag == ENABLE)
        return SUCCESS;

    send_len = set_cellular_uart_byte(send_len,dpid);
    send_len = set_cellular_uart_byte(send_len,DP_TYPE_BOOL);
    //
    send_len = set_cellular_uart_byte(send_len,0);
    send_len = set_cellular_uart_byte(send_len,1);
    //
    if(value == FALSE) {
        send_len = set_cellular_uart_byte(send_len,FALSE);
    }else {
        send_len = set_cellular_uart_byte(send_len,1);
    }

    cellular_uart_write_frame(STATE_UPLOAD_SYN_CMD, MCU_TX_VER, send_len);

    return SUCCESS;
}

/**
 * @brief  value型dp数据同步上传
 * @param[in] {dpid} dpid号
 * @param[in] {value} 当前dp值指针
 * @return Null
 * @note   Null
 */
u8 mcu_dp_value_update_syn(u8 dpid,u32 value)
{
    u16 send_len = 0;

    if(stop_update_flag == ENABLE)
        return SUCCESS;

    send_len = set_cellular_uart_byte(send_len,dpid);
    send_len = set_cellular_uart_byte(send_len,DP_TYPE_VALUE);
    //
    send_len = set_cellular_uart_byte(send_len,0);
    send_len = set_cellular_uart_byte(send_len,4);
    //
    send_len = set_cellular_uart_byte(send_len,value >> 24);
    send_len = set_cellular_uart_byte(send_len,value >> 16);
    send_len = set_cellular_uart_byte(send_len,value >> 8);
    send_len = set_cellular_uart_byte(send_len,value & 0xff);

    cellular_uart_write_frame(STATE_UPLOAD_SYN_CMD, MCU_TX_VER, send_len);

    return SUCCESS;
}

/**
 * @brief  string型dp数据同步上传
 * @param[in] {dpid} dpid号
 * @param[in] {value} 当前dp值指针
 * @param[in] {len} 数据长度
 * @return Null
 * @note   Null
 */
u8 mcu_dp_string_update_syn(u8 dpid,const u8 value[],u16 len)
{
    u16 send_len = 0;

    if(stop_update_flag == ENABLE)
        return SUCCESS;
    //
    send_len = set_cellular_uart_byte(send_len,dpid);
    send_len = set_cellular_uart_byte(send_len,DP_TYPE_STRING);
    //
    send_len = set_cellular_uart_byte(send_len,len / 0x100);
    send_len = set_cellular_uart_byte(send_len,len % 0x100);
    //
    send_len = set_cellular_uart_buffer(send_len,(u8 *)value,len);

    cellular_uart_write_frame(STATE_UPLOAD_SYN_CMD,MCU_TX_VER,send_len);

    return SUCCESS;
}

/**
 * @brief  enum型dp数据同步上传
 * @param[in] {dpid} dpid号
 * @param[in] {value} 当前dp值指针
 * @return Null
 * @note   Null
 */
u8 mcu_dp_enum_update_syn(u8 dpid,u8 value)
{
    u16 send_len = 0;

    if(stop_update_flag == ENABLE)
        return SUCCESS;

    send_len = set_cellular_uart_byte(send_len,dpid);
    send_len = set_cellular_uart_byte(send_len,DP_TYPE_ENUM);
    //
    send_len = set_cellular_uart_byte(send_len,0);
    send_len = set_cellular_uart_byte(send_len,1);

    send_len = set_cellular_uart_byte(send_len,value);

    cellular_uart_write_frame(STATE_UPLOAD_SYN_CMD,MCU_TX_VER,send_len);

    return SUCCESS;
}

/**
 * @brief  fault型dp数据同步上传
 * @param[in] {dpid} dpid号
 * @param[in] {value} 当前dp值指针
 * @return Null
 * @note   Null
 */
u8 mcu_dp_fault_update_syn(u8 dpid,u32 value)
{
    u16 send_len = 0;

    if(stop_update_flag == ENABLE)
        return SUCCESS;

    send_len = set_cellular_uart_byte(send_len,dpid);
    send_len = set_cellular_uart_byte(send_len,DP_TYPE_BITMAP);
    //
    send_len = set_cellular_uart_byte(send_len,0);

    if((value | 0xff) == 0xff) {
        send_len = set_cellular_uart_byte(send_len,1);
        send_len = set_cellular_uart_byte(send_len,value);
    }else if((value | 0xffff) == 0xffff) {
        send_len = set_cellular_uart_byte(send_len,2);
        send_len = set_cellular_uart_byte(send_len,value >> 8);
        send_len = set_cellular_uart_byte(send_len,value & 0xff);
    }else {
        send_len = set_cellular_uart_byte(send_len,4);
        send_len = set_cellular_uart_byte(send_len,value >> 24);
        send_len = set_cellular_uart_byte(send_len,value >> 16);
        send_len = set_cellular_uart_byte(send_len,value >> 8);
        send_len = set_cellular_uart_byte(send_len,value & 0xff);
    }

    cellular_uart_write_frame(STATE_UPLOAD_SYN_CMD,MCU_TX_VER,send_len);

    return SUCCESS;
}
#endif

#ifdef MCU_DP_UPLOAD_SYN_WITH_TIMESTAMP
/**
 * @brief  记录型raw型dp数据同步上传
 * @param[in] {dpid} dpid号
 * @param[in] {value} 当前dp值指针
 * @param[in] {len} 数据长度
 * @param[in] {timestamp}时间（Data[0]：是否带本地时间标志位：
0表示这条数据不带MCU给的时间，后面的时间模块认为数据无效不处理
1表示后面的时间数据有效，时间数据为设备所在的当地时间；
2表示后面的时间数据有效，时间数据为格林时间；
Data[1]为年份,  0x00表示2000年
Data[2]为月份，从1开始到12结束
Data[3]为日期，从1开始到31结束
Data[4]为时钟，从0开始到23结束
Data[5]为分钟，从0开始到59结束
Data[6]为秒钟，从0开始到15结束 ）
 * @return Null
 * @note   Null
 */
u8 mcu_dp_raw_update_syn_timestamp(u8 dpid,const u8 value[],u16 len,TIMESTAMP_T timestamp)
{
    u16 send_len = 0;

    if(stop_update_flag == ENABLE)
        return SUCCESS;
    //

    send_len = set_cellular_uart_buffer(send_len,timestamp.time,sizeof(timestamp.time));

    send_len = set_cellular_uart_byte(send_len,dpid);
    send_len = set_cellular_uart_byte(send_len,DP_TYPE_RAW);
    //
    send_len = set_cellular_uart_byte(send_len,len / 0x100);
    send_len = set_cellular_uart_byte(send_len,len % 0x100);
    //
    send_len = set_cellular_uart_buffer(send_len,(u8 *)value,len);

    cellular_uart_write_frame(DATA_RPT_WITH_TIME_CMD,MCU_TX_VER,send_len);

    return SUCCESS;
}

/**
 * @brief  记录型bool型dp数据同步上传
 * @param[in] {dpid} dpid号
 * @param[in] {value} 当前dp值指针
 * @param[in] {timestamp}时间
 * @return Null
 * @note   Null
 */
u8 mcu_dp_bool_update_syn_timestamp(u8 dpid,u8 value,TIMESTAMP_T timestamp)
{
    u16 send_len = 0;

    if(stop_update_flag == ENABLE)
        return SUCCESS;

    send_len = set_cellular_uart_buffer(send_len,timestamp.time,sizeof(timestamp.time));

    send_len = set_cellular_uart_byte(send_len,dpid);
    send_len = set_cellular_uart_byte(send_len,DP_TYPE_BOOL);
    //
    send_len = set_cellular_uart_byte(send_len,0);
    send_len = set_cellular_uart_byte(send_len,1);
    //
    if(value == FALSE) {
        send_len = set_cellular_uart_byte(send_len,FALSE);
    }else {
        send_len = set_cellular_uart_byte(send_len,1);
    }

    cellular_uart_write_frame(DATA_RPT_WITH_TIME_CMD, MCU_TX_VER, send_len);

    return SUCCESS;
}

/**
 * @brief  记录型value型dp数据同步上传
 * @param[in] {dpid} dpid号
 * @param[in] {value} 当前dp值指针
 * @param[in] {timestamp}时间
 * @return Null
 * @note   Null
 */
u8 mcu_dp_value_update_syn_timestamp(u8 dpid,u32 value,TIMESTAMP_T timestamp)
{
    u16 send_len = 0;

    if(stop_update_flag == ENABLE)
        return SUCCESS;

    send_len = set_cellular_uart_buffer(send_len,timestamp.time,sizeof(timestamp.time));
    send_len = set_cellular_uart_byte(send_len,dpid);
    send_len = set_cellular_uart_byte(send_len,DP_TYPE_VALUE);
    //
    send_len = set_cellular_uart_byte(send_len,0);
    send_len = set_cellular_uart_byte(send_len,4);
    //
    send_len = set_cellular_uart_byte(send_len,value >> 24);
    send_len = set_cellular_uart_byte(send_len,value >> 16);
    send_len = set_cellular_uart_byte(send_len,value >> 8);
    send_len = set_cellular_uart_byte(send_len,value & 0xff);

    cellular_uart_write_frame(DATA_RPT_WITH_TIME_CMD, MCU_TX_VER, send_len);

    return SUCCESS;
}

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
{
    u16 send_len = 0;

    if(stop_update_flag == ENABLE)
        return SUCCESS;
    send_len = set_cellular_uart_buffer(send_len,timestamp.time,sizeof(timestamp.time));
    //
    send_len = set_cellular_uart_byte(send_len,dpid);
    send_len = set_cellular_uart_byte(send_len,DP_TYPE_STRING);
    //
    send_len = set_cellular_uart_byte(send_len,len / 0x100);
    send_len = set_cellular_uart_byte(send_len,len % 0x100);
    //
    send_len = set_cellular_uart_buffer(send_len,(u8 *)value,len);

    cellular_uart_write_frame(DATA_RPT_WITH_TIME_CMD,MCU_TX_VER,send_len);

    return SUCCESS;
}

/**
 * @brief  记录型enum型dp数据同步上传
 * @param[in] {dpid} dpid号
 * @param[in] {value} 当前dp值指针
 * @param[in] {timestamp}时间
 * @return Null
 * @note   Null
 */
u8 mcu_dp_enum_update_syn_timestamp(u8 dpid,u8 value,TIMESTAMP_T timestamp)
{
    u16 send_len = 0;

    if(stop_update_flag == ENABLE)
        return SUCCESS;

    send_len = set_cellular_uart_buffer(send_len,timestamp.time,sizeof(timestamp.time));
    send_len = set_cellular_uart_byte(send_len,dpid);
    send_len = set_cellular_uart_byte(send_len,DP_TYPE_ENUM);
    //
    send_len = set_cellular_uart_byte(send_len,0);
    send_len = set_cellular_uart_byte(send_len,1);

    send_len = set_cellular_uart_byte(send_len,value);

    cellular_uart_write_frame(DATA_RPT_WITH_TIME_CMD,MCU_TX_VER,send_len);

    return SUCCESS;
}

/**
 * @brief  记录型fault型dp数据同步上传
 * @param[in] {dpid} dpid号
 * @param[in] {value} 当前dp值指针
 * @param[in] {timestamp}时间
 * @return Null
 * @note   Null
 */
u8 mcu_dp_fault_update_syn_timestamp(u8 dpid,u32 value,TIMESTAMP_T timestamp)
{
    u16 send_len = 0;

    if(stop_update_flag == ENABLE)
        return SUCCESS;

    send_len = set_cellular_uart_buffer(send_len,timestamp.time,sizeof(timestamp.time));
    send_len = set_cellular_uart_byte(send_len,dpid);
    send_len = set_cellular_uart_byte(send_len,DP_TYPE_BITMAP);
    //
    send_len = set_cellular_uart_byte(send_len,0);

    if((value | 0xff) == 0xff) {
        send_len = set_cellular_uart_byte(send_len,1);
        send_len = set_cellular_uart_byte(send_len,value);
    }else if((value | 0xffff) == 0xffff) {
        send_len = set_cellular_uart_byte(send_len,2);
        send_len = set_cellular_uart_byte(send_len,value >> 8);
        send_len = set_cellular_uart_byte(send_len,value & 0xff);
    }else {
        send_len = set_cellular_uart_byte(send_len,4);
        send_len = set_cellular_uart_byte(send_len,value >> 24);
        send_len = set_cellular_uart_byte(send_len,value >> 16);
        send_len = set_cellular_uart_byte(send_len,value >> 8);
        send_len = set_cellular_uart_byte(send_len,value & 0xff);
    }

    cellular_uart_write_frame(DATA_RPT_WITH_TIME_CMD,MCU_TX_VER,send_len);

    return SUCCESS;
}
#endif
/**
 * @brief  mcu获取bool型下发dp值
 * @param[in] {value} dp数据缓冲区地址
 * @param[in] {len} dp数据长度
 * @return 当前dp值
 * @note   Null
 */
u8 mcu_get_dp_download_bool(const u8 value[],u16 len)
{
    return(value[0]);
}

/**
 * @brief  mcu获取enum型下发dp值
 * @param[in] {value} dp数据缓冲区地址
 * @param[in] {len} dp数据长度
 * @return 当前dp值
 * @note   Null
 */
u8 mcu_get_dp_download_enum(const u8 value[],u16 len)
{
    return(value[0]);
}

/**
 * @brief  mcu获取value型下发dp值
 * @param[in] {value} dp数据缓冲区地址
 * @param[in] {len} dp数据长度
 * @return 当前dp值
 * @note   Null
 */
u32 mcu_get_dp_download_value(const u8 value[],u16 len)
{
    return(byte_to_int(value));
}

/**
 * @brief  串口接收数据暂存处理
 * @param[in] {value} 串口收到的1字节数据
 * @return Null
 * @note   在MCU串口处理函数中调用该函数,并将接收到的数据作为参数传入
 */
void uart_receive_input(u8 value)
{
    //#error "请在收到多个字节的串口数据时调用此函数,串口数据由MCU_SDK处理,用户请勿再另行处理,完成后删除该行"
    if(1 == rx_buf_out - rx_buf_in) {
        //串口接收缓存已满
    }else if((rx_buf_in > rx_buf_out) && ((rx_buf_in - rx_buf_out) >= sizeof(cellular_uart_rx_buf))) {
        //串口接收缓存已满
    }else {
        //串口接收缓存未满
        if(rx_buf_in >= (u8 *)(cellular_uart_rx_buf + sizeof(cellular_uart_rx_buf))) {
            rx_buf_in = (u8 *)(cellular_uart_rx_buf);
        }

        *rx_buf_in ++ = value;
    }
}

/**
 * @brief  串口接收多个字节数据暂存处理
 * @param[in] {value} 串口要接收的数据的源地址
 * @param[in] {data_len} 串口要接收的数据的数据长度
 * @return Null
 * @note   如需要支持一次多字节缓存，可调用该函数
 */
void uart_receive_buff_input(u8 value[], u16 data_len)
{
//    #error "请在需要一次缓存多个字节串口数据处调用此函数,串口数据由MCU_SDK处理,用户请勿再另行处理,完成后删除该行"
    u16 i = 0;
    for(i = 0; i < data_len; i++) {
        uart_receive_input(value[i]);
    }
}

/**
 * @brief  蜂窝设备串口数据处理服务
 * @param  Null
 * @return Null
 * @note   在MCU主函数while循环中调用该函数
 */
void cellular_uart_service(void)
{
    static u16 rx_in = 0;
    u16 offset = 0;
    u8 frame_type = 0;
    u16 rx_value_len = 0;
	
    while((rx_in < sizeof(cellular_data_process_buf)) && with_data_rxbuff() > 0) {
        cellular_data_process_buf[rx_in ++] = take_byte_rxbuff();
    }

    if(rx_in < PROTOCOL_HEAD) {
        return;
    }

    while((rx_in - offset) >= PROTOCOL_HEAD) {
        if(cellular_data_process_buf[offset + HEAD_FIRST] != FRAME_FIRST) {
            offset ++;
            continue;
        }

        if(cellular_data_process_buf[offset + HEAD_SECOND] != FRAME_SECOND) {
            offset ++;
            continue;
        }

#ifdef MCU_OTA_PROTOCL_V1
        frame_type = cellular_data_process_buf[offset + FRAME_TYPE];
        if(frame_type == UPDATE_START_CMD || frame_type == UPDATE_TRANS_CMD) {
            if(cellular_data_process_buf[offset + PROTOCOL_VERSION] != 1) {
                offset += 2;
                continue;
            }
        } else 
#endif
        {
            if(cellular_data_process_buf[offset + PROTOCOL_VERSION] != MCU_RX_VER) {
                offset += 2;
                continue;
            }
        }
        
        rx_value_len = cellular_data_process_buf[offset + LENGTH_HIGH] * 0x100;
        rx_value_len += (cellular_data_process_buf[offset + LENGTH_LOW] + PROTOCOL_HEAD);
        if(rx_value_len > sizeof(cellular_data_process_buf) + PROTOCOL_HEAD) {
            offset += 3;
            continue;
        }

        if((rx_in - offset) < rx_value_len) {
            break;
        }

        //数据接收完成
        if(get_check_sum((u8 *)cellular_data_process_buf + offset,rx_value_len - 1) != cellular_data_process_buf[offset + rx_value_len - 1]) {
            //校验出错
            //printf("crc error (crc:0x%X  but data:0x%X)\r\n",get_check_sum((u8 *)cellular_data_process_buf + offset,rx_value_len - 1),cellular_data_process_buf[offset + rx_value_len - 1]);
            offset += 3;
            continue;
        }

        data_handle(offset);
        offset += rx_value_len;
    }//end while

    rx_in -= offset;
    if(rx_in > 0) {
        my_memcpy((i8 *)cellular_data_process_buf, (const i8 *)cellular_data_process_buf + offset, rx_in);
    }
}

/**
 * @brief  协议串口初始化函数
 * @param  Null
 * @return Null
 * @note   在MCU初始化代码中调用该函数
 */
void cellular_protocol_init(void)
{
    //#error " 请在main函数中添加cellular_protocol_init()完成协议初始化,并删除该行"
    rx_buf_in = (u8 *)cellular_uart_rx_buf;
    rx_buf_out = (u8 *)cellular_uart_rx_buf;

    stop_update_flag = DISABLE;
#ifdef LOCK_SERIVCE_ENABLE
    is_setpswd_base = FALSE;
#endif
#ifndef CELLULAR_CONTROL_SELF_MODE
    cellular_work_state = CELLULAR_SATE_UNKNOW;
#endif
}

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
u8 mcu_get_reset_celluluar_flag(void)
{
    return reset_cellular_flag;
}

/**
 * @brief  MCU主动重置蜂窝设备激活状态
 * @param  Null
 * @return Null
 * @note   1:MCU主动调用,通过mcu_get_reset_celluluar_flag()函数获取重置蜂窝设备是否成功
 *         2:如果为模块自处理模式,MCU无须调用该函数
 */
void mcu_reset_cellular(void)
{
    reset_cellular_flag = RESET_CELLULAR_ERROR;

    cellular_uart_write_frame(DEACTIVE_CELLULAR_NET, MCU_TX_VER, 0);
}

/**
 * @brief  获取设置蜂窝设备工作模式状态成功标志
 * @param  Null
 * @return 蜂窝mode flag
 * -           0(SET_CELLULARCONFIG_ERROR):失败
 * -           1(SET_CELLULARCONFIG_SUCCESS):成功
 * @note   1:MCU主动调用mcu_set_cellular_mode()后调用该函数获取复位状态
 *         2:如果为模块自处理模式,MCU无须调用该函数
 */
u8 mcu_get_cellular_mode_flag(void)
{
    return set_cellularmode_flag;
}

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
void mcu_set_cellular_mode(u8 mode)
{
    u8 length = 0;

    set_cellularmode_flag = SET_CELLULARCONFIG_ERROR;

    length = set_cellular_uart_byte(length, mode);

    cellular_uart_write_frame(SET_CELLULAR_WORK_MODE, MCU_TX_VER, length);
}

/**
 * @brief  MCU主动获取当前蜂窝设备工作状态
 * @param  Null
 * @return 蜂窝 work state
 * -          0: SIM卡未连接
 * -          1: 搜索网络中
 * -          2: 已成功注册未联网
 * -          3: 联网成功并获取到IP
 * -          4: 已连接到云端
 * -          0xff: 未知状态
 * @note   如果为模块自处理模式,MCU无须调用该函数
 */
u8 mcu_get_cellular_work_state(void)
{
    return cellular_work_state;
}
#endif

#ifdef SUPPORT_GREEN_TIME
/**
 * @brief  MCU获取格林时间
 * @param  Null
 * @return Null
 * @note   MCU需要自行调用该功能
 */
void mcu_get_green_time(void)
{
    cellular_uart_write_frame(GET_ONLINE_TIME_CMD, MCU_TX_VER, 0);
}
#endif

#ifdef SUPPORT_MCU_RTC_CHECK
/**
 * @brief  MCU获取系统时间,用于校对本地时钟
 * @param  Null
 * @return Null
 * @note   MCU主动调用完成后在mcu_write_rtctime函数内校对rtc时钟
 */
void mcu_get_system_time(void)
{
    cellular_uart_write_frame(GET_LOCAL_TIME_CMD, MCU_TX_VER, 0);
}
#endif

#ifdef CELLULAR_HEARTSTOP_ENABLE
/**
 * @brief  通知蜂窝模组关闭心跳
 * @param  Null
 * @return Null
 * @note   MCU需要自行调用该功能
 */
void cellular_heart_stop(void)
{
    cellular_uart_write_frame(HEAT_BEAT_STOP, MCU_TX_VER, 0);
}
#endif

#ifdef GET_CELLULAR_STATUS_ENABLE
/**
 * @brief  获取当前蜂窝模组联网状态
 * @param  Null
 * @return Null
 * @note   MCU需要自行调用该功能
 */
void mcu_get_cellular_connect_status(void)
{
    cellular_uart_write_frame(GET_CELLULAR_STATE_CMD, MCU_TX_VER, 0);
}
#endif

#ifdef GET_MODULE_MAC_ENABLE
/**
 * @brief  获取蜂窝模块蓝牙MAC
 * @param  Null
 * @return Null
 * @note   MCU需要自行调用该功能
 */
void mcu_get_module_mac(void)
{
    cellular_uart_write_frame(GET_MAC_CMD, MCU_TX_VER, 0);
}
#endif

#ifdef GET_MODULE_REMAIN_MEMORY_ENABLE
/**
 * @brief  获取 蜂窝 模块剩余内存
 * @param  Null
 * @return Null
 * @note   MCU需要自行调用该功能
 */
void get_module_remain_memory(void)
{
    cellular_uart_write_frame(GET_MODULE_REMAIN_MEMORY_CMD, MCU_TX_VER, 0);
}
#endif

#ifdef GET_CELLULAR_RSSI_ENABLE
/**
 * @brief  获取当前蜂窝设备信号强度
 * @param  Null
 * @return Null
 * @note   MCU需要自行调用该功能
 */
void mcu_get_cellular_rssi(void)
{
    cellular_uart_write_frame(GET_CELLULAR_RSSI, MCU_TX_VER, 0);
}
#endif

#ifdef SET_FEATURE_TEST_ENABLE
/**
 * @brief  开始蜂窝设备自检功能
 * @param  Null
 * @return Null
 * @note   MCU需要自行调用该功能
 */
void mcu_set_feature_test(void)
{
    cellular_uart_write_frame(GET_FEATURE_TEST_CMD, MCU_TX_VER, 0);
}
#endif

#ifdef CELLULAR_SERVICE_ENABLE
/**
 * @brief  获取蜂窝设备的工作模式
 * @param  Null
 * @return Null
 * @note   MCU需要自行实现该功能
 */
void mcu_get_cellular_work_mode(void)
{
    u16 length = 0;
    length = set_cellular_uart_byte(length, GET_CELLULAR_WORK_MODE);
    cellular_uart_write_frame(GET_CELLULAR_CMD, MCU_TX_VER, length);
}

/**
 * @brief  获取国际移动用户识别码
 * @param  Null
 * @return Null
 * @note   MCU需要自行实现该功能
 */
void mcu_get_IMSI(void)
{
    u16 length = 0;
    length = set_cellular_uart_byte(length, GET_CELLULAR_IMSI);
    cellular_uart_write_frame(GET_CELLULAR_CMD, MCU_TX_VER, length);
}

/**
 * @brief  获取 SIM 卡识别码
 * @param  Null
 * @return Null
 * @note   MCU需要自行实现该功能
 */
void mcu_get_IMCI(void)
{
    u16 length = 0;
    length = set_cellular_uart_byte(length, GET_CELLULAR_IMCI);
    cellular_uart_write_frame(GET_CELLULAR_CMD, MCU_TX_VER, length);
}

/**
 * @brief  获取设备 IMEI
 * @param  Null
 * @return Null
 * @note   MCU需要自行实现该功能
 */
void mcu_get_IMEI(void)
{
    u16 length = 0;
    length = set_cellular_uart_byte(length, GET_CELLULAR_IMEI);
    cellular_uart_write_frame(GET_CELLULAR_CMD, MCU_TX_VER, length);
}
#ifdef GNSS_SERIVCE_ENABLE
/**
 * @brief  获取GNSS 定位信息
 * @param  Null
 * @return Null
 * @note   MCU需要自行实现该功能
 */
void mcu_get_gnss_location(void)
{
    u16 length = 0;
    length = set_cellular_uart_byte(length, GET_CELLULAR_GPS_LOCATION);
    cellular_uart_write_frame(GET_CELLULAR_CMD, MCU_TX_VER, length);
}
/**
 * @brief  获取GNSS信号强度
 * @param  Null
 * @return Null
 * @note   MCU需要自行实现该功能
 */
void mcu_get_gnss_rssi(void)
{
    u16 length = 0;
    length = set_cellular_uart_byte(length, GET_CELLULAR_GPS_SNR);
    cellular_uart_write_frame(GET_CELLULAR_CMD, MCU_TX_VER, length);
}
/**
 * @brief  获取GNSS 当前速度
 * @param  Null
 * @return Null
 * @note   MCU需要自行实现该功能
 */
void mcu_get_gnss_speed(void)
{
    u16 length = 0;
    length = set_cellular_uart_byte(length, GET_CELLULAR_GPS_SPEED);
    cellular_uart_write_frame(GET_CELLULAR_CMD, MCU_TX_VER, length);
}
/**
 * @brief  获取GNSS当前航向角
 * @param  Null
 * @return Null
 * @note   MCU需要自行实现该功能
 */
void mcu_get_gnss_course(void)
{
    u16 length = 0;
    length = set_cellular_uart_byte(length, GET_CELLULAR_GPS_COURSE);
    cellular_uart_write_frame(GET_CELLULAR_CMD, MCU_TX_VER, length);
}
/**
 * @brief  获取GNSS当前水平精度因子
 * @param  Null
 * @return Null
 * @note   MCU需要自行实现该功能
 */
void mcu_get_gnss_hdop(void)
{
    u16 length = 0;
    length = set_cellular_uart_byte(length, GET_CELLULAR_GPS_HDOP);
    cellular_uart_write_frame(GET_CELLULAR_CMD, MCU_TX_VER, length);
}
/**
 * @brief  获取GNSS当前海拔
 * @param  [in] {enable} true:open,false:close
 * @return Null
 * @note   MCU需要自行实现该功能
 */
void mcu_get_gnss_altitude(void)
{
    u16 length = 0;
    length = set_cellular_uart_byte(length, GET_CELLULAR_GPS_ALTITUDE);
    cellular_uart_write_frame(GET_CELLULAR_CMD, MCU_TX_VER, length);
}
/**
 * @brief  控制GNSS过滤功能
 * @param  {in} enable: 0:disable filter 1:enable filter
 * @return Null
 * @note   MCU需要自行实现该功能
 */
void mcu_ctl_gnss_filter(bool_t enable)
{
    u16 length = 0;
    u8 cmd[2] = {0};
    cmd[0] = CTL_CELLULAR_GPS_FILTER;
    cmd[1] = enable;
    length = set_cellular_uart_buffer(length, cmd, sizeof(cmd));
    cellular_uart_write_frame(SET_CELLULAR_CMD, MCU_TX_VER, length);
}
/**
 * @brief  设置GNSS过滤参数
 * @param  {in} filter_value : 0~99.9
 * @return Null
 * @note   MCU需要自行实现该功能
 */
void mcu_set_gnss_filter(float filter_value)
{
    if(filter_value < 0 || filter_value > 100)
        return;

    u16 length = 0;
    u16 filter = (u16)(filter_value * 10);
    u8 cmd[3] = {0};
    cmd[0] = SET_CELLULAR_GPS_FILTER;
    cmd[1] = filter >> 8;
    cmd[2] = filter & 0xff;
    length = set_cellular_uart_buffer(length, cmd, sizeof(cmd));
    cellular_uart_write_frame(SET_CELLULAR_CMD, MCU_TX_VER, length);
}
/**
 * @brief  获取GNSS当前信息
 * @param  Null
 * @return Null
 * @note   MCU需要自行实现该功能
 */
void mcu_get_gnss_info(void)
{
    u16 length = 0;
    length = set_cellular_uart_byte(length, GET_CELLULAR_GPS_INFO);
    cellular_uart_write_frame(GET_CELLULAR_CMD, MCU_TX_VER, length);
}
/**
 * @brief  控制GNSS服务
 * @param  [in] {enable} true:启动,false:关闭
 * @param  [in] {mode} GNSS定位跟踪类型
 * @return Null
 * @note   MCU需要自行实现该功能
 */
void mcu_control_gnss(bool_t enable,GNSS_ATTACH_MODE_e mode)
{
    u16 length = 0;
    u8 cmd[3] = {0};
    cmd[0] = SET_CELLULAR_CTRL_GNSS;
    cmd[1] = enable;
    cmd[2] = mode;
    length = set_cellular_uart_buffer(length, cmd,sizeof(cmd));
    cellular_uart_write_frame(SET_CELLULAR_CMD, MCU_TX_VER, length);
}

/**
 * @brief  复位GNSS设备
 * @param  [in] {pin} 蜂窝设备连接到GNSS设备RST的PIN脚
 * @return [in] {level} tree:高电平复位，false:低电平复位
 * @note   MCU需要自行实现该功能
 */
void mcu_reset_gnss(u8 pin,bool_t level)
{
    u16 length = 0;
    u8 cmd[3] = {0};
    cmd[0] = SET_CELLULAR_RESET_GNSS;
    cmd[1] = pin;
    cmd[2] = level;
    length = set_cellular_uart_buffer(length, cmd,sizeof(cmd));
    cellular_uart_write_frame(SET_CELLULAR_CMD, MCU_TX_VER, length);
}

/**
 * @brief  控制WIFI定位服务
 * @param  [in] {enable} true:启动,false:关闭
 * @return Null
 * @note   MCU需要自行实现该功能
 */
void mcu_control_wifi_location(bool_t enable)
{
    u16 length = 0;
    u8 cmd[2] = {0};
    cmd[0] = SET_CELLULAR_CTRL_WIFI;
    cmd[1] = enable;
    length = set_cellular_uart_buffer(length, cmd,sizeof(cmd));
    cellular_uart_write_frame(SET_CELLULAR_CMD, MCU_TX_VER, length);
}

/**
 * @brief  获取WIFI定位信息
 * @param  Null
 * @return Null
 * @note   MCU需要自行实现该功能
 */
void mcu_get_wifi_location(void)
{
    u16 length = 0;
    u8 cmd[1] = {0};
    cmd[0] = GET_CELLULAR_WIFI_LOCATION;
    length = set_cellular_uart_buffer(length, cmd,sizeof(cmd));
    cellular_uart_write_frame(GET_CELLULAR_CMD, MCU_TX_VER, length);
}

/**
 * @brief  获取LBS定位信息
 * @param  Null
 * @return Null
 * @note   MCU需要自行实现该功能
 */
void mcu_get_lbs_location(void)
{
    u16 length = 0;
    length = set_cellular_uart_byte(length, GET_CELLULAR_LBS_LOCATION);
    cellular_uart_write_frame(GET_CELLULAR_CMD, MCU_TX_VER, length);
}

/**
 * @brief  mcu设置gnss纬经度上报
 * @param  Null
 * @return Null
 * @note   MCU需要自行实现该功能
 */
void mcu_set_gnss_lat_lg_location(void)
{
    u16 length = 0;
    length = set_cellular_uart_byte(length, GET_CELLULAR_GPS_LOCATION_LAT_LG);
    cellular_uart_write_frame(GET_CELLULAR_CMD, MCU_TX_VER, length);
}

/**
 * @brief  mcu设置gnss自动上报
 * @param  Null
 * @return Null
 * @note   MCU需要自行实现该功能
 */
void mcu_set_cellular_auto_rpt_gps(u16 period, u8 dpid)
{
    u16 length = 0;
    u8 buf[4] = {0};
    buf[0] = SET_CELLULAR_AUTO_RPT_GPS;
    buf[1] = ((period>>8)&0xFF);
    buf[2] = (period&0xFF);
    buf[3] = dpid;
    length = set_cellular_uart_buffer(length, buf, sizeof(buf));
    cellular_uart_write_frame(SET_CELLULAR_CMD, MCU_TX_VER, length);
}

/**
 * @brief  mcu设置wifi自动上报
 * @param  Null
 * @return Null
 * @note   MCU需要自行实现该功能
 */
void mcu_set_cellular_auto_rpt_wifi(u16 period, u8 dpid)
{
    u16 length = 0;
    u8 buf[4] = {0};
    buf[0] = SET_CELLULAR_AUTO_RPT_WIFI;
    buf[1] = ((period>>8)&0xFF);
    buf[2] = (period&0xFF);
    buf[3] = dpid;
    length = set_cellular_uart_buffer(length, buf, sizeof(buf));
    cellular_uart_write_frame(SET_CELLULAR_CMD, MCU_TX_VER, length);
}

/**
 * @brief  mcu设置lbs自动上报
 * @param  Null
 * @return Null
 * @note   MCU需要自行实现该功能
 */
void mcu_set_cellular_auto_rpt_lbs(u16 period, u8 dpid)
{
    u16 length = 0;
    u8 buf[4] = {0};
    buf[0] = SET_CELLULAR_AUTO_RPT_LBS;
    buf[1] = ((period>>8)&0xFF);
    buf[2] = (period&0xFF);
    buf[3] = dpid;
    length = set_cellular_uart_buffer(length, buf, sizeof(buf));
    cellular_uart_write_frame(SET_CELLULAR_CMD, MCU_TX_VER, length);
}

/**
 * @brief  mcu获取各定位功能是否开启
 * @param  Null
 * @return Null
 * @note   MCU需要自行实现该功能
 */
void mcu_get_cellular_location_track_status(void)
{
    u16 length = 0;
    length = set_cellular_uart_byte(length, GET_CELLULAR_LOCATION_TRACK_STATUS);
    cellular_uart_write_frame(GET_CELLULAR_CMD, MCU_TX_VER, length);
}

#if (CELLULAR_MODULE_TYPE == 1)
/**
 * @brief  控制gnss设备主电源开启和关闭
 * @param  [in] {enable} true:启动,false:停止
 * @return Null
 * @note   MCU需要自行实现该功能
 */
void mcu_control_gnss_power(bool_t enable)
{
    u16 length = 0;
    u8 cmd[2] = {0};
    cmd[0] = SET_CELLULAR_GNSS_POWER;
    cmd[1] = enable;
    length = set_cellular_uart_buffer(length, cmd,sizeof(cmd));
    cellular_uart_write_frame(SET_CELLULAR_CMD, MCU_TX_VER, length);
}

/**
 * @brief  获取gnss设备电源状态
 * @param  Null
 * @return Null
 * @note   MCU需要自行实现该功能
 */
void mcu_get_gnss_power_status(void)
{
    u16 length = 0;
    length = set_cellular_uart_byte(length, GET_CELLULAR_GNSS_POWER_STATUS);
    cellular_uart_write_frame(GET_CELLULAR_CMD, MCU_TX_VER, length);
}

#endif
/**
 * @brief  控制AGNSS服务
 * @param  [in] {enable} true:Start,false:Stop
 * @return Null
 * @note   MCU需要自行实现该功能
 */
void mcu_ctl_agnss(bool_t enable)
{
    u16 length = 0;
    u8 cmd[2] = {0};
    cmd[0] = SET_CELLULAR_AGNSS;
    cmd[1] = enable;
    length = set_cellular_uart_buffer(length, cmd, sizeof(cmd));
    cellular_uart_write_frame(SET_CELLULAR_CMD, MCU_TX_VER, length);
}
#endif

#ifdef SUPPORT_CALL
/**
 * @brief  通知蜂窝模组，收到来电
 * @param  Null
 * @return Null
 * @note   MCU需要自行实现该功能
 */
void mcu_control_phone_callin_rsp(void)
{
    u16 length = 0;
    u8 cmd[2] = {0};
    cmd[0] = CONTRL_CELLULAR_PHONE;
    cmd[1] = 0x00;
    length = set_cellular_uart_buffer(length, cmd,sizeof(cmd));
    cellular_uart_write_frame(GET_CELLULAR_CMD, MCU_TX_VER, length);
}

/**
 * @brief  控制蜂窝模组电话呼出
 * @param  [in] {phone_num} 手机号码
 * @param  [in] {phone_num_len} 手机号码长度
 * @return Null
 * @note   MCU需要自行实现该功能
 */
void mcu_control_phone_callout(u8 *phone_num,u32 phone_num_len)
{
    u16 length = 0;
    u8 cmd[44] = {0};
    cmd[0] = CONTRL_CELLULAR_PHONE;
    cmd[1] = 0x01;
    my_memcpy(cmd+2, phone_num, phone_num_len);
    length = set_cellular_uart_buffer(length, cmd,phone_num_len+2);
    cellular_uart_write_frame(GET_CELLULAR_CMD, MCU_TX_VER, length);
}

/**
 * @brief  控制蜂窝模组电话接听
 * @param  Null
 * @return Null
 * @note   MCU需要自行实现该功能
 */
void mcu_control_phone_anwser(void)
{
    u16 length = 0;
    u8 cmd[2] = {0};
    cmd[0] = CONTRL_CELLULAR_PHONE;
    cmd[1] = 0x02;
    length = set_cellular_uart_buffer(length, cmd,sizeof(cmd));
    cellular_uart_write_frame(GET_CELLULAR_CMD, MCU_TX_VER, length);
}

/**
 * @brief  控制蜂窝模组电话挂断
 * @param  Null
 * @return Null
 * @note   MCU需要自行实现该功能
 */
void mcu_control_phone_hungup(void)
{
    u16 length = 0;
    u8 cmd[2] = {0};
    cmd[0] = CONTRL_CELLULAR_PHONE;
    cmd[1] = 0x03;
    length = set_cellular_uart_buffer(length, cmd,sizeof(cmd));
    cellular_uart_write_frame(GET_CELLULAR_CMD, MCU_TX_VER, length);
}

/**
 * @brief  获取蜂窝模组电话状态
 * @param  Null
 * @return Null
 * @note   MCU需要自行实现该功能
 */
void mcu_get_phone_status(void)
{
    u16 length = 0;
    u8 cmd[2] = {0};
    cmd[0] = CONTRL_CELLULAR_PHONE;
    cmd[1] = 0x04;
    length = set_cellular_uart_buffer(length, cmd,sizeof(cmd));
    cellular_uart_write_frame(GET_CELLULAR_CMD, MCU_TX_VER, length);
}


/**
 * @brief  发送dtmf信息
 * @param  [in] {dtmf_data} dtmf音频值
 * @return Null
 * @note   MCU需要自行实现该功能
 */
void mcu_send_dtmf(u8 dtmf_data)
{
    u16 length = 0;
    if (s_phone_call_status != CELLULAR_PHONE_CALLING) {
        return;
    }

    u8 cmd[2] = {0};
    cmd[0] = SET_CELLULAR_DTMF_DATA;
    cmd[1] = dtmf_data;
    length = set_cellular_uart_buffer(length, cmd,sizeof(cmd));
    cellular_uart_write_frame(SET_CELLULAR_CMD, MCU_TX_VER, length);
}

/**
 * @brief  DTMF监听控制
 * @param  [in] {enable} 0：关闭监听，1：打开监听
 * @return Null
 * @note   MCU需要自行实现该功能
 */
void mcu_ctl_dtmf_monitor(u8 enable)
{
    u16 length = 0;
    u8 cmd[2] = {0};
    cmd[0] = SET_CELLULAR_DTMF_CTRL;
    cmd[1] = enable;
    length = set_cellular_uart_buffer(length, cmd,sizeof(cmd));
    cellular_uart_write_frame(SET_CELLULAR_CMD, MCU_TX_VER, length);
}
#endif

#ifdef SUPPORT_SMS
/**
 * @brief  控制接收短信是否成功
 * @param  [in] {result} true:成功,false:失败
 * @return Null
 * @note   MCU需要自行实现该功能
 */
void mcu_control_recv_sms_rsp(bool_t result)
{
    u16 length = 0;
    u8 cmd[3] = {0};
    cmd[0] = CONTRL_CELLULAR_SMS;
    cmd[1] = 0x00;
    cmd[2] = result;
    length = set_cellular_uart_buffer(length, cmd,sizeof(cmd));
    cellular_uart_write_frame(GET_CELLULAR_CMD, MCU_TX_VER, length);
}

/**
 * @brief  控制蜂窝模组发送短信
 * @param  [in] {sms_ctx} 短信内容
 * @param  [in] {phone_num} 手机号
 * @return Null
 * @note   MCU需要自行实现该功能
 */
void mcu_control_send_sms(u8 *sms_ctx, u8 *phone_num)
{
    u16 length = 0;

    u8 cmd[2] = {0};
    cmd[0] = CONTRL_CELLULAR_SMS;
    cmd[1] = 0x01;
    length = set_cellular_uart_buffer(length, cmd,sizeof(cmd));
    length = set_cellular_uart_buffer(length, "{\"n\":\"", my_strlen("{\"n\":\""));
    length = set_cellular_uart_buffer(length,phone_num,my_strlen(phone_num));
    length = set_cellular_uart_buffer(length, "\",\"c\":\"", my_strlen("\",\"c\":\""));
    length = set_cellular_uart_buffer(length,sms_ctx,my_strlen(sms_ctx));
    length = set_cellular_uart_buffer(length, "\"}", my_strlen("\"}"));

    cellular_uart_write_frame(GET_CELLULAR_CMD, MCU_TX_VER, length);
}
#endif

/**
 * @brief  获取电池电量
 * @param  Null
 * @return Null
 * @note   MCU需要自行实现该功能
 */
void mcu_get_battery(void)
{
    u16 length = 0;
    length = set_cellular_uart_byte(length, GET_CELLULAR_VBAT_VOL);
    cellular_uart_write_frame(GET_CELLULAR_CMD, MCU_TX_VER, length);
}

/**
 * @brief  获取蜂窝模组充电状态
 * @param  Null
 * @return Null
 * @note   MCU需要自行实现该功能
 */
void mcu_get_charging_status(void)
{
    u16 length = 0;
    length = set_cellular_uart_byte(length, GET_CELLULAR_VBAT_STATUS);
    cellular_uart_write_frame(GET_CELLULAR_CMD, MCU_TX_VER, length);
}

/**
 * @brief  控制蜂窝模组音量设置
 * @param  [in] {ctrl_cmd} 1:本地音量 2:通话音量
 * @param  [in] {vol} 十进制：0-100
 * @return Null
 * @note   MCU需要自行实现该功能
 */
void mcu_control_set_volume(u8 ctrl_cmd, u8 vol)
{
    u16 length = 0;
    u8 cmd[3] = {0};
    cmd[0] = SET_CELLULAR_VOLUME;
    cmd[1] = ctrl_cmd;
    cmd[2] = vol;
    length = set_cellular_uart_buffer(length, cmd,sizeof(cmd));
    cellular_uart_write_frame(SET_CELLULAR_CMD, MCU_TX_VER, length);
}

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
                            u8 addr_len)
{
    u16 length = 0;

    u8 cmd[256] = {0};
    cmd[0] = CONTRL_CELLULAR_AUDIO_PLAY;
    cmd[1] = port;
    cmd[2] = play_cmd;
    cmd[3] = format;

    my_memcpy(cmd+4, addr, addr_len);
    length = set_cellular_uart_buffer(length, cmd,addr_len+4);
    cellular_uart_write_frame(GET_CELLULAR_CMD, MCU_TX_VER, length);
}

/**
 * @brief  获取本地音频播放状态
 * @param  Null
 * @return Null
 * @note   MCU需要自行实现该功能
 */
void mcu_get_audio_play_status(void)
{
    u16 length = 0;
    length = set_cellular_uart_byte(length, GET_CELLULAR_VOICE_PLAY_STATUS);
    cellular_uart_write_frame(GET_CELLULAR_CMD, MCU_TX_VER, length);
}

/**
 * @brief  设置低电量关机功能开启或关闭
 * @param  Null
 * @return Null
 * @note   MCU需要自行实现该功能
 */
void mcu_ctrl_low_vol_power(bool_t enable)
{
    u16 length = 0;
    u8 cmd[2] = {0};
    cmd[0] = SET_CELLULAR_CTRL_LOW_VOL_POWER;
    cmd[1] = enable;
    length = set_cellular_uart_buffer(length, cmd,sizeof(cmd));
    cellular_uart_write_frame(SET_CELLULAR_CMD, MCU_TX_VER, length);
}

/**
 * @brief  设置蓝牙连接功能开启或关闭
 * @param  Null
 * @return Null
 * @note   MCU需要自行实现该功能
 */
void mcu_ctrl_ble_conn(bool_t enable)
{
    u16 length = 0;
    u8 cmd[2] = {0};
    cmd[0] = SET_CELLULAR_CTRL_BLE_CONN;
    cmd[1] = enable;
    length = set_cellular_uart_buffer(length, cmd,sizeof(cmd));
    cellular_uart_write_frame(SET_CELLULAR_CMD, MCU_TX_VER, length);
}

/**
 * @brief  设置VoLTE功能开启或关闭
 * @param  [in] {enable} 0:关闭 1:打开
 * @return Null
 * @note   MCU需要自行实现该功能
 */
void mcu_ctrl_volte(bool_t enable)
{
    u16 length = 0;
    u8 cmd[2] = {0};
    cmd[0] = SET_CELLULAR_CTRL_VOLTE;
    cmd[1] = enable;
    length = set_cellular_uart_buffer(length, cmd,sizeof(cmd));
    cellular_uart_write_frame(SET_CELLULAR_CMD, MCU_TX_VER, length);
}
/**
 * @brief  启动或者关闭模块的静音模式
 * @param  [in] {type} 0: sms, 1: call in
 * @param  [in] {enable} 0: 打开, 1: 关闭
 * @return Null
 * @note   MCU需要自行实现该功能
 */
void mcu_ctrl_mute(u8 type,bool_t enable)
{
    u16 length = 0;
    u8 cmd[3] = {0};
    cmd[0] = SET_CELLULAR_CTRL_MUTE;
    cmd[1] = type;
    cmd[2] = enable;
    length = set_cellular_uart_buffer(length, cmd,sizeof(cmd));
    cellular_uart_write_frame(SET_CELLULAR_CMD, MCU_TX_VER, length);
}

/**
 * @brief  设置模块无法驻网成功的连续时间后重启
 * @param  [in] {duration} 连续无法驻网时间，单位秒
 * @return Null
 * @note   MCU需要自行实现该功能
 */
void mcu_set_offline_duraion(u16 duration)
{
    u16 length = 0;
    u8 cmd[3] = {0};
    cmd[0] = SET_CELLULAR_OFFLINE_TIME;
    cmd[1] = (duration & 0xff00) >> 8;
    cmd[2] = (duration & 0xff);
    length = set_cellular_uart_buffer(length, cmd,sizeof(cmd));
    cellular_uart_write_frame(SET_CELLULAR_CMD, MCU_TX_VER, length);
}
#endif

#ifdef LOCK_SERIVCE_ENABLE
/**
 * @brief  MCU获取unix时区
 * @param  Null
 * @return Null
 * @note   MCU需要自行调用该功能
 */
void mcu_get_unix_time_zone(void)
{
    cellular_uart_write_frame(GET_UNIX_TIME_ZONE, MCU_TX_VER, 0);
}

/**
 * @brief  MCU请求获取云端临时密码(带schedule列表)
 * @param  Null
 * @return Null
 * @note   MCU主动调用完成后在 get_schedule_temp_pass_handle 函数内可获取结果
 */
void mcu_get_schedule_temp_pass(void)
{
    cellular_uart_write_frame(GET_SCHEDULE_PASS, MCU_TX_VER, 0);
}

/**
 * @brief  设置密码进制服务
 * @param[in] {pswd_num} 连续的密码数字按键范围
 * @param[in] {start} 密码数字开始的数值
 * @return Null
 * @note   MCU需要自行调用后，可在get_set_psw_base_result函数中对结果进行处理
 */
void mcu_set_pswd_base(u8 pswd_num,u8 start)
{
    u16 length = 0;

    u8 cmd[2] = {0};
    cmd[0] = pswd_num;
    cmd[1] = start;

    length = set_cellular_uart_buffer(length, cmd,sizeof(cmd));
    cellular_uart_write_frame(SET_PSW_BASE, MCU_TX_VER, length);
}
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
void mcu_set_offline_dynamic_pswd(u8 green_time[],u8 pw[],u8 pw_len)
{
    u8 length = 0;
    u8 green_time_len = 6;
    length = set_cellular_uart_buffer(length, green_time,green_time_len);
    length = set_cellular_uart_byte(length, pw_len);
    length = set_cellular_uart_buffer(length, pw,pw_len);
    cellular_uart_write_frame(GET_OFFLINE_PASS, MCU_TX_VER, length);
}
#endif
#endif

/**
 * @brief  打开扩展服务的重置通知
 * @param  Null
 * @return Null
 * @note   MCU需要自行调用该功能
 */
void mcu_open_expand_reset(void)
{
    u16 length = 0;
    length = set_cellular_uart_byte(length, EXPAND_OPEN_RESET_SEVER);
    cellular_uart_write_frame(EXPAND_SERVICE_CMD, MCU_TX_VER, length);
}

/**
 * @brief  对模组进行软件重启
 * @return Null
 * @note   MCU需要自行实现该功能
 */
void mcu_ctrl_cellular_reset(void)
{
    u16 length = 0;
    length = set_cellular_uart_byte(length, SET_CELLULAR_RESET_SELF);
    cellular_uart_write_frame(SET_CELLULAR_CMD, MCU_TX_VER, length);
}

/**
 * @brief  模组外部唤醒管脚变化时进行配置。
 * @param  [in] {gpio} GPIO编号，具体和硬件和模组相关
 * @param  [in] {low_level_time} 低电平持续时间（10ms单位，最大250ms)(部分模组无法实现10ms，以实际结果为准)
 * @return Null
 * @note   MCU需要自行实现该功能
 */
void mcu_set_cellular_wakeup_gpio(u8 gpio,u8 low_level_time)
{
    u16 length = 0;
    u8 cmd[3] = {0};
    cmd[0] = SET_CELLULAR_WAKE_MCU_GPIO;
    cmd[1] = gpio;
    cmd[2] = low_level_time;
    length = set_cellular_uart_buffer(length, cmd,sizeof(cmd));
    cellular_uart_write_frame(SET_CELLULAR_CMD, MCU_TX_VER, length);
}

/**
 * @brief  SIM卡热插拔使能控制
 * @param  [in] {enable} 1使能；0关闭
 * @return Null
 * @note   MCU需要自行实现该功能
 */
void mcu_switch_sim_hotplug(u8 enable)
{
    u16 length = 0;
    u8 cmd[2] = {0};
    cmd[0] = SET_CELLULAR_HOTPLUG;
    cmd[1] = enable;
    length = set_cellular_uart_buffer(length, cmd,sizeof(cmd));
    cellular_uart_write_frame(SET_CELLULAR_CMD, MCU_TX_VER, length);
}

/**
 * @brief  获取模组版本信息
 * @return Null
 * @note   MCU需要自行实现该功能
 */
void mcu_get_cellular_ver(void)
{
    u16 length = 0;
    length = set_cellular_uart_byte(length, GET_CELLULAR_VER_INFO);
    cellular_uart_write_frame(GET_CELLULAR_CMD, MCU_TX_VER, length);
}

/**
 * @brief  蜂窝网络类型读取
 * @return Null
 * @note   MCU需要自行实现该功能
 */
void mcu_get_cellular_nettype(void)
{
    u16 length = 0;
    length = set_cellular_uart_byte(length, GET_CELLULAR_NET_TYPE);
    cellular_uart_write_frame(GET_CELLULAR_CMD, MCU_TX_VER, length);
}

/**
 * @brief  设置/查询通话MIC增益
 * @param  [in] {ctrl} 0 -- 查询; 1 -- 设置
 * @param  [in] {gain} 设置值  0~30（0静音）
 * @return Null
 * @note   MCU需要自行实现该功能
 */
void mcu_contrl_cellular_mic_gain(u8 ctrl,u8 gain)
{
    u16 length = 0;
    u8 cmd[3] = {0};
    cmd[0] = CONTRL_CELLULAR_MIC_GAIN;
    cmd[1] = ctrl;
    //设置
    if (ctrl == 1) {
        if (gain > 30) {
            return;
        }
        cmd[2] = gain;
    }
    if (ctrl) {
        length = set_cellular_uart_buffer(length, cmd,sizeof(cmd));
    }
    else {
        length = set_cellular_uart_buffer(length, cmd,2);
    }
    cellular_uart_write_frame(GET_CELLULAR_CMD, MCU_TX_VER, length);
}


/**
 * @brief  设置/查询侧音增益
 * @param  [in] {ctrl} 0 -- 查询; 1 -- 设置
 * @param  [in] {gain} 设置值  0~99（0静音）
 * @return Null
 * @note   MCU需要自行实现该功能
 */
void mcu_contrl_cellular_side_gain(u8 ctrl,u8 gain)
{
    u16 length = 0;
    u8 cmd[3] = {0};
    cmd[0] = CONTRL_CELLULAR_SIDE_GAIN;
    cmd[1] = ctrl;
    //设置
    if (ctrl == 1) {
        if (gain > 99) {
            return;
        }
        cmd[2] = gain;
    }
    if (ctrl) {
        length = set_cellular_uart_buffer(length, cmd,sizeof(cmd));
    }
    else {
        length = set_cellular_uart_buffer(length, cmd,2);
    }
    cellular_uart_write_frame(GET_CELLULAR_CMD, MCU_TX_VER, length);
}

#ifdef SET_RRC_ENABLE
/**
 * @brief  控制模组RRC
 * @param  [in] ctrl, 0:get rrc value, 1:set rrc value
 * @param  [in] rrc value
 * @return Null
 * @note   MCU需要自行实现该功能
 */
void mcu_ctl_cellular_rrc(u8 ctrl, u8 rrc)
{
    u16 length = 0;
    u8 cmd[3] = {0};
    cmd[0] = CONTRL_CELLULAR_SIDE_GAIN;
    cmd[1] = ctrl;
    //设置
    if (ctrl == 1) {
        if (rrc > 20) {
            return;
        }
        cmd[2] = rrc;
    }
    if (ctrl) {
        length = set_cellular_uart_buffer(length, cmd,sizeof(cmd));
    }
    else {
        length = set_cellular_uart_buffer(length, cmd,2);
    }
    cellular_uart_write_frame(GET_CELLULAR_CMD, MCU_TX_VER, length);
}
#endif

#ifdef GET_CELLULAR_PLMN_ENABLE
/**
 * @brief  查询当前的PLMN
 * @return Null
 * @note   MCU需要自行实现该功能
 */
void mcu_get_cellular_plmn(void)
{
    u16 length = 0;
    length = set_cellular_uart_byte(length, GET_CELLULAR_PLMN);
    cellular_uart_write_frame(GET_CELLULAR_CMD, MCU_TX_VER, length);
}
#endif

#ifdef SUPPORT_MEM_AUDIO
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
void mcu_contrl_cellular_mem_audio(u8 ctrl, u8 audio_id, u8 port, mem_audio_data_struct *audio_data)
{
    u16 length = 0;
    u8 cmd[8] = {0};
    cmd[0] = CONTRL_CELLULAR_MEM_PLAY;
    cmd[1] = ctrl;

    if (ctrl == 1) {
        cmd[2] = audio_id; 
        cmd[3] = audio_data->format; 
        cmd[4] = (audio_data->total >> 8);
        cmd[5] = (audio_data->total & 0xff);
        cmd[6] = (audio_data->offset >> 8);
        cmd[7] = (audio_data->offset &0xff);
        length = set_cellular_uart_buffer(length,(u8 *)cmd,sizeof(cmd));
        length = set_cellular_uart_buffer(length,(u8 *)audio_data->data, audio_data->len);
        cellular_uart_write_frame(GET_CELLULAR_CMD, MCU_TX_VER, length);
    }
    else if (ctrl == 2) {
        cmd[2] = audio_id; //ADIO ID
        length = set_cellular_uart_buffer(length,(u8 *)cmd,3);
        cellular_uart_write_frame(GET_CELLULAR_CMD, MCU_TX_VER, length);
    }
    else if (ctrl == 3) {
        cmd[2] = audio_id; //ADIO ID
        cmd[3] = port; 
        length = set_cellular_uart_buffer(length,(u8 *)cmd,4);
        cellular_uart_write_frame(GET_CELLULAR_CMD, MCU_TX_VER, length);
    }
    else if (ctrl == 4) {
        cmd[2] = audio_id; //ADIO ID
        length = set_cellular_uart_buffer(length,(u8 *)cmd,3);
        cellular_uart_write_frame(GET_CELLULAR_CMD, MCU_TX_VER, length);
    }
    else if (ctrl == 5) {
        length = set_cellular_uart_buffer(length,(u8 *)cmd,2);
        cellular_uart_write_frame(GET_CELLULAR_CMD, MCU_TX_VER, length);
    }
}
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
void mcu_contrl_cellular_tts_audio(u8 ctrl,char *context,u8 req_timeout)
{
    u16 length = 0;
    char str_timeout[16] = {0};
    memset((void *)cellular_uart_tx_buf,0,sizeof(cellular_uart_tx_buf));
    length = set_cellular_uart_byte(length,CONTRL_CELLULAR_ONLINE_TTS);
    // play
    if (ctrl == 1){
        length = set_cellular_uart_buffer(length, "{\"opt\":\"play\"", my_strlen("{\"opt\":\"play\""));
        length = set_cellular_uart_buffer(length, ",\"timeout\":", my_strlen(",\"timeout\":"));
        sprintf(str_timeout,"%d",req_timeout);
        length = set_cellular_uart_buffer(length,(u8 *)str_timeout,my_strlen((u8 *)str_timeout));
        length = set_cellular_uart_buffer(length, ",\"context\":\"", my_strlen(",\"context\":\""));
        length = set_cellular_uart_buffer(length,(u8 *)context,my_strlen((u8 *)context));
        length = set_cellular_uart_buffer(length, "\"}", my_strlen("\"}"));
    }
    // query
    else if (ctrl == 2) {
        length = set_cellular_uart_buffer(length, "{\"opt\":\"query\"", my_strlen("{\"opt\":\"query\""));
        length = set_cellular_uart_buffer(length, "}", my_strlen("}"));
    }
    cellular_uart_write_frame(GET_CELLULAR_CMD,MCU_TX_VER,length);
}
#endif

#ifdef SUPPORT_ADC_CTL
/**
 * @brief  adc初始化
 * @return Null
 * @note   MCU需要自行实现该功能
 */
void mcu_control_cellular_adc_init(u8 port, u16 reference_voltage)
{
    u16 length = 0;
    u8 cmd[5] = {0};
    cmd[0] = SET_CELLULAR_ADC;
    cmd[1] = 0;
    cmd[2] = port;
    cmd[3] = reference_voltage >> 8;
    cmd[4] = reference_voltage & 0xff;
    length = set_cellular_uart_buffer(length, cmd, sizeof(cmd));
    cellular_uart_write_frame(SET_CELLULAR_CMD,MCU_TX_VER,length);
}

/**
 * @brief  adc读取
 * @param  [in] {read_count} 一次读取个数
 * @return Null
 * @note   MCU需要自行实现该功能
 */
void mcu_control_cellular_adc_read(u8 port, u8 read_count)
{
    u16 length = 0;
    u8 cmd[4] = {0};
    cmd[0] = SET_CELLULAR_ADC;
    cmd[1] = 1;
    cmd[2] = port;
    cmd[3] = read_count;
    length = set_cellular_uart_buffer(length, cmd, sizeof(cmd));
    cellular_uart_write_frame(SET_CELLULAR_CMD,MCU_TX_VER,length);
}

/**
 * @brief  adc间隔读取
 * @param  [in] {read_count} 一次读取个数
 * @return Null
 * @note   MCU需要自行实现该功能
 */
void mcu_control_cellular_adc_interval(u8 port, u16 interval, u8 read_count)
{
    u16 length = 0;
    u8 cmd[6] = {0};
    cmd[0] = SET_CELLULAR_ADC;
    cmd[1] = 2;
    cmd[2] = port;
    cmd[3] = interval >> 8;
    cmd[4] = interval & 0xff ;
    cmd[5] = read_count;
    length = set_cellular_uart_buffer(length, cmd, sizeof(cmd));
    cellular_uart_write_frame(SET_CELLULAR_CMD,MCU_TX_VER,length);
}

/**
 * @brief  adc 去初始化
 * @return Null
 * @note   MCU需要自行实现该功能
 */
void mcu_control_cellular_adc_deinit(u8 port)
{
    u16 length = 0;
    u8 cmd[3] = {0};
    cmd[0] = SET_CELLULAR_ADC;
    cmd[1] = 4;
    cmd[2] = port;
    length = set_cellular_uart_buffer(length, cmd, sizeof(cmd));
    cellular_uart_write_frame(SET_CELLULAR_CMD,MCU_TX_VER,length);
}
#endif

#ifdef SUPPORT_GPIO_CTL
/**
 * @brief  gpio初始化
 * @param  [in] {para} 初始化参数
 * @return Null
 * @note   MCU需要自行实现该功能
 */
void mcu_control_cellular_gpio_init(u8 pin, gpio_init_struct_t *para)
{
    u16 length = 0;
    u8 cmd[6] = {0};
    cmd[0] = SET_CELLULAR_GPIO;
    cmd[1] = 0;
    cmd[2] = pin;
    cmd[3] = para->dir;
    cmd[4] = para->mode;
    cmd[5] = para->level;
    length = set_cellular_uart_buffer(length, cmd, sizeof(cmd));
    cellular_uart_write_frame(SET_CELLULAR_CMD,MCU_TX_VER,length);
}
/**
 * @brief  gpio去初始化
 * @return Null
 * @note   MCU需要自行实现该功能
 */
void mcu_control_cellular_gpio_deinit(u8 pin)
{
    u16 length = 0;
    u8 cmd[3] = {0};
    cmd[0] = SET_CELLULAR_GPIO;
    cmd[1] = 1;
    cmd[2] = pin;
    length = set_cellular_uart_buffer(length, cmd, sizeof(cmd));
    cellular_uart_write_frame(SET_CELLULAR_CMD,MCU_TX_VER,length);
}
/**
 * @brief  gpio写
 * @return Null
 * @note   MCU需要自行实现该功能
 */
void mcu_control_cellular_gpio_write(u8 pin, u8 level)
{
    u16 length = 0;
    u8 cmd[4] = {0};
    cmd[0] = SET_CELLULAR_GPIO;
    cmd[1] = 2;
    cmd[2] = pin;
    cmd[3] = level;
    length = set_cellular_uart_buffer(length, cmd, sizeof(cmd));
    cellular_uart_write_frame(SET_CELLULAR_CMD,MCU_TX_VER,length);
}
/**
 * @brief  gpio读
 * @return Null
 * @note   MCU需要自行实现该功能
 */
void mcu_control_cellular_gpio_read(u8 pin)
{
    u16 length = 0;
    u8 cmd[3] = {0};
    cmd[0] = SET_CELLULAR_GPIO;
    cmd[1] = 3;
    cmd[2] = pin;
    length = set_cellular_uart_buffer(length, cmd, sizeof(cmd));
    cellular_uart_write_frame(SET_CELLULAR_CMD,MCU_TX_VER,length);
}
/**
 * @brief  gpio定时写
 * @return Null
 * @note   MCU需要自行实现该功能
 */
void mcu_control_cellular_gpio_write_interval(u8 pin, u8 start_level, u16 high_interval, u16 low_interval)
{
    u16 length = 0;
    u8 cmd[8] = {0};
    cmd[0] = SET_CELLULAR_GPIO;
    cmd[1] = 4;
    cmd[2] = pin;
    cmd[3] = start_level;
    cmd[4] = high_interval >> 8;
    cmd[5] = high_interval & 0xff;
    cmd[6] = low_interval >> 8;
    cmd[7] = low_interval & 0xff;
    length = set_cellular_uart_buffer(length, cmd, sizeof(cmd));
    cellular_uart_write_frame(SET_CELLULAR_CMD,MCU_TX_VER,length);
}
#endif
