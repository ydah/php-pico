cmake_minimum_required(VERSION 3.13)

if(NOT DEFINED PICO_SDK_PATH)
  message(FATAL_ERROR "Set PICO_SDK_PATH to a Raspberry Pi Pico SDK checkout")
endif()
include(${PICO_SDK_PATH}/external/pico_sdk_import.cmake)

project(php_pico C CXX ASM)
set(CMAKE_C_STANDARD 99)
set(CMAKE_CXX_STANDARD 17)
pico_sdk_init()

get_filename_component(PPHP_ROOT "${CMAKE_CURRENT_LIST_DIR}/../.." ABSOLUTE)

set(PPHP_SOURCES
  ${PPHP_ROOT}/src/alloc.c
  ${PPHP_ROOT}/src/value.c
  ${PPHP_ROOT}/src/pstring.c
  ${PPHP_ROOT}/src/symbol.c
  ${PPHP_ROOT}/src/parray.c
  ${PPHP_ROOT}/src/resource.c
  ${PPHP_ROOT}/src/pclass.c
  ${PPHP_ROOT}/src/closure.c
  ${PPHP_ROOT}/src/api.c
  ${PPHP_ROOT}/src/exception.c
  ${PPHP_ROOT}/src/gc.c
  ${PPHP_ROOT}/src/value_ops.c
  ${PPHP_ROOT}/src/pbc.c
  ${PPHP_ROOT}/src/state.c
  ${PPHP_ROOT}/src/vm.c
  ${PPHP_ROOT}/compiler/lexer.c
  ${PPHP_ROOT}/compiler/ast.c
  ${PPHP_ROOT}/compiler/parser.c
  ${PPHP_ROOT}/compiler/codegen.c
  ${PPHP_ROOT}/stdlib/builtins.c
  ${PPHP_ROOT}/stdlib/strings.c
  ${PPHP_ROOT}/stdlib/arrays.c
  ${PPHP_ROOT}/stdlib/formatting.c
  ${PPHP_ROOT}/stdlib/json.c
  ${PPHP_ROOT}/stdlib/system.c
  ${PPHP_ROOT}/stdlib/files.c
  ${PPHP_ROOT}/pgems/pgems.c
  ${PPHP_ROOT}/hal/rp2040/hal_rp2040.c
  ${CMAKE_CURRENT_LIST_DIR}/main.c
)

add_executable(php-pico ${PPHP_SOURCES})
target_include_directories(php-pico PRIVATE
  ${PPHP_ROOT}/include
  ${PPHP_ROOT}/src
  ${PPHP_ROOT}/compiler
  ${PPHP_ROOT}/stdlib
  ${PPHP_ROOT}/pgems
)
target_compile_options(php-pico PRIVATE
  -Os -ffunction-sections -fdata-sections
  -Wall -Wextra -Werror -Wpedantic -Wconversion -Wshadow
)
target_link_options(php-pico PRIVATE -Wl,--gc-sections)
target_link_libraries(php-pico
  m
  pico_stdlib
  pico_unique_id
  hardware_adc
  hardware_clocks
  hardware_flash
  hardware_gpio
  hardware_i2c
  hardware_pwm
  hardware_spi
  hardware_sync
  hardware_uart
  hardware_watchdog
)
pico_enable_stdio_usb(php-pico 1)
pico_enable_stdio_uart(php-pico 1)
pico_add_extra_outputs(php-pico)
