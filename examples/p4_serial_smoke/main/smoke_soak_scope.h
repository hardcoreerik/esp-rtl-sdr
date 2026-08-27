/*
 * Pure soak-window scoring for p4_serial_smoke.
 * Driver get_health()/get_metrics() are cumulative from stream start; the SOAK
 * row must use before/after deltas for the drain window only.
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

/* CU8: samples = bytes/2; sps = bytes * 500 / window_ms (same as the driver). */
static inline uint32_t smoke_soak_scoped_sps(uint64_t delta_bytes, uint32_t window_ms)
{
    if (window_ms == 0) {
        return 0;
    }
    return (uint32_t)((delta_bytes * 500ull) / (uint64_t)window_ms);
}

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

    if (before == NULL || after == NULL || window_ms == 0 || programmed_sps == 0) {
        *out = z;
        return;
    }

    z.delta_bytes = after->bytes_total - before->bytes_total;
    z.delta_overruns = after->overruns - before->overruns;
    z.delta_consumer_drops = after->consumer_drops - before->consumer_drops;
    z.delta_short_transfers = after->short_transfers - before->short_transfers;
    z.scoped_sps = smoke_soak_scoped_sps(z.delta_bytes, window_ms);
    z.eff_pct = (int)(((uint64_t)z.scoped_sps * 100ull) / (uint64_t)programmed_sps);
    z.efficiency = (float)z.scoped_sps / (float)programmed_sps;
    const bool eff_ok = z.eff_pct >= 90;

    if (!eff_ok) {
        z.advice = "USB_STARVING";
    } else if (z.delta_consumer_drops > 0) {
        z.advice = "APP_TOO_SLOW";
    } else {
        z.advice = "ok";
    }

    const bool continuing_iq = z.delta_bytes > 0;
    const bool advice_ok =
        (strcmp(z.advice, "USB_STARVING") != 0) && (strcmp(z.advice, "APP_TOO_SLOW") != 0);
    z.pass = continuing_iq && eff_ok && (z.delta_overruns == 0) &&
             (z.delta_consumer_drops == 0) && advice_ok;
    *out = z;
}

#ifdef __cplusplus
}
#endif
