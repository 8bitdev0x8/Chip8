#pragma once

#include <cstdint>

namespace display {
void uploadVideoAsRGBA(const uint8_t* mono, unsigned int textureId);
} // namespace display
