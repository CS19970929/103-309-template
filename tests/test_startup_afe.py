"""Execute production boot/calibration/AFE functions with mocked hardware.

Run: python -B tests/test_startup_afe.py
Requires Windows Visual Studio C++ tools. All generated files go to user TEMP.
"""

import os
from pathlib import Path
import re
import shutil
import subprocess
import sys
import tempfile

sys.dont_write_bytecode = True
from test_current_calibration import function_source, msvc_environment

ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / "103 + 309/Project/Source"


def read(name):
    return (SOURCE / name).read_bytes().decode("latin1").replace("\r\n", "\n")


def without_comments(text):
    return re.sub(r"/\*.*?\*/|//[^\n]*", "", text, flags=re.DOTALL)


def main():
    folder = Path(os.environ["LOCALAPPDATA"]) / "CodexTemp/103-309-template/startup-afe-tests"
    folder.mkdir(parents=True, exist_ok=True)
    folder = Path(tempfile.mkdtemp(prefix="run-", dir=folder))
    for name in ["DataDeal.h", "SH367309_DataDeal.h", "SH367309_Func.h"]:
        text = without_comments(read(name)).replace('#include "conf.h"', "")
        if name == "DataDeal.h":
            # Exercise both AFE chemistry templates without changing the firmware selection.
            text = re.sub(r"^#define LIFEPO[^\n]*", "", text, flags=re.MULTILINE)
        (folder / name).write_text(text, encoding="ascii")

    current = read("DataDeal.c")
    constants = "\n".join(re.findall(r"^#define (?:BOOT_CURRENT|CURRENT_|SH309_CADC)\w+[^\n]*", current, re.MULTILINE))
    state = "\n".join(re.findall(r"^static (?:UINT8|INT32) g_(?:u8BootCurrentZeroStatus|i32BootCurrentZeroRawX4) = [^;]+;", current, re.MULTILINE))
    functions = {
        "DataDeal.c": ["BmsCurrent_RawToSigned", "BmsCurrent_AbsI32", "BmsCurrent_BootZeroFail",
                       "SH309_CurrentWaitFreshSample", "SH309_CurrentPrepareBootZero",
                       "SH309_CurrentCheckBootZeroSafe", "SH309_CurrentReadRaw",
                       "BmsCurrent_BootZeroCalibrate", "BmsCurrent_IsBootZeroValid",
                       "BmsCurrent_GetBootZeroStatus"],
        "SH367309_Func.c": ["SH367309_Enable_AFE_Wdt_Cadc_Drivers", "SH367309_DriverMos_Ctrl"],
        "IO_Control.c": ["Drivers_External_Ctrl"],
        "SH367309_DataDeal.c": ["Choose_Right_Value", "Refresh_Parameters", "Write_Parameters",
                                "SH367309_UpdataAfeConfig"],
        "I2C_AFE1.c": ["InitAFE1"],
        "conf/conf.c": ["InitIO"],
        "main.c": ["InitDevice"],
        "Cell_balance.c": ["CellBalance_UpdateReport"],
    }
    bodies = []
    for name, names in functions.items():
        text = read(name)
        if name == "SH367309_DataDeal.c":
            # The file also contains an inactive historical definition under #if 0.
            start = text.index("#if 0\nbool SH367309_UpdataAfeConfig")
            end = text.index("#endif", start) + len("#endif")
            text = text[:start] + text[end:]
        bodies.extend(function_source(text, name) for name in names)
    parameter_source = read("SH367309_DataDeal.c")
    globals_source = "\n".join(re.findall(r"^(?:const UINT16 AFE_\w+\[16\]|AFE_ROM_PARAMETERS_TypeDef AFE_ROM_PARAMETERS_Struction|AFE_Parameters_RS485_Typedef AFE_Parameters_RS485_Struction) = [^;]+;", parameter_source, re.MULTILINE))
    buffer_source = re.search(r"UINT8 ucMTPBuffer\[26\] = \{.*?\};", read("SH367309_Func.c"), re.DOTALL)[0]
    table_source = re.search(r"const unsigned char SeriesSelect_AFE1\[16\]\[16\] = \{.*?\};", read("main.c"), re.DOTALL)[0]
    ntc_source = re.search(r"UINT16 iSheldTemp_10K_NTC\[141\] = \{.*?\};", read("SH367309_Func.c"), re.DOTALL)[0]
    i2c_defines = "\n".join(re.findall(
        r"^#define MTP_(?:CONF|BSTATUS3|ADC2|BALANCEH)\s+[^\n]*",
        read("I2C_AFE1.h"), re.MULTILINE))
    gpio_defines = re.sub(r"^#include[^\n]*", "", without_comments(read("conf/conf_gpio.h")), flags=re.MULTILINE)
    pin_defines = "\n".join(f"#define GPIO_Pin_{pin} (1u << {pin})" for pin in range(16))

    harness = r'''
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
typedef uint8_t UINT8;
typedef uint16_t UINT16;
typedef uint32_t UINT32;
typedef int16_t INT16;
typedef int32_t INT32;
typedef enum { GPIO_PreCHG, GPIO_CHG, GPIO_DSG } GPIO_Type;
struct RS485MSG;
#include "DataDeal.h"
#include "SH367309_DataDeal.h"
#include "SH367309_Func.h"
I2C_DEFINES
#define ERROR_STATUS_AFE1 1
#define FORCE_CLOSE_MODE 2
#define log_i(...) ((void)0)
#define log_w(...) ((void)0)
#define log_e(...) ((void)0)
#define Feed_IWatchDog ((void)0)
static UINT8 gpio_outputs[5][16];
#define PAout(pin) gpio_outputs[0][pin]
#define PBout(pin) gpio_outputs[1][pin]
#define PCout(pin) gpio_outputs[2][pin]
#define PDout(pin) gpio_outputs[3][pin]
#define ctlc PBout(14)
typedef struct { int GPIO_Pin, GPIO_Mode, GPIO_Speed; } GPIO_InitTypeDef;
PIN_DEFINES
#define GPIO_Mode_Out_PP 0
#define GPIO_Mode_IN_FLOATING 1
#define GPIO_Speed_2MHz 0
#define GPIOA 0
#define GPIOB 1
#define GPIOC 2
#define GPIOD 3
#define GPIOE 4
#define RCC_APB2Periph_AFIO 0
#define RCC_APB2Periph_GPIOA 1
#define RCC_APB2Periph_GPIOB 2
#define RCC_APB2Periph_GPIOC 3
#define RCC_APB2Periph_GPIOD 4
#define RCC_APB2Periph_GPIOE 5
#define ENABLE 1
GPIO_DEFINES
static void RCC_APB2PeriphClockCmd(int clock, int on) { (void)clock; (void)on; }
static void GPIO_Init(int port, GPIO_InitTypeDef *config) {
    if (port == GPIOB && (config->GPIO_Pin & PIN_AFE1_CTL)
        && BmsCurrent_GetBootZeroStatus() == BOOT_CURRENT_ZERO_NOT_RUN) assert(ctlc == 0);
}
static void GPIO_ResetBits(int port, int pins) {
    if (port == GPIOB && (pins & PIN_AFE1_CTL)) ctlc = 0;
}
static void GPIO_SetBits(int port, int pins) { (void)port; (void)pins; }
static int GPIO_ReadInputDataBit(int port, int pin) { (void)port; (void)pin; return 0; }
struct { struct { UINT8 b1Status_MOS_CHG, b1Status_MOS_DSG, b1Status_Heat; } bits; } SystemStatus;
struct { struct { UINT8 b1OnOFF_MOS_Relay; } bits; } System_OnOFF_Func;
struct { UINT8 u8_DriverCtrl_Right;
    struct { struct { UINT8 b1Status_MOS_CHG, b1Status_MOS_DSG; } bits; } MosRelay_Status;
    struct { struct { UINT8 b2_DriverOFF_Flag; } bits; } DriverForceExt;
} Driver_Element;
struct { unsigned cnt_enter_chg_open, cnt_enter_dsg_open; } sys_time;
struct { UINT16 u16BalanceFlag1, u16BalanceFlag2, u16Ichg, u16IDischg; } g_stCellInfoReport;
struct OTHER_ELEMENT OtherElement;
SH367309_REG_STORE SH367309_Reg_Store;
UINT32 g_u32CS_Res_AFE;
NTC_TABLE
UINT8 SeriesNum = 16;
int AFE_PARAM_WRITE_Flag = 1, AFE_ResetFlag;
GLOBALS
CONSTANTS
STATE
static UINT8 registers[256];
static int failure, samples, elapsed_ms, writes, write_failure, readback_mismatch;
static int tr_failure, read_failure;
void Drivers_External_Ctrl(void);
static void assert_hold(void) {
    if (g_u8BootCurrentZeroStatus == BOOT_CURRENT_ZERO_NOT_RUN) assert(ctlc == 0);
}
UINT8 MTPRead(UINT8 addr, UINT8 len, UINT8 *data) {
    assert_hold();
    if (read_failure || (tr_failure && addr == 0x19)) return 0;
    if ((failure == 2 && addr == MTP_CONF) || (failure == 3 && addr == MTP_BSTATUS3)
        || (failure == 5 && addr == MTP_ADC2)) return 0;
    memcpy(data, registers + addr, len);
    if (addr == MTP_BSTATUS3 && failure == 4) *data = 1;
    if (addr == MTP_CONF && failure == 8) *data |= 0x10;
    if (addr == 0 && readback_mismatch && writes) data[8] ^= 1;
    if (addr == MTP_ADC2) {
        int raw = failure == 6 ? 41 : failure == 7 && samples ? 10 : 2;
        assert(elapsed_ms >= (samples + 1) * 300);
        data[0] = (UINT8)(raw >> 8); data[1] = (UINT8)raw; samples++;
    }
    return 1;
}
UINT8 MTPWrite(UINT8 addr, UINT8 len, UINT8 *data) {
    assert_hold();
    if (failure == 1 && addr == MTP_CONF) return 0;
    if (g_u8BootCurrentZeroStatus == BOOT_CURRENT_ZERO_NOT_RUN && addr == MTP_CONF)
        assert((*data & 0x70) == 0);
    memcpy(registers + addr, data, len); return 1;
}
UINT8 MTPWriteROM(UINT8 addr, UINT8 len, UINT8 *data) {
    writes++;
    if (write_failure) return 0;
    memcpy(registers + addr, data, len); return 1;
}
static void __delay_ms(UINT16 ms) {
    elapsed_ms += ms;
    Drivers_External_Ctrl();  /* Even an enable request during sampling stays inhibited. */
    assert_hold();
}
static void Delay1ms(UINT16 ms) { (void)ms; assert_hold(); }
void AFE_Reset(void) { assert(ctlc == 0); registers[MTP_CONF] = 0x70; }
UINT8 AFE_IsReady(void) { assert_hold(); return 0; }
static bool System_ERROR_UserCallback(int error) { (void)error; return false; }
static void InitShortCur(void) {}
#define EMPTY_INIT(name) static void name(void) {}
EMPTY_INIT(SystemInit) EMPTY_INIT(InitDelay) EMPTY_INIT(IsSleepStartUp)
EMPTY_INIT(jtag_disableAndConfIO) EMPTY_INIT(InitNVIC) EMPTY_INIT(InitSystemWakeUp)
EMPTY_INIT(InitE2PROM) EMPTY_INIT(InitADC) EMPTY_INIT(InitSci)
EMPTY_INIT(Init_ChargerLoad_Det) EMPTY_INIT(InitMosRelay_DOx)
EMPTY_INIT(InitData_SOC) EMPTY_INIT(InitTimer)
BODIES
static void reset_case(void) {
    memset(registers, 0, sizeof(registers));
    memset(&SH367309_Reg_Store, 0, sizeof(SH367309_Reg_Store));
    memset(&Driver_Element, 0, sizeof(Driver_Element));
    memset(&SystemStatus, 0, sizeof(SystemStatus));
    memset(&g_stCellInfoReport, 0, sizeof(g_stCellInfoReport));
    OtherElement.u16Sys_CS_Res = 2; OtherElement.u16Sys_CS_Res_Num = 2;
    OtherElement.u16Sys_SeriesNum = 16;
    System_OnOFF_Func.bits.b1OnOFF_MOS_Relay = 1;
    g_u8BootCurrentZeroStatus = BOOT_CURRENT_ZERO_NOT_RUN;
    g_i32BootCurrentZeroRawX4 = 0;
    ctlc = 0; failure = samples = elapsed_ms = writes = 0;
    write_failure = readback_mismatch = tr_failure = read_failure = 0;
    AFE_PARAM_WRITE_Flag = 1;
}
int main(void) {
    unsigned mask, n;
    const UINT8 statuses[] = { BOOT_CURRENT_ZERO_VALID, BOOT_CURRENT_ZERO_CONFIG_WRITE_ERROR,
        BOOT_CURRENT_ZERO_CONFIG_READBACK_ERROR, BOOT_CURRENT_ZERO_SAMPLE_READ_ERROR,
        BOOT_CURRENT_ZERO_FET_ACTIVE, BOOT_CURRENT_ZERO_SAMPLE_READ_ERROR,
        BOOT_CURRENT_ZERO_OUT_OF_RANGE, BOOT_CURRENT_ZERO_UNSTABLE, BOOT_CURRENT_ZERO_FET_ACTIVE };
    assert(sizeof(AFE_ROM_PARAMETERS_Struction) == 26);
    assert(((BYTE_00H_SCONF1) & 0x10) == 0);
    assert(((BYTE_01H_SCONF2) & 0x0c) == 0x0c);
    assert(BYTE_08H_BALV == ((VAL_BAL_OPEN / 20) & 0xff));
    for (n = 0; n < sizeof(statuses); n++) {
        reset_case(); failure = (int)n;
        Drivers_External_Ctrl(); assert(ctlc == 0);
        SH367309_DriverMos_Ctrl(GPIO_CHG, 1);
        SH367309_DriverMos_Ctrl(GPIO_DSG, 1);
        SH367309_DriverMos_Ctrl(GPIO_PreCHG, 1);
        assert((registers[MTP_CONF] & 0x70) == 0);
        InitDevice(); assert(ctlc == 0);
        assert(BmsCurrent_GetBootZeroStatus() == statuses[n]);
        assert(g_i32BootCurrentZeroRawX4 == (n == 0 ? 8 : 0));
        Drivers_External_Ctrl(); assert(ctlc == 1);
        System_OnOFF_Func.bits.b1OnOFF_MOS_Relay = 0;
        Drivers_External_Ctrl(); assert(ctlc == 0);
        System_OnOFF_Func.bits.b1OnOFF_MOS_Relay = 1;
        Driver_Element.DriverForceExt.bits.b2_DriverOFF_Flag = FORCE_CLOSE_MODE;
        Drivers_External_Ctrl(); assert(ctlc == 0);
    }
    reset_case(); InitDevice(); Drivers_External_Ctrl(); assert(ctlc == 1);
    InitIO(); assert(ctlc == 1); /* STOP wake retains the completed boot's output. */
    InitAFE1(); assert(ctlc == 0); Drivers_External_Ctrl(); assert(ctlc == 1);
    assert(BmsCurrent_IsBootZeroValid());
    reset_case(); ctlc = 1; InitIO(); assert(ctlc == 0);
    reset_case(); assert(Refresh_Parameters());
    assert(AFE_ROM_PARAMETERS_Struction.m00H_01H.BAL == 0);
    assert(AFE_ROM_PARAMETERS_Struction.m00H_01H.CTLC == 3);
    assert(AFE_ROM_PARAMETERS_Struction.m08H_09H.BALV == ((VAL_BAL_OPEN / 20) & 0xff));
    reset_case(); tr_failure = 1;
    assert(!SH367309_UpdataAfeConfig()); assert(writes == 0);
    assert(AFE_PARAM_WRITE_Flag == 1);
    reset_case(); assert(SH367309_UpdataAfeConfig()); assert(writes > 0);
    writes = 0; AFE_PARAM_WRITE_Flag = 1;
    assert(SH367309_UpdataAfeConfig()); assert(writes == 0);
    assert(AFE_PARAM_WRITE_Flag == 0);
    assert(SH367309_UpdataAfeConfig()); assert(AFE_PARAM_WRITE_Flag == 0);
    reset_case(); write_failure = 1;
    assert(!SH367309_UpdataAfeConfig()); assert(AFE_PARAM_WRITE_Flag == 1);
    reset_case(); readback_mismatch = 1;
    assert(!SH367309_UpdataAfeConfig());
    reset_case(); read_failure = 1;
    assert(!SH367309_UpdataAfeConfig()); assert(AFE_PARAM_WRITE_Flag == 1);
    reset_case();
    for (n = 5; n <= 16; n++) {
        SeriesNum = (UINT8)n;
        for (mask = 0; mask <= 65535; mask++) {
            unsigned cell, expected = 0;
            for (cell = 0; cell < n; cell++)
                if (mask & (1u << SeriesSelect_AFE1[n-1][cell])) expected |= 1u << cell;
            CellBalance_UpdateReport((UINT16)mask);
            assert(g_stCellInfoReport.u16BalanceFlag1 == expected);
            assert(g_stCellInfoReport.u16BalanceFlag2 == 0);
        }
    }
    SeriesNum = 255; CellBalance_UpdateReport(65535);
    assert(g_stCellInfoReport.u16BalanceFlag1 == 0);
    /* RTC policy balance/read-failure guards are exercised in test_sleep.py. */
    puts("PASS: boot success, eight calibration failures, CTLC interlocks/recovery, EEPROM failures/readback/idempotence and balance masks");
    return 0;
}
'''
    for token, value in {"GLOBALS": globals_source + "\n" + buffer_source + "\n" + table_source,
                         "CONSTANTS": constants, "STATE": state,
                         "NTC_TABLE": ntc_source, "I2C_DEFINES": i2c_defines,
                         "GPIO_DEFINES": gpio_defines, "PIN_DEFINES": pin_defines,
                         "BODIES": "\n\n".join(bodies)}.items():
        harness = harness.replace(token, without_comments(value))
    path = folder / "startup_harness.c"
    path.write_text(harness, encoding="ascii")
    env = msvc_environment()
    compiler = shutil.which("cl.exe", path=env["PATH"])
    for chemistry in ["LIFEPO", "TERNARYLI"]:
        exe = folder / f"startup_{chemistry}.exe"
        result = subprocess.run(
            [compiler, "/nologo", "/W3", "/WX", "/O2", f"/D{chemistry}", str(path),
             f"/Fe:{exe}", f"/Fo:{folder / 'startup.obj'}"], cwd=folder, env=env,
            capture_output=True, text=True, errors="replace")
        if result.returncode:
            raise RuntimeError(result.stdout + result.stderr)
        subprocess.run([str(exe)], check=True, cwd=folder)
    print(f"Temporary outputs: {folder}")


if __name__ == "__main__":
    main()
