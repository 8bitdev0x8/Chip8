// chip8_debugger.cpp
// Full CHIP-8 with ImGui debugger: run/pause/step, opcode viewer, disassembly highlighting.
// Requires GLFW, ImGui (imgui_impl_glfw.cpp, imgui_impl_opengl3.cpp) and OpenGL.

#include <iostream>
#include <fstream>
#include <vector>
#include <chrono>
#include <thread>
#include <iomanip>
#include <sstream>
#include <random>
#include <GLFW/glfw3.h>
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"

const unsigned int VIDEO_WIDTH = 64;
const unsigned int VIDEO_HEIGHT = 32;


const int keymap[16] = {
    GLFW_KEY_X, GLFW_KEY_1, GLFW_KEY_2, GLFW_KEY_3,
    GLFW_KEY_Q, GLFW_KEY_W, GLFW_KEY_E, GLFW_KEY_A,
    GLFW_KEY_S, GLFW_KEY_D, GLFW_KEY_Z, GLFW_KEY_C,
    GLFW_KEY_4, GLFW_KEY_R, GLFW_KEY_F, GLFW_KEY_V
};

struct Chip8State {
    uint8_t memory[4096];
    uint8_t V[16];
    uint16_t I;
    uint16_t pc;
    uint16_t stack[16];
    uint16_t sp;
    uint8_t delayTimer;
    uint8_t soundTimer;
    uint8_t keypad[16];
    uint8_t video[VIDEO_WIDTH * VIDEO_HEIGHT];
};


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
            0xF0,0x90,0x90,0x90,0xF0, // 0
            0x20,0x60,0x20,0x20,0x70, // 1
            0xF0,0x10,0xF0,0x80,0xF0, // 2
            0xF0,0x10,0xF0,0x10,0xF0, // 3
            0x90,0x90,0xF0,0x10,0x10, // 4
            0xF0,0x80,0xF0,0x10,0xF0, // 5
            0xF0,0x80,0xF0,0x90,0xF0, // 6
            0xF0,0x10,0x20,0x40,0x40, // 7
            0xF0,0x90,0xF0,0x90,0xF0, // 8
            0xF0,0x90,0xF0,0x10,0xF0, // 9
            0xF0,0x90,0xF0,0x90,0x90, // A
            0xE0,0x90,0xE0,0x90,0xE0, // B
            0xF0,0x80,0x80,0x80,0xF0, // C
            0xE0,0x90,0x90,0x90,0xE0, // D
            0xF0,0x80,0xF0,0x80,0xF0, // E
            0xF0,0x80,0xF0,0x80,0x80  // F
        };
        std::memcpy(memory + 0x050, fontset, sizeof(fontset));
    }

    bool loadROM(const std::string& path, size_t& romSize) {
        std::ifstream file(path, std::ios::binary | std::ios::ate);
        if (!file.is_open()) return false;
        std::streamsize size = file.tellg();
        file.seekg(0, std::ios::beg);
        if (size <= 0 || size > (4096 - 0x200)) return false;
        std::vector<char> buffer(size);
        if (!file.read(buffer.data(), size)) return false;
        std::memset(memory + 0x200, 0, 4096 - 0x200);
        for (size_t i = 0; i < buffer.size(); ++i) memory[0x200 + i] = static_cast<uint8_t>(buffer[i]);
        romSize = buffer.size();
        pc = 0x200;
        sp = 0;
        return true;
    }

    void cycle(std::mt19937& rng) {
        // fetch
        uint16_t opcode = (memory[pc] << 8) | memory[pc + 1];
        pc += 2;
        executeOpcode(opcode, rng);
    }

    // Expose a safe fetch for UI disassembly
    uint16_t peekOpcode(uint16_t addr) const {
        if (addr + 1 >= 4096) return 0;
        return (memory[addr] << 8) | memory[addr + 1];
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
                    // underflow - ignore (robustness)
                    pc = 0;
                }
                else {
                    --sp;
                    pc = stack[sp];
                }
                break;
            default:
                // SYS addr - ignored on modern interpreters
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
            else {
                // stack overflow - ignore call to prevent memory corruption
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
                V[0xF] = V[x] > V[y] ? 1 : 0;
                V[x] = static_cast<uint8_t>(V[x] - V[y]);
                break;
            case 0x6:
                // by convention many modern interpreters store LSB of Vx into VF then shift Vx >> 1
                V[0xF] = V[x] & 0x1;
                V[x] >>= 1;
                break;
            case 0x7:
                V[0xF] = V[y] > V[x] ? 1 : 0;
                V[x] = static_cast<uint8_t>(V[y] - V[x]);
                break;
            case 0xE:
                // shift left, store MSB in VF
                V[0xF] = (V[x] & 0x80) >> 7;
                V[x] <<= 1;
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
            uint8_t vx = V[x], vy = V[y];
            V[0xF] = 0;
            for (int row = 0; row < n; row++) {
                uint8_t sprite = memory[I + row];
                for (int col = 0; col < 8; col++) {
                    if (sprite & (0x80 >> col)) {
                        int px = (vx + col) % VIDEO_WIDTH;
                        int py = (vy + row) % VIDEO_HEIGHT;
                        int idx = px + py * VIDEO_WIDTH;
                        if (video[idx]) V[0xF] = 1; // collision if pixel will be erased
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
                if (keypad[V[x]]) pc += 2;
                break;
            case 0xA1: // SKNP Vx
                if (!keypad[V[x]]) pc += 2;
                break;
            default:
                break;
            }
            break;

        case 0xF000:
            switch (kk) {
            case 0x07: V[x] = delayTimer; break;
            case 0x0A: { // LD Vx, K - blocking key press
                bool pressed = false;
                for (int i = 0; i < 16; ++i) {
                    if (keypad[i]) {
                        V[x] = i;
                        pressed = true;
                        break;
                    }
                }
                if (!pressed) {
                    // Decrement PC so that this instruction repeats until a key is pressed
                    pc -= 2;
                }
                break;
            }
            case 0x15: delayTimer = V[x]; break;
            case 0x18: soundTimer = V[x]; break;
            case 0x1E: I = static_cast<uint16_t>(I + V[x]); break;
            case 0x29: // LD F, Vx - point I to sprite for digit Vx
                I = static_cast<uint16_t>(0x050 + V[x] * 5);
                break;
            case 0x33: // BCD
                memory[I] = V[x] / 100;
                memory[I + 1] = (V[x] / 10) % 10;
                memory[I + 2] = V[x] % 10;
                break;
            case 0x55: // LD [I], V0..Vx
                for (int i = 0; i <= x; ++i) memory[I + i] = V[i];
                // Many original implementations increment I by x+1:
                I = static_cast<uint16_t>(I + x + 1);
                break;
            case 0x65: // LD V0..Vx, [I]
                for (int i = 0; i <= x; ++i) V[i] = memory[I + i];
                // Many original implementations increment I by x+1:
                I = static_cast<uint16_t>(I + x + 1);
                break;
            default:
                break;
            }
            break;

        default:
            // unknown opcode - ignore
            break;
        }

        // Timers are handled externally in the main loop at 60Hz.
    }
};

// Utility: hex dump pretty print (small, used in UI)
std::string formatHex(const uint8_t* data, size_t size, size_t baseAddr = 0) {
    std::ostringstream oss;
    oss << std::hex << std::uppercase;
    for (size_t i = 0; i < size; ++i) {
        if (i % 16 == 0) {
            if (i) oss << "\n";
            oss << std::setw(4) << std::setfill('0') << (baseAddr + i) << ": ";
        }
        oss << std::setw(2) << std::setfill('0') << (int)data[i] << " ";
    }
    return oss.str();
}

// Convert monochrome video (0/1) to RGBA bytes for OpenGL
static void uploadVideoAsRGBA(const uint8_t* mono, GLuint texID) {
    static std::vector<uint32_t> rgba;
    rgba.resize(VIDEO_WIDTH * VIDEO_HEIGHT);
    for (size_t i = 0; i < VIDEO_WIDTH * VIDEO_HEIGHT; ++i) {
        uint8_t v = mono[i];
        // white pixel if v==1, else transparent/black
        // ARGB = 0xAARRGGBB, but glTexImage2D using GL_RGBA with GL_UNSIGNED_BYTE expects RGBA in memory as 0xRRGGBBAA little-endian.
        // We'll use 0xFF for alpha and R=G=B=255 for set pixels, else 0x00.
        uint32_t px = v ? 0xFFFFFFFFu : 0x000000FFu; // white with alpha if set; black with alpha 255 if not set
        // Note: depending on GL unpack, this often maps ok; if colors invert on your platform, swap channels here.
        rgba[i] = px;
    }

    glBindTexture(GL_TEXTURE_2D, texID);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, VIDEO_WIDTH, VIDEO_HEIGHT, 0, GL_RGBA, GL_UNSIGNED_BYTE, rgba.data());
}




// Opcode decode for UI
static std::string decodeOpcode(uint16_t opcode) {
    std::ostringstream ss;
    ss << std::hex << std::uppercase << std::setfill('0');
    ss << "0x" << std::setw(4) << opcode << "  ";

    uint16_t nnn = opcode & 0x0FFF;
    uint8_t n = opcode & 0x000F;
    uint8_t x = (opcode & 0x0F00) >> 8;
    uint8_t y = (opcode & 0x00F0) >> 4;
    uint8_t kk = opcode & 0x00FF;

    switch (opcode & 0xF000) {
    case 0x0000:
        if (opcode == 0x00E0) ss << "CLS";
        else if (opcode == 0x00EE) ss << "RET";
        else ss << "SYS " << std::setw(3) << nnn;
        break;
    case 0x1000: ss << "JP " << std::setw(3) << nnn; break;
    case 0x2000: ss << "CALL " << std::setw(3) << nnn; break;
    case 0x3000: ss << "SE V" << std::hex << (int)x << ", 0x" << std::setw(2) << (int)kk; break;
    case 0x4000: ss << "SNE V" << std::hex << (int)x << ", 0x" << std::setw(2) << (int)kk; break;
    case 0x5000: ss << "SE V" << std::hex << (int)x << ", V" << (int)y; break;
    case 0x6000: ss << "LD V" << std::hex << (int)x << ", 0x" << std::setw(2) << (int)kk; break;
    case 0x7000: ss << "ADD V" << std::hex << (int)x << ", 0x" << std::setw(2) << (int)kk; break;
    case 0x8000:
        switch (n) {
        case 0x0: ss << "LD V" << std::hex << (int)x << ", V" << (int)y; break;
        case 0x1: ss << "OR V" << std::hex << (int)x << ", V" << (int)y; break;
        case 0x2: ss << "AND V" << std::hex << (int)x << ", V" << (int)y; break;
        case 0x3: ss << "XOR V" << std::hex << (int)x << ", V" << (int)y; break;
        case 0x4: ss << "ADD V" << std::hex << (int)x << ", V" << (int)y; break;
        case 0x5: ss << "SUB V" << std::hex << (int)x << ", V" << (int)y; break;
        case 0x6: ss << "SHR V" << std::hex << (int)x; break;
        case 0x7: ss << "SUBN V" << std::hex << (int)x << ", V" << (int)y; break;
        case 0xE: ss << "SHL V" << std::hex << (int)x; break;
        default: ss << "UNKNOWN"; break;
        }
        break;
    case 0x9000: ss << "SNE V" << std::hex << (int)x << ", V" << (int)y; break;
    case 0xA000: ss << "LD I, " << std::setw(3) << nnn; break;
    case 0xB000: ss << "JP V0, " << std::setw(3) << nnn; break;
    case 0xC000: ss << "RND V" << std::hex << (int)x << ", 0x" << std::setw(2) << (int)kk; break;
    case 0xD000: ss << "DRW V" << std::hex << (int)x << ", V" << (int)y << ", " << std::dec << (int)n; break;
    case 0xE000:
        if (kk == 0x9E) ss << "SKP V" << std::hex << (int)x;
        else if (kk == 0xA1) ss << "SKNP V" << std::hex << (int)x;
        else ss << "UNKNOWN E";
        break;
    case 0xF000:
        switch (kk) {
        case 0x07: ss << "LD V" << std::hex << (int)x << ", DT"; break;
        case 0x0A: ss << "LD V" << std::hex << (int)x << ", K"; break;
        case 0x15: ss << "LD DT, V" << std::hex << (int)x; break;
        case 0x18: ss << "LD ST, V" << std::hex << (int)x; break;
        case 0x1E: ss << "ADD I, V" << std::hex << (int)x; break;
        case 0x29: ss << "LD F, V" << std::hex << (int)x; break;
        case 0x33: ss << "LD B, V" << std::hex << (int)x; break;
        case 0x55: ss << "LD [I], V0..V" << std::hex << (int)x; break;
        case 0x65: ss << "LD V0..V" << std::hex << (int)x << ", [I]"; break;
        default: ss << "UNKNOWN F"; break;
        }
        break;
    default:
        ss << "DATA/UNKNOWN";
        break;
    }

    return ss.str();
}

void saveState(const Chip8& c, std::vector<Chip8State>& hist, size_t maxHist) {
    Chip8State s;
    memcpy(s.memory, c.memory, sizeof(c.memory));
    memcpy(s.V, c.V, sizeof(c.V));
    s.I = c.I;
    s.pc = c.pc;
    memcpy(s.stack, c.stack, sizeof(c.stack));
    s.sp = c.sp;
    s.delayTimer = c.delayTimer;
    s.soundTimer = c.soundTimer;
    memcpy(s.keypad, c.keypad, sizeof(c.keypad));
    memcpy(s.video, c.video, sizeof(c.video));

    hist.push_back(s);
    if (hist.size() > maxHist) {
        hist.erase(hist.begin()); // use parameter 'hist'
    }
}

void restoreState(Chip8& c, std::vector<Chip8State>& hist) {
    if (hist.empty()) return;
    Chip8State s = hist.back();
    // copy from snapshot into emulator
    memcpy(c.memory, s.memory, sizeof(c.memory));
    memcpy(c.V, s.V, sizeof(c.V));
    c.I = s.I;
    c.pc = s.pc;
    memcpy(c.stack, s.stack, sizeof(c.stack));
    c.sp = s.sp;
    c.delayTimer = s.delayTimer;
    c.soundTimer = s.soundTimer;
    memcpy(c.keypad, s.keypad, sizeof(c.keypad));
    memcpy(c.video, s.video, sizeof(c.video));
    hist.pop_back(); // use parameter 'hist'
    c.drawFlag = true;
}


int main() {

    if (!glfwInit()) {
        std::cerr << "Failed to init GLFW\n";
        return -1;
    }
    // Create window
    GLFWwindow* window = glfwCreateWindow(1024, 768, "CHIP-8 Debugger", nullptr, nullptr);
    glfwMaximizeWindow(window);
    if (!window) {
        std::cerr << "Failed to create GLFW window\n";
        glfwTerminate();
        return -1;
    }
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1); // vsync

    // ImGui init
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = "chip8_debugger.ini"; // remember user window positions

    ImGui::StyleColorsDark();
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 130");

    // RNG
    std::random_device rd;
    std::mt19937 rng(rd());

    // Chip8 instance
    Chip8 chip8;

    std::vector<Chip8State> history;
    const size_t MAX_HISTORY = 1024; // store up to 1024 steps (~2 KB each)



    // Texture
    GLuint texID;
    glGenTextures(1, &texID);
    glBindTexture(GL_TEXTURE_2D, texID);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    // UI & runtime state
    bool showLoadROM = true;
    char romPath[512] = "C:/Users/jojys/Downloads/SpaceInvaders.ch8";
    size_t romSize = 0;

    bool isRunning = false;
    bool stepOnce = false;

    float clockHz = 60.0f; // interpreter cycles per second when running
    int clockHzInt = static_cast<int>(clockHz);

    // Timing accumulators
    using clock = std::chrono::high_resolution_clock;
    auto lastTime = clock::now();
    double acc = 0.0;       // accumulated time for cycles
    double timerAcc = 0.0;  // accumulated time for 60Hz timers
    const double timerTick = 1.0 / 60.0;

    // Track last PC for scrolling highlight
    uint16_t lastPC = 0xFFFF;

    // Main loop
    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();

        // Read keypad
        for (int i = 0; i < 16; ++i)
            chip8.keypad[i] = (glfwGetKey(window, keymap[i]) == GLFW_PRESS) ? 1 : 0;

        // ImGui frame
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        // ROM loading UI
        if (showLoadROM) {
            ImGui::Begin("Load ROM", &showLoadROM);
            ImGui::InputText("ROM Path", romPath, sizeof(romPath));
            if (ImGui::Button("Load")) {
                size_t newSize = 0;
                if (chip8.loadROM(romPath, newSize)) {
                    romSize = newSize;
                    showLoadROM = false;
                    isRunning = false;
                    stepOnce = false;
                }
                else {
                    ImGui::OpenPopup("Load Error");
                }
            }
            if (ImGui::BeginPopupModal("Load Error", NULL, ImGuiWindowFlags_AlwaysAutoResize)) {
                ImGui::Text("Failed to load ROM. Check path and file size.");
                if (ImGui::Button("OK")) ImGui::CloseCurrentPopup();
                ImGui::EndPopup();
            }
            ImGui::End();
        }

        // Timing
        auto now = clock::now();
        double dt = std::chrono::duration<double>(now - lastTime).count();
        lastTime = now;
        acc += dt;
        timerAcc += dt;

        // Determine how many cycles to run
        int cyclesToRun = 0;
        if (isRunning) {
            cyclesToRun = static_cast<int>(acc * clockHz);
        }
        else if (stepOnce) {
            cyclesToRun = 1;  // always run one cycle for step
        }

        // Execute cycles
        if (cyclesToRun > 0) {
            for (int c = 0; c < cyclesToRun; ++c)
                saveState(chip8, history, MAX_HISTORY);
                chip8.cycle(rng);

            acc -= cyclesToRun / clockHz;
            stepOnce = false;
        }

        // Update 60Hz timers
        while (timerAcc >= timerTick) {
            if (chip8.delayTimer > 0) --chip8.delayTimer;
            if (chip8.soundTimer > 0) --chip8.soundTimer;
            timerAcc -= timerTick;
        }

        // Update texture if needed
        if (chip8.drawFlag) {
            chip8.drawFlag = false;
            uploadVideoAsRGBA(chip8.video, texID);
        }

        // ---- UI: Screen ----
        ImGui::Begin("Screen");
        ImVec2 avail = ImGui::GetContentRegionAvail();
        float pixelScale = std::floor(std::min(avail.x / VIDEO_WIDTH, avail.y / VIDEO_HEIGHT));
        if (pixelScale < 1.0f) pixelScale = 1.0f;
        ImGui::Image((void*)(intptr_t)texID, ImVec2(VIDEO_WIDTH * pixelScale, VIDEO_HEIGHT * pixelScale));
        ImGui::End();

        // ---- UI: Debugger ----
        ImGui::Begin("Debugger");
        if (ImGui::Button(isRunning ? "Pause" : "Run")) isRunning = !isRunning;
        ImGui::SameLine();
        if (ImGui::Button("Step")) stepOnce = true;

        ImGui::SameLine();
        if (ImGui::Button("Step Back")) {
            restoreState(chip8, history);
            isRunning = false;
        }
        ImGui::SameLine();
        if (ImGui::Button("Reset")) {
            chip8.reset();
            romSize = 0;
            isRunning = false;
            showLoadROM = true;
            lastPC = 0xFFFF;
        }

        // Clock slider
        if (ImGui::SliderFloat("Clock (Hz)", &clockHz, 0.0f, 2000.0f)) clockHzInt = (int)clockHz;
        ImGui::SameLine();
        if (ImGui::InputInt("##clockint", &clockHzInt)) {
            //if (clockHzInt < 60) clockHzInt = 60;
            if (clockHzInt > 2000) clockHzInt = 2000;
            clockHz = (float)clockHzInt;
        }

        static float scaleFactor = 1.0f;
        ImGui::SliderFloat("UI Scale", &scaleFactor, 0.5f, 2.0f, "%.2f");
        ImGuiIO& io = ImGui::GetIO();
        io.FontGlobalScale = scaleFactor;

        // Registers
        ImGui::Text("Registers:");
        ImGui::Columns(2, nullptr, false);
        for (int i = 0; i < 16; ++i) {
            ImGui::Text("V%X: 0x%02X", i, chip8.V[i]);
            ImGui::NextColumn();
        }
        ImGui::Columns(1);
        ImGui::Text("I: 0x%04X  PC: 0x%04X  SP: %d", chip8.I, chip8.pc, chip8.sp);
        ImGui::Text("Delay: %d  Sound: %d", chip8.delayTimer, chip8.soundTimer);

        // Current opcode
        uint16_t curOp = chip8.peekOpcode(chip8.pc);
        ImGui::Separator();
        ImGui::Text("Current Opcode:");
        ImGui::Text("%s", decodeOpcode(curOp).c_str());
        ImGui::End();

        // ---- UI: Disassembly ----
        ImGui::Begin("Disassembly");

        if (romSize > 0) {
            uint16_t start = 0x200;
            uint16_t end = static_cast<uint16_t>(0x200 + ((romSize + 1) & ~1)); // round up to even

            // Child region with vertical scrollbar
            ImGui::BeginChild("DisasmChild", ImVec2(0, 0), false, ImGuiWindowFlags_AlwaysVerticalScrollbar);

            // Static variable to track last PC for scrolling
            static uint16_t lastPC = 0xFFFF;

            for (uint16_t addr = start; addr + 1 < end; addr += 2) {
                uint16_t op = chip8.peekOpcode(addr);
                std::string dec = decodeOpcode(op);

                // Highlight current opcode if PC is within this 2-byte opcode
                if (chip8.pc >= addr && chip8.pc < addr + 2) {
                    ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f), "-> %04X: %s", addr, dec.c_str());

                    // Auto-scroll to PC only when it changes
                    if (chip8.pc != lastPC) {
                        ImGui::SetScrollHereY(0.5f);
                        lastPC = chip8.pc;
                    }
                }
                else {
                    ImGui::Text("   %04X: %s", addr, dec.c_str());
                }
            }

            ImGui::EndChild();
        }
        else {
            ImGui::TextWrapped("No ROM loaded.");
        }

        ImGui::End();


        //// ---- UI: Memory & Stack ----
        //ImGui::Begin("Memory (first 4096 bytes)");
        //ImGui::TextWrapped("%s", formatHex(chip8.memory, 4096, 0).c_str());
        //ImGui::End();

       // ---- UI: Memory & Stack ----
        static uint8_t prevMemory[4096] = { 0 };
        static bool changed[4096] = { 0 };  // tracks changed bytes

        ImGui::Begin("Memory (first 4096 bytes)");

        // Print memory in rows of 16 bytes
        for (int i = 0; i < 4096; i += 16) {
            ImGui::Text("%04X: ", i);
            ImGui::SameLine(60); // offset for bytes

            for (int j = 0; j < 16 && (i + j) < 4096; ++j) {
                int idx = i + j;
                uint8_t val = chip8.memory[idx];

                // Mark as changed if it differs from previous memory
                if (val != prevMemory[idx]) {
                    changed[idx] = true;
                }

                ImVec4 color = changed[idx] ? ImVec4(1.0f, 0.0f, 0.0f, 1.0f)  // red if changed
                    : ImVec4(1.0f, 1.0f, 1.0f, 1.0f); // white if not

                ImGui::SameLine();
                ImGui::TextColored(color, "%02X", val);
            }
        }

        // Copy current memory to prevMemory
        memcpy(prevMemory, chip8.memory, 4096);

        ImGui::End();




        ImGui::Begin("Stack");
        for (int i = 0; i < 16; ++i) ImGui::Text("%02d: 0x%04X", i, chip8.stack[i]);
        ImGui::End();

        // Render
        ImGui::Render();
        int display_w, display_h;
        glfwGetFramebufferSize(window, &display_w, &display_h);
        glViewport(0, 0, display_w, display_h);
        glClearColor(0.12f, 0.12f, 0.12f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        glfwSwapBuffers(window);
    }


    // Cleanup
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    glfwDestroyWindow(window);
    glfwTerminate();

    return 0;
}
