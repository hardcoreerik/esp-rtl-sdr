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

/* The widths a profile offers at this RF, and how many there are. The count and the array are
 * chosen by ONE rule so they cannot disagree: on the HF upconverter route (V4/V4L at or below
 * 28.8 MHz and not on the direct route) the short HF list applies, otherwise the native list. */
struct MeasuredBandwidthList {
    const uint32_t *values;
    size_t count;
};

constexpr MeasuredBandwidthList measured_tuner_bandwidth_list(RtlProfileId profile,
                                                              uint32_t rf_hz,
                                                              bool hf_direct = false)
{
    if (profile != RtlProfileId::BlogV4 && profile != RtlProfileId::BlogV4L &&
        profile != RtlProfileId::BlogV3 && profile != RtlProfileId::NooelecSmartV5)
        return {nullptr, 0};
    if (rtl_profile_uses_v3_direct_sampling(profile, rf_hz)) return {nullptr, 0};
    return (profile == RtlProfileId::BlogV4 || profile == RtlProfileId::BlogV4L) &&
           rf_hz <= ESP_RTL_SDR_XTAL_HZ && !hf_direct
        ? MeasuredBandwidthList{kMeasuredHfBandwidths,
                                sizeof(kMeasuredHfBandwidths) / sizeof(uint32_t)}
        : MeasuredBandwidthList{kMeasuredNativeBandwidths,
                                sizeof(kMeasuredNativeBandwidths) / sizeof(uint32_t)};
}

constexpr size_t measured_tuner_bandwidth_count(RtlProfileId profile, uint32_t rf_hz,
                                                bool hf_direct = false)
{
    return measured_tuner_bandwidth_list(profile, rf_hz, hf_direct).count;
}

inline bool measured_tuner_bandwidth_plan(RtlProfileId profile, uint32_t rf_hz,
                                           uint32_t width_hz,
                                           MeasuredTunerBandwidthPlan *out,
                                           bool hf_direct = false)
{
    const MeasuredBandwidthList list = measured_tuner_bandwidth_list(profile, rf_hz, hf_direct);
    if (out == nullptr || list.count == 0) return false;
    const uint32_t *widths = list.values;
    const size_t count = list.count;
    bool found = false;
    for (size_t i = 0; i < count; ++i) found |= widths[i] == width_hz;
    if (!found) return false;
    *out = {width_hz, 1814972u,
            static_cast<uint8_t>(profile == RtlProfileId::BlogV4L ? 0xc4 :
                                profile == RtlProfileId::NooelecSmartV5 ? 0xc3 : 0xc5),
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
    if (profile == RtlProfileId::BlogV3 && width_hz == 0) {
        /* Older V3c-specific AUTO policy (2026-09-28): restore the boot IF and
         * filters. Nooelec's 2026-09-30 AUTO capture instead uses c3/8f, 1.815 MHz. */
        out->if_hz = kBlogV3DemodIfHz;
        out->reg0a = kBlogV3BootReg0a; out->reg0b = kBlogV3BootReg0b;
        out->if19 = 0x38; out->if1a = 0x11; out->if1b = 0x12;
    }
    return true;
}

/* Blog V4 / V4L: the vendor DLL's IF filter, IF and demod IF follow the sample rate on a cold open (our own black-box
 * PC captures, 2026-10-01: docs/captures/vendor_dll_rate_matrix_2026-10-01.md, two identical runs per dongle). The V3c and the
 * Nooelec keep the 3.57 MHz IF at every rate, so they are not in this table. 2.4 MS/s is the AUTO plan above and is left alone.
 * Only the listed rates apply (anything else keeps the previous behaviour), only on the native route (the HF upconverter route
 * was not measured), and never with an explicit tuner bandwidth (those are 2.4 MS/s only). The filter register 0a differs
 * between the two boards (V4 c5/d5, V4L c4/d4); 0b, the IF and the demod IF bytes are identical. */
struct BlogRateRow {
    uint32_t rate_sps;
    uint8_t reg0b;
    uint32_t if_hz;
    uint8_t if19, if1a, if1b;
};
constexpr BlogRateRow kBlogRateRows[] = {
    {250000u,  0xe6, 2125000u, 0x3b, 0x47, 0x1d},
    {256000u,  0xe6, 2125000u, 0x3b, 0x47, 0x1d},
    {960000u,  0xeb, 1700000u, 0x3c, 0x38, 0xe4},
    {1024000u, 0xeb, 1700000u, 0x3c, 0x38, 0xe4},
    {1400000u, 0xec, 1575000u, 0x3c, 0x80, 0x00},
    {1800000u, 0xac, 1750000u, 0x3c, 0x1c, 0x72},
    {1920000u, 0xae, 1675000u, 0x3c, 0x47, 0x1d},
    {2000000u, 0xaf, 1625000u, 0x3c, 0x63, 0x8f},
    {2048000u, 0xaf, 1625000u, 0x3c, 0x63, 0x8f},
    {2560000u, 0x6b, 3570000u, 0x38, 0x11, 0x12},
    {2800000u, 0x6b, 3570000u, 0x38, 0x11, 0x12},
    {2880000u, 0x6b, 3570000u, 0x38, 0x11, 0x12},
    {3200000u, 0x6b, 3570000u, 0x38, 0x11, 0x12},
};

inline bool rtl_blog_rate_plan(RtlProfileId profile, uint32_t rate_sps, uint32_t rf_hz,
                               MeasuredTunerBandwidthPlan *out)
{
    if (out == nullptr || (profile != RtlProfileId::BlogV4 && profile != RtlProfileId::BlogV4L)) {
        return false;
    }
    if (rf_hz <= ESP_RTL_SDR_XTAL_HZ) return false; /* HF upconverter / direct routes: not measured */
    for (const BlogRateRow &row : kBlogRateRows) {
        if (row.rate_sps != rate_sps) continue;
        const bool wide = row.reg0b == 0x6b;
        const uint8_t reg0a = profile == RtlProfileId::BlogV4L ? (wide ? 0xd4 : 0xc4)
                                                              : (wide ? 0xd5 : 0xc5);
        *out = {0u, row.if_hz, reg0a, row.reg0b, row.if19, row.if1a, row.if1b};
        return true;
    }
    return false;
}

/* Demod IF records of a bandwidth transaction: each IF byte write (0x19, 0x1a,
 * 0x1b) is followed by the page-0x0a reg-0x01 read that the captured init IF
 * sequence and PC live-bandwidth captures place after every demod write.
 * Without the reads, a live transition on the M5 Tab5 left the RTL2832 using
 * the new 0x19 byte with the previous transaction's 0x1a/0x1b (V3c: about
 * +/-95 kHz, V4L: -312 to +386 kHz) while the driver reported the requested RF.
 * The RTL2832 mechanism behind the read is not established. */
constexpr size_t kMeasuredBandwidthDemodIfRecordCount = 6;

inline void measured_bandwidth_demod_if_records(
    const MeasuredTunerBandwidthPlan &plan,
    RtlControlRecord (&out)[kMeasuredBandwidthDemodIfRecordCount])
{
    const RtlControlRecord settle_read = {0x0120, 0x000a, 0xc0, 1, {0, 0, 0, 0, 0, 0, 0, 0}};
    out[0] = {0x1920, 0x0011, 0x40, 1, {plan.if19, 0, 0, 0, 0, 0, 0, 0}};
    out[1] = settle_read;
    out[2] = {0x1a20, 0x0011, 0x40, 1, {plan.if1a, 0, 0, 0, 0, 0, 0, 0}};
    out[3] = settle_read;
    out[4] = {0x1b20, 0x0011, 0x40, 1, {plan.if1b, 0, 0, 0, 0, 0, 0, 0}};
    out[5] = settle_read;
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
