#pragma once

/*
 * Expected behaviour per dongle profile. This is an independent oracle: the
 * firmware compares what the driver reports on real hardware against this
 * table, and tests/host/test_function_test.cpp compares the table against the
 * driver's own profile code, so neither can drift silently.
 *
 * A new dongle profile with no row here is reported as SKIP ("no expectation
 * table"), never as PASS.
 */

#include <cstddef>
#include <cstdint>

#include "esp_rtl_sdr.h"

namespace ft {

struct Expect {
    esp_rtl_sdr_profile_t profile;
    uint32_t caps_required;  /* every bit must be present in get_device_capabilities() */
    uint32_t caps_forbidden; /* no bit may be present */
    size_t gain_steps;       /* length of the nominal gain ladder */
    uint32_t rf_min_hz;      /* lowest RF the profile accepts */
    uint32_t rf_max_hz;      /* highest RF the profile accepts */
    bool hf_direct_route;    /* V4 / V4L: set_hf_direct_min_hz is supported */
};

constexpr uint32_t kCommon =
    ESP_RTL_SDR_CAP_HOTPLUG | ESP_RTL_SDR_CAP_METRICS | ESP_RTL_SDR_CAP_CUSTOM_HZ |
    ESP_RTL_SDR_CAP_FREQ_CORRECTION | ESP_RTL_SDR_CAP_MULTI_DEVICE |
    ESP_RTL_SDR_CAP_CONTINUOUS_RATE | ESP_RTL_SDR_CAP_NEED | ESP_RTL_SDR_CAP_HEALTH |
    ESP_RTL_SDR_CAP_DELIVERY_MODE | ESP_RTL_SDR_CAP_STREAM | ESP_RTL_SDR_CAP_RETUNE |
    ESP_RTL_SDR_CAP_SYNC_READ | ESP_RTL_SDR_CAP_PASSPORT | ESP_RTL_SDR_CAP_GAIN |
    ESP_RTL_SDR_CAP_GAIN_AUTO | ESP_RTL_SDR_CAP_RTL_AGC | ESP_RTL_SDR_CAP_TUNER_BANDWIDTH;

constexpr Expect kExpect[] = {
    {ESP_RTL_SDR_PROFILE_BLOG_V4,
     kCommon | ESP_RTL_SDR_CAP_BIAS_TEE | ESP_RTL_SDR_CAP_HF_UPCONVERTER,
     ESP_RTL_SDR_CAP_DIRECT_SAMPLING, 28, ESP_RTL_SDR_FREQ_MIN_HZ, ESP_RTL_SDR_FREQ_MAX_HZ, true},
    {ESP_RTL_SDR_PROFILE_BLOG_V4L,
     kCommon | ESP_RTL_SDR_CAP_BIAS_TEE | ESP_RTL_SDR_CAP_HF_UPCONVERTER,
     ESP_RTL_SDR_CAP_DIRECT_SAMPLING, 29, ESP_RTL_SDR_FREQ_MIN_HZ, ESP_RTL_SDR_FREQ_MAX_HZ, true},
    {ESP_RTL_SDR_PROFILE_BLOG_V3,
     kCommon | ESP_RTL_SDR_CAP_BIAS_TEE | ESP_RTL_SDR_CAP_DIRECT_SAMPLING,
     ESP_RTL_SDR_CAP_HF_UPCONVERTER, 29, ESP_RTL_SDR_FREQ_MIN_HZ, ESP_RTL_SDR_FREQ_MAX_HZ, false},
    {ESP_RTL_SDR_PROFILE_NOOELEC_SMART_V5, kCommon | ESP_RTL_SDR_CAP_DIRECT_SAMPLING,
     ESP_RTL_SDR_CAP_BIAS_TEE | ESP_RTL_SDR_CAP_HF_UPCONVERTER, 29, 100000u, 1750000000u, false},
};

inline const Expect *find_expect(esp_rtl_sdr_profile_t profile)
{
    for (const Expect &e : kExpect) {
        if (e.profile == profile) {
            return &e;
        }
    }
    return nullptr;
}

} // namespace ft
