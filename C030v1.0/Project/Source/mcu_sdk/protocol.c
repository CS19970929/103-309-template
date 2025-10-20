/**********************************Copyright (c)**********************************
**                       版权所有 (C), 2015-2030, 涂鸦科技
**
**                             http://www.tuya.com
**
*********************************************************************************/
/**
 * @file    protocol.c
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

/******************************************************************************
                                移植须知:
1:MCU必须在while中直接调用mcu_api.c内的cellular_uart_service()函数
2:程序正常初始化完成后,建议不进行关串口中断,如必须关中断,关中断时间必须短,关中断会引起串口数据包丢失
3:请勿在中断/定时器中断内调用上报函数
******************************************************************************/

#include "cellular.h"
#include "adapter_4G.h"

CELLULAR_CALL_STATUS_E s_phone_call_status = CELLULAR_PHONE_IDLE;

#ifdef WEATHER_ENABLE
/**
 * @var    weather_choose
 * @brief  天气数据参数选择数组
 * @note   用户可以自定义需要的参数，注释或者取消注释即可，注意更改
 */
const i8 *weather_choose[WEATHER_CHOOSE_CNT] = {
    "temp",
    "humidity",
    "condition",
    "pm25",
    /*"pressure",
    "realFeel",
    "uvi",
    "tips",
    "windDir",
    "windLevel",
    "windSpeed",
    "sunRise",
    "sunSet",
    "aqi",
    "so2 ",
    "rank",
    "pm10",
    "o3",
    "no2",
    "co",
    "conditionNum",*/
};
#endif


/******************************************************************************
                              第一步:初始化
1:在需要使用到cellular相关文件的文件中include cellular.h"
2:在MCU初始化中调用mcu_api.c文件中的cellular_protocol_init()函数
3:将MCU串口单字节发送函数填入protocol.c文件中uart_transmit_output函数内,并删除#error
4:在MCU串口接收函数中调用mcu_api.c文件内的uart_receive_input函数,并将接收到的字节作为参数传入
5:单片机进入while循环后调用mcu_api.c文件内的cellular_uart_service()函数
******************************************************************************/

/******************************************************************************
                        1:dp数据点序列类型对照表
          **此为自动生成代码,如在开发平台有相关修改请重新下载MCU_SDK**
******************************************************************************/
const DOWNLOAD_CMD_S download_cmd[] =
{
  {DPID_VER, DP_TYPE_VALUE},
  {DPID_SOC, DP_TYPE_VALUE},
  {DPID_FULL_CAP, DP_TYPE_VALUE},
  {DPID_REMAINING_CAP, DP_TYPE_VALUE},
  {DPID_CHARGING_TIME, DP_TYPE_VALUE},
  {DPID_DISCHARGE_TIME, DP_TYPE_VALUE},
  {DPID_VOL, DP_TYPE_VALUE},
  {DPID_CUR, DP_TYPE_VALUE},
  {DPID_TEMP, DP_TYPE_VALUE},
  {DPID_CYCLES, DP_TYPE_VALUE},
  {DPID_SOH, DP_TYPE_VALUE},
  {DPID_FAULT_CODE, DP_TYPE_BITMAP},
  {DPID_NUM_1_VOL, DP_TYPE_VALUE},
  {DPID_NUM_2_VOL, DP_TYPE_VALUE},
  {DPID_NUM_3_VOL, DP_TYPE_VALUE},
  {DPID_NUM_4_VOL, DP_TYPE_VALUE},
  {DPID_NUM_5_VOL, DP_TYPE_VALUE},
  {DPID_NUM_6_VOL, DP_TYPE_VALUE},
  {DPID_NUM_7_VOL, DP_TYPE_VALUE},
  {DPID_NUM_8_VOL, DP_TYPE_VALUE},
  {DPID_NUM_9_VOL, DP_TYPE_VALUE},
  {DPID_NUM_10_VOL, DP_TYPE_VALUE},
  {DPID_NUM_11_VOL, DP_TYPE_VALUE},
  {DPID_NUM_12_VOL, DP_TYPE_VALUE},
};



/******************************************************************************
                           2:串口单字节发送函数
请将MCU串口发送函数填入该函数内,并将接收到的数据作为参数传入串口发送函数
******************************************************************************/

/**
 * @brief  串口发送数据
 * @param[in] {value} 串口要发送的1字节数据
 * @return Null
 */
void uart_transmit_output(u8 value)
{
    //Example:
    //Uart_PutChar(value);                                    //串口发送函数

    SendByte_4G(value);
}

/******************************************************************************
                           第二步:实现具体用户函数
1:APP下发数据处理
2:数据上报处理
******************************************************************************/

/******************************************************************************
                            1:所有数据上报处理
当前函数处理全部数据上报(包括可下发/可上报和只上报)
  需要用户按照实际情况实现:
  1:需要实现可下发/可上报数据点上报
  2:需要实现只上报数据点上报
此函数为MCU内部必须调用
用户也可调用此函数实现全部数据上报
******************************************************************************/

//自动化生成数据上报函数

/**
 * @brief  系统所有dp点信息上传,实现APP和muc数据同步
 * @param  Null
 * @return Null
 * @note   此函数SDK内部需调用，MCU必须实现该函数内数据上报功能，包括只上报和可上报可下发型数据
 */
void all_data_update(void)
{
    #error "请在此处理可下发可上报数据及只上报数据示例,处理完成后删除该行"

    /*
    //此代码为平台自动生成，请按照实际数据修改每个可下发可上报函数和只上报函数
    mcu_dp_value_update(DPID_VER,当前软件硬件版本); //VALUE型数据上报;
    mcu_dp_value_update(DPID_SOC,当前SOC); //VALUE型数据上报;
    mcu_dp_value_update(DPID_FULL_CAP,当前满容量); //VALUE型数据上报;
    mcu_dp_value_update(DPID_REMAINING_CAP,当前剩余容量); //VALUE型数据上报;
    mcu_dp_value_update(DPID_CHARGING_TIME,当前充电时间); //VALUE型数据上报;
    mcu_dp_value_update(DPID_DISCHARGE_TIME,当前放电时间); //VALUE型数据上报;
    mcu_dp_value_update(DPID_VOL,当前总电压); //VALUE型数据上报;
    mcu_dp_value_update(DPID_CUR,当前充放电电流); //VALUE型数据上报;
    mcu_dp_value_update(DPID_TEMP,当前电池温度); //VALUE型数据上报;
    mcu_dp_value_update(DPID_CYCLES,当前循环次数); //VALUE型数据上报;
    mcu_dp_value_update(DPID_SOH,当前健康状态); //VALUE型数据上报;
    mcu_dp_fault_update(DPID_FAULT_CODE,当前故障代码); //故障型数据上报;
    mcu_dp_value_update(DPID_NUM_1_VOL,当前第1节电压); //VALUE型数据上报;
    mcu_dp_value_update(DPID_NUM_2_VOL,当前第2节电压); //VALUE型数据上报;
    mcu_dp_value_update(DPID_NUM_3_VOL,当前第3节电压); //VALUE型数据上报;
    mcu_dp_value_update(DPID_NUM_4_VOL,当前第4节电压); //VALUE型数据上报;
    mcu_dp_value_update(DPID_NUM_5_VOL,当前第5节电压); //VALUE型数据上报;
    mcu_dp_value_update(DPID_NUM_6_VOL,当前第6节电压); //VALUE型数据上报;
    mcu_dp_value_update(DPID_NUM_7_VOL,当前第7节电压); //VALUE型数据上报;
    mcu_dp_value_update(DPID_NUM_8_VOL,当前第8节电压); //VALUE型数据上报;
    mcu_dp_value_update(DPID_NUM_9_VOL,当前第9节电压); //VALUE型数据上报;
    mcu_dp_value_update(DPID_NUM_10_VOL,当前第10节电压); //VALUE型数据上报;
    mcu_dp_value_update(DPID_NUM_11_VOL,当前第11节电压); //VALUE型数据上报;
    mcu_dp_value_update(DPID_NUM_12_VOL,当前第12节电压); //VALUE型数据上报;

    */

    mcu_dp_value_update(DPID_VER,当前软件硬件版本); //VALUE型数据上报;
    mcu_dp_value_update(DPID_SOC,当前SOC); //VALUE型数据上报;
    mcu_dp_value_update(DPID_FULL_CAP,当前满容量); //VALUE型数据上报;
    mcu_dp_value_update(DPID_REMAINING_CAP,当前剩余容量); //VALUE型数据上报;
    mcu_dp_value_update(DPID_CHARGING_TIME,当前充电时间); //VALUE型数据上报;
    mcu_dp_value_update(DPID_DISCHARGE_TIME,当前放电时间); //VALUE型数据上报;
    mcu_dp_value_update(DPID_VOL,当前总电压); //VALUE型数据上报;
    mcu_dp_value_update(DPID_CUR,当前充放电电流); //VALUE型数据上报;
    mcu_dp_value_update(DPID_TEMP,当前电池温度); //VALUE型数据上报;
    mcu_dp_value_update(DPID_CYCLES,当前循环次数); //VALUE型数据上报;
    mcu_dp_value_update(DPID_SOH,当前健康状态); //VALUE型数据上报;
    mcu_dp_fault_update(DPID_FAULT_CODE,当前故障代码); //故障型数据上报;
    mcu_dp_value_update(DPID_NUM_1_VOL,当前第1节电压); //VALUE型数据上报;
    mcu_dp_value_update(DPID_NUM_2_VOL,当前第2节电压); //VALUE型数据上报;
    mcu_dp_value_update(DPID_NUM_3_VOL,当前第3节电压); //VALUE型数据上报;
    mcu_dp_value_update(DPID_NUM_4_VOL,当前第4节电压); //VALUE型数据上报;
    mcu_dp_value_update(DPID_NUM_5_VOL,当前第5节电压); //VALUE型数据上报;
    mcu_dp_value_update(DPID_NUM_6_VOL,当前第6节电压); //VALUE型数据上报;
    mcu_dp_value_update(DPID_NUM_7_VOL,当前第7节电压); //VALUE型数据上报;
    mcu_dp_value_update(DPID_NUM_8_VOL,当前第8节电压); //VALUE型数据上报;
    mcu_dp_value_update(DPID_NUM_9_VOL,当前第9节电压); //VALUE型数据上报;
    mcu_dp_value_update(DPID_NUM_10_VOL,当前第10节电压); //VALUE型数据上报;
    mcu_dp_value_update(DPID_NUM_11_VOL,当前第11节电压); //VALUE型数据上报;
    mcu_dp_value_update(DPID_NUM_12_VOL,当前第12节电压); //VALUE型数据上报;
}


/******************************************************************************
                                WARNING!!!
                            2:所有数据上报处理
自动化代码模板函数,具体请用户自行实现数据处理
******************************************************************************/




/******************************************************************************
                                WARNING!!!
此部分函数用户请勿修改!!
******************************************************************************/

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
u8 dp_download_handle(u8 dpid,const u8 value[], u16 length)
{
    /*********************************
    当前函数处理可下发/可上报数据调用
    具体函数内需要实现下发数据处理
    完成用需要将处理结果反馈至APP端,否则APP会认为下发失败
    ***********************************/
    u8 ret;
    switch(dpid) {


        default:
        break;
    }
    return ret;
}

/**
 * @brief  获取所有dp命令总和
 * @param[in] Null
 * @return 下发命令总和
 * @note   该函数用户不能修改
 */
u8 get_download_cmd_total(void)
{
    return(sizeof(download_cmd) / sizeof(download_cmd[0]));
}


/******************************************************************************
                                WARNING!!!
此代码为SDK内部调用,请按照实际dp数据实现函数内部数据
******************************************************************************/

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
void upgrade_package_choose(u8 package_sz)
{
    #error "请自行实现请自行实现升级包大小选择代码,完成后请删除该行"
    u16 send_len = 0;
    send_len = set_cellular_uart_byte(send_len, package_sz);
    cellular_uart_write_frame(UPDATE_START_CMD, MCU_TX_VER, send_len);
}

/**
 * @brief  MCU进入固件升级模式
 * @param[in] {value} 固件缓冲区
 * @param[in] {position} 当前数据包在于固件位置
 * @param[in] {length} 当前固件包长度(固件包长度为0时,表示固件包发送完成)
 * @return Null
 * @note   MCU需要自行实现该功能
 */
u8 mcu_firm_update_handle(const u8 value[],u32 position,u16 length)
{
    #error "请自行完成MCU固件升级代码,完成后请删除该行"
    if(length == 0) {
        //固件数据发送完成

    }else {
        //固件数据处理

    }

    return SUCCESS;
}
#endif

#ifdef SUPPORT_GREEN_TIME
/**
 * @brief  获取到的格林时间
 * @param[in] {time} 获取到的格林时间数据
 * @return Null
 * @note   MCU需要自行实现该功能
 */
void mcu_get_greentime(u8 time[])
{
    #error "请自行完成相关代码,并删除该行"
    /*
    time[0] 为是否获取时间成功标志，为 0 表示失败，为 1表示成功
    time[1] 为年份，0x00 表示 2000 年
    time[2] 为月份，从 1 开始到12 结束
    time[3] 为日期，从 1 开始到31 结束
    time[4] 为时钟，从 0 开始到23 结束
    time[5] 为分钟，从 0 开始到59 结束
    time[6] 为秒钟，从 0 开始到59 结束
    */
    if(time[0] == 1) {
        //正确接收到蜂窝模块返回的格林数据

    }else {
        //获取格林时间出错,有可能是当前蜂窝模块未联网
    }
}
#endif

#ifdef SUPPORT_MCU_RTC_CHECK
/**
 * @brief  MCU校对本地RTC时钟
 * @param[in] {time} 获取到的格林时间数据
 * @return Null
 * @note   MCU需要自行实现该功能
 */
void mcu_write_rtctime(u8 time[])
{
    #error "请自行完成RTC时钟写入代码,并删除该行"
    /*
    Time[0] 为是否获取时间成功标志，为 0 表示失败，为 1表示成功
    Time[1] 为年份，0x00 表示 2000 年
    Time[2] 为月份，从 1 开始到12 结束
    Time[3] 为日期，从 1 开始到31 结束
    Time[4] 为时钟，从 0 开始到23 结束
    Time[5] 为分钟，从 0 开始到59 结束
    Time[6] 为秒钟，从 0 开始到59 结束
    Time[7] 为星期，从 1 开始到 7 结束，1代表星期一
   */
    if(time[0] == 1) {
        //正确接收到蜂窝模块返回的本地时钟数据

    }else {
        //获取本地时钟数据出错,有可能是当前蜂窝模块未联网
    }
}
#endif

#ifdef WEATHER_ENABLE
/**
* @brief  mcu打开天气服务
 * @param  Null
 * @return Null
 */
void mcu_open_weather(void)
{
    i32 i = 0;
    i8 buffer[13] = {0};
    u8 weather_len = 0;
    u16 send_len = 0;

    weather_len = sizeof(weather_choose) / sizeof(weather_choose[0]);

    for(i=0;i<weather_len;i++) {
        buffer[0] = sprintf((char *)buffer+1,"w.%s",weather_choose[i]);
        send_len = set_cellular_uart_buffer(send_len, (u8 *)buffer, buffer[0]+1);
    }

    #error "请根据提示，自行完善打开天气服务代码，完成后请删除该行"
    /*
    //当获取的参数有和时间有关的参数时(如:日出日落)，需要搭配t.unix或者t.local使用，需要获取的参数数据是按照格林时间还是本地时间
    buffer[0] = sprintf(buffer+1,"t.unix"); //格林时间   或使用  buffer[0] = sprintf(buffer+1,"t.local"); //本地时间
    send_len = set_cellular_uart_buffer(send_len, (u8 *)buffer, buffer[0]+1);
    */

    buffer[0] = sprintf((char *)buffer+1,"w.date.%d",WEATHER_FORECAST_DAYS_NUM);
    send_len = set_cellular_uart_buffer(send_len, (u8 *)buffer, buffer[0]+1);

    cellular_uart_write_frame(WEATHER_OPEN_CMD, MCU_TX_VER, send_len);
}

/**
 * @brief  打开天气功能返回用户自处理函数
 * @param[in] {res} 打开天气功能返回结果
 * @ref       0: 失败
 * @ref       1: 成功
 * @param[in] {err} 错误码
 * @return Null
 * @note   MCU需要自行实现该功能
 */
void weather_open_return_handle(u8 res, u8 err)
{
    #error "请自行完成打开天气功能返回数据处理代码,完成后请删除该行"
    u8 err_num = 0;

    if(res == 1) {
        //打开天气返回成功
    }else if(res == 0) {
        //打开天气返回失败
        //获取错误码
        err_num = err;
    }
}

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
void weather_data_user_handle(i8 *name, u8 type, const u8 *data, i8 day)
{
    #error "这里仅给出示例，请自行完善天气数据处理代码,完成后请删除该行"
    i32 value_int;
    i8 value_string[50];//由于有的参数内容较多，这里默认为50。您可以根据定义的参数，可以适当减少该值

    my_memset(value_string, '\0', 50);

    //首先获取数据类型
    if(type == 0) { //参数是INT型
        value_int = data[0] << 24 | data[1] << 16 | data[2] << 8 | data[3];
    }else if(type == 1) {
        my_strcpy(value_string, (const i8 *)data);
    }

    //注意要根据所选参数类型来获得参数值！！！
    if(my_strcmp(name, "temp") == 0) {
        printf("day:%d temp value is:%d\r\n", day, value_int);          //int 型
    }else if(my_strcmp(name, "humidity") == 0) {
        printf("day:%d humidity value is:%d\r\n", day, value_int);      //int 型
    }else if(my_strcmp(name, "pm25") == 0) {
        printf("day:%d pm25 value is:%d\r\n", day, value_int);          //int 型
    }else if(my_strcmp(name, "condition") == 0) {
        printf("day:%d condition value is:%s\r\n", day, value_string);  //string 型
    }
}
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
void get_upload_syn_result(u8 result)
{
    #error "请自行完成状态同步上报结果代码,并删除该行"

    if(result == 0) {
        //同步上报出错
    }else {
        //同步上报成功
    }
}
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
{
    #error "请自行完成状态同步上报结果代码,并删除该行"

    if(result == 0) {
        //同步上报出错
    }else {
        //同步上报成功
    }
}
#endif

#ifdef GET_CELLULAR_STATUS_ENABLE
/**
 * @brief  获取蜂窝设备 状态结果
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
void get_cellular_status(u8 result)
{
    #error "请自行完成获取 蜂窝设备 状态结果代码,并删除该行"

    switch(result) {
        case 0:
            //蜂窝设备工作状态1
        break;

        case 1:
            //蜂窝设备工作状态2
        break;

        case 2:
            //蜂窝设备工作状态3
        break;

        case 3:
            //蜂窝设备工作状态4
        break;

        case 4:
            //蜂窝设备工作状态5
        break;

        case 5:
            //蜂窝设备工作状态6
        break;

        default:break;
    }
}
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
void mcu_get_mac(u8 mac[])
{
    #error "请自行完成mac获取代码,并删除该行"
    /*
    mac[0]为是否获取mac成功标志，0x00 表示成功，为0x01表示失败
    mac[1]~mac[6]:当获取 MAC地址标志位如果mac[0]为成功，则表示模块有效的MAC地址
   */

    if(mac[0] == 1) {
        //获取mac出错
    }else {
        //正确接收到蜂窝模块返回的mac地址
    }
}
#endif

#ifdef GET_MODULE_REMAIN_MEMORY_ENABLE
/**
 * @brief  获取模块内存
 * @param[in] value[]:4个字节，大端格式
 * @return Null
 * @note   MCU需要自行实现该功能
 */
void remain_memory_result(unsigned char value[])
{
    #error "请自行实现获取模块内存处理代码,完成后请删除该行"

}
#endif

#ifdef GET_CELLULAR_RSSI_ENABLE
/**
 * @brief  获取当前蜂窝设备的LTE信号强度
 * @param[in] {rssi} 获取信号强度结果
 * @return Null
 * @note   MCU需要自行实现该功能
 */
void get_cellular_rssi_result(u8 rssi)
{
    #error "请自行实现获取当前蜂窝设备信号强度处理代码,完成后请删除该行"
    if(0 == rssi) {
        //获取失败
    }else {
        //rssi为正值，表示信号强度，单位为dBi
    }
}
#endif

#ifdef SET_FEATURE_TEST_ENABLE
/**
 * @brief  获取模组的自检状态
 * @param[in] {sim_st} SIM卡状态.1:tree,0:fail
 * @param[in] {auth} 是否经过涂鸦授权产测.1:tree,0:fail
 * @param[in] {rf} RF射频是否经过校准. 1:校准完成,0:未校准
 * @param[in] {signal} 获取当前设备的信号强度 0~31
 * @return Null
 * @note   MCU需要自行实现该功能
 */
void get_feature_test_result(u8 sim_st,u8 auth,u8 rf,u8 signal)
{
    #error "Complete the rssi fetch processing code yourself and delete the line"
}
#endif

#ifdef CELLULAR_SERVICE_ENABLE
/**
 * @brief  获取蜂窝设备的工作模式结果
 * @param[in] {result} 工作模式结果
 * @return Null
 * @note   MCU需要自行实现该功能
 */
static void get_cellular_work_mode_result(u8 result)
{
    #error "请自行实现获取蜂窝设备的工作模式结果处理代码,完成后请删除该行"

    switch(result) {
        case 1:
            //全功能模式
        break;

        case 4:
            //飞行模式
        break;

        default:break;
    }
}

/**
 * @brief  获取国际移动用户识别码结果
 * @param[in] {imsi} imsi码 15个字节
 * @param[in] {data_len} 数据长度
 * @return Null
 * @note   MCU需要自行实现该功能
 */
static void get_cellular_IMSI_result(u8 imsi[], u16 data_len)
{
    #error "请自行实现获取国际移动用户识别码结果处理代码,完成后请删除该行"
}

/**
 * @brief  获取国际移动用户识别码结果
 * @param[in] {imci} imci码 20个字节
 * @param[in] {data_len} 数据长度
 * @return Null
 * @note   MCU需要自行实现该功能
 */
static void get_cellular_IMCI_result(u8 imci[], u16 data_len)
{
    #error "请自行实现获取国际移动用户识别码结果处理代码,完成后请删除该行"
}

/**
 * @brief  获取设备 IMEI 结果
 * @param[in] {imei} imei码 15个字节
 * @param[in] {data_len} 数据长度
 * @return Null
 * @note   MCU需要自行实现该功能
 */
static void get_cellular_IMEI_result(u8 imei[], u16 data_len)
{
    #error "请自行实现获取国际移动用户识别码结果处理代码,完成后请删除该行"
}
#ifdef GNSS_SERIVCE_ENABLE
/**
 * @brief  获取设备GNSS定位信息
 * @param[in] {location} gnss定位信息,字符串类型:如“120.661,32.221”(经度、纬度)
 * @param[in] {data_len} 数据长度
 * @return Null
 * @note   MCU需要自行实现该功能
 */
static void get_gnss_location_result(u8 location[], u16 data_len)
{
    #error "请自行实现获取GNSS定位信息处理代码,完成后请删除该行"
    u8 ret = location[0];
    if (!ret) {
        //获取失败
    }
    else {
        //获取成功
    }
}

/**
 * @brief  获取设备GNSS当前信号强度
 * @param[in] {rssi} gnss设备的当前信号强度
 * @param[in] {data_len} 数据长度
 * @return Null
 * @note   MCU需要自行实现该功能
 */
static void get_gnss_rssi_result(u8 rssi[], u16 data_len)
{
    #error "请自行实现获取GNSS定位信息处理代码,完成后请删除该行"
    u8 ret = rssi[0];
    u8 rssi_val = 0;
    if (!ret) {
        //Got failed
    }
    else {
        //Got successed
        rssi_val = rssi[1];
    }
}

/**
 * @brief  获取设备GNSS当前速度
 * @param[in] {speed} gnss设备的当前速度,单位100m/H
 * @param[in] {data_len} 数据长度
 * @return Null
 * @note   MCU需要自行实现该功能
 */
static void get_gnss_speed_result(u8 speed[], u16 data_len)
{
    #error "请自行实现获取GNSS定位信息处理代码,完成后请删除该行"
    u8 ret = speed[0];
    u16 nspeed = speed[1] << 8 | speed[2];
    if (!ret) {
        //获取失败
    }
    else {
        //获取成功
    }
}

/**
 * @brief  获取GNSS当前航向角
 * @param[in] {course} gnss设备的当前航向角
 * @param[in] {data_len}  数据长度
 * @return Null
 * @note   MCU需要自行实现该功能
 */
static void get_gnss_course_result(u8 course[], u16 data_len)
{
    #error "请自行实现获取GNSS当前航向角处理代码,完成后请删除该行"
    u8 ret = course[0];
    u16 ncourse = course[1] << 8 | course[2];
    if (!ret) {
        //Got failed
    }
    else {
        //Got successed
    }
}

/**
 * @brief  获取GNSS当前水平精度因子
 * @param[in] {hdop} gnss设备的当前水平精度因子
 * @param[in] {data_len} 数据长度
 * @return Null
 * @note   MCU需要自行实现该功能
 */
static void get_gnss_hdop_result(u8 hdop[], u16 data_len)
{
    #error "请自行实现获取GNSS当前水平精度因子处理代码,完成后请删除该行"
    u8 ret = hdop[0];
    u16 nhdop = hdop[1] << 8 | hdop[2];
    if (!ret) {
        //Got failed
    }
    else {
        //Got successed
    }
}

/**
 * @brief  获取GNSS当前海拔
 * @param[in] {hdop} 海拔
 * @param[in] {data_len} 数据长度
 * @return Null
 * @note   MCU需要自行实现该功能
 */
static void get_gnss_altitude_result(u8 altitude[], u16 data_len)
{
    #error "请自行实现获取GNSS当前海拔处理代码,完成后请删除该行"
    u8 ret = altitude[0];
    u16 naltitude = altitude[1] << 8 | altitude[2];
    if (!ret) {
        //Got failed
    }
    else {
        //Got successed
    }
}

/**
 * @brief  控制GNSS滤波算法
 * @param[in] {result} 命令执行结果
 * @return Null
 * @note   MCU需要自行实现该功能
 */
static void ctl_gnss_filter_result(u8 result)
{
    #error "请自行实现控制GNSS滤波算法处理代码,完成后请删除该行"
    if (!result) {
        //Set failed
    }
    else {
        //Set successed
    }
}

/**
 * @brief  设置GNSS过滤参数
 * @param[in] {result} 命令执行结果
 * @return Null
 * @note   MCU需要自行实现该功能
 */
static void set_gnss_filter_result(u8 result)
{
    #error "请自行实现设置GNSS过滤参数处理代码,完成后请删除该行"
    if (!result) {
        //Set failed
    }
    else {
        //Set successed
    }
}

/**
 * @brief  获取GNSS当前信息
 * @param[in] {info} GNSS当前信息
 * @param[in] {data_len} 数据长度
 * @return Null
 * @note   MCU需要自行实现该功能
 */
static void get_gnss_info_result(u8 info[], u16 data_len)
{
    #error "请自行实现获取蜂窝设备的GNSS定位信息处理代码,完成后请删除该行"
    u8 ret = info[0];
    gps_info_struct_t gsp_info;
    u8 index = 0;
    if (!ret) {
        //Got failed
    } else {
        //You can use the following part of the code, if your MCU is in large-end mode
        my_memcpy(&gsp_info.lng, info+index, 4);
        index += 4;
        my_memcpy(&gsp_info.lat, info+index, 4);
        index += 4;
        gsp_info.snr = info[index++];
        gsp_info.satellites_num = info[index++];
        my_memcpy(&gsp_info.speed, info+index, 2);
        index += 2;
        my_memcpy(&gsp_info.course, info+index, 2);
        index += 2;
        my_memcpy(&gsp_info.hdop, info+index, 2);
        index += 2;
        my_memcpy(&gsp_info.altitude, info+index, 2);
        index += 2;
    }
}

static void remove_chacator(u8 *src,u8 *desc,i8 chacator1,i8 charactor2,i8 charactor3)
{
    u16 i = 0,j = 0;
    u16 len = strlen((const char *)src);

    for ( i = 0; i < len; i ++) {
        if (src[i] != chacator1 && src[i] != charactor2 && src[i] != charactor3) {
            desc[j] = src[i];
            j ++;
        }
    }
}

/**
 * @brief  获取设备蜂窝设备WIFI定位信息
 * @param[in] {ap_info} wifi定位信息,字符串类型:如["b27e525dc87d",-64],["957e5b5d087d",-64]
 * @param[in] {data_len} 数据长度
 * @return Null
 * @note   MCU需要自行实现该功能
 */
static void get_cellular_wifi_location_result(u8 ap_info[], u16 data_len)
{
    #error "请自行实现获取蜂窝设备的WIFI定位信息处理代码,完成后请删除该行"
    /**/
    u8 i,j;
    i8 wifi_info[256]={0};
    i8 mac[21] = {0};
    i8 rssi[8] = {0};
    i8 *ptemp = NULL;
    i8 *psrc = NULL;
    u8 send_ap_count = 0;

    u8 ap_count = ap_info[0];
    u8 loop_count =( ap_count/MAX_AP_COUNT);
    if (ap_count%MAX_AP_COUNT) {
        loop_count ++;
    }
    if (data_len < 2  || data_len >= CELLULAR_DATA_PROCESS_LMT) {
        return;
    }
    if (!ap_count) {
        //获取失败
    }
    else {
        //获取成功
    }
}

/**
 * @brief  设置GNSS
 * @param[in] {gnss_status} 设置GNSS状态返回值
 * @param[in] {gnss_mode} GNSS定位模式状态返回
 * @return Null
 * @note   MCU需要自行实现该功能
 */
static void get_set_gnss_result(u8 gnss_status, u8 gnss_mode)
{
    #error "请自行实现设置GNSS定位功能处理代码,完成后请删除该行"
    if (!gnss_status) {
        //设置失败
    }
    else {
        if (!gnss_mode) {
            //设置失败
        }
        else {
            //设置成功
        }
    }
}

/**
 * @brief  打开或者关闭WIFI定位功能
 * @param[in] {result} 设置状态返回值
 * @return Null
 * @note   MCU需要自行实现该功能
 */
static void set_cellular_wifi_result(u8 result)
{
    #error "请自行实现设置蜂窝设备的WIFI定位功能处理代码,完成后请删除该行"
    if (!result) {
        //设置失败
    }
    else {
        //设置成功
    }
}

/**
 * @brief  复位GNSS设置
 * @param[in] {result} 设置状态返回值
 * @param[in] {data_len} 数据长度
 * @return Null
 * @note   MCU需要自行实现该功能
 */
static void reset_gnss_result(u8 result)
{
    #error "请自行实现设置蜂窝设备的定位功能处理代码,完成后请删除该行"
    if (!result) {
        //设置失败
    }
    else {
        //设置成功
    }
}

/**
 * @brief  获取lbs定位信息
 * @param  [in] {lbs_info} lbs定位信息,字符串类型:如"46011,e615,04bafc0a"(运营商编码+位置区域码码+基站编码)
 * @param  [in] {data_len} 数据长度
 * @return Null
 * @note   MCU需要自行实现该功能
 */
static void get_cellular_lbs_location_result(u8 lbs_info[], u16 data_len)
{
    #error "请自行实现获取蜂窝设备的lbs定位功能处理代码,完成后请删除该行"
    u8 ret = lbs_info[0];
    if (!ret) {
        //获取失败
    }
    else {
        //获取成功
    }
}
#if (CELLULAR_MODULE_TYPE == 1)
/**
 * @brief  获取gnss设备主电源状态
 * @param  [in] {result} 获取电源状态的结果
 * @return Null
 * @note   MCU需要自行实现该功能
 */
static void get_gnss_power_status_result(u8 result)
{
    #error "请自行实现获取蜂窝设备的主电源状态功能处理代码,完成后请删除该行"
    switch(result) {
        case 0:
            //电源关闭
        break;

        case 1:
            //固件加载中
        break;

        case 2:
            //电源开启，固件加载完成
        break;

        default:break;
    }
}

/**
 * @brief  设置gnss设备主电源开启或关闭
 * @param  [in] {result} 设置主电源开启或关闭的返回值
 * @return Null
 * @note   MCU需要自行实现该功能
 */
static void get_set_gnss_power_result(u8 result)
{
    #error "请自行实现设置蜂窝设备的gnss主电源控制功能处理代码,完成后请删除该行"
    if (!result) {
        //设置失败
    }
    else {
        //设置成功
    }
}
#endif

/**
 * @brief  设置ANGSS开关
 * @param  [in] {result} 命令执行结果
 * @return Null
 * @note   MCU需要自行实现该功能
 */
static void get_set_agnss_result(u8 result)
{
    #error "请自行实现设置蜂窝设备的ANGSS功能处理代码,完成后请删除该行"
    if (!result) {
        //Set failed
    }
    else {
        //Set successed
    }
}
/**
 * @brief  获取设备GNSS纬经度定位信息
 * @param[in] {location} gnss定位信息,字符串类型:如“32.221,120.661”(纬度、经度)
 * @param[in] {data_len} 数据长度
 * @return Null
 * @note   MCU需要自行实现该功能
 */
static void get_gnss_location_lat_lg_result(u8 location[], u16 data_len)
{
    #error "请自行实现获取GNSS定位信息处理代码,完成后请删除该行"
    u8 ret = location[0];
    if (!ret) {
        //获取失败
    }
    else {
        //获取成功
    }
}
/**
 * @brief  获取设置gps定位周期性上报的返回结果
 * @param[in] {result} 设置状态返回值
 * @return Null
 * @note   MCU需要自行实现该功能
 */
static void get_set_cellular_auto_rpt_gps_result(u8 result)
{
    #error "请自行实现获取设置gps定位周期性上报功能返回结果的处理代码,完成后请删除该行"
    if (!result) {
        //设置失败
    }
    else {
        //设置成功
    }
}
/**
 * @brief  获取设置wifi定位周期性上报的返回结果
 * @param[in] {result} 设置状态返回值
 * @return Null
 * @note   MCU需要自行实现该功能
 */
static void get_set_cellular_auto_rpt_wifi_result(u8 result)
{
    #error "请自行实现获取设置wifi定位周期性上报功能返回结果的处理代码,完成后请删除该行"
    if (!result) {
        //设置失败
    }
    else {
        //设置成功
    }
}
/**
 * @brief  获取设置lbs定位周期性上报的返回结果
 * @param[in] {result} 设置状态返回值
 * @return Null
 * @note   MCU需要自行实现该功能
 */
static void get_set_cellular_auto_rpt_lbs_result(u8 result)
{
    #error "请自行实现获取设置lbs定位周期性上报功能返回结果的处理代码,完成后请删除该行"
    if (!result) {
        //设置失败
    }
    else {
        //设置成功
    }
}
/**
 * @brief  获取各定位功能是否开启
 * @param[in] {result} 各定位功能开启状态1: 开启 0: 关闭
 * @return Null
 * @note   MCU需要自行实现该功能
 */
static void get_cellular_location_track_status_result(u8 result[])
{
    #error "请自行实现获取各定位功能开启状态后的处理代码,完成后请删除该行"
    u8 gnss_result = 0;
    u8 wifi_result = 0;
    u8 lbs_result = 0;
    gnss_result = result[0];
    wifi_result = result[1];
    lbs_result = result[2];
    if (gnss_result){
        //gnss定位功能开启
    }
    if (wifi_result){
        //wifi定位功能开启
    }
    if (lbs_result){
        //lbs定位功能开启
    }
}
#endif

#ifdef SUPPORT_CALL
/**
 * @brief  获取电话业务返回结果
 * @param  [in] {phone_info} 电话业务返回信息
 * @param  [in] {data_len} 数据长度
 * @return Null
 * @note   MCU需要自行实现该功能
 */
static void get_contrl_phone_result(u8 phone_info[], u16 data_len)
{
    #error "请自行实现设置蜂窝设备的电话业务功能处理代码,完成后请删除该行"
    if (data_len < 2) {
        return;
    }
    u8 result = 0;
    u8 cmd = phone_info[0];
    switch (cmd) {
        case 0:
            //电话呼入提醒
            mcu_control_phone_callin_rsp();
            s_phone_call_status = CELLULAR_PHONE_DIAILING;
        break;

        case 1:{
            //电话呼出结果
            result = phone_info[1];
            if (!result) {
                //呼出失败
                s_phone_call_status = CELLULAR_PHONE_CALL_FAILED;
            }
            else {
                //呼出成功
                s_phone_call_status  = CELLULAR_PHONE_DIAILING;
            }
        }
        break;

        case 2:{
            //电话接听结果
            result = phone_info[1];
            if (!result) {
                //接听成功
                s_phone_call_status = CELLULAR_PHONE_CALLING;
            }
            else {
                //接听失败
                s_phone_call_status = CELLULAR_PHONE_CALL_FAILED;
            }
        }
        break;

        case 3:{
            //电话挂断结果
            result = phone_info[1];
            if (!result) {
                //挂断成功
                s_phone_call_status = CELLULAR_PHONE_IDLE;
            }
            else {
                //挂断失败
                s_phone_call_status = CELLULAR_PHONE_CALLING;
            }
        }
        break;

        case 4:{
            //电话状态查询结果
            result = phone_info[1];
            if (result == 0) {
                //拨号中
                s_phone_call_status = CELLULAR_PHONE_DIAILING;
            }
            else if(result == 1) {
                //空闲
                s_phone_call_status = CELLULAR_PHONE_IDLE;
            }
            else if(result == 2) {
                //通话失败
                s_phone_call_status = CELLULAR_PHONE_CALL_FAILED;
            }
            else if(result == 3) {
                //通话中
                s_phone_call_status = CELLULAR_PHONE_CALLING;
            }
            else {
                break;
            }
        }
        break;

        default:break;
    }
}

/**
 * @brief  设置VoLTE开关
 * @param  [in] {result} 设置VoLTE开关的结果
 * @return Null
 * @note   MCU需要自行实现该功能
 */
static void get_set_cellular_ctrl_volte_result(u8 result)
{
    #error "请自行实现设置VoLTE开关的处理代码,完成后请删除该行"
    if (result){
        //设置成功
    }else{
        //设置失败
    }
}
#endif

#ifdef SUPPORT_SMS
/**
 * @brief  获取短信业务返回结果
 * @param  [in] {sms_info} 短信业务返回信息
 * @param  [in] {data_len} 数据长度
 * @return Null
 * @note   MCU需要自行实现该功能
 */
static void get_contrl_sms_result(u8 sms_info[], u16 data_len)
{
    #error "请自行实现设置蜂窝设备的短信功能处理代码,完成后请删除该行"
    if (data_len < 2) {
        return;
    }
    u8 result = 0;
    u8 cmd = sms_info[0];
    switch (cmd) {
        case 0:
            //接收到短信
        break;

        case 1:{
            //发送短信
            result = sms_info[1];
            if (!result) {
                //发送失败
            }
            else {
                //发送成功
            }
        }
        break;

        default:break;
    }
}
#endif

/**
 * @brief  获取电池电量
 * @param  [in] {battery} 电池电量
 * @return Null
 * @note   MCU需要自行实现该功能
 */
static void get_cellular_vbat_vol_result(u8 battery)
{
    #error "请自行实现获取蜂窝设备的电池电量功能处理代码,完成后请删除该行"

}

/**
 * @brief  获取充电状态
 * @param  [in] {status} 充电状态
 * @return Null
 * @note   MCU需要自行实现该功能
 */
static void get_vbat_charging_status_result(u8 status)
{
    #error "请自行实现获取蜂窝设备的充电状态功能处理代码,完成后请删除该行"
    if (status == 1) {
        //开始充电
    }
    else if (status == 2) {
        //充电结束
    }
    else if (status == 3) {
        //电量低
    }
    else if (status == 4) {
        //电量超低
    }
    else if (status == 5) {
        //电池拔出
    }
    else if (status == 6) {
        //充电器故障
    }
    else if (status == 7) {
        //充电故障
    }
    else {
        return;
    }
}

/**
 * @brief  获取音量设置结果
 * @param  [in] {cmd} 1:本地音量 2:通话音量
 * @param  [in] {result} 音量设置结果
 * @return Null
 * @note   MCU需要自行实现该功能
 */
static void get_set_cellular_volume_result(u8 cmd, u8 result)
{
    #error "请自行实现获取蜂窝设备的音量设置功能处理代码,完成后请删除该行"
    switch (cmd) {
        case 1:{
            if (!result) {
                //获取成功
            }
            else {
                //获取失败
            }
        }
        break;

        case 2:{
            if (!result) {
                //获取成功
            }
            else {
                //获取失败
            }
        }
        break;

        default:break;
    }
}

/**
 * @brief  获取音频播放结果
 * @param  [in] {play_info} 蜂窝模组返回的播放信息
 * @param  [in] {data_len} 数据长度
 * @return Null
 * @note   MCU需要自行实现该功能
 */
static void get_contrl_cellulat_audio_play_result(u8 play_info[], u16 data_len)
{
    #error "请自行实现获取蜂窝设备的音频播放功能处理代码,完成后请删除该行"
    if (data_len < 4) {
        return;
    }
    u8 result = play_info[3];
    if (!result) {
        //获取成功
    }
    else {
        //获取失败
    }
}

/**
 * @brief  获取音频播放状态
 * @param  [in] {cmd} 0:空闲 1:播放中 2:播放中止 3:播放完成
 * @param  [in] {result} 播放设置结果
 * @return Null
 * @note   MCU需要自行实现该功能
 */
static void get_voice_play_status_result(u8 cmd, u8 result)
{
    #error "请自行实现获取蜂窝设备的音频播放状态功能处理代码,完成后请删除该行"
    switch (cmd) {
        case 0:{
            if (!result) {
                //获取成功
            }
            else {
                //获取失败
            }
        }
        break;

        case 1:{
            if (!result) {
                //获取成功
            }
            else {
                //获取失败
            }
        }
        break;

        case 2:{
            if (!result) {
                //获取成功
            }
            else {
                //获取失败
            }
        }
        break;

        case 3:{
            if (!result) {
                //获取成功
            }
            else {
                //获取失败
            }
        }
        break;

        default:break;
    }
}
/**
 * @brief  获取音频播放状态
 * @param  [in] {cmd} 1:播放完成
 * @param  [in] {result} 播放设置结果
 * @return Null
 * @note   MCU需要自行实现该功能
 */
static void get_voice_play_finish_result(u8 cmd)
{
    #error "请自行实现获取蜂窝设备的音频播放状态功能处理代码,完成后请删除该行"
}


/**
 * @brief  获取低电量关机设置结果
 * @param  [in] {result} 返回的设置结果
 * @return Null
 * @note   MCU需要自行实现该功能
 */
static void get_set_cellular_ctrl_low_vol_power_result(u8 result)
{
    #error "请自行实现获取低电量关机设置结果后的处理代码,完成后请删除该行"
    if (result){
        //设置成功
    }else{
        //设置失败
    }
}


/**
 * @brief  设置offline开关返回
 * @param  [in] {result} 设置offline开关的结果
 * @return Null
 * @note   MCU需要自行实现该功能
 */
void get_set_cellular_ctrl_offline_result(u8 result)
{
    #error "请自行实现设置offline开关的处理代码,完成后请删除该行"
    if (result){
        //设置成功
    }else{
        //设置失败
    }
}

/**
 * @brief  获取蓝牙控制是否启动的设置结果
 * @param  [in] {result} 返回的设置结果
 * @return Null
 * @note   MCU需要自行实现该功能
 */
static void get_set_cellular_ctrl_ble_conn_result(u8 result)
{
    #error "请自行实现获取蓝牙控制是否启动的处理代码,完成后请删除该行"
    if (result){
        //设置成功
    }else{
        //设置失败
    }
}
#ifdef SUPPORT_SHORTURL_GET
/**
 * @brief  获取二维码短链接地址
 * @param  [in] {shorturl} 返回的短链接地址
 * @return Null
 * @note   MCU需要自行实现该功能
 */
static void get_set_cellular_send_shorturl_result(u8 *shorturl)
{
    #error "请自行实现获取二维码短链接地址后的处理代码,完成后请删除该行"
    //获取短链接地址
}
#endif



/**
 * @brief  获取当前蜂窝设备的LTE信号强度
 * @param[in] {rssi} 获取信号强度结果
 * @return Null
 * @note   MCU需要自行实现该功能
 */
void get_dtmf_value_result(u8 dtmf)
{
    #error "请自行实现获取当前接收到的dtmf值处理代码,完成后请删除该行"
    switch (dtmf)
    {
        case 0:// DTMF 0
            break;
        case 1:// DTMF 1
            break;
        case 2:// DTMF 2
            break;
        case 3:// DTMF 3
            break;
        case 4:// DTMF 4
            break;
        case 5:// DTMF 5
            break;
        case 6:// DTMF 6
            break;
        case 7:// DTMF 7
            break;
        case 8:// DTMF 8
            break;
        case 9:// DTMF 9
            break;
        case 10:// DTMF A
            break;
        case 11:// DTMF B
            break;
        case 12:// DTMF C
            break;
        case 13:// DTMF D
            break;
        case 14:// DTMF #
            break;
        case 15:// DTMF *
            break;
        default:
            break;
    }
}
/**
 * @brief  获取发送DTMF数据结果
 * @param[in] {result} 发送的结果
 * @return Null
 * @note   MCU需要自行实现该功能
 */
void get_dtmf_ack_result(u8 result)
{
    #error "请自行实现获取蜂窝设备的DTMF结果处理代码,完成后请删除该行"

    if (result){
        //设置成功
    }else{
        //设置失败
    }
}

/**
 * @brief  获取模组版本信息
 * @param[in] {rssi} 获取信号强度结果
 * @return Null
 * @note   MCU需要自行实现该功能
 */
void get_cellular_ver_result(u8 ver_info[],u16 len,u8 result)
{
    #error "请自行实现获取蜂窝设备的版本结果处理代码,完成后请删除该行"
    if (result){
        //获取成功
    }
    else{
        //获取失败
    }
}

/**
 * @brief  获取当前网络类型结果
 * @param[in] {nettype} 网络类型
 * @return Null
 * @note   MCU需要自行实现该功能
 */
void get_cellular_nettype_result(u8 nettype)
{
    #error "请自行实现获取蜂窝设备的网络类型结果处理代码,完成后请删除该行"
    switch(nettype){
        case 1://其他异常码
        break;
        case 2://LTE网络
        break;
        case 4://CATM网络
        break;
        case 5://NB网络
        break;
        default:
        break;
    }
}

/**
 * @brief  获取设置/获取MIC增益结果
 * @param[in] {ctrl} 1：控制，0：获取
 * @param[in] {result} 0失败 1成功
 * @param[in] {gain} 0~30（0静音）当ctrl为获取的时候才有效
 * @return Null
 * @note   MCU需要自行实现该功能
 */
void contrl_cellular_mic_gain_result(u8 ctrl,u8 result,u8 gain)
{
    #error "请自行实现蜂窝设备的设置/获取MIC增益结果处理代码,完成后请删除该行"
    if (result){
        //成功
    }
    else{
        //失败
    }
}

/**
 * @brief  获取设置/获取侧音增益结果
 * @param[in] {ctrl} 1：控制，0：获取
 * @param[in] {result} 0失败 1成功
 * @param[in] {gain} 0~99（0静音）当ctrl为获取的时候才有效
 * @return Null
 * @note   MCU需要自行实现该功能
 */
void contrl_cellular_side_gain_result(u8 ctrl,u8 result,u8 gain)
{
    #error "请自行实现蜂窝设备的设置/获取MIC增益结果处理代码,完成后请删除该行"
    if (result){
        //成功
    }
    else{
        //失败
    }
}

/**
 * @brief  获取设置蜂窝模块静音结果
 * @param[in] {type} 0 -- sms静音，1 -- 呼入静音，2 -- 电话通话静音
 * @param[in] {result} 0失败 1成功
 * @return Null
 * @note   MCU需要自行实现该功能
 */
void get_set_cellular_ctrl_mute_result(u8 type, u8 result)
{
    #error "请自行实现蜂窝设备的设置静音结果处理代码,完成后请删除该行"
    if (result){
        //成功
        switch(type) {
            case 0:
            case 1:
            case 2:
            break;
        }
    }
    else{
        //失败
    }
}


/**
 * @brief  获取设置DTMF监听功能使能结果
 * @param[in] {result} 0失败 1成功
 * @return Null
 * @note   MCU需要自行实现该功能
 */
void get_set_cellular_ctrl_dtmf_result(u8 result)
{
    #error "请自行实现蜂窝设备的设置静音结果处理代码,完成后请删除该行"
    if (result){
        //成功
    }
    else{
        //失败
    }
}

/**
 * @brief  获取蜂窝上报的告警事件
 * @param[in] {result} 告警事件
 * @return Null
 * @note   MCU需要自行实现该功能
 */
void get_cellular_warning_result(u8 result)
{
    #error "请自行实现获取蜂窝上报的告警事件处理代码,完成后请删除该行"
    switch (result)
    {
        case 1:/* 网络注册错误 */
            break;
        case 2:/* 网络PDP激活错误 */
            break;
        case 3:/*  TCP网络无法连接 */
            break;
    }
}


/**
 * @brief  获取设置蜂窝外部唤醒GPIO管脚配置的应答
 * @param[in] {result} 0失败 1成功
 * @return Null
 * @note   MCU需要自行实现该功能
 */
void get_set_cellular_wakeup_gpio_result(u8 result)
{
    #error "请自行实现获取设置外部唤醒管脚配置处理代码,完成后请删除该行"
    if (result){
        //成功
    }
    else{
        //失败
    }
}


/**
 * @brief  获取通知MCU主动重启设备应答
 * @return Null
 * @note   MCU需要自行实现该功能
 */
void get_cellular_reboot_result(void)
{
    #error "请自行实现获取设置外部唤醒管脚配置处理代码,完成后请删除该行"
    u8 length = 0;
    length = set_cellular_uart_byte(length, REQ_CELLULAR_MCU_REBOOT);
    cellular_uart_write_frame(SET_CELLULAR_CMD, MCU_TX_VER, length);

    //sleep 1秒
    //使用电源控制重新启动蜂窝模块
}



/**
 * @brief  获取设置SIM卡热插拔使能控制的应答
 * @param[in] {result} 0失败 1成功
 * @return Null
 * @note   MCU需要自行实现该功能
 */
void get_set_cellular_sim_hotplug_result(u8 result)
{
    #error "请自行实现获取设置SIM卡热插拔使能控制的应答处理代码,完成后请删除该行"
    if (result){
        //成功
    }
    else{
        //失败
    }
}

#ifdef SUPPORT_ADC_CTL
/**
 * @brief  获取控制ADC处理结果
 * @return Null
 * @note   MCU需要自行实现该功能
 */
static void set_cellular_adc_result(u8 data[], u16 data_len)
{
    #error "请自行执行，完成后删除此行"
    u8 ctl = data[0];
    u8 port = data[1];
    u8 result = data[2];
    if(result) {
        //failure
        return;
    }
    // success
}
#endif

#ifdef SUPPORT_GPIO_CTL
/**
 * @brief  获取控制GPIO处理结果
 * @return Null
 * @note   MCU需要自行实现该功能
 */
static void set_celular_gpio_result(u8 data[], u16 data_len)
{
    #error "请自行执行，完成后删除此行"
    u8 ctl = data[0];
    u8 pin = data[1];
    u8 result = data[2];
    if(result) {
        //failure
        return;
    }
    // success
}
#endif

#ifdef SET_RRC_ENABLE
/**
 * @brief  获取控制RRC命令结果过
 * @param[in] {len} 数据长度
 * @return Null
 * @note   MCU需要自行实现该功能
 */
void get_cellular_rrc_result(u8 *data,u16 len)
{
    #error "请自行执行，完成后删除此行"
    u8 result = data[0];
    if(result== 0) {
        //success
        u8 rrc_value = data[1];
    } else {
        //failure
    }
}
#endif

#ifdef GET_CELLULAR_PLMN_ENABLE
/**
 * @brief  获取当前蜂窝模组的所在网络的PLMN列表应答(MA510模组支持)
 * @param[in] {result} 0失败 1成功
 * @param[in] {data} plmn值，每个PLMN4字节，大端模式
 * @param[in] {len} data 数据大小
 * @return Null
 * @note   MCU需要自行实现该功能
 */
void get_cellular_plmn_result(u8 result,u8 *data,u16 len)
{
    #error "请自行实现获取获取当前蜂窝模组的所在网络的PLMN列表应答处理代码,完成后请删除该行"
    int plmn;
    u16 i = 0;
    if (result){
        //成功
        for(i = 0; i < len/4; i ++) {
            // plmn = data[i*4] | data[i*4+1] | data[i*4+2] | data[i*4+3];
        }
    }
    else{
        //失败
    }
}

#endif

/**
 * @brief  内存播放音频服务处理
 * @param[in] {data} 数据缓冲区
 * @param[in] {len} 数据长度
 * @return Null
 * @note  MCU需要自行实现该功能
 */
void get_cellular_contrl_mem_audio_result(u8 *data,u16 len)
{
	#error "请自行执行，完成后删除此行"
    u8 cmd = data[0];
    if (cmd != 5) {
        u8 result = data[1];
        if(!result) {
            //成功
        } else {
            //失败
        }
    }
    else {
        u8 audio_sum = data[1]; 
    }
}

#ifdef SUPPORT_ONLINE_TTS
/**
 * @brief tts服务命令结果处理
 * @param[in] {result} 命令结果
 * @return Null
 * @note  MCU需要自行实现该功能
 */
void get_cellular_ctrl_tts_result(char result)
{
	#error "请自行执行，完成后删除此行"
    if (result == 0){
        //成功
    }
    else{
        //失败
    }
}
#endif
/**
 * @brief  蜂窝设备服务处理
 * @param[in] {p_data} 数据缓冲区
 * @param[in] {data_len} 数据长度
 * @return Null
 * @note   MCU需要自行实现该功能
 */
void cellular_service_process(u8 cell_cmd,u8 p_data[], u16 data_len)
{
    switch(p_data[0]) {
        case GET_CELLULAR_WORK_MODE:
            get_cellular_work_mode_result(p_data[1]);
        break;

        case GET_CELLULAR_IMSI:
            get_cellular_IMSI_result(p_data + 1, data_len - 1);
        break;

        case GET_CELLULAR_IMCI:
            get_cellular_IMCI_result(p_data + 1, data_len - 1);
        break;

        case GET_CELLULAR_IMEI:
            get_cellular_IMEI_result(p_data + 1, data_len - 1);
        break;
#ifdef GNSS_SERIVCE_ENABLE
        case GET_CELLULAR_GPS_LOCATION:
            get_gnss_location_result(p_data + 1, data_len - 1);
        break;
        case GET_CELLULAR_GPS_SNR:
            get_gnss_rssi_result(p_data + 1, data_len - 1);
        break;
        case GET_CELLULAR_GPS_SPEED:
            get_gnss_speed_result(p_data + 1, data_len - 1);
        break;
        case GET_CELLULAR_GPS_COURSE:
            get_gnss_course_result(p_data + 1, data_len - 1);
        break;
        case GET_CELLULAR_GPS_HDOP:
            get_gnss_hdop_result(p_data + 1, data_len - 1);
        break;
        case GET_CELLULAR_GPS_ALTITUDE:
            get_gnss_altitude_result(p_data + 1, data_len - 1);
        break;
        case CTL_CELLULAR_GPS_FILTER:
            ctl_gnss_filter_result(p_data[1]);
        break;
        case SET_CELLULAR_GPS_FILTER:
            set_gnss_filter_result(p_data[1]);
        break;
        case GET_CELLULAR_GPS_INFO:
            get_gnss_info_result(p_data + 1, data_len - 1);
        break;
        case GET_CELLULAR_WIFI_LOCATION:
            get_cellular_wifi_location_result(p_data + 1, data_len - 1);
        break;
        case SET_CELLULAR_CTRL_GNSS:
            get_set_gnss_result(p_data[1],p_data[2]);
        break;
        case SET_CELLULAR_CTRL_WIFI:
            set_cellular_wifi_result(p_data[1]);
        break;
        case SET_CELLULAR_RESET_GNSS:
            reset_gnss_result(p_data[1]);
        break;
        case GET_CELLULAR_LBS_LOCATION:
            get_cellular_lbs_location_result(p_data + 1, data_len - 1);
        break;
#if (CELLULAR_MODULE_TYPE == 1)
        case GET_CELLULAR_GNSS_POWER_STATUS:
            get_gnss_power_status_result(p_data[1]);
        break;
        case SET_CELLULAR_GNSS_POWER:
            get_set_gnss_power_result(p_data[1]);
        break;
#endif
        case SET_CELLULAR_AGNSS:
            get_set_agnss_result(p_data[1]);
        break;
#endif
#ifdef SUPPORT_CALL
        case CONTRL_CELLULAR_PHONE:
            get_contrl_phone_result(p_data + 1, data_len - 1);
        break;
#endif
#ifdef SUPPORT_SMS
        case CONTRL_CELLULAR_SMS:
            get_contrl_sms_result(p_data + 1, data_len - 1);
        break;
#endif
        case GET_CELLULAR_VBAT_VOL:
            get_cellular_vbat_vol_result(p_data[1]);
        break;
        case GET_CELLULAR_VBAT_STATUS:
            get_vbat_charging_status_result(p_data[1]);
        break;
        case CONTRL_CELLULAR_AUDIO_PLAY:
            get_contrl_cellulat_audio_play_result(p_data + 1, data_len - 1);
        break;
        case GET_CELLULAR_VOICE_PLAY_STATUS:
            get_voice_play_status_result(p_data[1], p_data[2]);
        break;
        case GET_CELLULAR_VOICE_PLAY_FINISH:
            get_voice_play_finish_result(p_data[1]);
        break;
        case SET_CELLULAR_VOLUME:
            get_set_cellular_volume_result(p_data[1], p_data[2]);
        break;
#ifdef GNSS_SERIVCE_ENABLE
        case GET_CELLULAR_GPS_LOCATION_LAT_LG:
            get_gnss_location_lat_lg_result(p_data + 1, data_len - 1);
        break;
        case SET_CELLULAR_AUTO_RPT_GPS:
            get_set_cellular_auto_rpt_gps_result(p_data[1]);
        break;
        case SET_CELLULAR_AUTO_RPT_WIFI:
            get_set_cellular_auto_rpt_wifi_result(p_data[1]);
        break;
        case SET_CELLULAR_AUTO_RPT_LBS:
            get_set_cellular_auto_rpt_lbs_result(p_data[1]);
        break;
        case GET_CELLULAR_LOCATION_TRACK_STATUS:
            get_cellular_location_track_status_result(p_data + 1);
        break;
#endif
        case SET_CELLULAR_CTRL_LOW_VOL_POWER:
            get_set_cellular_ctrl_low_vol_power_result(p_data[1]);
        break;
        case SET_CELLULAR_CTRL_BLE_CONN:
            get_set_cellular_ctrl_ble_conn_result(p_data[1]);
        break;
#ifdef SUPPORT_SHORTURL_GET
        case SET_CELLULAR_SEND_SHORTURL:
            get_set_cellular_send_shorturl_result(p_data + 1);
        break;
#endif
#ifdef SUPPORT_CALL
        case SET_CELLULAR_CTRL_VOLTE:
            get_set_cellular_ctrl_volte_result(p_data[1]);
        break;
#endif
        case SET_CELLULAR_OFFLINE_TIME:
            get_set_cellular_ctrl_offline_result(p_data[1]);
        break;

        case SET_CELLULAR_DTMF_DATA:
            if (cell_cmd == GET_CELLULAR_CMD) {
                get_dtmf_value_result(p_data[1]);
            } else {
                get_dtmf_ack_result(p_data[1]);
            }
        break;
        case GET_CELLULAR_VER_INFO:
            get_cellular_ver_result(p_data + 2,data_len-2,p_data[1]);
        break;
        case GET_CELLULAR_NET_TYPE:
            get_cellular_nettype_result(p_data[1]);
        break;
        case CONTRL_CELLULAR_MIC_GAIN:
            if (p_data[1] == 0) {
                contrl_cellular_mic_gain_result(p_data[1],p_data[2],p_data[3]);
            } else {
                contrl_cellular_mic_gain_result(p_data[1],p_data[2],0);
            }
        break;
        case CONTRL_CELLULAR_SIDE_GAIN:
            if (p_data[1] == 0) {
                contrl_cellular_side_gain_result(p_data[1],p_data[2],p_data[3]);
            } else {
                contrl_cellular_side_gain_result(p_data[1],p_data[2],0);
            }
        break;
        case SET_CELLULAR_CTRL_MUTE:
            get_set_cellular_ctrl_mute_result(p_data[1],p_data[2]);
        break;
        case SET_CELLULAR_DTMF_CTRL:
            get_set_cellular_ctrl_dtmf_result(p_data[1]);
        break;
        case RPT_CELLULAR_NET_WARNING:
            get_cellular_warning_result(p_data[1]);
        break;
        case SET_CELLULAR_WAKE_MCU_GPIO:
            get_set_cellular_wakeup_gpio_result(p_data[1]);
        break;
        case REQ_CELLULAR_MCU_REBOOT:
            get_cellular_reboot_result();
        break;
        case SET_CELLULAR_HOTPLUG:
            get_set_cellular_sim_hotplug_result(p_data[1]);
        break;
#ifdef SUPPORT_ADC_CTL
        case SET_CELLULAR_ADC:
            set_cellular_adc_result(p_data+1, data_len-1);
        break;
#endif
#ifdef SUPPORT_GPIO_CTL
        case SET_CELLULAR_GPIO:
            set_celular_gpio_result(p_data+1, data_len-1);
        break;
#endif
#ifdef GET_CELLULAR_PLMN_ENABLE
        case GET_CELLULAR_PLMN:
            get_cellular_plmn_result(p_data[1],p_data+2,data_len-1);
        break;
#endif
#ifdef SET_RRC_ENABLE
        case SET_CELLULAR_RRC:
            get_cellular_rrc_result(p_data+1,data_len-1);
        break;
#endif
#ifdef SUPPORT_MEM_AUDIO
        case CONTRL_CELLULAR_MEM_PLAY:
            get_cellular_contrl_mem_audio_result(&p_data[1],data_len - 1);
        break;
#endif
#ifdef SUPPORT_ONLINE_TTS
        case CONTRL_CELLULAR_ONLINE_TTS:
            get_cellular_ctrl_tts_result((char)p_data[1]);
        break;
#endif
        default:break;
    }
}
#endif
#ifdef LOCK_SERIVCE_ENABLE

/**
 * @brief  获取到的Unix时间
 * @param[in] {time} 获取到的Unix时间数据
 * @return Null
 * @note   MCU需要自行实现该功能
 */
void get_unix_time_zone(u8 time[])
{
    #error "请自行完成相关代码,并删除该行"
    /*
    time[0] 为是否获取时间成功标志，为 0 表示失败，为 1表示成功
    time[1]-time[4] 为Unix时间戳
    time[5] 为是否获取时区成功标志，为 0 表示失败，为 1表示成功
    time[6] 0：表示东区，1：表示西区
    time[7] 为时区
    time[8] 是否有夏令时 1:表示有夏令时 0:表示没有夏令时
    time[9]-time[12] 进入夏令时时间戳
    time[13]-time[16] 退出夏令时时间戳
    */
    if(time[0] == 1) {
        //正确接收到蜂窝模块返回的unix时区数据

    }else {
        //获取unix时间出错
    }
}

/**
 * @brief  获取密码进制服务设置结果
 * @param[in] {result} 获取到的设置密码进制服务返回的状态
 * @return Null
 * @note   MCU需要自行实现该功能
 */
void get_set_psw_base_result(u8 result)
{
    #error "请自行完成相关代码,并删除该行"
    if(result == 0) {
        //设置成功
        is_setpswd_base = TRUE;
    }else {
        //设置失败
        is_setpswd_base = FALSE;
    }
}

/**
 * @brief  schedule列表数据处理
 * @param[in] {schedule_num} schedule列表数量
 * @param[in] {schedule_data} schedule列表数据
 * @return Null
 * @note   MCU调用 schedule_temp_pass_data 成功后，该函数内可对 schedule 列表数据进行处理
 */
static void schedule_data_process(u8 schedule_num, const u8 schedule_data[])
{
    #error "请自行完成带schedule列表处理代码,并删除该行"

    u8 i = 0;
    u8 offset = 0;

    for(i=0;i<schedule_num;i++) {
        /*
        在此添加schedule列表数据处理
        schedule_data[offset]: 0：非全天有效，分时间段有效  1：全天有效。后面的起始时间和结束时间为无效数据
        schedule_data[offset+1]: 开始时间（小时）
        schedule_data[offset+2]: 开始时间（分钟）
        schedule_data[offset+3]: 结束时间（小时）
        schedule_data[offset+4]: 结束时间（分钟）
        schedule_data[offset+5]: 周循环   Bit0:周日 Bit1:周一 Bit2:周二 Bit3:周三 Bit4:周四 Bit5:周五 Bit6:周六
                                 若此条schedule周日，周三循环，则为0x09
        */
       /*当schedule_num大于1时，处理完列表数据后offset需要偏移*/
        offset += 6;
    }
}

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
                               u8 schedule_num, const u8 schedule_data[])
{
    #error "请自行完成带schedule列表临时密码信息处理代码,并删除该行"

    /*
    succ_flag为是否获取密码成功标志，为 0 表示失败，为 1 表示成功
   */
    /*
    注：获取多组密码成功，该函数会进入多次，
        直至将多组临时密码全部获取完结束；
        若失败则只进入一次。
   */
    if (succ_flag == 1) {
        //获取临时密码数据成功
        /*
        pass_ser为密码编号
        pass_valcnt为密码有效次数
        pass_state为密码当前状态

        gl_start为密码生效日期格林时间
        gl_start[0]为年份 , 0x00 表 示2000 年
        gl_start[1]为月份，从 1 开始到12 结束
        gl_start[2]为日期，从 1 开始到31 结束
        gl_start[3]为时钟，从 0 开始到23 结束
        gl_start[4]为分钟，从 0 开始到59 结束
        gl_start[5]为秒钟，从 0 开始到59 结束
        gl_end为密码截至日期格林时间，同gl_start

        pass指向临时密码数据(ascll码)，长度pass_len
        */

        //此处添加密码数据处理

        schedule_data_process(schedule_num, schedule_data);
    }else {
        //获取临时密码数据出错
    }
}
/**
 * @brief  MCU请求临时密码(带schedule列表)返回
 * @param[in] {data} 返回数据
 * @return Null
 * @note   Null
 */
void get_schedule_temp_pass_handle(const u8 data[])
{
    u8 i = 0;
    u8 pass_len = 0;
    u8 result = data[0];
    u8 pass_num = data[1];
    if(get_setpassword_base_flag()){
        pass_len = data[3];
    }else{
        pass_len = data[2];
    }

    u8 offset = 4;

    if (result == 1) {
        for (i=0;i<pass_num;i++) {
            schedule_temp_pass_data(result, data[offset], data[offset+1], data[offset+2], data+offset+3,
                               data+offset+9, data+offset+15, pass_len, data[offset+15+pass_len],data+offset+15+pass_len+1);
            offset += 15 + pass_len + 1 + 6*data[offset+15+pass_len];
        }
    }else {
        schedule_temp_pass_data(result, 0, 0, 0, 0, 0, 0, 0, 0, 0);
    }
}
#ifdef OFFLINE_DYN_PW_ENABLE
/**
 * @brief  离线动态密码结果
 * @param[in] {result_data} 结果数据
 * @return Null
 * @note   MCU需要先自行调用mcu_set_offline_dynamic_pswd函数后，在此函数对接收的结果进行处理
 */
void get_offline_dynamic_pswd_result(u8 result_data[])
{
    #error "请自行完成离线动态密码结果处理代码,并删除该行"
    u8 result; //密码正确性
    u8 type; //密码类型
    u8 decode_len; //解密后数据长度
    u8 decode[DECODE_MAX_LEN]; //解密数据

    result = result_data[0];
    if(0 == result) {
        //正确
    }else {
        //错误
        return; //错误时，无后续数据
    }

    type = result_data[1];
    switch(type) {
        case 0:
            //限时开门密码
        break;

        case 1:
            //单次开门密码
        break;

        case 2:
            //清除密码
        break;

        default:break;
    }

    decode_len = result_data[2];
    my_memcpy(decode,&result_data[3],decode_len);

    //可添加解密数据处理
}
#endif
#endif


/**
 * @brief  打开模块重置通知服务应答处理
 * @param[in] {result} 结果
 * @return Null
 * @note   MCU需要自行实现该功能
 */
void open_expand_service_reset_result(u8 result)
{
    #error "请自行实现获取蜂窝设备的工作模式结果处理代码,完成后请删除该行"

    if (result){
        //设置成功
    }else{
        //设置失败
    }
}

/**
 * @brief  处理模块重置类型的通知
 * @param[in] {result} 重置类型
 * @return Null
 * @note   MCU需要自行实现该功能
 */
void process_expand_service_reset_result(u8 result)
{
    #error "请自行实现获取蜂窝设备的工作模式结果处理代码,完成后请删除该行"
    u16 length = 0;
    switch(result) {
        case 0:
        // 模块本地重置
        break;
        case 1:
        // app远程重置
        break;
        case 2:
        // APP恢复出厂重置
        break;
    }

    length = set_cellular_uart_byte(length, EXPAND_UP_RESET_DATA);
    cellular_uart_write_frame(EXPAND_SERVICE_CMD, MCU_TX_VER, length);
}


/**
 * @brief  处理模块重置类型的通知
 * @param[in] {result} 重置类型
 * @return Null
 * @note   MCU需要自行实现该功能
 */
void process_expand_service_ota_result(u8 result)
{
    #error "请自行实现获取蜂窝设备的工作模式结果处理代码,完成后请删除该行"
    u16 length = 0;
    if(result == 1) {
        // 0x01:升级异常
    }

    length = set_cellular_uart_byte(length, EXPAND_FOTA_NOTIFY);
    cellular_uart_write_frame(EXPAND_SERVICE_CMD, MCU_TX_VER, length);
}

/**
 * @brief  扩展服务处理
 * @param[in] {p_data} 数据缓冲区
 * @return Null
 */
void expand_service_process(u8 p_data[])
{
    switch(p_data[0]) {
        case EXPAND_OPEN_RESET_SEVER:
            open_expand_service_reset_result(p_data[1]);
        break;
        case EXPAND_UP_RESET_DATA:
            process_expand_service_reset_result(p_data[1]);
        break;
        case EXPAND_FOTA_NOTIFY:
            process_expand_service_ota_result(p_data[1]);
        break;
    }
}
