cmake_minimum_required(VERSION 3.21)

project(bms_103_309 LANGUAGES C ASM)

set(PROJECT_ROOT "${CMAKE_CURRENT_LIST_DIR}")
set(APP_ROOT "${PROJECT_ROOT}/103 + 309/Project")
set(SOURCE_ROOT "${APP_ROOT}/Source")
set(STDPERIPH_ROOT "${APP_ROOT}/STM32F10x_StdPeriph_Lib_V3.5.0")

include(cmake/stm32f103.cmake)
include(cmake/common_flags.cmake)

add_executable(firmware)

set_target_properties(firmware PROPERTIES
    OUTPUT_NAME firmware
    SUFFIX ".elf"
)

target_sources(firmware PRIVATE
    "${SOURCE_ROOT}/ADC.c"
    "${SOURCE_ROOT}/DataDeal.c"
    "${SOURCE_ROOT}/EEPROM.c"
    "${SOURCE_ROOT}/Fault.c"
    "${SOURCE_ROOT}/Flash.c"
    "${SOURCE_ROOT}/Flash64KAppTest.c"
    "${SOURCE_ROOT}/FactoryAging.c"
    "${SOURCE_ROOT}/LowPowerSleep.c"
    "${SOURCE_ROOT}/Heat_Cool.c"
    "${SOURCE_ROOT}/I2C_AFE1.c"
    "${SOURCE_ROOT}/main.c"
    "${SOURCE_ROOT}/PubFunc.c"
    "${SOURCE_ROOT}/RTC.c"
    "${SOURCE_ROOT}/Runtime.c"
    "${SOURCE_ROOT}/Sci_Upper.c"
    "${SOURCE_ROOT}/SleepDeal.c"
    "${SOURCE_ROOT}/SOC.c"
    "${SOURCE_ROOT}/System_Init.c"
    "${SOURCE_ROOT}/System_Monitor.c"
    "${SOURCE_ROOT}/ProductionID.c"
    "${SOURCE_ROOT}/Can_HDX.c"
    "${SOURCE_ROOT}/CanFeidaoFrames.c"
    "${SOURCE_ROOT}/ChargerLoadFunc.c"
    "${SOURCE_ROOT}/LogRecord.c"
    "${SOURCE_ROOT}/LedBar.c"
    "${SOURCE_ROOT}/ShortFunc.c"
    "${SOURCE_ROOT}/IO_Control.c"
    "${SOURCE_ROOT}/IODrivers.c"
    "${SOURCE_ROOT}/SocEnhance.c"
    "${SOURCE_ROOT}/SH367309_DataDeal.c"
    "${SOURCE_ROOT}/SH367309_Func.c"
    "${SOURCE_ROOT}/rtc_sleep.c"
    "${SOURCE_ROOT}/conf/conf.c"
    "${SOURCE_ROOT}/easylogger/port/elog_port.c"
    "${SOURCE_ROOT}/easylogger/src/elog.c"
    "${SOURCE_ROOT}/easylogger/src/elog_async.c"
    "${SOURCE_ROOT}/easylogger/src/elog_buf.c"
    "${SOURCE_ROOT}/easylogger/src/elog_utils.c"
    "${STDPERIPH_ROOT}/drivers/core_cm3.c"
    "${STDPERIPH_ROOT}/drivers/stm32f10x_it.c"
    "${STDPERIPH_ROOT}/drivers/system_stm32f10x.c"
    "${STDPERIPH_ROOT}/src/misc.c"
    "${STDPERIPH_ROOT}/src/stm32f10x_adc.c"
    "${STDPERIPH_ROOT}/src/stm32f10x_bkp.c"
    "${STDPERIPH_ROOT}/src/stm32f10x_can.c"
    "${STDPERIPH_ROOT}/src/stm32f10x_cec.c"
    "${STDPERIPH_ROOT}/src/stm32f10x_crc.c"
    "${STDPERIPH_ROOT}/src/stm32f10x_dac.c"
    "${STDPERIPH_ROOT}/src/stm32f10x_dbgmcu.c"
    "${STDPERIPH_ROOT}/src/stm32f10x_dma.c"
    "${STDPERIPH_ROOT}/src/stm32f10x_exti.c"
    "${STDPERIPH_ROOT}/src/stm32f10x_flash.c"
    "${STDPERIPH_ROOT}/src/stm32f10x_fsmc.c"
    "${STDPERIPH_ROOT}/src/stm32f10x_gpio.c"
    "${STDPERIPH_ROOT}/src/stm32f10x_i2c.c"
    "${STDPERIPH_ROOT}/src/stm32f10x_iwdg.c"
    "${STDPERIPH_ROOT}/src/stm32f10x_pwr.c"
    "${STDPERIPH_ROOT}/src/stm32f10x_rcc.c"
    "${STDPERIPH_ROOT}/src/stm32f10x_rtc.c"
    "${STDPERIPH_ROOT}/src/stm32f10x_sdio.c"
    "${STDPERIPH_ROOT}/src/stm32f10x_spi.c"
    "${STDPERIPH_ROOT}/src/stm32f10x_tim.c"
    "${STDPERIPH_ROOT}/src/stm32f10x_usart.c"
    "${STDPERIPH_ROOT}/src/stm32f10x_wwdg.c"
    "${PROJECT_ROOT}/startup/startup_stm32f10x_hd_gcc.s"
    "${PROJECT_ROOT}/cmake/compat/syscalls_gcc.c"
)

target_include_directories(firmware PRIVATE
    "${APP_ROOT}/Lib"
    "${STDPERIPH_ROOT}/drivers"
    "${STDPERIPH_ROOT}/inc"
    "${SOURCE_ROOT}"
    "${SOURCE_ROOT}/conf"
    "${SOURCE_ROOT}/easylogger/inc"
    "${PROJECT_ROOT}/cmake/compat"
)

target_compile_definitions(firmware PRIVATE
    STM32F10X_MD
    USE_STDPERIPH_DRIVER
    $<$<CONFIG:Debug>:PROJECT_CFG_BUILD_PROFILE=1>
    $<$<CONFIG:Debug>:PROJECT_CFG_DEBUG_WATCH_ENABLE=1>
    $<$<CONFIG:Debug>:_DEBUG_>
    $<$<CONFIG:Release>:PROJECT_CFG_BUILD_PROFILE=0>
)

set_source_files_properties("${SOURCE_ROOT}/LedBar.c" PROPERTIES
    COMPILE_OPTIONS "-O0"
)

target_arm_none_eabi_options(firmware)

add_custom_command(TARGET firmware POST_BUILD
    COMMAND ${CMAKE_OBJCOPY} -O ihex $<TARGET_FILE:firmware> "${CMAKE_CURRENT_BINARY_DIR}/firmware.hex"
    COMMAND ${CMAKE_OBJCOPY} -O binary $<TARGET_FILE:firmware> "${CMAKE_CURRENT_BINARY_DIR}/firmware.bin"
    COMMAND ${CMAKE_SIZE} $<TARGET_FILE:firmware>
    BYPRODUCTS
        "${CMAKE_CURRENT_BINARY_DIR}/firmware.hex"
        "${CMAKE_CURRENT_BINARY_DIR}/firmware.bin"
    COMMENT "Generating firmware.hex/firmware.bin and printing size"
)
