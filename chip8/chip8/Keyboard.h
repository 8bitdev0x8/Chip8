#pragma once

#include <cstdint>

struct GLFWwindow;

namespace keyboard {
void poll(GLFWwindow* window, uint8_t keypad[16]);
} // namespace keyboard
