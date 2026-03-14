#pragma once

#include <cstdint>

namespace chip8_display {

inline constexpr uint32_t kChip8Width = 64u;
inline constexpr uint32_t kChip8Height = 32u;

void init();
void present(const uint32_t* chip8Video);

}