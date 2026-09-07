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
    HOST_FAULT unMdlFault_Third;
} g_stCellInfoReport;
static struct { uint8_t u8ErrFlag_CBC_DSG; } System_ErrFlag;
UINT8 SeriesNum = 19;
BMS_PARAMETERS g_bmsParameters = BMS_PARAMETERS_DEFAULT;
enum { ERROR_AFE1, ERROR_REMOVE_AFE1 };
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

GPIO_TypeDef host_gpioa, host_gpiob;
static uint8_t ram[256], tx_frame[128], rx_frame[128];
static unsigned bits, frames, write_count[256];
static int selected, fail_reads, fail_writes, corrupt_echo, corrupt_read_reg=-1;
static int frame_bad_read, frame_bad_write, frame_bad_echo, force_fet_off, flag_unlocked;
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
            if (addr==0x41) { flag_unlocked=(value&0x80)!=0; ram[0x5B]=force_fet_off ? 0 : value & 3; }
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
    assert(cfg->SPI_NSS==SPI_NSS_Soft && cfg->SPI_BaudRatePrescaler==AFE3520_CFG_SPI_DIVIDER);
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
    fail_reads=fail_writes=corrupt_echo=force_fet_off=0; corrupt_read_reg=-1;
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
    static const unsigned untouched[]={0x46,0x47,0x48,0x4D,0x4F};
    unsigned i;
    healthy();
    assert(Bms3520_BuildAfeConfig(&cfg));
    assert(ram[0x41]==0 && ram[0x42]==0 && ram[0x43]==19);
    assert(ram[0x44]==(0x18|(AFE3520_CFG_WDT_ENABLE<<2)) && ram[0x45]==0x7F);
    assert(ram[0x49]==3 && ram[0x4A]==0x52 && ram[0x4B]==2 && ram[0x4C]==0x12);
    assert(ram[0x4E]==3 && ram[0x50]==7);
    assert(ram[0x51]==0x86 && ram[0x52]==0x53 && ram[0x53]==0x77 && ram[0x54]==0xD7);
    for(i=0;i<sizeof(untouched)/sizeof(untouched[0]);++i) {
        assert(write_count[untouched[i]]==0); ram[untouched[i]]=0xA6;
    }
    assert(Afe3520_ApplyConfig(&cfg)==AFE3520_OK);
    for(i=0;i<sizeof(untouched)/sizeof(untouched[0]);++i) assert(ram[untouched[i]]==0xA6);
    CHECK_CASE("reference profile / watchdog variant / preserve unspecified registers");

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

    healthy(); Bms3520_ProtectionService();
    Bms3520_RequestMos(GPIO_CHG,1); Bms3520_RequestMos(GPIO_DSG,1);
    assert((ram[0x41]&3)==3 && reported_chg && reported_dsg);
    Afe3520_MarkConfigDirty(); Bms3520_ProtectionService();
    assert((ram[0x41]&3)==3 && reported_chg && reported_dsg);
    CHECK_CASE("config rewrite restores requested MOS commands despite old cache");

    force_fet_off=1; Bms3520_ProtectionService();
    assert((ram[0x41]&3)==3 && !reported_chg && !reported_dsg);
    force_fet_off=0; Bms3520_RequestMos(GPIO_CHG,0);
    assert(!(GPIOB->odr & PIN_M_CCC) && !reported_chg && reported_dsg);
    Bms3520_SetSystemBlock(1); Bms3520_ProtectionService();
    assert(!(ram[0x41]&3) && !(GPIOB->odr&PIN_M_CCC));
    CHECK_CASE("actual FET feedback, M_CCC follows charge, global fault closes MOS");

    healthy(); ram[0x59]|=8; Bms3520_ProtectionService();
    assert(!(ram[0x59]&8) && !Afe3520_ConfigDirty());
    before=write_count[0x44]; Bms3520_ProtectionService();
    assert(write_count[0x44]==before);
    CHECK_CASE("RST2 triggers one config repair, not false open-wire protection");

    healthy(); Bms3520_ProtectionService();
    before=write_count[0x40]; MosStartup_ApplyInitialState();
    assert(write_count[0x40]==before && (ram[0x41]&3)==3);
    CHECK_CASE("MOS startup does not overwrite SCONF1");
    healthy(); Bms3520_ProtectionService();
    Bms3520_RequestMos(GPIO_CHG,1); Bms3520_RequestMos(GPIO_DSG,1);
    ram[0x59]|=4; Bms3520_ProtectionService();
    assert(!(ram[0x41]&3));
    for(i=0;i<26;++i) Bms3520_ProtectionService();
    assert(!(ram[0x59]&4) && (ram[0x41]&3)==3);
    CHECK_CASE("WDT latch blocks MOS then clears after stable recovery");
    healthy();
    s_snapshot.valid=1; s_snapshot.cadcRaw=1000;
    { UINT16 code=0;
      assert(Afe3520_ReadCalibratedCurrentCode(&code) && code==327);
      s_snapshot.cadcRaw=(UINT16)(INT16)-1000;
      assert(Afe3520_ReadCalibratedCurrentCode(&code) && (INT16)code==-327);
      assert(!Afe3520_ReadCalibratedCurrentCode(0));
    }
    CHECK_CASE("native CADC preserves signed current calibration without MTP alias");
    healthy(); Bms3520_ProtectionService();
    Bms3520_RequestMos(GPIO_CHG,1); Bms3520_RequestMos(GPIO_DSG,1);
    fail_reads=100; fail_writes=100;
    Bms3520_ProtectionService();
    assert(afe_error && !s_snapshot.valid && !s_prot.mosFeedbackValid);
    assert(Bms3520_GetBlockMask() & AFE3520_BLOCK_GLOBAL_AFE_COMM);
    assert(!(GPIOB->odr & PIN_M_CCC));
    fail_reads=fail_writes=0;
    for(i=1;i<AFE3520_CFG_COMM_RECOVERY_TICKS;i++) {
        Bms3520_ProtectionService(); assert(afe_error && !(ram[0x41]&3));
    }
    Bms3520_ProtectionService(); assert(!afe_error && (ram[0x41]&3)==3);
    CHECK_CASE("persistent bus fault blocks MOS; recovery requires consecutive verified cycles");

    healthy(); ram[0x58]=AFE3520_FLAG1_SC;
    assert(Bms3520_ApplyAndVerifyAfeConfig() && (ram[0x58]&AFE3520_FLAG1_SC));
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
    healthy(); g_stCellInfoReport.u16Temperature[0]=1000; g_stCellInfoReport.u16Temperature[1]=200;
    Bms3520_ProtectionService();
    assert(g_stCellInfoReport.unMdlFault_Third.bits.b1CellChgOtp && g_stCellInfoReport.unMdlFault_Third.bits.b1CellChgUtp);
    g_stCellInfoReport.u16Temperature[0]=g_stCellInfoReport.u16Temperature[1]=650;
    for(i=0;i<BMS3520_SW_RECOVERY_STABLE_TICKS;i++) Bms3520_ProtectionService();
    assert(!g_stCellInfoReport.unMdlFault_Third.bits.b1CellChgOtp && !g_stCellInfoReport.unMdlFault_Third.bits.b1CellChgUtp);
    g_stCellInfoReport.u16Temperature[0]=0; Bms3520_ProtectionService();
    assert(g_stCellInfoReport.unMdlFault_Third.bits.b1CellChgUtp);
    CHECK_CASE("hot/cold latches recover independently; zero temperature sensor is not ignored");
    SeriesNum=21; assert(!Bms3520_BuildAfeConfig(&cfg));
    printf("PASS: %u cases, watchdog=%d\n",tests,AFE3520_CFG_WDT_ENABLE);
    return 0;
}
