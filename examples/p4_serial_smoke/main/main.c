/*
 * Minimal ESP-IDF smoke for esp_rtl_sdr public API contract (v0.6).
 *
 * Build with EXTRA_COMPONENT_DIRS pointing at the esp_rtl_sdr repo root.
 * With a Blog V4 attached on ESP32-P4 HS host, start() should stream;
 * without a dongle, NO_DEVICE is expected.
 */
#include <stdio.h>
#include <string.h>

#include "esp_log.h"
#include "nvs_flash.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_rtl_sdr.h"

static const char *TAG = "esp_rtl_sdr_smoke";

static void on_event(esp_rtl_sdr_event_t event, const void *payload, void *ctx)
{
    (void)payload;
    (void)ctx;
    ESP_LOGI(TAG, "event=%d", (int)event);
}

static void check_pure_helpers(void)
{
    uint32_t q = 0;
    if (!esp_rtl_sdr_normalize_frequency(96123456, &q) || q != 96123000) {
        ESP_LOGE(TAG, "normalize_frequency failed");
    }
    if (esp_rtl_sdr_normalize_frequency(1000, &q)) {
        ESP_LOGE(TAG, "normalize should reject out-of-range");
    }

    uint32_t preset_hz = 0;
    ESP_ERROR_CHECK(esp_rtl_sdr_preset_frequency_hz(ESP_RTL_SDR_PRESET_KZEL_96_1, &preset_hz));
    if (preset_hz != ESP_RTL_SDR_PRESET_KZEL_HZ) {
        ESP_LOGE(TAG, "preset KZEL mismatch");
    }

    size_t n = 0;
    ESP_ERROR_CHECK(esp_rtl_sdr_get_supported_rates(NULL, 0, &n));
    if (n < 3) {
        ESP_LOGE(TAG, "supported rates too few: %u", (unsigned)n);
    }
    uint32_t rates[16];
    size_t written = 0;
    if (n > sizeof(rates) / sizeof(rates[0])) {
        n = sizeof(rates) / sizeof(rates[0]);
    }
    ESP_ERROR_CHECK(esp_rtl_sdr_get_supported_rates(rates, n, &written));
    if (!esp_rtl_sdr_is_rate_supported(ESP_RTL_SDR_RATE_960K) ||
        !esp_rtl_sdr_is_rate_supported(ESP_RTL_SDR_RATE_2048K) ||
        !esp_rtl_sdr_is_rate_supported(ESP_RTL_SDR_RATE_250K) ||
        !esp_rtl_sdr_is_rate_supported(ESP_RTL_SDR_RATE_3200K)) {
        ESP_LOGE(TAG, "expected expanded rate allowlist");
    }
    if (esp_rtl_sdr_is_rate_supported(12345)) {
        ESP_LOGE(TAG, "junk rate must be rejected");
    }

    const uint32_t caps = esp_rtl_sdr_get_capabilities();
    const uint32_t need = ESP_RTL_SDR_CAP_STREAM | ESP_RTL_SDR_CAP_RETUNE |
                          ESP_RTL_SDR_CAP_SYNC_READ | ESP_RTL_SDR_CAP_FREQ_CORRECTION |
                          ESP_RTL_SDR_CAP_MULTI_DEVICE;
    if ((caps & need) != need) {
        ESP_LOGE(TAG, "missing Phase 1/2 caps: got=0x%08x need=0x%08x",
                 (unsigned)caps, (unsigned)need);
    }

    ESP_LOGI(TAG, "helpers ok; rates=%u version=%s packed=0x%08x caps=0x%08x",
             (unsigned)written, esp_rtl_sdr_get_version_string(),
             (unsigned)esp_rtl_sdr_get_version(), (unsigned)caps);
}

void app_main(void)
{
    ESP_ERROR_CHECK(nvs_flash_init());

    ESP_LOGI(TAG, "esp_rtl_sdr %s caps=0x%08x", esp_rtl_sdr_get_version_string(),
             (unsigned)esp_rtl_sdr_get_capabilities());

    check_pure_helpers();

    /* Bad struct_size must fail closed. */
    esp_rtl_sdr_config_t bad;
    esp_rtl_sdr_config_default(&bad);
    bad.struct_size = 1;
    if (esp_rtl_sdr_config_validate(&bad) == ESP_OK) {
        ESP_LOGE(TAG, "expected struct_size reject");
    }

    esp_rtl_sdr_config_t cfg;
    esp_rtl_sdr_config_default(&cfg);
    cfg.event_cb = on_event;
    ESP_ERROR_CHECK(esp_rtl_sdr_config_validate(&cfg));

    esp_rtl_sdr_handle_t sdr = NULL;
    ESP_ERROR_CHECK(esp_rtl_sdr_install(&cfg, &sdr));
    ESP_LOGI(TAG, "state=%s", esp_rtl_sdr_state_to_name(esp_rtl_sdr_get_state(sdr)));

    /* Phase 2 multi-device queries (count may be 0 without dongle). */
    (void)esp_rtl_sdr_refresh_device_list(sdr);
    size_t dev_count = 0;
    if (esp_rtl_sdr_get_device_count(sdr, &dev_count) == ESP_OK) {
        ESP_LOGI(TAG, "device candidates=%u", (unsigned)dev_count);
        for (size_t i = 0; i < dev_count; ++i) {
            esp_rtl_sdr_device_info_t di;
            if (esp_rtl_sdr_get_device_at(sdr, i, &di) == ESP_OK) {
                ESP_LOGI(TAG, "  [%u] %04x:%04x %s serial=%s", (unsigned)i, di.vid, di.pid,
                         di.product, di.serial);
            }
        }
    }
    {
        esp_rtl_sdr_device_info_t bogus;
        if (esp_rtl_sdr_get_device_at(sdr, 99, &bogus) != ESP_RTL_SDR_ERR_BAD_DEVICE) {
            ESP_LOGE(TAG, "expected BAD_DEVICE for out-of-range index");
        }
    }

    esp_rtl_sdr_device_info_t info;
    if (esp_rtl_sdr_get_device_info(sdr, &info) == ESP_OK) {
        ESP_LOGI(TAG, "open filter %04x:%04x %s %s present=%d", info.vid, info.pid,
                 info.manufacturer, info.product, (int)info.present);
    }

    esp_rtl_sdr_metrics_t metrics;
    memset(&metrics, 0, sizeof(metrics));
    ESP_ERROR_CHECK(esp_rtl_sdr_get_metrics(sdr, &metrics));

    /* Phase 1 desktop-shaped helpers (idle path). */
    ESP_ERROR_CHECK(esp_rtl_sdr_set_sample_rate(sdr, ESP_RTL_SDR_RATE_960K));
    ESP_ERROR_CHECK(esp_rtl_sdr_set_center_freq(sdr, 96100000));
    ESP_ERROR_CHECK(esp_rtl_sdr_set_freq_correction(sdr, -12));
    int ppm = 0;
    ESP_ERROR_CHECK(esp_rtl_sdr_get_freq_correction(sdr, &ppm));
    if (ppm != -12) {
        ESP_LOGE(TAG, "ppm mismatch got=%d", ppm);
    }
    if (esp_rtl_sdr_set_freq_correction(sdr, 9999) == ESP_OK) {
        ESP_LOGE(TAG, "out-of-range ppm must fail");
    }
    uint32_t got_hz = 0, got_sps = 0;
    ESP_ERROR_CHECK(esp_rtl_sdr_get_center_freq(sdr, &got_hz));
    ESP_ERROR_CHECK(esp_rtl_sdr_get_sample_rate(sdr, &got_sps));
    ESP_LOGI(TAG, "preferred freq=%u sps=%u ppm=%d", (unsigned)got_hz, (unsigned)got_sps, ppm);

    esp_rtl_sdr_stream_config_t stream;
    esp_rtl_sdr_stream_config_default(&stream);
    ESP_ERROR_CHECK(esp_rtl_sdr_stream_config_validate(&stream));

    /* Bad rate */
    stream.sample_rate_sps = 12345;
    if (esp_rtl_sdr_stream_config_validate(&stream) != ESP_RTL_SDR_ERR_BAD_RATE) {
        ESP_LOGE(TAG, "expected BAD_RATE");
    }
    stream.sample_rate_sps = ESP_RTL_SDR_RATE_960K;

    esp_err_t err = esp_rtl_sdr_start(sdr, &stream);
    ESP_LOGW(TAG, "start -> %s (NO_DEVICE ok if no dongle; OK if V4 attached)",
             esp_rtl_sdr_err_to_name(err));
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "stream active; state=%s",
                 esp_rtl_sdr_state_to_name(esp_rtl_sdr_get_state(sdr)));
        err = esp_rtl_sdr_set_center_freq(sdr, 100100000);
        ESP_LOGI(TAG, "set_center_freq -> %s", esp_rtl_sdr_err_to_name(err));
        err = esp_rtl_sdr_set_freq_correction(sdr, 5);
        ESP_LOGI(TAG, "set_freq_correction while streaming -> %s", esp_rtl_sdr_err_to_name(err));
        /* Mid-stream rate change must be BUSY */
        if (esp_rtl_sdr_set_sample_rate(sdr, ESP_RTL_SDR_RATE_2048K) != ESP_RTL_SDR_ERR_BUSY) {
            ESP_LOGE(TAG, "expected BUSY for mid-stream rate change");
        }
        uint8_t iq[4096];
        size_t nbytes = 0;
        err = esp_rtl_sdr_read(sdr, iq, sizeof(iq), 500, &nbytes);
        ESP_LOGI(TAG, "read -> %s bytes=%u", esp_rtl_sdr_err_to_name(err), (unsigned)nbytes);
        vTaskDelay(pdMS_TO_TICKS(200));
    } else if (err != ESP_RTL_SDR_ERR_NO_DEVICE) {
        /* Failed start must not leave half-open USB (FAULT or IDLE only). */
        esp_rtl_sdr_state_t st = esp_rtl_sdr_get_state(sdr);
        if (st == ESP_RTL_SDR_STATE_STREAMING) {
            ESP_LOGE(TAG, "handle left STREAMING after start failure");
        }
    }

    /* Idempotent stop + uninstall */
    ESP_ERROR_CHECK(esp_rtl_sdr_stop(sdr, 1000));
    ESP_ERROR_CHECK(esp_rtl_sdr_stop(sdr, 0));
    ESP_ERROR_CHECK(esp_rtl_sdr_uninstall(sdr));
    sdr = NULL;

    if (esp_rtl_sdr_uninstall(NULL) != ESP_OK) {
        ESP_LOGE(TAG, "uninstall(NULL) must be ESP_OK");
    }

    ESP_LOGI(TAG, "smoke complete");
}
