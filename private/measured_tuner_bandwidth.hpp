#pragma once

#include "rtl_profile.hpp"

#include <cstddef>
#include <cstdint>

/* Vendor-driver control choices at 2.4 MS/s, not measured analog passbands. */
struct MeasuredTunerBandwidthPlan {
    uint32_t requested_hz;
    uint32_t if_hz;
    uint8_t reg0a;
    uint8_t reg0b;
    uint8_t if19;
    uint8_t if1a;
    uint8_t if1b;
};

constexpr uint32_t kMeasuredNativeBandwidths[] = {
    0u, 200000u, 300000u, 500000u, 1000000u, 1800000u, 2400000u,
};
constexpr uint32_t kMeasuredHfBandwidths[] = {0u, 200000u, 500000u, 2400000u};

constexpr size_t measured_tuner_bandwidth_count(RtlProfileId profile, uint32_t rf_hz,
                                                bool hf_direct = false)
{
    if (profile != RtlProfileId::BlogV4 && profile != RtlProfileId::BlogV4L &&
        profile != RtlProfileId::BlogV3) return 0;
    if (profile == RtlProfileId::BlogV3 && rf_hz < kR820T2NativeMinHz) return 0;
    return profile != RtlProfileId::BlogV3 && rf_hz <= ESP_RTL_SDR_XTAL_HZ && !hf_direct
        ? sizeof(kMeasuredHfBandwidths) / sizeof(uint32_t)
        : sizeof(kMeasuredNativeBandwidths) / sizeof(uint32_t);
}

inline bool measured_tuner_bandwidth_plan(RtlProfileId profile, uint32_t rf_hz,
                                           uint32_t width_hz,
                                           MeasuredTunerBandwidthPlan *out,
                                           bool hf_direct = false)
{
    if (out == nullptr || measured_tuner_bandwidth_count(profile, rf_hz, hf_direct) == 0)
        return false;
    const bool hf = profile != RtlProfileId::BlogV3 && rf_hz <= ESP_RTL_SDR_XTAL_HZ &&
                    !hf_direct;
    const uint32_t *widths = hf ? kMeasuredHfBandwidths : kMeasuredNativeBandwidths;
    const size_t count = measured_tuner_bandwidth_count(profile, rf_hz, hf_direct);
    bool found = false;
    for (size_t i = 0; i < count; ++i) found |= widths[i] == width_hz;
    if (!found) return false;
    *out = {width_hz, 1814972u,
            static_cast<uint8_t>(profile == RtlProfileId::BlogV4L ? 0xc4 : 0xc5),
            0x8f, 0x3b, 0xf7, 0x78};
    switch (width_hz) {
    case 200000u:
    case 300000u:
        out->if_hz = 2125000u;
        out->reg0b = 0xe6;
        out->if1a = 0x47; out->if1b = 0x1d;
        break;
    case 500000u:
        out->if_hz = 2025000u;
        out->reg0b = 0xe8;
        out->if1a = 0x80; out->if1b = 0x00;
        break;
    case 1000000u:
        out->if_hz = 1700000u;
        out->reg0b = 0xeb;
        out->if19 = 0x3c; out->if1a = 0x38; out->if1b = 0xe4;
        break;
    case 1800000u:
        out->if_hz = 1750000u;
        out->reg0b = 0xac;
        out->if19 = 0x3c; out->if1a = 0x1c; out->if1b = 0x72;
        break;
    default: break;
    }
    return true;
}

enum class RtlBandwidthCommitResult : uint8_t { Applied, RolledBack, Fault };

template <typename Writer>
RtlBandwidthCommitResult rtl_bandwidth_commit(const MeasuredTunerBandwidthPlan &previous,
                                                const MeasuredTunerBandwidthPlan &next,
                                                Writer writer)
{
    if (writer(next, true) == 0) return RtlBandwidthCommitResult::Applied;
    return writer(previous, false) == 0 ? RtlBandwidthCommitResult::RolledBack
                                 : RtlBandwidthCommitResult::Fault;
}
