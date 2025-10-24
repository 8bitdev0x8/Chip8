#include <iostream>
#include <fstream>
#include <vector>
#include <chrono>
#include <thread>
#include <cstring>
#include <iomanip>
#include <sstream>
#include <GLFW/glfw3.h>
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"

const unsigned int VIDEO_WIDTH = 64;
const unsigned int VIDEO_HEIGHT = 32;

// Key mapping
const int keymap[16] = { GLFW_KEY_X,GLFW_KEY_1,GLFW_KEY_2,GLFW_KEY_3,
                         GLFW_KEY_Q,GLFW_KEY_W,GLFW_KEY_E,GLFW_KEY_A,
                         GLFW_KEY_S,GLFW_KEY_D,GLFW_KEY_Z,GLFW_KEY_C,
                         GLFW_KEY_4,GLFW_KEY_R,GLFW_KEY_F,GLFW_KEY_V };

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
    uint32_t video[VIDEO_WIDTH * VIDEO_HEIGHT]{};
    bool drawFlag = false;

    Chip8() { pc = 0x200; loadFontset(); }

    void loadFontset() {
        uint8_t fontset[80] = {
            0xF0,0x90,0x90,0x90,0xF0,0x20,0x60,0x20,0x20,0x70,
            0xF0,0x10,0xF0,0x80,0xF0,0xF0,0x10,0xF0,0x10,0xF0,
            0x90,0x90,0xF0,0x10,0x10,0xF0,0x80,0xF0,0x10,0xF0,
            0xF0,0x80,0xF0,0x90,0xF0,0xF0,0x10,0x20,0x40,0x40,
            0xF0,0x90,0xF0,0x90,0xF0,0xE0,0x90,0xE0,0x90,0xE0,
            0xF0,0x80,0x80,0x80,0xF0,0xE0,0x90,0x90,0x90,0xE0,
            0xF0,0x80,0xF0,0x80,0xF0,0xF0,0x80,0xF0,0x80,0x80
        };
        std::memcpy(memory, fontset, 80);
    }

    bool loadROM(const std::string& path, size_t& romSize) {
        std::ifstream file(path, std::ios::binary | std::ios::ate);
        if (!file.is_open()) return false;
        std::streamsize size = file.tellg();
        file.seekg(0, std::ios::beg);
        std::vector<char> buffer(size);
        if (!file.read(buffer.data(), size)) return false;
        std::memset(memory + 0x200, 0, 4096 - 0x200);
        for (size_t i = 0; i < buffer.size(); ++i) memory[0x200 + i] = buffer[i];
        romSize = buffer.size();
        return true;
    }

    void cycle() {
        uint16_t opcode = memory[pc] << 8 | memory[pc + 1];
        pc += 2;
        executeOpcode(opcode);
        if (delayTimer > 0) --delayTimer;
        if (soundTimer > 0) --soundTimer;
    }

private:
    void executeOpcode(uint16_t opcode) {
        uint16_t nnn = opcode & 0x0FFF;
        uint8_t n = opcode & 0x000F;
        uint8_t x = (opcode & 0x0F00) >> 8;
        uint8_t y = (opcode & 0x00F0) >> 4;
        uint8_t kk = opcode & 0x00FF;

        switch (opcode & 0xF000) {
        case 0x0000: switch (kk) {
        case 0xE0: std::memset(video, 0, sizeof(video)); drawFlag = true; break;
        case 0xEE: --sp; pc = stack[sp]; break;
        } break;
        case 0x1000: pc = nnn; break;
        case 0x2000: stack[sp] = pc; ++sp; pc = nnn; break;
        case 0x3000: if (V[x] == kk) pc += 2; break;
        case 0x4000: if (V[x] != kk) pc += 2; break;
        case 0x5000: if (V[x] == V[y]) pc += 2; break;
        case 0x6000: V[x] = kk; break;
        case 0x7000: V[x] += kk; break;
        case 0x8000: switch (n) {
        case 0x0: V[x] = V[y]; break;
        case 0x1: V[x] |= V[y]; break;
        case 0x2: V[x] &= V[y]; break;
        case 0x3: V[x] ^= V[y]; break;
        case 0x4: { uint16_t sum = V[x] + V[y]; V[0xF] = sum > 255 ? 1 : 0; V[x] = sum & 0xFF; break; }
        case 0x5: V[0xF] = V[x] > V[y] ? 1 : 0; V[x] -= V[y]; break;
        case 0x6: V[0xF] = V[x] & 1; V[x] >>= 1; break;
        case 0x7: V[0xF] = V[y] > V[x] ? 1 : 0; V[x] = V[y] - V[x]; break;
        case 0xE: V[0xF] = (V[x] & 0x80) >> 7; V[x] <<= 1; break;
        } break;
        case 0x9000: if (V[x] != V[y]) pc += 2; break;
        case 0xA000: I = nnn; break;
        case 0xB000: pc = nnn + V[0]; break;
        case 0xC000: V[x] = rand() & kk; break;
        case 0xD000: {
            uint8_t vx = V[x], vy = V[y]; V[0xF] = 0;
            for (int row = 0; row < n; row++) {
                uint8_t sprite = memory[I + row];
                for (int col = 0; col < 8; col++) {
                    if (sprite & (0x80 >> col)) {
                        int px = (vx + col) % VIDEO_WIDTH;
                        int py = (vy + row) % VIDEO_HEIGHT;
                        int idx = px + py * VIDEO_WIDTH;
                        if (video[idx]) V[0xF] = 1;
                        video[idx] ^= 0xFFFFFFFF;
                    }
                }
            }
            drawFlag = true;
            break;
        }
        case 0xE000: switch (kk) {
        case 0x9E: if (keypad[V[x]]) pc += 2; break;
        case 0xA1: if (!keypad[V[x]]) pc += 2; break;
        } break;
        case 0xF000: switch (kk) {
        case 0x07: V[x] = delayTimer; break;
        case 0x0A: {
            bool pressed = false;
            for (int i = 0; i < 16; i++) { if (keypad[i]) { V[x] = i; pressed = true; break; } }
            if (!pressed) pc -= 2;
            break;
        }
        case 0x15: delayTimer = V[x]; break;
        case 0x18: soundTimer = V[x]; break;
        case 0x1E: I += V[x]; break;
        case 0x29: I = V[x] * 5; break;
        case 0x33: memory[I] = V[x] / 100; memory[I + 1] = (V[x] / 10) % 10; memory[I + 2] = V[x] % 10; break;
        case 0x55: for (int i = 0; i <= x; i++) memory[I + i] = V[i]; break;
        case 0x65: for (int i = 0; i <= x; i++) V[i] = memory[I + i]; break;
        } break;
        }
    }
};

std::string formatHex(const uint8_t* data, size_t size) {
    std::ostringstream oss;
    for (size_t i = 0; i < size; i++) {
        if (i % 16 == 0) oss << std::endl << std::setw(4) << std::setfill('0') << std::hex << i << ": ";
        oss << std::setw(2) << std::setfill('0') << std::hex << (int)data[i] << " ";
    }
    return oss.str();
}

void renderTexture(uint32_t* video, GLuint texID) {
    glBindTexture(GL_TEXTURE_2D, texID);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, VIDEO_WIDTH, VIDEO_HEIGHT, 0, GL_RGBA, GL_UNSIGNED_BYTE, video);
}

int main() {
    if (!glfwInit()) return -1;
    GLFWwindow* window = glfwCreateWindow(800, 600, "CHIP-8 Debugger", nullptr, nullptr);
    glfwMakeContextCurrent(window);
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 130");

    Chip8 chip8;

    GLuint texID;
    glGenTextures(1, &texID);
    glBindTexture(GL_TEXTURE_2D, texID);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    bool showLoadROM = true;
    char romPath[256] = "";
    size_t romSize = 0;
    float clockSpeed = 500.0f; // Hz
    int clockSpeedInt = static_cast<int>(clockSpeed);
    auto lastTime = std::chrono::high_resolution_clock::now();

    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();
        for (int i = 0; i < 16; i++) chip8.keypad[i] = glfwGetKey(window, keymap[i]) == GLFW_PRESS;

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        // ROM loading
        if (showLoadROM) {
            ImGui::Begin("Load ROM", &showLoadROM);
            ImGui::InputText("ROM Path", romPath, 256);
            if (ImGui::Button("Load")) {
                if (chip8.loadROM(romPath, romSize)) showLoadROM = false;
            }
            ImGui::End();
        }

        // Run cycles
        auto currentTime = std::chrono::high_resolution_clock::now();
        double dt = std::chrono::duration<double>(currentTime - lastTime).count();
        int cycles = int(clockSpeed * dt);
        for (int i = 0; i < cycles; i++) chip8.cycle();
        lastTime = currentTime;

        // Update video texture
        if (chip8.drawFlag) { chip8.drawFlag = false; renderTexture(chip8.video, texID); }

        // Screen window
        ImGui::Begin("Screen");
        ImVec2 winSize = ImGui::GetContentRegionAvail();
        ImGui::Image((void*)(intptr_t)texID, winSize);
        ImGui::End();

        // ROM hex
        if (!showLoadROM) {
            ImGui::Begin("ROM Contents (Hex)");
            ImGui::TextWrapped("%s", formatHex(chip8.memory + 0x200, romSize).c_str());
            ImGui::End();
        }

        // RAM window
        ImGui::Begin("RAM");
        ImGui::TextWrapped("%s", formatHex(chip8.memory, 4096).c_str());
        ImGui::End();

        // Stack window
        ImGui::Begin("Stack");
        for (int i = 0; i < 16; i++) ImGui::Text("0x%04X", chip8.stack[i]);
        ImGui::End();

        // Registers
        ImGui::Begin("Registers");
        for (int i = 0; i < 16; i++) ImGui::Text("V%X: 0x%02X", i, chip8.V[i]);
        ImGui::Text("I: 0x%04X  PC: 0x%04X  SP: %d", chip8.I, chip8.pc, chip8.sp);
        ImGui::Text("DelayTimer: %d  SoundTimer: %d", chip8.delayTimer, chip8.soundTimer);

        // Slider + InputInt synced
        if (ImGui::SliderFloat("Clock (Hz)", &clockSpeed, 60.0f, 2000.0f))
            clockSpeedInt = static_cast<int>(clockSpeed);
        ImGui::SameLine();
        if (ImGui::InputInt("##ClockInt", &clockSpeedInt)) {
            if (clockSpeedInt < 60) clockSpeedInt = 60;
            if (clockSpeedInt > 2000) clockSpeedInt = 2000;
            clockSpeed = static_cast<float>(clockSpeedInt);
        }
        ImGui::End();

        ImGui::Render();
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        glfwSwapBuffers(window);
    }

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}
