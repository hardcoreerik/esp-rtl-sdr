/*
 * ESP-IDF Tab5 / P4 drop-in smoke for esp_rtl_sdr public API (v0.7.11).
 *
 * One USB host install per boot. URB layout is a Kconfig choice so A/B
 * (6x16 KiB vs 3x32 KiB) is two firmware images, not two installs.
 *
 * Quiet soak: BOTH + auto ring, drain via read() on a helper task, no
 * mid-stream EP0, then L4 gain / AUTO / RTL AGC matrix.
 *
 * Grep-friendly rows:
 *   SMOKE <name> PASS|FAIL
 *   SOAK <label> eff=<pct> sps=<n> bytes=<n> over=<n> drops=<n> short=<n> urbs=<c>x<b>
 *   SMOKE OVERALL PASS|FAIL passed=<n> failed=<n> hardware=<RUN|SKIP>
 */
#include <stdio.h>
#include <string.h>

#include "sdkconfig.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_rtl_sdr.h"

static const char *TAG = "esp_rtl_sdr_smoke";

#if CONFIG_ESP_RTL_SDR_SMOKE_URB_3X32K
#define SMOKE_XFER_COUNT 3u
#define SMOKE_XFER_BYTES 32768u
#define SMOKE_SOAK_NAME "usb_soak_960k_3x32k"
#else
#define SMOKE_XFER_COUNT 6u
#define SMOKE_XFER_BYTES 16384u
#define SMOKE_SOAK_NAME "usb_soak_960k_6x16k"
#endif

#ifndef CONFIG_ESP_RTL_SDR_SMOKE_SOAK_MS
#define CONFIG_ESP_RTL_SDR_SMOKE_SOAK_MS 8000
#endif

static int g_pass;
static int g_fail;
static volatile bool g_drain;

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
        return;
    }
    if (event == ESP_RTL_SDR_EVT_PASSPORT_PROGRESS && payload != NULL) {
        const esp_rtl_sdr_passport_entry_t *e = payload;
        ESP_LOGI(TAG, "passport progress rate=%u eff=%u stable=%d",
                 (unsigned)e->exact_sps, (unsigned)e->effective_sps, (int)e->stable);
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

/* Off-stack: xTaskCreate stack is 4 KiB; a 16 KiB local blew soak_drain
 * (issue #11). One soak at a time, so a single static buffer is enough. */
static uint8_t s_drain_buf[16384];

static void drain_task(void *arg)
{
    esp_rtl_sdr_handle_t sdr = (esp_rtl_sdr_handle_t)arg;
    while (g_drain) {
        size_t n = 0;
        (void)esp_rtl_sdr_read(sdr, s_drain_buf, sizeof(s_drain_buf), 50, &n);
    }
    vTaskDelete(NULL);
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
    smoke_row("version_0_7_11", ver != NULL && strcmp(ver, "0.7.11") == 0);

    const uint32_t caps = esp_rtl_sdr_get_capabilities();
    smoke_row("cap_gain", (caps & ESP_RTL_SDR_CAP_GAIN) != 0);
    smoke_row("cap_gain_auto", (caps & ESP_RTL_SDR_CAP_GAIN_AUTO) != 0);
    smoke_row("cap_rtl_agc", (caps & ESP_RTL_SDR_CAP_RTL_AGC) != 0);
    smoke_row("cap_bias_tee", (caps & ESP_RTL_SDR_CAP_BIAS_TEE) != 0);
    smoke_row("cap_sync_read", (caps & ESP_RTL_SDR_CAP_SYNC_READ) != 0);

    ESP_LOGI(TAG, "helpers version=%s caps=0x%08x urbs=%ux%u soak_ms=%d",
             ver, (unsigned)caps, SMOKE_XFER_COUNT, SMOKE_XFER_BYTES,
             CONFIG_ESP_RTL_SDR_SMOKE_SOAK_MS);
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

static void run_quiet_soak(esp_rtl_sdr_handle_t sdr)
{
    esp_rtl_sdr_metrics_t before;
    memset(&before, 0, sizeof(before));
    (void)esp_rtl_sdr_get_metrics(sdr, &before);

    g_drain = true;
    if (xTaskCreate(drain_task, "soak_drain", 4096, sdr, 5, NULL) != pdPASS) {
        g_drain = false;
        smoke_row(SMOKE_SOAK_NAME, false);
        return;
    }

    settle_ms(CONFIG_ESP_RTL_SDR_SMOKE_SOAK_MS);
    g_drain = false;
    settle_ms(80);

    esp_rtl_sdr_metrics_t after;
    esp_rtl_sdr_health_info_t health;
    memset(&after, 0, sizeof(after));
    memset(&health, 0, sizeof(health));
    (void)esp_rtl_sdr_get_metrics(sdr, &after);
    (void)esp_rtl_sdr_get_health(sdr, &health);

    const int eff_pct = (int)(health.efficiency * 100.0f + 0.5f);
    printf("SOAK %s eff=%d sps=%u bytes=%llu over=%u drops=%u short=%u urbs=%ux%u advice=%s\n",
           SMOKE_SOAK_NAME, eff_pct, (unsigned)after.effective_sps,
           (unsigned long long)after.bytes_total, (unsigned)after.overruns,
           (unsigned)after.consumer_drops, (unsigned)after.short_transfers,
           SMOKE_XFER_COUNT, SMOKE_XFER_BYTES, health.advice);
    ESP_LOGI(TAG, "soak overall=%d eff=%.3f programmed=%u delta_bytes=%llu",
             (int)health.overall, (double)health.efficiency,
             (unsigned)health.programmed_sps,
             (unsigned long long)(after.bytes_total - before.bytes_total));

    const bool ok = (after.bytes_total > before.bytes_total) &&
                    (health.efficiency >= 0.90f) &&
                    (health.overall != ESP_RTL_SDR_HEALTH_USB_STARVING);
    smoke_row(SMOKE_SOAK_NAME, ok);
}

void app_main(void)
{
    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_LOGI(TAG, "esp_rtl_sdr %s soak=%s urbs=%ux%u",
             esp_rtl_sdr_get_version_string(), SMOKE_SOAK_NAME,
             SMOKE_XFER_COUNT, SMOKE_XFER_BYTES);
    check_pure_helpers();

    esp_rtl_sdr_config_t cfg;
    esp_rtl_sdr_config_default(&cfg);
    cfg.event_cb = on_event;
    cfg.transfer_count = SMOKE_XFER_COUNT;
    cfg.transfer_bytes = SMOKE_XFER_BYTES;
    ESP_ERROR_CHECK(esp_rtl_sdr_config_validate(&cfg));
    smoke_row("default_auto_ring", cfg.pull_ring_bytes == 0 &&
                                       cfg.delivery_mode == ESP_RTL_SDR_DELIVERY_BOTH);
    smoke_row("urb_layout_applied", cfg.transfer_count == SMOKE_XFER_COUNT &&
                                        cfg.transfer_bytes == SMOKE_XFER_BYTES);

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
        stream.frequency_hz = 96100000;
        stream.sample_rate_sps = 960000;
        const esp_err_t start_err = esp_rtl_sdr_start(sdr, &stream);
        smoke_row("start", start_err == ESP_OK);
        if (start_err == ESP_OK) {
            settle_ms(400);
            smoke_read(sdr, "first_read");
            run_quiet_soak(sdr);
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
