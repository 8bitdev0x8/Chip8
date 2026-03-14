/*
 * CHIP-8 emulator for Adafruit RP2350 board
 * Display: HDMI via RP2350 HSTX peripheral (DVI 640x480 @ 60 Hz)
 * Input:   16 GPIO buttons (see KEY_PINS below)
 *
 * Build requirements:
 *   - Pico SDK >= 2.0  (RP2350 + HSTX support)
 *   - Place your ROM at  <project>/roms/rom.ch8  and re-run cmake
 *
 * CHIP-8 display (64x32) is scaled 8x -> 512x256, centred in 640x480.
 */

#include <cstring>
#include <cstdlib>
#include <cstdio>
#include "pico/stdlib.h"
#include "pico/platform.h"
#include "pico/stdio_usb.h"
#include "hardware/gpio.h"
#include "display.h"

#if defined(__has_include) && __has_include("../../build/rom.h")
#include "../../build/rom.h"   // VS project fallback include path
#else
static const uint8_t rom_data[] = {};
static const unsigned int rom_size = 0;
#endif

static constexpr uint32_t CHIP8_CYCLES_PER_FRAME = 8u;
static constexpr uint32_t FRAME_INTERVAL_US = 1000000u / 60u;   // ~16 667 us
static constexpr uint64_t SERIAL_KEY_LATCH_US = 250000u;
static constexpr int KEY_COUNT = 16;

// ---------------------------------------------------------------------------
// CHIP-8 machine
// ---------------------------------------------------------------------------
class Chip8 {
public:
    static constexpr uint16_t kRomStart = 0x200;
    static constexpr size_t kMemorySize = 4096;

    uint8_t  memory[kMemorySize]{};
    uint8_t  V[16]{};
    uint16_t I{};
    uint16_t pc{};
    uint16_t stack[16]{};
    uint16_t sp{};
    uint8_t  delayTimer{};
    uint8_t  soundTimer{};
    uint8_t  keypad[16]{};
    uint32_t video[chip8_display::kChip8Width * chip8_display::kChip8Height]{};
    bool     drawFlag{};

    Chip8() { reset(); }

    void reset() {
        memset(this, 0, sizeof(*this));
        pc = kRomStart;
        loadFontset();
    } 
 
    void loadFontset() {
        static const uint8_t fontset[80] = {
            0xF0,0x90,0x90,0x90,0xF0, 0x20,0x60,0x20,0x20,0x70,
            0xF0,0x10,0xF0,0x80,0xF0, 0xF0,0x10,0xF0,0x10,0xF0,
            0x90,0x90,0xF0,0x10,0x10, 0xF0,0x80,0xF0,0x10,0xF0,
            0xF0,0x80,0xF0,0x90,0xF0, 0xF0,0x10,0x20,0x40,0x40,
            0xF0,0x90,0xF0,0x90,0xF0, 0xE0,0x90,0xE0,0x90,0xE0,
            0xF0,0x80,0x80,0x80,0xF0, 0xE0,0x90,0x90,0x90,0xE0,
            0xF0,0x80,0xF0,0x80,0xF0, 0xF0,0x80,0xF0,0x80,0x80,
        };
        memcpy(memory, fontset, sizeof(fontset));
    }

    void loadROM(const uint8_t* data, size_t size) {
        if (size > (kMemorySize - kRomStart)) size = kMemorySize - kRomStart;
        memcpy(memory + kRomStart, data, size);
    }

    void cycle() {
        uint16_t opcode = (uint16_t)((memory[pc] << 8) | memory[pc + 1]);
        pc += 2;
        executeOpcode(opcode);
        if (delayTimer > 0) --delayTimer;
        if (soundTimer > 0) --soundTimer;
    }

private:
    void executeOpcode(uint16_t opcode) {
        uint16_t nnn = opcode & 0x0FFFu;
        uint8_t  n   = opcode & 0x000Fu;
        uint8_t  x   = (opcode & 0x0F00u) >> 8;
        uint8_t  y   = (opcode & 0x00F0u) >> 4;
        uint8_t  kk  = opcode & 0x00FFu;

        switch (opcode & 0xF000u) {
        case 0x0000:
            switch (kk) {
            case 0xE0: memset(video, 0, sizeof(video)); drawFlag = true; break;
            case 0xEE: pc = stack[--sp]; break;
            } break;
        case 0x1000: pc = nnn; break;
        case 0x2000: stack[sp++] = pc; pc = nnn; break;
        case 0x3000: if (V[x] == kk) pc += 2; break;
        case 0x4000: if (V[x] != kk) pc += 2; break;
        case 0x5000: if (V[x] == V[y]) pc += 2; break;
        case 0x6000: V[x] = kk; break;
        case 0x7000: V[x] += kk; break;
        case 0x8000:
            switch (n) {
            case 0x0: V[x] = V[y]; break;
            case 0x1: V[x] |= V[y]; break;
            case 0x2: V[x] &= V[y]; break;
            case 0x3: V[x] ^= V[y]; break;
            case 0x4: { uint16_t s = V[x]+V[y]; V[0xF]=s>255?1:0; V[x]=s&0xFF; break; }
            case 0x5: V[0xF]=V[x]>V[y]?1:0; V[x]-=V[y]; break;
            case 0x6: V[0xF]=V[x]&1; V[x]>>=1; break;
            case 0x7: V[0xF]=V[y]>V[x]?1:0; V[x]=V[y]-V[x]; break;
            case 0xE: V[0xF]=(V[x]&0x80)>>7; V[x]<<=1; break;
            } break;
        case 0x9000: if (V[x] != V[y]) pc += 2; break;
        case 0xA000: I = nnn; break;
        case 0xB000: pc = nnn + V[0]; break;
        case 0xC000: V[x] = (uint8_t)(rand() & kk); break;
        case 0xD000: {
            uint8_t vx = V[x], vy = V[y];
            V[0xF] = 0;
            for (int row = 0; row < n; row++) {
                uint8_t sprite = memory[I + row];
                for (int col = 0; col < 8; col++) {
                    if (sprite & (0x80u >> col)) {
                        uint32_t px = (vx + col) % chip8_display::kChip8Width;
                        uint32_t py = (vy + row) % chip8_display::kChip8Height;
                        uint32_t idx = px + py * chip8_display::kChip8Width;
                        if (video[idx]) V[0xF] = 1;
                        video[idx] ^= 0xFFFFFFFFu;
                    }
                }
            }
            drawFlag = true;
            break;
        }
        case 0xE000:
            switch (kk) {
            case 0x9E: if ( keypad[V[x]]) pc += 2; break;
            case 0xA1: if (!keypad[V[x]]) pc += 2; break;
            } break;
        case 0xF000:
            switch (kk) {
            case 0x07: V[x] = delayTimer; break;
            case 0x0A: {
                bool hit = false;
                for (int i = 0; i < 16; i++) { if (keypad[i]) { V[x]=i; hit=true; break; } }
                if (!hit) pc -= 2;
                break;
            }
            case 0x15: delayTimer = V[x]; break;
            case 0x18: soundTimer = V[x]; break;
            case 0x1E: I += V[x]; break;
            case 0x29: I = V[x] * 5; break;
            case 0x33:
                memory[I]   =  V[x] / 100;
                memory[I+1] = (V[x] / 10) % 10;
                memory[I+2] =  V[x] % 10;
                break;
            case 0x55: for (int i=0;i<=x;i++) memory[I+i]=V[i]; break;
            case 0x65: for (int i=0;i<=x;i++) V[i]=memory[I+i]; break;
            } break;
        }
    }
};

// ---------------------------------------------------------------------------
// GPIO key input
// 16 buttons wired to GPIO pins, active-low with internal pull-ups.
// Adjust KEY_PINS to match your physical wiring.
//
// Default CHIP-8 hex keypad layout:
//   1 2 3 C       GPIO: 21 22 26 27
//   4 5 6 D             28  2  3  4
//   7 8 9 E              5  6  7  8
//   A 0 B F              9 20 10 11
// ---------------------------------------------------------------------------
static const uint8_t KEY_PINS[16] = {
//   0   1   2   3   4   5   6   7
    20, 21, 22, 26, 27, 28,  2,  3,
//   8   9   A   B   C   D   E   F
     4,  5,  6,  7,  8,  9, 10, 11
};

static int key_from_serial_char(int ch) {
    switch (ch) {
    case '1': return 0x1;
    case '2': return 0x2;
    case '3': return 0x3;
    case '4': return 0xC;
    case 'Q': case 'q': return 0x4;
    case 'W': case 'w': return 0x5;
    case 'E': case 'e': return 0x6;
    case 'R': case 'r': return 0xD;
    case 'A': case 'a': return 0x7;
    case 'S': case 's': return 0x8;
    case 'D': case 'd': return 0x9;
    case 'F': case 'f': return 0xE;
    case 'Z': case 'z': return 0xA;
    case 'X': case 'x': return 0x0;
    case 'C': case 'c': return 0xB;
    case 'V': case 'v': return 0xF;
    default: return -1;
    }
}

static void keys_init() {
    for (int i = 0; i < KEY_COUNT; i++) {
        gpio_init(KEY_PINS[i]);
        gpio_set_dir(KEY_PINS[i], GPIO_IN);
        gpio_pull_up(KEY_PINS[i]);
    }
}

static void keys_update_gpio(uint8_t* keypad) {
    for (int i = 0; i < KEY_COUNT; i++)
        keypad[i] = !gpio_get(KEY_PINS[i]) ? 1u : 0u;  // active-low
}

static void keys_update_serial(uint8_t* serial_keypad, uint64_t* serial_expire_us) {
    const uint64_t now = time_us_64();

    if (*serial_expire_us != 0 && now >= *serial_expire_us) {
        memset(serial_keypad, 0, KEY_COUNT);
        *serial_expire_us = 0;
    }

    while (true) {
        int ch = getchar_timeout_us(0);
        if (ch == PICO_ERROR_TIMEOUT) break;

        int key = key_from_serial_char(ch);
        if (key >= 0) {
            memset(serial_keypad, 0, KEY_COUNT);
            serial_keypad[key] = 1u;
            *serial_expire_us = now + SERIAL_KEY_LATCH_US;
            printf("[serial] '%c' -> CHIP-8 0x%X\r\n", ch, key);
        }
    }
}

static void keys_merge(uint8_t* dst, const uint8_t* gpio_keypad, const uint8_t* serial_keypad) {
    for (int i = 0; i < KEY_COUNT; i++) {
        dst[i] = (gpio_keypad[i] || serial_keypad[i]) ? 1u : 0u;
    }
}

// ---------------------------------------------------------------------------
// main — core 0: emulation loop at ~500 Hz, display update at 60 Hz
// ---------------------------------------------------------------------------
int main() {
    stdio_init_all();

    // Give USB CDC a brief chance to connect so serial input/output is usable.
    for (int i = 0; i < 200 && !stdio_usb_connected(); ++i) {
        sleep_ms(10);
    }
    printf("Serial keypad ready. Use PC layout: 1234 / QWER / ASDF / ZXCV\r\n");

    chip8_display::init();

    keys_init();

    Chip8 chip8;
    chip8.loadROM(rom_data, rom_size);

    uint8_t gpio_keypad[KEY_COUNT]{};
    uint8_t serial_keypad[KEY_COUNT]{};
    uint64_t serial_expire_us = 0;

    uint64_t last_frame = time_us_64();

    while (true) {
        keys_update_serial(serial_keypad, &serial_expire_us);

        uint64_t now = time_us_64();
        if ((now - last_frame) >= FRAME_INTERVAL_US) {
            last_frame += FRAME_INTERVAL_US;

            keys_update_gpio(gpio_keypad);
            keys_merge(chip8.keypad, gpio_keypad, serial_keypad);

            for (uint32_t i = 0; i < CHIP8_CYCLES_PER_FRAME; i++)
                chip8.cycle();

            if (chip8.drawFlag) {
                chip8.drawFlag = false;
                chip8_display::present(chip8.video);
            }
        }

        tight_loop_contents();
    }
}

