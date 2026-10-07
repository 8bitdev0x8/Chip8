#include "Keyboard.h"

#include <GLFW/glfw3.h>

namespace keyboard {
void poll(GLFWwindow* window, uint8_t keypad[16]) {
    constexpr int keymap[16] = {
        GLFW_KEY_X, GLFW_KEY_1, GLFW_KEY_2, GLFW_KEY_3,
        GLFW_KEY_Q, GLFW_KEY_W, GLFW_KEY_E, GLFW_KEY_A,
        GLFW_KEY_S, GLFW_KEY_D, GLFW_KEY_Z, GLFW_KEY_C,
        GLFW_KEY_4, GLFW_KEY_R, GLFW_KEY_F, GLFW_KEY_V
    };
    for (int i = 0; i < 16; ++i) {
        keypad[i] = (glfwGetKey(window, keymap[i]) == GLFW_PRESS) ? 1 : 0;
    }
}
} // namespace keyboard
