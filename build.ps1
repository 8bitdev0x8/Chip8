# build.ps1 — Configure and build the CHIP-8 RP2350 project
# Prerequisites:
#   1. Pico SDK >= 2.0 installed; set PICO_SDK_PATH environment variable
#   2. ARM GCC toolchain on PATH (arm-none-eabi-gcc)
#   3. CMake >= 3.13 on PATH
#   4. Ninja (optional but recommended) or MinGW make
#
# Usage:
#   .\build.ps1           — full clean configure + build
#   .\build.ps1 -NoCmake  — just rebuild (skip cmake configure)

param([switch]$NoCmake)

$ErrorActionPreference = "Stop"

$BUILD_DIR = "$PSScriptRoot\build"

if (-not $NoCmake) {
    if (Test-Path $BUILD_DIR) { Remove-Item $BUILD_DIR -Recurse -Force }
    New-Item -ItemType Directory $BUILD_DIR | Out-Null

    Push-Location $BUILD_DIR
    try {
        cmake .. -G "Ninja" `
            -DCMAKE_BUILD_TYPE=Release `
            -DPICO_PLATFORM=rp2350 `
            -DPICO_BOARD=adafruit_feather_rp2350
    } finally { Pop-Location }
}

Push-Location $BUILD_DIR
try { cmake --build . --parallel } finally { Pop-Location }

Write-Host ""
Write-Host "Build complete. Flash chip8.uf2 to your RP2350 board." -ForegroundColor Green
