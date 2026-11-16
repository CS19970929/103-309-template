"""Exercise real CELL read, publication, quality checks and report statistics.

No board I/O, no BIN/HEX, no MCU flash writes. Compiles extracted production C.
"""
from pathlib import Path
import os
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / "103 + 309/Project/Source"


def function(source: str, signature: str) -> str:
    start = source.index(signature)
    brace = source.index("{", start)
    depth = 0
    for pos in range(brace, len(source)):
        depth += (source[pos] == "{") - (source[pos] == "}")
        if depth == 0:
            return source[start:pos + 1] + "\n"
    raise ValueError(signature)


def run() -> None:
    driver = (SOURCE / "I2C_AFE1.c").read_text(encoding="utf-8")
    i2c_header = (SOURCE / "I2C_AFE1.h").read_text(encoding="utf-8")
    mapping = (SOURCE / "DataDeal.c").read_text(encoding="utf-8")
    mapping_header = (SOURCE / "DataDeal.h").read_text(encoding="utf-8")
    main_header = (SOURCE / "main.h").read_text(encoding="utf-8")
    # This guard catches the real ARMCC5 failure: the table existed in
    # DataDeal.c but was invisible in the I2C_AFE1.c translation unit.
    declaration = "extern const unsigned char SeriesSelect_AFE1[16][16];"
    if declaration not in mapping_header or '#include "DataDeal.h"' not in main_header:
        raise RuntimeError("AFE channel mapping must be declared in DataDeal.h and reached by main.h")
    if 'UINT8 AfeCell_DecodeMv(UINT16 raw_word, UINT16 *voltage_mv);' not in i2c_header:
        raise RuntimeError("The report decoder must be declared for the DataDeal.c translation unit")
    tables = mapping[mapping.index("const unsigned char SeriesSelect_AFE1[16][16]"):mapping.index("};", mapping.index("const unsigned char SeriesSelect_AFE1[16][16]")) + 2]
    defines = "\n".join(line for line in driver.splitlines() if
                         line.startswith("#define AFE_CELL_"))
    ntc_start = driver.index('#define LENGTH_TBLTEMP_AFE_10K')
    ntc = driver[ntc_start:driver.index('};', ntc_start) + 2]
    structs = i2c_header[i2c_header.index('typedef struct _AFEDATA_'):
                         i2c_header.index('/* Software I2C')]
    production = (defines + "\n" + ntc + '\n' +
                  function((SOURCE / 'PubFunc.c').read_text(encoding='utf-8'), 'UINT16 GetEndValue(') +
                  function(driver, "UINT8 AfeCell_DecodeMv(") +
                  function(driver, "static UINT8 AfeCell_SuspiciousPair(") +
                  tables + '\nstatic UINT8 s_cell_pair_history_valid;\n' +
                  function(driver, 'void AfeCell_InvalidatePairHistory(') +
                  function(driver, 'UINT8 UpdateVoltageFromBqMaximo(') +
                  function(mapping, 'void DataLoad_CellVolt(') +
                  function(mapping, 'void DataLoad_CellVoltMaxMinFind('))
    prefix = """#include <stdint.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
typedef uint8_t UINT8;
typedef uint16_t UINT16;
typedef uint32_t UINT32;
typedef int16_t INT16;
typedef int32_t INT32;
#define U16_SwapEndian(x) ((UINT16)((((UINT16)(x)) << 8) | (((UINT16)(x)) >> 8)))
static UINT16 from_be(UINT16 v) { return U16_SwapEndian(v); }
"""
    prefix += structs + r'''
#define MTP_TEMP1 0x46U
#define VOLT_VBUS 0U
#define ERROR_STATUS_AFE1 1U
#define UPDNLMT16(v,hi,lo) do { if((v)>(hi)) (v)=(hi); } while(0)
static AFEDATA Registers_AFE1, next_frame;
static struct SH367309_Read SH367309_Read_AFE1;
static struct { UINT16 TR_ResRef; } SH367309_Reg_Store={1000U};
static struct {
    UINT16 u16VCell[32],u16VCellTotle,u16VCellMax,u16VCellMin,u16VCellDelta;
    UINT16 u16VCellMaxPosition,u16VCellMinPosition;
} g_stCellInfoReport;
static UINT8 SeriesNum=7U, read_failed, test_comm_fault;
static UINT16 g_u16CalibCoefK[1]={1024U};
static INT16 g_i16CalibCoefB[1];
static UINT8 SystemError_IsActive(UINT8 code) { assert(code==ERROR_STATUS_AFE1); return test_comm_fault; }
static UINT8 MTPRead(UINT8 addr, UINT8 len, UINT8 *p) {
    assert(addr==MTP_TEMP1 && len==sizeof(Registers_AFE1));
    if(read_failed) return 0U;
    memcpy(p,&next_frame,sizeof(next_frame)); return 1U;
}
'''
    cases = """
int main(void) {
    UINT16 mv=0;unsigned n,i;UINT16 mask;UINT8 channel;
    assert(AfeCell_DecodeMv(from_be(0x5000U),&mv) && mv==3200U);
    assert(AfeCell_DecodeMv(from_be(0x7D00U),&mv) && mv==5000U);
    assert(!AfeCell_DecodeMv(from_be(0xE000U),&mv) && mv==0U); /* no unsigned 8960mV */
    assert(!AfeCell_DecodeMv(from_be(0xF000U),&mv)); /* unsigned used to show 9600mV */
    assert(!AfeCell_DecodeMv(from_be(0xFA00U),&mv)); /* unsigned used to show 10000mV */
    assert(!AfeCell_DecodeMv(from_be(0x0000U),&mv));
    assert(!AfeCell_DecodeMv(from_be(0x7FFFU),&mv) && mv==5119U);
    assert(AfeCell_SuspiciousPair(3300,3300,2200,4400));
    assert(AfeCell_SuspiciousPair(3300,3300,4400,2200));
    assert(!AfeCell_SuspiciousPair(3300,3300,3200,3400));
    assert(!AfeCell_SuspiciousPair(3300,3300,2200,3900));
    for(n=5;n<=16;n++) {
        mask=0U;
        for(i=0;i<n;i++) {
            channel=SeriesSelect_AFE1[n-1U][i];
            assert(channel<16U && !(mask & (1U<<channel)));
            mask|=(UINT16)(1U<<channel);
        }
    }
    assert(SeriesSelect_AFE1[12][8]==8 && SeriesSelect_AFE1[12][9]==9);

    /* Boot while one lead is open: healthy cells still publish immediately. */
    next_frame.Temp1=next_frame.Temp2=from_be(16000U);
    next_frame.Cadc=(INT16)from_be(100U);
    for(i=0;i<16U;++i) next_frame.Cell[i]=from_be(21120U); /* 3300mV */
    next_frame.Cell[0]=from_be(0x7FFFU);
    assert(UpdateVoltageFromBqMaximo()==3U);
    DataLoad_CellVolt(); DataLoad_CellVoltMaxMinFind();
    assert(g_stCellInfoReport.u16VCell[0]==5119U);
    for(i=1;i<7U;++i) assert(g_stCellInfoReport.u16VCell[i]==3300U);
    for(i=7;i<32U;++i) assert(g_stCellInfoReport.u16VCell[i]==61001U);
    assert(g_stCellInfoReport.u16VCellMax==5119U && g_stCellInfoReport.u16VCellMin==3300U);
    assert(g_stCellInfoReport.u16VCellTotle>0U && SH367309_Read_AFE1.u16Current==100U);
    mv=SH367309_Read_AFE1.u16TempBat[0];
    next_frame.Cell[4]=from_be(22016U); /* 3440mV while the first cell remains bad */
    next_frame.Temp1=from_be(8000U); next_frame.Cadc=(INT16)from_be(200U);
    assert(UpdateVoltageFromBqMaximo()==3U); DataLoad_CellVolt();
    assert(g_stCellInfoReport.u16VCell[4]==3440U);
    assert(SH367309_Read_AFE1.u16Current==200U && SH367309_Read_AFE1.u16TempBat[0]!=mv);
    /* Negative/zero affects only that channel, never its healthy neighbors. */
    next_frame.Cell[0]=from_be(0xE000U);
    assert(UpdateVoltageFromBqMaximo()==3U); DataLoad_CellVolt();
    assert(g_stCellInfoReport.u16VCell[0]==0U && g_stCellInfoReport.u16VCell[4]==3440U);
    next_frame.Cell[0]=0U;
    assert(UpdateVoltageFromBqMaximo()==3U); DataLoad_CellVolt();
    assert(g_stCellInfoReport.u16VCell[1]==3300U);
    /* Displaying a bad pair must not replace the validation baseline. */
    for(i=0;i<16U;++i) next_frame.Cell[i]=from_be(21120U);
    assert(UpdateVoltageFromBqMaximo()==0U);
    next_frame.Cell[0]=from_be(14080U); next_frame.Cell[1]=from_be(28160U);
    for(i=0;i<3U;++i) {
        assert(UpdateVoltageFromBqMaximo()==3U); DataLoad_CellVolt();
        assert(g_stCellInfoReport.u16VCell[0]==2200U && g_stCellInfoReport.u16VCell[1]==4400U);
        assert(SH367309_Read_AFE1.u16VCell[0]==3300U && SH367309_Read_AFE1.u16VCell[1]==3300U);
    }
    next_frame.Cell[0]=next_frame.Cell[1]=from_be(21120U);
    assert(UpdateVoltageFromBqMaximo()==0U);
    mv=SH367309_Read_AFE1.u16TempBat[1];
    next_frame.Temp1=0U; next_frame.Temp2=from_be(8000U);
    next_frame.Cadc=(INT16)from_be(300U);
    assert(UpdateVoltageFromBqMaximo()==2U); DataLoad_CellVolt();
    assert(g_stCellInfoReport.u16VCell[1]==3300U && SH367309_Read_AFE1.u16Current==300U);
    assert(SH367309_Read_AFE1.u16TempBat[1]!=mv);
    /* Only a confirmed communication error zeros the configured cell slots. */
    g_i16CalibCoefB[0]=1; /* Offsets must not fabricate pack voltage on comm loss. */
    test_comm_fault=1U; DataLoad_CellVolt(); DataLoad_CellVoltMaxMinFind();
    for(i=0;i<7U;++i) assert(g_stCellInfoReport.u16VCell[i]==0U);
    for(i=7;i<32U;++i) assert(g_stCellInfoReport.u16VCell[i]==61001U);
    assert(!g_stCellInfoReport.u16VCellTotle && !g_stCellInfoReport.u16VCellMax && !g_stCellInfoReport.u16VCellMin);
    g_i16CalibCoefB[0]=0;
    test_comm_fault=0U; DataLoad_CellVolt(); assert(g_stCellInfoReport.u16VCell[1]==3300U);
    read_failed=1U; assert(UpdateVoltageFromBqMaximo()==1U);
    assert(g_stCellInfoReport.u16VCell[1]==3300U); /* no failed-frame publication */
    AfeCell_InvalidatePairHistory(); assert(!s_cell_pair_history_valid);
    puts("PASS CELL: signed decode, per-cell live publication, open-wire boot, independent NTC/current,");
    puts("     comm-only report zeroing, absent slots, preserved pair baseline and 5..16 mapping");
    return 0;
}
"""
    output = Path(tempfile.mkdtemp(prefix="bms-cell-voltage-", dir=tempfile.gettempdir()))
    target = output / "cell_voltage.c"
    target.write_text(prefix + production + cases, encoding="utf-8")
    vs = Path(r"C:\Program Files\Microsoft Visual Studio\2022\Community")
    env = os.environ.copy()
    if os.name == "nt" and (vs / "Common7/Tools/VsDevCmd.bat").exists():
        setup = output / "compiler-env.bat"
        setup.write_text('@echo off\ncall "' + str(vs / "Common7/Tools/VsDevCmd.bat") +
                         '" -no_logo -arch=x64 -host_arch=x64 >nul\nset\n', encoding="utf-8")
        for line in subprocess.check_output(["cmd.exe", "/d", "/c", str(setup)],
                                            text=True, errors="replace").splitlines():
            key, sep, value = line.partition("=")
            if sep and key:
                env[key] = value
        cc = sorted((vs / "VC/Tools/MSVC").glob("*/bin/Hostx64/x64/cl.exe"))[-1]
        exe = output / "cell_voltage.exe"
        command = [str(cc), "/nologo", "/W4", "/WX", "/utf-8", "/TC", "/std:c11",
                   str(target), "/Fe:" + str(exe), "/Fo:" + str(output / "cell_voltage.obj")]
    else:
        cc = os.environ.get("CC") or shutil.which("clang") or shutil.which("gcc")
        if not cc:
            raise RuntimeError("C compiler not found")
        exe = output / "cell_voltage"
        command = [str(cc), "-std=c99", "-Wall", "-Wextra", "-Werror",
                   str(target), "-o", str(exe)]
    subprocess.run(command, cwd=output, env=env, check=True)
    subprocess.run([str(exe)], cwd=output, check=True)
    print("Host-only evidence:", output)


if __name__ == "__main__":
    run()
