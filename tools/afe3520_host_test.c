/* Exercise the real C driver with an independent GPIO-level SPI peripheral.
 * No driver functions are replaced: command echoes, CRC, ACK and register
 * writes travel through the production bit-banged transport. */
#define MAIN_H
#include "afe3520_test_stubs/conf.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "afe3520/Afe3520App.h"
#include "afe3520/BmsProtection3520.h"
#include "afe3520/Afe3520Config.h"

typedef union {
    uint16_t all;
    struct {
        unsigned b1CellOvp:1, b1CellUvp:1, b1IchgOcp:1, b1IdischgOcp:1;
        unsigned b1CellChgUtp:1, b1CellChgOtp:1, b1CellDischgUtp:1, b1CellDischgOtp:1;
    } bits;
} HOST_FAULT;
static struct {
    uint16_t u16VCell[20], u16Temperature[4], u16Ichg, u16IDischg;
    HOST_FAULT unMdlFault_Third, unMdlFault_Second;
} g_stCellInfoReport;
static struct { uint8_t u8ErrFlag_CBC_DSG; } System_ErrFlag;
static RCC_ClocksTypeDef clocks = {72000000,72000000,36000000,72000000};
static uint16_t timer_count, timer_prescaler;
static unsigned timer_on, timer_clock, timer_stuck, selected_divider;
void RCC_GetClocksFreq(RCC_ClocksTypeDef *p) { *p=clocks; }
void RCC_APB1PeriphClockCmd(int p,int on) { assert(p==RCC_APB1Periph_TIM4); timer_clock=on; }
void RCC_APB1PeriphResetCmd(int t,int on) { assert(t==RCC_APB1Periph_TIM4); if(on) { timer_count=0; timer_on=0; } }
void TIM_SetAutoreload(int t,uint16_t value) { (void)t; assert(value==65535); }
void TIM_PrescalerConfig(int t,uint16_t value,int mode) { (void)t; (void)mode; timer_prescaler=value; }
void TIM_SetCounter(int t,uint16_t value) { (void)t; timer_count=value; }
uint16_t TIM_GetCounter(int t) { (void)t; if(timer_on && !timer_stuck) ++timer_count; return timer_count; }
void TIM_Cmd(int t,int on) { (void)t; timer_on=on; }
enum { IchgOcp_Second, IdischgOcp_Second };
static void FaultWarnRecord2(int code) { (void)code; }
UINT8 SeriesNum = 19;
UINT32 g_u32CS_Res_AFE = CS_Res_Num * 1000U / CS_Res;
enum { ERROR_AFE1, ERROR_REMOVE_AFE1, ERROR_EEPROM_STORE };
static int afe_error, reported_chg, reported_dsg;
static void System_ERROR_UserCallback(int code) { afe_error = (code == ERROR_AFE1); }
static void SystemRuntime_SetMosStatus(UINT8 c, UINT8 d) { reported_chg=c; reported_dsg=d; }
static void SystemRuntime_SetAfeStatus(UINT8 index, UINT8 ok) { (void)index; (void)ok; }
static void Delay1ms(UINT16 ms) { (void)ms; }

/* Include production sources directly so private state transitions can also
 * be asserted. The real main.h is suppressed, board dependencies are mocked. */
#include "../103 + 309/Project/Source/afe3520/Afe3520.c"
#include "../103 + 309/Project/Source/afe3520/Afe3520App.c"
#include "../103 + 309/Project/Source/afe3520/BmsProtection3520.c"
#include "../103 + 309/Project/Source/MosStartup.c"

/* Exercise the real software-write path; only persistent media is mocked. */
struct RS485MSG { uint8_t u16Buffer[64], AckType, ErrorType; };
enum { RS485_ACK_NEG=1, RS485_ERROR_DATA_INVALID=3, RS485_ERROR_CMD_INVALID=4 };
static uint16_t saved_soft[24], candidate_soft[24];
static uint16_t saved_hw[24], candidate_hw[24];
static unsigned config_saves, config_save_ok=1;
static void EEPROM_ConfigEditBegin(void) {
    memcpy(candidate_soft,saved_soft,sizeof(saved_soft));
    Bms3520_EncodeHardware(Bms3520_GetHardwareConfig(),candidate_hw);
}
static uint8_t EEPROM_ConfigEditSetAfeWord(uint16_t index,uint16_t value)
{ if(index>=24) return 0; candidate_soft[index]=value; return 1; }
static uint8_t EEPROM_ConfigEditCommit(void)
{ if(!config_save_ok) return 0; ++config_saves; memcpy(saved_soft,candidate_soft,sizeof(saved_soft)); memcpy(saved_hw,candidate_hw,sizeof(saved_hw)); return 1; }
static uint8_t EEPROM_ConfigEditSetHardware(const uint16_t words[24]) {
    BMS3520_HARDWARE_CONFIG p;
    if(!Bms3520_DecodeHardware(words,&p)) return 0;
    memcpy(candidate_hw,words,sizeof(candidate_hw)); return 1;
}
#include "../103 + 309/Project/Source/BmsParameters.c"
#include "hardware_handler.inc"

GPIO_TypeDef host_gpioa, host_gpiob;
static uint8_t ram[256], tx_frame[128], rx_frame[128];
static unsigned bits, frames, write_count[256];
static int selected, fail_reads, fail_writes, corrupt_echo, corrupt_read_reg=-1;
static int frame_bad_read, frame_bad_write, frame_bad_echo, force_fet_off, force_reverse_dsg, flag_unlocked;
static unsigned tests;

static uint8_t model_crc(const uint8_t *p, unsigned n)
{
    unsigned crc=0, i, j;
    for (i=0; i<n; ++i) {
        crc ^= p[i];
        for (j=0; j<8; ++j) crc = (crc & 128) ? ((crc*2)^7) : crc*2;
        crc &= 255;
    }
    return (uint8_t)crc;
}

static uint8_t response(unsigned pos)
{
    uint8_t cmd=tx_frame[0], addr=tx_frame[1];
    if (!pos) return frame_bad_echo ? 0 : 0xFF;
    if (pos<=3) return tx_frame[pos-1];
    if (cmd==0x02) {
        unsigned n=tx_frame[2];
        if (pos==4+n) return model_crc(rx_frame, pos) ^ (frame_bad_read ? 1 : 0);
        if (pos<4+n) return ram[addr+pos-4] ^ ((addr+pos-4)==(unsigned)corrupt_read_reg ? 1 : 0);
    }
    if (cmd==0x01 && (addr==0x58 || addr==0x59) && !flag_unlocked) return 0xFF;
    if (cmd==0x01 || cmd==0x0B)
        return (model_crc(tx_frame,3)==tx_frame[3] && !frame_bad_write) ? 0xA5 : 0xFF;
    return 0xFF;
}

void GPIO_ResetBits(GPIO_TypeDef *p, uint16_t pin)
{
    p->odr &= (uint16_t)~pin;
    if (p==GPIOA && pin==PIN_CS_SPI) {
        selected=1; bits=0; ++frames;
        memset(tx_frame,0,sizeof(tx_frame)); memset(rx_frame,0,sizeof(rx_frame));
        frame_bad_read=fail_reads>0; frame_bad_write=fail_writes>0;
        frame_bad_echo=corrupt_echo>0;
    }
}
void GPIO_SetBits(GPIO_TypeDef *p, uint16_t pin)
{
    p->odr |= pin;
    if (p==GPIOA && pin==PIN_CS_SPI && selected) {
        selected=0;
        if (tx_frame[0]==2 && fail_reads>0) --fail_reads;
        if (corrupt_echo>0) --corrupt_echo;
        if (tx_frame[0]==1 && bits==40 && rx_frame[4]==0xA5 && !frame_bad_echo) {
            unsigned addr=tx_frame[1]; uint8_t value=tx_frame[2];
            ++write_count[addr];
            /* LTCLR self-clears; flags are cleared by writing zero to selected bits. */
            if (addr==0x58 || addr==0x59) { ram[addr] &= value; flag_unlocked=0; }
            else ram[addr]=(addr==0x41) ? value & 0x7F : value;
            if (addr==0x41) { flag_unlocked=(value&0x80)!=0; ram[0x5B]=force_fet_off ? 0 : value & 3;
                if(force_reverse_dsg && (ram[0x44]&AFE3520_SCONF5_MOS_EN)) ram[0x5B]|=AFE3520_BSTATUS1_DSG_FET; }
        }
        if (tx_frame[0]==1 && fail_writes>0) --fail_writes;
    }
}
void GPIO_WriteBit(GPIO_TypeDef *p, uint16_t pin, BitAction value)
{ if (value) GPIO_SetBits(p,pin); else GPIO_ResetBits(p,pin); }
uint8_t GPIO_ReadInputDataBit(GPIO_TypeDef *p, uint16_t pin)
{
    unsigned pos=bits/8, shift=7-(bits%8); uint8_t value;
    if (p!=GPIOA || pin!=PIN_MISO_SPI) return (p->odr & pin)!=0;
    assert(selected && pos<sizeof(tx_frame));
    assert(GPIOA->odr & PIN_SCLK_SPI);
    if (GPIOA->odr & PIN_MOSI_SPI) tx_frame[pos] |= (uint8_t)(1U<<shift);
    value=response(pos); rx_frame[pos]=value; ++bits;
    return (value>>shift)&1;
}
uint8_t GPIO_ReadOutputDataBit(GPIO_TypeDef *p, uint16_t pin) { return (p->odr&pin)!=0; }
void GPIO_Init(GPIO_TypeDef *p, GPIO_InitTypeDef *g) { (void)p; (void)g; }
void RCC_APB2PeriphClockCmd(int clock, int on) { (void)clock; (void)on; }

SPI_TypeDef host_spi;
static unsigned hw_fault, hw_fault_resets, hw_resets, hw_rx_ready;
static uint8_t hw_rx;
void GPIO_PinRemapConfig(unsigned map, int on) { assert(map==GPIO_Remap_SPI1 && !on); }
void SPI_I2S_DeInit(SPI_TypeDef *spi) {
    assert(GPIOA->odr & PIN_CS_SPI); spi->enabled=0; hw_rx_ready=0; ++hw_resets;
    if(hw_fault_resets && !--hw_fault_resets) hw_fault=0;
}
void SPI_StructInit(SPI_InitTypeDef *spi) { memset(spi,0,sizeof(*spi)); }
void SPI_Init(SPI_TypeDef *spi, SPI_InitTypeDef *cfg) {
    (void)spi;
    assert(cfg->SPI_Direction==SPI_Direction_2Lines_FullDuplex && cfg->SPI_Mode==SPI_Mode_Master);
    assert(cfg->SPI_CPOL==SPI_CPOL_High && cfg->SPI_CPHA==SPI_CPHA_2Edge);
    assert(cfg->SPI_DataSize==SPI_DataSize_8b && cfg->SPI_FirstBit==SPI_FirstBit_MSB);
    assert(cfg->SPI_NSS==SPI_NSS_Soft);
    selected_divider=cfg->SPI_BaudRatePrescaler;
    assert(clocks.PCLK2_Frequency <= AFE3520_CFG_SPI_TARGET_HZ * selected_divider);
    assert(selected_divider==2 || clocks.PCLK2_Frequency > AFE3520_CFG_SPI_TARGET_HZ * (selected_divider/2));
}
void SPI_Cmd(SPI_TypeDef *spi, int on) { spi->enabled=on; }
void SPI_NSSInternalSoftwareConfig(SPI_TypeDef *spi, unsigned state) { (void)spi; assert(state==SPI_NSSInternalSoft_Set); }
FlagStatus SPI_I2S_GetFlagStatus(SPI_TypeDef *spi, uint16_t flag) {
    (void)spi;
    if(hw_fault==flag) return flag<=2 ? RESET : SET;
    if(flag==SPI_I2S_FLAG_TXE) return SET;
    if(flag==SPI_I2S_FLAG_RXNE) return hw_rx_ready ? SET : RESET;
    return RESET;
}
void SPI_I2S_SendData(SPI_TypeDef *spi, uint16_t value) {
    unsigned pos=bits/8;
    assert(spi->enabled && selected && !hw_rx_ready && pos<sizeof(tx_frame));
    tx_frame[pos]=(uint8_t)value; hw_rx=response(pos); rx_frame[pos]=hw_rx;
    bits+=8; hw_rx_ready=1;
}
uint16_t SPI_I2S_ReceiveData(SPI_TypeDef *spi) { (void)spi; hw_rx_ready=0; return hw_rx; }

static void healthy(void)
{
    unsigned i;
    hw_fault=hw_fault_resets=0;
    memset(ram,0,sizeof(ram)); memset(write_count,0,sizeof(write_count));
    memset(&g_stCellInfoReport,0,sizeof(g_stCellInfoReport));
    fail_reads=fail_writes=corrupt_echo=force_fet_off=force_reverse_dsg=0; corrupt_read_reg=-1;
    for (i=0;i<20;++i) {
        ram[0x69+2*i]=0x52; ram[0x6A+2*i]=0x80; /* 21120 -> 3300mV */
        g_stCellInfoReport.u16VCell[i]=3300;
    }
    for (i=0;i<4;++i) { ram[0x5D+2*i]=0x40; g_stCellInfoReport.u16Temperature[i]=650; }
    ram[0x65]=0x40; /* Safe internal temperature raw code. */
    SeriesNum=19; Bms3520_SetSystemBlock(0); Afe3520_AppInit();
    assert(!afe_error);
}

#define CHECK_CASE(name) do { ++tests; puts(name); } while(0)
int main(void)
{
    AFE3520_REG_CONFIG cfg;
    uint8_t value;
    unsigned before;

    unsigned i;
    healthy();
    assert(Bms3520_BuildAfeConfig(&cfg));
    assert(ram[0x41]==0 && ram[0x42]==0 && ram[0x43]==19);
    assert(ram[0x44]==AFE3520_CFG_EFFECTIVE_SCONF5 && ram[0x45]==(BMS3520_CFG_HW_PROTECTION ? s_hwConfig.enableMask : 0U));
    assert(ram[0x49]==3 && ram[0x4A]==0x52 && ram[0x4B]==2 && ram[0x4C]==0x12);
    assert(ram[0x4E]==3 && ram[0x50]==7);
    assert(ram[0x51]==0x86 && ram[0x52]==0x53 && ram[0x53]==0x77 && ram[0x54]==0xD7);
    assert(ram[0x46]==4 && ram[0x47]==0x57 && ram[0x48]==0xFF && ram[0x4D]==0x39 && ram[0x4F]==7);
    assert(cfg.writeMask == (1UL << AFE3520_CONFIG_LENGTH)-1UL);
    CHECK_CASE("physical-unit defaults explicitly configure all registers");
    {
        BMS3520_HARDWARE_CONFIG original=*Bms3520_GetHardwareConfig(), decoded;
        uint16_t words[24], roundtrip[24];
        const uint16_t expected[24]={0x3520,1,850,530,0x0339,0x0707,
            3550,0,1935,0,27513,0,50574,1,4150,2900,50,70,5,65521,1000,5000,0x017F,0};
        Bms3520_EncodeHardware(&original,words);
        assert(!memcmp(words,expected,sizeof(words)));
        assert(Bms3520_DecodeHardware(words,&decoded));
        Bms3520_EncodeHardware(&decoded,roundtrip);
        assert(!memcmp(words,roundtrip,sizeof(words)));
        words[1]=2; assert(!Bms3520_DecodeHardware(words,&decoded)); words[1]=1;
        words[2]|=0x8000; assert(!Bms3520_DecodeHardware(words,&decoded)); words[2]=850;
        words[22]|=0x0200; assert(!Bms3520_DecodeHardware(words,&decoded)); words[22]=0x017F;
        words[23]=1; assert(!Bms3520_DecodeHardware(words,&decoded)); words[23]=0;
        words[14]=4250; assert(!Bms3520_DecodeHardware(words,&decoded)); words[14]=4150;
        words[16]=0x8000; assert(!Bms3520_DecodeHardware(words,&decoded)); words[16]=50;
        memset(words,0xFF,sizeof(words)); assert(Bms3520_RestoreHardware(words));
        assert(!memcmp(&original,Bms3520_GetHardwareConfig(),sizeof(original)));
        original.ovMv=4300; original.scOcd2Multiplier=6; original.scDelayUs=576;
        original.dsgUtOhm=123456; original.recoveryMs=6000;
        Bms3520_EncodeHardware(&original,words); assert(Bms3520_RestoreHardware(words));
        Bms3520_EncodeHardware(Bms3520_GetHardwareConfig(),roundtrip);
        assert(!memcmp(words,roundtrip,sizeof(words)));
        assert(Bms3520_RestoreHardware(expected));
    }
    CHECK_CASE("versioned hardware image: golden vector, lossless roundtrip, reserved bits, hysteresis, legacy restore");
    {
        struct RS485MSG request={0};
        BMS_PARAMETERS original=g_bmsParameters;
        BMS3520_HARDWARE_CONFIG hw=*Bms3520_GetHardwareConfig();
        uint8_t dirty=Afe3520_ConfigDirty();
        assert(Bms3520_ParamImageValid(&g_bmsParameters));
        request.u16Buffer[2]=0x24; request.u16Buffer[5]=24; request.u16Buffer[6]=48;
        for(i=0;i<24;i++) {
            uint16_t v=AfeParam_AtConst((UINT16)i)->curValue;
            request.u16Buffer[7+2*i]=(uint8_t)(v>>8); request.u16Buffer[8+2*i]=(uint8_t)v;
        }
        assert(Sci_WrRegs_0x10_AFE_Parameters(0,&request) && !request.AckType && config_saves==1);
        request.u16Buffer[7]=0x0E; request.u16Buffer[8]=0xD8; /* 3800 mV */
        config_save_ok=0; Sci_WrRegs_0x10_AFE_Parameters(0,&request);
        assert(request.AckType && config_saves==1 && !memcmp(&original,&g_bmsParameters,sizeof(original)));
        config_save_ok=1; request.AckType=0; Sci_WrRegs_0x10_AFE_Parameters(0,&request);
        assert(!request.AckType && config_saves==2 && g_bmsParameters.u16VcellOvp.curValue==3800);
        assert(Afe3520_ConfigDirty()==dirty && !memcmp(&hw,Bms3520_GetHardwareConfig(),sizeof(hw)));
        request.u16Buffer[9]=0x0F; request.u16Buffer[10]=0xA0; /* recovery 4000 > trigger */
        Sci_WrRegs_0x10_AFE_Parameters(0,&request); assert(request.AckType && config_saves==2);
        assert(EEPROM_ResetData_AFE_ParametersToDefault());
        assert(!memcmp(&original,&g_bmsParameters,sizeof(original)));
    }
    CHECK_CASE("software default writes succeed; failed save cannot change runtime; hardware stays independent; reset restores defaults");
    {
        struct RS485MSG request={0};
        BMS3520_HARDWARE_CONFIG original=*Bms3520_GetHardwareConfig(), next=original;
        uint16_t words[24]; unsigned saves=config_saves;
        next.ovMv=4300; Bms3520_EncodeHardware(&next,words);
        request.u16Buffer[2]=0x25;request.u16Buffer[5]=24;request.u16Buffer[6]=48;
        for(i=0;i<24;i++) {request.u16Buffer[7+2*i]=(uint8_t)(words[i]>>8);request.u16Buffer[8+2*i]=(uint8_t)words[i];}
        before=frames; config_save_ok=0; Test_WriteHardware(&request);
        assert(request.AckType && config_saves==saves && frames==before);
        assert(!memcmp(&original,Bms3520_GetHardwareConfig(),sizeof(original)));
        config_save_ok=1; request.AckType=0; request.u16Buffer[5]=23; Test_WriteHardware(&request);
        assert(request.AckType && config_saves==saves && frames==before);
        request.AckType=0;request.u16Buffer[5]=24;request.u16Buffer[3]=1;Test_WriteHardware(&request);
        assert(request.AckType && config_saves==saves && frames==before);
        request.AckType=0;request.u16Buffer[3]=0;corrupt_read_reg=0x4A;Test_WriteHardware(&request);
        assert(!request.AckType && config_saves==saves+1 && !s_prot.configValid && Afe3520_ConfigDirty());
        assert(!memcmp(saved_hw,words,sizeof(words)) && Bms3520_GetHardwareConfig()->ovMv==4300);
        corrupt_read_reg=-1;for(i=0;i<4;i++) Bms3520_Service200ms();
        assert(s_prot.configValid && !Afe3520_ConfigDirty());
        Bms3520_SetHardwareConfig(&original);
    }
    CHECK_CASE("real hardware write handler rejects partial writes; failed persistence is inert; saved pending config recovers");
    {
        BMS3520_HARDWARE_CONFIG saved=*Bms3520_GetHardwareConfig(), next=saved;
        before=frames; next.ovMv=4251;
        assert(Bms3520_SetHardwareConfig(&next)==BMS3520_CONFIG_INVALID && frames==before);
        next=saved; next.scDelayUs=160; assert(!Bms3520_ValidateHardwareConfig(&next));
        next=saved; next.chgOtRecoveryC=-32768; assert(!Bms3520_ValidateHardwareConfig(&next));
        next=saved; next.chgUtRecoveryC=32767; assert(!Bms3520_ValidateHardwareConfig(&next));
        next=saved; next.ovMv=4300; next.ovDelayMs=980;
        assert(Bms3520_SetHardwareConfig(&next)==BMS3520_CONFIG_VERIFIED);
        assert(ram[0x49]==0x33 && ram[0x4A]==0x5C && !(ram[0x41]&3));
        Bms3520_Service200ms();
        next=saved; corrupt_read_reg=0x4A;
        assert(Bms3520_SetHardwareConfig(&next)==BMS3520_CONFIG_PENDING);
        assert(!s_prot.configValid && !s_snapshot.valid && !(GPIOB->odr&PIN_M_CCC));
        corrupt_read_reg=-1;
        for(i=0;i<4;i++) Bms3520_Service200ms();
        assert(s_prot.configValid && !afe_error);
        assert(!memcmp(Bms3520_GetHardwareConfig(),&saved,sizeof(saved)));
    }
    CHECK_CASE("runtime config rejects bad units/hysteresis, verifies writes, inhibits MOS during failed apply");

    healthy(); Bms3520_Service200ms();
    Bms3520_RequestMos(GPIO_CHG,1); Bms3520_RequestMos(GPIO_DSG,1);
    g_stCellInfoReport.u16VCell[0]=5000;
    for(i=0;i<100;i++) Bms3520_Service200ms();
    assert(!!(s_prot.chargeBlocks & AFE3520_BLOCK_CHG_SW_OV)==BMS3520_CFG_SW_PROTECTION);
    assert(s_prot.requestedCharge==1);
    g_stCellInfoReport.u16VCell[0]=3300;
    for(i=0;i<10;i++) Bms3520_Service200ms();
    assert(!(s_prot.chargeBlocks & AFE3520_BLOCK_CHG_SW_OV));
    CHECK_CASE("software protection selected only in modes 1/3; recovery preserves requested MOS");

    healthy(); Bms3520_Service200ms();
    Bms3520_RequestMos(GPIO_CHG,1); Bms3520_RequestMos(GPIO_DSG,1);
    ram[0x58]=AFE3520_FLAG1_OV|AFE3520_FLAG1_UV;
    g_stCellInfoReport.u16VCell[0]=2000;
    for(i=0;i<30;i++) Bms3520_Service200ms();
#if BMS3520_CFG_HW_PROTECTION
    assert(!(ram[0x58]&AFE3520_FLAG1_OV) && (ram[0x58]&AFE3520_FLAG1_UV));
    assert(s_prot.dischargeBlocks & AFE3520_BLOCK_DSG_HW_UV);
#endif
    assert(!(s_prot.chargeBlocks & AFE3520_BLOCK_CHG_HW_OV));
    Bms3520_RequestMos(GPIO_DSG,0);
    g_stCellInfoReport.u16VCell[0]=3300;
    for(i=0;i<30;i++) Bms3520_Service200ms();
    assert(!(ram[0x58] & (AFE3520_FLAG1_OV|AFE3520_FLAG1_UV)));
    assert(!s_prot.requestedDischarge && !(ram[0x41]&2));
    CHECK_CASE("each hardware latch recovers independently; recovery never cancels manual MOS-off");
#if BMS3520_CFG_SW_PROTECTION && BMS3520_CFG_HW_PROTECTION
    healthy(); Bms3520_Service200ms(); Bms3520_RequestMos(GPIO_CHG,1);
    g_stCellInfoReport.u16VCell[0]=5000;
    for(i=0;i<100;i++) Bms3520_Service200ms();
    ram[0x58]=AFE3520_FLAG1_OCC;
    g_stCellInfoReport.u16VCell[0]=3300; g_stCellInfoReport.u16Ichg=20;
    for(i=0;i<10;i++) Bms3520_Service200ms();
    assert(!(s_prot.chargeBlocks & AFE3520_BLOCK_CHG_SW_OV));
    assert(s_prot.chargeBlocks & AFE3520_BLOCK_CHG_HW_OCC);
    assert(!(ram[0x41]&1) && s_prot.requestedCharge);
    g_stCellInfoReport.u16Ichg=0;
    for(i=0;i<30;i++) Bms3520_Service200ms();
    assert(!(s_prot.chargeBlocks & AFE3520_BLOCK_CHG_HW_OCC) && (ram[0x41]&1));
    CHECK_CASE("combined mode keeps blocking until BOTH independent protection sources recover");
#endif
    healthy();


    assert(Afe3520_UpdateMeasurements()==0 && g_afe3520Measurements.u16VCell[0]==3300);
    fail_reads=5; assert(Afe3520_UpdateMeasurements()!=0 && !Afe3520_IsReady());
    assert(Afe3520_UpdateMeasurements()==0);
    CHECK_CASE("sample success=0, failure!=0, next frame recovers");

    before=frames; fail_reads=2;
    assert(Afe3520_Read(0x44,&value,1)==AFE3520_OK && frames-before==3);
    before=frames; fail_writes=2;
    assert(Afe3520_Write(0x50,7)==AFE3520_OK && frames-before==3);
    corrupt_echo=5; assert(Afe3520_Read(0x44,&value,1)==AFE3520_ERR_SPI);
    assert(Afe3520_Read(0x44,&value,1)==AFE3520_OK);
    assert(!selected && (GPIOA->odr & PIN_CS_SPI));
    CHECK_CASE("CRC/NACK/echo fault injection, bounded retry, CS idle recovery");

    corrupt_read_reg=0x4A;
    assert(Afe3520_ApplyConfig(&cfg)==AFE3520_ERR_VERIFY && Afe3520_ConfigDirty());
    corrupt_read_reg=-1;
    assert(Afe3520_ApplyConfig(&cfg)==AFE3520_OK && !Afe3520_ConfigDirty());
    CHECK_CASE("real config mismatch rejected; self-clearing LTCLR accepted");

    ram[0x58]=0x43; ram[0x59]=0x27;
    assert(Afe3520_ClearFlags(1,0x20)==AFE3520_OK);
    assert(ram[0x58]==0x42 && ram[0x59]==7);
    CHECK_CASE("per-register LTCLR unlock; unrelated flag latches preserved");

    healthy(); Bms3520_Service200ms();
    Bms3520_RequestMos(GPIO_CHG,1); Bms3520_RequestMos(GPIO_DSG,1);
    assert((ram[0x41]&3)==3 && reported_chg && reported_dsg);
    Afe3520_MarkConfigDirty(); Bms3520_Service200ms();
    assert((ram[0x41]&3)==3 && reported_chg && reported_dsg);
    CHECK_CASE("config rewrite restores requested MOS commands despite old cache");

    force_fet_off=1; Bms3520_Service200ms();
    assert((ram[0x41]&3)==3 && !reported_chg && !reported_dsg);
    force_fet_off=0; Bms3520_RequestMos(GPIO_CHG,0);
    assert(!(GPIOB->odr & PIN_M_CCC) && !reported_chg && reported_dsg);
    Bms3520_SetSystemBlock(1); Bms3520_Service200ms();
    assert(!(ram[0x41]&3) && !(GPIOB->odr&PIN_M_CCC));
    CHECK_CASE("actual FET feedback, M_CCC follows charge, global fault closes MOS");
    healthy(); Bms3520_Service200ms();
    force_reverse_dsg=1; Bms3520_RequestMos(GPIO_DSG,0);
    assert(!s_prot.requestedDischarge && !(ram[0x41]&AFE3520_SCONF2_DSGMOS));
    assert(s_prot.actualDischarge==AFE3520_CFG_COMMON_PORT);
    force_reverse_dsg=0; Bms3520_Service200ms();
    assert(!s_prot.requestedDischarge && !s_prot.actualDischarge);
    CHECK_CASE("common-port reverse FET feedback does not overwrite manual request; separate port disables MOS_EN");


    healthy(); ram[0x59]|=8; Bms3520_Service200ms();
    assert(!(ram[0x59]&8) && !Afe3520_ConfigDirty());
    before=write_count[0x44]; Bms3520_Service200ms();
    assert(write_count[0x44]==before);
    CHECK_CASE("RST2 triggers one config repair, not false open-wire protection");

    healthy(); Bms3520_Service200ms();
    before=write_count[0x40]; MosStartup_ApplyInitialState();
    assert(write_count[0x40]==before && (ram[0x41]&3)==3);
    CHECK_CASE("MOS startup does not overwrite SCONF1");
    healthy();
    assert(!s_prot.requestedCharge && !s_prot.requestedDischarge);
    MosStartup_ApplyInitialState();
    assert(s_prot.requestedCharge && s_prot.requestedDischarge && !(ram[0x41]&3));
    Bms3520_Service200ms();
    assert((ram[0x41]&3)==3 && reported_chg && reported_dsg);
    Bms3520_RequestMos(GPIO_DSG,0); Bms3520_Service200ms();
    assert(!s_prot.requestedDischarge && !reported_dsg);
    CHECK_CASE("boot seeds requests before first service, waits for protection, never overrides later manual-off");

    healthy(); Bms3520_Service200ms();
    Bms3520_RequestMos(GPIO_CHG,1); Bms3520_RequestMos(GPIO_DSG,1);
    ram[0x59]|=4; Bms3520_Service200ms();
    assert(!(ram[0x41]&3));
    for(i=0;i<26;++i) Bms3520_Service200ms();
    assert(!(ram[0x59]&4) && (ram[0x41]&3)==3);
    CHECK_CASE("WDT latch blocks MOS then clears after stable recovery");
    healthy();
    s_snapshot.valid=1; s_snapshot.cadcRaw=0;
    ram[0x91]=1000>>8; ram[0x92]=1000&255;
    { UINT16 code=0;
      assert(Afe3520_ReadCalibratedCurrentCode(&code) && code==368);
      ram[0x91]=((UINT16)(INT16)-1000)>>8; ram[0x92]=((UINT16)(INT16)-1000)&255;
      assert(Afe3520_ReadCalibratedCurrentCode(&code) && (INT16)code==-368);
      assert(!Afe3520_ReadCalibratedCurrentCode(0));
      assert(Afe3520_CurrentMaFromCadc(29127)==400000);
      assert(Afe3520_CurrentMaFromCadc(-29127)==-400000);
      assert(Afe3520_CurrentMaFromCadc(0)==0);
      assert(Afe3520_CurrentMaFromCadc(1000)==13732);
      g_u32CS_Res_AFE=1000; assert(Afe3520_CurrentMaFromCadc(29127)==100000);
      g_u32CS_Res_AFE=4000;
      fail_reads=100; assert(!Afe3520_ReadCalibratedCurrentCode(&code));
    }
    CHECK_CASE("native CADC preserves signed current calibration without MTP alias");
    healthy(); Bms3520_Service200ms();
    Bms3520_RequestMos(GPIO_CHG,1); Bms3520_RequestMos(GPIO_DSG,1);
    fail_reads=100; fail_writes=100;
    Bms3520_Service200ms();
    assert(afe_error && !s_snapshot.valid && !s_prot.mosFeedbackValid);
    assert(Bms3520_GetBlockMask() & AFE3520_BLOCK_GLOBAL_AFE_COMM);
    assert(!(GPIOB->odr & PIN_M_CCC));
    fail_reads=fail_writes=0;
    for(i=1;i<AFE3520_CFG_COMM_RECOVERY_TICKS;i++) {
        Bms3520_Service200ms(); assert(afe_error && !(ram[0x41]&3));
    }
    Bms3520_Service200ms(); assert(!afe_error && (ram[0x41]&3)==3);
    CHECK_CASE("persistent bus fault blocks MOS; recovery requires consecutive verified cycles");

    healthy(); ram[0x58]=AFE3520_FLAG1_SC;
    assert(Bms3520_ApplyAndVerifyAfeConfig());
    assert(!!(ram[0x58]&AFE3520_FLAG1_SC)==BMS3520_CFG_HW_PROTECTION);
    CHECK_CASE("configuration repair preserves live protection latches");

#if AFE3520_CFG_USE_HARDWARE_SPI
    for(i=1;i<=5;i++) {
        healthy(); before=hw_resets; hw_fault=i; hw_fault_resets=1;
        assert(Afe3520_Read(0x41,&value,1)==AFE3520_OK);
        assert(hw_resets==before+1 && (GPIOA->odr & PIN_CS_SPI));
    }
    CHECK_CASE("TXE/RXNE/BSY timeout and OVR/MODF reset SPI and retry successfully");
    healthy(); hw_fault=2; hw_fault_resets=100;
    assert(Afe3520_Read(0x41,&value,1)==AFE3520_ERR_TIMEOUT);
    assert(!s_snapshot.valid && Afe3520_ConfigDirty() && (GPIOA->odr & PIN_CS_SPI));
    hw_fault=hw_fault_resets=0;
    assert(Afe3520_Read(0x41,&value,1)==AFE3520_OK);
    CHECK_CASE("persistent RXNE timeout is bounded; subsequent frame can recover");
#endif
#if BMS3520_CFG_SW_PROTECTION
    healthy(); g_stCellInfoReport.u16Temperature[0]=1000; g_stCellInfoReport.u16Temperature[1]=200;
    Bms3520_Service200ms();
    assert(g_stCellInfoReport.unMdlFault_Third.bits.b1CellChgOtp && g_stCellInfoReport.unMdlFault_Third.bits.b1CellChgUtp);
    g_stCellInfoReport.u16Temperature[0]=g_stCellInfoReport.u16Temperature[1]=650;
    for(i=0;i<BMS3520_SW_RECOVERY_STABLE_TICKS;i++) Bms3520_Service200ms();
    assert(!g_stCellInfoReport.unMdlFault_Third.bits.b1CellChgOtp && !g_stCellInfoReport.unMdlFault_Third.bits.b1CellChgUtp);
    g_stCellInfoReport.u16Temperature[0]=0; Bms3520_Service200ms();
    assert(g_stCellInfoReport.unMdlFault_Third.bits.b1CellChgUtp);
    CHECK_CASE("hot/cold latches recover independently; zero temperature sensor is not ignored");
#endif
    SeriesNum=21; assert(!Bms3520_BuildAfeConfig(&cfg));
    { const uint32_t mhz[]={8,24,48,72}; unsigned n;
      for(n=0;n<4;n++) {
        clocks.HCLK_Frequency=mhz[n]*1000000; clocks.PCLK1_Frequency=clocks.HCLK_Frequency/2;
        clocks.PCLK2_Frequency=clocks.HCLK_Frequency;
        healthy(); assert(Afe3520_Service()==AFE3520_OK);
        assert(!timer_on && !timer_clock && s_timerHz<=8000000);
#if AFE3520_CFG_USE_HARDWARE_SPI
        assert(selected_divider==(n==0?16:n==1?64:n==2?128:256));
        clocks.PCLK2_Frequency/=2; assert(Afe3520_Service()==AFE3520_OK);
        assert(selected_divider==(n==0?8:n==1?32:n==2?64:128));
#endif
        Afe3520_TimerStart(); timer_count=65530;
#if AFE3520_CFG_USE_HARDWARE_SPI
        { uint16_t before_timeout=timer_count;
          hw_fault=SPI_I2S_FLAG_RXNE; hw_rx_ready=0;
          assert(!Afe3520_WaitSpiFlag(SPI_I2S_FLAG_RXNE,SET));
          assert((uint16_t)(timer_count-before_timeout) >= (s_timerHz/1000));
          hw_fault=0; s_frameError=AFE3520_OK;
        }
#endif
        Afe3520_SpiDelayUs(10); assert(s_frameError==AFE3520_OK);
        timer_stuck=1; Afe3520_SpiDelayUs(1); assert(s_frameError==AFE3520_ERR_TIMEOUT);
        timer_stuck=0; Afe3520_EndFrame();
      }
    }
    CHECK_CASE("8/24/48/72 MHz and changed APB2 adapt per frame; timer wrap and stopped timer remain bounded");
    printf("PASS: %u cases, mode=%d SPI=%d WDT=%d commonPort=%d\n", tests, BMS3520_CFG_PROTECTION_MODE, AFE3520_CFG_USE_HARDWARE_SPI, AFE3520_CFG_WDT_ENABLE, AFE3520_CFG_COMMON_PORT);
    return 0;
}
