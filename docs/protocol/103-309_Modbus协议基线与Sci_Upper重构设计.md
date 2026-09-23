# 103-309 Modbus通信协议基线与 Sci_Upper 重构设计

- 基线分支：`refactor/a036-soc-framework-v2`
- 基线提交：`19b5d223b044cb9c168040a4814bbda6cc07fbc1`
- MCU：STM32F103C8
- AFE：SH367309
- 生产编译器：ARMCC 5.06 update 7
- 当前通信核心：`Sci_Upper.c`
- 目的：冻结现有线协议行为，为后续完全重构 `Sci_Upper` 提供兼容基线。
- 兼容原则：**地址不改**；为兼容旧上位机/APP，功能码、寄存器顺序、单位、大小端、magic value、特殊帧行为默认也不改。

> 本文档以当前源码“实际读写行为”为准；旧宏名/注释与实际字段冲突时，以真实结构体访问和实际发送内容为准。

## 1. 当前模块现状

当前 `Sci_Upper.c` 约 3800 行，同时承担 Modbus RTU、三路 UART 状态机、寄存器映射、EEPROM 副作用、5A A5 客户协议、DD 客户协议和部分 printf 重定向。最新 MAP 中 `sci_upper.o` ROM=15126 B，约占全固件 ROM 的 24.4%。

重构目标：

- 外部协议兼容；
- 阅读路径短；
- UART / Modbus / Register Map / Persistence / Legacy Protocol 分层；
- 参数校验统一；
- 消除 USART1/2/3 重复代码；
- 显著降低 Flash 占用。

## 2. Modbus RTU 基本协议

| 项目 | 当前行为 |
| --- | --- |
| Slave address | 0x01 |
| Broadcast address | 0x00 |
| 功能码 | 0x03 / 0x06 / 0x10 |
| 默认波特率 | 9600；EEPROM 可切换 19200 / 115200 |
| 串口格式 | 8N1 |
| 寄存器数据 | Big-endian，高字节先传 |
| CRC16 | Modbus RTU；CRC low byte 先传 |
| 0x03 成功响应 | [ADDR][03][BYTE_COUNT][DATA...][CRC_L][CRC_H] |
| 0x06/0x10 成功响应 | 回显请求头 6 Byte + CRC |
| 异常响应 | [ADDR][FUNC\|0x80][ERROR][CRC_L][CRC_H] |

| 错误码 | 含义 |
| --- | --- |
| 0x01 | 地址非法 |
| 0x02 | CRC错误 |
| 0x03 | 数据非法 |
| 0x04 | 命令非法 |
| 0x05 | 只读拒写 |
| 0x06 | 只写拒读 |
| 0x07 | 无权限 |
| 0x08 | 未知错误 |

## 3. 0x06 单寄存器命令区

| 地址 | 功能 | Value / 说明 |
| --- | --- | --- |
| 0x1000 | 恢复校准系数 | 0x55AA~0x55B0，见下表 |
| 0x1001 | 清除保护记录 | 0x0001 |
| 0x1002 | 软件保护参数恢复默认 | 0x0001 |
| 0x1003 | Other/System 参数恢复默认 | 0x0001 |
| 0x1004 | 加热/制冷参数恢复默认 | 0x0001 |
| 0x1005 | 单次设置 SOC | 0~100 |
| 0x1006 | SH367309 AFE 参数恢复默认 | 0x0001 |
| 0x1007 | 清除 Event Record | 0x0001 |
| 0x1008 | 修改 UART 波特率 | 1=9600, 2=19200, 3=115200 |
| 0x1100 | SWITCH ON | Value 1~32；当前代码仅校验，未执行实际动作 |
| 0x1101 | SWITCH OFF | Value 1~32；当前代码仅校验，未执行实际动作 |
| 0x1102 | 系统功能打开 | Value=bit+1 |
| 0x1103 | 系统功能关闭 | Value=bit+1 |

### 3.1 0x1000 校准恢复 magic

| Value | 功能 |
| --- | --- |
| 0x55AA | Cell1~Cell32 K/B 恢复默认 |
| 0x55AB | AFE1 Voltage 校准恢复 |
| 0x55AC | AFE2 Voltage 校准恢复 |
| 0x55AD | VBUS 校准恢复 |
| 0x55AE | 温度校准恢复 |
| 0x55AF | 放电电流校准恢复 |
| 0x55B0 | 充电电流校准恢复 |

### 3.2 0x1102 / 0x1103 系统功能位

| Value | 功能 |
| --- | --- |
| 1 | Balance |
| 2 | BMS Source |
| 3 | MOS / Relay |
| 4 | Relay Record |
| 5 | SOC Fixed |
| 6 | Heat |
| 7 | Cool |
| 8 | AFE1 |
| 9 | AFE2 |
| 10 | Sleep（ON 时立即进入 DEEP_MODE） |
| 11 | SOC Zero |
| 12~32 | Reserved / 当前仍允许置位 |

注意：`0x1100/0x1101` 当前只校验 Value 1~32，没有真正执行 Switch ON/OFF 动作，属于兼容占位行为。  
`UART_BAUD_115200` 历史常量值写成 11520，初始化时再映射为实际 115200；第一阶段不要改 EEPROM 兼容语义。

## 4. 0x2000~0x205D 校准参数

共 47 个 channel，每个 channel 两个寄存器：K/B。K 为 Q10 gain，倍率=K/1024，默认 1024，合法范围 512~1536。FC10 只能从 K 地址开始，quantity=2。

| K 地址 | B 地址 | Channel |
| --- | --- | --- |
| 0x2000 | 0x2001 | Cell1 |
| 0x2002 | 0x2003 | Cell2 |
| 0x2004 | 0x2005 | Cell3 |
| 0x2006 | 0x2007 | Cell4 |
| 0x2008 | 0x2009 | Cell5 |
| 0x200A | 0x200B | Cell6 |
| 0x200C | 0x200D | Cell7 |
| 0x200E | 0x200F | Cell8 |
| 0x2010 | 0x2011 | Cell9 |
| 0x2012 | 0x2013 | Cell10 |
| 0x2014 | 0x2015 | Cell11 |
| 0x2016 | 0x2017 | Cell12 |
| 0x2018 | 0x2019 | Cell13 |
| 0x201A | 0x201B | Cell14 |
| 0x201C | 0x201D | Cell15 |
| 0x201E | 0x201F | Cell16 |
| 0x2020 | 0x2021 | Cell17 |
| 0x2022 | 0x2023 | Cell18 |
| 0x2024 | 0x2025 | Cell19 |
| 0x2026 | 0x2027 | Cell20 |
| 0x2028 | 0x2029 | Cell21 |
| 0x202A | 0x202B | Cell22 |
| 0x202C | 0x202D | Cell23 |
| 0x202E | 0x202F | Cell24 |
| 0x2030 | 0x2031 | Cell25 |
| 0x2032 | 0x2033 | Cell26 |
| 0x2034 | 0x2035 | Cell27 |
| 0x2036 | 0x2037 | Cell28 |
| 0x2038 | 0x2039 | Cell29 |
| 0x203A | 0x203B | Cell30 |
| 0x203C | 0x203D | Cell31 |
| 0x203E | 0x203F | Cell32 |
| 0x2040 | 0x2041 | AFE1 Voltage |
| 0x2042 | 0x2043 | AFE2 Voltage |
| 0x2044 | 0x2045 | VBUS |
| 0x2046 | 0x2047 | Charge Current |
| 0x2048 | 0x2049 | Discharge Current |
| 0x204A | 0x204B | Temp1 |
| 0x204C | 0x204D | Temp2 |
| 0x204E | 0x204F | Temp3 |
| 0x2050 | 0x2051 | Temp4 |
| 0x2052 | 0x2053 | Temp5 |
| 0x2054 | 0x2055 | Temp6 |
| 0x2056 | 0x2057 | ENV Temp1 |
| 0x2058 | 0x2059 | ENV Temp2 |
| 0x205A | 0x205B | ENV Temp3 |
| 0x205C | 0x205D | MOS Temp |

兼容陷阱：B 的读取按 INT16 二补码原样输出；写入却按 bit15=sign、低15位=magnitude 解码。读写编码不完全对称，第一阶段重构必须保持当前 wire behavior。

## 5. 0x2100~0x2140 软件保护参数

13 组，每组固定 5 个寄存器：Level1 / Level2 / Level3 / Recover / Filter。FC10 只能从每组首地址开始，quantity=5。Filter 在保护逻辑中按 10 ms tick 使用。

| 地址 | 保护组 | 成员 |
| --- | --- | --- |
| 0x2100 | Cell OVP | Level1 |
| 0x2101 | Cell OVP | Level2 |
| 0x2102 | Cell OVP | Level3 |
| 0x2103 | Cell OVP | Recover |
| 0x2104 | Cell OVP | Filter |
| 0x2105 | Cell UVP | Level1 |
| 0x2106 | Cell UVP | Level2 |
| 0x2107 | Cell UVP | Level3 |
| 0x2108 | Cell UVP | Recover |
| 0x2109 | Cell UVP | Filter |
| 0x210A | Pack OVP | Level1 |
| 0x210B | Pack OVP | Level2 |
| 0x210C | Pack OVP | Level3 |
| 0x210D | Pack OVP | Recover |
| 0x210E | Pack OVP | Filter |
| 0x210F | Pack UVP | Level1 |
| 0x2110 | Pack UVP | Level2 |
| 0x2111 | Pack UVP | Level3 |
| 0x2112 | Pack UVP | Recover |
| 0x2113 | Pack UVP | Filter |
| 0x2114 | Charge OCP | Level1 |
| 0x2115 | Charge OCP | Level2 |
| 0x2116 | Charge OCP | Level3 |
| 0x2117 | Charge OCP | Recover |
| 0x2118 | Charge OCP | Filter |
| 0x2119 | Discharge OCP | Level1 |
| 0x211A | Discharge OCP | Level2 |
| 0x211B | Discharge OCP | Level3 |
| 0x211C | Discharge OCP | Recover |
| 0x211D | Discharge OCP | Filter |
| 0x211E | Charge OTP | Level1 |
| 0x211F | Charge OTP | Level2 |
| 0x2120 | Charge OTP | Level3 |
| 0x2121 | Charge OTP | Recover |
| 0x2122 | Charge OTP | Filter |
| 0x2123 | Charge UTP | Level1 |
| 0x2124 | Charge UTP | Level2 |
| 0x2125 | Charge UTP | Level3 |
| 0x2126 | Charge UTP | Recover |
| 0x2127 | Charge UTP | Filter |
| 0x2128 | Discharge OTP | Level1 |
| 0x2129 | Discharge OTP | Level2 |
| 0x212A | Discharge OTP | Level3 |
| 0x212B | Discharge OTP | Recover |
| 0x212C | Discharge OTP | Filter |
| 0x212D | Discharge UTP | Level1 |
| 0x212E | Discharge UTP | Level2 |
| 0x212F | Discharge UTP | Level3 |
| 0x2130 | Discharge UTP | Recover |
| 0x2131 | Discharge UTP | Filter |
| 0x2132 | MOS OTP | Level1 |
| 0x2133 | MOS OTP | Level2 |
| 0x2134 | MOS OTP | Level3 |
| 0x2135 | MOS OTP | Recover |
| 0x2136 | MOS OTP | Filter |
| 0x2137 | Cell Delta | Level1 |
| 0x2138 | Cell Delta | Level2 |
| 0x2139 | Cell Delta | Level3 |
| 0x213A | Cell Delta | Recover |
| 0x213B | Cell Delta | Filter |
| 0x213C | SOC Low | Level1 |
| 0x213D | SOC Low | Level2 |
| 0x213E | SOC Low | Level3 |
| 0x213F | SOC Low | Recover |
| 0x2140 | SOC Low | Filter |

单位：Cell voltage=mV；Pack voltage=V×100；Current=A×10；Temperature=(°C+40)×10；Delta V=mV；SOC=%；Filter=10 ms。

## 6. 0x2200~0x2255 SOC / 补偿 / RTC

### 6.1 OCV / SOC Table 0x2200~0x2229

21 点，每点 Voltage/SOC 两个寄存器。FC10 start=0x2200、quantity=42。Voltage=mV，SOC=%。当前写入路径已有整表合法性校验。

| Point | Voltage 地址 | SOC 地址 |
| --- | --- | --- |
| 1 | 0x2200 | 0x2201 |
| 2 | 0x2202 | 0x2203 |
| 3 | 0x2204 | 0x2205 |
| 4 | 0x2206 | 0x2207 |
| 5 | 0x2208 | 0x2209 |
| 6 | 0x220A | 0x220B |
| 7 | 0x220C | 0x220D |
| 8 | 0x220E | 0x220F |
| 9 | 0x2210 | 0x2211 |
| 10 | 0x2212 | 0x2213 |
| 11 | 0x2214 | 0x2215 |
| 12 | 0x2216 | 0x2217 |
| 13 | 0x2218 | 0x2219 |
| 14 | 0x221A | 0x221B |
| 15 | 0x221C | 0x221D |
| 16 | 0x221E | 0x221F |
| 17 | 0x2220 | 0x2221 |
| 18 | 0x2222 | 0x2223 |
| 19 | 0x2224 | 0x2225 |
| 20 | 0x2226 | 0x2227 |
| 21 | 0x2228 | 0x2229 |

### 6.2 Copper Loss 0x222A~0x2249

| 地址 | 字段 | 单位 / 说明 |
| --- | --- | --- |
| 0x222A | CopperLoss[0] | µΩ |
| 0x222B | CopperLoss[1] | µΩ |
| 0x222C | CopperLoss[2] | µΩ |
| 0x222D | CopperLoss[3] | µΩ |
| 0x222E | CopperLoss[4] | µΩ |
| 0x222F | CopperLoss[5] | µΩ |
| 0x2230 | CopperLoss[6] | µΩ |
| 0x2231 | CopperLoss[7] | µΩ |
| 0x2232 | CopperLoss[8] | µΩ |
| 0x2233 | CopperLoss[9] | µΩ |
| 0x2234 | CopperLoss[10] | µΩ |
| 0x2235 | CopperLoss[11] | µΩ |
| 0x2236 | CopperLoss[12] | µΩ |
| 0x2237 | CopperLoss[13] | µΩ |
| 0x2238 | CopperLoss[14] | µΩ |
| 0x2239 | CopperLoss[15] | µΩ |
| 0x223A | CopperLoss_Num[0] | 0=结束/无配置；1~32=Cell index |
| 0x223B | CopperLoss_Num[1] | 0=结束/无配置；1~32=Cell index |
| 0x223C | CopperLoss_Num[2] | 0=结束/无配置；1~32=Cell index |
| 0x223D | CopperLoss_Num[3] | 0=结束/无配置；1~32=Cell index |
| 0x223E | CopperLoss_Num[4] | 0=结束/无配置；1~32=Cell index |
| 0x223F | CopperLoss_Num[5] | 0=结束/无配置；1~32=Cell index |
| 0x2240 | CopperLoss_Num[6] | 0=结束/无配置；1~32=Cell index |
| 0x2241 | CopperLoss_Num[7] | 0=结束/无配置；1~32=Cell index |
| 0x2242 | CopperLoss_Num[8] | 0=结束/无配置；1~32=Cell index |
| 0x2243 | CopperLoss_Num[9] | 0=结束/无配置；1~32=Cell index |
| 0x2244 | CopperLoss_Num[10] | 0=结束/无配置；1~32=Cell index |
| 0x2245 | CopperLoss_Num[11] | 0=结束/无配置；1~32=Cell index |
| 0x2246 | CopperLoss_Num[12] | 0=结束/无配置；1~32=Cell index |
| 0x2247 | CopperLoss_Num[13] | 0=结束/无配置；1~32=Cell index |
| 0x2248 | CopperLoss_Num[14] | 0=结束/无配置；1~32=Cell index |
| 0x2249 | CopperLoss_Num[15] | 0=结束/无配置；1~32=Cell index |

FC10 start=0x222A、quantity=32，一次写 16 个 resistance + 16 个 cell number。

### 6.3 RTC 0x224A~0x2255

| 地址 | 字段 |
| --- | --- |
| 0x224A | Time Year |
| 0x224B | Time Month |
| 0x224C | Time Day |
| 0x224D | Time Hour |
| 0x224E | Time Minute |
| 0x224F | Time Second |
| 0x2250 | Alarm Year |
| 0x2251 | Alarm Month |
| 0x2252 | Alarm Day |
| 0x2253 | Alarm Hour |
| 0x2254 | Alarm Minute |
| 0x2255 | Alarm Second |

FC10 start=0x224A、quantity=12。

## 7. 0x2300~0x2337 系统配置

这一段是当前维护风险最高的区域：`Sci_Upper.h` 中多处 enum 名称已和真实结构体字段不一致。以下映射以实际指针访问的结构体字段顺序为准。

### 7.1 OTHER_ELEMENT 0x2300~0x231F

| 地址 | 真实字段 | 单位 / 说明 |
| --- | --- | --- |
| 0x2300 | Balance_OpenVoltage | mV |
| 0x2301 | Balance_OpenWindow | mV |
| 0x2302 | Balance_CloseWindow | mV |
| 0x2303 | Balance_Res1 | Reserved |
| 0x2304 | Balance_Res2 | Reserved |
| 0x2305 | Balance_Res3 | Reserved |
| 0x2306 | Balance_Res4 | Reserved |
| 0x2307 | Balance_Res5 | Reserved |
| 0x2308 | CS_Cur_CHGmax | A×10 |
| 0x2309 | CS_Cur_DSGmax | A×10 |
| 0x230A | CBC_DelayT | µs×10 |
| 0x230B | CBC_Cur_DSG | A×10 |
| 0x230C | Soc_TableSelect | SOC table selector |
| 0x230D | Password_Always | Legacy / 基本未使用 |
| 0x230E | CurLimit_Vdelta | mV |
| 0x230F | CurLimit_Cur | A×10 |
| 0x2310 | Sleep_VNormal | mV |
| 0x2311 | Sleep_TimeNormal | min |
| 0x2312 | Sleep_Vlow | mV |
| 0x2313 | Sleep_TimeVlow | min |
| 0x2314 | Sleep_VirCur_Chg | A×10 |
| 0x2315 | Sleep_VirCur_Dsg | A×10 |
| 0x2316 | Sleep_RTC_WakeUpTime | min |
| 0x2317 | Sleep_TimeRTC | min |
| 0x2318 | Soc_Ah | Ah×10 |
| 0x2319 | Soc_Cycle_times | cycle |
| 0x231A | Soc_V_100 | mV / legacy |
| 0x231B | Soc_V_0 | mV / legacy |
| 0x231C | Sys_SeriesNum | cells |
| 0x231D | Sys_CS_Res | current shunt parameter |
| 0x231E | Sys_CS_Res_Num | scaling numerator |
| 0x231F | Sys_PreChg_Time | s |

当前合法 FC10 block：

| Start | Quantity | 实际字段区 |
| --- | --- | --- |
| 0x2300 | 8 | Balance 8 regs |
| 0x2308 | 8 | Current/CBC/SOC table select/current limit 8 regs |
| 0x2310 | 8 | Sleep 8 regs |
| 0x2318 | 4 | SOC 4 regs |
| 0x231C | 4 | System 4 regs |

### 7.2 HEAT_COOL_ELEMENT 0x2320~0x2337

| 地址 | 实际字段 |
| --- | --- |
| 0x2320 | Heat_OpenTemp |
| 0x2321 | Heat_CloseTemp |
| 0x2322 | Heat_OpenCur |
| 0x2323 | Cool_OpenTemp |
| 0x2324 | Cool_CloseTemp |
| 0x2325 | Heat_Res3 |
| 0x2326 | Heat_Res4 |
| 0x2327 | Heat_Res5 |
| 0x2328 | Heat_Res6 |
| 0x2329 | Heat_Res7 |
| 0x232A | Heat_Res8 |
| 0x232B | Heat_Res9 |
| 0x232C | Heat_Res10 |
| 0x232D | Cool_Res1 |
| 0x232E | Cool_Res2 |
| 0x232F | Cool_Res3 |
| 0x2330 | Cool_Res4 |
| 0x2331 | Cool_Res5 |
| 0x2332 | Cool_Res6 |
| 0x2333 | Cool_Res7 |
| 0x2334 | Cool_Res8 |
| 0x2335 | Cool_Res9 |
| 0x2336 | Cool_Res10 |
| 0x2337 | Cool_Res11 |

FC10 start=0x2320、quantity=24。温度字段使用 (°C+40)×10；Heat_OpenCur 使用 A×10。旧 header 中 HEAT_DSG_HIGH / COOL_DSG_H 等名字已经和实际字段含义发生漂移，后续只保留为兼容 alias。

## 8. 0x2400~0x2417 SH367309 AFE 参数

当前协议暴露每个 `AFE_Value_Typedef.curValue`。FC03 可读；FC10 可从 0x2400~0x2417 任意合法地址写连续子区间，只要不越过 0x2417。

| 地址 | 参数 | 单位 |
| --- | --- | --- |
| 0x2400 | Cell OVP | mV |
| 0x2401 | Cell OVP Recover | mV |
| 0x2402 | Cell OVP Filter | 10 ms |
| 0x2403 | Cell UVP | mV |
| 0x2404 | Cell UVP Recover | mV |
| 0x2405 | Cell UVP Filter | 10 ms |
| 0x2406 | Charge OCP1 | A×10 |
| 0x2407 | Charge OCP1 Filter | delay |
| 0x2408 | Charge OCP2 | A×10 |
| 0x2409 | Charge OCP2 Filter | delay |
| 0x240A | Discharge OCP1 | A×10 |
| 0x240B | Discharge OCP1 Filter | delay |
| 0x240C | Discharge OCP2 | A×10 |
| 0x240D | Discharge OCP2 Filter | delay |
| 0x240E | Charge OTP | (T+40)×10 |
| 0x240F | Charge OTP Recover | (T+40)×10 |
| 0x2410 | Charge UTP | (T+40)×10 |
| 0x2411 | Charge UTP Recover | (T+40)×10 |
| 0x2412 | Discharge OTP | (T+40)×10 |
| 0x2413 | Discharge OTP Recover | (T+40)×10 |
| 0x2414 | Discharge UTP | (T+40)×10 |
| 0x2415 | Discharge UTP Recover | (T+40)×10 |
| 0x2416 | CBC_Cur_DSG | A×10 / legacy |
| 0x2417 | CBC_DelayT | legacy delay |

当前风险：写入路径未统一在协议层使用结构体 minValue/maxValue 完整验证；启动读取 EEPROM 时才有范围校验。新架构应改成 decode → validate → apply → persist。

## 9. 0xC000 特殊数据页

这些地址不是普通线性参数区，更接近 legacy page command。

| 地址 | 功能 | 当前内容 |
| --- | --- | --- |
| 0xC000 | LCD 简化页 | 5 registers：固定1、Pack Voltage、legacy current、MaxTemp、SOC |
| 0xC001 | 第三级保护历史 | 10条记录，每条 FaultCode + 年/月/日/时/分/秒，共70 registers |
| 0xC002 | 产品信息 | Serial 32B + Hardware 32B + Software 32B，共96B |
| 0xC003 | Reserved | 未实现 |
| 0xC004 | Battery Number | sysinfo.BatSnum，当前11B |
| 0xC005~0xC007 | Reserved | 未实现 |
| 0xC008 | Event Record | 100条事件，每条2B：event code + time code |

### 9.1 Event Record 事件编号

| Code | Event |
| --- | --- |
| 0 | Null |
| 1 | BMS Start Up |
| 2 | BMS Sleep |
| 3 | Balance Open |
| 4 | Heat Open |
| 5 | Cool Open |
| 6 | Cell OVP |
| 7 | Pack OVP |
| 8 | Charge OCP |
| 9 | Cell UVP |
| 10 | Pack UVP |
| 11 | Discharge OCP |
| 12 | Charge UTP |
| 13 | Discharge UTP |
| 14 | Charge OTP |
| 15 | Discharge OTP |
| 16 | Cell Delta Over |
| 17 | CBC Error |
| 18 | AFE1 Error |
| 19 | AFE2 Error |
| 20 | EEPROM Error |

当前 `LogTime_Map()` 有运算符优先级问题：源码注释希望 1min 内/7day 内按小时/>7day 分档，但当前 >60s 且 <=7day 的结果基本会变成 1。属于兼容敏感 bug，不在架构迁移中暗改。

## 10. 0xD000 实时 BMS 数据

| 地址 | 字段 | 单位 / 说明 |
| --- | --- | --- |
| 0xD000 | Cell1 Voltage | mV |
| 0xD001 | Cell2 Voltage | mV |
| 0xD002 | Cell3 Voltage | mV |
| 0xD003 | Cell4 Voltage | mV |
| 0xD004 | Cell5 Voltage | mV |
| 0xD005 | Cell6 Voltage | mV |
| 0xD006 | Cell7 Voltage | mV |
| 0xD007 | Cell8 Voltage | mV |
| 0xD008 | Cell9 Voltage | mV |
| 0xD009 | Cell10 Voltage | mV |
| 0xD00A | Cell11 Voltage | mV |
| 0xD00B | Cell12 Voltage | mV |
| 0xD00C | Cell13 Voltage | mV |
| 0xD00D | Cell14 Voltage | mV |
| 0xD00E | Cell15 Voltage | mV |
| 0xD00F | Cell16 Voltage | mV |
| 0xD010 | Cell17 Voltage | mV |
| 0xD011 | Cell18 Voltage | mV |
| 0xD012 | Cell19 Voltage | mV |
| 0xD013 | Cell20 Voltage | mV |
| 0xD014 | Cell21 Voltage | mV |
| 0xD015 | Cell22 Voltage | mV |
| 0xD016 | Cell23 Voltage | mV |
| 0xD017 | Cell24 Voltage | mV |
| 0xD018 | Cell25 Voltage | mV |
| 0xD019 | Cell26 Voltage | mV |
| 0xD01A | Cell27 Voltage | mV |
| 0xD01B | Cell28 Voltage | mV |
| 0xD01C | Cell29 Voltage | mV |
| 0xD01D | Cell30 Voltage | mV |
| 0xD01E | Cell31 Voltage | mV |
| 0xD01F | Cell32 Voltage | mV |
| 0xD020 | VCellMax | mV |
| 0xD021 | VCellMin | mV |
| 0xD022 | VCellMaxPosition | 1-based |
| 0xD023 | VCellMinPosition | 1-based |
| 0xD024 | VCellDelta | mV |
| 0xD025 | Pack Voltage | V×100 |
| 0xD026 | AFE1 TEMP1 | (T+40)×10 |
| 0xD027 | AFE1 TEMP2 | (T+40)×10 |
| 0xD028 | AFE1 TEMP3 | (T+40)×10 |
| 0xD029 | AFE2 TEMP1 | (T+40)×10 |
| 0xD02A | AFE2 TEMP2 | (T+40)×10 |
| 0xD02B | AFE2 TEMP3 | (T+40)×10 |
| 0xD02C | ENV TEMP1 | (T+40)×10 |
| 0xD02D | ENV TEMP2 | (T+40)×10 |
| 0xD02E | ENV TEMP3 | (T+40)×10 |
| 0xD02F | MOS TEMP1 | (T+40)×10 |
| 0xD030 | TempMax | (T+40)×10 |
| 0xD031 | TempMin | (T+40)×10 |
| 0xD032 | Charge Current | A×10 |
| 0xD033 | Discharge Current | A×10 |
| 0xD034 | SOC | % |
| 0xD035 | SOH | % / compatibility field |
| 0xD036 | CapacityNow | Ah×100 |
| 0xD037 | CapacityFull | Ah×100 |
| 0xD038 | CapacityFactory | Ah×100 |
| 0xD039 | Cycle Count | count |
| 0xD03A | First level fault | bitfield |
| 0xD03B | Second level fault | bitfield |
| 0xD03C | Third level fault | bitfield |
| 0xD03D | BalanceFlag1 | bitfield |
| 0xD03E | BalanceFlag2 | bitfield |

### 10.1 D03A~D03C Fault bit

| Bit | 含义 |
| --- | --- |
| 0 | Cell OVP |
| 1 | Cell UVP |
| 2 | Pack OVP |
| 3 | Pack UVP |
| 4 | Charge OCP |
| 5 | Discharge OCP |
| 6 | Charge OTP |
| 7 | Discharge OTP |
| 8 | Charge UTP |
| 9 | Discharge UTP |
| 10 | Cell Delta |
| 11 | Temperature Delta |
| 12 | SOC Low |
| 13 | MOS OTP |
| 14 | Reserved |
| 15 | Reserved |

## 11. 0xD100 扩展实时状态

| 地址 | 含义 |
| --- | --- |
| 0xD100 | __LIANXING_VERSION__ |
| 0xD101 | Charge remaining time |
| 0xD102 | Discharge remaining time |
| 0xD103 | 最新 First fault #1/#2 |
| 0xD104 | 最新 First fault #3/#4 |
| 0xD105 | 最新 Second fault #1/#2 |
| 0xD106 | 最新 Second fault #3/#4 |
| 0xD107 | 最新 Third fault #1/#2 |
| 0xD108 | 最新 Third fault #3/#4 |
| 0xD109 | Error AFE1 / AFE2 |
| 0xD10A | Error CAN / EEPROM communication |
| 0xD10B | Error SPI / Upper |
| 0xD10C | Error Client / Screen |
| 0xD10D | Error WiFi / Bluetooth |
| 0xD10E | Error APP / CBC Charge |
| 0xD10F | EEPROM store / HSE |
| 0xD110 | LSE / Vdelta |
| 0xD111 | Balance / ADC |
| 0xD112 | Heat / Cool |
| 0xD113 | CBC Discharge / SOC calibration |
| 0xD114 | TempBreak / Reserved |
| 0xD115 | SystemStatus low16 |
| 0xD116 | SystemStatus high16 |
| 0xD117 | System Function enable low16 |
| 0xD118 | System Function enable high16 |
| 0xD119 | Reserved 0 |
| 0xD11A | Reserved 0 |
| 0xD11B | Heat/Cool Fault Flag |
| 0xD11C | Reserved 0 |
| 0xD11D | Reserved 0 |
| 0xD11E | Reserved 0 |
| 0xD11F | Reserved 0 |
| 0xD120 | Reserved 0 |

D101/D102 单位 minute；ETA 无效时为 0xFFFF。0xD200 当前固定返回 0，作为预留地址。

### 11.1 SystemStatus bit（D115/D116）

| Bit | 含义 |
| --- | --- |
| 0 | StartUpBMS |
| 1 | MOS_PRE |
| 2 | MOS_CHG |
| 3 | MOS_DSG |
| 4 | Relay_PRE |
| 5 | Relay_CHG |
| 6 | Relay_DSG |
| 7 | Relay_MAIN |
| 8 | Heat |
| 9 | Cool |
| 10 | AFE1 |
| 11 | AFE2 |
| 12 | Balance |
| 13 | ToSleep |
| 14 | Button Close IO |
| 15 | Heat Close IO |
| 16 | SysLimits |
| 17 | CBC Close IO |
| 18 | Driver External Control |
| 19 | Reserved |
| 20 | Project Version bit0 |
| 21 | Project Version bit1 |
| 22 | Project Version bit2 |
| 23 | Project Version bit3 |
| 24 | Reserved |
| 25 | Reserved |
| 26 | Reserved |
| 27 | Reserved |
| 28 | Reserved |
| 29 | Reserved |
| 30 | Reserved |
| 31 | Reserved |

当前 OPEN==0 时 D115 对部分 bit 有历史极性转换，这也是 wire compatibility 的一部分。

### 11.2 System Function Enable bit（D117/D118）

| Bit | 功能 |
| --- | --- |
| 0 | Balance |
| 1 | BMS Source |
| 2 | MOS / Relay |
| 3 | Relay Record |
| 4 | SOC Fixed |
| 5 | Heat |
| 6 | Cool |
| 7 | AFE1 |
| 8 | AFE2 |
| 9 | Sleep |
| 10 | SOC Zero |

### 11.3 Heat/Cool Fault bit（D11B）

| Bit | 含义 |
| --- | --- |
| 0 | Heat Open Circuit |
| 1 | Heat On-Time Max |
| 2 | Heat Short Circuit |
| 3 | Heat Normal/Fault state |
| 4~7 | Reserved |
| 8 | Cool Open Circuit |
| 9 | Cool On-Time Max |
| 10 | Cool Short Circuit |
| 11 | Cool Normal/Fault state |
| 12~15 | Reserved |

## 12. FFFx 产品信息与升级命令

| 地址 | 方向 | 功能 |
| --- | --- | --- |
| 0xFFF0 | 0x10 Write | Serial Number，ASCII/raw，内部最大32B |
| 0xFFF1 | 0x10 Write | Hardware Version |
| 0xFFF2 | 0x10 Write | Software Version |
| 0xFFF3 | 0x10 Write | Battery Number，quantity=6，实际保存11B |
| 0xFFFD | 0x10 Write | 设置 FLASH_TO_IAP_VALUE，发送完成后进入升级流程 |

FFFx 当前应视为写地址。读产品信息使用 C002，Battery Number 使用 C004。对 FFFx 发 0x03 可能进入错误的 D 区 offset，属于当前潜在越界风险。

## 13. USART2 私有 5A A5 协议

USART2 除 Modbus 外还解析 `5A A5 ... F0`。请求通常为 `5A A5 CMD XX XX BCC F0`，BCC 对 bytes[2..4] 计算。Payload 的 16-bit 字段多为 little-endian，与 Modbus 不同。

| CMD | 当前响应内容摘要 |
| --- | --- |
| 0x03 | Full Capacity、Charge ETA、Discharge ETA、5个 Reserved 16-bit、BCC、F0 |
| 0x04 | Cell1~7、MaxTemp、Balance、PackVoltage、Current、SOC、Level3 fault、部分保护阈值、容量与恢复阈值 |
| 0x05 | AA EB 0F ... FA：Cycle、2B software version、11B battery number、BCC |
| 0x06 | Series、Cell、MOS temp、Balance、PackVoltage、Current、SOC、Fault、保护阈值、容量、计数器、Cycle/Version/Battery number |

Current 使用 10000 作为 0A offset。该协议应在重构后独立到 legacy protocol 模块，不应继续和 Modbus server 混写。

## 14. USART2 私有 DD...77 协议

| 请求 / 命令 | 行为 |
| --- | --- |
| DD A5 AA 00 FF 56 77 | 读取11个保护计数器；响应 DD AA 00 16 + 22B data + checksum + 77 |
| DD 5A 01 02 28 28 FF AD 77 | 清除保护计数器；响应 DD 01 00 00 00 77 |
| DD 5A E1 02 00 xx checksumH checksumL 77 | MOS Remote Control |

MOS Remote Control 注释意图：00释放强制关闭；01关CHG；02关DSG；03同时关CHG+DSG。但当前实际代码与注释不完全一致，尤其 xx=03 只把 CHG 设为 KEEP。属于兼容敏感 bug，不能在架构重构时暗改。

## 15. 已确认的架构 / 协议风险

- 0x2300~0x2337 的旧 enum 名称与真实字段已经漂移。
- 一个 `Sci_Upper.c` 同时承担 UART / Modbus / Register Map / EEPROM / 两套客户协议 / printf，职责过载。
- USART1/2/3 RX、TX、App 状态机大量复制，直接增加 Flash。
- 校准和保护分发使用巨大 switch，实际上可以按 range/index 算术分发。
- 0x03 当前先构造整页到 `g_u8SCITxBuff[251]`，再 slice/copy，浪费 RAM、Flash 和边界复杂度。
- 通过结构体指针连续访问实现 wire map，使 C struct layout 隐式成为协议 ABI。
- 多处 address / quantity / byte-count 边界校验不足。
- AFE / Protection / Other / HeatCool / RTC 的 validation 策略不统一。
- Modbus ACK success 目前更多代表“RAM 已接受 / dirty flag 已置位”，不等于 EEPROM 已持久化成功。
- Broadcast 0x00 当前可能回复，与标准 Modbus 行为不一致；需兼容性评估后再改。
- Calibration B 读写编码不对称。
- Event time code 存在表达式优先级 bug。
- DD MOS xx=0x03 的实际行为和注释意图不一致。
- FFFx FC03 存在错误分发/越界风险。
- `u16Buffer` 实际为 UINT8[]，还有 PORTECT / HAEDWARE 等历史命名，维护成本高。

## 16. 推荐重构架构

建议控制在 5~6 个核心模块，避免在 64KB MCU 上为了“抽象”引入过多函数指针和 descriptor table：

```text
communication/
├── bms_comm.c/.h          # 对外 Init/App
├── uart_transport.c/.h    # USART1/2/3 共用状态机 + STM32 StdPeriph 硬件适配
├── modbus_rtu.c/.h        # RTU frame / CRC / exception / FC03/06/10
├── modbus_map.c/.h        # canonical address map + read/write dispatch
├── modbus_command.c       # 0x1000 / 0x1100 命令语义
└── legacy_protocol.c      # 5A A5 + DD 协议
```

### 16.1 Canonical Register Map

新代码只存在一个协议地址真源，例如：

- MB_REG_CALIB_BASE = 0x2000
- MB_REG_PROTECT_BASE = 0x2100
- MB_REG_SOC_TABLE_BASE = 0x2200
- MB_REG_SYSTEM_BASE = 0x2300
- MB_REG_AFE_BASE = 0x2400
- MB_REG_SPECIAL_BASE = 0xC000
- MB_REG_RUNTIME_BASE = 0xD000

旧 `RS485_CMD_ADDR_xxx` 宏短期作为 alias 保留，保证旧代码、APP 文档和工具不被一次性打断。

### 16.2 Range-based dispatch

不建议创建数百项 function-pointer descriptor table。推荐使用连续区间算术：

- calibration：`channel=(addr-0x2000)>>1`
- protection：`group=(addr-0x2100)/5`、`member=(addr-0x2100)%5`
- AFE：`index=addr-0x2400`

这样更省 Flash，也更容易审核地址连续性。

### 16.3 UART Transport 共用

USART1/2/3 只保留：

- USART instance；
- GPIO/Remap；
- baud；
- enabled protocols。

RX/TX/state machine 只实现一次。USART2 配置 MODBUS | LEGACY_5AA5 | LEGACY_DD；USART1/3 只启用 MODBUS。

### 16.4 FC03 直接读取

新流程：

```text
validate frame
→ validate start/count
→ for each requested register
→ ModbusMap_Read(addr)
→ encode_be16()
→ CRC
→ TX
```

删除“构造整页 → g_u8SCITxBuff → slice/copy”。

### 16.5 FC06 / FC10 Transaction

统一：

```text
decode
→ validate range
→ validate quantity
→ validate all values
→ apply
→ mark persistence
→ side effect
→ ACK
```

必须先全部校验通过，再修改 RAM，避免半写。

### 16.6 Persistence 解耦

Modbus 层不再直接依赖 `u32E2P_...WriteFlag`。通过参数服务接口表达：

- 参数是什么；
- 是否 dirty；
- 是否需要 side effect。

EEPROM / Internal Flash / EasyFlash / NVM3 应由 persistence 层决定。

### 16.7 Legacy Protocol 隔离

USART2 transport 根据首字节/帧头路由：

- 0x00 / 0x01 → Modbus
- 0x5A → Legacy 5A A5
- 0xDD → Legacy DD

三套 parser 不再混在一个 RX 函数里。

## 17. Flash 优化优先级

| 优先级 | 动作 | 预期价值 |
| --- | --- | --- |
| P1 | 合并 USART1/2/3 状态机 | 最大潜在 ROM 收益，消除整段重复逻辑 |
| P1 | 校准/保护 range arithmetic 替代巨大 switch | 降低 Code，地址关系更清晰 |
| P1 | 删除整页临时 g_u8SCITxBuff 和二次 copy | 降低 RAM + copy code |
| P1 | 统一 BE16 decode/encode/CRC/异常帧 | 减少重复实现 |
| P2 | 私有协议做 compile-time feature switch | 具体项目不用时可被 linker 真正裁掉 |
| P2 | 清理历史 no-op、重复注释和死代码 | 改善可读性并释放少量 Flash |

不建议第一阶段使用“每个寄存器一个 callback descriptor”方案；它会增加 RO data、函数指针和 indirect call，不适合当前 64KB Flash。

## 18. 重构验收标准

- 地址 100% 不变。
- 功能码、寄存器顺序、单位、大小端、magic value、旧客户固定帧保持兼容。
- ARMCC5：0 Error。
- GNU source compile gate：PASS。
- Warning 不增加。
- 每个行为变更单独 commit。
- 每个 commit 自动跑 Windows Keil + MAP。
- 不能只看新的 `sci_upper.o`，必须看整个 communication aggregate ROM 与全固件 Total ROM。
- 第一阶段目标：通信相关净减至少约 4 KB；全固件 ROM 尽量降到 <=58 KB，并重新留下约 6~8 KB Flash 余量；最终以 ARMCC MAP 实测为准。

## 19. 重构前必须建立的 Protocol Regression

建议建立 `tools/protocol/modbus_regression.py` 或等价工具，覆盖：

- FC03：0x2000~0x205D、0x2100~0x2140、0x2200~0x2255、0x2300~0x2337、0x2400~0x2417、C000/C001/C002/C004/C008、D000~D03E、D100~D120、D200。
- FC06：0x1000~0x1008、0x1100~0x1103。
- FC10：所有合法 block。
- CRC、byte count、endian。
- address 越界。
- quantity=0 / quantity 超界。
- byte count 与 quantity 不匹配。
- calibration 奇数 B 地址作为 start。
- protection 非 group-start 写。
- FFFx FC03。
- AFE invalid parameter。
- 5A A5 固定请求/响应。
- DD 固定请求/响应。
- 重构前后对相同 input frame 做 byte-for-byte response comparison。

## 20. 推荐实施顺序

| Phase | 内容 |
| --- | --- |
| 0 | 冻结协议：提交本文档 + 地址静态检查 + 协议回归基线，不改运行行为 |
| 1 | Canonical Address Map：准确命名；旧宏 alias |
| 2 | 抽 Modbus Core：CRC / frame / exception / FC03/06/10 |
| 3 | 合并 3 路 UART transport |
| 4 | 重写 FC03 direct map read，删除 g_u8SCITxBuff |
| 5 | 重写 FC10 range/group dispatch |
| 6 | 重写 FC06 command dispatch |
| 7 | 抽离 5A A5 legacy protocol |
| 8 | 抽离 DD legacy protocol |
| 9 | 统一 bounds / validation / safety，兼容敏感项逐个决定 |
| 10 | 根据 MAP 做最后一轮 Flash 优化 |

## 21. 最终原则

`Sci_Upper` 已不适合继续局部打补丁。正确路径是：

```text
冻结协议 ABI
→ 建立 regression
→ 建 canonical map
→ 抽 Modbus core
→ 合并 UART
→ 重写 read/write
→ 隔离 legacy
→ 统一 validation/persistence
→ 每步 Keil + MAP 验证
```

**外部协议保持兼容，内部结构允许完全重构。**
