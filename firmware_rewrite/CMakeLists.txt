cmake_minimum_required(VERSION 3.16)

project(bms_firmware_rewrite C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

add_library(bms_rewrite_core
    src/bms_app.c
    src/bms_comm.c
    src/bms_power_can.c
    src/bms_soc.c
    src/bms_storage.c
)

target_include_directories(bms_rewrite_core PUBLIC include)

if(CMAKE_C_COMPILER_ID MATCHES "Clang|GNU")
    target_compile_options(bms_rewrite_core PRIVATE -Wall -Wextra -Werror)
endif()

add_executable(test_rewrite_core tests/test_rewrite_core.c)
target_link_libraries(test_rewrite_core PRIVATE bms_rewrite_core)

enable_testing()
add_test(NAME rewrite_core COMMAND test_rewrite_core)
