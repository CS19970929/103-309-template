#ifndef CONF_H
#define CONF_H

// #define __test__
// #define _DEBUG_

// #define __VIRTURE_CURR__

#define SNum 			4

//#define TERNARYLI		//潩??�?潩�?�?
#define LIFEPO			//潩潩潩?�?潩�?�?


#define CS_Res			1
#define CS_Res_Num		10

#define __FUNC__HEAT__
// #define __FUNC__LED__
#define __FUNC_RTC__


//#define _DI_SWITCH_SYS_ONOFF	//DI??????
//#define _DI_SWITCH_DSG_ONOFF	//DI?????????????MOS
#define _DI_SWITCH_longKEY_ONOFF

#ifdef __FUNC__HEAT__
#define CHG_LOWTEMP_PARAM   120
#define HEAT_OPEN_CURR      50
#else
#define CHG_LOWTEMP_PARAM   380
#define HEAT_OPEN_CURR      500
#endif // DEBUG

#define   CURR_80A      0
#define   CURR_100A     1
#define   CURR_150A     2
#define   CURR_200A     3
#define   CURR_250A     4


#define   LEVEL_CURR     CURR_150A


#ifdef __FUNC_RTC__

#define __SLEEP_VNORMAL__             	2200
#define	__SLEEP_TIMENORMAL__	          10080
#define __SLEEP_VLOW__     		          3000
#define	__SLEEP_TIMEVLOW__		          1440

#else

#define __SLEEP_VNORMAL__             	4200
#define	__SLEEP_TIMENORMAL__	          10080
// #define	__SLEEP_TIMENORMAL__	          (30 * 24 * 60)
#define __SLEEP_VLOW__     		          3000
#define	__SLEEP_TIMEVLOW__		          1440

#endif


#define AFE_COV_H       4200
#define AFE_CUV_H       2000
#define AFE_OCC_H       20
#define AFE_ODC_H       110



#ifdef __test__

#define  ENTER_RTC_TIME     (10 * 10)

#define RTC_DEEP_TIME      (4)

#define RTC_TIME            20

#define RTC_SOC_OCV         3

#define APP_DEEP_TIME       20

#else

#define  ENTER_RTC_TIME     (60 * 10)
// #define  ENTER_RTC_TIME     (30)

#define RTC_DEEP_TIME      (3 * 60 * 24 * 7)

#define RTC_TIME            30
// #define RTC_TIME            10

#define RTC_SOC_OCV         (3 * 60 * 6)

#define APP_DEEP_TIME       (60 * 10)

#define NORMAL_L3_SLEEP_UNIT  60
#define NORMAL_L2_SLEEP_UNIT  60

#endif


// #define _SLEEP_WITH_CURRENT

// #define __CTLC__

// #define BSP_Printf		printf
#define BSP_Printf(...)

#define DEBUG_LINE() 																												\
  BSP_Printf("Log: [%s:%s] line = %d\n", __FILE__, __func__, __LINE__)
#define DEBUG_INFO(fmt, ...)                                                \
  BSP_Printf("Log: [%s:%s] line = %d\n" fmt "\n", __FILE__, __func__, __LINE__, \
         ##__VA_ARGS__)


//todo 电压bug

#endif
