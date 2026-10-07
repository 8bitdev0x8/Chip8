#include "Display.h"

#include "Chip8.h"
#include <GLFW/glfw3.h>
#include <vector>

namespace display {
void uploadVideoAsRGBA(const uint8_t* mono, unsigned int texID) {
    static std::vector<uint32_t> rgba;
    rgba.resize(VIDEO_WIDTH * VIDEO_HEIGHT);
    for (size_t i = 0; i < VIDEO_WIDTH * VIDEO_HEIGHT; ++i) {
        uint8_t v = mono[i];
        // white pixel if v==1, else transparent/black
        // ARGB = 0xAARRGGBB, but glTexImage2D using GL_RGBA with GL_UNSIGNED_BYTE expects RGBA in memory as 0xRRGGBBAA little-endian.
        // We'll use 0xFF for alpha and R=G=B=255 for set pixels, else 0x00.
        uint32_t px = v ? 0xFFFFFFFFu : 0xFF000000u; // opaque white or opaque black (RGBA byte order on Windows)
        // Note: depending on GL unpack, this often maps ok; if colors invert on your platform, swap channels here.
        rgba[i] = px;
    }

    glBindTexture(GL_TEXTURE_2D, texID);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, VIDEO_WIDTH, VIDEO_HEIGHT, 0, GL_RGBA, GL_UNSIGNED_BYTE, rgba.data());
}
} // namespace display
