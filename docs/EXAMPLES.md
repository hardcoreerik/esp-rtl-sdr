# Usage examples — esp_rtl_sdr

Driver-level recipes (not full demod apps). Runnable smoke:
[`../examples/p4_serial_smoke/`](../examples/p4_serial_smoke/).  
Full param docs: [`API_REFERENCE.md`](API_REFERENCE.md).

All examples assume Blog V4 on ESP32-P4 HS USB Host and:

```c
#include "esp_rtl_sdr.h"
#include "esp_log.h"
```

---

## 1. Minimal install + teardown

```c
void example_lifecycle(void)
{
    esp_rtl_sdr_config_t cfg;
    esp_rtl_sdr_config_default(&cfg);

    esp_rtl_sdr_handle_t sdr = NULL;
    ESP_ERROR_CHECK(esp_rtl_sdr_install(&cfg, &sdr));
    /* … work … */
    ESP_ERROR_CHECK(esp_rtl_sdr_stop(sdr, 0));
    ESP_ERROR_CHECK(esp_rtl_sdr_uninstall(sdr));
    sdr = NULL;
}
```

---

## 2. FM-class continuous stream (async IQ)

Broadcast FM–class defaults via intent, then stream.

```c
static void on_iq(esp_rtl_sdr_event_t ev, const void *payload, void *ctx)
{
    if (ev != ESP_RTL_SDR_EVT_IQ_BLOCK || !payload) return;
    const esp_rtl_sdr_iq_block_t *iq = payload;
    /* iq->data: CU8 I,Q interleaved — copy or process before return */
    (void)iq;
    (void)ctx;
}

void example_fm_stream(void)
{
    esp_rtl_sdr_config_t cfg;
    esp_rtl_sdr_config_default(&cfg);
    cfg.event_cb = on_iq;

    esp_rtl_sdr_handle_t sdr = NULL;
    ESP_ERROR_CHECK(esp_rtl_sdr_install(&cfg, &sdr));
    ESP_ERROR_CHECK(esp_rtl_sdr_apply_need(sdr, ESP_RTL_SDR_NEED_FM));
    /* Optional explicit LO (e.g. local station): */
    ESP_ERROR_CHECK(esp_rtl_sdr_set_center_freq(sdr, 100100000));

    esp_err_t err = esp_rtl_sdr_start_hz(sdr, 0, 0);
    if (err == ESP_RTL_SDR_ERR_NO_DEVICE) {
        ESP_LOGW("ex", "no Blog V4 — still IDLE (fail-closed)");
        (void)esp_rtl_sdr_uninstall(sdr);
        return;
    }
    ESP_ERROR_CHECK(err);

    /* run until stop */
    vTaskDelay(pdMS_TO_TICKS(5000));
    ESP_ERROR_CHECK(esp_rtl_sdr_stop(sdr, 0));
    ESP_ERROR_CHECK(esp_rtl_sdr_uninstall(sdr));
}
```

**App note:** Real FM demod (stereo, RDS) lives in your app or OrcSDR — this driver only delivers CU8 IQ.

---

## 3. ADS-B-class LO + rate

```c
void example_adsb_tune(esp_rtl_sdr_handle_t sdr)
{
    ESP_ERROR_CHECK(esp_rtl_sdr_apply_need(sdr, ESP_RTL_SDR_NEED_ADSB));
    /* 1090 MHz, 2.048 MSPS preferred — start when ready */
    ESP_ERROR_CHECK(esp_rtl_sdr_start_hz(sdr, 0, 0));
}
```

Or explicit:

```c
ESP_ERROR_CHECK(esp_rtl_sdr_start_hz(sdr, 1090000000u, ESP_RTL_SDR_RATE_2048K));
```

---

## 4. NOAA weather (WX)

```c
void example_wx(esp_rtl_sdr_handle_t sdr)
{
    ESP_ERROR_CHECK(esp_rtl_sdr_apply_need(sdr, ESP_RTL_SDR_NEED_WX));
    /* 162.400 MHz, 960k */
    ESP_ERROR_CHECK(esp_rtl_sdr_start_hz(sdr, 0, 0));
}
```

---

## 5. Sync pull only (no event callback)

```c
void example_sync_read(void)
{
    esp_rtl_sdr_config_t cfg;
    esp_rtl_sdr_config_default(&cfg); /* event_cb = NULL */

    esp_rtl_sdr_handle_t sdr = NULL;
    ESP_ERROR_CHECK(esp_rtl_sdr_install(&cfg, &sdr));
    ESP_ERROR_CHECK(esp_rtl_sdr_start_hz(sdr, 100100000, ESP_RTL_SDR_RATE_960K));

    for (int i = 0; i < 50; i++) {
        uint8_t buf[8192];
        size_t n = 0;
        esp_err_t e = esp_rtl_sdr_read(sdr, buf, sizeof(buf), 500, &n);
        if (e == ESP_OK) {
            ESP_LOGI("ex", "got %u bytes", (unsigned)n);
        } else if (e != ESP_RTL_SDR_ERR_TIMEOUT) {
            ESP_LOGE("ex", "%s", esp_rtl_sdr_err_to_name(e));
            break;
        }
    }
    (void)esp_rtl_sdr_stop(sdr, 0);
    (void)esp_rtl_sdr_uninstall(sdr);
}
```

---

## 6. Hot retune from IQ callback (0.7.3+)

```c
static volatile uint32_t g_next_hz;
static volatile bool g_want_retune;

static void on_evt(esp_rtl_sdr_event_t ev, const void *payload, void *ctx)
{
    esp_rtl_sdr_handle_t sdr = (esp_rtl_sdr_handle_t)ctx;
    if (ev == ESP_RTL_SDR_EVT_IQ_BLOCK && g_want_retune) {
        (void)esp_rtl_sdr_retune_hz(sdr, g_next_hz); /* queues; returns OK */
        g_want_retune = false;
    }
    if (ev == ESP_RTL_SDR_EVT_RETUNED && payload) {
        ESP_LOGI("ex", "LO now %u", (unsigned)*(const uint32_t *)payload);
    }
}
```

Do **not** call `start`/`stop`/`uninstall` from the callback.

---

## 7. Health monitoring loop

```c
void example_health_poll(esp_rtl_sdr_handle_t sdr)
{
    esp_rtl_sdr_health_info_t h;
    if (esp_rtl_sdr_get_health(sdr, &h) != ESP_OK) return;
    ESP_LOGI("ex", "overall=%d eff=%.2f advice=%s",
             (int)h.overall, (double)h.efficiency, h.advice);
    if (h.overall == ESP_RTL_SDR_HEALTH_USB_STARVING) {
        /* consider lower rate after stop/start */
    }
}
```

---

## 8. Rate passport then NEED_MAX_STABLE

```c
void example_passport(esp_rtl_sdr_handle_t sdr)
{
    /* must not be streaming */
    esp_rtl_sdr_passport_opts_t opts;
    esp_rtl_sdr_passport_opts_default(&opts);
    opts.dwell_ms = 1000;
    opts.recommended_only = true;

    esp_rtl_sdr_rate_passport_t pass;
    esp_err_t err = esp_rtl_sdr_probe_rates(sdr, &opts, &pass);
    if (err != ESP_OK) {
        ESP_LOGW("ex", "probe: %s", esp_rtl_sdr_err_to_name(err));
        return;
    }
    ESP_LOGI("ex", "best_stable=%u", (unsigned)pass.best_stable_sps);
    ESP_ERROR_CHECK(esp_rtl_sdr_apply_need(sdr, ESP_RTL_SDR_NEED_MAX_STABLE));
    ESP_ERROR_CHECK(esp_rtl_sdr_start_hz(sdr, 0, 0));
}
```

---

## 9. Continuous custom rate + ppm

```c
void example_custom_rate_ppm(esp_rtl_sdr_handle_t sdr)
{
    ESP_ERROR_CHECK(esp_rtl_sdr_set_freq_correction(sdr, -12)); /* ppm */
    ESP_ERROR_CHECK(esp_rtl_sdr_set_sample_rate(sdr, 1536000)); /* quantizes */
    ESP_ERROR_CHECK(esp_rtl_sdr_start_hz(sdr, 433920000, 0));
}
```

---

## 10. Capability-safe gain attempt (0.7.x = skip)

```c
void example_gain_if_ready(esp_rtl_sdr_handle_t sdr)
{
    if (!(esp_rtl_sdr_get_capabilities() & ESP_RTL_SDR_CAP_GAIN)) {
        ESP_LOGW("ex", "CAP_GAIN off — Phase 3 capture still open");
        return;
    }
    ESP_ERROR_CHECK(esp_rtl_sdr_set_tuner_gain_mode(sdr, ESP_RTL_SDR_GAIN_MODE_MANUAL));
    ESP_ERROR_CHECK(esp_rtl_sdr_set_tuner_gain(sdr, 400)); /* 40.0 dB tenths */
}
```

---

## Where full apps live

| Goal | Place |
|---|---|
| Driver contract smoke | `examples/p4_serial_smoke` |
| Product UI / demod / map | **Your app** or [OrcSDR](https://github.com/hardcoreerik/OrcSDR) |
| Lab stimulus | [`LAB_HOBBYIST.md`](LAB_HOBBYIST.md) |
