# Architecture — esp_rtl_sdr

**Current source architecture (reviewed 2026-11-04):** one shared RTL2832U high-speed USB/IQ transport supports model-aware tuning across five implemented device profiles. The driver provides radio hardware capabilities; consumer applications such as OrcSDR implement their own audio and digital protocol decoders. [Profile matrix](docs/PROFILES.md) · [current truth](PROJECT_TRUTH.md) · [OrcSDR native decoder inventory](https://github.com/hardcoreerik/OrcSDR/blob/main/docs/NATIVE_DECODER_INVENTORY.md).

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
        ├─────────────────────┬──────────────────────┐
        ▼                     ▼                      ▼
  ┌────────────────┐  ┌────────────────┐  ┌────────────────┐
  │ blog_v4        │  │ blog_v4l       │  │ blog_v3        │
  │ R828D          │  │ R828S          │  │ V3c/R820 family│
  └────────────────┘  └────────────────┘  └────────────────┘
        ├─────────────────────┬──────────────────────┐
        ▼                     ▼
  ┌────────────────┐  ┌────────────────────┐
  │nooelec_smart_v5│  │e4000_experimental  │
  │R820 / direct-Q │  │historic ID; E4000  │
  └────────────────┘  └────────────────────┘
```

**Current source implements all five named profiles:** `blog_v4`, `blog_v4l`, `blog_v3` (V3c hardware-qualified), `nooelec_smart_v5` and `e4000_experimental`. The last profile name is historical, not a claim of missing implementation. The maintainer confirms E4000/XTR works as an OrcSDR receiver **within its supported RF tuning range**; OrcSDR intentionally refuses out-of-range tuning even if the target dashboard opens. That is application-level capability gating, not a driver failure. Complete band-edge characterization, calibration and cross-device soak testing remain independent evidence goals.

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

- Sample rates are supported within documented continuous hardware windows (225001–300000 and 900001–3.2 million SPS), quantized to actual RTL2832 rates; recommended `ESP_RTL_SDR_RATE_*` constants are convenient presets, **not** the only supported rates.
- Frequency for `CUSTOM_HZ` is quantized to the driver's global frequency quantum. Start/retune operations then validate against the selected receiver's RF range, and PLL planning may reject further requests; out-of-range input must not silently appear to tune.
- New rate/tuner windows require measured profile support and host/device acceptance, not guesses from a different dongle.

---

## Concurrent receivers (multiple handles)

One handle still owns one stream. Multiple `install()` calls share a refcounted
USB host session and claim distinct USB addresses. See
[`docs/MULTI_DEVICE_ARCHITECTURE.md`](docs/MULTI_DEVICE_ARCHITECTURE.md).

Single-dongle apps do not need to change (`BIND_ANY` + one handle).

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

Unknown devices **fail closed** with the current unsupported-device error contract. Do not infer compatibility from generic RTL2832U VID/PID; see [API reference](docs/API_REFERENCE.md) for exact error values.

### “Any RTL2832U”

Architecture supports controlled growth. Current claimed profiles are **Blog V4, V4L, V3c, Nooelec NESDR SMArt V5 and E4000/XTR**, with model-specific capabilities and explicitly scoped RF acceptance. Blog V4 was the first profile, **not the only populated profile**.

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
