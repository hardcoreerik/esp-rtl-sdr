#pragma once

#include <cstdint>

/* V4L R828S route from the 2026-09-25 vendor-driver USB capture.
 * This is not V4's R828D three-input triplexer. */
struct MeasuredV4LFrontendPlan {
    uint8_t pre17;
    uint8_t pre1a;
    uint8_t pre1b;
    uint8_t post1a;
    uint8_t post1b;
    uint8_t gpd;
    uint8_t gpoe;
    uint8_t gpo;
    uint8_t reg05;
    bool post_input;
};

constexpr MeasuredV4LFrontendPlan measured_v4l_frontend_plan(uint32_t rf_hz,
                                                              bool bias_on,
                                                              uint8_t reg05_low_bits)
{
    const bool upconverted = rf_hz < 28800000u;
    const bool hf_gpio = rf_hz <= 28800000u;
    return {
        static_cast<uint8_t>(upconverted ? 0x28 : 0x20),
        0x2a,
        static_cast<uint8_t>(upconverted ? 0xdf : 0x34),
        0x68,
        0x00,
        0x06,
        0x39,
        static_cast<uint8_t>((hf_gpio ? 0x18 : 0x38) | (bias_on ? 1 : 0)),
        static_cast<uint8_t>((hf_gpio ? 0xe0 : 0x80) | (reg05_low_bits & 0x1f)),
        upconverted,
    };
}
