#pragma once

#include <cstdint>
#include <cstddef>

#include "rtl_control.hpp"

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

/* direct: tune 24-28.8 MHz on the tuner's native input instead of the
 * upconverter (esp_rtl_sdr_set_hf_direct_min_hz). Same GPIO/input state the
 * vendor driver uses above 28.8 MHz; the analog passband there is unmeasured. */
constexpr MeasuredV4LFrontendPlan measured_v4l_frontend_plan(uint32_t rf_hz,
                                                              bool bias_on,
                                                              uint8_t reg05_low_bits,
                                                              bool direct = false)
{
    const bool upconverted = rf_hz < 28800000u && !direct;
    const bool hf_gpio = rf_hz <= 28800000u && !direct;
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

/* Patch the existing PLL template at its capture-derived input phases. A
 * false result skips V4-only post-PLL writes, including its reg-06 triplexer. */
inline bool measured_v4l_patch_tune_record(uint32_t rf_hz, size_t index,
                                            RtlControlRecord &rec, bool direct = false)
{
    const auto plan = measured_v4l_frontend_plan(rf_hz, false, 0, direct);
    switch (index) {
    case 0: rec.data[1] = plan.pre17; break;
    case 1: rec.data[1] = plan.pre1a; break;
    case 2: rec.data[1] = plan.pre1b; break;
    case 19:
        if (!plan.post_input) return false;
        rec.data[1] = plan.post1a;
        break;
    case 20:
        if (!plan.post_input) return false;
        rec.data[0] = 0x1b;
        rec.data[1] = plan.post1b;
        break;
    case 21: return false;
    default: break;
    }
    return true;
}
