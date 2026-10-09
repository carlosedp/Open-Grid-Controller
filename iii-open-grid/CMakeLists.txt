cmake_minimum_required(VERSION 3.13)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

#------------------------------------------------------------------------
#--- board selection (must come before the SDK import)

set(OG_BOARD "rp2040_zero" CACHE STRING "Target board: rp2040_zero or xiao_rp2040")
set_property(CACHE OG_BOARD PROPERTY STRINGS rp2040_zero xiao_rp2040)

if (OG_BOARD STREQUAL "rp2040_zero")
    set(PICO_BOARD waveshare_rp2040_zero)
    set(OG_BOARD_DEFINE OG_BOARD_RP2040_ZERO)
elseif (OG_BOARD STREQUAL "xiao_rp2040")
    set(PICO_BOARD seeed_xiao_rp2040)
    set(OG_BOARD_DEFINE OG_BOARD_XIAO_RP2040)
else ()
    message(FATAL_ERROR "Unknown OG_BOARD '${OG_BOARD}' (use rp2040_zero or xiao_rp2040)")
endif ()

set(OG_I2C_HZ 100000 CACHE STRING "Keypad I2C clock in Hz")

#------------------------------------------------------------------------
#--- pico-sdk (PICO_SDK_PATH, or -DPICO_SDK_FETCH_FROM_GIT=ON)

set(PICO_SDK_FETCH_FROM_GIT_TAG "2.2.0" CACHE STRING "pico-sdk tag to fetch")
include(pico_sdk_import.cmake)

project(iii_open_grid C CXX ASM)

if (PICO_SDK_VERSION_STRING VERSION_LESS "2.0.0")
    message(FATAL_ERROR "pico-sdk 2.0.0 or later required, found ${PICO_SDK_VERSION_STRING}")
endif ()

pico_sdk_init()

set(THIRD_PARTY ${CMAKE_CURRENT_LIST_DIR}/../third_party)
set(III_DIR ${THIRD_PARTY}/iii)

find_package(Python3 REQUIRED COMPONENTS Interpreter)

#------------------------------------------------------------------------
#--- iii's lib.lua, embedded as C (regenerated whenever lib.lua changes)

set(LIB_LUA_C ${CMAKE_CURRENT_BINARY_DIR}/lib_lua.c)
add_custom_command(
        OUTPUT ${LIB_LUA_C}
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_LIST_DIR}/tools/embed_lua.py
                ${III_DIR}/lua/lib.lua ${LIB_LUA_C}
        DEPENDS ${III_DIR}/lua/lib.lua ${CMAKE_CURRENT_LIST_DIR}/tools/embed_lua.py
)

#------------------------------------------------------------------------
#--- third-party libraries

add_subdirectory(${THIRD_PARTY}/lua ${CMAKE_CURRENT_BINARY_DIR}/lua)

add_library(lfs STATIC
        ${THIRD_PARTY}/littlefs/lfs.c
        ${THIRD_PARTY}/littlefs/lfs_util.c
)
target_include_directories(lfs PUBLIC ${THIRD_PARTY}/littlefs)

#------------------------------------------------------------------------
#--- firmware

add_executable(iii-open-grid
        src/main.c
        src/grid.c
        src/keypad.c
        src/monome_serial.c
        src/newlib_locks.c
        src/usb_descriptors.c
        ${III_DIR}/flash.c
        ${III_DIR}/fs.c
        ${III_DIR}/iii.c
        ${III_DIR}/metro.c
        ${III_DIR}/midi.c
        ${III_DIR}/repl.c
        ${III_DIR}/serial.c
        ${III_DIR}/util.c
        ${III_DIR}/vm.c
        ${LIB_LUA_C}
)

target_include_directories(iii-open-grid PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}/src
        ${III_DIR}
        ${III_DIR}/resource
        ${THIRD_PARTY}/lua
)

target_compile_definitions(iii-open-grid PRIVATE
        ${OG_BOARD_DEFINE}
        OG_I2C_HZ=${OG_I2C_HZ}
        PICO_MALLOC_PANIC=0
        # crystal startup margin, as in monome's iii ports
        PICO_XOSC_STARTUP_DELAY_MULTIPLIER=64
)

target_link_libraries(iii-open-grid PRIVATE
        lua
        lfs
        pico_stdlib
        pico_unique_id
        pico_multicore
        hardware_flash
        hardware_i2c
        hardware_sync
        tinyusb_device
)

pico_enable_stdio_usb(iii-open-grid 0)
pico_enable_stdio_uart(iii-open-grid 0)
pico_set_program_name(iii-open-grid "iii-open-grid")
pico_set_program_version(iii-open-grid "og-0.1.0")
pico_add_extra_outputs(iii-open-grid)
