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
    BlogV4L = ESP_RTL_SDR_PROFILE_BLOG_V4L,
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
constexpr uint32_t kR820T2NativeMinHz = 24000000u;    /* captured V3/Nooelec Q-route cutoff */
constexpr uint32_t kBlogV3DemodIfHz = 3570000u;       /* measured V3c matched IF */
/* Older V3c-specific evidence (2026-09-28), retained for V3c only, not Nooelec.
 * V3c boot tuner filter registers 0x0a/0x0b. Boot runs the captured R820-family
 * reinit slice (kRtlTunerReinitFirst..Last), whose last writes of these
 * registers are d5/6b; read back from the chip after a cold boot on hardware
 * (2026-09-28). They belong with kBlogV3DemodIfHz. The PC's c5/8f is its
 * pairing with a 1.815 MHz IF and is not the state V3c boots in. */
constexpr uint8_t kBlogV3BootReg0a = 0xd5;
constexpr uint8_t kBlogV3BootReg0b = 0x6b;

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
    /* V4L before V4.
     *
     * The V4L carries an R828S, which "may appear to software like an R820T
     * or R860 over I2C" (RTL-SDR Blog). Before this profile existed the
     * exact match on "Blog V4" rejected "Blog V4L", detection fell through
     * to the I2C probe, the probe saw an R820T-like tuner and the stick came
     * up as BlogV3 - complete with a direct-sampling HF path this board does
     * not have. The EEPROM strings were being read correctly and then
     * ignored, so identity has to be settled from them, not from I2C. */
    if (rtl_profile_text_is(manufacturer, "RTLSDRBlog") &&
        rtl_profile_text_is(product, "Blog V4L")) {
        return RtlProfileId::BlogV4L;
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
    case RtlProfileId::BlogV4L: return "blog_v4l_r828s";
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
    case RtlProfileId::BlogV4L:
        /* R828S answers where an R820T/R860 would, so it shares the
         * 0x74 -> 0x34 remapped addressing rather than the V4's R828D
         * address. This is the one V4L behaviour that is evidenced. */
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
           ESP_RTL_SDR_CAP_RTL_AGC | ESP_RTL_SDR_CAP_TUNER_BANDWIDTH;
}

/**
 * Active-device capability mask. Identity ≠ tuner family ≠ board front-end.
 * Unknown / detached → 0.
 * BlogV3 and Nooelec share R820T2 I2C remapping; board features remain distinct.
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
        /* Generic R820T2 identity includes the user-tested V3c, but does not
         * prove a bias circuit on every stick. Enable only by explicit API. */
        return common | ESP_RTL_SDR_CAP_STREAM | ESP_RTL_SDR_CAP_RETUNE |
               ESP_RTL_SDR_CAP_SYNC_READ | ESP_RTL_SDR_CAP_PASSPORT |
               ESP_RTL_SDR_CAP_GAIN | ESP_RTL_SDR_CAP_GAIN_AUTO |
               ESP_RTL_SDR_CAP_RTL_AGC | ESP_RTL_SDR_CAP_BIAS_TEE |
               ESP_RTL_SDR_CAP_DIRECT_SAMPLING | ESP_RTL_SDR_CAP_TUNER_BANDWIDTH;
    case RtlProfileId::BlogV4L:
        /* R828S at 0x34 with its own measured upconverter route. Never use
         * BlogV3 direct-Q or BlogV4 triplexer handling on this board. Gain
         * values are nominal PC requests, not calibrated analog dB. */
        return common | ESP_RTL_SDR_CAP_STREAM | ESP_RTL_SDR_CAP_RETUNE |
               ESP_RTL_SDR_CAP_SYNC_READ | ESP_RTL_SDR_CAP_PASSPORT |
               ESP_RTL_SDR_CAP_GAIN | ESP_RTL_SDR_CAP_GAIN_AUTO |
               ESP_RTL_SDR_CAP_RTL_AGC | ESP_RTL_SDR_CAP_BIAS_TEE |
               ESP_RTL_SDR_CAP_HF_UPCONVERTER | ESP_RTL_SDR_CAP_TUNER_BANDWIDTH;
    case RtlProfileId::NooelecSmartV5:
        /* First-party PC captures, 2026-09-30. Q bypasses tuner controls;
         * this board has no bias tee or V4 HF upconverter. P4 acceptance open. */
        return common | ESP_RTL_SDR_CAP_STREAM | ESP_RTL_SDR_CAP_RETUNE |
               ESP_RTL_SDR_CAP_SYNC_READ | ESP_RTL_SDR_CAP_PASSPORT |
               ESP_RTL_SDR_CAP_GAIN | ESP_RTL_SDR_CAP_GAIN_AUTO |
               ESP_RTL_SDR_CAP_RTL_AGC | ESP_RTL_SDR_CAP_DIRECT_SAMPLING |
               ESP_RTL_SDR_CAP_TUNER_BANDWIDTH;
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

inline void rtl_profile_clear_bias_request(bool &want)
{
    want = false;
}

inline bool rtl_profile_uses_v4_hf_routing(RtlProfileId profile)
{
    return profile == RtlProfileId::BlogV4;
}

inline bool rtl_profile_uses_r820t2_i2c_remap(RtlProfileId profile)
{
    return profile == RtlProfileId::BlogV3 ||
           profile == RtlProfileId::NooelecSmartV5 ||
           profile == RtlProfileId::BlogV4L;
}

inline constexpr bool rtl_profile_uses_v3_direct_sampling(RtlProfileId profile,
                                                         uint32_t frequency_hz)
{
    /* Nooelec 2026-09-30 captures independently match the older V3 Q sequence.
     * This is the captured PC route cutoff, not a claim of PLL lock at 24 MHz. */
    return (profile == RtlProfileId::BlogV3 || profile == RtlProfileId::NooelecSmartV5) &&
           frequency_hz < kR820T2NativeMinHz;
}

inline bool rtl_profile_needs_cold_tuner_reinit(RtlProfileId profile,
                                                 uint32_t frequency_hz)
{
    /* V4L keeps the cold reinit.
     *
     * Not a guess: before this profile existed the V4L was misdetected as
     * BlogV3 and streamed at 2.4 MS/s under that path, which includes this
     * reinit. Dropping it while changing identity would risk breaking a
     * stream that demonstrably works, and nothing about R828S says the
     * sequence is unnecessary. Revisit if a first-party V4L capture shows
     * otherwise. */
    return rtl_profile_uses_r820t2_i2c_remap(profile) &&
           !rtl_profile_uses_v3_direct_sampling(profile, frequency_hz);
}

/** Map the existing wire records using independently measured board differences. */
inline RtlControlRecord rtl_profile_map_tuner_record(RtlProfileId profile,
                                                     const RtlControlRecord &record)
{
    RtlControlRecord mapped = record;
    const uint16_t tuner_addr = rtl_profile_tuner_i2c_value(profile);
    if (tuner_addr != 0 &&
        (mapped.index == 0x0610 || mapped.index == 0x0600) &&
        (mapped.value & 0x00ffu) == kBlogV4TunerI2cValue) {
        mapped.value = static_cast<uint16_t>((mapped.value & 0xff00u) | tuner_addr);
    }
    /* Older V4/V3 tables use c5/d5. Nooelec's 2026-09-30 captures use c3/d3;
     * preserve the other bits, especially the direct-mode standby value 36. */
    if (profile == RtlProfileId::NooelecSmartV5 && mapped.request_type == 0x40 &&
        mapped.index == 0x0610 && mapped.value == kR820T2TunerI2cValue &&
        mapped.length == 2 && mapped.data[0] == 0x0a &&
        (mapped.data[1] == 0xc5 || mapped.data[1] == 0xd5)) {
        mapped.data[1] = static_cast<uint8_t>(mapped.data[1] - 2);
    }
    return mapped;
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
    if (profile == RtlProfileId::NooelecSmartV5) {
        /* Manufacturer's model limits; the 60 kHz capture was exploratory. */
        return frequency_hz >= 100000u && frequency_hz <= 1750000000u;
    }
    if (profile == RtlProfileId::Unknown) {
        return false;
    }
    return true;
}

inline uint32_t rtl_profile_tuner_frequency_hz(RtlProfileId profile, uint32_t rf_hz,
                                               bool hf_direct = false)
{
    if (rtl_profile_uses_v3_direct_sampling(profile, rf_hz)) {
        return 0; /* tuner bypassed */
    }
    if (rtl_profile_uses_v4_hf_routing(profile)) {
        return hf_direct ? rf_hz : esp_rtl_sdr_tuner_frequency_hz(rf_hz);
    }
    if (profile == RtlProfileId::BlogV4L && rf_hz < ESP_RTL_SDR_XTAL_HZ && !hf_direct) {
        return rf_hz + ESP_RTL_SDR_XTAL_HZ;
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
 * sweep data and regression. The Nooelec 2026-09-30 cold FM captures now
 * independently confirm 3.57 MHz. Explicit Nooelec bandwidth AUTO uses a
 * different captured IF, 1.815 MHz; it must not inherit the older V3 AUTO
 * boot-state policy. See docs/captures/nooelec_v5_2026-09-30.md.
 */
inline double rtl_profile_pll_if_offset_hz(RtlProfileId profile)
{
    constexpr double kMeasuredV4IfOffsetHz = 1814972.0;
    if (profile == RtlProfileId::BlogV3 || profile == RtlProfileId::NooelecSmartV5) {
        return static_cast<double>(kBlogV3DemodIfHz);
    }
    return kMeasuredV4IfOffsetHz;
}

/** Non-zero only when initialization must restore a profile-specific demod IF. */
inline uint32_t rtl_profile_demod_if_restore_hz(RtlProfileId profile)
{
    return profile == RtlProfileId::BlogV3 || profile == RtlProfileId::NooelecSmartV5
        ? kBlogV3DemodIfHz : 0u;
}

/** R820T2 boards whose RF mux / tracking filter must follow the tuned band (#25). */
inline bool rtl_profile_uses_r820t2_band_select(RtlProfileId profile)
{
    /* V4L is excluded: its R828S has its own measured 17/1a/1b route. */
    return profile == RtlProfileId::BlogV3 || profile == RtlProfileId::NooelecSmartV5;
}

/**
 * R820T2 RF front-end band select. Every row is reproduced by our own black-box capture of a
 * Nooelec SMArt v5 (docs/captures/nooelec_v5_band_sweep_2026-10-01.md): both sides of each boundary.
 * kRtlFinalTuneTemplate was captured on FM, so without this every R820T2 tune leaves
 * the RF mux and tracking filter on the 90-110 MHz row (17=20, 1a=2a, 1b=34) and the
 * ADC sees no RF at e.g. 433.92 MHz (hardcoreerik/esp-rtl-sdr#25).
 * open_d is r17 bit 3, rf_mux_ploy is r1a mask 0xc3, tf_c is r1b.
 */
struct R820T2BandRow {
    uint16_t mhz; /* row applies from this LO frequency up to the next row */
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

/** LO the R820T2 PLL is programmed to: (ppm-corrected) tuner frequency + PLL IF. */
inline uint32_t rtl_r820t2_lo_hz(uint32_t tuner_hz, double if_offset_hz)
{
    return tuner_hz + static_cast<uint32_t>(if_offset_hz + 0.5);
}

/** Band row for an LO frequency. The captured sweep shows the rows switch on LO (tuner + PLL IF), not RF. */
inline const R820T2BandRow &rtl_r820t2_band_for_lo_hz(uint32_t lo_hz)
{
    const uint32_t mhz = lo_hz / 1000000u;
    const R820T2BandRow *row = &kR820T2Bands[0];
    for (const R820T2BandRow &r : kR820T2Bands) {
        if (mhz >= r.mhz) {
            row = &r;
        }
    }
    return *row;
}

/** Apply a band row to one tune-template tuner write (17/1a/1b); other records unchanged. */
inline void rtl_r820t2_patch_band_record(const R820T2BandRow &band, RtlControlRecord &rec)
{
    if (rec.request_type != 0x40 || rec.index != 0x0610 || rec.length != 2) {
        return;
    }
    switch (rec.data[0]) {
    case 0x17: rec.data[1] = static_cast<uint8_t>((rec.data[1] & ~0x08) | band.open_d); break;
    case 0x1a: rec.data[1] = static_cast<uint8_t>((rec.data[1] & ~0xc3) | band.rf_mux_ploy); break;
    case 0x1b: rec.data[1] = band.tf_c; break;
    default: break;
    }
}
