#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"
#include "pico/multicore.h"
#include "hardware/clocks.h"
#include "hardware/irq.h"
#include "hardware/sync.h"
#include "hardware/vreg.h"

#include "dvi.h"
#include "dvi_serialiser.h"
#include "Chip8.h"
#include "Roms.h"

// --- Configuration ---
const struct dvi_serialiser_cfg adafruit_feather_rp2350_cfg = {
    .pio = pio0,
    .sm_tmds = {0, 1, 2},
    .pins_tmds = {18, 16, 12}, 
    .pins_clk = 14,
    .invert_diffpairs = false
};

#define DVI_TIMING dvi_timing_640x480p_60hz
#define FRAME_WIDTH 320
#define FRAME_HEIGHT 240

struct dvi_inst dvi0;
uint16_t framebuffer[FRAME_WIDTH * FRAME_HEIGHT];

// Core 1: runs the DVI encoder
void core1_main() {
    dvi_register_irqs_this_core(&dvi0, DMA_IRQ_0);
    dvi_start(&dvi0);
    dvi_scanbuf_main_16bpp(&dvi0);
}

Chip8 chip8;
int current_rom_idx = 0;

void load_current_rom() {
    chip8.reset();
    size_t loaded;
    chip8.loadROMFromBuffer(ROM_LIST[current_rom_idx].data, ROM_LIST[current_rom_idx].size, loaded);
    printf("Loaded ROM: %s (%u bytes)\n", ROM_LIST[current_rom_idx].name, (unsigned int)loaded);
}

void update_framebuffer() {
    const int scale = 5;
    const int offset_y = (FRAME_HEIGHT - (VIDEO_HEIGHT * scale)) / 2;

    memset(framebuffer, 0, sizeof(framebuffer));

    for (int cy = 0; cy < VIDEO_HEIGHT; ++cy) {
        for (int cx = 0; cx < VIDEO_WIDTH; ++cx) {
            if (chip8.video[cx + cy * VIDEO_WIDTH]) {
                uint16_t color = 0xFFFF;
                int start_y = offset_y + cy * scale;
                int start_x = cx * scale;
                for (int py = 0; py < scale; ++py) {
                    for (int px = 0; px < scale; ++px) {
                        framebuffer[(start_y + py) * FRAME_WIDTH + (start_x + px)] = color;
                    }
                }
            }
        }
    }
}

const uint8_t font_5x7[][5] = {
    {0x7C, 0x12, 0x11, 0x12, 0x7C}, // A - 0
    {0x7F, 0x49, 0x49, 0x49, 0x36}, // B
    {0x3E, 0x41, 0x41, 0x41, 0x22}, // C
    {0x7F, 0x41, 0x41, 0x22, 0x1C}, // D
    {0x7F, 0x49, 0x49, 0x49, 0x41}, // E
    {0x7F, 0x09, 0x09, 0x09, 0x01}, // F
    {0x3E, 0x41, 0x49, 0x49, 0x7A}, // G
    {0x7F, 0x08, 0x08, 0x08, 0x7F}, // H
    {0x00, 0x41, 0x7F, 0x41, 0x00}, // I
    {0x20, 0x40, 0x41, 0x3F, 0x01}, // J
    {0x7F, 0x08, 0x14, 0x22, 0x41}, // K
    {0x7F, 0x40, 0x40, 0x40, 0x40}, // L
    {0x7F, 0x02, 0x0C, 0x02, 0x7F}, // M
    {0x7F, 0x04, 0x08, 0x10, 0x7F}, // N
    {0x3E, 0x41, 0x41, 0x41, 0x3E}, // O
    {0x7F, 0x09, 0x09, 0x09, 0x06}, // P
    {0x3E, 0x41, 0x51, 0x21, 0x5E}, // Q
    {0x7F, 0x09, 0x19, 0x29, 0x46}, // R
    {0x46, 0x49, 0x49, 0x49, 0x31}, // S
    {0x01, 0x01, 0x7F, 0x01, 0x01}, // T
    {0x3F, 0x40, 0x40, 0x40, 0x3F}, // U
    {0x1F, 0x20, 0x40, 0x20, 0x1F}, // V
    {0x3F, 0x40, 0x38, 0x40, 0x3F}, // W
    {0x63, 0x14, 0x08, 0x14, 0x63}, // X
    {0x07, 0x08, 0x70, 0x08, 0x07}, // Y
    {0x61, 0x51, 0x49, 0x45, 0x43}, // Z - 25
    {0x00, 0x00, 0x00, 0x00, 0x00}, // Space - 26
    {0x3E, 0x51, 0x49, 0x45, 0x3E}, // 0 - 27
    {0x00, 0x42, 0x7F, 0x40, 0x00}, // 1
    {0x42, 0x61, 0x51, 0x49, 0x46}, // 2
    {0x21, 0x41, 0x45, 0x4B, 0x31}, // 3
    {0x18, 0x14, 0x12, 0x7F, 0x10}, // 4
    {0x27, 0x45, 0x45, 0x45, 0x39}, // 5
    {0x3C, 0x4A, 0x49, 0x49, 0x30}, // 6
    {0x01, 0x71, 0x09, 0x05, 0x03}, // 7
    {0x36, 0x49, 0x49, 0x49, 0x36}, // 8
    {0x06, 0x49, 0x49, 0x29, 0x1E}, // 9 - 36
    {0x00, 0x36, 0x36, 0x00, 0x00}, // : - 37
    {0x40, 0x40, 0x40, 0x40, 0x40}, // _ - 38
    {0x02, 0x01, 0x51, 0x09, 0x06}  // ? - 39 (Fallback)
};

void draw_char(int x, int y, char c, uint16_t color) {
    int idx = 39; // Default to '?'
    if (c >= 'a' && c <= 'z') c -= 32;
    
    if (c >= 'A' && c <= 'Z') idx = c - 'A';
    else if (c == ' ') idx = 26;
    else if (c >= '0' && c <= '9') idx = 27 + (c - '0');
    else if (c == ':') idx = 37;
    else if (c == '_') idx = 38;

    const uint8_t* glyph = font_5x7[idx];
    for (int col = 0; col < 5; ++col) {
        uint8_t line = glyph[col];
        for (int row = 0; row < 7; ++row) {
            if ((line >> row) & 1) { // Fixed: Bit 0 is the top row
                int px = x + col;
                int py = y + row;
                if (px >= 0 && px < FRAME_WIDTH && py >= 0 && py < FRAME_HEIGHT) {
                    framebuffer[py * FRAME_WIDTH + px] = color;
                }
            }
        }
    }
}

void draw_string(int x, int y, const char* str, uint16_t color) {
    while (*str) {
        draw_char(x, y, *str, color);
        x += 6;
        str++;
    }
}

int main() {
    vreg_set_voltage(VREG_VOLTAGE_1_20);
    sleep_ms(10);
    set_sys_clock_khz(DVI_TIMING.bit_clk_khz, true);

    stdio_init_all();
    // Wait for USB connection/terminal to catch up
    sleep_ms(2000); 
    
    printf("\n--- Chip8 Pico 2 Starting ---\n");
    dvi0.timing = &DVI_TIMING;
    dvi0.ser_cfg = adafruit_feather_rp2350_cfg;
    dvi_init(&dvi0, next_striped_spin_lock_num(), next_striped_spin_lock_num());

    multicore_launch_core1(core1_main);

    load_current_rom();

    static std::mt19937 rng(1234);
    char status_msg[64];
    uint32_t last_key_time = 0;

    while (true) {
        // Handle Serial Input for Keypad
        int c = getchar_timeout_us(0);
        if (c != PICO_ERROR_TIMEOUT) {
            int key = -1;
            // Standard Keyboard Mapping
            if (c == '1') key = 0x1; else if (c == '2') key = 0x2; else if (c == '3') key = 0x3; else if (c == '4') key = 0xC;
            else if (c == 'q') key = 0x4; else if (c == 'w') key = 0x5; else if (c == 'e') key = 0x6; else if (c == 'r') key = 0xD;
            else if (c == 'a') key = 0x7; else if (c == 's') key = 0x8; else if (c == 'd') key = 0x9; else if (c == 'f') key = 0xE;
            else if (c == 'z') key = 0xA; else if (c == 'x') key = 0x0; else if (c == 'c') key = 0xB; else if (c == 'v') key = 0xF;
            else if (c == 'R' || c == 'p') load_current_rom(); // Reset

            if (key != -1) {
                chip8.keypad[key] = 1;
                last_key_time = to_ms_since_boot(get_absolute_time());
            }
        }

        // Clear keypad after 50ms (simulating key release for Serial)
        if (to_ms_since_boot(get_absolute_time()) - last_key_time > 50) {
            memset(chip8.keypad, 0, sizeof(chip8.keypad));
        }

        for (int i = 0; i < 10; ++i) {
            chip8.cycle(rng);
        }

        if (chip8.drawFlag) {
            chip8.drawFlag = false;
            update_framebuffer();
            
            snprintf(status_msg, sizeof(status_msg), "RUNNING: %s", ROM_LIST[current_rom_idx].name);
            draw_string(10, FRAME_HEIGHT - 15, status_msg, 0x07E0);
        }

        if (chip8.delayTimer > 0) chip8.delayTimer--;
        if (chip8.soundTimer > 0) chip8.soundTimer--;

        for (uint y = 0; y < FRAME_HEIGHT; ++y) {
            const uint16_t *scanline = &framebuffer[y * FRAME_WIDTH];
            queue_add_blocking_u32(&dvi0.q_colour_valid, &scanline);
            while (queue_try_remove_u32(&dvi0.q_colour_free, &scanline));
        }
    }

    return 0;

    
}
