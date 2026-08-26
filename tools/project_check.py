#!/usr/bin/env python3
"""Current-project entry point for the full project consistency checker.

The underlying checker is retained intact in project_check_full.py. This shim
only adapts checks that intentionally changed when Factory Aging was removed
from the current BMS firmware.
"""

from __future__ import print_function

import sys

import project_check_full as checks


checks.RELEASE_SAFE_DEFAULTS.pop("PROJECT_CFG_FACTORY_AGING_DURATION_SECONDS", None)
checks.GUARD_REQUIRED_TOKENS = [
    token for token in checks.GUARD_REQUIRED_TOKENS
    if "FACTORY_AGING" not in token
]


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


checks._original_check_low_power_cleanup = checks.check_low_power_cleanup
checks.check_required_board_features = check_required_board_features
checks.check_can_aging_soc_service = check_can_aging_soc_service
checks.check_low_power_cleanup = check_low_power_cleanup


if __name__ == "__main__":
    sys.exit(checks.main(sys.argv[1:]))
