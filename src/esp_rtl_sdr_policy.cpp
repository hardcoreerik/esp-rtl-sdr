/*
 * Pure policy helpers — no USB, FreeRTOS, or host tasks.
 * Host unit tests compile this file alone (see tests/host).
 */

#include "esp_rtl_sdr.h"

#include <cstddef>
#include <cstring>

/* -------------------------------------------------------------------------- */
/* Recommended rates (discovery / passport defaults)                          */
/* -------------------------------------------------------------------------- */

static const uint32_t kRecommendedRates[] = {
    ESP_RTL_SDR_RATE_250K,  ESP_RTL_SDR_RATE_256K,  ESP_RTL_SDR_RATE_960K,
    ESP_RTL_SDR_RATE_1024K, ESP_RTL_SDR_RATE_1800K, ESP_RTL_SDR_RATE_2048K,
    ESP_RTL_SDR_RATE_2400K, ESP_RTL_SDR_RATE_2560K, ESP_RTL_SDR_RATE_3200K,
};

/* -------------------------------------------------------------------------- */
/* Version / capabilities                                                     */
/* -------------------------------------------------------------------------- */

uint32_t esp_rtl_sdr_get_version(void)
{
    return (static_cast<uint32_t>(ESP_RTL_SDR_VERSION_MAJOR) << 16) |
           (static_cast<uint32_t>(ESP_RTL_SDR_VERSION_MINOR) << 8) |
           static_cast<uint32_t>(ESP_RTL_SDR_VERSION_PATCH);
}

const char *esp_rtl_sdr_get_version_string(void)
{
    return ESP_RTL_SDR_VERSION_STRING;
}

uint32_t esp_rtl_sdr_get_capabilities(void)
{
    /* MEASURED_2026_08_12: CAP_GAIN + CAP_BIAS_TEE from lab USBPcap (Blog V4). */
    return ESP_RTL_SDR_CAP_STREAM | ESP_RTL_SDR_CAP_RETUNE | ESP_RTL_SDR_CAP_METRICS |
           ESP_RTL_SDR_CAP_CUSTOM_HZ | ESP_RTL_SDR_CAP_HOTPLUG |
           ESP_RTL_SDR_CAP_FREQ_CORRECTION | ESP_RTL_SDR_CAP_MULTI_DEVICE |
           ESP_RTL_SDR_CAP_SYNC_READ | ESP_RTL_SDR_CAP_CONTINUOUS_RATE |
           ESP_RTL_SDR_CAP_NEED | ESP_RTL_SDR_CAP_HEALTH | ESP_RTL_SDR_CAP_PASSPORT |
           ESP_RTL_SDR_CAP_DELIVERY_MODE | ESP_RTL_SDR_CAP_GAIN | ESP_RTL_SDR_CAP_BIAS_TEE;
}

bool esp_rtl_sdr_delivery_mode_uses_callback_iq(esp_rtl_sdr_delivery_mode_t mode)
{
    return mode == ESP_RTL_SDR_DELIVERY_BOTH || mode == ESP_RTL_SDR_DELIVERY_CALLBACK;
}

bool esp_rtl_sdr_delivery_mode_uses_read(esp_rtl_sdr_delivery_mode_t mode)
{
    return mode == ESP_RTL_SDR_DELIVERY_BOTH || mode == ESP_RTL_SDR_DELIVERY_READ;
}

const char *esp_rtl_sdr_state_to_name(esp_rtl_sdr_state_t state)
{
    switch (state) {
    case ESP_RTL_SDR_STATE_UNINSTALLED: return "UNINSTALLED";
    case ESP_RTL_SDR_STATE_IDLE: return "IDLE";
    case ESP_RTL_SDR_STATE_STREAMING: return "STREAMING";
    case ESP_RTL_SDR_STATE_STOPPING: return "STOPPING";
    case ESP_RTL_SDR_STATE_FAULT: return "FAULT";
    case ESP_RTL_SDR_STATE_STARTING: return "STARTING";
    default: return "UNKNOWN";
    }
}

const char *esp_rtl_sdr_err_to_name(esp_err_t err)
{
    switch (err) {
    case ESP_OK: return "ESP_OK";
    case ESP_ERR_INVALID_ARG: return "ESP_ERR_INVALID_ARG";
    case ESP_ERR_INVALID_STATE: return "ESP_ERR_INVALID_STATE";
    case ESP_ERR_INVALID_SIZE: return "ESP_ERR_INVALID_SIZE";
    case ESP_ERR_NO_MEM: return "ESP_ERR_NO_MEM";
    case ESP_ERR_TIMEOUT: return "ESP_ERR_TIMEOUT";
    case ESP_RTL_SDR_ERR_NO_DEVICE: return "ESP_RTL_SDR_ERR_NO_DEVICE";
    case ESP_RTL_SDR_ERR_NOT_V4: return "ESP_RTL_SDR_ERR_UNSUPPORTED_DEVICE";
    case ESP_RTL_SDR_ERR_BUSY: return "ESP_RTL_SDR_ERR_BUSY";
    case ESP_RTL_SDR_ERR_BAD_DEVICE: return "ESP_RTL_SDR_ERR_BAD_DEVICE";
    case ESP_RTL_SDR_ERR_NOT_STREAMING: return "ESP_RTL_SDR_ERR_NOT_STREAMING";
    case ESP_RTL_SDR_ERR_BAD_RATE: return "ESP_RTL_SDR_ERR_BAD_RATE";
    case ESP_RTL_SDR_ERR_BAD_FREQ: return "ESP_RTL_SDR_ERR_BAD_FREQ";
    case ESP_RTL_SDR_ERR_USB: return "ESP_RTL_SDR_ERR_USB";
    case ESP_RTL_SDR_ERR_TIMEOUT: return "ESP_RTL_SDR_ERR_TIMEOUT";
    case ESP_RTL_SDR_ERR_FAULT: return "ESP_RTL_SDR_ERR_FAULT";
    case ESP_RTL_SDR_ERR_NOT_READY: return "ESP_RTL_SDR_ERR_NOT_READY";
    case ESP_RTL_SDR_ERR_UNSUPPORTED: return "ESP_RTL_SDR_ERR_UNSUPPORTED";
    case ESP_RTL_SDR_ERR_STALE_HANDLE: return "ESP_RTL_SDR_ERR_STALE_HANDLE";
    case ESP_RTL_SDR_ERR_REENTRANT: return "ESP_RTL_SDR_ERR_REENTRANT";
    case ESP_RTL_SDR_ERR_NOT_CLAIMED: return "ESP_RTL_SDR_ERR_NOT_CLAIMED";
    default:
#if defined(ESP_PLATFORM)
        return esp_err_to_name(err);
#else
        return "UNKNOWN_ERR";
#endif
    }
}

/* -------------------------------------------------------------------------- */
/* Rates / frequency                                                          */
/* -------------------------------------------------------------------------- */

static bool rate_in_hardware_window(uint32_t sps)
{
    if (sps >= ESP_RTL_SDR_RATE_LOW_MIN_HZ && sps <= ESP_RTL_SDR_RATE_LOW_MAX_HZ) {
        return true;
    }
    if (sps >= ESP_RTL_SDR_RATE_HIGH_MIN_HZ && sps <= ESP_RTL_SDR_RATE_HIGH_MAX_HZ) {
        return true;
    }
    return false;
}

bool esp_rtl_sdr_quantize_sample_rate(uint32_t requested_sps, uint32_t *out_exact_sps)
{
    if (out_exact_sps == nullptr || requested_sps == 0) {
        return false;
    }
    if (!rate_in_hardware_window(requested_sps)) {
        return false;
    }
    /* 28-bit resampler field (low 2 bits clear) — same mask family as ecosystem drivers. */
    uint32_t ratio =
        static_cast<uint32_t>((static_cast<uint64_t>(ESP_RTL_SDR_XTAL_HZ) << 22) / requested_sps);
    ratio &= 0x0ffffffcu;
    if (ratio == 0) {
        /* e.g. request 225000 → raw ratio 0x20000000 → mask zeroes the field */
        return false;
    }
    const uint32_t exact = static_cast<uint32_t>(
        (static_cast<uint64_t>(ESP_RTL_SDR_XTAL_HZ) << 22) / ratio);
    if (exact == 0) {
        return false;
    }
    /* Exact programmed rate may differ from request (integer ratio); still accepted. */
    *out_exact_sps = exact;
    return true;
}

bool esp_rtl_sdr_is_rate_supported(uint32_t sample_rate_sps)
{
    uint32_t exact = 0;
    return esp_rtl_sdr_quantize_sample_rate(sample_rate_sps, &exact);
}

esp_err_t esp_rtl_sdr_get_supported_rates(uint32_t *out_rates, size_t max_count,
                                          size_t *out_count)
{
    const size_t total = sizeof(kRecommendedRates) / sizeof(kRecommendedRates[0]);
    if (out_count == nullptr) {
        return ESP_ERR_INVALID_ARG;
    }
    if (max_count == 0) {
        *out_count = total;
        return ESP_OK;
    }
    if (out_rates == nullptr) {
        return ESP_ERR_INVALID_ARG;
    }
    const size_t n = (max_count < total) ? max_count : total;
    for (size_t i = 0; i < n; ++i) {
        out_rates[i] = kRecommendedRates[i];
    }
    *out_count = total;
    return (max_count < total) ? ESP_ERR_INVALID_SIZE : ESP_OK;
}

bool esp_rtl_sdr_normalize_frequency(uint32_t in_hz, uint32_t *out_hz)
{
    if (out_hz == nullptr) {
        return false;
    }
    if (in_hz < ESP_RTL_SDR_FREQ_MIN_HZ || in_hz > ESP_RTL_SDR_FREQ_MAX_HZ) {
        return false;
    }
    uint32_t q = (in_hz / ESP_RTL_SDR_FREQ_QUANT_HZ) * ESP_RTL_SDR_FREQ_QUANT_HZ;
    if (q < ESP_RTL_SDR_FREQ_MIN_HZ) {
        q = ESP_RTL_SDR_FREQ_MIN_HZ;
    }
    if (q > ESP_RTL_SDR_FREQ_MAX_HZ) {
        q = ESP_RTL_SDR_FREQ_MAX_HZ;
    }
    *out_hz = q;
    return true;
}

esp_err_t esp_rtl_sdr_preset_frequency_hz(esp_rtl_sdr_preset_t preset, uint32_t *out_hz)
{
    if (out_hz == nullptr) {
        return ESP_ERR_INVALID_ARG;
    }
    switch (preset) {
    case ESP_RTL_SDR_PRESET_KZEL_96_1:
        *out_hz = ESP_RTL_SDR_PRESET_KZEL_HZ;
        return ESP_OK;
    case ESP_RTL_SDR_PRESET_NOAA_162_4:
        *out_hz = ESP_RTL_SDR_PRESET_NOAA_HZ;
        return ESP_OK;
    default:
        return ESP_ERR_INVALID_ARG;
    }
}

/* -------------------------------------------------------------------------- */
/* Config defaults / validate                                                 */
/* -------------------------------------------------------------------------- */

static bool is_xfer_bytes_ok(size_t n)
{
    if (n < ESP_RTL_SDR_MIN_XFER_BYTES || n > ESP_RTL_SDR_MAX_XFER_BYTES) {
        return false;
    }
    return (n % 512u) == 0;
}

void esp_rtl_sdr_config_default(esp_rtl_sdr_config_t *config)
{
    if (config == nullptr) {
        return;
    }
    std::memset(config, 0, sizeof(*config));
    config->struct_size = sizeof(esp_rtl_sdr_config_t);
    config->host_library_already_installed = false;
#if defined(CONFIG_ESP_RTL_SDR_ESP_DEFAULT_TRANSFER_BYTES)
    config->transfer_bytes = static_cast<size_t>(CONFIG_ESP_RTL_SDR_ESP_DEFAULT_TRANSFER_BYTES);
#else
    config->transfer_bytes = ESP_RTL_SDR_DEFAULT_XFER_BYTES;
#endif
#if defined(CONFIG_ESP_RTL_SDR_ESP_DEFAULT_TRANSFER_COUNT)
    config->transfer_count = static_cast<size_t>(CONFIG_ESP_RTL_SDR_ESP_DEFAULT_TRANSFER_COUNT);
#else
    config->transfer_count = ESP_RTL_SDR_DEFAULT_XFER_COUNT;
#endif
    config->control_timeout_ms = 1000;
    config->usb_task_priority = 0;
    config->usb_task_core_id = 0xFF;
    config->delivery_mode = ESP_RTL_SDR_DELIVERY_BOTH;
    config->pull_ring_bytes = 0;
}

void esp_rtl_sdr_stream_config_default(esp_rtl_sdr_stream_config_t *stream)
{
    if (stream == nullptr) {
        return;
    }
    std::memset(stream, 0, sizeof(*stream));
    stream->struct_size = sizeof(esp_rtl_sdr_stream_config_t);
    stream->preset = ESP_RTL_SDR_PRESET_KZEL_96_1;
    stream->frequency_hz = ESP_RTL_SDR_PRESET_KZEL_HZ;
    stream->sample_rate_sps = ESP_RTL_SDR_RATE_960K;
}

/* Append-only ABI: accept [min, sizeof]; smaller structs get defaults for new fields. */
static constexpr size_t kConfigSizeMin =
    offsetof(esp_rtl_sdr_config_t, usb_task_core_id) + sizeof(uint8_t);
static constexpr size_t kStreamSizeMin =
    offsetof(esp_rtl_sdr_stream_config_t, timeout_ms) + sizeof(uint32_t);

esp_err_t esp_rtl_sdr_config_validate(const esp_rtl_sdr_config_t *config)
{
    if (config == nullptr) {
        return ESP_ERR_INVALID_ARG;
    }
    if (config->struct_size < kConfigSizeMin ||
        config->struct_size > sizeof(esp_rtl_sdr_config_t)) {
        return ESP_ERR_INVALID_ARG;
    }
    esp_rtl_sdr_config_t local;
    esp_rtl_sdr_config_default(&local);
    std::memcpy(&local, config, config->struct_size);
    local.struct_size = sizeof(local);

    if (!is_xfer_bytes_ok(local.transfer_bytes)) {
        return ESP_ERR_INVALID_ARG;
    }
    if (local.transfer_count < ESP_RTL_SDR_MIN_XFER_COUNT ||
        local.transfer_count > ESP_RTL_SDR_MAX_XFER_COUNT) {
        return ESP_ERR_INVALID_ARG;
    }
    if (local.control_timeout_ms == 0 ||
        local.control_timeout_ms > ESP_RTL_SDR_MAX_TIMEOUT_MS) {
        return ESP_ERR_INVALID_ARG;
    }
    if (local.usb_task_core_id != 0xFF && local.usb_task_core_id > 1) {
        return ESP_ERR_INVALID_ARG;
    }
    if (local.delivery_mode > ESP_RTL_SDR_DELIVERY_READ) {
        return ESP_ERR_INVALID_ARG;
    }
    /* pull_ring_bytes 0 = auto; else must be even and within a sane bound */
    if (local.pull_ring_bytes != 0) {
        if ((local.pull_ring_bytes % 2u) != 0 || local.pull_ring_bytes < 1024u ||
            local.pull_ring_bytes > (1024u * 1024u)) {
            return ESP_ERR_INVALID_ARG;
        }
    }
    return ESP_OK;
}

esp_err_t esp_rtl_sdr_stream_config_validate(const esp_rtl_sdr_stream_config_t *stream)
{
    if (stream == nullptr) {
        return ESP_ERR_INVALID_ARG;
    }
    if (stream->struct_size < kStreamSizeMin ||
        stream->struct_size > sizeof(esp_rtl_sdr_stream_config_t)) {
        return ESP_ERR_INVALID_ARG;
    }
    esp_rtl_sdr_stream_config_t local;
    esp_rtl_sdr_stream_config_default(&local);
    std::memcpy(&local, stream, stream->struct_size);
    local.struct_size = sizeof(local);

    if (local.preset > ESP_RTL_SDR_PRESET_CUSTOM_HZ) {
        return ESP_ERR_INVALID_ARG;
    }
    if (local.sample_rate_sps != 0 &&
        !esp_rtl_sdr_is_rate_supported(local.sample_rate_sps)) {
        return ESP_RTL_SDR_ERR_BAD_RATE;
    }
    if (local.preset == ESP_RTL_SDR_PRESET_CUSTOM_HZ) {
        uint32_t q = 0;
        if (!esp_rtl_sdr_normalize_frequency(local.frequency_hz, &q)) {
            return ESP_RTL_SDR_ERR_BAD_FREQ;
        }
    }
    if (local.max_bytes != 0 && (local.max_bytes % 2u) != 0) {
        return ESP_ERR_INVALID_ARG;
    }
    if (local.timeout_ms > ESP_RTL_SDR_MAX_TIMEOUT_MS) {
        return ESP_ERR_INVALID_ARG;
    }
    return ESP_OK;
}

void esp_rtl_sdr_passport_opts_default(esp_rtl_sdr_passport_opts_t *opts)
{
    if (opts == nullptr) {
        return;
    }
    std::memset(opts, 0, sizeof(*opts));
    opts->struct_size = sizeof(*opts);
    opts->frequency_hz = 0;
    opts->dwell_ms = ESP_RTL_SDR_PASSPORT_DEFAULT_DWELL_MS;
    opts->min_efficiency_pct = 95;
    opts->recommended_only = true;
}
