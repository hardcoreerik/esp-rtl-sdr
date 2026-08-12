/*
 * Minimal ESP-IDF smoke for esp_rtl_sdr public API contract (v0.7).
 *
 * Build with EXTRA_COMPONENT_DIRS pointing at the esp_rtl_sdr repo root.
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
    (void)ctx;
    if (event == ESP_RTL_SDR_EVT_PASSPORT_PROGRESS && payload != NULL) {
        const esp_rtl_sdr_passport_entry_t *e = payload;
        ESP_LOGI(TAG, "passport progress rate=%u eff=%u stable=%d",
                 (unsigned)e->exact_sps, (unsigned)e->effective_sps, (int)e->stable);
        return;
    }
    ESP_LOGI(TAG, "event=%d", (int)event);
}

static void check_pure_helpers(void)
{
    uint32_t q = 0;
    if (!esp_rtl_sdr_normalize_frequency(96123456, &q) || q != 96123000) {
        ESP_LOGE(TAG, "normalize_frequency failed");
    }

    uint32_t exact = 0;
    if (!esp_rtl_sdr_quantize_sample_rate(2048000, &exact) || exact == 0) {
        ESP_LOGE(TAG, "quantize 2048k failed");
    }
    if (!esp_rtl_sdr_is_rate_supported(1536000)) {
        ESP_LOGE(TAG, "1536k continuous should be supported");
    }
    if (esp_rtl_sdr_is_rate_supported(500000)) {
        ESP_LOGE(TAG, "500k gap band must be rejected");
    }
    if (esp_rtl_sdr_is_rate_supported(4000000)) {
        ESP_LOGE(TAG, "4M above max must be rejected");
    }

    size_t n = 0;
    ESP_ERROR_CHECK(esp_rtl_sdr_get_supported_rates(NULL, 0, &n));
    uint32_t rates[16];
    size_t written = 0;
    if (n > sizeof(rates) / sizeof(rates[0])) {
        n = sizeof(rates) / sizeof(rates[0]);
    }
    ESP_ERROR_CHECK(esp_rtl_sdr_get_supported_rates(rates, n, &written));

    const uint32_t caps = esp_rtl_sdr_get_capabilities();
    const uint32_t need =
        ESP_RTL_SDR_CAP_CONTINUOUS_RATE | ESP_RTL_SDR_CAP_NEED | ESP_RTL_SDR_CAP_HEALTH |
        ESP_RTL_SDR_CAP_PASSPORT | ESP_RTL_SDR_CAP_SYNC_READ;
    if ((caps & need) != need) {
        ESP_LOGE(TAG, "missing 0.7 caps got=0x%08x need=0x%08x", (unsigned)caps,
                 (unsigned)need);
    }

    ESP_LOGI(TAG, "helpers ok rates=%u exact2048=%u version=%s caps=0x%08x",
             (unsigned)written, (unsigned)exact, esp_rtl_sdr_get_version_string(),
             (unsigned)caps);
}

void app_main(void)
{
    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_LOGI(TAG, "esp_rtl_sdr %s", esp_rtl_sdr_get_version_string());
    check_pure_helpers();

    esp_rtl_sdr_config_t cfg;
    esp_rtl_sdr_config_default(&cfg);
    cfg.event_cb = on_event;
    ESP_ERROR_CHECK(esp_rtl_sdr_config_validate(&cfg));

    esp_rtl_sdr_handle_t sdr = NULL;
    ESP_ERROR_CHECK(esp_rtl_sdr_install(&cfg, &sdr));

    ESP_ERROR_CHECK(esp_rtl_sdr_apply_need(sdr, ESP_RTL_SDR_NEED_FM));
    ESP_ERROR_CHECK(esp_rtl_sdr_apply_need(sdr, ESP_RTL_SDR_NEED_ADSB));
    uint32_t hz = 0, sps = 0;
    ESP_ERROR_CHECK(esp_rtl_sdr_get_center_freq(sdr, &hz));
    ESP_ERROR_CHECK(esp_rtl_sdr_get_sample_rate(sdr, &sps));
    ESP_LOGI(TAG, "after NEED_ADSB freq=%u sps=%u", (unsigned)hz, (unsigned)sps);

    /* Continuous custom rate */
    ESP_ERROR_CHECK(esp_rtl_sdr_set_sample_rate(sdr, 1536000));
    ESP_ERROR_CHECK(esp_rtl_sdr_get_sample_rate(sdr, &sps));
    ESP_LOGI(TAG, "continuous 1536k -> exact %u", (unsigned)sps);

    esp_rtl_sdr_health_info_t health;
    ESP_ERROR_CHECK(esp_rtl_sdr_get_health(sdr, &health));
    ESP_LOGI(TAG, "health idle advice=%s", health.advice);

    (void)esp_rtl_sdr_refresh_device_list(sdr);
    size_t dev_count = 0;
    (void)esp_rtl_sdr_get_device_count(sdr, &dev_count);
    ESP_LOGI(TAG, "candidates=%u", (unsigned)dev_count);

    /* Optional short passport if dongle present (can take ~entry*dwell). */
    if (dev_count > 0) {
        esp_rtl_sdr_passport_opts_t popts;
        esp_rtl_sdr_passport_opts_default(&popts);
        popts.dwell_ms = 800; /* short smoke dwell */
        popts.recommended_only = true;
        esp_rtl_sdr_rate_passport_t pass;
        esp_err_t perr = esp_rtl_sdr_probe_rates(sdr, &popts, &pass);
        ESP_LOGW(TAG, "probe_rates -> %s best_stable=%u entries=%u",
                 esp_rtl_sdr_err_to_name(perr), (unsigned)pass.best_stable_sps,
                 (unsigned)pass.entry_count);
        if (perr == ESP_OK) {
            ESP_ERROR_CHECK(esp_rtl_sdr_apply_need(sdr, ESP_RTL_SDR_NEED_MAX_STABLE));
        }
    }

    esp_rtl_sdr_stream_config_t stream;
    esp_rtl_sdr_stream_config_default(&stream);
    stream.preset = ESP_RTL_SDR_PRESET_CUSTOM_HZ;
    stream.frequency_hz = 0; /* preferred */
    stream.sample_rate_sps = 0;
    esp_err_t err = esp_rtl_sdr_start(sdr, &stream);
    ESP_LOGW(TAG, "start -> %s", esp_rtl_sdr_err_to_name(err));
    if (err == ESP_OK) {
        vTaskDelay(pdMS_TO_TICKS(400));
        ESP_ERROR_CHECK(esp_rtl_sdr_get_health(sdr, &health));
        ESP_LOGI(TAG, "health streaming overall=%d eff=%.2f advice=%s",
                 (int)health.overall, (double)health.efficiency, health.advice);
        uint8_t iq[4096];
        size_t nbytes = 0;
        err = esp_rtl_sdr_read(sdr, iq, sizeof(iq), 500, &nbytes);
        ESP_LOGI(TAG, "read -> %s bytes=%u", esp_rtl_sdr_err_to_name(err),
                 (unsigned)nbytes);
    }

    ESP_ERROR_CHECK(esp_rtl_sdr_stop(sdr, 1000));
    ESP_ERROR_CHECK(esp_rtl_sdr_uninstall(sdr));
    ESP_LOGI(TAG, "smoke complete");
}
