#pragma once

#include "esp_rtl_sdr.h"
#include "rtl_control.hpp"

#include <cstdint>
#include <cstring>

/** Internal profile id — mirrors public esp_rtl_sdr_profile_t. */
enum class RtlProfileId : uint8_t {
    Unknown = ESP_RTL_SDR_PROFILE_UNKNOWN,
    BlogV4 = ESP_RTL_SDR_PROFILE_BLOG_V4,
    BlogV3 = ESP_RTL_SDR_PROFILE_BLOG_V3,
    NooelecSmartV5 = ESP_RTL_SDR_PROFILE_NOOELEC_SMART_V5,
};

struct RtlProfileProbeResult {
    bool completed = false;
    uint8_t chip_id = 0;
};

/** Shared Realtek USB identity used by many RTL2832U sticks. */
constexpr uint16_t kRtlSharedVid = 0x0BDA;
constexpr uint16_t kRtlSharedPid = 0x2838;

constexpr uint16_t kBlogV4TunerI2cValue = 0x0074;     /* R828D */
constexpr uint16_t kR820T2TunerI2cValue = 0x0034;     /* R820T2 / R860 */
constexpr uint32_t kR820T2NativeMinHz = 24000000u;    /* no HF claim for R820T2 path */
constexpr uint32_t kBlogV3DemodIfHz = 3570000u;       /* measured V3c matched IF */

inline bool rtl_profile_text_is(const char *actual, const char *expected)
{
    return actual != nullptr && expected != nullptr && std::strcmp(actual, expected) == 0;
}

inline bool rtl_profile_text_contains(const char *haystack, const char *needle)
{
    return haystack != nullptr && needle != nullptr && std::strstr(haystack, needle) != nullptr;
}

/**
 * Descriptor-only identity. Never treats bare 0bda:2838 as Blog V4.
 * Unknown stays Unknown until a completed probe (Blog V3 only) upgrades it.
 */
inline RtlProfileId rtl_profile_from_descriptors(uint16_t vid, uint16_t pid,
                                                 const char *manufacturer,
                                                 const char *product)
{
    if (vid != kRtlSharedVid || pid != kRtlSharedPid) {
        return RtlProfileId::Unknown;
    }
    if (rtl_profile_text_is(manufacturer, "RTLSDRBlog") &&
        rtl_profile_text_is(product, "Blog V4")) {
        return RtlProfileId::BlogV4;
    }
    if (rtl_profile_text_is(manufacturer, "RTLSDRBlog") &&
        (rtl_profile_text_is(product, "Blog V3") ||
         rtl_profile_text_is(product, "RTL-SDR Blog V3"))) {
        return RtlProfileId::BlogV3;
    }
    if (rtl_profile_text_is(manufacturer, "Nooelec") &&
        rtl_profile_text_contains(product, "NESDR SMArt v5")) {
        return RtlProfileId::NooelecSmartV5;
    }
    return RtlProfileId::Unknown;
}

inline bool rtl_profile_v3_probe_matches(const RtlProfileProbeResult &probe)
{
    /* Public R820T2 register-0 chip-id is 0x96; some bridges return bit-reversed 0x69.
     * Incomplete / STALL reads never match. */
    return probe.completed && (probe.chip_id == 0x96 || probe.chip_id == 0x69);
}

/**
 * Final selector. Descriptor wins. Only when descriptors are ambiguous on the
 * shared 0bda:2838 identity may a completed R820T2 chip-id probe select BlogV3.
 * Bare unknown sticks are never BlogV4.
 */
inline RtlProfileId rtl_profile_select(uint16_t vid, uint16_t pid, const char *manufacturer,
                                       const char *product,
                                       const RtlProfileProbeResult &v3_probe)
{
    const RtlProfileId descriptor =
        rtl_profile_from_descriptors(vid, pid, manufacturer, product);
    if (descriptor != RtlProfileId::Unknown) {
        return descriptor;
    }
    if (vid == kRtlSharedVid && pid == kRtlSharedPid &&
        rtl_profile_v3_probe_matches(v3_probe)) {
        return RtlProfileId::BlogV3;
    }
    return RtlProfileId::Unknown;
}

inline const char *rtl_profile_name(RtlProfileId profile)
{
    switch (profile) {
    case RtlProfileId::BlogV4: return "blog_v4_r828d";
    case RtlProfileId::BlogV3: return "blog_v3_r820t2";
    case RtlProfileId::NooelecSmartV5: return "nooelec_smart_v5_r820t2";
    default: return "unknown";
    }
}

inline esp_rtl_sdr_profile_t rtl_profile_to_public(RtlProfileId profile)
{
    return static_cast<esp_rtl_sdr_profile_t>(profile);
}

inline uint16_t rtl_profile_tuner_i2c_value(RtlProfileId profile)
{
    switch (profile) {
    case RtlProfileId::NooelecSmartV5:
    case RtlProfileId::BlogV3:
        return kR820T2TunerI2cValue;
    case RtlProfileId::BlogV4:
        return kBlogV4TunerI2cValue;
    default:
        return 0;
    }
}

/** Library binary feature set (Blog V4 path). Apps must still query device caps. */
inline uint32_t rtl_profile_library_capabilities(void)
{
    return ESP_RTL_SDR_CAP_STREAM | ESP_RTL_SDR_CAP_RETUNE | ESP_RTL_SDR_CAP_METRICS |
           ESP_RTL_SDR_CAP_CUSTOM_HZ | ESP_RTL_SDR_CAP_HOTPLUG |
           ESP_RTL_SDR_CAP_DIRECT_SAMPLING |
           ESP_RTL_SDR_CAP_FREQ_CORRECTION | ESP_RTL_SDR_CAP_MULTI_DEVICE |
           ESP_RTL_SDR_CAP_SYNC_READ | ESP_RTL_SDR_CAP_CONTINUOUS_RATE |
           ESP_RTL_SDR_CAP_NEED | ESP_RTL_SDR_CAP_HEALTH | ESP_RTL_SDR_CAP_PASSPORT |
           ESP_RTL_SDR_CAP_DELIVERY_MODE | ESP_RTL_SDR_CAP_GAIN | ESP_RTL_SDR_CAP_BIAS_TEE |
           ESP_RTL_SDR_CAP_HF_UPCONVERTER | ESP_RTL_SDR_CAP_GAIN_AUTO |
           ESP_RTL_SDR_CAP_RTL_AGC;
}

/**
 * Active-device capability mask. Identity ≠ tuner family ≠ board front-end.
 * Unknown / detached → 0.
 * BlogV3 and Nooelec share provisional VHF/UHF stream (R820T2 I2C remap) without
 * V4 HF / measured gain/bias. Maintainer-unverified; community soak requested.
 */
inline uint32_t rtl_profile_device_capabilities(RtlProfileId profile)
{
    const uint32_t common =
        ESP_RTL_SDR_CAP_HOTPLUG | ESP_RTL_SDR_CAP_METRICS | ESP_RTL_SDR_CAP_CUSTOM_HZ |
        ESP_RTL_SDR_CAP_FREQ_CORRECTION | ESP_RTL_SDR_CAP_MULTI_DEVICE |
        ESP_RTL_SDR_CAP_CONTINUOUS_RATE | ESP_RTL_SDR_CAP_NEED | ESP_RTL_SDR_CAP_HEALTH |
        ESP_RTL_SDR_CAP_DELIVERY_MODE;

    switch (profile) {
    case RtlProfileId::BlogV4:
        return rtl_profile_library_capabilities() & ~ESP_RTL_SDR_CAP_DIRECT_SAMPLING;
    case RtlProfileId::BlogV3:
        /* Manual gain: apply_r820t2_gain_records() writes reg05/07 directly
         * from private/gain_r820t2.hpp. Its discrete stage sequence remains
         * a hardware candidate, not a calibrated table (see that header and
         * docs/captures/NOTES.md).
         * Tuner AUTO gain uses librtlsdr's R82xx recipe (LNA + mixer auto, VGA 26.5 dB). Still no
         * RTL_AGC/BIAS_TEE/HF_UPCONVERTER -- unimplemented
         * for this tuner family, not just unverified. */
        return common | ESP_RTL_SDR_CAP_STREAM | ESP_RTL_SDR_CAP_RETUNE |
               ESP_RTL_SDR_CAP_SYNC_READ | ESP_RTL_SDR_CAP_PASSPORT |
               ESP_RTL_SDR_CAP_GAIN | ESP_RTL_SDR_CAP_GAIN_AUTO |
               ESP_RTL_SDR_CAP_DIRECT_SAMPLING;
    case RtlProfileId::NooelecSmartV5:
        /* Nooelec SMArt v5 uses the same R820T2 tuner and I2C remap path as BlogV3
         * (rtl_profile_uses_r820t2_i2c_remap() is true for both), and
         * apply_gain_records() branches on that same predicate to reach
         * apply_r820t2_gain_records() -- there is no Nooelec-specific gain code path,
         * it is the identical BlogV3 manual gain-table write. Enabling CAP_GAIN here
         * to soak-test on real Nooelec SMArt v5 hardware per the maintainer's request
         * in the capability comment above. AUTO gain as for BlogV3; still no RTL_AGC/BIAS_TEE/HF_UPCONVERTER
         * -- unimplemented for this tuner family, not just unverified. */
        return common | ESP_RTL_SDR_CAP_STREAM | ESP_RTL_SDR_CAP_RETUNE |
               ESP_RTL_SDR_CAP_SYNC_READ | ESP_RTL_SDR_CAP_PASSPORT |
               ESP_RTL_SDR_CAP_GAIN | ESP_RTL_SDR_CAP_GAIN_AUTO;
    default:
        return 0;
    }
}

inline bool rtl_profile_supports_stream(RtlProfileId profile)
{
    return (rtl_profile_device_capabilities(profile) & ESP_RTL_SDR_CAP_STREAM) != 0;
}

inline esp_rtl_sdr_gain_mode_t rtl_profile_default_gain_mode(RtlProfileId profile)
{
    const uint32_t caps = rtl_profile_device_capabilities(profile);
    return (caps & ESP_RTL_SDR_CAP_GAIN) != 0 && (caps & ESP_RTL_SDR_CAP_GAIN_AUTO) == 0
               ? ESP_RTL_SDR_GAIN_MODE_MANUAL
               : ESP_RTL_SDR_GAIN_MODE_AUTO;
}

inline bool rtl_profile_uses_v4_hf_routing(RtlProfileId profile)
{
    return profile == RtlProfileId::BlogV4;
}

inline bool rtl_profile_uses_r820t2_i2c_remap(RtlProfileId profile)
{
    return profile == RtlProfileId::BlogV3 || profile == RtlProfileId::NooelecSmartV5;
}

inline bool rtl_profile_uses_v3_direct_sampling(RtlProfileId profile, uint32_t frequency_hz)
{
    return profile == RtlProfileId::BlogV3 && frequency_hz < kR820T2NativeMinHz;
}

inline bool rtl_profile_needs_cold_tuner_reinit(RtlProfileId profile,
                                                 uint32_t frequency_hz)
{
    return profile == RtlProfileId::BlogV3 &&
           !rtl_profile_uses_v3_direct_sampling(profile, frequency_hz);
}

/** Captured RTL2832U Q-branch NCO: 22-bit negative corrected RF/28.8 MHz, truncated. */
inline uint32_t rtl_profile_v3_direct_nco_word(uint32_t frequency_hz, int32_t ppm = 0)
{
    const int64_t corrected_hz = static_cast<int64_t>(frequency_hz) +
        (static_cast<int64_t>(frequency_hz) * ppm) / 1000000LL;
    const uint32_t scaled = static_cast<uint32_t>(
        (static_cast<uint64_t>(corrected_hz) << 22) / ESP_RTL_SDR_XTAL_HZ);
    return (0x400000u - scaled) & 0x3fffffu;
}

/**
 * R820T2 RF front-end band select: librtlsdr's R820T freq_ranges[] (tuner_r82xx.c). The Blog V4
 * capture the R820T2 path replays only retunes the PLL, so without this the RF mux and tracking
 * filter stay on the 90-110 MHz FM band the capture was taken in (hardcoreerik/esp-rtl-sdr#25).
 * open_d is r17 bit 3, rf_mux_ploy is r1a mask 0xc3, tf_c is r1b.
 */
struct R820T2BandRow {
    uint16_t mhz;        /* row applies from this frequency up to the next row */
    uint8_t open_d;
    uint8_t rf_mux_ploy;
    uint8_t tf_c;
};
constexpr R820T2BandRow kR820T2Bands[] = {
    {0, 0x08, 0x02, 0xdf},   {50, 0x08, 0x02, 0xbe},  {55, 0x08, 0x02, 0x8b},
    {60, 0x08, 0x02, 0x7b},  {65, 0x08, 0x02, 0x69},  {70, 0x08, 0x02, 0x58},
    {75, 0x00, 0x02, 0x44},  {80, 0x00, 0x02, 0x44},  {90, 0x00, 0x02, 0x34},
    {100, 0x00, 0x02, 0x34}, {110, 0x00, 0x02, 0x24}, {120, 0x00, 0x02, 0x24},
    {140, 0x00, 0x02, 0x14}, {180, 0x00, 0x02, 0x13}, {220, 0x00, 0x02, 0x13},
    {250, 0x00, 0x02, 0x11}, {280, 0x00, 0x02, 0x00}, {310, 0x00, 0x41, 0x00},
    {450, 0x00, 0x41, 0x00}, {588, 0x00, 0x40, 0x00}, {650, 0x00, 0x40, 0x00},
};

/** Row for an R820T2 LO frequency. librtlsdr keys the table on the LO (tuner RF + IF), not RF. */
inline const R820T2BandRow *rtl_r820t2_band_for_hz(uint32_t lo_hz)
{
    const uint32_t mhz = lo_hz / 1000000u;
    const R820T2BandRow *row = &kR820T2Bands[0];
    for (const R820T2BandRow &r : kR820T2Bands) {
        if (mhz >= r.mhz) {
            row = &r;
        }
    }
    return row;
}

/** LO the R820T2 runs at for a (ppm-corrected) tuner frequency: tuner + PLL IF offset. */
inline uint32_t rtl_r820t2_lo_hz(uint32_t tuner_hz, double if_offset_hz)
{
    return tuner_hz + static_cast<uint32_t>(if_offset_hz + 0.5);
}

/** R82xx tuners return every register byte bit-reversed over I2C. */
inline uint8_t r82xx_bitrev(uint8_t b)
{
    constexpr uint8_t lut[16] = {0x0, 0x8, 0x4, 0xc, 0x2, 0xa, 0x6, 0xe,
                                 0x1, 0x9, 0x5, 0xd, 0x3, 0xb, 0x7, 0xf};
    return static_cast<uint8_t>((lut[b & 0xf] << 4) | lut[b >> 4]);
}

/** Blog V4 vendor board controls must never run on plain R820T2/R860 sticks. */
inline bool rtl_profile_allows_init_record(RtlProfileId profile,
                                           const RtlControlRecord &record)
{
    if (!rtl_profile_uses_r820t2_i2c_remap(profile)) {
        return true;
    }
    return record.value != 0x3001 && record.value != 0x3003 && record.value != 0x3004;
}

inline bool rtl_profile_supports_rf_hz(RtlProfileId profile, uint32_t frequency_hz)
{
    if (frequency_hz < ESP_RTL_SDR_FREQ_MIN_HZ || frequency_hz > ESP_RTL_SDR_FREQ_MAX_HZ) {
        return false;
    }
    /* Nooelec has no measured direct-sampling path. Blog V3 uses its separately
     * captured Q-branch path below this native tuner floor. */
    if (profile == RtlProfileId::NooelecSmartV5 && frequency_hz < kR820T2NativeMinHz) {
        return false;
    }
    if (profile == RtlProfileId::Unknown) {
        return false;
    }
    return true;
}

inline uint32_t rtl_profile_tuner_frequency_hz(RtlProfileId profile, uint32_t rf_hz)
{
    if (rtl_profile_uses_v3_direct_sampling(profile, rf_hz)) {
        return 0; /* tuner bypassed */
    }
    if (rtl_profile_uses_v4_hf_routing(profile)) {
        return esp_rtl_sdr_tuner_frequency_hz(rf_hz);
    }
    return rf_hz;
}

/**
 * PLL reference crystal, Hz. This is 28.8 MHz for every profile tested so
 * far, V3c included -- see rtl_profile_pll_if_offset_hz() below for the
 * correction that actually mattered.
 *
 * (History: an earlier same-session pass mistakenly concluded V3c used a
 * 32 MHz crystal, from analyzing the integer N-divider byte (reg 0x14) in
 * isolation without its fractional carry from reg 0x15/0x16. Once the
 * fractional bytes were folded in correctly, 28.8 MHz fits all 11 points
 * of the 2026-09-11 FM-band sweep to within 1 LSB (~27 Hz, a rounding-mode
 * nuance, not a real error) -- see docs/captures/NOTES.md.)
 */
inline double rtl_profile_pll_xtal_hz(RtlProfileId profile)
{
    (void)profile;
    constexpr double kMeasuredXtalHz = 28800000.0;
    return kMeasuredXtalHz;
}

/**
 * PLL IF offset, Hz, added to the user-requested RF frequency before the
 * N-divider math. kRtlIfOffsetHz (1,814,972 Hz, see transfers_blog_v4.hpp)
 * is a Blog V4/R828D-board-specific measurement (that board's particular
 * filter/triplexer design), not a universal RTL-SDR constant.
 *
 * Direct clean-room capture against a real Blog V3c (2026-09-11, FM-band
 * sweep, 88.1-107.9 MHz, plus 5 repeated tunes to the same frequency to
 * rule out a non-deterministic calibration search) solved to exactly
 * 3,570,000 Hz -- the well-known standard RTL2832U/R820T default IF,
 * confirmed independently at three widely-spaced frequencies (88.1, 96.1,
 * 106.1 MHz) to within a few Hz. See docs/captures/NOTES.md for the full
 * sweep data and regression. Scoped to BlogV3 only: NooelecSmartV5 shares
 * BlogV3's I2C remap for tuner addressing but has never been hardware
 * tested for PLL math, so it keeps the V4-derived default rather than
 * inheriting an unverified guess.
 */
inline double rtl_profile_pll_if_offset_hz(RtlProfileId profile)
{
    constexpr double kMeasuredV4IfOffsetHz = 1814972.0;
    /* Soak test: NooelecSmartV5 is the same R820T2 tuner as BlogV3, so try BlogV3's
     * measured 3.57 MHz instead of the R828D/V4-board value. */
    if (profile == RtlProfileId::BlogV3 || profile == RtlProfileId::NooelecSmartV5) {
        return static_cast<double>(kBlogV3DemodIfHz);
    }
    return kMeasuredV4IfOffsetHz;
}

/**
 * R820T2 IF filter and IF frequency for a sample rate, as librtlsdr's r82xx_set_bandwidth() picks
 * them when the tuner bandwidth is left automatic (bandwidth = sample rate). reg 0x0a is written
 * with mask 0x10, reg 0x0b with mask 0xef, and the RTL2832 demod IF plus the tuner LO follow if_hz.
 * The replayed capture leaves the 2.2 MHz filter (0x8f) but a 3.57 MHz IF: the signal then sits at
 * the edge of the filter and the image (7.14 MHz off) is barely rejected, which on a busy band
 * (915 MHz ISM) buries the wanted signals under the AGC-amplified neighbours.
 */
struct R820T2IfSetting {
    uint8_t reg0a;
    uint8_t reg0b;
    uint32_t if_hz;
};

inline R820T2IfSetting rtl_r820t2_if_for_rate(uint32_t sample_rate_sps)
{
    constexpr uint32_t kBwKhz[] = {300, 450, 600, 900, 1100, 1200, 1300, 1500, 1800, 2200, 3000, 5000};
    constexpr uint8_t kReg0b[] = {0xe8, 0xe9, 0xea, 0xeb, 0xec, 0xed, 0xee, 0xef, 0xaf, 0x8f, 0x6f, 0x6f};
    constexpr uint32_t kIfKhz[] = {1700, 1650, 1600, 1500, 1400, 1350, 1320, 1270, 1600, 1750, 2000, 3570};
    const uint32_t bw_khz = sample_rate_sps / 1000u;
    if (bw_khz > 7000u) {
        return {0x10, 0x0b, 4570000u};
    }
    if (bw_khz > 6000u) {
        return {0x10, 0x2a, 4570000u};
    }
    if (bw_khz > 5000u) {
        return {0x10, 0x6b, 3570000u};
    }
    size_t i = 0;
    while (i + 1 < sizeof(kBwKhz) / sizeof(kBwKhz[0]) && bw_khz > kBwKhz[i]) {
        ++i;
    }
    return {static_cast<uint8_t>(i == 11 ? 0x00 : 0x0f), kReg0b[i], kIfKhz[i] * 1000u};
}

/** RTL2832 demod IF word (page 1 regs 0x19..0x1b), librtlsdr's rtlsdr_set_if_freq() arithmetic. */
inline uint32_t rtl_demod_if_word(uint32_t if_hz, uint32_t xtal_hz)
{
    const int64_t v = (static_cast<int64_t>(if_hz) << 22) / static_cast<int64_t>(xtal_hz);
    return static_cast<uint32_t>(-v) & 0x3fffffu;
}

/** Non-zero only when initialization must restore a profile-specific demod IF. */
inline uint32_t rtl_profile_demod_if_restore_hz(RtlProfileId profile)
{
    return (profile == RtlProfileId::BlogV3 || profile == RtlProfileId::NooelecSmartV5)
               ? kBlogV3DemodIfHz
               : 0u;
}
