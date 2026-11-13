#!/usr/bin/env python3
"""Check live BMS configuration; use --comm-tool for the independent tool contract."""
import argparse
import re
import sys
import xml.etree.ElementTree as ET
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]

PROJECT_CONFIG = ROOT / "103 + 309" / "Project" / "Source" / "conf" / "Project_Config.h"

PROJECT_CONFIG_ENCODING = "gbk"

BUILD_GUARD = ROOT / "103 + 309" / "Project" / "Source" / "conf" / "Project_BuildGuard.h"

ELOG_CFG_H = ROOT / "103 + 309" / "Project" / "Source" / "easylogger" / "inc" / "elog_cfg.h"

SCI_UPPER_C = ROOT / "103 + 309" / "Project" / "Source" / "Sci_Upper.c"

CAN_HDX_C = ROOT / "103 + 309" / "Project" / "Source" / "Can_HDX.c"

FLASH_C = ROOT / "103 + 309" / "Project" / "Source" / "Flash.c"

FLASH_H = ROOT / "103 + 309" / "Project" / "Source" / "Flash.h"

COMM_TOOL_ARCH_DOC = ROOT / "docs" / "COMM_TOOL_CAN_IAP_ARCHITECTURE_2026-05-22.md"

COMM_TOOL_SERIAL_DOC = ROOT / "docs" / "COMM_TOOL_SERIAL_PROTOCOL.md"

BMS_CAN_SERVICE_DOC = ROOT / "docs" / "BMS_CAN_SERVICE_PROTOCOL.md"

BMS_CAN_IAP_DOC = ROOT / "docs" / "BMS_CAN_IAP_PROTOCOL.md"

BMS_CAN_IAP_RELIABILITY_DOC = ROOT / "docs" / "BMS_CAN_IAP_RELIABILITY_STATUS_2026-05-22.md"

BMS_SERIAL_IAP_REFACTOR_DOC = ROOT / "docs" / "BMS_SERIAL_IAP_REFACTOR_2026-05-22.md"

COMM_TOOL_KEIL_DOC = ROOT / "docs" / "COMM_TOOL_F103RET6_KEIL_PORT_2026-05-23.md"

COMM_TOOL_UART_SELECT_DOC = ROOT / "docs" / "COMM_TOOL_UART_SELECT_2026-05-25.md"

COMM_TOOL_BMS_REVIEW_HTML = ROOT / "docs" / "COMM_TOOL_BMS_APP_REVIEW_LOGGING_2026-05-25.html"

COMM_TOOL_HOST = ROOT / "tools" / "comm_tool_host.py"

COMM_TOOL_HOST_START = ROOT / "tools" / "start_comm_tool_host.ps1"

COMM_TOOL_UART_SELECT_SCRIPT = ROOT / "tools" / "set_comm_tool_uart.ps1"

COMM_TOOL_SOURCE = ROOT / "firmware" / "comm_tool_f103ret6" / "source" / "app"

COMM_TOOL_BSP_SOURCE = ROOT / "firmware" / "comm_tool_f103ret6" / "source" / "bsp"

COMM_TOOL_KEIL_PROJECT = ROOT / "firmware" / "comm_tool_f103ret6" / "keil" / "COMM_TOOL_F103RET6.uvprojx"

COMM_TOOL_IAP_PROJECT = ROOT / "firmware" / "comm_tool_f103ret6" / "keil" / "COMM_TOOL_IAP.uvprojx"

class Reporter(object):
    def __init__(self, verbose):
        self.verbose = verbose
        self.errors = []
        self.warnings = []
        self.ok_count = 0

    def ok(self, message):
        self.ok_count += 1
        if self.verbose:
            print("[OK]   " + message)

    def warn(self, message):
        self.warnings.append(message)
        print("[WARN] " + message)

    def fail(self, message):
        self.errors.append(message)
        print("[FAIL] " + message)

    def summary(self):
        print("")
        print("Project check summary:")
        print("  OK:       {0}".format(self.ok_count))
        print("  Warnings: {0}".format(len(self.warnings)))
        print("  Errors:   {0}".format(len(self.errors)))
        return 1 if self.errors else 0

def read_text(path):
    if path == PROJECT_CONFIG:
        return path.read_text(encoding=PROJECT_CONFIG_ENCODING, errors="replace")
    return path.read_text(encoding="utf-8", errors="replace")

def check_comm_tool_can_iap_contract(reporter):
    docs = [
        COMM_TOOL_ARCH_DOC,
        COMM_TOOL_SERIAL_DOC,
        BMS_CAN_SERVICE_DOC,
        BMS_CAN_IAP_DOC,
        BMS_CAN_IAP_RELIABILITY_DOC,
        COMM_TOOL_KEIL_DOC,
        COMM_TOOL_UART_SELECT_DOC,
        COMM_TOOL_BMS_REVIEW_HTML,
    ]
    source_files = [
        COMM_TOOL_HOST,
        COMM_TOOL_HOST_START,
        COMM_TOOL_UART_SELECT_SCRIPT,
        COMM_TOOL_SOURCE / "ct_config.h",
        COMM_TOOL_SOURCE / "ct_protocol.c",
        COMM_TOOL_SOURCE / "ct_debug_log.c",
        COMM_TOOL_SOURCE / "ct_debug_log.h",
        COMM_TOOL_SOURCE / "ct_flash_store.c",
        COMM_TOOL_SOURCE / "ct_can_gateway.c",
        COMM_TOOL_SOURCE / "ct_upgrade_manager.c",
        COMM_TOOL_SOURCE / "ct_boot_control.c",
        COMM_TOOL_SOURCE / "ct_self_iap.c",
        COMM_TOOL_SOURCE / "ct_app.c",
        ROOT / "firmware" / "comm_tool_f103ret6" / "source" / "iap" / "ct_iap.c",
        ROOT / "firmware" / "comm_tool_f103ret6" / "source" / "iap" / "ct_iap_main.c",
        COMM_TOOL_BSP_SOURCE / "board.c",
        COMM_TOOL_BSP_SOURCE / "board_uart.c",
        COMM_TOOL_BSP_SOURCE / "board_can.c",
        COMM_TOOL_KEIL_PROJECT,
        COMM_TOOL_IAP_PROJECT,
        ELOG_CFG_H,
        PROJECT_CONFIG,
        BUILD_GUARD,
    ]
    required = docs + source_files
    if any(not path.exists() for path in required):
        missing = [str(path.relative_to(ROOT)) for path in required if not path.exists()]
        reporter.fail("comm tool CAN-IAP files missing: {0}".format(",".join(missing)))
        return

    arch_doc = read_text(COMM_TOOL_ARCH_DOC)
    serial_doc = read_text(COMM_TOOL_SERIAL_DOC)
    service_doc = read_text(BMS_CAN_SERVICE_DOC)
    iap_doc = read_text(BMS_CAN_IAP_DOC)
    iap_reliability_doc = read_text(BMS_CAN_IAP_RELIABILITY_DOC)
    keil_doc = read_text(COMM_TOOL_KEIL_DOC)
    uart_select_doc = read_text(COMM_TOOL_UART_SELECT_DOC)
    review_html = read_text(COMM_TOOL_BMS_REVIEW_HTML)
    host_py = read_text(COMM_TOOL_HOST)
    start_ps1 = read_text(COMM_TOOL_HOST_START)
    uart_select_ps1 = read_text(COMM_TOOL_UART_SELECT_SCRIPT)
    config_h = read_text(COMM_TOOL_SOURCE / "ct_config.h")
    protocol_c = read_text(COMM_TOOL_SOURCE / "ct_protocol.c")
    debug_log_c = read_text(COMM_TOOL_SOURCE / "ct_debug_log.c")
    debug_log_h = read_text(COMM_TOOL_SOURCE / "ct_debug_log.h")
    flash_c = read_text(COMM_TOOL_SOURCE / "ct_flash_store.c")
    can_c = read_text(COMM_TOOL_SOURCE / "ct_can_gateway.c")
    upgrade_c = read_text(COMM_TOOL_SOURCE / "ct_upgrade_manager.c")
    app_c = read_text(COMM_TOOL_SOURCE / "ct_app.c")
    iap_c = read_text(ROOT / "firmware" / "comm_tool_f103ret6" / "source" / "iap" / "ct_iap.c")
    board_c = read_text(COMM_TOOL_BSP_SOURCE / "board.c")
    board_uart_c = read_text(COMM_TOOL_BSP_SOURCE / "board_uart.c")
    board_can_c = read_text(COMM_TOOL_BSP_SOURCE / "board_can.c")
    comm_tool_uvprojx = read_text(COMM_TOOL_KEIL_PROJECT)
    comm_tool_iap_uvprojx = read_text(COMM_TOOL_IAP_PROJECT)
    elog_cfg_h = read_text(ELOG_CFG_H)
    project_config_h = read_text(PROJECT_CONFIG)
    build_guard_h = read_text(BUILD_GUARD)
    bms_can_c = read_text(CAN_HDX_C)

    if (
        "PC 上位机 <UART> comm tool" in arch_doc
        and "0x08004800" in arch_doc
        and "0x08008000" in arch_doc
        and "0x08018000" in arch_doc
        and "FW_BEGIN" in serial_doc
        and "FW_DATA" in serial_doc
        and "ConfirmAppAddress 0x08004800" in serial_doc
        and "ConfirmAppAddress 0x08008000" in serial_doc
        and "GET_STATUS" in service_doc
        and "ENTER_IAP" in service_doc
        and "0x14F8F000" in iap_doc
        and "0x14000000" in iap_doc
        and "COMMIT" in iap_doc
        and "0x0801F800" in iap_reliability_doc
        and "VTOR" in iap_reliability_doc
    ):
        reporter.ok("comm tool architecture and protocol documents record UART/CAN/IAP rules")
    else:
        reporter.fail("comm tool docs should record UART chain, 0x08004800, cache area, commands, and CAN-IAP commit protocol")

    if (
        "CMD_FW_BEGIN" in host_py
        and "CMD_FW_DATA" in host_py
        and "CMD_UPGRADE" in host_py
        and "BMS_APP_BASE_ADDR = 0x08004800" in host_py
        and "COMM_TOOL_APP_BASE_ADDR = 0x08008000" in host_py
        and "confirm_app_address" in host_py
        and "AppAddress" in start_ps1
        and "pyserial" in host_py
        and "comm_tool_host.py" in start_ps1
        and "ConfirmAppAddress" in start_ps1
    ):
        reporter.ok("PC comm tool host enforces serial protocol and App address confirmation")
    else:
        reporter.fail("PC comm tool host should support firmware download, upgrade commands, pyserial, and 0x08004800 confirmation")

    if (
        "CT_SELF_APP_BASE               0x08008000u" in config_h
        and "CT_FW_CACHE_BASE               0x08018000u" in config_h
        and "CtProtocol_Encode" in protocol_c
        and "CtDebugLog_Record" in protocol_c
        and "CtFlash_Begin" in flash_c
        and "CT_FW_CACHE_BASE" in flash_c
        and "CtCan_IapSendCommit" in can_c
        and "CtCan_IapWaitAck" in can_c
        and "CtUpgrade_Start" in upgrade_c
        and "CtUpgrade_Task" in upgrade_c
        and "frames_this_block" in upgrade_c
        and "frame_count = s_ctx.seq" in upgrade_c
        and "CT_CMD_FW_BEGIN" in app_c
        and "CT_CMD_UPGRADE" in app_c
        and "CtSelfIap_PollCan" in app_c
    ):
        reporter.ok("comm tool firmware source contains protocol, flash cache, CAN gateway, and upgrade manager")
    else:
        reporter.fail("comm tool firmware source should contain protocol parser, flash cache, CAN-IAP commit, ACK wait, and command dispatch")

    if (
        "CT_BUILD_PROFILE_RELEASE" in config_h
        and "CT_DEBUG_LOG_ENABLE" in config_h
        and "CT_DEBUG_LOG_CAPACITY" in config_h
        and "CMD_DEBUG_LOG" in host_py
        and "debug-log" in host_py
        and "CT_CMD_DEBUG_LOG" in app_c
        and "handle_debug_log" in app_c
        and "CtDebugLog_EncodeLatest" in app_c
        and "CtDebugLog_Record" in debug_log_h
        and "CT_DEBUG_LOG_ENABLE" in debug_log_c
        and "CT_LOG_EVT_CMD_RX" in debug_log_h
        and "ct_debug_log.c" in comm_tool_uvprojx
        and "ct_debug_log.h" in comm_tool_uvprojx
        and "串口" in review_html
        and "环形结构化日志" in review_html
        and "Release" in review_html
        and "Debug" in review_html
    ):
        reporter.ok("comm tool debug logging is structured, protocol-readable, and release gated")
    else:
        reporter.fail("comm tool debug logging should use CT_CMD_DEBUG_LOG ring records and stay disabled in Release")

    if "PROJECT_CFG_DEBUG_SERIAL_LOG_ENABLE" in project_config_h:
        reporter.fail("PROJECT_CFG_DEBUG_SERIAL_LOG_ENABLE should not be a current app config macro")
    else:
        reporter.ok("BMS App serial log macro is removed from current app config")

    if (
        "COMM_TOOL_F103RET6.uvprojx" in keil_doc
        and "COMM_TOOL_IAP.uvprojx" in keil_doc
        and "CT_COMM_UART_PORT" in keil_doc
        and "USART1" in keil_doc
        and "PB6" in keil_doc
        and "PB7" in keil_doc
        and "USART3" in keil_doc
        and "PC10" in keil_doc
        and "PC11" in keil_doc
        and "PA11" in keil_doc
        and "PA12" in keil_doc
        and "PB15" in keil_doc
        and "PC12" in keil_doc
        and "PC13" in keil_doc
        and "PD2" in keil_doc
        and "0x08008000" in keil_doc
        and "0x08018000" in keil_doc
        and "STM32F103RE" in comm_tool_uvprojx
        and "COMM_TOOL_Release" in comm_tool_uvprojx
        and "IROM(0x08008000,0x10000)" in comm_tool_uvprojx
        and "ct_app.c" in comm_tool_uvprojx
        and "board_uart.c" in comm_tool_uvprojx
        and "COMM_TOOL_IAP" in comm_tool_iap_uvprojx
        and "IROM(0x08000000,0x8000)" in comm_tool_iap_uvprojx
        and "ct_iap.c" in comm_tool_iap_uvprojx
        and "ct_iap_main.c" in comm_tool_iap_uvprojx
        and "CT_COMM_UART_PORT              CT_COMM_UART_PORT_USART1" in config_h
        and "CT_COMM_UART_PORT_USART3" in config_h
        and "BOARD_UART_INSTANCE" in board_uart_c
        and "BOARD_UART_IRQHandler" in board_uart_c
        and "GPIO_Remap_USART1" in board_uart_c
        and "GPIO_PartialRemap_USART3" in board_uart_c
        and "USART1_IRQHandler" in board_uart_c
        and "USART3" in board_uart_c
        and "IAP_SERIAL_USART" in iap_c
        and "IAP_SERIAL_IRQHandler" in iap_c
        and "GPIO_Remap_USART1" in iap_c
        and "GPIO_PartialRemap_USART3" in iap_c
        and "-Port USART1" in uart_select_doc
        and "-Port USART3" in uart_select_doc
        and "ValidateSet('USART1', 'USART3')" in uart_select_ps1
        and "USB_LP_CAN1_RX0_IRQHandler" in board_can_c
        and "CAN_FilterScale_32bit" in board_can_c
        and "GPIO_Pin_11" in board_can_c
        and "GPIO_Pin_12" in board_can_c
        and "BOARD_DEBUG_LED_PIN" in board_c
        and "BOARD_CAN_POWER_PIN" in board_c
        and "BOARD_PWSV_CTRL_PIN" in board_c
        and "BOARD_PWSV_STB_PIN" in board_c
        and "SysTick_Config" in board_c
    ):
        reporter.ok("comm tool Keil/BSP contract records RET6 UART selection, CAN, power, LED, and cache boundary")
    else:
        reporter.fail("comm tool Keil/BSP should fix RET6 App/IAP projects, UART selection, CAN PA11/PA12, power pins, PB15 LED, and 0x08018000 cache boundary")

    if (
        "FEIDAO_CAN_APP_CMD_GET_STATUS" in bms_can_c
        and "FEIDAO_CAN_APP_CMD_ENTER_IAP" in bms_can_c
        and "FEIDAO_CAN_APP_CMD_READ_BLOCK" in bms_can_c
        and "feidao_can_service_tx" in bms_can_c
        and "AppUpgrade_RequestIap() == 0U" in bms_can_c
        and "s_app.enter_iap_delay_ticks" in bms_can_c
    ):
        reporter.ok("BMS App exposes queued CAN service for status, block read, and entering IAP")
    else:
        reporter.fail("BMS App should expose queued CAN GET_STATUS/READ_BLOCK/ENTER_IAP service")

def check_serial_iap_refactor_contract(reporter):
    required = [
        BMS_SERIAL_IAP_REFACTOR_DOC,
        FLASH_C,
        FLASH_H,
        SCI_UPPER_C,
        CAN_HDX_C,
    ]
    if any(not path.exists() for path in required):
        missing = [str(path.relative_to(ROOT)) for path in required if not path.exists()]
        reporter.fail("serial IAP refactor files missing: {0}".format(",".join(missing)))
        return

    doc = read_text(BMS_SERIAL_IAP_REFACTOR_DOC)
    flash_c = read_text(FLASH_C)
    flash_h = read_text(FLASH_H)
    sci_upper_c = read_text(SCI_UPPER_C)
    can_hdx_c = read_text(CAN_HDX_C)

    if (
        "0xFFFD" in doc
        and "0xFFFE" in doc
        and "0xFFFF" in doc
        and "0x0801F800" in doc
        and "AppUpgrade_RequestIap" in flash_h
        and "APP_UPGRADE_MAILBOX_ADDR" in flash_c
        and "AppUpgrade_IsIapRequested()" in flash_c
        and "AppUpgrade_RequestIap() == 0U" in sci_upper_c
        and "AppUpgrade_RequestIap() == 0U" in can_hdx_c
    ):
        reporter.ok("serial IAP refactor keeps protocol entry and verifies App-to-IAP mailbox writes")
    else:
        reporter.fail("serial IAP refactor should document 0xFFFD/0xFFFE/0xFFFF and use AppUpgrade_RequestIap mailbox readback")


SOURCE = ROOT / "103 + 309/Project/Source"
PROJECT = ROOT / "103 + 309/Project/Users/CommomSH367309_16series_103RCT6_C.uvprojx"
REMOVED_SOURCES = {"IrqDebug.c", "StartupDefaultHandler.c", "ShortFunc.c",
                   "DebugHooks.c", "DebugWatch.c", "SystemDebug.c"}
REMOVED_TOKENS = ("IrqDebug", "IRQ_DEBUG_", "FlashSelfTest", "STORAGE_SELF_TEST",
                  "AFE_CURRENT_DIAG", "AFE_CURRENT_DEBUG_INJECT", "VCELL_DISP_TEST",
                  "__SOC_5_PROTECT_", "g_stLowPowerBlockDebug", "g_dbg")


def check_bms(reporter):
    tree = ET.parse(PROJECT)
    targets = tree.findall("./Targets/Target")
    if {t.findtext("TargetName") for t in targets} != {"FD_Release", "FD_Debug"}:
        reporter.fail("Expected FD_Release and FD_Debug")
    for target in targets:
        name = target.findtext("TargetName")
        common = target.find("TargetOption/TargetCommonOption")
        if common.findtext("Device") != "STM32F103CB":
            reporter.fail(name + ": MCU must be STM32F103CB")
        defines = set((target.findtext("TargetOption/TargetArmAds/Cads/VariousControls/Define") or "").split(','))
        if not {"STM32F10X_MD", "USE_STDPERIPH_DRIVER"} <= defines:
            reporter.fail(name + ": missing StdPeriph target defines")
        if any("DEBUG" in item or "BUILD_PROFILE" in item for item in defines):
            reporter.fail(name + ": obsolete runtime debug defines")
        for entry in target.findall(".//Files/File"):
            path = entry.findtext("FilePath", "")
            filename = entry.findtext("FileName", "")
            if filename in REMOVED_SOURCES:
                reporter.fail(name + ": removed source still in project: " + filename)
            if not (PROJECT.parent / path).exists():
                reporter.fail(name + ": missing project file: " + path)
        expected = ".\\Objects\\" if name == "FD_Release" else ".\\Objects_Debug\\"
        if common.findtext("OutputDirectory") != expected:
            reporter.fail(name + ": output directories must remain isolated")
        reporter.ok(name + ": MCU, source entries, defines and output directory")
    code = "\n".join(p.read_text(encoding="utf-8", errors="replace")
                     for p in SOURCE.rglob("*") if p.suffix in (".c", ".h") and "cppcheck" not in str(p))
    code = re.sub(r"/\*.*?\*/|//[^\n]*", "", code, flags=re.S)
    for token in REMOVED_TOKENS:
        if token in code:
            reporter.fail("Removed BMS instrumentation remains: " + token)
    required = ("Time_T sys_time", "extern Time_T  sys_time", "__VIRTURE_CURRENT__",
                "sys_time.CHG", "sys_time.DSG", "MCUO_DEBUG_LED1 = !MCUO_DEBUG_LED1",
                "Monitor_TempBreak", "FlashProgramHalfWordVerified", "StorageFlash_StatisticsTask")
    for token in required:
        if token not in code:
            reporter.fail("Required retained behavior missing: " + token)
    if not (SOURCE / "Heat_Cool.c").exists() or not (SOURCE / "Heat_Cool.h").exists():
        reporter.fail("Retained heating source/parameter header missing")
    for path in ("IrqDebug.c", "IrqDebug.h", "FlashSelfTest.h", "StartupDefaultHandler.c", "ShortFunc.c", "ShortFunc.h"):
        if (SOURCE / path).exists():
            reporter.fail("Removed source still exists: " + path)
    flash = (SOURCE / "Flash.h").read_text(encoding="utf-8")
    for macro, address in {"FLASH_ADDR_IAP_START":0x08000000,"FLASH_ADDR_APP_START":0x08004800,
                           "FLASH_PARAMETER_RESET_BEGIN":0x0801C000,"FLASH_STORAGE_BEGIN":0x0801E000,
                           "FLASH_STORAGE_END":0x08020000,"FLASH_STATS_A":0x0801F800,"FLASH_STATS_B":0x0801FC00}.items():
        match = re.search(r"^#define\s+"+macro+r"\s+([^\n]+)",flash,re.M)
        value = re.search(r"0x[0-9A-Fa-f]+",match[1]) if match else None
        if value is None or int(value[0],16) != address:
            reporter.fail("Flash boundary changed: " + macro)
    if "((UINT32)0x400U)" not in flash:
        reporter.fail("Flash erase page must remain 1 KiB")
    eeprom = (SOURCE / "EEPROM.h").read_text(encoding="utf-8")
    macros = set(re.findall(r"^#define\s+(\w+)",eeprom,re.M))
    if macros != {"EEPROM_H", "E2P_PARA_NUM_PROTECT", "E2P_PARA_NUM_RTC",
                  "E2P_PARA_NUM_OTHER_ELEMENT1", "E2P_PARA_NUM_RESERVED_RW_PARAM"}:
        reporter.fail("EEPROM header must expose only live parameter counts")
    reporter.ok("BMS instrumentation removed; retained features and Flash boundaries checked")


def main(argv=None):
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('-q','--quiet',action='store_true')
    parser.add_argument('--comm-tool',action='store_true',help='Also check the independent communication tool contract')
    args=parser.parse_args(argv)
    reporter=Reporter(verbose=not args.quiet)
    check_bms(reporter)
    if args.comm_tool:
        check_comm_tool_can_iap_contract(reporter)
        check_serial_iap_refactor_contract(reporter)
    return reporter.summary()


if __name__=='__main__':
    sys.exit(main())
