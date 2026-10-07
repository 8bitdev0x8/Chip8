// chip8_debugger.cpp
// Full CHIP-8 with ImGui debugger: run/pause/step, opcode viewer, disassembly highlighting.
// Requires GLFW, ImGui (imgui_impl_glfw.cpp, imgui_impl_opengl3.cpp) and OpenGL.

#include <iostream>
#include <fstream>
#include <vector>
#include <deque>
#include <chrono>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <thread>
#include <iomanip>
#include <sstream>
#include <random>
#ifndef NOMINMAX
#define NOMINMAX
#endif
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>
#include <commdlg.h>
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include "Chip8.h"
#include "Display.h"
#include "Keyboard.h"

using namespace chip8;

constexpr int MAX_CPU_HZ = 1000;



bool browseForROM(GLFWwindow* window, char* path, size_t pathCapacity) {
    if (pathCapacity == 0) return false;

    char selectedPath[512] = {};
    OPENFILENAMEA dialog{};
    dialog.lStructSize = sizeof(dialog);
    dialog.hwndOwner = glfwGetWin32Window(window);
    dialog.lpstrFilter =
        "CHIP-8 ROMs (*.ch8;*.c8;*.rom)\0*.ch8;*.c8;*.rom\0"
        "All files (*.*)\0*.*\0";
    dialog.lpstrFile = selectedPath;
    dialog.nMaxFile = static_cast<DWORD>(sizeof(selectedPath));
    dialog.lpstrDefExt = "ch8";
    dialog.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;

    if (!GetOpenFileNameA(&dialog)) return false;

    const size_t selectedLength = std::strlen(selectedPath);
    if (selectedLength + 1 > pathCapacity) return false;
    std::memcpy(path, selectedPath, selectedLength + 1);
    return true;
}
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

int main() {

    if (!glfwInit()) {
        std::cerr << "Failed to init GLFW\n";
        return -1;
    }
    // Create window
    GLFWwindow* window = glfwCreateWindow(1024, 768, "CHIP-8 Debugger", nullptr, nullptr);
    if (!window) {
        std::cerr << "Failed to create GLFW window\n";
        glfwTerminate();
        return -1;
    }
    glfwMaximizeWindow(window);
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

    std::deque<Chip8State> history;
    const size_t MAX_HISTORY = 1024; // roughly 6 MiB at the current snapshot size



    // Texture
    GLuint texID;
    glGenTextures(1, &texID);
    glBindTexture(GL_TEXTURE_2D, texID);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    // UI & runtime state
    char romPath[512] = "";
    size_t romSize = 0;
    std::vector<uint16_t> disassemblyOpcodes;
    std::vector<std::string> disassemblyText;

    bool isRunning = false;
    bool stepOnce = false;

    // CHIP-8 timers tick at 60 Hz; games typically need several hundred
    // instructions per second. 700 Hz is a useful general-purpose default.
    float clockHz = 700.0f;
    int clockHzInt = static_cast<int>(clockHz);

    // Timing accumulators
    using clock = std::chrono::high_resolution_clock;
    auto lastTime = clock::now();
    double acc = 0.0;       // accumulated time for cycles
    double timerAcc = 0.0;  // accumulated time for 60Hz timers
    const double timerTick = 1.0 / 60.0;

    // Track last PC for scrolling highlight
    uint16_t lastPC = 0xFFFF;
    bool resetWindowLayout = false;
    bool showROMLoadError = false;
    bool showAbout = false;

    auto loadCurrentROM = [&]() {
        size_t newSize = 0;
        if (!chip8.loadROM(romPath, newSize)) return false;
        romSize = newSize;
        const size_t instructionCount = (romSize + 1) / 2;
        disassemblyOpcodes.resize(instructionCount);
        disassemblyText.resize(instructionCount);
        for (size_t row = 0; row < instructionCount; ++row) {
            const uint16_t addr = static_cast<uint16_t>(0x200 + row * 2);
            disassemblyOpcodes[row] = chip8.peekOpcode(addr);
            disassemblyText[row] = decodeOpcode(disassemblyOpcodes[row]);
        }
        history.clear();
        isRunning = false;
        stepOnce = false;
        acc = 0.0;
        timerAcc = 0.0;
        return true;
    };

    // Main loop
    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();

        // Poll the physical keyboard and update the CHIP-8 keypad.
        keyboard::poll(window, chip8.keypad);

        // ImGui frame
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        if (ImGui::BeginMainMenuBar()) {
            if (ImGui::BeginMenu("File")) {
                if (ImGui::MenuItem("Open ROM...")) {
                    if (browseForROM(window, romPath, sizeof(romPath)) && !loadCurrentROM()) {
                        showROMLoadError = true;
                    }
                }
                ImGui::EndMenu();
            }
            if (ImGui::BeginMenu("Window")) {
                if (ImGui::MenuItem("Reset Window Positions")) resetWindowLayout = true;
                ImGui::EndMenu();
            }
            if (ImGui::BeginMenu("Help")) {
                if (ImGui::MenuItem("About")) showAbout = true;
                ImGui::EndMenu();
            }
            ImGui::EndMainMenuBar();
        }

        const ImVec2 displaySize = ImGui::GetIO().DisplaySize;

        // First-run layout: screen and memory on the left, CPU controls and
        // stack in the center, and disassembly on the right. ImGui's ini file
        // keeps any layout the user changes after this initial arrangement.
        const float margin = 8.0f;
        const float gap = 8.0f;
        const float layoutWidth = displaySize.x - 2.0f * margin - 2.0f * gap;
        const float menuHeight = ImGui::GetFrameHeight();
        const float layoutHeight = displaySize.y - 2.0f * margin - gap - menuHeight;
        const ImGuiCond layoutCondition = resetWindowLayout ? ImGuiCond_Always : ImGuiCond_FirstUseEver;
        const float leftWidth = layoutWidth * 0.37f;
        const float centerWidth = layoutWidth * 0.34f;
        const float rightWidth = layoutWidth - leftWidth - centerWidth;
        const float leftTopHeight = layoutHeight * 0.50f;
        const float centerTopHeight = layoutHeight * 0.64f;
        const float leftX = margin;
        const float centerX = leftX + leftWidth + gap;
        const float rightX = centerX + centerWidth + gap;
        const float topY = margin + menuHeight;
        const float bottomY = topY + leftTopHeight + gap;
        const float bottomHeight = layoutHeight - leftTopHeight;
        const float stackY = topY + centerTopHeight + gap;
        const float stackHeight = layoutHeight - centerTopHeight;

        // Timing
        auto now = clock::now();
        double dt = std::chrono::duration<double>(now - lastTime).count();
        lastTime = now;
        dt = std::clamp(dt, 0.0, 0.25); // bound catch-up after a pause or stalled frame
        // Determine how many cycles to run
        int cyclesToRun = 0;
        if (isRunning) {
            timerAcc += dt;
            if (clockHz > 0.0f) {
                acc += dt;
                cyclesToRun = static_cast<int>(acc * clockHz);
            }
        }
        else if (stepOnce) {
            cyclesToRun = 1;  // always run one cycle for step
        }

        // Execute cycles
        if (cyclesToRun > 0) {
            for (int c = 0; c < cyclesToRun; ++c) {
                saveState(chip8, history, MAX_HISTORY);
                if (!chip8.cycle(rng)) {
                    isRunning = false;
                    break;
                }
            }

            if (clockHz > 0.0f) acc = std::max(0.0, acc - cyclesToRun / clockHz);
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
            display::uploadVideoAsRGBA(chip8.video, texID);
        }

        // ---- UI: Screen ----
        ImGui::SetNextWindowPos(ImVec2(leftX, topY), layoutCondition);
        ImGui::SetNextWindowSize(ImVec2(leftWidth, leftTopHeight), layoutCondition);
        ImGui::Begin("Screen");
        ImVec2 avail = ImGui::GetContentRegionAvail();
        float pixelScale = std::floor(std::min(avail.x / VIDEO_WIDTH, avail.y / VIDEO_HEIGHT));
        if (pixelScale < 1.0f) pixelScale = 1.0f;
        ImGui::Image((ImTextureID)(intptr_t)texID, ImVec2(VIDEO_WIDTH * pixelScale, VIDEO_HEIGHT * pixelScale));
        ImGui::End();

        // ---- UI: Debugger ----
        ImGui::SetNextWindowPos(ImVec2(centerX, topY), layoutCondition);
        ImGui::SetNextWindowSize(ImVec2(centerWidth, centerTopHeight), layoutCondition);
        ImGui::Begin("Debugger");
        if (showAbout) {
            ImGui::OpenPopup("About CHIP-8 Debugger");
            showAbout = false;
        }
        if (ImGui::BeginPopupModal("About CHIP-8 Debugger", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::TextUnformatted("CHIP-8 Debugger");
            ImGui::Separator();
            ImGui::TextWrapped("Created for learning emulation & low-level systems by 8bitdev0x8.");
            if (ImGui::Button("Close")) ImGui::CloseCurrentPopup();
            ImGui::EndPopup();
        }
        ImGui::Text("ROM");
        ImGui::SetNextItemWidth(-1.0f);
        ImGui::InputText("ROM Path", romPath, sizeof(romPath));
        if (ImGui::Button("Load")) {
            if (!loadCurrentROM()) showROMLoadError = true;
        }
        ImGui::SameLine();
        if (ImGui::Button("Browse...")) browseForROM(window, romPath, sizeof(romPath));
        if (showROMLoadError) {
            ImGui::OpenPopup("Load Error");
            showROMLoadError = false;
        }
        if (ImGui::BeginPopupModal("Load Error", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::Text("Failed to load ROM. Check path and file size.");
            if (ImGui::Button("OK")) ImGui::CloseCurrentPopup();
            ImGui::EndPopup();
        }
        ImGui::Separator();
        if (romSize > 0) {
            if (ImGui::Button(isRunning ? "Pause" : "Run")) isRunning = !isRunning;
            ImGui::SameLine();
            if (ImGui::Button("Step")) stepOnce = true;
        }
        else {
            ImGui::TextDisabled("Load a ROM to run or step.");
        }

        ImGui::SameLine();
        if (ImGui::Button("Step Back")) {
            restoreState(chip8, history);
            isRunning = false;
        }
        ImGui::SameLine();
        if (ImGui::Button("Reset")) {
            chip8.reset();
            romSize = 0;
            history.clear();
            isRunning = false;
            stepOnce = false;
            acc = 0.0;
            timerAcc = 0.0;
            lastPC = 0xFFFF;
        }

        // Clock slider
        if (ImGui::SliderFloat("Clock (Hz)", &clockHz, 0.0f, static_cast<float>(MAX_CPU_HZ)))
            clockHzInt = static_cast<int>(clockHz);
        ImGui::SameLine();
        if (ImGui::InputInt("##clockint", &clockHzInt)) {
            clockHzInt = std::clamp(clockHzInt, 0, MAX_CPU_HZ);
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
        ImGui::Separator();
        ImGui::Text("CHIP-8 keypad: 123C / 456D / 789E / A0BF");
        ImGui::Text("Keyboard:      1234 / QWER / ASDF / ZXCV");
        ImGui::TextDisabled("Run/Pause, Step, Step Back, and Reset are above.");
        ImGui::End();

        // ---- UI: Disassembly ----
        ImGui::SetNextWindowPos(ImVec2(rightX, topY), layoutCondition);
        ImGui::SetNextWindowSize(ImVec2(rightWidth, layoutHeight), layoutCondition);
        ImGui::Begin("Disassembly");

        if (romSize > 0) {
            uint16_t start = 0x200;
            uint16_t end = static_cast<uint16_t>(0x200 + ((romSize + 1) & ~1)); // round up to even

            // Child region with vertical scrollbar
            ImGui::BeginChild("DisasmChild", ImVec2(0, 0), false, ImGuiWindowFlags_AlwaysVerticalScrollbar);

            // Static variable to track last PC for scrolling
            static uint16_t lastPC = 0xFFFF;

            const int instructionCount = (end - start) / 2;
            if (chip8.pc >= start && chip8.pc < end && chip8.pc != lastPC) {
                const int currentRow = (chip8.pc - start) / 2;
                ImGui::SetScrollY(currentRow * ImGui::GetTextLineHeightWithSpacing());
                lastPC = chip8.pc;
            }
            ImGuiListClipper clipper;
            clipper.Begin(instructionCount);
            while (clipper.Step()) {
                for (int row = clipper.DisplayStart; row < clipper.DisplayEnd; ++row) {
                    const uint16_t addr = static_cast<uint16_t>(start + row * 2);
                    const uint16_t op = chip8.peekOpcode(addr);
                    if (disassemblyOpcodes[row] != op) {
                        disassemblyOpcodes[row] = op;
                        disassemblyText[row] = decodeOpcode(op);
                    }
                    const std::string& dec = disassemblyText[row];

                    if (chip8.pc >= addr && chip8.pc < addr + 2) {
                        ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f), "-> %04X: %s", addr, dec.c_str());
                    } else {
                        ImGui::Text("   %04X: %s", addr, dec.c_str());
                    }
                }
            }
            clipper.End();

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
        ImGui::SetNextWindowPos(ImVec2(leftX, bottomY), layoutCondition);
        ImGui::SetNextWindowSize(ImVec2(leftWidth, bottomHeight), layoutCondition);
        ImGui::Begin("Memory (first 4096 bytes)");

        // Highlight the address range occupied by the loaded ROM.
        ImGui::BeginChild("MemoryChild", ImVec2(0, 0), false, ImGuiWindowFlags_AlwaysVerticalScrollbar);
        ImGuiListClipper memoryClipper;
        memoryClipper.Begin(4096 / 16);
        while (memoryClipper.Step()) {
            for (int row = memoryClipper.DisplayStart; row < memoryClipper.DisplayEnd; ++row) {
                const int base = row * 16;
                ImGui::Text("%04X:", base);
                for (int col = 0; col < 16; ++col) {
                    const int idx = base + col;
                    const bool isRomAddress = romSize > 0 && idx >= 0x200 && idx < 0x200 + romSize;
                    const ImVec4 color = isRomAddress ? ImVec4(1.0f, 0.0f, 0.0f, 1.0f)
                        : ImVec4(1.0f, 1.0f, 1.0f, 1.0f);
                    ImGui::SameLine();
                    ImGui::TextColored(color, "%02X", chip8.memory[idx]);
                }
            }
        }
        memoryClipper.End();
        ImGui::EndChild();

        ImGui::End();




        ImGui::SetNextWindowPos(ImVec2(centerX, stackY), layoutCondition);
        ImGui::SetNextWindowSize(ImVec2(centerWidth, stackHeight), layoutCondition);
        ImGui::Begin("Stack");
        for (int i = 0; i < 16; ++i) ImGui::Text("%02d: 0x%04X", i, chip8.stack[i]);
        ImGui::End();

        resetWindowLayout = false;

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
