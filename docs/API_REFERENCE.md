# esp_rtl_sdr — API Reference

> **Version tracked:** `0.7.4` (see `ESP_RTL_SDR_VERSION_*` in [`include/esp_rtl_sdr.h`](../include/esp_rtl_sdr.h))  
> **Header of record:** [`include/esp_rtl_sdr.h`](../include/esp_rtl_sdr.h)  
> **Design contract (invariants, ABI growth):** [`API.md`](API.md)  
> **What works on hardware right now:** [`../PROJECT_TRUTH.md`](../PROJECT_TRUTH.md) wins on any claim conflict.

This is the **detailed human reference** for every public type and function: parameters, return semantics, threading rules, and copy-paste examples. It is written for someone shipping a FreeRTOS product on **ESP32-P4**, not for skimming a header alone.

---

## How to read this document

Best-in-class API docs (Stripe, ESP-IDF, libusb) share one pattern:

| Section | Purpose |
|---|---|
| **Concepts** | Mental model before signatures |
| **Types** | Structs / enums with field tables |
| **Functions** | Signature → params → returns → notes → example |
| **Recipes** | End-to-end goals (“stream FM”, “retune from IQ callback”) |

**Conventions used below**

| Marker | Meaning |
|---|---|
| **Required** | Must be non-NULL / valid or the call fails |
| **Optional** | NULL or zero has defined behavior |
| **Out** | Written only on success (unless noted) |
| **Borrowed** | Pointer valid only until callback returns |
| **Idempotent** | Safe to call again with same outcome |
| **CAP-gated** | Check `get_capabilities()` before assuming the path works |

---

## Table of contents

1. [Ten-minute path](#1-ten-minute-path)
2. [Conventions](#2-conventions)
3. [Concepts](#3-concepts)
4. [Errors](#4-errors)
5. [Capabilities](#5-capabilities)
6. [Constants & macros](#6-constants--macros)
7. [Types](#7-types)
8. [Version & diagnostics](#8-version--diagnostics)
9. [Config helpers](#9-config-helpers)
10. [Lifecycle](#10-lifecycle)
11. [Queries](#11-queries)
12. [Streaming](#12-streaming)
13. [Center frequency, rate, sync read](#13-center-frequency-rate-sync-read)
14. [PPM & multi-device](#14-ppm--multi-device)
15. [Intent, health, passport](#15-intent-health-passport)
16. [Gain & bias (stubs)](#16-gain--bias-stubs)
17. [Events catalog](#17-events-catalog)
18. [Recipes](#18-recipes)
19. [Symbol index](#19-symbol-index)

---

## 1. Ten-minute path

```c
#include "esp_rtl_sdr.h"

static void on_sdr(esp_rtl_sdr_event_t ev, const void *payload, void *ctx)
{
    if (ev == ESP_RTL_SDR_EVT_IQ_BLOCK && payload) {
        const esp_rtl_sdr_iq_block_t *iq = payload;
        /* CU8 interleaved I,Q — valid only until this function returns */
        (void)iq->data;
        (void)iq->bytes;
    }
    (void)ctx;
}

void app_radio_start(void)
{
    esp_rtl_sdr_config_t cfg;
    esp_rtl_sdr_config_default(&cfg);          /* always first */
    cfg.event_cb = on_sdr;
    cfg.event_ctx = NULL;

    esp_rtl_sdr_handle_t sdr = NULL;
    ESP_ERROR_CHECK(esp_rtl_sdr_install(&cfg, &sdr));

    ESP_ERROR_CHECK(esp_rtl_sdr_apply_need(sdr, ESP_RTL_SDR_NEED_FM));
    ESP_ERROR_CHECK(esp_rtl_sdr_start_hz(sdr, 0, 0)); /* preferred LO + rate */

    /* … later … */
    ESP_ERROR_CHECK(esp_rtl_sdr_stop(sdr, 0));
    ESP_ERROR_CHECK(esp_rtl_sdr_uninstall(sdr));
}
```

If `start` / `start_hz` returns `ESP_RTL_SDR_ERR_NO_DEVICE`, the handle is still **IDLE** — fail-closed, not half-open USB.

Hardware walk-through: [`../examples/p4_serial_smoke/`](../examples/p4_serial_smoke/).

---

## 2. Conventions

### Include

```c
#include "esp_rtl_sdr.h"
```

Requires ESP-IDF (`esp_err.h`) and a C11-capable toolchain. C++ callers get `extern "C"` linkage from the header.

### Handle

```c
typedef struct esp_rtl_sdr_handle *esp_rtl_sdr_handle_t;
```

Opaque. One handle = one logical Blog V4 session (interface 0). Never free it yourself — always `esp_rtl_sdr_uninstall()`.

### `esp_err_t` discipline

| Pattern | Rule |
|---|---|
| Success | `ESP_OK` |
| Component errors | `ESP_RTL_SDR_ERR_*` (`0x12A00` base) |
| Invalid pointers / ranges | Often `ESP_ERR_INVALID_ARG` |
| Logging | `esp_rtl_sdr_err_to_name(err)` — never NULL |
| Sticky error | `esp_rtl_sdr_get_last_error(handle)` |

### `struct_size` (ABI growth)

Config / stream / health / passport structs start with `size_t struct_size`.

1. Call `*_default()` so `struct_size` is set.  
2. Or set `cfg.struct_size = sizeof(esp_rtl_sdr_config_t)` yourself.  
3. New fields are **only appended**; older apps with smaller `struct_size` get trailing fields defaulted.

### Discover before assume

```c
uint32_t caps = esp_rtl_sdr_get_capabilities();
if (!(caps & ESP_RTL_SDR_CAP_GAIN)) {
    /* set_tuner_gain will return ERR_UNSUPPORTED — do not expect RF effect */
}
```

---

## 3. Concepts

### Lifecycle state machine

```text
                 install()
  UNINSTALLED ──────────────► IDLE
                               │
                    start() / start_hz()
                               │
                               ▼
                           STARTING ── fail ──► IDLE or FAULT
                               │ ok
                               ▼
                           STREAMING ◄── retune_hz / set_center_freq
                               │                 (async if from callback)
                            stop()
                               │
                               ▼
                             IDLE
                               │
                          uninstall()
                               │
                               ▼
                           destroyed
```

| State | Meaning |
|---|---|
| `UNINSTALLED` | No handle / after destroy |
| `IDLE` | Handle alive; not streaming |
| `STARTING` | `start` in progress |
| `STREAMING` | Bulk IQ active |
| `STOPPING` | Teardown in progress |
| `FAULT` | Unrecoverable without `reset` / `stop` / `uninstall` |

**Fail-closed:** a failed `start` never leaves the interface claimed without a matching stop path. Handle stays `IDLE` or moves to `FAULT`.

### Threading model (read this twice)

| Context | Allowed | Forbidden |
|---|---|---|
| App FreeRTOS task | Full public API (serialized per handle) | Concurrent `uninstall` with other calls on same handle |
| Event callback | `get_state`, `get_metrics`, `get_device_info`, `get_last_error`, `release_iq_block`, **`retune_hz` / `set_center_freq` (queued async)** | `install`, `uninstall`, `start`, `start_hz`, `stop`, `reset`, `probe_rates` → `ERR_REENTRANT` |
| USB completion ISR | Nothing | All public APIs |

- Public entry points take a **per-handle mutex**.  
- Event callbacks run **outside** that mutex so you can call safe query APIs.  
- IQ pointers are **borrowed** until the callback returns (default).  
- Callbacks must return quickly — no display paint, flash erase, or long network I/O.

### IQ sample format

| Property | Value |
|---|---|
| Type | **CU8** — `uint8_t` |
| Layout | Interleaved **I, Q, I, Q, …** |
| Pair | 2 bytes = 1 complex sample |
| Bias | Unsigned (RTL-SDR style); ~127 is midscale |
| Delivery | `EVT_IQ_BLOCK` and/or `esp_rtl_sdr_read()` |

```c
/* i = data[2*n], q = data[2*n+1] */
for (size_t n = 0; n < iq->bytes / 2; n++) {
    int16_t i = (int16_t)iq->data[2 * n] - 127;
    int16_t q = (int16_t)iq->data[2 * n + 1] - 127;
    (void)i; (void)q;
}
```

### Rate policy (summary)

| Window | Range |
|---|---|
| Low band | **225001 … 300000** Hz |
| Gap (rejected) | 300001 … 899999 Hz |
| High band | **900000 … 3200000** Hz |

Any in-window rate is quantized to an exact RTL2832 ratio (`quantize_sample_rate`). Named macros (`RATE_960K`, `RATE_2048K`, …) are conveniences — not the only legal values. Details: [`RATES.md`](RATES.md).

### Frequency policy

| Item | Value |
|---|---|
| Range | 24 MHz … 1766 MHz (`FREQ_MIN_HZ` … `FREQ_MAX_HZ`) |
| Quantization | 1 kHz (`FREQ_QUANT_HZ`) |
| PPM | Software LO offset, ±200 |

### Ownership of USB Host stack

| `config.host_library_already_installed` | Who calls `usb_host_install` / uninstall |
|---|---|
| `false` (default) | Driver owns stack for this handle |
| `true` | App already installed host library; must keep it alive for handle lifetime |

---

## 4. Errors

### Component codes

| Macro | Typical cause | App action |
|---|---|---|
| `ESP_RTL_SDR_ERR_NO_DEVICE` | No Blog V4 attached | Wait / hotplug / refresh list |
| `ESP_RTL_SDR_ERR_NOT_V4` / `UNSUPPORTED_DEVICE` | Device present, identity not accepted | Wrong stick; fail closed |
| `ESP_RTL_SDR_ERR_BAD_DEVICE` | Index / serial not in candidate list | Refresh, reselect |
| `ESP_RTL_SDR_ERR_BUSY` | Already streaming, stop in progress, rate change while stream, etc. | Stop first or wait |
| `ESP_RTL_SDR_ERR_NOT_STREAMING` | `retune` / `read` without stream | Start first |
| `ESP_RTL_SDR_ERR_BAD_RATE` | Outside windows / quantize fail | Use `is_rate_supported` |
| `ESP_RTL_SDR_ERR_BAD_FREQ` | Out of range / zero / normalize fail | Clamp / check antenna band |
| `ESP_RTL_SDR_ERR_USB` | Transfer / control path failure | `get_last_error`, reconnect |
| `ESP_RTL_SDR_ERR_TIMEOUT` | Control or stop / read timed out | Retry or increase timeout |
| `ESP_RTL_SDR_ERR_FAULT` | Handle in FAULT | `reset` or `uninstall` |
| `ESP_RTL_SDR_ERR_NOT_READY` | Client not ready yet | Brief wait / retry |
| `ESP_RTL_SDR_ERR_UNSUPPORTED` | CAP off (gain/bias) or path not built | Check capabilities |
| `ESP_RTL_SDR_ERR_STALE_HANDLE` | Use after uninstall | Clear pointer |
| `ESP_RTL_SDR_ERR_REENTRANT` | Forbidden API from event callback | Defer to app task |
| `ESP_RTL_SDR_ERR_NOT_CLAIMED` | Present but not claimed | Start / claim path |

### `esp_rtl_sdr_err_to_name`

```c
const char *esp_rtl_sdr_err_to_name(esp_err_t err);
```

| | |
|---|---|
| **Parameters** | `err` — any `esp_err_t` including component codes |
| **Returns** | Stable C string; **never NULL** (unknown → generic text) |
| **Thread-safe** | Yes (static tables) |

```c
ESP_LOGE(TAG, "start failed: %s", esp_rtl_sdr_err_to_name(err));
```

---

## 5. Capabilities

### `esp_rtl_sdr_get_capabilities`

```c
uint32_t esp_rtl_sdr_get_capabilities(void);
```

| | |
|---|---|
| **Parameters** | none |
| **Returns** | Bitmask of `esp_rtl_sdr_cap_t` for **this binary** |
| **Notes** | No handle required. CAP bits track **implemented** paths only. |

### Capability map (0.7.4)

| Flag | Bit | Status | Meaning |
|---|---|---|---|
| `ESP_RTL_SDR_CAP_STREAM` | 0 | **On** | `start` / `stop` bulk IQ |
| `ESP_RTL_SDR_CAP_RETUNE` | 1 | **On** | In-stream `retune_hz` |
| `ESP_RTL_SDR_CAP_HOTPLUG` | 2 | **On** | Disconnect / reconnect events |
| `ESP_RTL_SDR_CAP_METRICS` | 3 | **On** | `get_metrics` |
| `ESP_RTL_SDR_CAP_CUSTOM_HZ` | 4 | **On** | `PRESET_CUSTOM_HZ` |
| `ESP_RTL_SDR_CAP_BIAS_TEE` | 5 | **Off** | Until measured |
| `ESP_RTL_SDR_CAP_DIRECT_SAMPLING` | 6 | **Off** | Not claimed |
| `ESP_RTL_SDR_CAP_IQ_ACQUIRE` | 7 | **Off** | Borrow mode only |
| `ESP_RTL_SDR_CAP_FREQ_CORRECTION` | 8 | **On** | Software ppm |
| `ESP_RTL_SDR_CAP_MULTI_DEVICE` | 9 | **On** | Enumerate / select |
| `ESP_RTL_SDR_CAP_SYNC_READ` | 10 | **On** | Blocking `read` |
| `ESP_RTL_SDR_CAP_CONTINUOUS_RATE` | 11 | **On** | In-window + quantize |
| `ESP_RTL_SDR_CAP_NEED` | 12 | **On** | `apply_need` |
| `ESP_RTL_SDR_CAP_HEALTH` | 13 | **On** | `get_health` / `EVT_HEALTH` |
| `ESP_RTL_SDR_CAP_PASSPORT` | 14 | **On** | `probe_rates` |
| `ESP_RTL_SDR_CAP_GAIN` | 15 | **Off** | Until Phase 3 capture |
| `ESP_RTL_SDR_CAP_DELIVERY_MODE` | 16 | **On** | `config.delivery_mode` |

```c
const uint32_t need = ESP_RTL_SDR_CAP_STREAM | ESP_RTL_SDR_CAP_SYNC_READ;
if ((esp_rtl_sdr_get_capabilities() & need) != need) {
    /* wrong binary / unexpected build */
}
```

---

## 6. Constants & macros

### USB identity (Blog V4)

| Macro | Value |
|---|---|
| `ESP_RTL_SDR_USB_VID` | `0x0BDA` |
| `ESP_RTL_SDR_USB_PID` | `0x2838` |

### Named sample rates (Hz)

| Macro | Hz | Notes |
|---|---|---|
| `ESP_RTL_SDR_RATE_250K` | 250000 | Low band |
| `ESP_RTL_SDR_RATE_256K` | 256000 | Low band |
| `ESP_RTL_SDR_RATE_960K` | 960000 | FM-class continuous (**provenance**) |
| `ESP_RTL_SDR_RATE_1024K` | 1024000 | |
| `ESP_RTL_SDR_RATE_1800K` | 1800000 | |
| `ESP_RTL_SDR_RATE_2048K` | 2048000 | ADS-B-class (**provenance**) |
| `ESP_RTL_SDR_RATE_2400K` | 2400000 | PC clean-room capture rate |
| `ESP_RTL_SDR_RATE_2560K` | 2560000 | Vendor “stable” ceiling |
| `ESP_RTL_SDR_RATE_3200K` | 3200000 | Max; drops expected |

### Windows & policy

| Macro | Value |
|---|---|
| `ESP_RTL_SDR_RATE_LOW_MIN_HZ` | 225001 |
| `ESP_RTL_SDR_RATE_LOW_MAX_HZ` | 300000 |
| `ESP_RTL_SDR_RATE_HIGH_MIN_HZ` | 900000 |
| `ESP_RTL_SDR_RATE_HIGH_MAX_HZ` | 3200000 |
| `ESP_RTL_SDR_RATE_STABLE_MAX_HZ` | 2560000 |
| `ESP_RTL_SDR_XTAL_HZ` | 28800000 |
| `ESP_RTL_SDR_PPM_MIN` / `MAX` | −200 / +200 |
| `ESP_RTL_SDR_FREQ_MIN_HZ` / `MAX` | 24e6 / 1766e6 |
| `ESP_RTL_SDR_FREQ_QUANT_HZ` | 1000 |
| `ESP_RTL_SDR_PRESET_KZEL_HZ` | 96100000 |
| `ESP_RTL_SDR_PRESET_NOAA_HZ` | 162400000 |

### Bulk defaults

| Macro | Value |
|---|---|
| `ESP_RTL_SDR_DEFAULT_XFER_BYTES` | 16384 (multiple of 512) |
| `ESP_RTL_SDR_DEFAULT_XFER_COUNT` | 6 |
| `ESP_RTL_SDR_MIN/MAX_XFER_BYTES` | 512 … 262144 |
| `ESP_RTL_SDR_MIN/MAX_XFER_COUNT` | 2 … 8 |
| `ESP_RTL_SDR_BULK_EP_IN` | `0x81` |
| `ESP_RTL_SDR_DEFAULT_STOP_TIMEOUT_MS` | 3000 |
| `ESP_RTL_SDR_MAX_TIMEOUT_MS` | 30000 |
| `ESP_RTL_SDR_MAX_DEVICES` | 8 |
| `ESP_RTL_SDR_PASSPORT_MAX_ENTRIES` | 12 |
| `ESP_RTL_SDR_PASSPORT_DEFAULT_DWELL_MS` | 1500 |

---

## 7. Types

### `esp_rtl_sdr_state_t`

| Value | Name |
|---|---|
| 0 | `ESP_RTL_SDR_STATE_UNINSTALLED` |
| 1 | `ESP_RTL_SDR_STATE_IDLE` |
| 2 | `ESP_RTL_SDR_STATE_STREAMING` |
| 3 | `ESP_RTL_SDR_STATE_STOPPING` |
| 4 | `ESP_RTL_SDR_STATE_FAULT` |
| 5 | `ESP_RTL_SDR_STATE_STARTING` |

```c
const char *esp_rtl_sdr_state_to_name(esp_rtl_sdr_state_t state);
```

Never NULL. Safe for logging unknown values.

### `esp_rtl_sdr_preset_t`

| Value | Behavior |
|---|---|
| `PRESET_KZEL_96_1` | Fixed LO 96.1 MHz (measured reference) |
| `PRESET_NOAA_162_4` | Fixed LO 162.400 MHz |
| `PRESET_CUSTOM_HZ` | Use `stream.frequency_hz` (required) |

Named presets **ignore** `frequency_hz` in the stream config.

### `esp_rtl_sdr_need_t` (intent)

| Need | Preferred behavior (does **not** start stream) |
|---|---|
| `NEED_FM` | FM-class: ~960k @ preferred LO |
| `NEED_ADSB` | 1090 MHz, 2.048 MSPS |
| `NEED_WX` | NOAA WX 162.400 MHz, 960k |
| `NEED_HF` | Stores HF LO intent; full upconverter CAP still open |
| `NEED_MAX_STABLE` | Passport `best_stable_sps` if valid, else 2.048M |
| `NEED_LISTEN` | Lowest-drop default: 960k, keep LO |

### `esp_rtl_sdr_health_t`

| Value | Meaning |
|---|---|
| `HEALTH_UNKNOWN` | Not enough data |
| `HEALTH_OK` | Within policy |
| `HEALTH_USB_STARVING` | Host/USB cannot sustain programmed SPS |
| `HEALTH_APP_TOO_SLOW` | Consumer drops / ring pressure |
| `HEALTH_RF_CLIPPING` | Sample range near full-scale |
| `HEALTH_RF_WEAK` | Very low sample swing |

### `esp_rtl_sdr_device_info_t`

| Field | Type | Meaning |
|---|---|---|
| `vid` / `pid` | `uint16_t` | USB IDs |
| `serial` | `char[32]` | USB serial string |
| `manufacturer` / `product` | `char[48]` | Descriptor strings |
| `high_speed` | `bool` | HS vs FS |
| `present` | `bool` | `false` if no accepted V4 attached |

### `esp_rtl_sdr_metrics_t`

| Field | Meaning |
|---|---|
| `bytes_total` | IQ bytes delivered this stream |
| `blocks_total` | Block count |
| `short_transfers` | Short USB completions |
| `overruns` | USB/host could not keep free slots / pipeline |
| `consumer_drops` | App too slow (ring policy) |
| `sample_min` / `sample_max` | CU8 extrema seen |
| `sample_mean` | `float` mean (ABI-stable, not double) |
| `effective_sps` | Measured delivery rate |
| `frequency_hz` | Last applied LO |
| `sample_rate_sps` | Programmed rate |
| `last_error` | Sticky error code |
| `uptime_ms` | Stream uptime while streaming |

### `esp_rtl_sdr_iq_block_t`

| Field | Meaning |
|---|---|
| `data` | Borrowed CU8 pointer |
| `bytes` | Always even |
| `sequence` | Monotonic per stream (wraps) |
| `frequency_hz` | LO at delivery |
| `sample_rate_sps` | Rate at delivery |
| `host_timestamp_us` | `esp_timer`-style; 0 if unknown |

### `esp_rtl_sdr_error_info_t`

| Field | Meaning |
|---|---|
| `code` | `esp_err_t` |
| `message` | Short human text (`char[96]`) |

Payload of `EVT_ERROR`.

### `esp_rtl_sdr_health_info_t`

| Field | Meaning |
|---|---|
| `struct_size` | Set by driver on output |
| `usb` / `rf` / `overall` | Category enums |
| `efficiency` | `effective_sps / programmed_sps` (0 if unknown) |
| `effective_sps` / `programmed_sps` | Rates |
| `overruns` / `consumer_drops` | Counters |
| `sample_min` / `sample_max` | CU8 range |
| `advice` | Short hint; non-empty when `overall != OK` |

### Passport types

**`esp_rtl_sdr_passport_entry_t`** — one rate row:

| Field | Meaning |
|---|---|
| `requested_sps` / `exact_sps` | Asked vs programmed |
| `effective_sps` | Measured |
| `overruns` / `consumer_drops` | During dwell |
| `sample_min` / `max` | Range |
| `stable` | Met efficiency bar |
| `start_err` | Error if that rate failed to start |

**`esp_rtl_sdr_rate_passport_t`** — full matrix for *this* host + stick:

| Field | Meaning |
|---|---|
| `entry_count` / `entries[]` | Up to `PASSPORT_MAX_ENTRIES` |
| `best_stable_sps` | 0 if none |
| `max_tried_sps` | Highest attempted |
| `probe_freq_hz` / `dwell_ms` | Probe settings |
| `valid` | False until successful probe stored |

**`esp_rtl_sdr_passport_opts_t`** — always `passport_opts_default()` first:

| Field | Meaning |
|---|---|
| `frequency_hz` | LO during probe; 0 = preferred / KZEL |
| `dwell_ms` | Per rate; 0 = default 1500 |
| `min_efficiency_pct` | Stable threshold; 0 = 95 |
| `recommended_only` | Named rates only vs extra high-band steps |

### `esp_rtl_sdr_config_t`

| Field | Default idea | Constraints |
|---|---|---|
| `struct_size` | `sizeof(...)` | Required for validate |
| `host_library_already_installed` | `false` | See ownership |
| `transfer_bytes` | 16384 | [512, 262144], multiple of 512 |
| `transfer_count` | 6 | [2, 8] |
| `control_timeout_ms` | driver default | (0, 30000] |
| `event_cb` | NULL | Optional |
| `event_ctx` | NULL | Passed to callback |
| `iq_acquire_mode` | `false` | Ignored until CAP_IQ_ACQUIRE |
| `usb_task_priority` | 0 = driver default | |
| `usb_task_core_id` | 0xFF = no affinity | 0, 1, or 0xFF |
| `delivery_mode` | `BOTH` | `BOTH` / `CALLBACK` / `READ` (0.7.4+) |
| `pull_ring_bytes` | 0 = auto | Even, 1 KiB…1 MiB if set; lazy alloc when mode uses read |

### Delivery modes (0.7.4)

| Mode | `EVT_IQ_BLOCK` | `read()` | Pull-ring RAM |
|---|---|---|---|
| `DELIVERY_BOTH` (default) | Yes | Yes | Lazy on first IQ or `read` |
| `DELIVERY_CALLBACK` | Yes | `ERR_UNSUPPORTED` | **Never** |
| `DELIVERY_READ` | No | Yes | Lazy on first IQ or `read` |

Other events (`STARTED`, `RETUNED`, `HEALTH`, `ERROR`, …) still use `event_cb` when set.

```c
cfg.delivery_mode = ESP_RTL_SDR_DELIVERY_CALLBACK; /* no pull ring */
/* or */
cfg.delivery_mode = ESP_RTL_SDR_DELIVERY_READ;     /* no IQ callbacks */
cfg.pull_ring_bytes = 256 * 1024;                  /* optional size */
```

Helpers: `esp_rtl_sdr_delivery_mode_uses_callback_iq()` / `_uses_read()`.

### `esp_rtl_sdr_stream_config_t`

| Field | Meaning |
|---|---|
| `preset` | Named LO or `CUSTOM_HZ` |
| `frequency_hz` | Required for `CUSTOM_HZ`; ignored for named presets |
| `sample_rate_sps` | 0 = use preferred; else must be in-window |
| `max_bytes` | 0 = continuous; else even CU8 bound |
| `timeout_ms` | Soft wall-clock cap; 0 = none; ≤ 30000 |

### Event callback type

```c
typedef void (*esp_rtl_sdr_event_cb_t)(esp_rtl_sdr_event_t event,
                                       const void *payload,
                                       void *user_ctx);
```

| Param | Meaning |
|---|---|
| `event` | Discriminator — cast `payload` accordingly |
| `payload` | May be NULL for some events |
| `user_ctx` | From `config.event_ctx` |

See [Events catalog](#17-events-catalog).

### Gain mode (stub surface)

```c
typedef enum {
    ESP_RTL_SDR_GAIN_MODE_AUTO = 0,
    ESP_RTL_SDR_GAIN_MODE_MANUAL = 1,
} esp_rtl_sdr_gain_mode_t;
```

---

## 8. Version & diagnostics

### `esp_rtl_sdr_get_version`

```c
uint32_t esp_rtl_sdr_get_version(void);
```

| | |
|---|---|
| **Returns** | `(major << 16) \| (minor << 8) \| patch` |
| **Use** | Runtime compatibility checks |

### `esp_rtl_sdr_get_version_string`

```c
const char *esp_rtl_sdr_get_version_string(void);
```

| | |
|---|---|
| **Returns** | e.g. `"0.7.3"`; never NULL; static storage |

```c
ESP_LOGI(TAG, "esp_rtl_sdr %s", esp_rtl_sdr_get_version_string());
```

Compile-time: `ESP_RTL_SDR_VERSION_MAJOR/MINOR/PATCH`, `ESP_RTL_SDR_VERSION_STRING`.

---

## 9. Config helpers

### `esp_rtl_sdr_config_default`

```c
void esp_rtl_sdr_config_default(esp_rtl_sdr_config_t *config);
```

| Param | Role |
|---|---|
| `config` | **Out** — zeroed + safe defaults + `struct_size`. NULL-safe no-op. |

**Always call before setting fields.**

### `esp_rtl_sdr_stream_config_default`

```c
void esp_rtl_sdr_stream_config_default(esp_rtl_sdr_stream_config_t *stream);
```

Same pattern for stream configs.

### `esp_rtl_sdr_config_validate` / `stream_config_validate`

```c
esp_err_t esp_rtl_sdr_config_validate(const esp_rtl_sdr_config_t *config);
esp_err_t esp_rtl_sdr_stream_config_validate(const esp_rtl_sdr_stream_config_t *stream);
```

| | |
|---|---|
| **Handle** | Not required |
| **Returns** | `ESP_OK` or `ESP_ERR_INVALID_ARG` / component error |
| **NULL** | → invalid arg |

Validates ranges (transfer size multiple of 512, core id, rates, etc.) without touching USB.

### `esp_rtl_sdr_is_rate_supported`

```c
bool esp_rtl_sdr_is_rate_supported(uint32_t sample_rate_sps);
```

| | |
|---|---|
| **Returns** | `true` if in hardware windows **and** quantizes legally |
| **Notes** | Continuous rates — not only named macros |

### `esp_rtl_sdr_quantize_sample_rate`

```c
bool esp_rtl_sdr_quantize_sample_rate(uint32_t requested_sps, uint32_t *out_exact_sps);
```

| Param | Role |
|---|---|
| `requested_sps` | Desired rate |
| `out_exact_sps` | **Out**, required — exact programmable SPS |

| Returns | |
|---|---|
| `true` | `*out_exact_sps` set |
| `false` | Out of window / NULL out / illegal |

```c
uint32_t exact = 0;
if (!esp_rtl_sdr_quantize_sample_rate(1536000, &exact)) {
    /* gap band or bad */
}
```

### `esp_rtl_sdr_get_supported_rates`

```c
esp_err_t esp_rtl_sdr_get_supported_rates(uint32_t *out_rates,
                                          size_t max_count,
                                          size_t *out_count);
```

| Param | Role |
|---|---|
| `out_rates` | Optional buffer of recommended **named** rates |
| `max_count` | Capacity; **0** = size query only |
| `out_count` | **Out**, required — number available / written |

| Returns | |
|---|---|
| `ESP_OK` | Success |
| Invalid arg | NULL `out_count`, etc. |

**Note:** Continuous in-window rates are accepted by `set_sample_rate` / `start` even when **not** in this list.

```c
size_t n = 0;
ESP_ERROR_CHECK(esp_rtl_sdr_get_supported_rates(NULL, 0, &n));
uint32_t rates[16];
size_t written = 0;
ESP_ERROR_CHECK(esp_rtl_sdr_get_supported_rates(rates,
    n < 16 ? n : 16, &written));
```

### `esp_rtl_sdr_normalize_frequency`

```c
bool esp_rtl_sdr_normalize_frequency(uint32_t in_hz, uint32_t *out_hz);
```

Clamp + quantize to policy. Returns `false` if out of absolute range or `out_hz` is NULL.

### `esp_rtl_sdr_preset_frequency_hz`

```c
esp_err_t esp_rtl_sdr_preset_frequency_hz(esp_rtl_sdr_preset_t preset,
                                          uint32_t *out_hz);
```

| Returns | |
|---|---|
| `ESP_OK` | Named preset LO written |
| `ESP_ERR_INVALID_ARG` | `CUSTOM_HZ` or NULL out (caller must supply Hz) |

---

## 10. Lifecycle

### `esp_rtl_sdr_install`

```c
esp_err_t esp_rtl_sdr_install(const esp_rtl_sdr_config_t *config,
                              esp_rtl_sdr_handle_t *out_handle);
```

| Param | Role |
|---|---|
| `config` | **Required** — prefer `config_default` + validate |
| `out_handle` | **Required** — on success non-NULL; on failure **NULL** (cleared first) |

| Returns | |
|---|---|
| `ESP_OK` | Handle ready (`IDLE`) |
| Invalid arg | Bad config / NULL |
| Other | Resource / USB client setup failure |

**Does not require a dongle present.** Attach is reported via events when devices appear.

```c
esp_rtl_sdr_config_t cfg;
esp_rtl_sdr_config_default(&cfg);
cfg.event_cb = on_sdr;
cfg.event_ctx = app;

esp_rtl_sdr_handle_t sdr = NULL;
esp_err_t err = esp_rtl_sdr_install(&cfg, &sdr);
if (err != ESP_OK) {
    /* sdr is NULL */
}
```

### `esp_rtl_sdr_uninstall`

```c
esp_err_t esp_rtl_sdr_uninstall(esp_rtl_sdr_handle_t handle);
```

| | |
|---|---|
| **NULL** | **Idempotent** → `ESP_OK` |
| **Streaming** | Best-effort `stop` first |
| **Second call** | `STALE_HANDLE` (do not retain the pointer) |
| **Ownership** | Single owner task; no concurrent API during uninstall |

```c
(void)esp_rtl_sdr_uninstall(sdr);
sdr = NULL;
```

---

## 11. Queries

### `esp_rtl_sdr_get_state`

```c
esp_rtl_sdr_state_t esp_rtl_sdr_get_state(esp_rtl_sdr_handle_t handle);
```

| | |
|---|---|
| **NULL / stale** | Returns `UNINSTALLED` without crashing |
| **Lock** | Short timeout → FAULT-like snapshot if contended |

### `esp_rtl_sdr_get_last_error`

```c
esp_err_t esp_rtl_sdr_get_last_error(esp_rtl_sdr_handle_t handle);
```

Sticky last error on handle. Invalid handle → `STALE_HANDLE`.

### `esp_rtl_sdr_get_device_info`

```c
esp_err_t esp_rtl_sdr_get_device_info(esp_rtl_sdr_handle_t handle,
                                      esp_rtl_sdr_device_info_t *out_info);
```

| Param | Role |
|---|---|
| `out_info` | **Out** — not modified on failure |

`present == false` if no accepted V4 is attached. Thread-safe snapshot.

### `esp_rtl_sdr_get_metrics`

```c
esp_err_t esp_rtl_sdr_get_metrics(esp_rtl_sdr_handle_t handle,
                                  esp_rtl_sdr_metrics_t *out_metrics);
```

Thread-safe. `uptime_ms` computed at snapshot while `STREAMING`. `out_metrics` untouched on failure.

```c
esp_rtl_sdr_metrics_t m;
if (esp_rtl_sdr_get_metrics(sdr, &m) == ESP_OK) {
    ESP_LOGI(TAG, "eff_sps=%u overruns=%u drops=%u",
             (unsigned)m.effective_sps, (unsigned)m.overruns,
             (unsigned)m.consumer_drops);
}
```

---

## 12. Streaming

### `esp_rtl_sdr_start`

```c
esp_err_t esp_rtl_sdr_start(esp_rtl_sdr_handle_t handle,
                            const esp_rtl_sdr_stream_config_t *stream);
```

Claim interface → clean-room init → sample rate → tune → multi-URB bulk IN.

| Param | Role |
|---|---|
| `handle` | Live handle in `IDLE` |
| `stream` | **Required** — use `stream_config_default` |

| Return | Meaning |
|---|---|
| `ESP_OK` | Streaming (or transitioning to) |
| `ESP_ERR_INVALID_ARG` / `BAD_RATE` / `BAD_FREQ` | Policy |
| `ERR_BUSY` | Already streaming / stopping |
| `ERR_NO_DEVICE` | No V4 |
| `ERR_UNSUPPORTED` | Path not built |
| `ERR_REENTRANT` | Called from event callback |
| `ERR_USB` / `TIMEOUT` / `FAULT` | Hardware |

**On failure:** handle remains **IDLE** (or **FAULT** if unrecoverable). Never half-open claim.

```c
esp_rtl_sdr_stream_config_t st;
esp_rtl_sdr_stream_config_default(&st);
st.preset = ESP_RTL_SDR_PRESET_CUSTOM_HZ;
st.frequency_hz = 100100000;
st.sample_rate_sps = ESP_RTL_SDR_RATE_960K;
esp_err_t err = esp_rtl_sdr_start(sdr, &st);
```

### `esp_rtl_sdr_start_hz`

```c
esp_err_t esp_rtl_sdr_start_hz(esp_rtl_sdr_handle_t handle,
                               uint32_t frequency_hz,
                               uint32_t sample_rate_sps);
```

| Param | Semantics |
|---|---|
| `frequency_hz` | **0** → preferred center (must already be set) |
| `sample_rate_sps` | **0** → preferred rate (default ~960k after install) |

Builds a stream config and calls `start`. Same error set as `start`.

```c
ESP_ERROR_CHECK(esp_rtl_sdr_set_center_freq(sdr, 162400000));
ESP_ERROR_CHECK(esp_rtl_sdr_set_sample_rate(sdr, ESP_RTL_SDR_RATE_960K));
ESP_ERROR_CHECK(esp_rtl_sdr_start_hz(sdr, 0, 0));
```

### `esp_rtl_sdr_retune_hz`

```c
esp_err_t esp_rtl_sdr_retune_hz(esp_rtl_sdr_handle_t handle, uint32_t frequency_hz);
```

In-stream LO change. **Drains bulk URBs → EP0 tune → resubmit.** Never EP0 while bulk is outstanding.

| Caller | Behavior |
|---|---|
| App task | Applies on calling task; may block briefly while URBs drain |
| Event callback | **Queues** only; returns `ESP_OK`; delivery task applies later; emits `EVT_RETUNED` |
| Coalescing | Newer pending retune replaces target LO |

| Returns | |
|---|---|
| `ESP_OK` | Accepted (applied **or** queued) |
| `ERR_NOT_STREAMING` | Idle |
| `BAD_FREQ` / `FAULT` / `TIMEOUT` | Reject / hardware |

**0.7.3:** intentional async from callback — does **not** return `ERR_REENTRANT`.

```c
/* from IQ callback — non-blocking queue */
if (need_jump) {
    (void)esp_rtl_sdr_retune_hz(sdr, 1090000000u);
}
```

### `esp_rtl_sdr_stop`

```c
esp_err_t esp_rtl_sdr_stop(esp_rtl_sdr_handle_t handle, uint32_t timeout_ms);
```

| Param | Role |
|---|---|
| `timeout_ms` | **0** → `DEFAULT_STOP_TIMEOUT_MS` (3000) |

| | |
|---|---|
| **Idempotent** | Already idle → `ESP_OK` |
| **NULL handle** | `ESP_OK` |
| **Events** | `EVT_STOPPED` once when leaving stream |

### `esp_rtl_sdr_reset`

```c
esp_err_t esp_rtl_sdr_reset(esp_rtl_sdr_handle_t handle);
```

Clear `FAULT` → `IDLE` if not streaming. Streaming → `ERR_BUSY`. Clears metrics counters on success.

### `esp_rtl_sdr_release_iq_block`

```c
esp_err_t esp_rtl_sdr_release_iq_block(esp_rtl_sdr_handle_t handle,
                                       const esp_rtl_sdr_iq_block_t *block);
```

| Mode | Behavior |
|---|---|
| Borrow (default) | **No-op** returning `ESP_OK` if `block` non-NULL — safe to call always |
| Acquire (`CAP_IQ_ACQUIRE`) | Required before buffer reuse — **not enabled yet** |

---

## 13. Center frequency, rate, sync read

### `esp_rtl_sdr_set_center_freq` / `get_center_freq`

```c
esp_err_t esp_rtl_sdr_set_center_freq(esp_rtl_sdr_handle_t handle, uint32_t frequency_hz);
esp_err_t esp_rtl_sdr_get_center_freq(esp_rtl_sdr_handle_t handle, uint32_t *out_hz);
```

| State | `set` behavior |
|---|---|
| `IDLE` | Stores preferred LO for next start |
| `STREAMING` | Same as `retune_hz` (async if from callback) |

Frequency normalized (quantize/clamp). **0 rejected.**

`get`: last applied or preferred; **0** if never set and never streamed. `out_hz` required.

### `esp_rtl_sdr_set_sample_rate` / `get_sample_rate`

```c
esp_err_t esp_rtl_sdr_set_sample_rate(esp_rtl_sdr_handle_t handle, uint32_t sample_rate_sps);
esp_err_t esp_rtl_sdr_get_sample_rate(esp_rtl_sdr_handle_t handle, uint32_t *out_sps);
```

| State | `set` behavior |
|---|---|
| `IDLE` | Stores preferred (quantized) for next start if stream rate is 0 |
| `STREAMING` | **`ERR_BUSY`** — rate change requires stop/start (Phase 1) |

Must pass `is_rate_supported`. `get` returns last applied or preferred exact SPS.

### `esp_rtl_sdr_read`

```c
esp_err_t esp_rtl_sdr_read(esp_rtl_sdr_handle_t handle,
                           uint8_t *out_buf,
                           size_t max_bytes,
                           uint32_t timeout_ms,
                           size_t *out_bytes);
```

Blocking pull of interleaved CU8 IQ. Works **with or without** an event callback while `STREAMING`.

| Param | Role |
|---|---|
| `out_buf` | **Required** destination |
| `max_bytes` | Capacity; odd values truncated down to even |
| `timeout_ms` | **0** = non-blocking poll of ring |
| `out_bytes` | **Required** — bytes copied (0 on empty timeout) |

| Returns | |
|---|---|
| `ESP_OK` | **Any** bytes copied (`*out_bytes > 0`) |
| `ERR_TIMEOUT` | None within timeout while streaming |
| `ERR_NOT_STREAMING` | Idle |
| Other | Fault / arg errors |

```c
uint8_t buf[16384];
size_t n = 0;
esp_err_t err = esp_rtl_sdr_read(sdr, buf, sizeof(buf), 1000, &n);
if (err == ESP_OK) {
    /* process n bytes */
} else if (err == ESP_RTL_SDR_ERR_TIMEOUT) {
    /* still streaming; no data yet */
}
```

---

## 14. PPM & multi-device

### `esp_rtl_sdr_set_freq_correction` / `get_freq_correction`

```c
esp_err_t esp_rtl_sdr_set_freq_correction(esp_rtl_sdr_handle_t handle, int ppm);
esp_err_t esp_rtl_sdr_get_freq_correction(esp_rtl_sdr_handle_t handle, int *out_ppm);
```

Software LO offset:

```text
tune_hz = request_hz + request_hz * ppm / 1e6   (integer)
```

| | |
|---|---|
| Range | `[PPM_MIN, PPM_MAX]` = ±200 |
| Default | 0 |
| Effect | Next tune / retune / `set_center_freq` |

### Multi-device

```c
esp_err_t esp_rtl_sdr_refresh_device_list(esp_rtl_sdr_handle_t handle);
esp_err_t esp_rtl_sdr_get_device_count(esp_rtl_sdr_handle_t handle, size_t *out_count);
esp_err_t esp_rtl_sdr_get_device_at(esp_rtl_sdr_handle_t handle, size_t index,
                                    esp_rtl_sdr_device_info_t *out_info);
esp_err_t esp_rtl_sdr_select_device(esp_rtl_sdr_handle_t handle, size_t index);
esp_err_t esp_rtl_sdr_select_device_serial(esp_rtl_sdr_handle_t handle, const char *serial);
```

| Function | Semantics |
|---|---|
| `refresh_device_list` | Rescan USB for accepted profiles; does not close current unless vanished |
| `get_device_count` | Candidates after last refresh / install scan |
| `get_device_at` | Snapshot index `[0, count)`; does not change open device |
| `select_device` | By index; must **not** be streaming; `ERR_BAD_DEVICE` if invalid |
| `select_device_serial` | Exact, case-sensitive serial match; must not stream |

```c
ESP_ERROR_CHECK(esp_rtl_sdr_refresh_device_list(sdr));
size_t n = 0;
ESP_ERROR_CHECK(esp_rtl_sdr_get_device_count(sdr, &n));
for (size_t i = 0; i < n; i++) {
    esp_rtl_sdr_device_info_t di;
    if (esp_rtl_sdr_get_device_at(sdr, i, &di) == ESP_OK) {
        ESP_LOGI(TAG, "[%u] %s %s", (unsigned)i, di.product, di.serial);
    }
}
if (n > 0) {
    ESP_ERROR_CHECK(esp_rtl_sdr_select_device(sdr, 0));
}
```

---

## 15. Intent, health, passport

### `esp_rtl_sdr_apply_need`

```c
esp_err_t esp_rtl_sdr_apply_need(esp_rtl_sdr_handle_t handle, esp_rtl_sdr_need_t need);
```

Sets preferred LO + sample rate (quantized). **Does not start streaming.**

| Need | Notes |
|---|---|
| `NEED_HF` | LO stored; upconverter CAP open — honesty first |
| `NEED_MAX_STABLE` | Uses last successful passport `best_stable_sps` when `valid` |

```c
ESP_ERROR_CHECK(esp_rtl_sdr_apply_need(sdr, ESP_RTL_SDR_NEED_ADSB));
ESP_ERROR_CHECK(esp_rtl_sdr_start_hz(sdr, 0, 0));
```

### `esp_rtl_sdr_get_health`

```c
esp_err_t esp_rtl_sdr_get_health(esp_rtl_sdr_handle_t handle,
                                 esp_rtl_sdr_health_info_t *out_health);
```

Snapshot from live metrics. Safe while streaming. Use `advice[]` for UI / logs.

```c
esp_rtl_sdr_health_info_t h;
ESP_ERROR_CHECK(esp_rtl_sdr_get_health(sdr, &h));
if (h.overall != ESP_RTL_SDR_HEALTH_OK) {
    ESP_LOGW(TAG, "health: %s", h.advice);
}
```

### `esp_rtl_sdr_passport_opts_default`

```c
void esp_rtl_sdr_passport_opts_default(esp_rtl_sdr_passport_opts_t *opts);
```

NULL-safe pattern: always call before setting fields.

### `esp_rtl_sdr_probe_rates`

```c
esp_err_t esp_rtl_sdr_probe_rates(esp_rtl_sdr_handle_t handle,
                                  const esp_rtl_sdr_passport_opts_t *opts,
                                  esp_rtl_sdr_rate_passport_t *out_passport);
```

On-device rate passport for **this** P4 + stick.

| Constraint | |
|---|---|
| Must **not** already be streaming | |
| Blocks ~ `entry_count * dwell_ms` | |
| Emits | `EVT_PASSPORT_PROGRESS`, `EVT_PASSPORT_DONE` |
| Stores | Passport on handle for `NEED_MAX_STABLE` |

```c
esp_rtl_sdr_passport_opts_t opts;
esp_rtl_sdr_passport_opts_default(&opts);
opts.dwell_ms = 800;
opts.recommended_only = true;

esp_rtl_sdr_rate_passport_t pass;
esp_err_t err = esp_rtl_sdr_probe_rates(sdr, &opts, &pass);
if (err == ESP_OK && pass.valid) {
    ESP_LOGI(TAG, "best_stable=%u", (unsigned)pass.best_stable_sps);
    ESP_ERROR_CHECK(esp_rtl_sdr_apply_need(sdr, ESP_RTL_SDR_NEED_MAX_STABLE));
}
```

### `esp_rtl_sdr_get_rate_passport`

```c
esp_err_t esp_rtl_sdr_get_rate_passport(esp_rtl_sdr_handle_t handle,
                                        esp_rtl_sdr_rate_passport_t *out_passport);
```

Copy last passport. `valid == false` if never probed successfully.

---

## 16. Gain & bias (stubs)

These exist so apps can compile against a stable surface. **Hardware effect is not claimed** until CAP bits flip after clean-room capture ([`GAIN_BIAS_CAPTURE.md`](GAIN_BIAS_CAPTURE.md)).

| Function | Today |
|---|---|
| `set_tuner_gain_mode` | Always `ERR_UNSUPPORTED` |
| `get_tuner_gain_mode` | Returns last requested (default AUTO) — OK even with CAP off |
| `set_tuner_gain` | `ERR_UNSUPPORTED` (tenths of dB, e.g. 496 = 49.6 dB) |
| `get_tuner_gain` | Last requested; 0 if never set |
| `get_tuner_gains` | `*out_count = 0`, `ESP_OK` if buffers valid |
| `set_bias_tee` | `ERR_UNSUPPORTED` |
| `get_bias_tee` | Last requested preference |

```c
if (esp_rtl_sdr_get_capabilities() & ESP_RTL_SDR_CAP_GAIN) {
    ESP_ERROR_CHECK(esp_rtl_sdr_set_tuner_gain_mode(sdr, ESP_RTL_SDR_GAIN_MODE_MANUAL));
    ESP_ERROR_CHECK(esp_rtl_sdr_set_tuner_gain(sdr, 400));
} else {
    /* expected path on 0.7.x */
}
```

---

## 17. Events catalog

| Event | Payload type | When |
|---|---|---|
| `EVT_ENUMERATED` | `device_info_t` | Device seen |
| `EVT_READY` | — / info | Accepted, not streaming |
| `EVT_STREAM_STARTED` | — | Bulk running |
| `EVT_IQ_BLOCK` | `iq_block_t` **borrowed** | Each IQ transfer delivered |
| `EVT_STOPPED` | — | Left streaming (once) |
| `EVT_ERROR` | `error_info_t` | Fault / USB error narrative |
| `EVT_DISCONNECTED` | — | Device gone |
| `EVT_RETUNED` | `const uint32_t *` frequency_hz | LO applied (incl. async) |
| `EVT_HEALTH` | `health_info_t` | Health snapshot |
| `EVT_PASSPORT_PROGRESS` | `passport_entry_t` | One rate finished |
| `EVT_PASSPORT_DONE` | `rate_passport_t` | Probe complete |

### Full callback skeleton

```c
static void on_sdr(esp_rtl_sdr_event_t ev, const void *payload, void *ctx)
{
    esp_rtl_sdr_handle_t sdr = (esp_rtl_sdr_handle_t)ctx;

    switch (ev) {
    case ESP_RTL_SDR_EVT_IQ_BLOCK: {
        const esp_rtl_sdr_iq_block_t *iq = payload;
        if (!iq || !iq->data) break;
        /* process quickly OR copy to your ring */
        (void)esp_rtl_sdr_release_iq_block(sdr, iq); /* no-op in borrow mode */
        break;
    }
    case ESP_RTL_SDR_EVT_RETUNED: {
        const uint32_t *hz = payload;
        ESP_LOGI(TAG, "retuned %u", hz ? (unsigned)*hz : 0);
        break;
    }
    case ESP_RTL_SDR_EVT_ERROR: {
        const esp_rtl_sdr_error_info_t *e = payload;
        ESP_LOGE(TAG, "sdr error: %s", e ? e->message : "?");
        break;
    }
    case ESP_RTL_SDR_EVT_HEALTH: {
        const esp_rtl_sdr_health_info_t *h = payload;
        if (h && h->overall != ESP_RTL_SDR_HEALTH_OK) {
            ESP_LOGW(TAG, "%s", h->advice);
        }
        break;
    }
    case ESP_RTL_SDR_EVT_DISCONNECTED:
        ESP_LOGW(TAG, "dongle disconnected");
        break;
    default:
        break;
    }
    /* NEVER: start / stop / uninstall / reset / probe_rates here */
}
```

---

## 18. Recipes

### A. Async-only stream (UI / DSP task elsewhere)

```c
/* install with event_cb; process EVT_IQ_BLOCK; stop/uninstall from owner task */
```

### B. Sync-only stream (no callback)

```c
esp_rtl_sdr_config_t cfg;
esp_rtl_sdr_config_default(&cfg);
/* cfg.event_cb left NULL */

esp_rtl_sdr_handle_t sdr = NULL;
ESP_ERROR_CHECK(esp_rtl_sdr_install(&cfg, &sdr));
ESP_ERROR_CHECK(esp_rtl_sdr_start_hz(sdr, 100100000, ESP_RTL_SDR_RATE_960K));

for (;;) {
    uint8_t buf[8192];
    size_t n = 0;
    esp_err_t e = esp_rtl_sdr_read(sdr, buf, sizeof(buf), 500, &n);
    if (e == ESP_OK) {
        /* demod n bytes */
    } else if (e != ESP_RTL_SDR_ERR_TIMEOUT) {
        break;
    }
}
(void)esp_rtl_sdr_stop(sdr, 0);
(void)esp_rtl_sdr_uninstall(sdr);
```

### C. Both callback + `read`

Supported: callback for events/metrics side-channel; another task may `read` the pull ring. Still return quickly from the callback.

### D. Mission presets + health loop

```c
ESP_ERROR_CHECK(esp_rtl_sdr_apply_need(sdr, ESP_RTL_SDR_NEED_WX));
ESP_ERROR_CHECK(esp_rtl_sdr_start_hz(sdr, 0, 0));

while (running) {
    esp_rtl_sdr_health_info_t h;
    if (esp_rtl_sdr_get_health(sdr, &h) == ESP_OK &&
        h.overall == ESP_RTL_SDR_HEALTH_USB_STARVING) {
        /* optional: stop, lower rate, restart */
    }
    vTaskDelay(pdMS_TO_TICKS(1000));
}
```

### E. Hot retune from IQ callback (0.7.3+)

```c
case ESP_RTL_SDR_EVT_IQ_BLOCK: {
    if (scan_next) {
        (void)esp_rtl_sdr_retune_hz(sdr, next_hz); /* queues; look for EVT_RETUNED */
        scan_next = false;
    }
    break;
}
```

### F. Fail-closed start handling

```c
esp_err_t err = esp_rtl_sdr_start_hz(sdr, 1090000000u, ESP_RTL_SDR_RATE_2048K);
if (err == ESP_RTL_SDR_ERR_NO_DEVICE) {
    /* still IDLE — show "plug Blog V4" */
} else if (err != ESP_OK) {
    ESP_LOGE(TAG, "%s state=%s", esp_rtl_sdr_err_to_name(err),
             esp_rtl_sdr_state_to_name(esp_rtl_sdr_get_state(sdr)));
}
```

---

## 19. Symbol index

| Symbol | Section |
|---|---|
| `esp_rtl_sdr_get_version` / `_string` | §8 |
| `esp_rtl_sdr_err_to_name` | §4 |
| `esp_rtl_sdr_state_to_name` | §7 |
| `esp_rtl_sdr_get_capabilities` | §5 |
| `esp_rtl_sdr_config_default` / `validate` | §9 |
| `esp_rtl_sdr_stream_config_default` / `validate` | §9 |
| `esp_rtl_sdr_is_rate_supported` | §9 |
| `esp_rtl_sdr_quantize_sample_rate` | §9 |
| `esp_rtl_sdr_get_supported_rates` | §9 |
| `esp_rtl_sdr_normalize_frequency` | §9 |
| `esp_rtl_sdr_preset_frequency_hz` | §9 |
| `esp_rtl_sdr_install` / `uninstall` | §10 |
| `esp_rtl_sdr_get_state` / `get_last_error` | §11 |
| `esp_rtl_sdr_get_device_info` / `get_metrics` | §11 |
| `esp_rtl_sdr_start` / `start_hz` / `stop` | §12 |
| `esp_rtl_sdr_retune_hz` / `reset` | §12 |
| `esp_rtl_sdr_release_iq_block` | §12 |
| `esp_rtl_sdr_set/get_center_freq` | §13 |
| `esp_rtl_sdr_set/get_sample_rate` | §13 |
| `esp_rtl_sdr_read` | §13 |
| `esp_rtl_sdr_set/get_freq_correction` | §14 |
| `esp_rtl_sdr_refresh_device_list` | §14 |
| `esp_rtl_sdr_get_device_count` / `get_device_at` | §14 |
| `esp_rtl_sdr_select_device` / `select_device_serial` | §14 |
| `esp_rtl_sdr_apply_need` | §15 |
| `esp_rtl_sdr_get_health` | §15 |
| `esp_rtl_sdr_passport_opts_default` | §15 |
| `esp_rtl_sdr_probe_rates` / `get_rate_passport` | §15 |
| `esp_rtl_sdr_set/get_tuner_gain*` | §16 |
| `esp_rtl_sdr_set/get_bias_tee` | §16 |

---

## Related documents

| Doc | Role |
|---|---|
| [`API.md`](API.md) | Design contract, threading invariants, versioning policy |
| [`../include/esp_rtl_sdr.h`](../include/esp_rtl_sdr.h) | Source of truth for signatures |
| [`../PROJECT_TRUTH.md`](../PROJECT_TRUTH.md) | Hardware / maturity claims |
| [`RATES.md`](RATES.md) | Rate windows deep dive |
| [`../architecture.md`](../architecture.md) | Layering & USB path |
| [`../examples/p4_serial_smoke/`](../examples/p4_serial_smoke/) | Runnable smoke |

---

*If this reference disagrees with the header, the header wins for signatures; if it oversells hardware, PROJECT_TRUTH wins — open a `truth:` issue.*
