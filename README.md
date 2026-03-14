# CHIP-8 on RP2350 (Adafruit Feather RP2350)

This project builds a CHIP-8 emulator firmware for RP2350 boards using the Raspberry Pi Pico SDK.

The emulator:
- Renders CHIP-8 graphics (64x32) over DVI/HDMI via HSTX.
- Loads a ROM from `roms/rom.ch8` at build/configure time.
- Accepts input from:
  - 16 GPIO buttons (active-low, configurable in source)
  - USB serial keyboard mapping (`1 2 3 4 / Q W E R / A S D F / Z X C V`)

## Prerequisites

Install these on your machine:

1. Git
2. CMake (3.13+)
3. Ninja
4. ARM GCC toolchain (`arm-none-eabi-gcc` on PATH)
5. Pico SDK (2.0+)
6. PowerShell (for `build.ps1` on Windows)

## 1) Clone the repository

```powershell
git clone https://github.com/<your-user>/Chip8.git
cd Chip8
```

## 2) Get Pico SDK (if not already present)

The build system checks multiple SDK locations, including `./pico-sdk`.

From the repo root:

```powershell
git clone --depth 1 --recurse-submodules https://github.com/raspberrypi/pico-sdk.git pico-sdk
```

Optional alternative: set `PICO_SDK_PATH` to an existing SDK install.

```powershell
$env:PICO_SDK_PATH = "D:\path\to\pico-sdk"
```

## 3) Add a ROM

Place your CHIP-8 ROM at:

`roms/rom.ch8`

The CMake configure step converts this ROM into a generated header (`build/rom.h`) embedded in firmware.

If no ROM is present, build still succeeds but runs with an empty ROM.

## 4) Build

### Windows (recommended script)

From repo root:

```powershell
.\build.ps1
```

This does a clean configure + build and produces:

- `build/chip8.uf2`
- `build/chip8.bin`
- `build/chip8.hex`

Rebuild without re-configuring:

```powershell
.\build.ps1 -NoCmake
```

### Manual CMake build (all platforms)

```bash
cmake -S . -B build -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DPICO_PLATFORM=rp2350 \
  -DPICO_BOARD=adafruit_feather_rp2350

cmake --build build --parallel
```

## 5) Run on hardware

1. Put your RP2350 board into BOOTSEL mode.
2. Copy `build/chip8.uf2` to the mounted RPI-RP2 drive.
3. Board reboots and runs CHIP-8.

## Input mapping

### Serial keyboard mapping (USB CDC)

```
CHIP-8 keypad   Keyboard
1 2 3 C         1 2 3 4
4 5 6 D         Q W E R
7 8 9 E         A S D F
A 0 B F         Z X C V
```

### GPIO mapping

GPIO key pin mapping is defined in `chip8/chip8/main.cpp` (`KEY_PINS`).

## Troubleshooting

- Error: Pico SDK not found
  - Clone SDK into `pico-sdk` as shown above, or set `PICO_SDK_PATH`.
- Error: `arm-none-eabi-gcc` not found
  - Install ARM GNU Toolchain and ensure it is on PATH.
- ROM changes not reflected
  - Re-run CMake configure (`.\build.ps1` without `-NoCmake`) so `build/rom.h` is regenerated.
- No serial input
  - Open USB serial monitor after flash and use the key mapping above.

## Notes

- Default target board in CMake is `adafruit_feather_rp2350`.
- If using a different RP2350 board, adjust `PICO_BOARD` in build args or in `CMakeLists.txt`.
- Display pipeline is configured for DVI/HDMI output using RP2350 HSTX.

## License

See `LICENSE`.
