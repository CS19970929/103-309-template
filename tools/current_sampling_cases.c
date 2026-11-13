/* Host stubs for the real startup calibration and signed-current functions. */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "CurrentCalibration.h"
typedef uint8_t UINT8; typedef uint16_t UINT16; typedef uint32_t UINT32;
typedef int16_t INT16; typedef int32_t INT32;
/* CURRENT_TYPES */
enum { MDL_ICHG, MDL_IDSG, KB_NUM };
static struct { AFE_CURRENT_RUNTIME cur; } s_data;
static struct { MTP_REG_CONF REG_MTP_CONF; } SH367309_Reg_Store;
static struct { UINT16 u16Current; } SH367309_Read_AFE1;
static struct { UINT16 u16Ichg,u16IDischg; } g_stCellInfoReport;
static struct { UINT16 CHG,DSG; UINT8 isdebugenable; } sys_time;
static UINT16 g_u16CalibCoefK[KB_NUM];
static INT16 g_i16CalibCoefB[KB_NUM];
static UINT32 g_u32CS_Res_AFE=1000U;
static UINT8 hardware_conf,bstatus,write_fail,conf_fail,status_fail,sample_fail;
static INT16 samples[2];
static unsigned sample_reads,waits,writes;
#define U16_SwapEndian(value) ((UINT16)((((UINT16)(value))<<8)|(((UINT16)(value))>>8)))
static void __delay_ms(UINT16 ms) { assert(ms==300U); ++waits; }
static UINT8 MTPWrite(UINT8 address,UINT8 length,UINT8 *data) {
    assert(address==MTP_CONF && length==1U); ++writes;
    if(write_fail) return 0U;
    hardware_conf=*data; return 1U;
}
static UINT8 MTPRead(UINT8 address,UINT8 length,UINT8 *data) {
    if(address==MTP_CONF) {
        assert(length==1U); if(conf_fail) return 0U;
        *data=hardware_conf; return 1U;
    }
    if(address==MTP_BSTATUS3) {
        assert(length==1U); if(status_fail) return 0U;
        *data=bstatus; return 1U;
    }
    assert(address==MTP_ADC2 && length==2U);
    if(sample_fail) return 0U;
    assert(sample_reads<2U);
    data[0]=(UINT8)(((UINT16)samples[sample_reads])>>8);
    data[1]=(UINT8)samples[sample_reads++]; return 1U;
}
/* CURRENT_FUNCTIONS */
static void reset_case(void) {
    memset(&s_data,0,sizeof(s_data)); memset(&sys_time,0,sizeof(sys_time));
    memset(&SH367309_Reg_Store,0,sizeof(SH367309_Reg_Store));
    memset(&g_stCellInfoReport,0,sizeof(g_stCellInfoReport));
    hardware_conf=bstatus=write_fail=conf_fail=status_fail=sample_fail=0U;
    writes=sample_reads=waits=0U; samples[0]=10; samples[1]=12;
    g_u16CalibCoefK[MDL_ICHG]=g_u16CalibCoefK[MDL_IDSG]=1024;
    g_i16CalibCoefB[MDL_ICHG]=g_i16CalibCoefB[MDL_IDSG]=0;
    g_u32CS_Res_AFE=1000U;
}
int main(void) {
    unsigned before;
    reset_case(); AfeCurrent_StartupZeroCal();
    assert(s_data.cur.zeroStatus==AFE_CURRENT_ZERO_VALID && s_data.cur.zeroOffsetRawX4==44);
    assert(waits==2U && sample_reads==2U && (hardware_conf&0x78U)==0x08U);
    before=writes; AfeCurrent_StartupZeroCal(); assert(writes==before && sample_reads==2U);
    SH367309_Read_AFE1.u16Current=11U; DataLoad_Current();
    assert(AfeCurrent_GetCurrent_mA()==0 && !g_stCellInfoReport.u16Ichg && !g_stCellInfoReport.u16IDischg);
    SH367309_Read_AFE1.u16Current=2158U; DataLoad_Current();
    assert(AfeCurrent_GetCurrent_mA()==20000 && g_stCellInfoReport.u16Ichg==200);
    SH367309_Read_AFE1.u16Current=(UINT16)(INT16)-2136; DataLoad_Current();
    assert(AfeCurrent_GetCurrent_mA()==-20000 && g_stCellInfoReport.u16IDischg==200);
    reset_case();write_fail=1U;AfeCurrent_StartupZeroCal();
    assert(s_data.cur.zeroStatus==AFE_CURRENT_ZERO_CONFIG_WRITE_ERROR && !waits && !sample_reads);
    reset_case();conf_fail=1U;AfeCurrent_StartupZeroCal();
    assert(s_data.cur.zeroStatus==AFE_CURRENT_ZERO_CONFIG_READBACK_ERROR && !sample_reads);
    reset_case();status_fail=1U;AfeCurrent_StartupZeroCal();
    assert(s_data.cur.zeroStatus==AFE_CURRENT_ZERO_SAMPLE_READ_ERROR && !sample_reads);
    reset_case();bstatus=1U;AfeCurrent_StartupZeroCal();
    assert(s_data.cur.zeroStatus==AFE_CURRENT_ZERO_FET_ACTIVE && !sample_reads);
    reset_case();sample_fail=1U;AfeCurrent_StartupZeroCal(); assert(!s_data.cur.zeroOffsetRawX4);
    reset_case();samples[1]=66;AfeCurrent_StartupZeroCal();
    assert(s_data.cur.zeroStatus==AFE_CURRENT_ZERO_OUT_OF_RANGE && !s_data.cur.zeroOffsetRawX4);
    reset_case();samples[1]=26;AfeCurrent_StartupZeroCal();
    assert(s_data.cur.zeroStatus==AFE_CURRENT_ZERO_UNSTABLE && !s_data.cur.zeroOffsetRawX4);
    reset_case();SH367309_Read_AFE1.u16Current=2147U;DataLoad_Current();
    assert(AfeCurrent_GetCurrent_mA()==20000);
    g_u16CalibCoefK[MDL_ICHG]=2048U;DataLoad_Current();assert(AfeCurrent_GetCurrent_mA()==40000);
    g_u16CalibCoefK[MDL_ICHG]=1024U;g_i16CalibCoefB[MDL_ICHG]=1024;
    DataLoad_Current();assert(AfeCurrent_GetCurrent_mA()==21000);
    reset_case();sys_time.isdebugenable=1U;sys_time.CHG=17U;sys_time.DSG=0U;
    SH367309_Read_AFE1.u16Current=2147U;DataLoad_Current();
#ifdef __VIRTURE_CURRENT__
    assert(AfeCurrent_GetCurrent_mA()==1700 && g_stCellInfoReport.u16Ichg==17U);
    sys_time.CHG=3U;sys_time.DSG=11U;DataLoad_Current();
    assert(AfeCurrent_GetCurrent_mA()==-800 && g_stCellInfoReport.u16Ichg==3U && g_stCellInfoReport.u16IDischg==11U);
    sys_time.isdebugenable=0U;DataLoad_Current();assert(AfeCurrent_GetCurrent_mA()==20000);
    puts("PASS retained virtual current: CHG/DSG in A*10, signed mA, enable/disable and real-sample fallback");
#else
    assert(AfeCurrent_GetCurrent_mA()==20000 && g_stCellInfoReport.u16Ichg==200U);
    puts("PASS production current: virtual controls have no effect when feature is disabled");
#endif
    puts("PASS real current sampling: 300 ms independent samples, FET/write/read/range/stability gates, zero offset and K/B");
    return 0;
}
