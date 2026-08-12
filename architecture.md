# Architecture — esp_rtl_sdr

## Goals

1. Provide a **production-minded** ESP-IDF USB Host client for RTL2832U-class SDR dongles.
2. Deliver continuous **CU8 IQ** with a fail-closed lifecycle suitable for FreeRTOS.
3. Stay **clean-room**: no librtlsdr / rtl-sdr-blog source.
4. Grow **profile-by-profile** toward multi-tuner support without collapsing into a monolithic port.
5. Remain **board-agnostic**: host BSP (VBUS, display, network) lives in apps.
6. Evolve a **dongle nervous system** (intent, health, on-host passport) that uses
   the MCU host’s vantage point — see `docs/VISION.md`.

## Non-goals

- Drop-in ABI compatibility with `rtl-sdr.h` / librtlsdr.
- Claiming every eBay “RTL2832U” works without a measured profile.
- Demodulation, ADS-B decode, WebSDR UI (application layer).
- Full-Speed ESP32-S2/S3 until measured.

---

## Layering

```text
┌──────────────────────────────────────────────────────────┐
│  Application (OrcSDR Tab5, Waveshare shell, your app)    │
│  demod · UI · rtl_tcp · storage · gain policy UI         │
└────────────────────────────┬─────────────────────────────┘
                             │  esp_rtl_sdr.h
┌────────────────────────────▼─────────────────────────────┐
│  Public API                                              │
│  validate · install · start · retune · stop · metrics    │
│  need · health · passport · continuous rates · ppm       │
│  events (IQ_BLOCK, HEALTH, PASSPORT_*, …)                │
└────────────────────────────┬─────────────────────────────┘
                             │
┌────────────────────────────▼─────────────────────────────┐
│  Core host client                                        │
│  USB host install/client · IF0 claim · bulk EP 0x81      │
│  multi-URB pool · dual-core delivery · state machine     │
│  rate quantize · passport soak · reentrancy · fail-closed│
└────────────────────────────┬─────────────────────────────┘
                             │
        ┌────────────────────┼────────────────────┐
        ▼                    ▼                    ▼
 ┌─────────────┐      ┌─────────────┐      ┌─────────────┐
 │ Profile     │      │ Profile     │      │ Profile     │
 │ blog_v4     │      │ r820t2      │      │ future…     │
 │ (R828D)     │      │ (planned)   │      │             │
 │ init/rate/  │      │             │      │             │
 │ tune tables │      │             │      │             │
 └─────────────┘      └─────────────┘      └─────────────┘
```

**Today:** only the **blog_v4** profile is populated (`private/transfers_blog_v4.hpp`).

---

## Lifecycle state machine

```text
UNINSTALLED
     │ install()
     v
   IDLE  <──────────────────────────────┐
     │ start()                          │
     v                                  │
 STREAMING ── retune_hz() (queued) ─────┤
     │                                  │
     │ stop()                           │
     └──────────────────────────────────┘
     │
     +── disconnect / fatal ──> FAULT ── reset/stop/uninstall ──> IDLE / gone
```

### Rules

| Rule | Behavior |
|---|---|
| Fail closed | Failed `start` never leaves bulk half-open |
| Idempotent stop | `stop` when IDLE → OK |
| NULL uninstall | `uninstall(NULL)` → OK |
| Reentrancy | Lifecycle APIs from event callback → `ERR_REENTRANT` |
| IQ borrow | `EVT_IQ_BLOCK` pointer valid until callback returns (default) |

Full contract: [`docs/API.md`](docs/API.md).

---

## Threading model

| Role | Typical core | Responsibility |
|---|---|---|
| USB host lib + client | Core 0 | EP0, URB complete, host events |
| IQ delivery | Core 1 | Post `EVT_IQ_BLOCK` to app callback |
| Application | Core 1 (or free) | Demod, UI, network — **must return quickly from callback** |

Public API is serialized per handle (mutex). Queries use short lock timeouts where specified.

---

## Data path

```text
RTL2832U bulk IN 0x81 (HS, 512 B MPS)
        │
        ▼
 multi-URB transfers (default 6 × 16 KiB; app-configurable)
        │
        ▼
 ring / delivery task
        │
        ▼
 EVT_IQ_BLOCK { data, bytes, sequence, frequency_hz, sample_rate_sps, ts }
        │
        ▼
 application consumer
```

**Sample format:** interleaved **unsigned** I/Q bytes (I0, Q0, I1, Q1, …), same default family as desktop rtl-sdr CU8.

---

## Sample rate and frequency policy

- Rates are **allowlisted** (`ESP_RTL_SDR_RATE_*`). Unsupported rates fail validation.
- Frequency for `CUSTOM_HZ` is clamped/quantized (`normalize_frequency`).
- Expanding rates requires measurement + table updates in the active profile — not free-form PLL guessing in public API.

---

## Multi-dongle / multi-tuner strategy

### Identity

1. USB VID/PID allowlist (common RTL2832U OEM IDs over time).
2. Optional manufacturer/product string match per profile.
3. Tuner probe only with **observed** STALL/success semantics (Blog V4: six expected absent-address STALLs then R828D at measured address).

### Profiles

Each profile supplies:

| Artifact | Content |
|---|---|
| Accept filter | Who we bind |
| Init sequence | Ordered EP0 vendor ops |
| Cleanup sequence | Ordered teardown |
| Rate programs | Allowlisted SPS → control sequence |
| Tune / PLL | Frequency packing for that tuner |
| Optional | Gain tables, bias-T, direct sampling — only when evidenced |

Unknown devices: **fail closed** with `ERR_NOT_V4` / `ERR_UNSUPPORTED` (error names may broaden to `ERR_NOT_SUPPORTED_DEVICE` in a later API revision).

### “Any RTL2832U”

Architecture supports growth. **Marketing and Project_truth** only claim profiles that are measured. First profile remains **RTL-SDR Blog V4 (R828D)**.

---

## Clean-room boundary

```text
ALLOWED                         FORBIDDEN
───────                         ────────
Public USB / ESP-IDF docs       Reading librtlsdr / rtl-sdr-blog source
Independent USB captures        Pasting register dumps from those trees
Public chip datasheets          “Translate librtlsdr to ESP-IDF”
Measured ESP32-P4 results       Silent copy of gain/I2C tables
```

See [`docs/CLEAN_ROOM.md`](docs/CLEAN_ROOM.md).

---

## Board BSP vs driver

| Lives in driver | Lives in app / board BSP |
|---|---|
| USB host client for SDR | VBUS enable (Tab5 PMIC, etc.) |
| EP0/bulk SDR protocol | Display, touch, audio codec |
| IQ delivery | Demod, ADS-B, web UI |
| Metrics | Ethernet / Wi-Fi policy |

Waveshare second-board work proved: **same driver tables, different BSP**, no driver fork.

---

## Relationship to desktop librtlsdr

| Desktop | esp_rtl_sdr |
|---|---|
| Wide C API, many tuners via one ported stack | Narrow measured API + profiles |
| libusb on PC | ESP USB Host + FreeRTOS |
| Free sample rates / gains | Allowlist + evidence gates |
| de-facto standard for GQRX/rtl_tcp | ESP-native contract; apps adapt |

We **close functional gaps** over roadmap phases without becoming a GPL port.

---

## Versioning and compatibility

- Semantic version in header macros `ESP_RTL_SDR_VERSION_*`.
- Config structs carry `struct_size` for ABI growth (append-only fields).
- Capability bits (`ESP_RTL_SDR_CAP_*`) gate optional features; apps must check.

---

## Example consumer sketch

```c
#include "esp_rtl_sdr.h"

static void on_event(esp_rtl_sdr_event_t ev, const void *payload, void *ctx) {
    if (ev == ESP_RTL_SDR_EVT_IQ_BLOCK) {
        const esp_rtl_sdr_iq_block_t *iq = payload;
        /* process iq->data[0..iq->bytes) quickly */
        (void)esp_rtl_sdr_release_iq_block((esp_rtl_sdr_handle_t)ctx, iq);
    }
}

void app_main(void) {
    esp_rtl_sdr_config_t cfg;
    esp_rtl_sdr_config_default(&cfg);
    cfg.event_cb = on_event;

    esp_rtl_sdr_handle_t sdr = NULL;
    ESP_ERROR_CHECK(esp_rtl_sdr_install(&cfg, &sdr));
    cfg.event_ctx = sdr; /* if desired */

    esp_rtl_sdr_stream_config_t st;
    esp_rtl_sdr_stream_config_default(&st);
    st.preset = ESP_RTL_SDR_PRESET_CUSTOM_HZ;
    st.frequency_hz = 1090000000;
    st.sample_rate_sps = ESP_RTL_SDR_RATE_2048K;
    esp_err_t err = esp_rtl_sdr_start(sdr, &st);
    /* ERR_NO_DEVICE is OK if dongle not attached yet */
}
```

Full example: `examples/p4_serial_smoke/`.
