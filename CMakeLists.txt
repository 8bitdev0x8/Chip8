cmake_minimum_required(VERSION 3.13)

# Pull in the Pico SDK from common locations.
# Priority: -DPICO_SDK_PATH, repo-local clone, environment variable, build tree fallback.
set(_PICO_SDK_CANDIDATES)

if(DEFINED PICO_SDK_PATH)
    list(APPEND _PICO_SDK_CANDIDATES "${PICO_SDK_PATH}")
endif()

list(APPEND _PICO_SDK_CANDIDATES
    "${CMAKE_SOURCE_DIR}/pico-sdk"
)

if(DEFINED ENV{PICO_SDK_PATH})
    list(APPEND _PICO_SDK_CANDIDATES "$ENV{PICO_SDK_PATH}")
endif()

list(APPEND _PICO_SDK_CANDIDATES
    "${CMAKE_BINARY_DIR}/pico-sdk"
)

set(_PICO_SDK_IMPORT)
foreach(_sdk_path IN LISTS _PICO_SDK_CANDIDATES)
    if(EXISTS "${_sdk_path}/external/pico_sdk_import.cmake")
        set(_PICO_SDK_IMPORT "${_sdk_path}/external/pico_sdk_import.cmake")
        set(PICO_SDK_PATH "${_sdk_path}" CACHE PATH "Path to Pico SDK" FORCE)
        break()
    endif()
endforeach()

if(_PICO_SDK_IMPORT)
    include("${_PICO_SDK_IMPORT}")
else()
    message(FATAL_ERROR "Pico SDK not found. Clone it to ${CMAKE_SOURCE_DIR}/pico-sdk, pass -DPICO_SDK_PATH=<path>, or set PICO_SDK_PATH.")
endif()

project(chip8 C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

# RP2350 target
set(PICO_PLATFORM rp2350-arm-s)
set(PICO_BOARD adafruit_feather_rp2350)  # or adafruit_metro_rp2350 — change to match your board

pico_sdk_init()

# --------------------------------------------------------------------------
# Convert a ROM binary into a C header at configure time.
# Usage: place your .ch8 ROM at roms/rom.ch8 before running cmake.
# --------------------------------------------------------------------------
set(ROM_FILE "${CMAKE_SOURCE_DIR}/roms/rom.ch8")
set(ROM_HEADER "${CMAKE_BINARY_DIR}/rom.h")

if(EXISTS "${ROM_FILE}")
    file(READ "${ROM_FILE}" ROM_BYTES HEX)
    string(REGEX REPLACE "(..)" "0x\\1," ROM_BYTES "${ROM_BYTES}")
    file(WRITE "${ROM_HEADER}"
        "#pragma once\n"
        "#include <stdint.h>\n"
        "static const uint8_t rom_data[] = { ${ROM_BYTES} };\n"
        "static const unsigned int rom_size = sizeof(rom_data);\n"
    )
else()
    # Provide an empty placeholder so the build doesn't fail
    file(WRITE "${ROM_HEADER}"
        "#pragma once\n"
        "#include <stdint.h>\n"
        "// No ROM found at roms/rom.ch8 — place your .ch8 file there and re-run cmake.\n"
        "static const uint8_t rom_data[] = {};\n"
        "static const unsigned int rom_size = 0;\n"
    )
    message(WARNING "No ROM found at ${ROM_FILE}. Place a .ch8 ROM there and re-run cmake.")
endif()

add_executable(chip8
    chip8/chip8/display.cpp
    chip8/chip8/main.cpp
)

target_include_directories(chip8 PRIVATE
    ${CMAKE_BINARY_DIR}   # exposes rom.h
)

target_link_libraries(chip8
    pico_stdlib
    pico_multicore
    hardware_dma
    hardware_gpio
    hardware_clocks
    hardware_structs
)

# Use USB for stdio (useful for debugging via minicom / PuTTY)
pico_enable_stdio_usb(chip8 1)
pico_enable_stdio_uart(chip8 0)

# Generate UF2 / bin / hex
pico_add_extra_outputs(chip8)
