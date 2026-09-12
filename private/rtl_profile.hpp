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
        return rtl_profile_library_capabilities();
    case RtlProfileId::BlogV3:
        /* Manual gain (2026-09-11): apply_r820t2_gain_records() writes
         * reg05/07 directly from private/interpolated_gain_r820t2.hpp --
         * two hardware-confirmed anchors, 27 interpolated points, NOT a
         * full measured table (see that header + docs/captures/NOTES.md).
         * Still no AUTO/RTL_AGC/BIAS_TEE/HF_UPCONVERTER -- unimplemented
         * for this tuner family, not just unverified. */
        return common | ESP_RTL_SDR_CAP_STREAM | ESP_RTL_SDR_CAP_RETUNE |
               ESP_RTL_SDR_CAP_SYNC_READ | ESP_RTL_SDR_CAP_PASSPORT |
               ESP_RTL_SDR_CAP_GAIN;
    case RtlProfileId::NooelecSmartV5:
        /* Provisional: stream/retune/sync-read/passport; no V4 HF or measured gain/bias. */
        return common | ESP_RTL_SDR_CAP_STREAM | ESP_RTL_SDR_CAP_RETUNE |
               ESP_RTL_SDR_CAP_SYNC_READ | ESP_RTL_SDR_CAP_PASSPORT;
    default:
        return 0;
    }
}

inline bool rtl_profile_supports_stream(RtlProfileId profile)
{
    return (rtl_profile_device_capabilities(profile) & ESP_RTL_SDR_CAP_STREAM) != 0;
}

inline bool rtl_profile_uses_v4_hf_routing(RtlProfileId profile)
{
    return profile == RtlProfileId::BlogV4;
}

inline bool rtl_profile_uses_r820t2_i2c_remap(RtlProfileId profile)
{
    return profile == RtlProfileId::BlogV3 || profile == RtlProfileId::NooelecSmartV5;
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
    /* R820T2-family provisional paths: fail-closed below native floor (~24 MHz).
     * No V4 HF upconverter / Cable-2 / GPIO5. */
    if (rtl_profile_uses_r820t2_i2c_remap(profile) && frequency_hz < kR820T2NativeMinHz) {
        return false;
    }
    if (profile == RtlProfileId::Unknown) {
        return false;
    }
    return true;
}

inline uint32_t rtl_profile_tuner_frequency_hz(RtlProfileId profile, uint32_t rf_hz)
{
    if (rtl_profile_uses_v4_hf_routing(profile)) {
        return esp_rtl_sdr_tuner_frequency_hz(rf_hz);
    }
    return rf_hz;
}
