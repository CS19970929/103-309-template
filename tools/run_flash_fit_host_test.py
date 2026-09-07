"""Differential checks against the pre-size-optimization firmware (6651592).

Run in a VS developer shell (cl) or with gcc/clang. Generated C and executables
stay in the user temp directory. The Flash model currently requires Windows.
"""
import os
from pathlib import Path
import re
import shutil
import subprocess

ROOT = Path(__file__).resolve().parents[1]
SOURCE = "103 + 309/Project/Source/"
BASELINE = "6651592"
OUT = Path(os.environ.get("LOCALAPPDATA", os.environ.get("TEMP", "/tmp"))) / "CodexTemp/afe-reference-align/flash-fit-tests"
CC = shutil.which("cl") or shutil.which("gcc") or shutil.which("clang")
COMMON = """#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
typedef uint8_t UINT8; typedef uint16_t UINT16; typedef uint32_t UINT32;
typedef int16_t INT16; typedef int32_t INT32;
static uint32_t seed=17;
static uint32_t next_random(void) { seed=seed*1664525U+1013904223U; return seed; }
"""


def read(name, old=False):
    data = subprocess.check_output(["git", "show", f"{BASELINE}:{SOURCE}{name}"], cwd=ROOT) if old else (ROOT / SOURCE / name).read_bytes()
    return data.decode("latin1").replace("\r\n", "\n")


def clean(text):
    return re.sub(r"/\*.*?\*/|//[^\n]*", "", text, flags=re.S)


def function(text, name):
    match = re.search(r"^(?:static )?[\w]+\s+" + name + r"\([^)]*\)\s*\{.*?^\}", text, re.S | re.M)
    assert match, name
    return clean(match.group()) + "\n"


def macro(text, name):
    # Join continuations first to avoid a greedy first-line match.
    match = re.search(r"^#define\s+" + name + r"\b[^\n]*", text.replace("\\\n", ""), re.M)
    assert match, name
    return clean(match.group()) + "\n"


def run(name, code):
    path = OUT / f"{name}.c"
    exe = OUT / f"{name}.exe"
    path.write_text(COMMON + code, encoding="ascii")
    if Path(CC).name.lower() == "cl.exe":
        cmd = [CC, "/nologo", "/O2", "/std:c11", str(path), f"/Fe:{exe}", f"/Fo:{OUT}/"]
    else:
        cmd = [CC, "-O2", "-std=c99", str(path), "-o", str(exe)]
    result = subprocess.run(cmd, cwd=OUT, capture_output=True, text=True, errors="replace")
    if result.returncode:
        raise RuntimeError(result.stdout + result.stderr)
    return subprocess.check_output([str(exe)], cwd=OUT)


def scalar_checks():
    fault = read("Fault.h")
    old_fault = read("Fault.h", True)
    other = read("DataDeal.h")
    code = "#define E2P_PARA_NUM_PROTECT 65U\n#define E2P_PARA_NUM_OTHER_ELEMENT1 32U\n"
    for name in ("BMS_PROTECT_GROUP_WORDS", "BMS_PROTECT_DELAY_MIN", "BMS_PROTECT_DELAY_MAX", "E2P_PROTECT_GROUP_LIMITS"):
        code += macro(fault, name)
    for name in ("E2P_PROTECT_MIN_PRT", "E2P_PROTECT_MAX_PRT"):
        code += macro(old_fault, name)
    for name in ("OtherElement_min", "OtherElement_max"):
        code += macro(other, name)
    code += """
static const UINT16 s_protectGroupLimits[13][2]=E2P_PROTECT_GROUP_LIMITS;
static const UINT16 g_u16OtherParamMin[32]=OtherElement_min, g_u16OtherParamMax[32]=OtherElement_max;
static const UINT16 old_min[65]=E2P_PROTECT_MIN_PRT, old_max[65]=E2P_PROTECT_MAX_PRT;
"""
    code += function(read("EEPROM.c"), "BmsParam_ValueInRange")
    code += function(read("PubFunc.c", True), "Sci_CRC16RTU").replace("Sci_CRC16RTU", "OldCrc")
    code += function(read("Flash.c"), "StorageFlash_Crc16Update")
    code += function(read("Flash.c"), "StorageFlash_Crc16")
    code += function(read("PubFunc.c"), "Sci_CRC16RTU")
    code += """
int main(void) {
    unsigned i,v,n; UINT8 frame[255];
    for(i=0;i<97;++i) for(v=0;v<65536;++v) {
        unsigned min=i<65?old_min[i]:g_u16OtherParamMin[i-65];
        unsigned max=i<65?old_max[i]:g_u16OtherParamMax[i-65];
        assert(BmsParam_ValueInRange(i<65,i<65?i:i-65,v)==(v>=min && v<=max));
    }
    assert(!BmsParam_ValueInRange(1,65,1000) && !BmsParam_ValueInRange(0,32,1000));
    for(n=0;n<1000;++n) {
        for(i=0;i<255;++i) frame[i]=(UINT8)next_random();
        for(i=0;i<256;++i) assert(Sci_CRC16RTU(frame,i)==OldCrc(frame,i));
    }
    assert(Sci_CRC16RTU((UINT8 *)"123456789",9)==0x4B37);
    puts("PASS: 6,356,992 parameter values and 256,000 CRC frames");
    return 0;
}
"""
    print(run("scalars", code).decode().strip())


def serial_checks():
    header = clean(read("Sci_Upper.h"))
    definitions = "\n".join(line for line in header.splitlines() if line.startswith("#define") and re.search(r"\b(?:RS485_|SCI_TX_BUF_LEN)", line))
    definitions += "\n" + re.search(r"enum RS485_CMD_E\s*\{.*?\};", header, re.S).group()
    definitions += re.search(r"struct RS485MSG\s*\{.*?\};", header, re.S).group()
    hardware = """
typedef struct { volatile UINT16 SR,DR,CR1; } USART_TypeDef;
typedef int IRQn_Type; typedef int GPIO_TypeDef;
static USART_TypeDef uart1,uart2;
#define USART1 (&uart1)
#define USART2 (&uart2)
#define USART_SR_PE 1U
#define USART_SR_FE 2U
#define USART_SR_NE 4U
#define USART_SR_ORE 8U
#define USART_SR_IDLE 16U
#define USART_SR_RXNE 32U
#define USART_SR_TC 64U
#define USART_SR_TXE 128U
#define USART_FLAG_TC USART_SR_TC
#define USART_CR1_RE 4U
#define USART_CR1_TE 8U
#define USART_CR1_IDLEIE 16U
#define USART_CR1_RXNEIE 32U
#define USART_CR1_TCIE 64U
#define USART_CR1_TXEIE 128U
#define _COMMOM_UPPER_SCI2
static UINT32 primask, external;
#define __DMB() ((void)0)
#define __get_PRIMASK() primask
#define __disable_irq() (primask=1U)
#define __enable_irq() (primask=0U)
static void SleepDeal_RecordExternalComm(void) { ++external; }
static void USART_ClearFlag(USART_TypeDef *u,UINT16 f) { u->SR &= ~f; }
static void Sci_DataInit(struct RS485MSG *s) { memset(s,0,sizeof(*s)); }
/* Deterministic business handlers isolate transport behavior. AFE host tests
 * separately exercise the real software/hardware parameter handlers. */
static void CRC_verify(struct RS485MSG *s) { if (s->u16Buffer[s->ptr_no-1]==0xEE) s->AckType=RS485_ACK_NEG; }
static void Sci_Deal_ReadRegs_0x03(struct RS485MSG *s) { s->u16RdRegByteNum=4; }
static void Sci_Deal_WrReg_0x06(struct RS485MSG *s) { s->u16RdRegByteNum=2; }
static void Sci_Deal_WrRegs_0x10(struct RS485MSG *s) { s->u16RdRegByteNum=s->u16Buffer[6]; }
static void Sci_ACK_0x03(struct RS485MSG *s) { s->AckLenth=s->u16Buffer[0]?7:0; s->u16Buffer[2]=s->AckType; }
static void Sci_ACK_0x06_0x10(struct RS485MSG *s) { s->AckLenth=s->u16Buffer[0]?8:0; s->u16Buffer[2]=s->AckType; }
"""
    harness = r"""
static void trace(struct SCI_PORT_RUNTIME *p, struct RS485MSG *m) {
    unsigned i;
    printf("%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u:",
      p->pstUsart->CR1,p->pstUsart->SR,p->pstUsart->DR,p->u16TxIndex,p->u16TxLength,
      p->u8FramePending,ERROR_COUNT,primask,external,u8FlashUpdateE2PROM,u8FlashUpdateFlag,
      Sci_PortIsBusy(p),m->ptr_no,m->csr);
    for(i=0;i<sizeof(*m);++i) printf("%02x",((unsigned char *)m)[i]);
    putchar('\n');
}
static void event(struct SCI_PORT_RUNTIME *p, struct RS485MSG *m, unsigned sr,unsigned dr) {
    p->pstUsart->SR=sr; p->pstUsart->DR=dr; Sci_PortIRQHandler(p); trace(p,m);
}
int main(void) {
    unsigned round,k; struct SCI_PORT_RUNTIME *p; struct RS485MSG *m;
    static const UINT8 frame[11]={1,16,0x25,0,0,1,2,0,7,0,0};
    for(round=0;round<1000;++round) {
        p=round&1?&g_stSciPort1:&g_stSciPort2;
        m=round&1?&g_stCurrentMsgPtr_SCI1:&g_stCurrentMsgPtr_SCI2;
        Sci_PortAbortTransfer(p); primask=round&1;
        for(k=0;k<11;++k) event(p,m,USART_SR_RXNE,frame[k]);
        Sci_PortService(p); trace(p,m);
        u8FlashUpdateE2PROM=round&1;
        for(k=0;k<9;++k) event(p,m,USART_SR_TXE,0);
        event(p,m,USART_SR_TC,0);
        for(k=0;k<30;++k) {
            unsigned r=next_random(); event(p,m,r&255U,(r>>8)&255U);
            if(r&256U) Sci_PortService(p);
            trace(p,m);
        }
        /* Explicit partial frames, invalid opcode/address, CRC failure, and
         * RXNE+IDLE at the final byte for read/single-write frames. */
        Sci_PortAbortTransfer(p);
        event(p,m,USART_SR_RXNE,round%3==0?0:1);
        event(p,m,USART_SR_RXNE,round%2?3:6);
        for(k=0;k<5;++k) event(p,m,USART_SR_RXNE,0);
        event(p,m,USART_SR_RXNE|USART_SR_IDLE,round%5==0?0xEE:0);
        Sci_PortService(p); trace(p,m);
    }
    assert(!Sci_PortIsBusy(0)); Sci_PortService(0);
    return 0;
}
"""
    outputs = []
    for old in (True, False):
        text = read("Sci_Upper.c", old)
        prefix = text[text.index("static struct RS485MSG"):text.index("void Sci_WrRegs_0x10_CalibCoef")]
        prefix = re.sub(r"^typedef char SCI_.*?;\n|^struct stCell_Info.*?;\n", "", prefix, flags=re.M)
        start = text.index("static void Sci_ModbusResetMessage(struct RS485MSG *s)\n{")
        end = text.index("static void Sci_InitCommonPort", start)
        body = clean(text[start:end])
        error = "(*p->pu16ErrorCounter)" if old else "p->errorCount"
        outputs.append(run("serial_old" if old else "serial_new", definitions + hardware + clean(prefix) + body + harness.replace("ERROR_COUNT", error)))
    assert outputs[0] == outputs[1], "UART observable state/response diverged"
    print("PASS: both UARTs, 1,000 frame/error sequences, identical byte-by-byte state traces")


def flash_checks():
    model = """
#include <windows.h>
#define FLASH_STORAGE_RECORD_ALIGNMENT 4U
#define FLASH_STORAGE_PAGE_SIZE 1024U
#define FLASH_STORAGE_RECORD_VERSION 2U
#define FLASH_FLAG_EOP 1U
#define FLASH_FLAG_PGERR 2U
#define FLASH_FLAG_WRPRTERR 4U
#define ERROR_EEPROM_STORE 1U
typedef enum { FLASH_COMPLETE, FLASH_ERROR_PG } FLASH_Status;
static unsigned fail_at,steps,errors;
static UINT16 FlashReadOneHalfWord(UINT32 addr) { return *(UINT16 *)(uintptr_t)addr; }
static void FLASH_Unlock(void) {}
static void FLASH_Lock(void) {}
static void FLASH_ClearFlag(unsigned flags) { (void)flags; }
static void System_ERROR_UserCallback(unsigned error) { errors+=error; }
static FLASH_Status FLASH_ErasePage(UINT32 addr) {
    if(++steps==fail_at) return FLASH_ERROR_PG;
    memset((void *)(uintptr_t)addr,255,1024); return FLASH_COMPLETE;
}
static FLASH_Status FLASH_ProgramHalfWord(UINT32 addr,UINT16 value) {
    if(++steps==fail_at) return FLASH_ERROR_PG;
    *(UINT16 *)(uintptr_t)addr &= value; return FLASH_COMPLETE;
}
UINT8 StorageFlash_IsAreaBlank(UINT32 addr,UINT16 length);
"""
    harness = r"""
int main(void) {
    unsigned round,i; UINT8 payload[80],loaded[80]; unsigned char *flash;
    flash=VirtualAlloc((void *)0x08000000,65536,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE);
    assert(flash==(unsigned char *)0x08000000); memset(flash,255,65536);
    for(round=0;round<600;++round) {
        uint32_t hash=2166136261U;
        unsigned saved,valid;
        for(i=0;i<80;++i) payload[i]=(UINT8)next_random();
        fail_at=round%3?round%48+1:0; steps=0;
        if(round&1) {
            saved=StorageFlash_SavePair(0x0800E800,0x0800EC00,0x43464731,payload,80);
            valid=StorageFlash_LoadPair(0x0800E800,0x0800EC00,0x43464731,80,loaded);
        } else {
            saved=StorageFlash_SaveJournalPair(0x0800F000,0x0800F400,0x534F4331,payload,24);
            valid=StorageFlash_LoadJournalPair(0x0800F000,0x0800F400,0x534F4331,24,loaded);
        }
        if(saved) assert(valid && !memcmp(payload,loaded,round&1?80:24));
        for(i=0;i<65536;++i) hash=(hash^flash[i])*16777619U;
        printf("%u %u %u %u %u\n",saved,valid,steps,errors,hash);
        /* Corrupt committed/partial records between reboot-style loads. */
        if(round%11==0) flash[0xE800+(next_random()%4096)]^=1;
    }
    return 0;
}
"""
    outputs = []
    for old in (True, False):
        text = read("Flash.c", old)
        body = clean(text[text.index("typedef struct"):text.index("FLASH_Status FlashWriteOneHalfWord")])
        if old:
            body = body.replace("static UINT8 StorageFlash_IsAreaBlank", "UINT8 StorageFlash_IsAreaBlank")
        outputs.append(run("flash_old" if old else "flash_new", model + body + harness))
    assert outputs[0] == outputs[1], "Flash selection/write/recovery diverged"
    print("PASS: 600 CONFIG/SOC writes, erase/program faults and corruption; identical Flash images/results")


def vector_checks():
    vendor = ROOT / "103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers"
    old = (vendor / "startup_stm32f10x_hd.s").read_text()
    new = read("startup_stm32f103xb.s")
    vectors = lambda s: re.findall(r"\bDCD\s+(\w+)", s)
    assert len(vectors(new)) == 59 and vectors(new) == vectors(old)[:59]
    assert vectors(new)[-1] == "USBWakeUp_IRQHandler"
    assert re.search(r"Stack_Size\s+EQU\s+(\S+)", old).group(1) == re.search(r"Stack_Size\s+EQU\s+(\S+)", new).group(1)
    print("PASS: all 59 F103C8 vectors retain positions; stack size unchanged")


def log_temperature_checks():
    assert not (ROOT / SOURCE / "ADC.c").exists() and not (ROOT / SOURCE / "ADC.h").exists()
    project = (ROOT / "103 + 309/Project/Users/BMS_SH3673520.uvprojx").read_text()
    for name in ("ADC.c", "stm32f10x_adc.c", "stm32f10x_dma.c"):
        assert f"<FileName>{name}</FileName>" not in project
    header = clean(read("DataDeal.h"))
    code = re.search(r"enum TempArray\s*\{.*?\};", header, re.S).group()
    code += re.search(r"enum tagInfoForKBArray\s*\{.*?\};", header, re.S).group()
    code += "\n" + macro(read("afe3520/Afe3520Config.h"), "AFE3520_MOS_TEMP_INDEX")
    code += """
static struct { UINT16 u16TempBat[4]; } g_afe3520Measurements;
static struct { UINT16 u16Temperature[TEMP_NUM]; } g_stCellInfoReport;
static UINT16 g_u16CalibCoefK[KB_NUM]; static INT16 g_i16CalibCoefB[KB_NUM];
static unsigned checks;
static void Monitor_TempBreak(UINT16 *value) { (void)value; ++checks; }
""" + function(read("DataDeal.c"), "DataLoad_Temperature") + """
int main(void) {
    unsigned i,v;
    for(i=0;i<KB_NUM;++i) g_u16CalibCoefK[i]=1024;
    g_afe3520Measurements.u16TempBat[0]=650;
    g_afe3520Measurements.u16TempBat[1]=660;
    g_afe3520Measurements.u16TempBat[2]=990;
    for(v=0;v<=1400;++v) {
        g_afe3520Measurements.u16TempBat[3]=v;
        checks=0; DataLoad_Temperature();
        assert(g_stCellInfoReport.u16Temperature[MOS_TEMP1]==v/10*10);
        assert(g_stCellInfoReport.u16Temperature[0]==650 && g_stCellInfoReport.u16Temperature[1]==660);
        assert(checks==3);
    }
    g_u16CalibCoefK[MDL_TEMP_MOS1]=2048; g_i16CalibCoefB[MDL_TEMP_MOS1]=1024;
    g_afe3520Measurements.u16TempBat[3]=650; DataLoad_Temperature();
    assert(g_stCellInfoReport.u16Temperature[MOS_TEMP1]==910);
    puts("PASS: MOS uses TS4 only, -40..100 C mapping, calibration and broken-sensor monitor retained");
    return 0;
}
"""
    print(run("ts4_temperature", code).decode().strip())
    header = clean(read("Sci_Upper.h"))
    defs = "\n".join(line for line in header.splitlines() if line.startswith("#define") and re.search(r"\b(?:RS485_|SCI_TX_BUF_LEN)", line)) + "\n"
    defs += re.search(r"enum RS485_CMD_E\s*\{.*?\};", header, re.S).group()
    defs += re.search(r"struct RS485MSG\s*\{.*?\};", header, re.S).group()
    defs += """
typedef int8_t INT8;
#define BMS3520_HW_REGISTER_BASE 0x2500U
#define BMS3520_HW_READ_WORDS 32U
#define RS485_ADDR_RW_BMS_PARAMETER 0x2400U
#define BMS_PARAMETER_COUNT 24U
#define E2P_PARA_NUM_OTHER_ELEMENT1 32U
#define SOC_TABLE_SIZE 21U
#define E2P_PARA_NUM_RTC 21U
#define E2P_PARA_NUM_PROTECT 65U
#define KB_NUM 47U
#define PRODUCT_ID_LENGTH_MAX 32U
#define Record_len 10U
static UINT8 FaultPoint_Third; static UINT16 Fault_record_Third[Record_len];
static struct { UINT8 BMS_SerialNumber[32], BMS_HardWareVersion[32], BMS_SoftWareVersion[32]; } ProductionInfor;
#define EVENT_RECORD_LENGTH FLASH_STORAGE_LOG_RECORD_COUNT
static struct { UINT16 point; UINT8 records[EVENT_RECORD_LENGTH][2]; } s_log_record;
static UINT8 g_u8SCITxBuff[EVENT_RECORD_LENGTH*2U];
"""
    protocol = read("Sci_Upper.c")
    for name in ("Sci_RangeFits", "Sci_GetReadWindowWordCount", "Sci_Deal_ReadRegs_0x03", "Sci_RecordBackIndex", "Sci_PutWordBE", "Sci_PutZeroWordsBE", "Sci_PutBytes"):
        defs += function(protocol, name)
    defs += function(read("LogRecord.c"), "Sci_ACK_0x03_ReadRegs_EventRecord")
    defs += function(protocol, "Sci_FillReadRegsLCD")
    defs += """
static void Sci_BuildReadWindow(UINT16 addr,UINT16 *offset,UINT8 *buffer) {
    assert(addr>=RS485_ADDR_EVENT_RECORD && addr<RS485_ADDR_EVENT_RECORD+EVENT_RECORD_LENGTH);
    Sci_FillReadRegsLCD(*offset,offset,buffer);
}
"""
    for name in ("StorageFlash_Crc16Update", "StorageFlash_Crc16"):
        defs += function(read("Flash.c"), name)
    defs += function(read("PubFunc.c"), "Sci_CRC16RTU")
    defs += function(protocol, "Sci_ACK_0x03")
    defs += """
int main(void) {
    unsigned offset,i; struct RS485MSG s;
    s_log_record.point=37;
    for(i=0;i<EVENT_RECORD_LENGTH;++i) { s_log_record.records[i][0]=i%20+1; s_log_record.records[i][1]=i%255; }
    for(offset=0;offset<EVENT_RECORD_LENGTH;offset+=100) {
        unsigned addr=RS485_ADDR_EVENT_RECORD+offset;
        memset(&s,0,sizeof(s)); s.u16Buffer[0]=1; s.u16Buffer[2]=addr>>8; s.u16Buffer[3]=addr;
        s.u16Buffer[5]=100; s.enRs485CmdType=RS485_CMD_READ_REGS;
        Sci_Deal_ReadRegs_0x03(&s); assert(s.AckType==RS485_ACK_POS);
        Sci_ACK_0x03(&s); assert(s.AckLenth==205 && s.u16Buffer[2]==200);
        for(i=0;i<100;++i) {
            unsigned n=(s_log_record.point+EVENT_RECORD_LENGTH-1-offset-i)%EVENT_RECORD_LENGTH;
            assert(s.u16Buffer[3+2*i]==s_log_record.records[n][0]);
            assert(s.u16Buffer[4+2*i]==s_log_record.records[n][1]);
        }
    }
    memset(&s,0,sizeof(s)); offset=RS485_ADDR_EVENT_RECORD+EVENT_RECORD_LENGTH;
    s.u16Buffer[2]=offset>>8; s.u16Buffer[3]=offset; s.u16Buffer[5]=1;
    Sci_Deal_ReadRegs_0x03(&s); assert(s.AckType==RS485_ACK_NEG);
    printf("PASS: %u logs, 100-word pages, newest-first ring wrap and end-boundary rejection\\n",EVENT_RECORD_LENGTH);
    return 0;
}
"""
    for count in (100, 500):
        print(run(f"logs_{count}", f"#define FLASH_STORAGE_LOG_RECORD_COUNT {count}U\n" + defs).decode().strip())


def main():
    if not CC:
        raise SystemExit("Run in a VS developer shell or put gcc/clang on PATH")
    OUT.mkdir(parents=True, exist_ok=True)
    scalar_checks()
    serial_checks()
    flash_checks()
    vector_checks()
    log_temperature_checks()


if __name__ == "__main__":
    main()
