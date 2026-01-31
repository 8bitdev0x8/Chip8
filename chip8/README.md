# Chip8 for RP2350

A simple Chip8 emulator designed to run on the Raspberry Pi RP2350 (Pico 2).

## Prerequisites
*   [Raspberry Pi Pico SDK](https://github.com/raspberrypi/pico-sdk)
*   CMake
*   ARM GCC Toolchain (`arm-none-eabi-gcc`)
*   (Optional) [Visual Studio Code](https://code.visualstudio.com/) with [Raspberry Pi Pico Extension](https://marketplace.visualstudio.com/items?itemName=raspberry-pi.raspberry-pi-pico)

## How to Build

### Option 1: Using Visual Studio Code (Recommended)
1.  Install the **Raspberry Pi Pico** extension in VS Code.
2.  Open this folder in VS Code.
3.  The extension should detect the project. Click "Compile Project" or check the status bar for build, options.
4.  The output `.uf2` file will be in the `build/` directory.

### Option 2: Command Line
1.  Open a terminal in this directory.
2.  Create a build directory:
    ```bash
    mkdir build
    cd build
    ```
3.  Configure CMake (ensure `PICO_SDK_PATH` is set if not fetching automatically):
    ```bash
    cmake -DPICO_PLATFORM=rp2350 ..
    ```
    *Note: The first time usually takes longer as it downloads the SDK.*
4.  Build:
    ```bash
    make
    ```
    *(Or `ninja` or `cmake --build .` depending on your system)*

## Flashing
1.  Hold the BOOTSEL button on your RP2350 device.
2.  Plug it into your computer via USB.
3.  Copy the generated `chip8_pico.uf2` file from the `build` folder to the mass storage device that appears (e.g., `RP2350`).
