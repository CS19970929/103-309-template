# 0x10 子地址完整清单

本文把 `0x10` 写寄存器的所有子地址按功能块完整展开。

主报告入口：

- [`COMMUNICATION_LAYOUT_REPORT.md`](E:/TODO/103%20+%20309%20-%20%E5%89%AF%E6%9C%AC%20-%20%E5%89%AF%E6%9C%AC/COMMUNICATION_LAYOUT_REPORT.md)

## 校准区

- 范围：0x2000 ~ 0x205D
- 说明：电芯、AFE、总压、电流、温度校准

| 地址 | 宏名 | 入口 |
|---|---|---|
| 0x2000 | `RS485_CMD_ADDR_VC1CALIB_K` | `Sci_WrRegs_0x10_*()` |
| 0x2001 | `RS485_CMD_ADDR_VC1CALIB_B` | `Sci_WrRegs_0x10_*()` |
| 0x2002 | `RS485_CMD_ADDR_VC2CALIB_K` | `Sci_WrRegs_0x10_*()` |
| 0x2003 | `RS485_CMD_ADDR_VC2CALIB_B` | `Sci_WrRegs_0x10_*()` |
| 0x2004 | `RS485_CMD_ADDR_VC3CALIB_K` | `Sci_WrRegs_0x10_*()` |
| 0x2005 | `RS485_CMD_ADDR_VC3CALIB_B` | `Sci_WrRegs_0x10_*()` |
| 0x2006 | `RS485_CMD_ADDR_VC4CALIB_K` | `Sci_WrRegs_0x10_*()` |
| 0x2007 | `RS485_CMD_ADDR_VC4CALIB_B` | `Sci_WrRegs_0x10_*()` |
| 0x2008 | `RS485_CMD_ADDR_VC5CALIB_K` | `Sci_WrRegs_0x10_*()` |
| 0x2009 | `RS485_CMD_ADDR_VC5CALIB_B` | `Sci_WrRegs_0x10_*()` |
| 0x200A | `RS485_CMD_ADDR_VC6CALIB_K` | `Sci_WrRegs_0x10_*()` |
| 0x200B | `RS485_CMD_ADDR_VC6CALIB_B` | `Sci_WrRegs_0x10_*()` |
| 0x200C | `RS485_CMD_ADDR_VC7CALIB_K` | `Sci_WrRegs_0x10_*()` |
| 0x200D | `RS485_CMD_ADDR_VC7CALIB_B` | `Sci_WrRegs_0x10_*()` |
| 0x200E | `RS485_CMD_ADDR_VC8CALIB_K` | `Sci_WrRegs_0x10_*()` |
| 0x200F | `RS485_CMD_ADDR_VC8CALIB_B` | `Sci_WrRegs_0x10_*()` |
| 0x2010 | `RS485_CMD_ADDR_VC9CALIB_K` | `Sci_WrRegs_0x10_*()` |
| 0x2011 | `RS485_CMD_ADDR_VC9CALIB_B` | `Sci_WrRegs_0x10_*()` |
| 0x2012 | `RS485_CMD_ADDR_VC10CALIB_K` | `Sci_WrRegs_0x10_*()` |
| 0x2013 | `RS485_CMD_ADDR_VC10CALIB_B` | `Sci_WrRegs_0x10_*()` |
| 0x2014 | `RS485_CMD_ADDR_VC11CALIB_K` | `Sci_WrRegs_0x10_*()` |
| 0x2015 | `RS485_CMD_ADDR_VC11CALIB_B` | `Sci_WrRegs_0x10_*()` |
| 0x2016 | `RS485_CMD_ADDR_VC12CALIB_K` | `Sci_WrRegs_0x10_*()` |
| 0x2017 | `RS485_CMD_ADDR_VC12CALIB_B` | `Sci_WrRegs_0x10_*()` |
| 0x2018 | `RS485_CMD_ADDR_VC13CALIB_K` | `Sci_WrRegs_0x10_*()` |
| 0x2019 | `RS485_CMD_ADDR_VC13CALIB_B` | `Sci_WrRegs_0x10_*()` |
| 0x201A | `RS485_CMD_ADDR_VC14CALIB_K` | `Sci_WrRegs_0x10_*()` |
| 0x201B | `RS485_CMD_ADDR_VC14CALIB_B` | `Sci_WrRegs_0x10_*()` |
| 0x201C | `RS485_CMD_ADDR_VC15CALIB_K` | `Sci_WrRegs_0x10_*()` |
| 0x201D | `RS485_CMD_ADDR_VC15CALIB_B` | `Sci_WrRegs_0x10_*()` |
| 0x201E | `RS485_CMD_ADDR_VC16CALIB_K` | `Sci_WrRegs_0x10_*()` |
| 0x201F | `RS485_CMD_ADDR_VC16CALIB_B` | `Sci_WrRegs_0x10_*()` |
| 0x2020 | `RS485_CMD_ADDR_VC17CALIB_K` | `Sci_WrRegs_0x10_*()` |
| 0x2021 | `RS485_CMD_ADDR_VC17CALIB_B` | `Sci_WrRegs_0x10_*()` |
| 0x2022 | `RS485_CMD_ADDR_VC18CALIB_K` | `Sci_WrRegs_0x10_*()` |
| 0x2023 | `RS485_CMD_ADDR_VC18CALIB_B` | `Sci_WrRegs_0x10_*()` |
| 0x2024 | `RS485_CMD_ADDR_VC19CALIB_K` | `Sci_WrRegs_0x10_*()` |
| 0x2025 | `RS485_CMD_ADDR_VC19CALIB_B` | `Sci_WrRegs_0x10_*()` |
| 0x2026 | `RS485_CMD_ADDR_VC20CALIB_K` | `Sci_WrRegs_0x10_*()` |
| 0x2027 | `RS485_CMD_ADDR_VC20CALIB_B` | `Sci_WrRegs_0x10_*()` |
| 0x2028 | `RS485_CMD_ADDR_VC21CALIB_K` | `Sci_WrRegs_0x10_*()` |
| 0x2029 | `RS485_CMD_ADDR_VC21CALIB_B` | `Sci_WrRegs_0x10_*()` |
| 0x202A | `RS485_CMD_ADDR_VC22CALIB_K` | `Sci_WrRegs_0x10_*()` |
| 0x202B | `RS485_CMD_ADDR_VC22CALIB_B` | `Sci_WrRegs_0x10_*()` |
| 0x202C | `RS485_CMD_ADDR_VC23CALIB_K` | `Sci_WrRegs_0x10_*()` |
| 0x202D | `RS485_CMD_ADDR_VC23CALIB_B` | `Sci_WrRegs_0x10_*()` |
| 0x202E | `RS485_CMD_ADDR_VC24CALIB_K` | `Sci_WrRegs_0x10_*()` |
| 0x202F | `RS485_CMD_ADDR_VC24CALIB_B` | `Sci_WrRegs_0x10_*()` |
| 0x2030 | `RS485_CMD_ADDR_VC25CALIB_K` | `Sci_WrRegs_0x10_*()` |
| 0x2031 | `RS485_CMD_ADDR_VC25CALIB_B` | `Sci_WrRegs_0x10_*()` |
| 0x2032 | `RS485_CMD_ADDR_VC26CALIB_K` | `Sci_WrRegs_0x10_*()` |
| 0x2033 | `RS485_CMD_ADDR_VC26CALIB_B` | `Sci_WrRegs_0x10_*()` |
| 0x2034 | `RS485_CMD_ADDR_VC27CALIB_K` | `Sci_WrRegs_0x10_*()` |
| 0x2035 | `RS485_CMD_ADDR_VC27CALIB_B` | `Sci_WrRegs_0x10_*()` |
| 0x2036 | `RS485_CMD_ADDR_VC28CALIB_K` | `Sci_WrRegs_0x10_*()` |
| 0x2037 | `RS485_CMD_ADDR_VC28CALIB_B` | `Sci_WrRegs_0x10_*()` |
| 0x2038 | `RS485_CMD_ADDR_VC29CALIB_K` | `Sci_WrRegs_0x10_*()` |
| 0x2039 | `RS485_CMD_ADDR_VC29CALIB_B` | `Sci_WrRegs_0x10_*()` |
| 0x203A | `RS485_CMD_ADDR_VC30CALIB_K` | `Sci_WrRegs_0x10_*()` |
| 0x203B | `RS485_CMD_ADDR_VC30CALIB_B` | `Sci_WrRegs_0x10_*()` |
| 0x203C | `RS485_CMD_ADDR_VC31CALIB_K` | `Sci_WrRegs_0x10_*()` |
| 0x203D | `RS485_CMD_ADDR_VC31CALIB_B` | `Sci_WrRegs_0x10_*()` |
| 0x203E | `RS485_CMD_ADDR_VC32CALIB_K` | `Sci_WrRegs_0x10_*()` |
| 0x203F | `RS485_CMD_ADDR_VC32CALIB_B` | `Sci_WrRegs_0x10_*()` |
| 0x2040 | `RS485_CMD_ADDR_AFE1CALIB_K` | `Sci_WrRegs_0x10_*()` |
| 0x2041 | `RS485_CMD_ADDR_AFE1CALIB_B` | `Sci_WrRegs_0x10_*()` |
| 0x2042 | `RS485_CMD_ADDR_AFE2CALIB_K` | `Sci_WrRegs_0x10_*()` |
| 0x2043 | `RS485_CMD_ADDR_AFE2CALIB_B` | `Sci_WrRegs_0x10_*()` |
| 0x2044 | `RS485_CMD_ADDR_VBUSCALIB_K` | `Sci_WrRegs_0x10_*()` |
| 0x2045 | `RS485_CMD_ADDR_VBUSCALIB_B` | `Sci_WrRegs_0x10_*()` |
| 0x2046 | `RS485_CMD_ADDR_ICHGCALIB_K` | `Sci_WrRegs_0x10_*()` |
| 0x2047 | `RS485_CMD_ADDR_ICHGCALIB_B` | `Sci_WrRegs_0x10_*()` |
| 0x2048 | `RS485_CMD_ADDR_IDISCHGCALIB_K` | `Sci_WrRegs_0x10_*()` |
| 0x2049 | `RS485_CMD_ADDR_IDISCHGCALIB_B` | `Sci_WrRegs_0x10_*()` |
| 0x204A | `RS485_CMD_ADDR_TEMP1_CALIB_K` | `Sci_WrRegs_0x10_*()` |
| 0x204B | `RS485_CMD_ADDR_TEMP1_CALIB_B` | `Sci_WrRegs_0x10_*()` |
| 0x204C | `RS485_CMD_ADDR_TEMP2_CALIB_K` | `Sci_WrRegs_0x10_*()` |
| 0x204D | `RS485_CMD_ADDR_TEMP2_CALIB_B` | `Sci_WrRegs_0x10_*()` |
| 0x204E | `RS485_CMD_ADDR_TEMP3_CALIB_K` | `Sci_WrRegs_0x10_*()` |
| 0x204F | `RS485_CMD_ADDR_TEMP3_CALIB_B` | `Sci_WrRegs_0x10_*()` |
| 0x2050 | `RS485_CMD_ADDR_TEMP4_CALIB_K` | `Sci_WrRegs_0x10_*()` |
| 0x2051 | `RS485_CMD_ADDR_TEMP4_CALIB_B` | `Sci_WrRegs_0x10_*()` |
| 0x2052 | `RS485_CMD_ADDR_TEMP5_CALIB_K` | `Sci_WrRegs_0x10_*()` |
| 0x2053 | `RS485_CMD_ADDR_TEMP5_CALIB_B` | `Sci_WrRegs_0x10_*()` |
| 0x2054 | `RS485_CMD_ADDR_TEMP6_CALIB_K` | `Sci_WrRegs_0x10_*()` |
| 0x2055 | `RS485_CMD_ADDR_TEMP6_CALIB_B` | `Sci_WrRegs_0x10_*()` |
| 0x2056 | `RS485_CMD_ADDR_TEMP_ENV1_CALIB_K` | `Sci_WrRegs_0x10_*()` |
| 0x2057 | `RS485_CMD_ADDR_TEMP_ENV1_CALIB_B` | `Sci_WrRegs_0x10_*()` |
| 0x2058 | `RS485_CMD_ADDR_TEMP_ENV2_CALIB_K` | `Sci_WrRegs_0x10_*()` |
| 0x2059 | `RS485_CMD_ADDR_TEMP_ENV2_CALIB_B` | `Sci_WrRegs_0x10_*()` |
| 0x205A | `RS485_CMD_ADDR_TEMP_ENV3_CALIB_K` | `Sci_WrRegs_0x10_*()` |
| 0x205B | `RS485_CMD_ADDR_TEMP_ENV3_CALIB_B` | `Sci_WrRegs_0x10_*()` |
| 0x205C | `RS485_CMD_ADDR_TEMP_MOS_CALIB_K` | `Sci_WrRegs_0x10_*()` |
| 0x205D | `RS485_CMD_ADDR_TEMP_MOS_CALIB_B` | `Sci_WrRegs_0x10_*()` |

## 保护区

- 范围：0x2100 ~ 0x2140
- 说明：单体/总压/电流/温度/压差/SOC 保护

| 地址 | 宏名 | 入口 |
|---|---|---|
| 0x2100 | `RS485_CMD_ADDR_VCELL_OVP_FIRST` | `Sci_WrRegs_0x10_*()` |
| 0x2101 | `RS485_CMD_ADDR_VCELL_OVP_SECOND` | `Sci_WrRegs_0x10_*()` |
| 0x2102 | `RS485_CMD_ADDR_VCELL_OVP_THIRD` | `Sci_WrRegs_0x10_*()` |
| 0x2103 | `RS485_CMD_ADDR_VCELL_OVP_RCV` | `Sci_WrRegs_0x10_*()` |
| 0x2104 | `RS485_CMD_ADDR_VCELL_OVP_FILTER` | `Sci_WrRegs_0x10_*()` |
| 0x2105 | `RS485_CMD_ADDR_VCELL_UVP_FIRST` | `Sci_WrRegs_0x10_*()` |
| 0x2106 | `RS485_CMD_ADDR_VCELL_UVP_SECOND` | `Sci_WrRegs_0x10_*()` |
| 0x2107 | `RS485_CMD_ADDR_VCELL_UVP_THIRD` | `Sci_WrRegs_0x10_*()` |
| 0x2108 | `RS485_CMD_ADDR_VCELL_UVP_RCV` | `Sci_WrRegs_0x10_*()` |
| 0x2109 | `RS485_CMD_ADDR_VCELL_UVP_FILTER` | `Sci_WrRegs_0x10_*()` |
| 0x210A | `RS485_CMD_ADDR_VBUS_OVP_FIRST` | `Sci_WrRegs_0x10_*()` |
| 0x210B | `RS485_CMD_ADDR_VBUS_OVP_SECOND` | `Sci_WrRegs_0x10_*()` |
| 0x210C | `RS485_CMD_ADDR_VBUS_OVP_THIRD` | `Sci_WrRegs_0x10_*()` |
| 0x210D | `RS485_CMD_ADDR_VBUS_OVP_RCV` | `Sci_WrRegs_0x10_*()` |
| 0x210E | `RS485_CMD_ADDR_VBUS_OVP_FILTER` | `Sci_WrRegs_0x10_*()` |
| 0x210F | `RS485_CMD_ADDR_VBUS_UVP_FIRST` | `Sci_WrRegs_0x10_*()` |
| 0x2110 | `RS485_CMD_ADDR_VBUS_UVP_SECOND` | `Sci_WrRegs_0x10_*()` |
| 0x2111 | `RS485_CMD_ADDR_VBUS_UVP_THIRD` | `Sci_WrRegs_0x10_*()` |
| 0x2112 | `RS485_CMD_ADDR_VBUS_UVP_RCV` | `Sci_WrRegs_0x10_*()` |
| 0x2113 | `RS485_CMD_ADDR_VBUS_UVP_FILTER` | `Sci_WrRegs_0x10_*()` |
| 0x2114 | `RS485_CMD_ADDR_ICHG_OCP_FIRST` | `Sci_WrRegs_0x10_*()` |
| 0x2115 | `RS485_CMD_ADDR_ICHG_OCP_SECOND` | `Sci_WrRegs_0x10_*()` |
| 0x2116 | `RS485_CMD_ADDR_ICHG_OCP_THIRD` | `Sci_WrRegs_0x10_*()` |
| 0x2117 | `RS485_CMD_ADDR_ICHG_OCP_RCV` | `Sci_WrRegs_0x10_*()` |
| 0x2118 | `RS485_CMD_ADDR_ICHG_OCP_FILTER` | `Sci_WrRegs_0x10_*()` |
| 0x2119 | `RS485_CMD_ADDR_IDSG_OCP_FIRST` | `Sci_WrRegs_0x10_*()` |
| 0x211A | `RS485_CMD_ADDR_IDSG_OCP_SECOND` | `Sci_WrRegs_0x10_*()` |
| 0x211B | `RS485_CMD_ADDR_IDSG_OCP_THIRD` | `Sci_WrRegs_0x10_*()` |
| 0x211C | `RS485_CMD_ADDR_IDSG_OCP_RCV` | `Sci_WrRegs_0x10_*()` |
| 0x211D | `RS485_CMD_ADDR_IDSG_OCP_FILTER` | `Sci_WrRegs_0x10_*()` |
| 0x211E | `RS485_CMD_ADDR_TCHG_OTP_FIRST` | `Sci_WrRegs_0x10_*()` |
| 0x211F | `RS485_CMD_ADDR_TCHG_OTP_SECOND` | `Sci_WrRegs_0x10_*()` |
| 0x2120 | `RS485_CMD_ADDR_TCHG_OTP_THIRD` | `Sci_WrRegs_0x10_*()` |
| 0x2121 | `RS485_CMD_ADDR_TCHG_OTP_RCV` | `Sci_WrRegs_0x10_*()` |
| 0x2122 | `RS485_CMD_ADDR_TCHG_OTP_FILTER` | `Sci_WrRegs_0x10_*()` |
| 0x2123 | `RS485_CMD_ADDR_TCHG_UTP_FIRST` | `Sci_WrRegs_0x10_*()` |
| 0x2124 | `RS485_CMD_ADDR_TCHG_UTP_SECOND` | `Sci_WrRegs_0x10_*()` |
| 0x2125 | `RS485_CMD_ADDR_TCHG_UTP_THIRD` | `Sci_WrRegs_0x10_*()` |
| 0x2126 | `RS485_CMD_ADDR_TCHG_UTP_RCV` | `Sci_WrRegs_0x10_*()` |
| 0x2127 | `RS485_CMD_ADDR_TCHG_UTP_FILTER` | `Sci_WrRegs_0x10_*()` |
| 0x2128 | `RS485_CMD_ADDR_TDSG_OTP_FIRST` | `Sci_WrRegs_0x10_*()` |
| 0x2129 | `RS485_CMD_ADDR_TDSG_OTP_SECOND` | `Sci_WrRegs_0x10_*()` |
| 0x212A | `RS485_CMD_ADDR_TDSG_OTP_THIRD` | `Sci_WrRegs_0x10_*()` |
| 0x212B | `RS485_CMD_ADDR_TDSG_OTP_RCV` | `Sci_WrRegs_0x10_*()` |
| 0x212C | `RS485_CMD_ADDR_TDSG_OTP_FILTER` | `Sci_WrRegs_0x10_*()` |
| 0x212D | `RS485_CMD_ADDR_TDSG_UTP_FIRST` | `Sci_WrRegs_0x10_*()` |
| 0x212E | `RS485_CMD_ADDR_TDSG_UTP_SECOND` | `Sci_WrRegs_0x10_*()` |
| 0x212F | `RS485_CMD_ADDR_TDSG_UTP_THIRD` | `Sci_WrRegs_0x10_*()` |
| 0x2130 | `RS485_CMD_ADDR_TDSG_UTP_RCV` | `Sci_WrRegs_0x10_*()` |
| 0x2131 | `RS485_CMD_ADDR_TDSG_UTP_FILTER` | `Sci_WrRegs_0x10_*()` |
| 0x2132 | `RS485_CMD_ADDR_TMOS_OTP_FIRST` | `Sci_WrRegs_0x10_*()` |
| 0x2133 | `RS485_CMD_ADDR_TMOS_OTP_SECOND` | `Sci_WrRegs_0x10_*()` |
| 0x2134 | `RS485_CMD_ADDR_TMOS_OTP_THIRD` | `Sci_WrRegs_0x10_*()` |
| 0x2135 | `RS485_CMD_ADDR_TMOS_OTP_RCV` | `Sci_WrRegs_0x10_*()` |
| 0x2136 | `RS485_CMD_ADDR_TMOS_OTP_FILTER` | `Sci_WrRegs_0x10_*()` |
| 0x2137 | `RS485_CMD_ADDR_VDELTA_OP_FIRST` | `Sci_WrRegs_0x10_*()` |
| 0x2138 | `RS485_CMD_ADDR_VDELTA_OP_SECOND` | `Sci_WrRegs_0x10_*()` |
| 0x2139 | `RS485_CMD_ADDR_VDELTA_OP_THIRD` | `Sci_WrRegs_0x10_*()` |
| 0x213A | `RS485_CMD_ADDR_VDELTA_OP_RCV` | `Sci_WrRegs_0x10_*()` |
| 0x213B | `RS485_CMD_ADDR_VDELTA_OP_FILTER` | `Sci_WrRegs_0x10_*()` |
| 0x213C | `RS485_CMD_ADDR_SOC_UP_FIRST` | `Sci_WrRegs_0x10_*()` |
| 0x213D | `RS485_CMD_ADDR_SOC_UP_SECOND` | `Sci_WrRegs_0x10_*()` |
| 0x213E | `RS485_CMD_ADDR_SOC_UP_THIRD` | `Sci_WrRegs_0x10_*()` |
| 0x213F | `RS485_CMD_ADDR_SOC_UP_RCV` | `Sci_WrRegs_0x10_*()` |
| 0x2140 | `RS485_CMD_ADDR_SOC_UP_FILTER` | `Sci_WrRegs_0x10_*()` |

## SOC / 铜损 / RTC 区

- 范围：0x2200 ~ 0x2255
- 说明：SOC 表、铜损、串数、RTC

| 地址 | 宏名 | 入口 |
|---|---|---|
| 0x2200 | `RS485_CMD_ADDR_SOC_VOLTAGE1` | `Sci_WrRegs_0x10_*()` |
| 0x2201 | `RS485_CMD_ADDR_SOC_VALUE1` | `Sci_WrRegs_0x10_*()` |
| 0x2202 | `RS485_CMD_ADDR_SOC_VOLTAGE2` | `Sci_WrRegs_0x10_*()` |
| 0x2203 | `RS485_CMD_ADDR_SOC_VALUE2` | `Sci_WrRegs_0x10_*()` |
| 0x2204 | `RS485_CMD_ADDR_SOC_VOLTAGE3` | `Sci_WrRegs_0x10_*()` |
| 0x2205 | `RS485_CMD_ADDR_SOC_VALUE3` | `Sci_WrRegs_0x10_*()` |
| 0x2206 | `RS485_CMD_ADDR_SOC_VOLTAGE4` | `Sci_WrRegs_0x10_*()` |
| 0x2207 | `RS485_CMD_ADDR_SOC_VALUE4` | `Sci_WrRegs_0x10_*()` |
| 0x2208 | `RS485_CMD_ADDR_SOC_VOLTAGE5` | `Sci_WrRegs_0x10_*()` |
| 0x2209 | `RS485_CMD_ADDR_SOC_VALUE5` | `Sci_WrRegs_0x10_*()` |
| 0x220A | `RS485_CMD_ADDR_SOC_VOLTAGE6` | `Sci_WrRegs_0x10_*()` |
| 0x220B | `RS485_CMD_ADDR_SOC_VALUE6` | `Sci_WrRegs_0x10_*()` |
| 0x220C | `RS485_CMD_ADDR_SOC_VOLTAGE7` | `Sci_WrRegs_0x10_*()` |
| 0x220D | `RS485_CMD_ADDR_SOC_VALUE7` | `Sci_WrRegs_0x10_*()` |
| 0x220E | `RS485_CMD_ADDR_SOC_VOLTAGE8` | `Sci_WrRegs_0x10_*()` |
| 0x220F | `RS485_CMD_ADDR_SOC_VALUE8` | `Sci_WrRegs_0x10_*()` |
| 0x2210 | `RS485_CMD_ADDR_SOC_VOLTAGE9` | `Sci_WrRegs_0x10_*()` |
| 0x2211 | `RS485_CMD_ADDR_SOC_VALUE9` | `Sci_WrRegs_0x10_*()` |
| 0x2212 | `RS485_CMD_ADDR_SOC_VOLTAGE10` | `Sci_WrRegs_0x10_*()` |
| 0x2213 | `RS485_CMD_ADDR_SOC_VALUE10` | `Sci_WrRegs_0x10_*()` |
| 0x2214 | `RS485_CMD_ADDR_SOC_VOLTAGE11` | `Sci_WrRegs_0x10_*()` |
| 0x2215 | `RS485_CMD_ADDR_SOC_VALUE11` | `Sci_WrRegs_0x10_*()` |
| 0x2216 | `RS485_CMD_ADDR_SOC_VOLTAGE12` | `Sci_WrRegs_0x10_*()` |
| 0x2217 | `RS485_CMD_ADDR_SOC_VALUE12` | `Sci_WrRegs_0x10_*()` |
| 0x2218 | `RS485_CMD_ADDR_SOC_VOLTAGE13` | `Sci_WrRegs_0x10_*()` |
| 0x2219 | `RS485_CMD_ADDR_SOC_VALUE13` | `Sci_WrRegs_0x10_*()` |
| 0x221A | `RS485_CMD_ADDR_SOC_VOLTAGE14` | `Sci_WrRegs_0x10_*()` |
| 0x221B | `RS485_CMD_ADDR_SOC_VALUE14` | `Sci_WrRegs_0x10_*()` |
| 0x221C | `RS485_CMD_ADDR_SOC_VOLTAGE15` | `Sci_WrRegs_0x10_*()` |
| 0x221D | `RS485_CMD_ADDR_SOC_VALUE15` | `Sci_WrRegs_0x10_*()` |
| 0x221E | `RS485_CMD_ADDR_SOC_VOLTAGE16` | `Sci_WrRegs_0x10_*()` |
| 0x221F | `RS485_CMD_ADDR_SOC_VALUE16` | `Sci_WrRegs_0x10_*()` |
| 0x2220 | `RS485_CMD_ADDR_SOC_VOLTAGE17` | `Sci_WrRegs_0x10_*()` |
| 0x2221 | `RS485_CMD_ADDR_SOC_VALUE17` | `Sci_WrRegs_0x10_*()` |
| 0x2222 | `RS485_CMD_ADDR_SOC_VOLTAGE18` | `Sci_WrRegs_0x10_*()` |
| 0x2223 | `RS485_CMD_ADDR_SOC_VALUE18` | `Sci_WrRegs_0x10_*()` |
| 0x2224 | `RS485_CMD_ADDR_SOC_VOLTAGE19` | `Sci_WrRegs_0x10_*()` |
| 0x2225 | `RS485_CMD_ADDR_SOC_VALUE19` | `Sci_WrRegs_0x10_*()` |
| 0x2226 | `RS485_CMD_ADDR_SOC_VOLTAGE20` | `Sci_WrRegs_0x10_*()` |
| 0x2227 | `RS485_CMD_ADDR_SOC_VALUE20` | `Sci_WrRegs_0x10_*()` |
| 0x2228 | `RS485_CMD_ADDR_SOC_VOLTAGE21` | `Sci_WrRegs_0x10_*()` |
| 0x2229 | `RS485_CMD_ADDR_SOC_VALUE21` | `Sci_WrRegs_0x10_*()` |
| 0x222A | `RS485_CMD_ADDR_COPPERLOSS1` | `Sci_WrRegs_0x10_*()` |
| 0x222B | `RS485_CMD_ADDR_COPPERLOSS2` | `Sci_WrRegs_0x10_*()` |
| 0x222C | `RS485_CMD_ADDR_COPPERLOSS3` | `Sci_WrRegs_0x10_*()` |
| 0x222D | `RS485_CMD_ADDR_COPPERLOSS4` | `Sci_WrRegs_0x10_*()` |
| 0x222E | `RS485_CMD_ADDR_COPPERLOSS5` | `Sci_WrRegs_0x10_*()` |
| 0x222F | `RS485_CMD_ADDR_COPPERLOSS6` | `Sci_WrRegs_0x10_*()` |
| 0x2230 | `RS485_CMD_ADDR_COPPERLOSS7` | `Sci_WrRegs_0x10_*()` |
| 0x2231 | `RS485_CMD_ADDR_COPPERLOSS8` | `Sci_WrRegs_0x10_*()` |
| 0x2232 | `RS485_CMD_ADDR_COPPERLOSS9` | `Sci_WrRegs_0x10_*()` |
| 0x2233 | `RS485_CMD_ADDR_COPPERLOSS10` | `Sci_WrRegs_0x10_*()` |
| 0x2234 | `RS485_CMD_ADDR_COPPERLOSS11` | `Sci_WrRegs_0x10_*()` |
| 0x2235 | `RS485_CMD_ADDR_COPPERLOSS12` | `Sci_WrRegs_0x10_*()` |
| 0x2236 | `RS485_CMD_ADDR_COPPERLOSS13` | `Sci_WrRegs_0x10_*()` |
| 0x2237 | `RS485_CMD_ADDR_COPPERLOSS14` | `Sci_WrRegs_0x10_*()` |
| 0x2238 | `RS485_CMD_ADDR_COPPERLOSS15` | `Sci_WrRegs_0x10_*()` |
| 0x2239 | `RS485_CMD_ADDR_COPPERLOSS16` | `Sci_WrRegs_0x10_*()` |
| 0x223A | `RS485_CMD_ADDR_CELLNUM1` | `Sci_WrRegs_0x10_*()` |
| 0x223B | `RS485_CMD_ADDR_CELLNUM2` | `Sci_WrRegs_0x10_*()` |
| 0x223C | `RS485_CMD_ADDR_CELLNUM3` | `Sci_WrRegs_0x10_*()` |
| 0x223D | `RS485_CMD_ADDR_CELLNUM4` | `Sci_WrRegs_0x10_*()` |
| 0x223E | `RS485_CMD_ADDR_CELLNUM5` | `Sci_WrRegs_0x10_*()` |
| 0x223F | `RS485_CMD_ADDR_CELLNUM6` | `Sci_WrRegs_0x10_*()` |
| 0x2240 | `RS485_CMD_ADDR_CELLNUM7` | `Sci_WrRegs_0x10_*()` |
| 0x2241 | `RS485_CMD_ADDR_CELLNUM8` | `Sci_WrRegs_0x10_*()` |
| 0x2242 | `RS485_CMD_ADDR_CELLNUM9` | `Sci_WrRegs_0x10_*()` |
| 0x2243 | `RS485_CMD_ADDR_CELLNUM10` | `Sci_WrRegs_0x10_*()` |
| 0x2244 | `RS485_CMD_ADDR_CELLNUM11` | `Sci_WrRegs_0x10_*()` |
| 0x2245 | `RS485_CMD_ADDR_CELLNUM12` | `Sci_WrRegs_0x10_*()` |
| 0x2246 | `RS485_CMD_ADDR_CELLNUM13` | `Sci_WrRegs_0x10_*()` |
| 0x2247 | `RS485_CMD_ADDR_CELLNUM14` | `Sci_WrRegs_0x10_*()` |
| 0x2248 | `RS485_CMD_ADDR_CELLNUM15` | `Sci_WrRegs_0x10_*()` |
| 0x2249 | `RS485_CMD_ADDR_CELLNUM16` | `Sci_WrRegs_0x10_*()` |
| 0x224A | `RS485_CMD_ADDR_RTC_TIME_YEAR` | `Sci_WrRegs_0x10_*()` |
| 0x224B | `RS485_CMD_ADDR_RTC_TIME_MONTH` | `Sci_WrRegs_0x10_*()` |
| 0x224C | `RS485_CMD_ADDR_RTC_TIME_DAY` | `Sci_WrRegs_0x10_*()` |
| 0x224D | `RS485_CMD_ADDR_RTC_TIME_HOUR` | `Sci_WrRegs_0x10_*()` |
| 0x224E | `RS485_CMD_ADDR_RTC_TIME_MINUTE` | `Sci_WrRegs_0x10_*()` |
| 0x224F | `RS485_CMD_ADDR_RTC_TIME_SECOND` | `Sci_WrRegs_0x10_*()` |
| 0x2250 | `RS485_CMD_ADDR_RTC_ALARM_YEAR` | `Sci_WrRegs_0x10_*()` |
| 0x2251 | `RS485_CMD_ADDR_RTC_ALARM_MONTH` | `Sci_WrRegs_0x10_*()` |
| 0x2252 | `RS485_CMD_ADDR_RTC_ALARM_DAY` | `Sci_WrRegs_0x10_*()` |
| 0x2253 | `RS485_CMD_ADDR_RTC_ALARM_HOUR` | `Sci_WrRegs_0x10_*()` |
| 0x2254 | `RS485_CMD_ADDR_RTC_ALARM_MINUTE` | `Sci_WrRegs_0x10_*()` |
| 0x2255 | `RS485_CMD_ADDR_RTC_ALARM_SECOND` | `Sci_WrRegs_0x10_*()` |

## 均衡 / 系统 / 睡眠 / 热管理区

- 范围：0x2300 ~ 0x2337
- 说明：均衡、系统、睡眠、SOC 扩展、热管理

| 地址 | 宏名 | 入口 |
|---|---|---|
| 0x2300 | `RS485_CMD_ADDR_BALANCE_OV` | `Sci_WrRegs_0x10_*()` |
| 0x2301 | `RS485_CMD_ADDR_BALANCE_OW` | `Sci_WrRegs_0x10_*()` |
| 0x2302 | `RS485_CMD_ADDR_BALANCE_CW1` | `Sci_WrRegs_0x10_*()` |
| 0x2303 | `RS485_CMD_ADDR_BALANCE_CW2` | `Sci_WrRegs_0x10_*()` |
| 0x2304 | `RS485_CMD_ADDR_OPENTIME_ODD` | `Sci_WrRegs_0x10_*()` |
| 0x2305 | `RS485_CMD_ADDR_OPENTIME_EVEN` | `Sci_WrRegs_0x10_*()` |
| 0x2306 | `RS485_CMD_ADDR_OPENTIME_MOS` | `Sci_WrRegs_0x10_*()` |
| 0x2307 | `RS485_CMD_ADDR_OPENTIME_RES` | `Sci_WrRegs_0x10_*()` |
| 0x2308 | `RS485_CMD_ADDR_CS_CUR_CHGMAX` | `Sci_WrRegs_0x10_*()` |
| 0x2309 | `RS485_CMD_ADDR_CS_CUR_DSGMAX` | `Sci_WrRegs_0x10_*()` |
| 0x230A | `RS485_CMD_ADDR_CBC_CUR_CHG` | `Sci_WrRegs_0x10_*()` |
| 0x230B | `RS485_CMD_ADDR_CBC_CUR_DSG` | `Sci_WrRegs_0x10_*()` |
| 0x230C | `RS485_CMD_ADDR_COOL_DSG_H` | `Sci_WrRegs_0x10_*()` |
| 0x230D | `RS485_CMD_ADDR_COOL_DSG_L` | `Sci_WrRegs_0x10_*()` |
| 0x230E | `RS485_CMD_ADDR_COOL_CHG_H` | `Sci_WrRegs_0x10_*()` |
| 0x230F | `RS485_CMD_ADDR_COOL_CHG_L` | `Sci_WrRegs_0x10_*()` |
| 0x2310 | `RS485_CMD_ADDR_SLEEP_V_NORMAL` | `Sci_WrRegs_0x10_*()` |
| 0x2311 | `RS485_CMD_ADDR_SLEEP_TIME_NORMAL` | `Sci_WrRegs_0x10_*()` |
| 0x2312 | `RS485_CMD_ADDR_SLEEP_V_LOW` | `Sci_WrRegs_0x10_*()` |
| 0x2313 | `RS485_CMD_ADDR_SLEEP_TIME_LOW` | `Sci_WrRegs_0x10_*()` |
| 0x2314 | `RS485_CMD_ADDR_SLEEP_I_CHG` | `Sci_WrRegs_0x10_*()` |
| 0x2315 | `RS485_CMD_ADDR_SLEEP_I_DSG` | `Sci_WrRegs_0x10_*()` |
| 0x2316 | `RS485_CMD_ADDR_SLEEP_RES1` | `Sci_WrRegs_0x10_*()` |
| 0x2317 | `RS485_CMD_ADDR_SLEEP_RES2` | `Sci_WrRegs_0x10_*()` |
| 0x2318 | `RS485_CMD_ADDR_SOC_AH` | `Sci_WrRegs_0x10_*()` |
| 0x2319 | `RS485_CMD_ADDR_SOC_CYCLE_TIME` | `Sci_WrRegs_0x10_*()` |
| 0x231A | `RS485_CMD_ADDR_SOC_RES1` | `Sci_WrRegs_0x10_*()` |
| 0x231B | `RS485_CMD_ADDR_SOC_RES2` | `Sci_WrRegs_0x10_*()` |
| 0x231C | `RS485_CMD_ADDR_SYS_SERIES_NUM` | `Sci_WrRegs_0x10_*()` |
| 0x231D | `RS485_CMD_ADDR_SYS_CS_RESIS` | `Sci_WrRegs_0x10_*()` |
| 0x231E | `RS485_CMD_ADDR_SYS_CS_NUM` | `Sci_WrRegs_0x10_*()` |
| 0x231F | `RS485_CMD_ADDR_SYS_RES1` | `Sci_WrRegs_0x10_*()` |
| 0x2320 | `RS485_CMD_ADDR_HEAT_DSG_HIGH` | `Sci_WrRegs_0x10_*()` |
| 0x2321 | `RS485_CMD_ADDR_HEAT_DSG_LOW` | `Sci_WrRegs_0x10_*()` |
| 0x2322 | `RS485_CMD_ADDR_HEAT_CHG_HIGH` | `Sci_WrRegs_0x10_*()` |
| 0x2323 | `RS485_CMD_ADDR_HEAT_CHG_LOW` | `Sci_WrRegs_0x10_*()` |
| 0x2324 | `RS485_CMD_ADDR_HEAT_CUR_MAX` | `Sci_WrRegs_0x10_*()` |
| 0x2325 | `RS485_CMD_ADDR_HEAT_CUR_MIN` | `Sci_WrRegs_0x10_*()` |
| 0x2326 | `RS485_CMD_ADDR_HEAT_TIME_MAX` | `Sci_WrRegs_0x10_*()` |
| 0x2327 | `RS485_CMD_ADDR_HEAT_RES1` | `Sci_WrRegs_0x10_*()` |
| 0x2328 | `RS485_CMD_ADDR_HEAT_RES2` | `Sci_WrRegs_0x10_*()` |
| 0x2329 | `RS485_CMD_ADDR_HEAT_RES3` | `Sci_WrRegs_0x10_*()` |
| 0x232A | `RS485_CMD_ADDR_HEAT_RES4` | `Sci_WrRegs_0x10_*()` |
| 0x232B | `RS485_CMD_ADDR_HEAT_RES5` | `Sci_WrRegs_0x10_*()` |
| 0x232C | `RS485_CMD_ADDR_HEAT_RES6` | `Sci_WrRegs_0x10_*()` |
| 0x232D | `RS485_CMD_ADDR_COOL_DSG_HIGH` | `Sci_WrRegs_0x10_*()` |
| 0x232E | `RS485_CMD_ADDR_COOL_DSG_LOW` | `Sci_WrRegs_0x10_*()` |
| 0x232F | `RS485_CMD_ADDR_COOL_CHG_HIGH` | `Sci_WrRegs_0x10_*()` |
| 0x2330 | `RS485_CMD_ADDR_COOL_CHG_LOW` | `Sci_WrRegs_0x10_*()` |
| 0x2331 | `RS485_CMD_ADDR_COOL_CUR_MAX` | `Sci_WrRegs_0x10_*()` |
| 0x2332 | `RS485_CMD_ADDR_COOL_CUR_MIN` | `Sci_WrRegs_0x10_*()` |
| 0x2333 | `RS485_CMD_ADDR_COOL_TIME_MAX` | `Sci_WrRegs_0x10_*()` |
| 0x2334 | `RS485_CMD_ADDR_COOL_RES1` | `Sci_WrRegs_0x10_*()` |
| 0x2335 | `RS485_CMD_ADDR_COOL_RES2` | `Sci_WrRegs_0x10_*()` |
| 0x2336 | `RS485_CMD_ADDR_COOL_RES3` | `Sci_WrRegs_0x10_*()` |
| 0x2337 | `RS485_CMD_ADDR_COOL_RES4` | `Sci_WrRegs_0x10_*()` |

## 额外地址

| 地址 | 宏名 | 入口 |
|---|---|---|
| 0xFFF0 | `RS485_ADDR_SN_SERIAL_NUM` | `Sci_WrRegs_0x10_SN_Version()` |
| 0xFFF1 | `RS485_ADDR_SN_HAEDWARE_VER` | `Sci_WrRegs_0x10_SN_Version()` |
| 0xFFF2 | `RS485_ADDR_SN_SOFTWARE_VER` | `Sci_WrRegs_0x10_SN_Version()` |
| 0xFFFD | `RS485_CMD_ADDR_FLASH_CONNECT` | `Sci_WrRegs_0x10_FlashConnect()` |

## 校验说明

- 上表地址来自 `Sci_Upper.h` 的 `enum RS485_CMD_RW_E`。
- `0x10` 写入大部分只看起始地址，因此上表以起始地址为主。
- `0x2400 ~ 0x2417` 为 AFE 参数块，已在主报告中单独说明。

