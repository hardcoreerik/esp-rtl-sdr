/*
 * Minimal ESP-IDF smoke for esp_rtl_sdr public API contract (v0.3+).
 *
 * Build once EXTRA_COMPONENT_DIRS points at ../../components.
 * start() is expected to return ERR_UNSUPPORTED until Gate 2 extraction.
 */
#include <stdio.h>
#include <string.h>

#include "esp_log.h"
#include "nvs_flash.h"
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
    if (n == 0) {
        ESP_LOGE(TAG, "supported rates empty");
    }
    uint32_t rates[8];
    size_t written = 0;
    ESP_ERROR_CHECK(esp_rtl_sdr_get_supported_rates(rates, n, &written));
    if (!esp_rtl_sdr_is_rate_supported(ESP_RTL_SDR_RATE_960K)) {
        ESP_LOGE(TAG, "960k should be supported");
    }

    ESP_LOGI(TAG, "helpers ok; rates=%u version=%s packed=0x%08x",
             (unsigned)written, esp_rtl_sdr_get_version_string(),
             (unsigned)esp_rtl_sdr_get_version());
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

    esp_rtl_sdr_device_info_t info;
    if (esp_rtl_sdr_get_device_info(sdr, &info) == ESP_OK) {
        ESP_LOGI(TAG, "filter %04x:%04x %s %s present=%d", info.vid, info.pid,
                 info.manufacturer, info.product, (int)info.present);
    }

    esp_rtl_sdr_metrics_t metrics;
    memset(&metrics, 0, sizeof(metrics));
    ESP_ERROR_CHECK(esp_rtl_sdr_get_metrics(sdr, &metrics));

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
        err = esp_rtl_sdr_retune_hz(sdr, 100100000);
        ESP_LOGI(TAG, "retune_hz (queue-only) -> %s", esp_rtl_sdr_err_to_name(err));
        vTaskDelay(pdMS_TO_TICKS(500));
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
