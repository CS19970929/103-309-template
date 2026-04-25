#ifndef UPGRADE_PARAM_POLICY_H
#define UPGRADE_PARAM_POLICY_H

/*
 * Customer upgrade parameter policy.
 *
 * Workflow for a customer-specific package:
 * 1. Modify the firmware default parameter tables.
 * 2. Enable only the reset switches that must overwrite field parameters.
 * 3. Increase UPGRADE_PARAM_POLICY_VERSION before building the package.
 */
#define UPGRADE_PARAM_POLICY_ENABLE        0
#define UPGRADE_PARAM_POLICY_VERSION       0x0001

#define UPGRADE_PARAM_RESET_AFE            1
#define UPGRADE_PARAM_RESET_PROTECT        0
#define UPGRADE_PARAM_RESET_SOC_TABLE      0
#define UPGRADE_PARAM_RESET_SOC_CONFIG     0
#define UPGRADE_PARAM_RESET_SOC_SNAPSHOT   0

/* Test only. Keep 0 in release packages to avoid repeated resets. */
#define UPGRADE_PARAM_FORCE_REAPPLY        0

#define UPGRADE_PARAM_POLICY_HAS_ACTION \
	(UPGRADE_PARAM_RESET_AFE || \
	 UPGRADE_PARAM_RESET_PROTECT || \
	 UPGRADE_PARAM_RESET_SOC_TABLE || \
	 UPGRADE_PARAM_RESET_SOC_CONFIG || \
	 UPGRADE_PARAM_RESET_SOC_SNAPSHOT)

#if UPGRADE_PARAM_POLICY_ENABLE && (UPGRADE_PARAM_POLICY_VERSION == 0xFFFF)
#error "UPGRADE_PARAM_POLICY_VERSION must not be 0xFFFF"
#endif

#endif
