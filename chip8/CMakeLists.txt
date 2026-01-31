# == DO NOT EDIT THE FOLLOWING LINES for the Raspberry Pi Pico VS Code Extension to work ==
if(WIN32)
    set(USERHOME $ENV{USERPROFILE})
else()
    set(USERHOME $ENV{HOME})
endif()
set(sdkVersion 2.2.0)
set(toolchainVersion 14_2_Rel1)
set(picotoolVersion 2.2.0-a4)
set(picoVscode ${USERHOME}/.pico-sdk/cmake/pico-vscode.cmake)
if (EXISTS ${picoVscode})
    include(${picoVscode})
endif()
# ====================================================================================
set(PICO_BOARD adafruit_feather_rp2350 CACHE STRING "Board type")

cmake_minimum_required(VERSION 3.13)

# Initialize Pico SDK via FetchContent
include(FetchContent)
FetchContent_Declare(
    pico_sdk
    GIT_REPOSITORY https://github.com/raspberrypi/pico-sdk
    GIT_TAG master
)

# Set Platform to RP2350
set(PICO_PLATFORM rp2350-arm-s)

if (NOT DEFINED ENV{PICO_SDK_PATH})
    message(STATUS "PICO_SDK_PATH not found, fetching...")
    FetchContent_MakeAvailable(pico_sdk)
else()
    include($ENV{PICO_SDK_PATH}/external/pico_sdk_import.cmake)
endif()

project(chip8_pico C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

# Initialize the SDK
pico_sdk_init()

add_executable(chip8_pico
    chip8/main.cpp
)

# Add inclusion for headers
# Enable USB output for debug, disable UART
pico_enable_stdio_usb(chip8_pico 1)
pico_enable_stdio_uart(chip8_pico 0)

FetchContent_Declare(
    pico_dvi
    GIT_REPOSITORY https://github.com/Wren6991/PicoDVI
    GIT_TAG master
)
FetchContent_GetProperties(pico_dvi)
if(NOT pico_dvi_POPULATED)
    FetchContent_Populate(pico_dvi)
    add_subdirectory(${pico_dvi_SOURCE_DIR}/software ${pico_dvi_BINARY_DIR})
endif()

target_include_directories(chip8_pico PRIVATE 
    chip8
    ${pico_dvi_SOURCE_DIR}/software/include
    ${pico_dvi_SOURCE_DIR}/software/libdvi
)

target_link_libraries(chip8_pico 
    pico_stdlib
    pico_multicore
    libdvi
    hardware_dma
    hardware_pio
)

# Create .uf2 file
pico_add_extra_outputs(chip8_pico)
