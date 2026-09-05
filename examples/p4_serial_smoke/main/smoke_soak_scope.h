/*
 * Thin soak-window adapter for p4_serial_smoke.
 * Scoring lives in public esp_rtl_sdr_metrics_delta / esp_rtl_sdr_health_from_window
 * so apps do not reinvent before/after delta math. get_health()/get_metrics() remain
 * cumulative from stream start; SOAK uses the window helpers only.
 */
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "esp_rtl_sdr.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint64_t delta_bytes;
    uint32_t delta_overruns;
    uint32_t delta_consumer_drops;
    uint32_t delta_short_transfers;
    uint32_t scoped_sps;
    int eff_pct;
    float efficiency;
    const char *advice;
    bool pass;
} smoke_soak_scope_t;

static inline void smoke_soak_scope_from_metrics(const esp_rtl_sdr_metrics_t *before,
                                                 const esp_rtl_sdr_metrics_t *after,
                                                 uint32_t window_ms,
                                                 uint32_t programmed_sps,
                                                 smoke_soak_scope_t *out)
{
    smoke_soak_scope_t z;
    if (out == NULL) {
        return;
    }
    memset(&z, 0, sizeof(z));
    z.advice = "invalid";
    z.pass = false;

    esp_rtl_sdr_metrics_window_t win;
    esp_rtl_sdr_health_info_t health;
    memset(&win, 0, sizeof(win));
    memset(&health, 0, sizeof(health));

    if (esp_rtl_sdr_metrics_delta(before, after, window_ms, programmed_sps, &win) !=
            ESP_OK ||
        esp_rtl_sdr_health_from_window(before, after, window_ms, programmed_sps,
                                       &health) != ESP_OK) {
        *out = z;
        return;
    }

    z.delta_bytes = win.delta_bytes;
    z.delta_overruns = win.delta_overruns;
    z.delta_consumer_drops = win.delta_consumer_drops;
    z.delta_short_transfers = win.delta_short_transfers;
    z.scoped_sps = win.effective_sps;
    z.efficiency = win.efficiency;
    z.eff_pct = (int)(((uint64_t)win.effective_sps * 100ull) / (uint64_t)programmed_sps);

    if (health.overall == ESP_RTL_SDR_HEALTH_USB_STARVING) {
        z.advice = "USB_STARVING";
    } else if (health.overall == ESP_RTL_SDR_HEALTH_APP_TOO_SLOW) {
        z.advice = "APP_TOO_SLOW";
    } else if (health.overall == ESP_RTL_SDR_HEALTH_OK) {
        z.advice = "ok";
    } else {
        z.advice = "invalid";
    }

    const bool continuing_iq = z.delta_bytes > 0;
    const bool eff_ok = z.eff_pct >= 90;
    const bool advice_ok = (health.overall == ESP_RTL_SDR_HEALTH_OK);
    z.pass = continuing_iq && eff_ok && (z.delta_overruns == 0) &&
             (z.delta_consumer_drops == 0) && advice_ok;
    *out = z;
}

#ifdef __cplusplus
}
#endif
