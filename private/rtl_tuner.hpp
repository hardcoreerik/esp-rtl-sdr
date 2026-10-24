#pragma once

#include <cstddef>
#include <cstdint>

#include "esp_err.h"
#include "rtl_profile.hpp"

struct esp_rtl_sdr_handle;

/*
 * Internal tuner-family boundary for new implementations.
 *
 * Legacy R820/R828 profiles remain on their proven path until they can be
 * migrated with their own byte-for-byte regression evidence. A new family
 * supplies tuner initialization/tuning and declares which records from the
 * shared captured sample-rate slice are safe for its tuner.
 */
struct RtlTunerDriver {
    const char *name;
    uint16_t i2c_address;
    uint32_t min_rf_hz;
    uint32_t max_rf_hz;
    esp_err_t (*initialize)(esp_rtl_sdr_handle *handle);
    esp_err_t (*tune)(esp_rtl_sdr_handle *handle, uint32_t frequency_hz);
    bool (*allows_sample_rate_record)(size_t record_index);
};

const RtlTunerDriver *rtl_tuner_driver_for_profile(RtlProfileId profile);
