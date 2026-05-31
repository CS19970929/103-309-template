# Keil 构建基准记录

记录日期：2026-05-31

本文件记录当前 Keil 工程的构建基准，用于后续 CMake + Ninja + arm-none-eabi-gcc 并行构建迁移对照。当前阶段只扫描和记录，不修改业务代码、不修改 Keil 工程。

## 扫描范围

- 工作区：`D:\code\103-309-template-cmake-gcc`
- 原始工作区参考：`D:\code\103-309-template`
- Keil 工程：`103 + 309/Project/Users/CommomSH367309_16series_103RCT6_C.uvprojx`
- 主业务目录：`103 + 309/Project/Source`
- StdPeriph/CMSIS 目录：`103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0`
- 当前可提交分支中未包含原工作区未跟踪目录 `C030v1.0/`，本次基线不把该目录混入迁移提交。

## Keil 目标

### FD_Release

- 目标名：`FD_Release`
- 芯片：`STM32F103C8`
- CPU：`Cortex-M3`
- Keil CPU/存储描述：`IRAM(0x20000000,0x00005000) IROM(0x08000000,0x00010000) CPUTYPE("Cortex-M3") CLOCK(12000000) ELITTLE`
- 输出目录：`103 + 309/Project/Users/Objects`
- 输出名：`FD_Release`
- Keil 工程内 `CreateHexFile`：`0`
- After Build：`fromelf.exe --bin -o "$L@L.bin" "#L"`
- C 编译优化：`Optim=1`，`oTime=0`
- Keil 工程内 scatter 字段为空；实际 Keil 构建在 `Objects/FD_Release.sct` 生成 scatter 文件。

### FD_Debug

- 目标名：`FD_Debug`
- 芯片：`STM32F103C8`
- CPU：`Cortex-M3`
- 输出目录：`103 + 309/Project/Users/Objects`
- 输出名：`FD_Debug`
- C 编译优化：`Optim=1`，`oTime=0`
- 目标额外宏：`PROJECT_CFG_BUILD_PROFILE=1`、`PROJECT_CFG_DEBUG_WATCH_ENABLE=1`、`_DEBUG_`

## 工具链基准

来自原工作区 `Objects/FD_Release.build_log.htm`：

- Toolchain Path：`C:\Keil_v5\ARM\ARMCC\Bin`
- C Compiler：`Armcc.exe V5.06 update 7 (build 960)`
- Assembler：`Armasm.exe V5.06 update 7 (build 960)`
- Linker/Locator：`ArmLink.exe V5.06 update 7 (build 960)`
- Hex Converter：`FromElf.exe V5.06 update 7 (build 960)`

## 预定义宏

### Release

- `STM32F10X_MD`
- `USE_STDPERIPH_DRIVER`

### Debug

- `STM32F10X_MD`
- `USE_STDPERIPH_DRIVER`
- `PROJECT_CFG_BUILD_PROFILE=1`
- `PROJECT_CFG_DEBUG_WATCH_ENABLE=1`
- `_DEBUG_`

量产隔离规则：量产程序必须保持 `PROJECT_CFG_BUILD_PROFILE 0`。SOC 测试固件才允许使用 `PROJECT_CFG_BUILD_PROFILE 2`、`PROJECT_CFG_SOC_TEST_MODE_ENABLE 1`、`PROJECT_CFG_SOC_TEST_ACCEL_TICKS_MAX 300`。

## Include 路径

相对 Keil 工程目录 `103 + 309/Project/Users`：

- `../Lib`
- `../STM32F10x_StdPeriph_Lib_V3.5.0/drivers`
- `../STM32F10x_StdPeriph_Lib_V3.5.0/inc`
- `../Source`
- `../Source/conf`
- `../Source/easylogger/inc`

## Startup 文件

- Keil startup：`103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/startup_stm32f10x_hd.s`
- 类型：ARMASM/MDK-ARM 语法
- 设备族注释：STM32F10x High Density Devices
- 备注：Keil 目标设备标识是 `STM32F103C8`，宏使用 `STM32F10X_MD`，但工程当前引用 high-density startup。GCC 并行构建需要保留旧 startup，并新增 GCC 语法 startup 文件，避免覆盖 Keil 文件。

## Scatter / 内存布局

原工作区 Keil 构建生成的 `103 + 309/Project/Users/Objects/FD_Release.sct` 内容要点：

- App 起始地址：`0x08004800`
- App Flash 长度：`0x00020000`
- RAM 起始地址：`0x20000000`
- RAM 长度：`0x00005000`
- Reset 段：`*.o (RESET, +First)`
- RO/XO：`.ANY (+RO)`、`.ANY (+XO)`
- RW/ZI：`.ANY (+RW +ZI)`

安全规则：禁止把 `FD_Debug.bin` 或 `FD_Release.bin` 裸写到 `0x08000000`，App 烧录必须使用 `0x08004800`。新增烧录脚本必须保留 `0x08004800` 地址检查和 dry-run 输出。

## 源文件列表

两个 Keil 目标当前源文件列表相同：C 文件 64 个，汇编文件 1 个，头文件记录 32 个。

### 用户源文件

- `103 + 309/Project/Source/ADC.c`
- `103 + 309/Project/Source/DataDeal.c`
- `103 + 309/Project/Source/EEPROM.c`
- `103 + 309/Project/Source/Fault.c`
- `103 + 309/Project/Source/Flash.c`
- `103 + 309/Project/Source/Flash64KAppTest.c`
- `103 + 309/Project/Source/FactoryAging.c`
- `103 + 309/Project/Source/LowPowerSleep.c`
- `103 + 309/Project/Source/Heat_Cool.c`
- `103 + 309/Project/Source/I2C_AFE1.c`
- `103 + 309/Project/Source/main.c`
- `103 + 309/Project/Source/PubFunc.c`
- `103 + 309/Project/Source/RTC.c`
- `103 + 309/Project/Source/Runtime.c`
- `103 + 309/Project/Source/Sci_Upper.c`
- `103 + 309/Project/Source/SleepDeal.c`
- `103 + 309/Project/Source/SOC.c`
- `103 + 309/Project/Source/System_Init.c`
- `103 + 309/Project/Source/System_Monitor.c`
- `103 + 309/Project/Source/ProductionID.c`
- `103 + 309/Project/Source/Can_HDX.c`
- `103 + 309/Project/Source/CanFeidaoFrames.c`
- `103 + 309/Project/Source/ChargerLoadFunc.c`
- `103 + 309/Project/Source/LogRecord.c`
- `103 + 309/Project/Source/LedBar.c`
- `103 + 309/Project/Source/ShortFunc.c`
- `103 + 309/Project/Source/IO_Control.c`
- `103 + 309/Project/Source/IODrivers.c`
- `103 + 309/Project/Source/SocEnhance.c`
- `103 + 309/Project/Source/SH367309_DataDeal.c`
- `103 + 309/Project/Source/SH367309_Func.c`
- `103 + 309/Project/Source/rtc_sleep.c`
- `103 + 309/Project/Source/conf/conf.c`
- `103 + 309/Project/Source/easylogger/port/elog_port.c`
- `103 + 309/Project/Source/easylogger/src/elog.c`
- `103 + 309/Project/Source/easylogger/src/elog_async.c`
- `103 + 309/Project/Source/easylogger/src/elog_buf.c`
- `103 + 309/Project/Source/easylogger/src/elog_utils.c`

### CMSIS/StdPeriph 源文件

- `103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/core_cm3.c`
- `103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/stm32f10x_it.c`
- `103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/system_stm32f10x.c`
- `103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/src/misc.c`
- `103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/src/stm32f10x_adc.c`
- `103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/src/stm32f10x_bkp.c`
- `103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/src/stm32f10x_can.c`
- `103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/src/stm32f10x_cec.c`
- `103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/src/stm32f10x_crc.c`
- `103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/src/stm32f10x_dac.c`
- `103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/src/stm32f10x_dbgmcu.c`
- `103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/src/stm32f10x_dma.c`
- `103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/src/stm32f10x_exti.c`
- `103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/src/stm32f10x_flash.c`
- `103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/src/stm32f10x_fsmc.c`
- `103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/src/stm32f10x_gpio.c`
- `103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/src/stm32f10x_i2c.c`
- `103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/src/stm32f10x_iwdg.c`
- `103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/src/stm32f10x_pwr.c`
- `103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/src/stm32f10x_rcc.c`
- `103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/src/stm32f10x_rtc.c`
- `103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/src/stm32f10x_sdio.c`
- `103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/src/stm32f10x_spi.c`
- `103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/src/stm32f10x_tim.c`
- `103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/src/stm32f10x_usart.c`
- `103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/src/stm32f10x_wwdg.c`

### 汇编文件

- `103 + 309/Project/STM32F10x_StdPeriph_Lib_V3.5.0/drivers/startup_stm32f10x_hd.s`

## Keil 链接对象顺序

原工作区 `Objects/FD_Release.lnp` 显示链接对象顺序与 Keil 工程源文件顺序一致，并使用：

- `--cpu Cortex-M3`
- `--library_type=microlib`
- `--strict`
- `--scatter ".\Objects\FD_Release.sct"`
- `--map`
- `--list ".\Listings\FD_Release.map"`
- 输出：`.\Objects\FD_Release.axf`

## 当前 Keil Release 尺寸

来自原工作区 `Users/Listings/FD_Release.map`：

- Code：`59788`
- RO Data：`3604`
- RW Data：`1276`
- ZI Data：`5900`
- Total RO Size：`63392` bytes
- Total RW Size：`7176` bytes
- Total ROM Size：`63952` bytes
- 当前 `FD_Release.bin` 大小：`63952` bytes

## 迁移注意事项

- 当前 Keil startup 是 ARMASM 语法，GCC 需要新增独立 `.s` startup，不覆盖旧文件。
- Keil 使用 microlib，GCC 默认 newlib/newlib-nano 的启动、syscalls、printf 行为可能不同，需要在迁移记录中单独跟踪。
- App 起始地址必须固定为 `0x08004800`，不得退回 `0x08000000`。
- 当前工程宏使用 `STM32F10X_MD`，但 startup 文件为 `startup_stm32f10x_hd.s`，这是需要重点复核的历史配置。
- 本次迁移只建立并行 GCC 构建，不删除 Keil 工程。
