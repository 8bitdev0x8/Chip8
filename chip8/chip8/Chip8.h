#pragma once

#include <cstdint>
#include <cstring>
#include <random>

const unsigned int VIDEO_WIDTH = 64;
const unsigned int VIDEO_HEIGHT = 32;

class Chip8 {
public:
    uint8_t memory[4096]{};
    uint8_t V[16]{};
    uint16_t I{};
    uint16_t pc{};
    uint16_t stack[16]{};
    uint16_t sp{};
    uint8_t delayTimer{};
    uint8_t soundTimer{};
    uint8_t keypad[16]{};
    uint8_t video[VIDEO_WIDTH * VIDEO_HEIGHT]{}; // monochrome 0 or 1
    bool drawFlag = false;

    // Quirks
    bool quirkShift = false; // true: V[x] = V[x] >> 1, false: V[x] = V[y] >> 1
    bool quirkLoadStore = false; // true: I is NOT incremented, false: I = I + x + 1

    Chip8() { reset(); }

    void reset() {
        std::memset(memory, 0, sizeof(memory));
        std::memset(V, 0, sizeof(V));
        I = 0;
        pc = 0x200;
        std::memset(stack, 0, sizeof(stack));
        sp = 0;
        delayTimer = 0;
        soundTimer = 0;
        std::memset(keypad, 0, sizeof(keypad));
        std::memset(video, 0, sizeof(video));
        drawFlag = true;
        loadFontset();
    }

    void loadFontset() {
        // Classic font location at 0x050
        uint8_t fontset[80] = {
            0xF0, 0x90, 0x90, 0x90, 0xF0, // 0
            0x20, 0x60, 0x20, 0x20, 0x70, // 1
            0xF0, 0x10, 0xF0, 0x80, 0xF0, // 2
            0xF0, 0x10, 0xF0, 0x10, 0xF0, // 3
            0x90, 0x90, 0xF0, 0x10, 0x10, // 4
            0xF0, 0x80, 0xF0, 0x10, 0xF0, // 5
            0xF0, 0x80, 0xF0, 0x90, 0xF0, // 6
            0xF0, 0x10, 0x20, 0x40, 0x40, // 7
            0xF0, 0x90, 0xF0, 0x90, 0xF0, // 8
            0xF0, 0x90, 0xF0, 0x10, 0xF0, // 9
            0xF0, 0x90, 0xF0, 0x90, 0x90, // A
            0xE0, 0x90, 0xE0, 0x90, 0xE0, // B
            0xF0, 0x80, 0x80, 0x80, 0xF0, // C
            0xE0, 0x90, 0x90, 0x90, 0xE0, // D
            0xF0, 0x80, 0xF0, 0x80, 0xF0, // E
            0xF0, 0x80, 0xF0, 0x80, 0x80  // F
        };
        std::memcpy(memory + 0x050, fontset, sizeof(fontset));
    }

    bool loadROMFromBuffer(const uint8_t* buffer, size_t size, size_t& loadedSize) {
        if (size <= 0 || size > (4096 - 0x200)) return false;
        std::memset(memory + 0x200, 0, 4096 - 0x200);
        std::memcpy(memory + 0x200, buffer, size);
        loadedSize = size;
        pc = 0x200;
        sp = 0;
        I = 0;
        return true;
    }

    void cycle(std::mt19937& rng) {
        // fetch
        uint16_t opcode = (memory[pc] << 8) | memory[pc + 1];
        pc += 2;
        executeOpcode(opcode, rng);
    }

private:
    void executeOpcode(uint16_t opcode, std::mt19937& rng) {
        uint16_t nnn = opcode & 0x0FFF;
        uint8_t n = opcode & 0x000F;
        uint8_t x = (opcode & 0x0F00) >> 8;
        uint8_t y = (opcode & 0x00F0) >> 4;
        uint8_t kk = opcode & 0x00FF;

        switch (opcode & 0xF000) {
        case 0x0000:
            switch (kk) {
            case 0xE0: // CLS
                std::memset(video, 0, sizeof(video));
                drawFlag = true;
                break;
            case 0xEE: // RET
                if (sp == 0) {
                    pc = 0; // stack underflow
                } else {
                    --sp;
                    pc = stack[sp];
                }
                break;
            default:
                break;
            }
            break;

        case 0x1000: // JP nnn
            pc = nnn;
            break;

        case 0x2000: // CALL nnn
            if (sp < 16) {
                stack[sp] = pc;
                ++sp;
                pc = nnn;
            }
            break;

        case 0x3000: // SE Vx, kk
            if (V[x] == kk) pc += 2;
            break;

        case 0x4000: // SNE Vx, kk
            if (V[x] != kk) pc += 2;
            break;

        case 0x5000: // SE Vx, Vy
            if ((opcode & 0x000F) == 0x0) {
                if (V[x] == V[y]) pc += 2;
            }
            break;

        case 0x6000: // LD Vx, kk
            V[x] = kk;
            break;

        case 0x7000: // ADD Vx, kk
            V[x] = static_cast<uint8_t>(V[x] + kk);
            break;

        case 0x8000:
            switch (n) {
            case 0x0: V[x] = V[y]; break;
            case 0x1: V[x] |= V[y]; break;
            case 0x2: V[x] &= V[y]; break;
            case 0x3: V[x] ^= V[y]; break;
            case 0x4: {
                uint16_t sum = V[x] + V[y];
                V[0xF] = sum > 255 ? 1 : 0;
                V[x] = sum & 0xFF;
                break;
            }
            case 0x5:
                V[0xF] = V[x] >= V[y] ? 1 : 0; // Improved: should be >= for VF=1 on no borrow
                V[x] = static_cast<uint8_t>(V[x] - V[y]);
                break;
            case 0x6:
                if (quirkShift) {
                    V[0xF] = V[x] & 0x1;
                    V[x] >>= 1;
                } else {
                    V[0xF] = V[y] & 0x1;
                    V[x] = V[y] >> 1;
                }
                break;
            case 0x7:
                V[0xF] = V[y] >= V[x] ? 1 : 0;
                V[x] = static_cast<uint8_t>(V[y] - V[x]);
                break;
            case 0xE:
                if (quirkShift) {
                    V[0xF] = (V[x] & 0x80) >> 7;
                    V[x] <<= 1;
                } else {
                    V[0xF] = (V[y] & 0x80) >> 7;
                    V[x] = V[y] << 1;
                }
                break;
            default:
                break;
            }
            break;

        case 0x9000: // SNE Vx, Vy
            if ((opcode & 0x000F) == 0x0) {
                if (V[x] != V[y]) pc += 2;
            }
            break;

        case 0xA000: // LD I, nnn
            I = nnn;
            break;

        case 0xB000: // JP V0, nnn
            pc = nnn + V[0];
            break;

        case 0xC000: { // RND Vx, kk
            std::uniform_int_distribution<int> dist(0, 255);
            V[x] = static_cast<uint8_t>(dist(rng) & kk);
            break;
        }

        case 0xD000: { // DRW Vx, Vy, nibble
            uint8_t vx = V[x] % VIDEO_WIDTH;
            uint8_t vy = V[y] % VIDEO_HEIGHT;
            V[0xF] = 0;
            for (int row = 0; row < n; row++) {
                if (vy + row >= VIDEO_HEIGHT) break; // Clipping
                uint8_t sprite = memory[I + row];
                for (int col = 0; col < 8; col++) {
                    if (vx + col >= VIDEO_WIDTH) break; // Clipping
                    if (sprite & (0x80 >> col)) {
                        int idx = (vx + col) + (vy + row) * VIDEO_WIDTH;
                        if (video[idx]) V[0xF] = 1;
                        video[idx] ^= 1;
                    }
                }
            }
            drawFlag = true;
            break;
        }

        case 0xE000:
            switch (kk) {
            case 0x9E: // SKP Vx
                if (V[x] < 16 && keypad[V[x]]) pc += 2;
                break;
            case 0xA1: // SKNP Vx
                if (V[x] >= 16 || !keypad[V[x]]) pc += 2;
                break;
            default:
                break;
            }
            break;

        case 0xF000:
            switch (kk) {
            case 0x07: V[x] = delayTimer; break;
            case 0x0A: { // LD Vx, K
                bool pressed = false;
                for (int i = 0; i < 16; ++i) {
                    if (keypad[i]) {
                        V[x] = i;
                        pressed = true;
                        break;
                    }
                }
                if (!pressed) pc -= 2;
                break;
            }
            case 0x15: delayTimer = V[x]; break;
            case 0x18: soundTimer = V[x]; break;
            case 0x1E: I = static_cast<uint16_t>(I + V[x]); break;
            case 0x29: I = static_cast<uint16_t>(0x050 + (V[x] & 0xF) * 5); break;
            case 0x33:
                memory[I] = V[x] / 100;
                memory[I + 1] = (V[x] / 10) % 10;
                memory[I + 2] = V[x] % 10;
                break;
            case 0x55: // LD [I], V0..Vx
                for (int i = 0; i <= x; ++i) memory[I + i] = V[i];
                if (!quirkLoadStore) I = static_cast<uint16_t>(I + x + 1);
                break;
            case 0x65: // LD V0..Vx, [I]
                for (int i = 0; i <= x; ++i) V[i] = memory[I + i];
                if (!quirkLoadStore) I = static_cast<uint16_t>(I + x + 1);
                break;
            default:
                break;
            }
            break;
        }
    }
};
