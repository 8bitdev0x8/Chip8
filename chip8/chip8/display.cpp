#include "display.h"

#include <cstring>

#include "pico/stdlib.h"
#include "pico/multicore.h"
#include "hardware/dma.h"
#include "hardware/gpio.h"
#include "hardware/irq.h"
#include "hardware/sync.h"
#include "hardware/structs/hstx_ctrl.h"
#include "hardware/structs/hstx_fifo.h"
#include "hardware/structs/bus_ctrl.h"
#include "hardware/regs/hstx_ctrl.h"

// Some IntelliSense configurations don't expose RP2350-specific GPIO function enums.
#ifndef GPIO_FUNC_HSTX
#define GPIO_FUNC_HSTX 0u
#endif

namespace chip8_display {
namespace {

constexpr uint32_t kFrameWidth = 640u;
constexpr uint32_t kFrameHeight = 480u;
constexpr uint32_t kScale = 8u;
constexpr uint32_t kOffsetX = (kFrameWidth - kChip8Width * kScale) / 2u;
constexpr uint32_t kOffsetY = (kFrameHeight - kChip8Height * kScale) / 2u;

constexpr uint32_t kTmdsCtrl00 = 0x354u;
constexpr uint32_t kTmdsCtrl01 = 0x0abu;
constexpr uint32_t kTmdsCtrl10 = 0x154u;
constexpr uint32_t kTmdsCtrl11 = 0x2abu;

constexpr uint32_t kSyncV0H0 = kTmdsCtrl00 | (kTmdsCtrl00 << 10) | (kTmdsCtrl00 << 20);
constexpr uint32_t kSyncV0H1 = kTmdsCtrl01 | (kTmdsCtrl00 << 10) | (kTmdsCtrl00 << 20);
constexpr uint32_t kSyncV1H0 = kTmdsCtrl10 | (kTmdsCtrl00 << 10) | (kTmdsCtrl00 << 20);
constexpr uint32_t kSyncV1H1 = kTmdsCtrl11 | (kTmdsCtrl00 << 10) | (kTmdsCtrl00 << 20);

constexpr uint32_t kModeHFrontPorch = 16u;
constexpr uint32_t kModeHSyncWidth = 96u;
constexpr uint32_t kModeHBackPorch = 48u;
constexpr uint32_t kModeHActivePixels = 640u;

constexpr uint32_t kModeVFrontPorch = 10u;
constexpr uint32_t kModeVSyncWidth = 2u;
constexpr uint32_t kModeVBackPorch = 33u;
constexpr uint32_t kModeVActiveLines = 480u;

constexpr uint32_t kModeHTotalPixels = kModeHFrontPorch + kModeHSyncWidth + kModeHBackPorch + kModeHActivePixels;
constexpr uint32_t kModeVTotalLines = kModeVFrontPorch + kModeVSyncWidth + kModeVBackPorch + kModeVActiveLines;

constexpr uint32_t kHstxCmdRawRepeat = 0x1u << 12;
constexpr uint32_t kHstxCmdTmds = 0x2u << 12;
constexpr uint32_t kHstxCmdNop = 0xfu << 12;

uint8_t framebuffer[kFrameWidth * kFrameHeight]{};

uint32_t vblankLineVsyncOff[] = {
    kHstxCmdRawRepeat | kModeHFrontPorch,
    kSyncV1H1,
    kHstxCmdRawRepeat | kModeHSyncWidth,
    kSyncV1H0,
    kHstxCmdRawRepeat | (kModeHBackPorch + kModeHActivePixels),
    kSyncV1H1,
    kHstxCmdNop
};

uint32_t vblankLineVsyncOn[] = {
    kHstxCmdRawRepeat | kModeHFrontPorch,
    kSyncV0H1,
    kHstxCmdRawRepeat | kModeHSyncWidth,
    kSyncV0H0,
    kHstxCmdRawRepeat | (kModeHBackPorch + kModeHActivePixels),
    kSyncV0H1,
    kHstxCmdNop
};

uint32_t vactiveLine[] = {
    kHstxCmdRawRepeat | kModeHFrontPorch,
    kSyncV1H1,
    kHstxCmdNop,
    kHstxCmdRawRepeat | kModeHSyncWidth,
    kSyncV1H0,
    kHstxCmdNop,
    kHstxCmdRawRepeat | kModeHBackPorch,
    kSyncV1H1,
    kHstxCmdTmds | kModeHActivePixels
};

int dmaChanPing = -1;
int dmaChanPong = -1;
bool dmaPong = false;
uint32_t vScanline = 2;
bool vactiveCmdlistPosted = false;

inline uint8_t rgb332(uint8_t red, uint8_t green, uint8_t blue) {
    return static_cast<uint8_t>(((red & 0xC0u) >> 6) | ((green & 0xE0u) >> 3) | (blue & 0xE0u));
}

inline void setPixel(uint32_t x, uint32_t y, bool on) {
    framebuffer[y * kFrameWidth + x] = on ? rgb332(255, 255, 255) : 0;
}

void clearFramebuffer() {
    memset(framebuffer, 0, sizeof(framebuffer));
}

void dma_irq_handler() {
    uint channel = dmaPong ? static_cast<uint>(dmaChanPong) : static_cast<uint>(dmaChanPing);
    dma_channel_hw_t* dmaChannel = &dma_hw->ch[channel];
    dma_hw->intr = 1u << channel;
    dmaPong = !dmaPong;

    if (vScanline >= kModeVFrontPorch && vScanline < (kModeVFrontPorch + kModeVSyncWidth)) {
        dmaChannel->read_addr = reinterpret_cast<uintptr_t>(vblankLineVsyncOn);
        dmaChannel->transfer_count = count_of(vblankLineVsyncOn);
    } else if (vScanline < kModeVFrontPorch + kModeVSyncWidth + kModeVBackPorch) {
        dmaChannel->read_addr = reinterpret_cast<uintptr_t>(vblankLineVsyncOff);
        dmaChannel->transfer_count = count_of(vblankLineVsyncOff);
    } else if (!vactiveCmdlistPosted) {
        dmaChannel->read_addr = reinterpret_cast<uintptr_t>(vactiveLine);
        dmaChannel->transfer_count = count_of(vactiveLine);
        vactiveCmdlistPosted = true;
    } else {
        uint32_t row = vScanline - (kModeVTotalLines - kModeVActiveLines);
        dmaChannel->read_addr = reinterpret_cast<uintptr_t>(&framebuffer[row * kModeHActivePixels]);
        dmaChannel->transfer_count = kModeHActivePixels / sizeof(uint32_t);
        vactiveCmdlistPosted = false;
    }

    if (!vactiveCmdlistPosted) {
        vScanline = (vScanline + 1u) % kModeVTotalLines;
    }
}

void initDvi() {
    hstx_ctrl_hw->expand_tmds =
        (2u  << HSTX_CTRL_EXPAND_TMDS_L2_NBITS_LSB) |
        (0u  << HSTX_CTRL_EXPAND_TMDS_L2_ROT_LSB)   |
        (2u  << HSTX_CTRL_EXPAND_TMDS_L1_NBITS_LSB) |
        (29u << HSTX_CTRL_EXPAND_TMDS_L1_ROT_LSB)   |
        (1u  << HSTX_CTRL_EXPAND_TMDS_L0_NBITS_LSB) |
        (26u << HSTX_CTRL_EXPAND_TMDS_L0_ROT_LSB);

    hstx_ctrl_hw->expand_shift =
        (4u << HSTX_CTRL_EXPAND_SHIFT_ENC_N_SHIFTS_LSB) |
        (8u << HSTX_CTRL_EXPAND_SHIFT_ENC_SHIFT_LSB)    |
        (1u << HSTX_CTRL_EXPAND_SHIFT_RAW_N_SHIFTS_LSB) |
        (0u << HSTX_CTRL_EXPAND_SHIFT_RAW_SHIFT_LSB);

    hstx_ctrl_hw->csr = 0;
    hstx_ctrl_hw->csr =
        HSTX_CTRL_CSR_EXPAND_EN_BITS |
        (5u << HSTX_CTRL_CSR_CLKDIV_LSB) |
        (5u << HSTX_CTRL_CSR_N_SHIFTS_LSB) |
        (2u << HSTX_CTRL_CSR_SHIFT_LSB) |
        HSTX_CTRL_CSR_EN_BITS;

    hstx_ctrl_hw->bit[2] = HSTX_CTRL_BIT0_CLK_BITS;
    hstx_ctrl_hw->bit[3] = HSTX_CTRL_BIT0_CLK_BITS | HSTX_CTRL_BIT0_INV_BITS;
    for (uint lane = 0; lane < 3; ++lane) {
        static const int laneToOutputBit[3] = {0, 6, 4};
        int bit = laneToOutputBit[lane];
        uint32_t laneDataSelect =
            (lane * 10u)     << HSTX_CTRL_BIT0_SEL_P_LSB |
            (lane * 10u + 1) << HSTX_CTRL_BIT0_SEL_N_LSB;
        hstx_ctrl_hw->bit[bit] = laneDataSelect;
        hstx_ctrl_hw->bit[bit + 1] = laneDataSelect | HSTX_CTRL_BIT0_INV_BITS;
    }

    for (int gpio = 12; gpio <= 19; ++gpio) {
        gpio_set_function(static_cast<uint>(gpio), static_cast<gpio_function_t>(GPIO_FUNC_HSTX));
    }

    dmaChanPing = dma_claim_unused_channel(true);
    dmaChanPong = dma_claim_unused_channel(true);

    dma_channel_config config = dma_channel_get_default_config(static_cast<uint>(dmaChanPing));
    channel_config_set_chain_to(&config, static_cast<uint>(dmaChanPong));
    channel_config_set_dreq(&config, DREQ_HSTX);
    dma_channel_configure(static_cast<uint>(dmaChanPing), &config, &hstx_fifo_hw->fifo,
        vblankLineVsyncOff, count_of(vblankLineVsyncOff), false);

    config = dma_channel_get_default_config(static_cast<uint>(dmaChanPong));
    channel_config_set_chain_to(&config, static_cast<uint>(dmaChanPing));
    channel_config_set_dreq(&config, DREQ_HSTX);
    dma_channel_configure(static_cast<uint>(dmaChanPong), &config, &hstx_fifo_hw->fifo,
        vblankLineVsyncOff, count_of(vblankLineVsyncOff), false);

    dma_hw->ints0 = (1u << dmaChanPing) | (1u << dmaChanPong);
    dma_hw->inte0 = (1u << dmaChanPing) | (1u << dmaChanPong);
    irq_set_exclusive_handler(DMA_IRQ_0, dma_irq_handler);
    irq_set_enabled(DMA_IRQ_0, true);

    bus_ctrl_hw->priority = BUSCTRL_BUS_PRIORITY_DMA_W_BITS | BUSCTRL_BUS_PRIORITY_DMA_R_BITS;
    dma_hw->multi_channel_trigger = 1u << static_cast<uint>(dmaChanPing);
}

void core1VideoEntry() {
    initDvi();
    while (true) {
        __wfi();
    }
}

}  // namespace

void init() {
    clearFramebuffer();
    multicore_launch_core1(core1VideoEntry);
    sleep_ms(50);
}

void present(const uint32_t* chip8Video) {
    for (uint32_t y = 0; y < kChip8Height; ++y) {
        for (uint32_t x = 0; x < kChip8Width; ++x) {
            bool on = chip8Video[x + y * kChip8Width] != 0;
            for (uint32_t sy = 0; sy < kScale; ++sy) {
                for (uint32_t sx = 0; sx < kScale; ++sx) {
                    setPixel(kOffsetX + x * kScale + sx, kOffsetY + y * kScale + sy, on);
                }
            }
        }
    }
}

}  // namespace chip8_display