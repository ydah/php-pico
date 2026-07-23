cmake_minimum_required(VERSION 3.13)

if(NOT DEFINED PICO_SDK_PATH AND DEFINED ENV{PICO_SDK_PATH})
  set(PICO_SDK_PATH "$ENV{PICO_SDK_PATH}")
endif()
if(NOT DEFINED PICO_SDK_PATH)
  message(FATAL_ERROR "Set PICO_SDK_PATH to a Raspberry Pi Pico SDK checkout")
endif()
include(${PICO_SDK_PATH}/external/pico_sdk_import.cmake)

project(php_pico VERSION 1.0.0 LANGUAGES C CXX ASM)
option(PPHP_ENABLE_COMPILER "Build the PHP source compiler" ON)
option(PPHP_ENABLE_FLOAT "Build floating-point language support" ON)
option(PPHP_WARNINGS "Emit PHP-compatible runtime warnings" ON)
option(PPHP_TYPECHECK "Enforce declared PHP types at runtime" OFF)
option(PPHP_RC_DEBUG "Enable reference-count consistency checks" OFF)
option(PPHP_RP2040_LTO "Use release LTO for php-pico sources" ON)
if(NOT "${PPHP_ENABLE_COMPILER}" MATCHES "^(0|1|ON|OFF|TRUE|FALSE)$")
  message(FATAL_ERROR "PPHP_ENABLE_COMPILER must be ON/OFF or 1/0")
endif()
if(NOT "${PPHP_ENABLE_FLOAT}" MATCHES "^(0|1|ON|OFF|TRUE|FALSE)$")
  message(FATAL_ERROR "PPHP_ENABLE_FLOAT must be ON/OFF or 1/0")
endif()
if(NOT "${PPHP_WARNINGS}" MATCHES "^(0|1|ON|OFF|TRUE|FALSE)$")
  message(FATAL_ERROR "PPHP_WARNINGS must be ON/OFF or 1/0")
endif()
if(NOT "${PPHP_TYPECHECK}" MATCHES "^(0|1|ON|OFF|TRUE|FALSE)$")
  message(FATAL_ERROR "PPHP_TYPECHECK must be ON/OFF or 1/0")
endif()
if(NOT "${PPHP_RC_DEBUG}" MATCHES "^(0|1|ON|OFF|TRUE|FALSE)$")
  message(FATAL_ERROR "PPHP_RC_DEBUG must be ON/OFF or 1/0")
endif()
# pico-sdk 2.3 headers use C11 static_assert; php-pico sources remain C99.
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
pico_sdk_init()

# The firmware is C-only and never uses new/delete. Avoid pulling the SDK's
# C++ allocation shim (and a full target libstdc++) into the release image.
set_property(TARGET pico_cxx_options PROPERTY INTERFACE_SOURCES "")

get_filename_component(PPHP_ROOT "${CMAKE_CURRENT_LIST_DIR}/../.." ABSOLUTE)

set(PPHP_SOURCES
  ${PPHP_ROOT}/src/alloc.c
  ${PPHP_ROOT}/src/value.c
  ${PPHP_ROOT}/src/pstring.c
  ${PPHP_ROOT}/src/symbol.c
  ${PPHP_ROOT}/src/parray.c
  ${PPHP_ROOT}/src/resource.c
  ${PPHP_ROOT}/src/module.c
  ${PPHP_ROOT}/src/pclass.c
  ${PPHP_ROOT}/src/closure.c
  ${PPHP_ROOT}/src/api.c
  ${PPHP_ROOT}/src/exception.c
  ${PPHP_ROOT}/src/gc.c
  ${PPHP_ROOT}/src/rc_debug.c
  ${PPHP_ROOT}/src/mini_printf.c
  ${PPHP_ROOT}/src/value_ops.c
  ${PPHP_ROOT}/src/pbc.c
  ${PPHP_ROOT}/src/state.c
  ${PPHP_ROOT}/src/vm.c
  ${PPHP_ROOT}/stdlib/builtins.c
  ${PPHP_ROOT}/stdlib/strings.c
  ${PPHP_ROOT}/stdlib/arrays.c
  ${PPHP_ROOT}/stdlib/formatting.c
  ${PPHP_ROOT}/stdlib/json.c
  ${PPHP_ROOT}/stdlib/system.c
  ${PPHP_ROOT}/stdlib/files.c
  ${PPHP_ROOT}/pgems/pgems.c
  ${PPHP_ROOT}/shell/p2sh_device.c
  ${PPHP_ROOT}/fs/fs_lfs.c
  ${PPHP_ROOT}/third_party/littlefs/lfs.c
  ${PPHP_ROOT}/third_party/littlefs/lfs_util.c
  ${PPHP_ROOT}/hal/rp2040/hal_rp2040.c
  ${CMAKE_CURRENT_LIST_DIR}/main.c
)

if(PPHP_ENABLE_FLOAT)
  list(APPEND PPHP_SOURCES ${PPHP_ROOT}/src/float_format.c)
endif()

if(PPHP_ENABLE_COMPILER)
  list(APPEND PPHP_SOURCES
    ${PPHP_ROOT}/compiler/lexer.c
    ${PPHP_ROOT}/compiler/ast.c
    ${PPHP_ROOT}/compiler/parser.c
    ${PPHP_ROOT}/compiler/codegen.c
  )
endif()

set_source_files_properties(
  ${PPHP_SOURCES}
  PROPERTIES COMPILE_OPTIONS "-Wall;-Wextra;-Werror;-Wpedantic;-Wconversion;-Wshadow"
)
set_source_files_properties(
  ${PPHP_ROOT}/third_party/littlefs/lfs.c
  ${PPHP_ROOT}/third_party/littlefs/lfs_util.c
  PROPERTIES COMPILE_OPTIONS "-Wall;-Wextra;-Wno-conversion;-Wno-shadow;-Wno-unused-function"
)
set_property(SOURCE
  ${PPHP_ROOT}/third_party/littlefs/lfs.c
  ${PPHP_ROOT}/third_party/littlefs/lfs_util.c
  APPEND PROPERTY COMPILE_DEFINITIONS
    "$<$<CONFIG:Release>:LFS_NO_DEBUG>"
)
set_source_files_properties(
  ${PPHP_ROOT}/src/mini_printf.c
  PROPERTIES COMPILE_OPTIONS
    "-Wall;-Wextra;-Werror;-Wpedantic;-Wconversion;-Wshadow;$<$<CONFIG:Release>:-Oz;-fno-lto>"
)

# Keep Pico SDK sources outside LTO: GNU ld --wrap is used by the SDK for
# printf and math shims, and whole-target LTO bypasses those wrapper symbols.
# This path is verified with GCC only; other toolchains retain their defaults.
set(PPHP_RP2040_LTO_ENABLED OFF)
if(PPHP_RP2040_LTO)
  if(CMAKE_C_COMPILER_ID STREQUAL "GNU")
    set(PPHP_RP2040_LTO_ENABLED ON)
  else()
    message(STATUS
      "PPHP_RP2040_LTO requested but skipped for unsupported C compiler "
      "${CMAKE_C_COMPILER_ID}")
  endif()
endif()
if(PPHP_RP2040_LTO_ENABLED)
  set(PPHP_LTO_SOURCES ${PPHP_SOURCES})
  list(REMOVE_ITEM PPHP_LTO_SOURCES ${PPHP_ROOT}/src/mini_printf.c)
  set_property(SOURCE ${PPHP_LTO_SOURCES} APPEND PROPERTY
    COMPILE_OPTIONS "$<$<CONFIG:Release>:-Os;-flto;-fipa-pta>"
  )
endif()

add_executable(php-pico ${PPHP_SOURCES})
target_include_directories(php-pico PRIVATE
  ${PPHP_ROOT}/include
  ${PPHP_ROOT}/src
  ${PPHP_ROOT}/stdlib
  ${PPHP_ROOT}/pgems
  ${PPHP_ROOT}/shell
)
if(PPHP_ENABLE_COMPILER)
  target_include_directories(php-pico PRIVATE ${PPHP_ROOT}/compiler)
endif()
target_include_directories(php-pico SYSTEM PRIVATE
  ${PPHP_ROOT}/third_party/littlefs
)
target_compile_definitions(php-pico PRIVATE
  LFS_NO_MALLOC=1
  PPHP_ENABLE_COMPILER=$<BOOL:${PPHP_ENABLE_COMPILER}>
  PPHP_ENABLE_FLOAT=$<BOOL:${PPHP_ENABLE_FLOAT}>
  PPHP_WARNINGS=$<BOOL:${PPHP_WARNINGS}>
  PPHP_TYPECHECK=$<BOOL:${PPHP_TYPECHECK}>
  PPHP_RC_DEBUG=$<BOOL:${PPHP_RC_DEBUG}>
)
target_compile_options(php-pico PRIVATE
  -Os -fmerge-all-constants -ffunction-sections -fdata-sections
)
target_link_options(php-pico PRIVATE -Wl,--gc-sections)
if(PPHP_RP2040_LTO_ENABLED)
  target_link_options(php-pico PRIVATE
    "$<$<CONFIG:Release>:-Os>"
    "$<$<CONFIG:Release>:-flto>"
  )
endif()
target_link_libraries(php-pico
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
target_compile_definitions(php-pico PRIVATE
  PICO_STDIO_SHORT_CIRCUIT_CLIB_FUNCS=0
  snprintf=pphp_snprintf
  vsnprintf=pphp_vsnprintf
)
if(PPHP_ENABLE_FLOAT)
  target_link_libraries(php-pico m)
  pico_minimize_runtime(php-pico INCLUDE
    DEFAULT_ALARM_POOL PRINTF_MINIMAL FLOAT
  )
else()
  pico_minimize_runtime(php-pico INCLUDE
    DEFAULT_ALARM_POOL PRINTF_MINIMAL
  )
endif()
add_library(pico_printf_pphp INTERFACE)
pico_set_printf_implementation(php-pico pphp)
pico_enable_stdio_usb(php-pico 1)
pico_enable_stdio_uart(php-pico 1)
pico_add_extra_outputs(php-pico)
if(NOT PICO_NO_PICOTOOL)
  add_custom_command(TARGET php-pico POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${CMAKE_CURRENT_BINARY_DIR}/php-pico.uf2
            ${CMAKE_CURRENT_BINARY_DIR}/php-pico-${PROJECT_VERSION}.uf2
  )
endif()
