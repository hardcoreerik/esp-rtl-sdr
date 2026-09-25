/*
 * Multi-receiver identity / metadata / stats helpers.
 * Compiles on-device and in host unit tests (no USB / FreeRTOS).
 */

#include "esp_rtl_sdr.h"
#include "rtl_multi.hpp"

#if defined(ESP_PLATFORM)
#include "sdkconfig.h"
#endif

#include <cstdio>
#include <cstring>

void esp_rtl_sdr_identity_default(esp_rtl_sdr_identity_t *out)
{
    if (out == nullptr) {
        return;
    }
    std::memset(out, 0, sizeof(*out));
    out->struct_size = sizeof(*out);
    std::snprintf(out->usb_path, sizeof(out->usb_path), "root");
}

void esp_rtl_sdr_capture_meta_default(esp_rtl_sdr_capture_meta_t *out)
{
    if (out == nullptr) {
        return;
    }
    std::memset(out, 0, sizeof(*out));
    out->struct_size = sizeof(*out);
}

void esp_rtl_sdr_stream_stats_default(esp_rtl_sdr_stream_stats_t *out)
{
    if (out == nullptr) {
        return;
    }
    std::memset(out, 0, sizeof(*out));
    out->struct_size = sizeof(*out);
}

void esp_rtl_sdr_hub_stats_default(esp_rtl_sdr_hub_stats_t *out)
{
    if (out == nullptr) {
        return;
    }
    std::memset(out, 0, sizeof(*out));
    out->struct_size = sizeof(*out);
#if defined(CONFIG_USB_HOST_HUBS_SUPPORTED)
    out->hubs_compiled_in = CONFIG_USB_HOST_HUBS_SUPPORTED;
#else
    out->hubs_compiled_in = false;
#endif
}

void esp_rtl_sdr_format_usb_path(uint8_t parent_addr, uint8_t hub_port, char *dst,
                                 size_t dst_sz)
{
    if (dst == nullptr || dst_sz == 0) {
        return;
    }
    if (parent_addr == 0 || hub_port == 0) {
        std::snprintf(dst, dst_sz, "root");
        return;
    }
    std::snprintf(dst, dst_sz, "P%u.p%u", static_cast<unsigned>(parent_addr),
                  static_cast<unsigned>(hub_port));
}

void esp_rtl_sdr_fill_capture_meta(esp_rtl_sdr_capture_meta_t *out, uint8_t device_id,
                                   uint8_t usb_addr, uint8_t hub_port, uint32_t sequence,
                                   int64_t host_timestamp_us, uint32_t sample_count,
                                   uint32_t center_hz, uint32_t tuner_hz,
                                   uint32_t sample_rate_sps, uint8_t gain_mode,
                                   int gain_tenth_db, int ppm, esp_rtl_sdr_profile_t profile,
                                   bool bias_tee, esp_rtl_sdr_state_t stream_status,
                                   uint32_t dropped_buffers, uint32_t usb_errors,
                                   uint32_t flags)
{
    if (out == nullptr) {
        return;
    }
    esp_rtl_sdr_capture_meta_default(out);
    out->device_id = device_id;
    out->usb_addr = usb_addr;
    out->hub_port = hub_port;
    out->sequence = sequence;
    out->host_timestamp_us = host_timestamp_us;
    out->sample_count = sample_count;
    out->center_frequency_hz = center_hz;
    out->tuner_frequency_hz = tuner_hz;
    out->sample_rate_sps = sample_rate_sps;
    out->bandwidth_hz = sample_rate_sps;
    out->gain_mode = gain_mode;
    out->gain_tenth_db = gain_tenth_db;
    out->freq_correction_ppm = ppm;
    out->profile = profile;
    out->bias_tee = bias_tee;
    out->stream_status = stream_status;
    out->dropped_buffers = dropped_buffers;
    out->usb_errors = usb_errors;
    out->flags = flags;
}
