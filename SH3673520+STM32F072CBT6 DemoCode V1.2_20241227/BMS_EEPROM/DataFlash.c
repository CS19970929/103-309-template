/********************************************************************************
Copyright (C), Sinowealth Electronic. Ltd.
Author: 	Sino
Version: 	V0.0
Date: 		2024/05/11
History:
	V0.0		2024/05/11		 Preliminary
********************************************************************************/
#include "DataFlash.h"

/*
; *------- <<< Use Configuration Wizard in Context Menu >>> ------------------
*/

// <h>MCU参数(SubClassID=0x00，langth=40)
// <h>电池包配置(E2uiPackConfigMap)
// <q>预放电控制
#define _EPCM_PDSGMOS_EN			0		    // BIT0; 0：不支持预放电;	1：支持预放电
// <q>均衡功能
#define _EPCM_BAL_EN				0		    // BIT1; 0：不支持均衡功能;	1：支持均衡功能
// <q>断线功能
#define _EPCM_CTO_EN				0		    // BIT2; 0：不支持断线功能;	1：支持断线功能


#define	_E2_PACKCONFIGMAP           ((_EPCM_CTO_EN<<2)\
                                    |(_EPCM_BAL_EN<<1)|(_EPCM_PDSGMOS_EN))
// </h>
// <h>单节过欠压保护释放
// <o>单节过压保护释放电压(mV)<0-5000>
#define	_E2_OVR_VOL					4150		// 4150mV
// <o>单节欠压保护释放电压(mV)<0-5000>
#define	_E2_UVR_VOL					3000		// 3000mV
// <o>单节过压保护释放延时(70mS)
#define	_E2_OVR_DELAY				14			// 0.98s
// <o>单节欠压保护释放延时(70mS)
#define	_E2_UVR_DELAY				14			// 0.98s
// </h>

// <h>高低温保护保护恢复
// <o>充电高温恢复阈值(2731+10*x℃)<2331-3581>
// <i>由于配置向导无法处理负数，低温保护输入值=2731+10*x，其中x为设置温度，如需要设置的保护温度为-10℃，输入值=2731+10*(-10)=2631
#define	_E2_OTCR_TEMP				3181		// 45℃
// <o>充电低温恢复阈值(2731+10*x℃)<2331-3581>
// <i>由于配置向导无法处理负数，低温保护输入值=2731+10*x，其中x为设置温度，如需要设置的保护温度为-10℃，输入值=2731+10*(-10)=2631
#define	_E2_UTCR_TEMP				2781		// 5℃
// <o>放电高温恢复阈值(2731+10*x℃)<2331-3581>
// <i>由于配置向导无法处理负数，低温保护输入值=2731+10*x，其中x为设置温度，如需要设置的保护温度为-10℃，输入值=2731+10*(-10)=2631
#define	_E2_OTDR_TEMP				3281		// 55℃
// <o>放电低温恢复阈值(2731+10*x℃)<2331-3581>
// <i>由于配置向导无法处理负数，低温保护输入值=2731+10*x，其中x为设置温度，如需要设置的保护温度为-10℃，输入值=2731+10*(-10)=2631
#define	_E2_UTDR_TEMP				2681		// -5℃
// <o>温度保护释放延时(70mS)
#define	_E2_OUTR_DELAY				28			// 1.96s
// </h>

// <h>总压过欠压保护
// <o>总压欠压保护阈值(0.1V)<0-1000>
#define	_E2_PACKUV_VOL				540			// 54.0V
// <o>总压欠压恢复阈值(0.1V)<0-1000>
#define	_E2_PACKUVR_VOL				580			// 58.0V
// <o>总压欠压延时(70mS)
#define	_E2_PACKUV_DELAY			14			// 0.98s
// <o>总压欠压恢复延时(70mS)
#define	_E2_PACKUVR_DELAY			14			// 0.98s
// <o>总压过压保护阈值(0.1V)<0-1000>
#define	_E2_PACKOV_VOL				840			// 84.0V
// <o>总压过压恢复阈值(0.1V)<0-1000>
#define	_E2_PACKOVR_VOL				800			// 80.0V
// <o>总压过压延时(70mS)
#define	_E2_PACKOV_DELAY			14			// 0.98s
// <o>总压过压恢复延时(70mS)
#define	_E2_PACKOVR_DELAY			14			// 0.98s
//</h>

// <h>平衡配置
// <o>平衡开启电压(mV)<0-5000>
#define	_E2_BAL_VOL					4200		// 4200mV
// <o>平衡开启充电电流(mA)<0x0-0x7ff>
#define	_E2_BALANCE_CURRENT			200			// 200mA
// <o>平衡开启压差(mV)<0-5000>
#define	_E2_BAL_VOLDIFF				50			// 50mV
// <o>平衡开启延时(70mS)
#define	_E2_BAL_DELAY				29			// 2.03s
//</h>

// <o>充电器接入检测阈值(0.1V)<0-1000>
#define	_E2_CHGCHK_VOL				840			// 84.0V
// <o>充电器拔除检测阈值(0.1V)<0-1000>
#define	_E2_CHGRCHK_VOL				600			// 60.0V
// <o>负载/充电器检测延时(70mS)
#define	_E2_LOADCHGCHK_DELAY		14			// 0.98s
// <o>零电流允许偏差(mA)<0-65535>
#define _E2_DFILTERCUR              100			// 100mA

#define _RAM_CHECK_DATA0            _RAM_CHECK_DATA
//</h>

// <h>AFE参数(SubClassID=0x0A,langth=43)
// <o>AFE的SCONF1<0x0-0xff>
#define	_E2_AFE_SCONF1              0x00
// <o>AFE的SCONF2<0x0-0xff>
#define	_E2_AFE_SCONF2              0x40		// PUMP_EN = 0,PD_EN = 1
// <o>AFE的SCONF3<0x0-0xff>
#define	_E2_AFE_SCONF3              0x40		// CGR_WK = 1
// <o>AFE的SCONF4<0x0-0xff>
#define	_E2_AFE_SCONF4              0x74
// <o>AFE的SCONF5<0x0-0xff>
#define	_E2_AFE_SCONF5              0x38		// MOS_EN = 1
// <o>AFE的SCONF6<0x0-0xff>
#define	_E2_AFE_SCONF6              0xFF
// <o>AFE的SCONF7<0x0-0xff>
#define	_E2_AFE_SCONF7              0x04
// <o>AFE的OWV_ALARMH<0x0-0xff>
#define	_E2_AFE_OWV_ALARMH          0x57
// <o>AFE的ALARML<0x0-0xff>
#define	_E2_AFE_ALARML              0xFF
// <o>  AFE的过充电保护延时(OVT)
// <0x00=> 140ms
// <0x10=> 280ms
// <0x20=> 490ms
// <0x30=> 0.98s
// <0x40=> 2.03s
// <0x50=> 3.01s
// <0x60=> 4.97s
// <0x70=> 10.01s
#define _E2_AFE_OVT					0x30
// <o> AFE的过充电保护电压(OV),5mV/Step<0-4500>
#define _E2_AFE_OV					4200
#define	_E2_AFE_OVT_OVH             (U8)(_E2_AFE_OVT | ((_E2_AFE_OV/5)>>8))
#define	_E2_AFE_OVL                 (U8)(_E2_AFE_OV/5)

// <o> AFE的过放电保护延时(OVT)
// <0x00=> 490ms
// <0x10=> 770ms
// <0x20=> 0.98s
// <0x30=> 1.47s
// <0x40=> 2.03s
// <0x50=> 3.01s
// <0x60=> 4.97s
// <0x70=> 10.01s
#define _E2_AFE_UVT					0x20
// <o> AFE的过放电保护电压(UV),5mV/Step<0-4500>
#define _E2_AFE_UV					2700
#define	_E2_AFE_UVT_UVH             (U8)(_E2_AFE_UVT | ((_E2_AFE_UV/5)>>8))
#define	_E2_AFE_UVL                 (U8)(_E2_AFE_UV/5)

// <o> AFE的放电过流1保护延时(OCD1T)
// <0x00=> 140ms
// <0x10=> 280ms
// <0x20=> 490ms
// <0x30=> 0.98s
// <0x40=> 2.03s
// <0x50=> 3.01s
// <0x60=> 4.97s
// <0x70=> 10.01s
#define _E2_AFE_OCD1T				0x30
// <o> AFE的放电过流1保护阈值(OCD1V)
// <0x00=> 5mV
// <0x01=> 10mV
// <0x02=> 15mV
// <0x03=> 20mV
// <0x04=> 25mV
// <0x05=> 30mV
// <0x06=> 35mV
// <0x07=> 40mV
// <0x08=> 45mV
// <0x09=> 50mV
// <0x0A=> 55mV
// <0x0B=> 60mV
// <0x0C=> 65mV
// <0x0D=> 70mV
// <0x0E=> 75mV
// <0x0F=> 80mV
#define _E2_AFE_OCD1V				0x09
#define	_E2_AFE_OCD1V_OCD1T			(U8)(_E2_AFE_OCD1T | _E2_AFE_OCD1V)

// <o> AFE的放电过流2保护延时(OCD2T)
// <0x00=> 25ms
// <0x10=> 50ms
// <0x20=> 75ms
// <0x30=> 100ms
// <0x40=> 125ms
// <0x50=> 150ms
// <0x60=> 175ms
// <0x70=> 200ms
// <0x80=> 225ms
// <0x90=> 250ms
// <0xA0=> 275ms
// <0xB0=> 300ms
// <0xC0=> 325ms
// <0xD0=> 350ms
// <0xE0=> 375ms
// <0xF0=> 400ms
#define _E2_AFE_OCD2T				0x30
// <o> AFE的放电过流2保护阈值(OCD2V)
// <0x00=> 10mV
// <0x01=> 20mV
// <0x02=> 30mV
// <0x03=> 40mV
// <0x04=> 50mV
// <0x05=> 60mV
// <0x06=> 70mV
// <0x07=> 80mV
// <0x08=> 90mV
// <0x09=> 100mV
// <0x0A=> 110mV
// <0x0B=> 120mV
// <0x0C=> 130mV
// <0x0D=> 140mV
// <0x0E=> 150mV
// <0x0F=> 160mV
#define _E2_AFE_OCD2V				0x09
#define	_E2_AFE_OCD2V_OCD2T			(U8)(_E2_AFE_OCD2T | _E2_AFE_OCD2V)

// <o> AFE的短路保护延时(SCT)
// <0x00=> 0us
// <0x01=> 32us
// <0x02=> 64us
// <0x03=> 96us
// <0x04=> 128us
// <0x05=> 192us
// <0x06=> 224us
// <0x07=> 256us
// <0x08=> 288us
// <0x09=> 320us
// <0x0A=> 384us
// <0x0B=> 448us
// <0x0C=> 480us
// <0x0D=> 512us
// <0x0E=> 544us
// <0x0F=> 576us
#define _E2_AFE_SCT					0x07
// <o> AFE的短路保护阈值(SCV)
// <0x00=> 2倍的放电过流2保护阈值
// <0x10=> 3倍的放电过流2保护阈值
// <0x20=> 4倍的放电过流2保护阈值
// <0x30=> 6倍的放电过流2保护阈值
#define _E2_AFE_SCV					0x00
#define	_E2_AFE_SCV_SCT				(U8)(_E2_AFE_SCT | _E2_AFE_SCV)

// <o> AFE的充电过流保护延时(OCCT)
// <0x00=> 140ms
// <0x20=> 280ms
// <0x40=> 490ms
// <0x60=> 0.98s
// <0x80=> 2.03s
// <0xA0=> 3.01s
// <0xC0=> 4.97s
// <0xE0=> 10.01s
#define _E2_AFE_OCCT				0x60
// <o> AFE的短路保护阈值(OCCV)
// <0x00=> 1.375ms
// <0x01=> 2.750ms
// <0x02=> 4.125ms
// <0x03=> 5.500ms
// <0x04=> 6.875ms
// <0x05=> 8.250ms
// <0x06=> 9.625ms
// <0x07=> 11.000ms
// <0x08=> 12.375ms
// <0x09=> 13.750ms
// <0x0A=> 15.125ms
// <0x0B=> 16.500ms
// <0x0C=> 17.875ms
// <0x0D=> 19.250ms
// <0x0E=> 20.625ms
// <0x0F=> 22.000ms
// <0x10=> 23.375ms
// <0x11=> 24.750ms
// <0x12=> 26.125ms
// <0x13=> 27.500ms
// <0x14=> 28.875ms
// <0x15=> 30.250ms
// <0x16=> 31.625ms
// <0x17=> 33.000ms
// <0x18=> 34.375ms
// <0x19=> 35.750ms
// <0x1A=> 37.125ms
// <0x1B=> 38.500ms
// <0x1C=> 39.875ms
// <0x1D=> 41.250ms
// <0x1E=> 42.625ms
// <0x1F=> 44.000ms
#define _E2_AFE_OCCV					0x0F
#define	_E2_AFE_OCCV_OCCT				(U8)(_E2_AFE_OCCT | _E2_AFE_OCCV)

// <o>AFE的OTC(2731+10*x℃)<2331-3581>
// <i>由于配置向导无法处理负数，高温保护输入值=2731+10*x，其中x为设置温度，如需要设置的保护温度为-10℃，输入值=2731+10*(-10)=2631
#define	_E2_AFE_OTC_TEMP			3231		// 50℃
// <o>AFE的OTD(2731+10*x℃)<2331-3581>
// <i>由于配置向导无法处理负数，高温保护输入值=2731+10*x，其中x为设置温度，如需要设置的保护温度为-10℃，输入值=2731+10*(-10)=2631
#define	_E2_AFE_OTD_TEMP			3331		// 60℃
// <o>AFE的UTD(2731+10*x℃)<2331-3581>
// <i>由于配置向导无法处理负数，低温保护输入值=2731+10*x，其中x为设置温度，如需要设置的保护温度为-10℃，输入值=2731+10*(-10)=2631
#define	_E2_AFE_UTC_TEMP			2731		// 0℃
// <o>AFE的UTD(2731+10*x℃)<2331-3581>
// <i>由于配置向导无法处理负数，低温保护输入值=2731+10*x，其中x为设置温度，如需要设置的保护温度为-10℃，输入值=2731+10*(-10)=2631
#define	_E2_AFE_UTD_TEMP			2631		// -10℃

#define	_RAM_CHECK_DATAA			_RAM_CHECK_DATA
//</h>


// <h>校准参数(SubClassID=0x0B,langth=15)
// <o>AFE总电压增益1(U16)<0x0-0xffff>
#define	_E2_VPACK_GAIN1				32000		// U16 E2usVPackGain1
// <o>AFE总电压增益2(U16)<0x0-0xffff>
#define	_E2_VPACK_GAIN2				12800		// U16 E2usVPackGain2
// <o>VADC电流增益(S16)<0x0-0x7fff>
#define	_E2_CURRENT_GAIN1			2913		// S16 E2ssCurrGain1
// <o>VADC零电流偏移(S16)<0x0-0x7fff>
#define	_E2_CURRENT_OFFSET1			-11			// S16 E2ssCurrOffset1
// <o>CADC电流增益(S16)<0x0-0x7fff>
#define	_E2_CURRENT_GAIN2			2913		// S16 E2ssCurrGain2
// <o>VADC零电流偏移(S16)<0x0-0x7fff>
#define	_E2_CURRENT_OFFSET2			5			// S16 E2ssCurrOffset2
// <o>外部温度1偏移量(S16)<0x0-0x7fff>
#define	_E2_TS1_OFFSET              0			// S16 E2ssTempe1Offset
// <o>外部温度2偏移量(S16)<0x0-0x7fff>
#define _E2_TS2_OFFSET              0			// S16 E2ssTempe2Offset
// <o>环境温度偏移量(S16)<0x0-0x7fff>
#define	_E2_TS3_OFFSET              0			// S16 E2ssTempe4Offset
// <o>MOS温度偏移量(S16)<0x0-0x7fff>
#define _E2_TS4_OFFSET				0			// S16 E2ssTempe5Offset
#define _RAM_CHECK_DATAB            _RAM_CHECK_DATA
//</h>


//*<<< end of configuration section >>>
/*****************************************************************************************************************/
// 备份A区
/*****************************************************************************************************************/
#if defined(__CC_ARM)
PARAMETER const dataflash1 __attribute__((at(DATAFLASH_ADDRESS_1))) = 
#elif defined(__GNUC__)
PARAMETER const dataflash1 __attribute__((section(".ARM.__at_"DATAFLASH_ADDRESS_1))) = 
#endif
{
	// 系统信息区开始 SubClassID=0x0		langth=43
	.E2usPackConfigMap		= _E2_PACKCONFIGMAP,
	
	.E2usPackUVVol			= _E2_PACKUV_VOL,  
	.E2usPackUVRVol			= _E2_PACKUVR_VOL,  
	.E2ucPackUVDelay		= _E2_PACKUV_DELAY,
	.E2ucPackUVRDelay		= _E2_PACKUVR_DELAY,
	
	.E2usPackOVVol			= _E2_PACKOV_VOL,  
	.E2usPackOVRVol			= _E2_PACKOVR_VOL,  
	.E2ucPackOVDelay		= _E2_PACKOV_DELAY, 
	.E2ucPackOVRDelay		= _E2_PACKOVR_DELAY,
	
	.E2usOVRVol				= _E2_OVR_VOL,
	.E2usUVRVol				= _E2_UVR_VOL,
	.E2usUTCRTemp			= _E2_UTCR_TEMP,
	.E2usOTCRTemp			= _E2_OTCR_TEMP,
	.E2usUTDRTemp			= _E2_UTDR_TEMP,
	.E2usOTDRTemp			= _E2_OTDR_TEMP,
	
	.E2ucOVRDelay			= _E2_OVR_DELAY,
	.E2ucUVRDelay			= _E2_UVR_DELAY,
	.E2ucOUTRDelay			= _E2_OUTR_DELAY,
	
	.E2ucLoadChgChkDelay	= _E2_LOADCHGCHK_DELAY,
	.E2usChgChkVol			= _E2_CHGCHK_VOL,
	.E2usChgRChkVol			= _E2_CHGRCHK_VOL,
	
	.E2usBalanceVol			= _E2_BAL_VOL,
	.E2usBalanceVolDiff		= _E2_BAL_VOLDIFF,
	.E2ssBalanceCur			= _E2_BALANCE_CURRENT,
	.E2ucBalanceDelay		= _E2_BAL_DELAY,
	.E2ssDfilterCur			= _E2_DFILTERCUR,
	
	.E2ucRamCheckFlg0		= _RAM_CHECK_DATA0,
	
	// AFE参数区开始 SubClassID=0xA		langth=25
	.E2usAFEOTCTemp			= _E2_AFE_OTC_TEMP,
	.E2usAFEOTDTemp			= _E2_AFE_OTD_TEMP,
	.E2usAFEUTCTemp			= _E2_AFE_UTC_TEMP,
	.E2usAFEUTDTemp			= _E2_AFE_UTD_TEMP,
	
	.E2ucAFESCONF1			= _E2_AFE_SCONF1,
	.E2ucAFESCONF2			= _E2_AFE_SCONF2,
	.E2ucAFESCONF3			= _E2_AFE_SCONF3,
	.E2ucAFESCONF4			= _E2_AFE_SCONF4,
	.E2ucAFESCONF5			= _E2_AFE_SCONF5, 
	.E2ucAFESCONF6			= _E2_AFE_SCONF6,  
	.E2ucAFESCONF7			= _E2_AFE_SCONF7,   
	.E2ucAFEOWV_ALARMH		= _E2_AFE_OWV_ALARMH,
	.E2ucAFEALARML			= _E2_AFE_ALARML,
	.E2ucAFEOVT_OVH			= _E2_AFE_OVT_OVH,
	.E2ucAFEOVL				= _E2_AFE_OVL,
	.E2ucAFEUVT_UVH			= _E2_AFE_UVT_UVH,
	.E2ucAFEUVL				= _E2_AFE_UVL,
	.E2ucAFEOCD1V_OCD1T		= _E2_AFE_OCD1V_OCD1T,
	.E2ucAFEOCD2V_OCD2T		= _E2_AFE_OCD2V_OCD2T,
	.E2ucAFESCV_SCT			= _E2_AFE_SCV_SCT,
	.E2ucAFEOCCV_OCCT		= _E2_AFE_OCCV_OCCT,
	.E2ucRamCheckFlgA		= _RAM_CHECK_DATAA,
	
	//校准参数区开始 SubClassID=0xB			langth=20
	.E2usVPackGain1			= _E2_VPACK_GAIN1,
	.E2usVPackGain2			= _E2_VPACK_GAIN2,
	.E2ssCurrGain1			= _E2_CURRENT_GAIN1,
	.E2ssCurrOffset1		= _E2_CURRENT_OFFSET1,
	.E2ssCurrGain2			= _E2_CURRENT_GAIN2,
	.E2ssCurrOffset2		= _E2_CURRENT_OFFSET2,
	.E2ssTSnOffset[0]		= _E2_TS1_OFFSET,
	.E2ssTSnOffset[1]		= _E2_TS2_OFFSET,
	.E2ssTSnOffset[2]		= _E2_TS3_OFFSET,
	.E2ssTSnOffset[3]		= _E2_TS4_OFFSET,
	.E2ucRamCheckFlgB		= _RAM_CHECK_DATAB,
	
	.E2usCheckFlag 			= 0x5AA5,
};
    
/*****************************************************************************************************************/
    //备份B区
/*****************************************************************************************************************/
#if defined(__CC_ARM)
PARAMETER const dataflash2 __attribute__((at(DATAFLASH_ADDRESS_2))) = 
#elif defined(__GNUC__)
PARAMETER const dataflash2 __attribute__((section(".ARM.__at_"DATAFLASH_ADDRESS_2))) = 
#endif
{
	// 系统信息区开始 SubClassID=0x0		langth=43
	.E2usPackConfigMap		= _E2_PACKCONFIGMAP,
	
	.E2usPackUVVol			= _E2_PACKUV_VOL,  
	.E2usPackUVRVol			= _E2_PACKUVR_VOL,  
	.E2ucPackUVDelay		= _E2_PACKUV_DELAY,
	.E2ucPackUVRDelay		= _E2_PACKUVR_DELAY,
	
	.E2usPackOVVol			= _E2_PACKOV_VOL,  
	.E2usPackOVRVol			= _E2_PACKOVR_VOL,  
	.E2ucPackOVDelay		= _E2_PACKOV_DELAY, 
	.E2ucPackOVRDelay		= _E2_PACKOVR_DELAY,
	
	.E2usOVRVol				= _E2_OVR_VOL,
	.E2usUVRVol				= _E2_UVR_VOL,
	.E2usUTCRTemp			= _E2_UTCR_TEMP,
	.E2usOTCRTemp			= _E2_OTCR_TEMP,
	.E2usUTDRTemp			= _E2_UTDR_TEMP,
	.E2usOTDRTemp			= _E2_OTDR_TEMP,
	
	.E2ucOVRDelay			= _E2_OVR_DELAY,
	.E2ucUVRDelay			= _E2_UVR_DELAY,
	.E2ucOUTRDelay			= _E2_OUTR_DELAY,
	
	.E2ucLoadChgChkDelay	= _E2_LOADCHGCHK_DELAY,
	.E2usChgChkVol			= _E2_CHGCHK_VOL,
	.E2usChgRChkVol			= _E2_CHGRCHK_VOL,
	
	.E2usBalanceVol			= _E2_BAL_VOL,
	.E2usBalanceVolDiff		= _E2_BAL_VOLDIFF,
	.E2ssBalanceCur			= _E2_BALANCE_CURRENT,
	.E2ucBalanceDelay		= _E2_BAL_DELAY,
	.E2ssDfilterCur			= _E2_DFILTERCUR,
	
	.E2ucRamCheckFlg0		= _RAM_CHECK_DATA0,
	
	// AFE参数区开始 SubClassID=0xA		langth=25
	.E2usAFEOTCTemp			= _E2_AFE_OTC_TEMP,
	.E2usAFEOTDTemp			= _E2_AFE_OTD_TEMP,
	.E2usAFEUTCTemp			= _E2_AFE_UTC_TEMP,
	.E2usAFEUTDTemp			= _E2_AFE_UTD_TEMP,
	
	.E2ucAFESCONF1			= _E2_AFE_SCONF1,
	.E2ucAFESCONF2			= _E2_AFE_SCONF2,
	.E2ucAFESCONF3			= _E2_AFE_SCONF3,
	.E2ucAFESCONF4			= _E2_AFE_SCONF4,
	.E2ucAFESCONF5			= _E2_AFE_SCONF5, 
	.E2ucAFESCONF6			= _E2_AFE_SCONF6,  
	.E2ucAFESCONF7			= _E2_AFE_SCONF7,   
	.E2ucAFEOWV_ALARMH		= _E2_AFE_OWV_ALARMH,
	.E2ucAFEALARML			= _E2_AFE_ALARML,
	.E2ucAFEOVT_OVH			= _E2_AFE_OVT_OVH,
	.E2ucAFEOVL				= _E2_AFE_OVL,
	.E2ucAFEUVT_UVH			= _E2_AFE_UVT_UVH,
	.E2ucAFEUVL				= _E2_AFE_UVL,
	.E2ucAFEOCD1V_OCD1T		= _E2_AFE_OCD1V_OCD1T,
	.E2ucAFEOCD2V_OCD2T		= _E2_AFE_OCD2V_OCD2T,
	.E2ucAFESCV_SCT			= _E2_AFE_SCV_SCT,
	.E2ucAFEOCCV_OCCT		= _E2_AFE_OCCV_OCCT,
	.E2ucRamCheckFlgA		= _RAM_CHECK_DATAA,
	
	//校准参数区开始 SubClassID=0xB			langth=20
	.E2usVPackGain1			= _E2_VPACK_GAIN1,
	.E2usVPackGain2			= _E2_VPACK_GAIN2,
	.E2ssCurrGain1			= _E2_CURRENT_GAIN1,
	.E2ssCurrOffset1		= _E2_CURRENT_OFFSET1,
	.E2ssCurrGain2			= _E2_CURRENT_GAIN2,
	.E2ssCurrOffset2		= _E2_CURRENT_OFFSET2,
	.E2ssTSnOffset[0]		= _E2_TS1_OFFSET,
	.E2ssTSnOffset[1]		= _E2_TS2_OFFSET,
	.E2ssTSnOffset[2]		= _E2_TS3_OFFSET,
	.E2ssTSnOffset[3]		= _E2_TS4_OFFSET,
	.E2ucRamCheckFlgB		= _RAM_CHECK_DATAB,
	
	.E2usCheckFlag 			= 0x5AA5,
};
