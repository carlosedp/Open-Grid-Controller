cmake_minimum_required(VERSION 3.13)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

#------------------------------------------------------------------------
#--- board selection (must come before the SDK import)
#
# PT_BOARD names the controller board; its family (boards/<family>/) holds the
# grid hardware: board.h (size, pins) and an implementation of src/grid_hw.h.
# The name is what gh_board() reports and what the UF2 is named after.

set(PT_BOARD "rp2040_zero" CACHE STRING "Target board: rp2040_zero or xiao_rp2040")
set_property(CACHE PT_BOARD PROPERTY STRINGS rp2040_zero xiao_rp2040)

if (PT_BOARD STREQUAL "rp2040_zero")
    set(PICO_BOARD waveshare_rp2040_zero)
    set(PT_BOARD_FAMILY open_grid)
    set(PT_BOARD_DEFINE OG_BOARD_RP2040_ZERO)
elseif (PT_BOARD STREQUAL "xiao_rp2040")
    set(PICO_BOARD seeed_xiao_rp2040)
    set(PT_BOARD_FAMILY open_grid)
    set(PT_BOARD_DEFINE OG_BOARD_XIAO_RP2040)
else ()
    message(FATAL_ERROR "Unknown PT_BOARD '${PT_BOARD}' (use rp2040_zero or xiao_rp2040)")
endif ()
set(PT_BOARD_DIR ${CMAKE_CURRENT_LIST_DIR}/boards/${PT_BOARD_FAMILY})

set(PT_I2C_HZ 100000 CACHE STRING "Keypad I2C clock in Hz (open_grid)")
option(PT_LUA_VM_IN_RAM "Run the Lua interpreter loop (luaV_execute, ~11 KB) from RAM; costs that much heap" ON)
option(PT_FAST_CLOCK "Run at 200 MHz (officially supported on RP2040 since pico-sdk 2.1) instead of 125 MHz" OFF)
# Flash offset of iii's status sector (mode); the filesystem follows 64 KB later.
# Fixed so updates keep the files. Changing it moves them once (back up first).
set(PT_FLASH_STATUS_OFFSET 0x80000 CACHE STRING "Flash offset of the status sector, 64 KB aligned")

#------------------------------------------------------------------------
#--- pico-sdk (PICO_SDK_PATH, or -DPICO_SDK_FETCH_FROM_GIT=ON)

set(PICO_SDK_FETCH_FROM_GIT_TAG "2.2.0" CACHE STRING "pico-sdk tag to fetch")
include(pico_sdk_import.cmake)

project(parterre C CXX ASM)

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
if (PT_LUA_VM_IN_RAM)
    target_compile_options(lua PRIVATE -include ${CMAKE_CURRENT_LIST_DIR}/src/lua_vm_in_ram.h)
endif ()

add_library(lfs STATIC
        ${THIRD_PARTY}/littlefs/lfs.c
        ${THIRD_PARTY}/littlefs/lfs_util.c
)
target_include_directories(lfs PUBLIC ${THIRD_PARTY}/littlefs)

#------------------------------------------------------------------------
#--- firmware

# the board family's grid hardware
if (PT_BOARD_FAMILY STREQUAL "open_grid")
    set(PT_BOARD_SOURCES ${PT_BOARD_DIR}/keypad.c)
endif ()

set(III_SOURCES
        ${III_DIR}/flash.c
        ${III_DIR}/fs.c
        ${III_DIR}/iii.c
        ${III_DIR}/metro.c
        ${III_DIR}/midi.c
        ${III_DIR}/repl.c
        ${III_DIR}/serial.c
        ${III_DIR}/util.c
        ${III_DIR}/vm.c
)

add_executable(parterre
        src/main.c
        src/boot_anim.c
        src/file_dump.c
        src/midi_monitor.c
        src/midi_uart.c
        src/grid.c
        src/monome_serial.c
        src/heap_stats.c
        src/newlib_locks.c
        src/status_led.c
        src/usb_descriptors.c
        src/watchdog.c
        ${PT_BOARD_SOURCES}
        ${III_SOURCES}
        ${LIB_LUA_C}
)

# monome's device firmwares build iii with TinyUSB's example warnings as
# errors; use the same here so our iii changes still build there
set_source_files_properties(${III_SOURCES} PROPERTIES COMPILE_OPTIONS
        "-Wall;-Wextra;-Werror;-Wfatal-errors;-Wdouble-promotion;-Wfloat-equal;-Wwrite-strings;-Wsign-compare;-Wmissing-format-attribute;-Wunreachable-code;-Wcast-align;-Wcast-qual;-Wnull-dereference;-Wuninitialized;-Wunused;-Wredundant-decls;-Wconversion;-Wcast-function-type;-Wstrict-overflow;-Wno-strict-aliasing")

target_include_directories(parterre PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}/src
        ${PT_BOARD_DIR}
        ${III_DIR}
        ${III_DIR}/resource
        ${THIRD_PARTY}/lua
)

target_compile_definitions(parterre PRIVATE
        ${PT_BOARD_DEFINE}
        OG_I2C_HZ=${PT_I2C_HZ}
        # 200 MHz with the core voltage raised to 1.15 V by the SDK. The flash
        # divider comes from the board header: XIAO clk/2 (100 MHz at 200 MHz),
        # RP2040-Zero clk/4 (50 MHz)
        $<$<BOOL:${PT_FAST_CLOCK}>:PICO_USE_FASTEST_SUPPORTED_CLOCK=1>
        $<$<BOOL:${PT_LUA_VM_IN_RAM}>:OG_LUA_VM_IN_RAM=1>
        PICO_MALLOC_PANIC=0
        # core 1 runs on its own stack in main RAM (grid.c), so core 0 gets
        # both scratch banks, 8 KB, for Lua
        PICO_CORE1_STACK_SIZE=0
        # crystal startup margin, as in monome's iii ports
        PICO_XOSC_STARTUP_DELAY_MULTIPLIER=64
        # mode and filesystem at a fixed place, so updates don't move the files
        III_FLASH_STATUS_OFFSET=${PT_FLASH_STATUS_OFFSET}u
)

# The program has to end below the pinned status sector (checked after each build).
add_custom_command(TARGET parterre POST_BUILD
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_LIST_DIR}/tools/check_flash_layout.py
                ${CMAKE_NM} $<TARGET_FILE:parterre> ${PT_FLASH_STATUS_OFFSET}
        VERBATIM)

target_link_libraries(parterre PRIVATE
        lua
        lfs
        pico_stdlib
        pico_unique_id
        pico_multicore
        hardware_flash
        hardware_i2c
        hardware_irq
        hardware_pio
        hardware_sync
        hardware_uart
        tinyusb_device
)

# heap_stats.c tracks the heap high-water mark by wrapping newlib's _sbrk
target_link_options(parterre PRIVATE "LINKER:--wrap=_sbrk")
# midi_monitor.c sees every MIDI message sent and received (and passes sends
# on to midi_uart.c for the TRS jack)
target_link_options(parterre PRIVATE "LINKER:--wrap=midi_tx" "LINKER:--wrap=vm_handle_midi")
# watchdog.c stops feeding the watchdog once a reboot is armed (iii's ^^r)
target_link_options(parterre PRIVATE "LINKER:--wrap=watchdog_reboot")

pico_enable_stdio_usb(parterre 0)
pico_enable_stdio_uart(parterre 0)
pico_set_program_name(parterre "parterre")
pico_set_program_version(parterre "og-0.3.16")
pico_add_extra_outputs(parterre)
