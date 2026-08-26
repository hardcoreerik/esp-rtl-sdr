/*
 * ESP-IDF Tab5 / P4 drop-in smoke for esp_rtl_sdr public API (v0.7.9).
 *
 * Default config only: delivery BOTH, pull_ring_bytes = 0 (auto). The driver
 * must allocate on no-PSRAM Tab5-class heaps; do not hardcode 64 KiB here.
 * IQ event logging is suppressed so serial cannot stall the delivery path.
 *
 * Grep-friendly rows:
 *   SMOKE <name> PASS|FAIL
 *   SMOKE OVERALL PASS|FAIL passed=<n> failed=<n> hardware=<RUN|SKIP>
 */
#include <stdio.h>
#include <string.h>

#include "esp_log.h"
#include "nvs_flash.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_rtl_sdr.h"

static const char *TAG = "esp_rtl_sdr_smoke";

static int g_pass;
static int g_fail;

static void smoke_row(const char *name, bool ok)
{
    if (ok) {
        g_pass++;
        printf("SMOKE %s PASS\n", name);
    } else {
        g_fail++;
        printf("SMOKE %s FAIL\n", name);
        ESP_LOGE(TAG, "FAIL %s", name);
    }
}

static void on_event(esp_rtl_sdr_event_t event, const void *payload, void *ctx)
{
    (void)ctx;
    if (event == ESP_RTL_SDR_EVT_IQ_BLOCK) {
        return; /* never log IQ; serial stalls the delivery path */
    }
    if (event == ESP_RTL_SDR_EVT_PASSPORT_PROGRESS && payload != NULL) {
        const esp_rtl_sdr_passport_entry_t *e = payload;
        ESP_LOGI(TAG, "passport progress rate=%u eff=%u stable=%d",
                 (unsigned)e->exact_sps, (unsigned)e->effective_sps, (int)e->stable);
        return;
    }
}

static void settle_ms(int ms)
{
    vTaskDelay(pdMS_TO_TICKS(ms));
}

static bool smoke_read(esp_rtl_sdr_handle_t sdr, const char *name)
{
    uint8_t iq[4096];
    size_t n = 0;
    const esp_err_t err = esp_rtl_sdr_read(sdr, iq, sizeof(iq), 1000, &n);
    const bool ok = (err == ESP_OK && n == sizeof(iq));
    smoke_row(name, ok);
    ESP_LOGI(TAG, "%s -> %s bytes=%u", name, esp_rtl_sdr_err_to_name(err), (unsigned)n);
    return ok;
}

static size_t wait_devices(esp_rtl_sdr_handle_t sdr, int timeout_ms)
{
    size_t count = 0;
    int waited = 0;
    do {
        (void)esp_rtl_sdr_refresh_device_list(sdr);
        (void)esp_rtl_sdr_get_device_count(sdr, &count);
        if (count > 0) {
            return count;
        }
        settle_ms(200);
        waited += 200;
    } while (waited < timeout_ms);
    return 0;
}

static void check_pure_helpers(void)
{
    uint32_t q = 0;
    smoke_row("normalize_frequency",
              esp_rtl_sdr_normalize_frequency(96123456, &q) && q == 96123000);

    uint32_t exact = 0;
    smoke_row("quantize_2048k",
              esp_rtl_sdr_quantize_sample_rate(2048000, &exact) && exact != 0);
    smoke_row("rate_1536k_ok", esp_rtl_sdr_is_rate_supported(1536000));
    smoke_row("rate_500k_reject", !esp_rtl_sdr_is_rate_supported(500000));
    smoke_row("rate_4M_reject", !esp_rtl_sdr_is_rate_supported(4000000));

    const char *ver = esp_rtl_sdr_get_version_string();
    smoke_row("version_0_7_9", ver != NULL && strcmp(ver, "0.7.9") == 0);

    const uint32_t caps = esp_rtl_sdr_get_capabilities();
    smoke_row("cap_gain", (caps & ESP_RTL_SDR_CAP_GAIN) != 0);
    smoke_row("cap_gain_auto", (caps & ESP_RTL_SDR_CAP_GAIN_AUTO) != 0);
    smoke_row("cap_rtl_agc", (caps & ESP_RTL_SDR_CAP_RTL_AGC) != 0);
    smoke_row("cap_bias_tee", (caps & ESP_RTL_SDR_CAP_BIAS_TEE) != 0);
    smoke_row("cap_sync_read", (caps & ESP_RTL_SDR_CAP_SYNC_READ) != 0);

    ESP_LOGI(TAG, "helpers version=%s caps=0x%08x", ver, (unsigned)caps);
}

static void run_l4_matrix(esp_rtl_sdr_handle_t sdr)
{
    esp_err_t err;
    int gain = -1;
    esp_rtl_sdr_gain_mode_t mode = ESP_RTL_SDR_GAIN_MODE_MANUAL;
    bool rtl = true;

    err = esp_rtl_sdr_set_tuner_gain(sdr, 0);
    settle_ms(250);
    (void)esp_rtl_sdr_get_tuner_gain(sdr, &gain);
    smoke_row("manual_gain_0", err == ESP_OK && gain == 0);
    smoke_read(sdr, "manual_gain_0_read");

    err = esp_rtl_sdr_set_tuner_gain(sdr, 297);
    settle_ms(250);
    (void)esp_rtl_sdr_get_tuner_gain(sdr, &gain);
    smoke_row("manual_gain_297", err == ESP_OK && gain == 297);

    err = esp_rtl_sdr_set_tuner_gain(sdr, 400);
    settle_ms(250);
    (void)esp_rtl_sdr_get_tuner_gain(sdr, &gain);
    smoke_row("manual_gain_400_nearest_402", err == ESP_OK && gain == 402);

    err = esp_rtl_sdr_set_tuner_gain_mode(sdr, ESP_RTL_SDR_GAIN_MODE_AUTO);
    settle_ms(250);
    (void)esp_rtl_sdr_get_tuner_gain_mode(sdr, &mode);
    smoke_row("tuner_auto_set_get", err == ESP_OK && mode == ESP_RTL_SDR_GAIN_MODE_AUTO);

    err = esp_rtl_sdr_set_tuner_gain_mode(sdr, ESP_RTL_SDR_GAIN_MODE_AUTO);
    smoke_row("tuner_auto_idempotent", err == ESP_OK);
    smoke_read(sdr, "tuner_auto_read");

    err = esp_rtl_sdr_set_tuner_gain(sdr, 297);
    settle_ms(250);
    (void)esp_rtl_sdr_get_tuner_gain_mode(sdr, &mode);
    smoke_row("gain_forces_manual", err == ESP_OK && mode == ESP_RTL_SDR_GAIN_MODE_MANUAL);

    err = esp_rtl_sdr_set_tuner_gain_mode(sdr, ESP_RTL_SDR_GAIN_MODE_AUTO);
    settle_ms(250);
    (void)esp_rtl_sdr_get_tuner_gain_mode(sdr, &mode);
    smoke_row("tuner_auto_restore", err == ESP_OK && mode == ESP_RTL_SDR_GAIN_MODE_AUTO);

    err = esp_rtl_sdr_set_rtl_agc(sdr, true);
    settle_ms(250);
    (void)esp_rtl_sdr_get_rtl_agc(sdr, &rtl);
    (void)esp_rtl_sdr_get_tuner_gain_mode(sdr, &mode);
    smoke_row("rtl_agc_on", err == ESP_OK && rtl);
    smoke_row("rtl_agc_independent", mode == ESP_RTL_SDR_GAIN_MODE_AUTO);

    err = esp_rtl_sdr_set_rtl_agc(sdr, false);
    settle_ms(250);
    (void)esp_rtl_sdr_get_rtl_agc(sdr, &rtl);
    smoke_row("rtl_agc_off", err == ESP_OK && !rtl);
    smoke_read(sdr, "rtl_agc_read");

    err = esp_rtl_sdr_set_tuner_gain_mode(sdr, ESP_RTL_SDR_GAIN_MODE_MANUAL);
    settle_ms(250);
    (void)esp_rtl_sdr_get_tuner_gain_mode(sdr, &mode);
    smoke_row("manual_restore_mode", err == ESP_OK && mode == ESP_RTL_SDR_GAIN_MODE_MANUAL);

    err = esp_rtl_sdr_set_tuner_gain(sdr, 297);
    settle_ms(250);
    (void)esp_rtl_sdr_get_tuner_gain(sdr, &gain);
    smoke_row("manual_restore_gain", err == ESP_OK && gain == 297);
    smoke_read(sdr, "manual_restore_read");
}

void app_main(void)
{
    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_LOGI(TAG, "esp_rtl_sdr %s", esp_rtl_sdr_get_version_string());
    check_pure_helpers();

    esp_rtl_sdr_config_t cfg;
    esp_rtl_sdr_config_default(&cfg);
    cfg.event_cb = on_event;
    /* Default drop-in: BOTH + auto ring. Do not set pull_ring_bytes. */
    ESP_ERROR_CHECK(esp_rtl_sdr_config_validate(&cfg));
    smoke_row("default_auto_ring", cfg.pull_ring_bytes == 0 &&
                                       cfg.delivery_mode == ESP_RTL_SDR_DELIVERY_BOTH);

    esp_rtl_sdr_handle_t sdr = NULL;
    ESP_ERROR_CHECK(esp_rtl_sdr_install(&cfg, &sdr));

    const char *hw = "SKIP";
    const size_t dev_count = wait_devices(sdr, 3000);
    ESP_LOGI(TAG, "candidates=%u", (unsigned)dev_count);

    if (dev_count == 0) {
        smoke_row("no_device_skip", true);
    } else {
        hw = "RUN";
        esp_rtl_sdr_stream_config_t stream;
        esp_rtl_sdr_stream_config_default(&stream);
        stream.preset = ESP_RTL_SDR_PRESET_CUSTOM_HZ;
        stream.frequency_hz = 0;
        stream.sample_rate_sps = 0;
        const esp_err_t start_err = esp_rtl_sdr_start(sdr, &stream);
        smoke_row("start", start_err == ESP_OK);
        if (start_err == ESP_OK) {
            settle_ms(400);
            smoke_read(sdr, "first_read");
            run_l4_matrix(sdr);

            esp_rtl_sdr_metrics_t metrics;
            esp_rtl_sdr_health_info_t health;
            smoke_row("metrics_after", esp_rtl_sdr_get_metrics(sdr, &metrics) == ESP_OK &&
                                           metrics.bytes_total > 0);
            smoke_row("health_after", esp_rtl_sdr_get_health(sdr, &health) == ESP_OK);
            ESP_LOGI(TAG, "metrics bytes=%llu overruns=%u drops=%u sps=%u",
                     (unsigned long long)metrics.bytes_total, (unsigned)metrics.overruns,
                     (unsigned)metrics.consumer_drops, (unsigned)metrics.effective_sps);
            ESP_LOGI(TAG, "health overall=%d advice=%s", (int)health.overall, health.advice);
        }
    }

    smoke_row("stop", esp_rtl_sdr_stop(sdr, 1000) == ESP_OK);
    smoke_row("uninstall", esp_rtl_sdr_uninstall(sdr) == ESP_OK);

    const bool overall = (g_fail == 0);
    printf("SMOKE OVERALL %s passed=%d failed=%d hardware=%s\n",
           overall ? "PASS" : "FAIL", g_pass, g_fail, hw);
}
