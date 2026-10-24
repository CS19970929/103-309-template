"""Test real UART/Modbus C with host stubs; all outputs stay in user temp."""
from pathlib import Path
import os,subprocess,re,sys
sys.stdout.reconfigure(errors="replace")
import tempfile
ROOT=Path(__file__).resolve().parents[1]
src=ROOT/'103 + 309/Project/Source'
parent=Path(os.environ['LOCALAPPDATA'])/'CodexTemp/103-309-copy/uart-tests'
parent.mkdir(parents=True,exist_ok=True)
out=Path(tempfile.mkdtemp(prefix='run-',dir=parent))
sci=(src/'Sci_Upper.c').read_text(encoding='utf-8'); hdr=(src/'Sci_Upper.h').read_text(encoding='utf-8',errors='replace')
pub=(src/'PubFunc.c').read_text(encoding='utf-8')
def func(text,sig):
 start=text.index(sig);brace=text.index('{',start)
 while ';' in text[start:brace]: start=text.index(sig,start+len(sig));brace=text.index('{',start)
 depth=0
 for pos in range(brace,len(text)):
  depth+=(text[pos]=='{')-(text[pos]=='}')
  if depth==0:return text[start:pos+1]+'\n'
 raise ValueError(sig)
def decl(text,sig):
 start=text.index(sig);return text[start:text.index('};',start)+2]+'\n'
pre=r'''
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <assert.h>
typedef uint8_t UINT8; typedef uint16_t UINT16; typedef uint32_t UINT32;
typedef int8_t INT8; typedef int16_t INT16; typedef int32_t INT32;
'''
pre+='\n'.join(line for line in hdr.splitlines() if line.startswith('#define'))+'\n'
pre+=decl(hdr,'enum RS485_CMD_E {')+decl(hdr,'struct RS485MSG {')
pre+=decl((src/'DataDeal.h').read_text(encoding='utf-8'),'enum tagInfoForKBArray {')
pre+=r'''
#define E2P_PARA_NUM_PROTECT 65
#define E2P_PARA_NUM_OTHER_ELEMENT1 32
#define E2P_PARA_NUM_RTC 12
#define SOC_TABLE_SIZE 42
#define AFE_PARAMETES_TOTAL_LENGTH 24
#define RS485_ADDR_RW_AFE_PARAMETER 0x2400
#define FLASH_STORAGE_LOG_RECORD_COUNT 100
#define PRODUCT_ID_LENGTH_MAX 32
#define Record_len 10
static UINT8 g_u8SCITxBuff[SCI_TX_BUF_LEN];
static UINT16 Fault_record_Third[Record_len];
static UINT8 FaultPoint_Third;
static struct {UINT8 BMS_SerialNumber[32], BMS_HardWareVersion[32], BMS_SoftWareVersion[32];} ProductionInfor;
void Sci_Deal_ReadRegs_0x03(struct RS485MSG *s);
static int writes;
static void SleepDeal_RecordExternalComm(void) {}
static void Sci_Deal_WrReg_0x06(struct RS485MSG *s) { ++writes; }
static void Sci_Deal_WrRegs_0x10(struct RS485MSG *s) { ++writes; }
static void fill(UINT8 *p,UINT8 value) { memset(p,value,SCI_TX_BUF_LEN); }
static void Sci_ACK_0x03_ReadRegs_Data(struct RS485MSG *s,UINT8 *p) { fill(p,0xd0); }
static void Sci_ACK_0x03_RW_AFE_Parameters(struct RS485MSG *s,UINT8 *p) { fill(p,0x24); }
static void Sci_ACK_0x03_RW_Data_OtherCanAdd(struct RS485MSG *s,UINT8 *p) { fill(p,0x23); }
static void Sci_ACK_0x03_RW_Data_Other(struct RS485MSG *s,UINT8 *p) { fill(p,0x22); }
static void Sci_ACK_0x03_RW_Data_Pro(struct RS485MSG *s,UINT8 *p) { fill(p,0x21); }
static void Sci_ACK_0x03_RW_Data_Cali(struct RS485MSG *s,UINT8 *p) { fill(p,0x20); }
static void Sci_PutCurrentDiagWords(UINT8 *p,UINT16 *index) {fill(p,0xc1);}
static void Sci_ACK_0x03_ReadRegs_EventRecord(UINT8 *p) {fill(p,0xe0);}
'''
code=pre+sci[sci.index('enum SCI_READ_BLOCK {'):sci.index('void Sci_WrRegs_0x10_CalibCoef(')]+func(pub,'UINT16 Sci_CRC16RTU(')
for sig in ['static void Sci_SetWrError(', 'static UINT8 Sci_RangeFits(', 'static const struct SCI_READ_WINDOW *Sci_FindReadWindow(',
            'static void Sci_PutWordBE(', 'static void Sci_PutZeroWordsBE(', 'static UINT8 Sci_RecordBackIndex(',
            'static void Sci_PutBytes(', 'static void Sci_PutFactoryAgingStatusWords(',
            'void Sci_ACK_0x03_ReadRegs_LCD(', 'static void Sci_LoadReadData(', 'void Sci_ACK_0x03(', 'UINT8 Sci_HostReadWords(', 'void Sci_ACK_0x06_0x10(',
            'void CRC_verify(', 'void Sci_Deal_ReadRegs_0x03(', 'void Sci_ResetMessage(',
            'UINT8 Sci_ModbusFeed(', 'UINT8 Sci_ModbusFrameValid(',
            'void Sci_ModbusProcessFrame(']:code+=func(sci,sig)
code+=r'''
static int frame(struct RS485MSG *s, UINT8 slave,UINT8 function,UINT16 address,UINT16 value,int corrupt) {
 UINT8 p[8]; UINT16 crc; int i,complete=0;
 p[0]=slave;p[1]=function;p[2]=(UINT8)(address>>8);p[3]=(UINT8)address;p[4]=(UINT8)(value>>8);p[5]=(UINT8)value;
 crc=Sci_CRC16RTU(p,6);p[6]=(UINT8)crc;p[7]=(UINT8)(crc>>8);if(corrupt)p[7]^=1;
 Sci_ResetMessage(s);
 for(i=0;i<8;++i)complete+=Sci_ModbusFeed(s,p[i]);
 if(complete)Sci_ModbusProcessFrame(s);
 return complete;
}
static int check(const char *name,int ok) { printf("%s %s\n",ok?"PASS":"FAIL",name);return !ok; }
int main(void) {
 struct RS485MSG s; int failed=0,i,complete=0; UINT8 p[8]={1,3,0xd0,0,0,1};UINT16 crc;
 assert(frame(&s,1,3,0xd000,1,0)==1 && s.AckLenth==7);
 printf("normal_read reply=%u data=%02x%02x\n",s.AckLenth,s.u16Buffer[3],s.u16Buffer[4]);
 frame(&s,0,6,0x1102,1,0); printf("broadcast_write writes=%d reply=%u\n",writes,s.AckLenth);
 failed+=check("broadcast write executes without reply",writes==1 && s.AckLenth==0);
 frame(&s,1,3,0xd000,1,1); printf("bad_crc reply=%u function=%02x error=%02x\n",s.AckLenth,s.u16Buffer[1],s.u16Buffer[2]);
 failed+=check("CRC-invalid request is silently discarded",s.AckLenth==0);
 crc=Sci_CRC16RTU(p,6);p[6]=(UINT8)crc;p[7]=(UINT8)(crc>>8);Sci_ResetMessage(&s);
 for(i=0;i<3;++i)complete+=Sci_ModbusFeed(&s,p[i]);
 /* USART IDLE is disabled; timed gap checks are covered below. */
 for(i=3;i<8;++i)complete+=Sci_ModbusFeed(&s,p[i]);
 printf("valid_frame_with_idle completed=%d buffered=%u\n",complete,s.ptr_no);
 failed+=check("legal sub-1.5-character gap retains frame",complete==1);
 frame(&s,1,3,0x2100,1,0); frame(&s,1,3,0xc000,5,0);
 printf("lcd_read reply=%u data=%02x%02x\n",s.AckLenth,s.u16Buffer[3],s.u16Buffer[4]);
 failed+=check("LCD does not reuse prior protection payload",s.u16Buffer[3]!=0x21 || s.u16Buffer[4]!=0x21);
 frame(&s,1,3,0x2000,1,0); frame(&s,1,3,0xc080,5,0);
 printf("aging_read reply=%u data=%02x%02x\n",s.AckLenth,s.u16Buffer[3],s.u16Buffer[4]);
 failed+=check("aging does not reuse prior calibration payload",s.u16Buffer[3]!=0x20 || s.u16Buffer[4]!=0x20);
 for(i=0;i<65536;++i) {
  Sci_ResetMessage(&s);s.u16Buffer[2]=(UINT8)(i>>8);s.u16Buffer[3]=(UINT8)i;s.u16Buffer[5]=1;
  Sci_Deal_ReadRegs_0x03(&s);
  if(s.AckType==RS485_ACK_POS) { Sci_ACK_0x03(&s);assert(s.AckLenth==7); }
 }
 {
  UINT16 words[123]; int before=writes;
  assert(Sci_HostReadWords(0xc002,48,words)==0);
  assert(Sci_HostReadWords(0xc003,1,words)==RS485_ERROR_ADDR_INVALID);
  assert(Sci_HostReadWords(0xd200,18,words)==0);
  assert(Sci_HostReadWords(0xd200,19,words)==RS485_ERROR_ADDR_INVALID);
  assert(Sci_HostReadWords(0xd000,32768,words)==RS485_ERROR_DATA_INVALID);
  frame(&s,0,3,0xd000,1,0); assert(!s.AckLenth && writes==before);
  frame(&s,1,6,0x1102,7,1); assert(!s.AckLenth && writes==before);
 }
 puts("PASS all 65536 single-word address classifications and replies stay bounded in this harness");
 printf("failures=%d\n",failed);return failed?1:0;
}
'''
(out/'uart_contract_probe.c').write_text(code,encoding='utf-8')
vs=Path(r'C:/Program Files/Microsoft Visual Studio/2022/Community')
setup=out/'compiler-env.bat';setup.write_text('@echo off\ncall "'+str(vs/'Common7/Tools/VsDevCmd.bat')+'" -no_logo -arch=x64 -host_arch=x64 >nul\nset\n')
env=os.environ.copy()
for line in subprocess.check_output(['cmd.exe','/d','/c',str(setup)],text=True,errors='replace').splitlines():
 key,sep,value=line.partition('=')
 if sep and key:env[key]=value
compiler=sorted((vs/'VC/Tools/MSVC').glob('*/bin/Hostx64/x64/cl.exe'))[-1]
r=subprocess.run([str(compiler),'/nologo','/W3','/utf-8','/TC','/std:c11',str(out/'uart_contract_probe.c'),'/Fe:'+str(out/'uart_contract_probe.exe'),'/Fo:'+str(out/'uart_contract_probe.obj')],cwd=out,env=env,capture_output=True,text=True,errors='replace')
if r.returncode:print(r.stdout,r.stderr);raise SystemExit(r.returncode)
r=subprocess.run([str(out/'uart_contract_probe.exe')],cwd=out,capture_output=True,text=True,errors='replace');print(r.stdout)
if r.returncode: raise SystemExit(r.returncode)

# Compile the actual transport and customer implementation together.
core=code[:code.index('static int frame(')]
cut=core.index('#define')
fixtures=(ROOT/'tools/uart_host_cases.h').read_text(encoding='utf-8')
combo=core[:cut]+fixtures+'\n'+core[cut:]
combo=combo.replace('static void SleepDeal_RecordExternalComm(void) {}',
                    'static void SleepDeal_RecordExternalComm(void) { ++comm_activity; }')
for name in ('struct SOC_CAL_ELEMENT_UPPER {','struct MDLCHGFAULT_BITS',
             'union  MDLCHGFAULT_REG','struct stCell_Info {'):
    combo+=decl(hdr,name)
combo+=r'''
static struct stCell_Info g_stCellInfoReport;
static UINT8 SeriesNum=7;
static INT32 measured_ma;
static INT32 AfeCurrent_GetCurrent_mA(void) { return measured_ma; }
static struct { struct { UINT16 curValue; } u16TChgOTp,u16VcellOvp,u16VcellUvp,
    u16VcellOvp_Rcv,u16VcellUvp_Rcv,u16IchgOcp_First,u16IdsgOcp_First;
} AFE_Parameters_RS485_Struction;
static struct { struct { struct { UINT8 SC; } bits; } REG_BSTATUS1; } SH367309_Reg_Store;
'''
for name in ('Sci_Customer.c','Sci_Uart.c'):
    combo+='\n'.join(line for line in (src/name).read_text(encoding='utf-8').splitlines()
                      if not line.startswith('#include'))+'\n'
combo+=(ROOT/'tools/uart_protocol_cases.c').read_text(encoding='utf-8')
(out/'uart_combined.c').write_text(combo,encoding='utf-8')
r=subprocess.run([str(compiler),'/nologo','/W3','/utf-8','/TC','/std:c11',
                  str(out/'uart_combined.c'),'/Fe:'+str(out/'uart_combined.exe'),
                  '/Fo:'+str(out/'uart_combined.obj')],cwd=out,env=env,capture_output=True,text=True,errors='replace')
if r.returncode: print(r.stdout,r.stderr);raise SystemExit(r.returncode)
subprocess.run([str(out/'uart_combined.exe')],cwd=out,check=True)
print('Host evidence only; files: '+str(out))
