#!/usr/bin/env python3
"""Current-project entry point for the full project consistency checker.

The underlying checker is retained intact in project_check_full.py. This shim
adapts checks that intentionally changed in the current BMS firmware and adds
hard guards for the STM32F103C8 persistent-storage contract.
"""

from __future__ import print_function

import re
import sys

import project_check_full as checks


checks.RELEASE_SAFE_DEFAULTS.pop("PROJECT_CFG_FACTORY_AGING_DURATION_SECONDS", None)
checks.GUARD_REQUIRED_TOKENS = [
    token for token in checks.GUARD_REQUIRED_TOKENS
    if "FACTORY_AGING" not in token
]

EEPROM_C = checks.ROOT / "103 + 309" / "Project" / "Source" / "EEPROM.c"
EEPROM_H = checks.ROOT / "103 + 309" / "Project" / "Source" / "EEPROM.h"
SH367309_DATADEAL_C = checks.ROOT / "103 + 309" / "Project" / "Source" / "SH367309_DataDeal.c"
SH367309_DATADEAL_H = checks.ROOT / "103 + 309" / "Project" / "Source" / "SH367309_DataDeal.h"


def check_required_board_features(reporter):
    required = [
        checks.PROJECT_CONFIG,
        checks.BUILD_GUARD,
        checks.FLASH_C,
        checks.UPGRADE_PARAM_POLICY_H,
    ]
    if any(not path.exists() for path in required):
        return

    project_config = checks.read_text(checks.PROJECT_CONFIG)
    build_guard = checks.read_text(checks.BUILD_GUARD)
    flash_c = checks.read_text(checks.FLASH_C)
    policy_h = checks.read_text(checks.UPGRADE_PARAM_POLICY_H)

    if (
        "PROJECT_CFG_IAP_ENABLE" not in project_config
        and "#ifdef _IAP" not in flash_c
        and "u8FlashUpdateFlag" in flash_c
        and "MCU_RESET();" in flash_c
        and "IAP is a required board feature" in build_guard
    ):
        reporter.ok("IAP is always compiled and guarded against legacy disable macros")
    else:
        reporter.fail("IAP must be always compiled and must not be a Project_Config.h switch")

    if (
        "PROJECT_CFG_UPGRADE_PARAM_POLICY_ENABLE" not in project_config
        and "#define UPGRADE_PARAM_POLICY_ENABLE        1" in policy_h
        and "#error \"Upgrade parameter policy is a required board feature and must not be disabled\"" in policy_h
        and "PROJECT_CFG_UPGRADE_PARAM_POLICY_VERSION" in policy_h
        and "PROJECT_CFG_UPGRADE_PARAM_RESET_EVENT_RECORD" in policy_h
        and "PROJECT_CFG_UPGRADE_PARAM_UPDATE_OTHER_ELEMENT" in policy_h
        and "Upgrade parameter policy is a required board feature" in build_guard
    ):
        reporter.ok("upgrade parameter policy mechanism is required while reset policy remains configurable")
    else:
        reporter.fail("upgrade parameter policy must always be present while keeping version/reset settings configurable")

    if "PROJECT_CFG_FACTORY_AGING" in project_config or "FACTORY_AGING" in checks.read_text(checks.FLASH_H):
        reporter.fail("Factory Aging configuration/storage must stay removed from the current BMS firmware")
    else:
        reporter.ok("Factory Aging configuration/storage is removed from the current BMS firmware")


def check_can_aging_soc_service(reporter):
    required = [checks.CAN_HDX_C, checks.CAN_FEIDAO_FRAMES_C, checks.FLASH_H]
    if any(not path.exists() for path in required):
        missing = [str(path.relative_to(checks.ROOT)) for path in required if not path.exists()]
        reporter.fail("CAN/SOC service files missing: {0}".format(",".join(missing)))
        return

    flash_h = checks.read_text(checks.FLASH_H)
    if "FLASH_FACTORY_AGING" in flash_h or "STORAGE_FLASH_FACTORY_AGING" in flash_h:
        reporter.fail("Factory Aging Flash API must stay removed")
    else:
        reporter.ok("Factory Aging Flash API is absent")


def check_low_power_cleanup(reporter):
    original_read_text = checks.read_text

    def read_text_without_legacy_aging_requirement(path):
        text = original_read_text(path)
        if path == checks.RTC_SLEEP_H:
            # The legacy checker used LP_BLOCK_AGING as a positive requirement.
            # Inject it only into that old check; verify the real source is clean below.
            return text + "\nLP_BLOCK_AGING\n"
        return text

    checks.read_text = read_text_without_legacy_aging_requirement
    try:
        checks._original_check_low_power_cleanup(reporter)
    finally:
        checks.read_text = original_read_text

    rtc_sleep_h = original_read_text(checks.RTC_SLEEP_H)
    if "LP_BLOCK_AGING" in rtc_sleep_h:
        reporter.fail("obsolete LP_BLOCK_AGING must stay removed")
    else:
        reporter.ok("obsolete LP_BLOCK_AGING is removed")


def check_storage_contract(reporter):
    required = [
        checks.PROJECT,
        checks.FLASH_H,
        checks.FLASH_C,
        EEPROM_C,
        EEPROM_H,
        SH367309_DATADEAL_C,
        SH367309_DATADEAL_H,
        checks.LOGRECORD_C,
        checks.SOC_ENHANCE_C,
        checks.SCI_UPPER_C,
    ]
    if any(not path.exists() for path in required):
        missing = [str(path.relative_to(checks.ROOT)) for path in required if not path.exists()]
        reporter.fail("storage contract files missing: {0}".format(",".join(missing)))
        return

    project = checks.read_text(checks.PROJECT)
    flash_h = checks.read_text(checks.FLASH_H)
    flash_c = checks.read_text(checks.FLASH_C)
    eeprom_c = checks.read_text(EEPROM_C)
    eeprom_h = checks.read_text(EEPROM_H)
    sh_c = checks.read_text(SH367309_DATADEAL_C)
    sh_h = checks.read_text(SH367309_DATADEAL_H)
    log_c = checks.read_text(checks.LOGRECORD_C)
    soc_c = checks.read_text(checks.SOC_ENHANCE_C)
    sci_c = checks.read_text(checks.SCI_UPPER_C)

    c8_contract = (
        project.count("<Device>STM32F103C8</Device>") >= 2
        and "STM32F10X_MD" in project
        and "#if !defined(STM32F10X_MD)" in flash_h
        and "FLASH_STORAGE_PAGE_SIZE           ((UINT32)0x00000400)" in flash_h
    )
    if c8_contract:
        reporter.ok("BMS target is explicitly STM32F103C8/STM32F10X_MD with 1KB Flash pages")
    else:
        reporter.fail("BMS target must stay STM32F103C8/STM32F10X_MD with 1KB Flash pages")

    partition_tokens = [
        "FLASH_ADDR_STORAGE_START           ((UINT32)0x0801E000)",
        "FLASH_ADDR_STORAGE_END             ((UINT32)0x08020000)",
        "FLASH_ADDR_STORAGE_CONFIG_SLOT_A   ((UINT32)0x0801E000)",
        "FLASH_ADDR_STORAGE_CONFIG_SLOT_B   ((UINT32)0x0801E400)",
        "FLASH_ADDR_STORAGE_SOC_SLOT_A      ((UINT32)0x0801E800)",
        "FLASH_ADDR_STORAGE_SOC_SLOT_B      ((UINT32)0x0801EC00)",
        "FLASH_ADDR_STORAGE_LOG_SLOT_A      ((UINT32)0x0801F000)",
        "FLASH_ADDR_STORAGE_LOG_DELTA_A     ((UINT32)0x0801F400)",
        "FLASH_ADDR_STORAGE_LOG_SLOT_B      ((UINT32)0x0801F800)",
        "FLASH_ADDR_STORAGE_LOG_DELTA_B     ((UINT32)0x0801FC00)",
        "FLASH_STORAGE_RECORD_ALIGNMENT     ((UINT16)4U)",
    ]
    if all(token in flash_h for token in partition_tokens):
        reporter.ok("persistent layout is fixed to eight 1KB pages at 0x0801E000..0x0801FFFF")
    else:
        reporter.fail("persistent layout/address/alignment contract drifted")

    config_tokens = [
        "typedef struct",
        "UINT16 u16FormatVersion;",
        "UINT16 u16AppliedPolicyVersion;",
        "UINT16 afe[BMS_CONFIG_AFE_WORD_COUNT];",
        "UINT16 protect[BMS_CONFIG_PROTECT_WORD_COUNT];",
        "UINT16 calibK[BMS_CONFIG_CALIB_WORD_COUNT];",
        "INT16 calibB[BMS_CONFIG_CALIB_WORD_COUNT];",
        "UINT16 other[BMS_CONFIG_OTHER_WORD_COUNT];",
        "} BMS_CONFIG;",
        "FLASH_STORAGE_CONFIG_FORMAT_VERSION    ((UINT16)0x0002U)",
    ]
    if all(token in flash_h for token in config_tokens):
        reporter.ok("BMS_CONFIG owns AFE/protect/K-B calibration/Other as one versioned image")
    else:
        reporter.fail("BMS_CONFIG must contain every persistent parameter group")

    split_flash_tokens = [
        "StorageFlash_LoadAfeData",
        "StorageFlash_SaveAfeData",
        "StorageFlash_LoadRwParamData",
        "StorageFlash_SaveRwParamData",
        "StorageFlash_GetConfigPolicyVersion",
        "StorageFlash_SetConfigPolicyVersion",
    ]
    split_hits = [token for token in split_flash_tokens if token in flash_h or token in flash_c]
    if split_hits:
        reporter.fail("category-specific Flash APIs returned: {0}".format(",".join(split_hits)))
    else:
        reporter.ok("Flash layer exposes one CONFIG object instead of parameter-category storage")

    if (
        "StorageFlash_LoadConfigData(&config)" in eeprom_c
        and "EEPROM_SaveConfigToFlash" in eeprom_c
        and "config->calibK" in eeprom_c
        and "config->calibB" in eeprom_c
        and "return EEPROM_SaveConfigToFlash();" in sh_c
    ):
        reporter.ok("boot and parameter saves build/validate/apply one BMS_CONFIG")
    else:
        reporter.fail("runtime parameter persistence must flow through one BMS_CONFIG service")

    raw_flash_tokens = ["FLASH_Unlock", "FLASH_Lock", "FLASH_ErasePage", "FLASH_ProgramHalfWord"]
    raw_log_hits = [token for token in raw_flash_tokens if token in log_c]
    if raw_log_hits:
        reporter.fail("LogRecord bypasses Flash service: {0}".format(",".join(raw_log_hits)))
    else:
        reporter.ok("Log Delta writes are routed through the Flash storage service")

    if "VERSION_V2" in soc_c:
        reporter.fail("stale SOC V2 source alias must stay removed")
    else:
        reporter.ok("SOC persistence uses CURRENT format naming only")

    if "ReadEEPROM_AFE_Parameters" in sh_c or "ReadEEPROM_AFE_Parameters" in sh_h:
        reporter.fail("stale split AFE-load API must be removed")
    else:
        reporter.ok("stale split AFE-load API is absent")

    if "EEPROM_SaveRWParametersToFlash" in eeprom_h or "EEPROM_SaveRWParametersToFlash" in sci_c:
        reporter.fail("legacy RW-parameter persistence name must be replaced by EEPROM_SaveConfigToFlash")
    else:
        reporter.ok("upper-layer writers use the unified Config persistence API name")

    if re.search(r"void\s+Sci_WrRegs_0x10_CalibCoef\s*\([^)]*\)\s*\{\s*\}", sci_c, re.S):
        reporter.fail("calibration write handler is still empty")
    else:
        reporter.ok("calibration write handler is implemented")

    irom_pattern = re.compile(
        r"<OCR_RVCT4>.*?<Type>1</Type>.*?"
        r"<StartAddress>0x8004800</StartAddress>\s*"
        r"<Size>0x19800</Size>.*?</OCR_RVCT4>",
        re.S,
    )
    irom_matches = irom_pattern.findall(project)
    if len(irom_matches) >= 2:
        reporter.ok("Keil Release/Debug IROM is bounded to 0x08004800..0x0801DFFF")
    else:
        reporter.fail("Keil Release/Debug IROM must be 0x08004800 + 0x19800")


checks._original_check_low_power_cleanup = checks.check_low_power_cleanup
checks._original_check_serial_iap_refactor_contract = checks.check_serial_iap_refactor_contract
checks.check_required_board_features = check_required_board_features
checks.check_can_aging_soc_service = check_can_aging_soc_service
checks.check_low_power_cleanup = check_low_power_cleanup


def check_serial_iap_refactor_contract(reporter):
    checks._original_check_serial_iap_refactor_contract(reporter)
    check_storage_contract(reporter)


checks.check_serial_iap_refactor_contract = check_serial_iap_refactor_contract


if __name__ == "__main__":
    sys.exit(checks.main(sys.argv[1:]))
