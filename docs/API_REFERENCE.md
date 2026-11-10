# esp_rtl_sdr — API Reference

> **Version tracked:** `0.9.6` (see `ESP_RTL_SDR_VERSION_*` in [`include/esp_rtl_sdr.h`](../include/esp_rtl_sdr.h))
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
| **CAP-gated** | Check `get_device_capabilities(handle)` for the attached receiver; `get_capabilities()` only describes binary features |

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
16. [Gain & bias (board-specific PC controls)](#16-gain--bias-board-specific-pc-controls)
17. [Events catalog](#17-events-catalog)
18. [Recipes](#18-recipes)
19. [Symbol index](#19-symbol-index)
20. [Profile, routing, identity and recovery](#20-profile-routing-identity-and-recovery)
21. [Acquisition metadata](#21-acquisition-metadata)

---

## 1. Ten-minute path

```c
#include "esp_rtl_sdr.h"

/* Keep this pointer in owner-task storage across calls and failures. */
static esp_rtl_sdr_handle_t sdr;

esp_err_t app_radio_start(void)
{
    if (sdr != NULL) return ESP_RTL_SDR_ERR_BUSY;
    esp_rtl_sdr_config_t cfg;
    esp_rtl_sdr_config_default(&cfg);
    cfg.delivery_mode = ESP_RTL_SDR_DELIVERY_READ;
    esp_err_t err = esp_rtl_sdr_install(&cfg, &sdr);
    if (err != ESP_OK) return err; /* sdr may need cleanup even on failure */
    return esp_rtl_sdr_start_hz(sdr, 100100000u, ESP_RTL_SDR_RATE_960K);
}

esp_err_t app_radio_read(uint8_t *buf, size_t capacity, size_t *bytes)
{
    return esp_rtl_sdr_read(sdr, buf, capacity, 100, bytes);
}

esp_err_t app_radio_close(void)
{
    esp_err_t err = esp_rtl_sdr_uninstall(sdr); /* includes stop */
    if (err == ESP_OK) sdr = NULL;
    return err; /* retain cleanup-only sdr on failure; owner schedules retry */
}
```

Call start once from the owner task; read only after a successful start and
process exactly the returned byte count. TIMEOUT means no bytes this time.
After any install/start failure, call close if the stored pointer is non-NULL;
a failed close leaves that pointer available for later cleanup retries. Do not
retry normal controls through a cleanup-only handle. Board VBUS setup is the
application's responsibility; this snippet does not configure a board.

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

Opaque. One handle represents one logical receiver session, selected from the implemented profiles. Multiple handles share a USB host session with exclusive device-address claims. Never free a handle yourself: `esp_rtl_sdr_uninstall()` invalidates it only on `ESP_OK`. On failure retain the cleanup-only pointer for retry; never call through a pointer after successful uninstall.

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
3. Base config/stream validation accepts sizes between the legacy minimum and current `sizeof`; absent trailing config fields receive defaults. Larger sizes are rejected. This compatibility rule is not universal: acquisition config and `read_ex` metadata require exact sizes (§21).

### Discover before assume

```c
uint32_t caps = esp_rtl_sdr_get_device_capabilities(sdr);
if (!(caps & ESP_RTL_SDR_CAP_GAIN)) {
    /* No accepted device is open, or this receiver lacks gain control. */
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
| App FreeRTOS task | Public API subject to function-specific state/ownership rules | Concurrent `uninstall` with other calls on same handle |
| Event callback | `get_state`, `get_metrics`, `get_device_info`, `get_last_error`, `release_iq_block`, **`retune_hz` / `set_center_freq` (queued async)** | `install`, `uninstall`, `start`, `start_hz`, `stop`, `reset`, `probe_rates` → `ERR_REENTRANT` |
| USB completion ISR | Nothing | All public APIs |

- Query/control paths use per-handle synchronization; use one owner for lifecycle and one legacy reader. Do not infer that every public function acquires the same mutex.
- Event callbacks run **outside** that mutex so you can call safe query APIs.  
- All current IQ callback pointers are **borrowed** until callback return; the acquire flag is ignored.
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
| Gap (rejected) | 300001 … 900000 Hz |
| High band | **900001 … 3200000** Hz |

Any in-window rate is quantized to an exact RTL2832 ratio (`quantize_sample_rate`). Named macros (`RATE_960K`, `RATE_2048K`, …) are conveniences — not the only legal values. Details: [`RATES.md`](RATES.md).

### Frequency policy

| Item | Value |
|---|---|
| Arithmetic range | 24 kHz … 1766 MHz (`FREQ_MIN_HZ` … `FREQ_MAX_HZ`) |
| Device range | Profile-specific; Blog V4 accepts the full arithmetic range, Blog V3/V3c uses Q-branch direct sampling below 24 MHz. Nooelec SMArt v5 accepts 100 kHz–1750 MHz, using Q below the captured 24 MHz route cutoff; its manufacturer rates native operation from 25 MHz and the PC 24 MHz boundary tune warned of no PLL lock. Nooelec tuner gain/mode and bandwidth setters are unsupported while Q bypasses the tuner. |
| Quantization | Exact Hz (`FREQ_QUANT_HZ` = 1) |
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
| `ESP_RTL_SDR_ERR_NO_DEVICE` | No accepted receiver available | Wait / hotplug / refresh list |
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
| `ESP_RTL_SDR_ERR_UNSUPPORTED` | Wrong delivery mode, or path not built | Check capabilities / mode |
| `ESP_RTL_SDR_ERR_STALE_HANDLE` | Handle validation failed | Never reuse a pointer after successful uninstall; validation cannot make a freed pointer safe |
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

### Capability map

This table describes the binary's implemented paths. `esp_rtl_sdr_get_device_capabilities()`
narrows that mask for the active profile; detached/unknown devices return zero.
Neither mask establishes RF calibration or hardware soak acceptance.

| Flag | Bit | Status | Meaning |
|---|---|---|---|
| `ESP_RTL_SDR_CAP_STREAM` | 0 | **On** | `start` / `stop` bulk IQ |
| `ESP_RTL_SDR_CAP_RETUNE` | 1 | **On** | In-stream `retune_hz` |
| `ESP_RTL_SDR_CAP_HOTPLUG` | 2 | **On** | Disconnect / reconnect events |
| `ESP_RTL_SDR_CAP_METRICS` | 3 | **On** | `get_metrics` |
| `ESP_RTL_SDR_CAP_CUSTOM_HZ` | 4 | **On** | `PRESET_CUSTOM_HZ` |
| `ESP_RTL_SDR_CAP_BIAS_TEE` | 5 | **Profile-specific** | Manual GPIO bias control on V4/V4L/BlogV3; generic BlogV3 identity requires a user warning |
| `ESP_RTL_SDR_CAP_DIRECT_SAMPLING` | 6 | **Profile-specific** | V3/V3c and Nooelec V5 automatic Q branch below 24 MHz |
| `ESP_RTL_SDR_CAP_IQ_ACQUIRE` | 7 | **Off** | Borrow mode only |
| `ESP_RTL_SDR_CAP_FREQ_CORRECTION` | 8 | **On** | Software ppm |
| `ESP_RTL_SDR_CAP_MULTI_DEVICE` | 9 | **On** | Enumerate / select |
| `ESP_RTL_SDR_CAP_SYNC_READ` | 10 | **On** | Blocking `read` |
| `ESP_RTL_SDR_CAP_CONTINUOUS_RATE` | 11 | **On** | In-window + quantize |
| `ESP_RTL_SDR_CAP_NEED` | 12 | **On** | `apply_need` |
| `ESP_RTL_SDR_CAP_HEALTH` | 13 | **On** | `get_health` / `EVT_HEALTH` |
| `ESP_RTL_SDR_CAP_PASSPORT` | 14 | **On** | `probe_rates` |
| `ESP_RTL_SDR_CAP_GAIN` | 15 | **Profile-specific** | Nominal manual ladders on all five implemented profiles |
| `ESP_RTL_SDR_CAP_DELIVERY_MODE` | 16 | **On** | `config.delivery_mode` |
| `ESP_RTL_SDR_CAP_HF_UPCONVERTER` | 17 | **Profile-specific** | V4/V4L RF&lt;28.8 MHz → tuner LO+28.8e6 unless route override applies |
| `ESP_RTL_SDR_CAP_GAIN_AUTO` | 18 | **On** | Tuner AGC AUTO (measured 05/07/0c) |
| `ESP_RTL_SDR_CAP_RTL_AGC` | 19 | **On** | RTL2832 digital AGC (demod 0x19) |
| `ESP_RTL_SDR_CAP_TUNER_BANDWIDTH` | 20 | **Profile-specific** | Measured filter/PLL/demod IF choices at 2.4 MS/s; V3 direct-Q HF unsupported |
| `ESP_RTL_SDR_CAP_OFFSET_TUNING` | 21 | **Profile-specific** | Experimental E4000; configure in IDLE |

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
| `ESP_RTL_SDR_RATE_HIGH_MIN_HZ` | 900001 |
| `ESP_RTL_SDR_RATE_HIGH_MAX_HZ` | 3200000 |
| `ESP_RTL_SDR_RATE_STABLE_MAX_HZ` | 2560000 |
| `ESP_RTL_SDR_XTAL_HZ` | 28800000 |
| `ESP_RTL_SDR_PPM_MIN` / `MAX` | −200 / +200 |
| `ESP_RTL_SDR_FREQ_MIN_HZ` / `MAX` | 24e3 / 1766e6 |
| `ESP_RTL_SDR_FREQ_QUANT_HZ` | 1 |
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
| `NEED_HF` | Stores HF LO intent; V4 routing is applied when streaming starts |
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
| `pull_ring_bytes` | 0 = auto | Even, 1 KiB…1 MiB if set; allocated before submissions when mode uses read |
| `bind_device_index` | `ESP_RTL_SDR_BIND_ANY` | First unclaimed accepted address, or candidate index below `ESP_RTL_SDR_MAX_DEVICES` |
| `bind_serial` | Empty | Optional exact serial match; selection does not make identical/empty serials unique |

**Legacy `struct_size`:** If the app was compiled against a header that ends at
`usb_task_core_id` (or any size that omits the new trailing fields),
`install` / `config_validate` copy only `struct_size` bytes over defaults — so
**`delivery_mode` stays `BOTH`** and **`pull_ring_bytes` stays `0` (auto)**. Apps
need a current header (and full `struct_size`) to select CALLBACK/READ or a
custom pull size.

### Delivery modes (0.7.4)

| Mode | `EVT_IQ_BLOCK` | `read()` | Pull-ring RAM |
|---|---|---|---|
| `DELIVERY_BOTH` (default) | Yes | Yes | At start, before bulk submission |
| `DELIVERY_CALLBACK` | Yes | `ERR_UNSUPPORTED` | **Never** |
| `DELIVERY_READ` | No | Yes | At start, before bulk submission |

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

### Gain mode

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

Validate and normalize to the exact-Hz arithmetic policy. Returns `false` if
outside 24 kHz…1766 MHz or `out_hz` is NULL. A later start/retune can still
reject a frequency that the selected device profile does not support.

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
| `out_handle` | **Required** — cleared first; non-NULL on success. On failure normally NULL; a non-NULL value is a retained cleanup-only handle requiring uninstall retry |

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
    /* If sdr != NULL, retain it and retry uninstall; partial cleanup failed. */
}
```

### `esp_rtl_sdr_uninstall`

```c
esp_err_t esp_rtl_sdr_uninstall(esp_rtl_sdr_handle_t handle);
```

| | |
|---|---|
| **NULL** | **Idempotent** → `ESP_OK` |
| **Streaming** | Drain transfers, then join workers; failures retain resources |
| **Success** | Destroys handle; never reuse its pointer |
| **Failure** | Retains cleanup-only handle; keep pointer and retry uninstall; reboot may be required |
| **Empty host device list** | Final owned-host teardown returns `ESP_ERR_INVALID_STATE`; enumeration idle is unproven |
| **Ownership** | Single owner task; no concurrent API during uninstall |

```c
if (esp_rtl_sdr_uninstall(sdr) == ESP_OK) {
    sdr = NULL;
} /* Otherwise retain sdr for teardown retry; do not restart it. */
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
| `ERR_NO_DEVICE` | No accepted receiver available |
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

Clear `FAULT` → `IDLE` if not streaming. STARTING/STREAMING/STOPPING or outstanding
URBs → `ERR_BUSY`. A timed-out stop must finish draining before reset can succeed.
Clears metrics counters on success; not a hardware endpoint-buffer-reset API.

### `esp_rtl_sdr_release_iq_block`

```c
esp_err_t esp_rtl_sdr_release_iq_block(esp_rtl_sdr_handle_t handle,
                                       const esp_rtl_sdr_iq_block_t *block);
```

| Mode | Behavior |
|---|---|
| Borrow (default) | **No-op** returning `ESP_OK` if `block` non-NULL — safe to call always |
| Acquire (`CAP_IQ_ACQUIRE`) | Not implemented; acquire flag ignored, all current callback payloads are borrowed |

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
| Effect | Stores preference; while streaming queues a retune, otherwise next tune/start |

This is **RF tuning correction**, including the direct-Q tuning path, not
sample-clock calibration. While streaming the setter requests a retune.
Its stored PPM preference is updated before the retune result is known; a
getter is not proof that a failed operation changed the hardware. Acquisition
clock estimates (§21) describe rate uncertainty without correcting the clock.

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

Lifetime/cumulative snapshot from live metrics (stream start). Safe while
streaming. Use `advice[]` for UI / logs. Early pull-ring overflow remains in these
counters for the whole stream - for soak / app windows use
`esp_rtl_sdr_health_from_window()` instead.

```c
esp_rtl_sdr_health_info_t h;
ESP_ERROR_CHECK(esp_rtl_sdr_get_health(sdr, &h));
if (h.overall != ESP_RTL_SDR_HEALTH_OK) {
    ESP_LOGW(TAG, "health: %s", h.advice);
}
```

### `esp_rtl_sdr_metrics_delta` / `esp_rtl_sdr_health_from_window` (0.7.13+)

Pure helpers (no handle mutex). Take two `esp_rtl_sdr_get_metrics()` snapshots plus
`window_ms` and programmed SPS:

```c
esp_rtl_sdr_metrics_t before, after;
esp_rtl_sdr_metrics_window_t win;
esp_rtl_sdr_health_info_t wh;
ESP_ERROR_CHECK(esp_rtl_sdr_get_metrics(sdr, &before));
/* ... measure window_ms ... */
ESP_ERROR_CHECK(esp_rtl_sdr_get_metrics(sdr, &after));
ESP_ERROR_CHECK(esp_rtl_sdr_metrics_delta(&before, &after, window_ms, sps, &win));
ESP_ERROR_CHECK(esp_rtl_sdr_health_from_window(&before, &after, window_ms, sps, &wh));
```

| Field / result | Meaning |
|---|---|
| `delta_*` | Bytes / overruns / consumer_drops / short_transfers in the window |
| `effective_sps` | CU8 scoped rate: `delta_bytes * 500 / window_ms` |
| `efficiency` | `effective_sps / programmed_sps` |
| `usb` / `overall` | Window only: `OK`, `USB_STARVING` (<90% eff), or `APP_TOO_SLOW` (delta drops) |
| `rf` | Left `UNKNOWN` (sample swing is not windowed by these counters) |

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

## 16. Gain & bias (board-specific PC controls)

Clean-room tables from lab USBPcap (`private/measured_gain_bias_v4.hpp`,
`private/gain_r820t2.hpp`, and the V4L capture report). CAP bits on
since **0.7.5** (`MEASURED_2026_08_12`). Needs a claimed interface (`start` succeeded).
The original V4 capture report is dated evidence, not acceptance for every
profile or current release. See [`PROJECT_TRUTH.md`](../PROJECT_TRUTH.md) for
open standalone soak, calibration and loaded bias-current gates.

| Function | Behavior |
|---|---|
| `set_tuner_gain_mode(MANUAL)` | Restore last ladder step (or 0.0 dB); no-op if already MANUAL; unsupported while V3/Nooelec direct sampling bypasses the tuner |
| `set_tuner_gain_mode(AUTO)` | Board-specific `05/07/0c` (`CAP_GAIN_AUTO`): V4 `e8/78/6b`, V4L HF `e3/75/6b`, V4L FM `83/75/6b`, V3c FM `88/78/6b` |
| `get_tuner_gain_mode` | Last **requested** mode — not register readback |
| `set_tuner_gain` | Nearest measured step; forces MANUAL; cancels queued AUTO; unsupported while V3/Nooelec direct sampling bypasses the tuner |
| `get_tuner_gain` | Last requested/accepted step (0 if never set) — software shadow |
| `get_tuner_gains` | V4: 28 nominal steps; V4L/BlogV3: 29 nominal steps including 48.0 dB. These are PC request labels, not calibrated gain. |
| `set_bias_tee` | Explicit manual ON/OFF, `3001=19/18`; OFF default, on stop and new attachment. BlogV3 identity is generic and must be user-warned before enable. |
| `get_bias_tee` | Last requested preference |
| `set/get_rtl_agc` | **0.7.8+** demod `0x19` ON=`0x25` OFF=`0x05` (`CAP_RTL_AGC`); get is shadow |
| `get_tuner_bandwidths` | At 2.4 MS/s, native route: auto/200/300/500/1000/1800/2400 kHz; V4/V4L HF route: auto/200/500/2400 kHz. Size query supported. V3/Nooelec direct-Q HF returns unsupported; E4000 has no public bandwidth control. |
| `set_tuner_bandwidth` | Queues a live filter, PLL, and demod IF change; `ESP_OK` means accepted, not applied. Zero means automatic, not sample rate or audio bandwidth. |
| `get_tuner_bandwidth_state` | Returns requested/applied widths separately; after a failed write, applied remains the prior width if rollback succeeded. Check state/last error; failed rollback enters FAULT without IQ resume. |

While streaming, gain / mode / bias / RTL AGC / tuner bandwidth **setters are async** (delivery task,
one bulk-pause window). `ESP_OK` means the request was queued or applied on the
non-streaming path — **not** that the device ACKed each EP0 byte. A smoke app
can prove API returns, continued IQ, metrics, and health around each transition.
It **cannot** independently prove every register write landed. First-party PC
captures document sequence derivation; readback on the tested receiver and
physical measurements provide separate evidence of programmed and RF state.

```c
if (esp_rtl_sdr_get_device_capabilities(sdr) & ESP_RTL_SDR_CAP_GAIN) {
    ESP_ERROR_CHECK(esp_rtl_sdr_set_tuner_gain_mode(sdr, ESP_RTL_SDR_GAIN_MODE_MANUAL));
    ESP_ERROR_CHECK(esp_rtl_sdr_set_tuner_gain(sdr, 400)); /* nearest nominal ladder label, not measured RF gain */
}
if (esp_rtl_sdr_get_device_capabilities(sdr) & ESP_RTL_SDR_CAP_GAIN_AUTO) {
    ESP_ERROR_CHECK(esp_rtl_sdr_set_tuner_gain_mode(sdr, ESP_RTL_SDR_GAIN_MODE_AUTO));
}
if (esp_rtl_sdr_get_device_capabilities(sdr) & ESP_RTL_SDR_CAP_BIAS_TEE) {
    ESP_ERROR_CHECK(esp_rtl_sdr_set_bias_tee(sdr, true));
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
| `EVT_IQ_BLOCK_EX` | `esp_rtl_sdr_iq_block_ex_t` **borrowed** | Opt-in acquisition data + metadata (§21); emitted alongside legacy IQ event |
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

This uses the owner-storage functions in §1, configured for READ delivery.

```c
esp_err_t started = app_radio_start();
while (started == ESP_OK) {
    uint8_t buf[8192];
    size_t n = 0;
    esp_err_t err = app_radio_read(buf, sizeof(buf), &n);
    if (err == ESP_OK) {
        /* demodulate exactly n bytes; an app stop request should also exit */
    } else if (err != ESP_RTL_SDR_ERR_TIMEOUT) {
        break;
    }
}
esp_err_t cleanup = app_radio_close();
/* On failure, the static owner pointer is preserved. Schedule another close
 * attempt; do not call start/read through the cleanup-only handle. */
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
    /* No accepted receiver is available; show a reconnect/discovery action. */
} else if (err != ESP_OK) {
    ESP_LOGE(TAG, "%s state=%s", esp_rtl_sdr_err_to_name(err),
             esp_rtl_sdr_state_to_name(esp_rtl_sdr_get_state(sdr)));
}
```

---

## 19. Symbol index

Every declared public function and named struct/enum in both headers is listed
here. Sections provide behavior and the headers provide exact declarations.

| Symbol | Section |
|---|---|
| `esp_rtl_sdr_apply_need` | §15 |
| `esp_rtl_sdr_cap_t` | §5 |
| `esp_rtl_sdr_capture_meta_default` | §20 |
| `esp_rtl_sdr_capture_meta_t` | §20 |
| `esp_rtl_sdr_config_default` | §1 |
| `esp_rtl_sdr_config_t` | §1 |
| `esp_rtl_sdr_config_validate` | §9 |
| `esp_rtl_sdr_delivery_mode_t` | §20 |
| `esp_rtl_sdr_delivery_mode_uses_callback_iq` | §7 |
| `esp_rtl_sdr_delivery_mode_uses_read` | §20 |
| `esp_rtl_sdr_device_info_t` | §7 |
| `esp_rtl_sdr_err_to_name` | §2 |
| `esp_rtl_sdr_error_info_t` | §7 |
| `esp_rtl_sdr_event_t` | §7 |
| `esp_rtl_sdr_fill_capture_meta` | §20 |
| `esp_rtl_sdr_format_usb_path` | §20 |
| `esp_rtl_sdr_frequency_uses_hf_upconverter` | §20 |
| `esp_rtl_sdr_gain_mode_t` | §7 |
| `esp_rtl_sdr_get_bias_tee` | §20 |
| `esp_rtl_sdr_get_candidate_identity` | §20 |
| `esp_rtl_sdr_get_capabilities` | §5 |
| `esp_rtl_sdr_get_capture_meta` | §20 |
| `esp_rtl_sdr_get_center_freq` | §13 |
| `esp_rtl_sdr_get_device_at` | §14 |
| `esp_rtl_sdr_get_device_capabilities` | §2 |
| `esp_rtl_sdr_get_device_count` | §14 |
| `esp_rtl_sdr_get_device_info` | §11 |
| `esp_rtl_sdr_get_freq_correction` | §14 |
| `esp_rtl_sdr_get_health` | §15 |
| `esp_rtl_sdr_get_hf_direct_min_hz` | §20 |
| `esp_rtl_sdr_get_hub_stats` | §20 |
| `esp_rtl_sdr_get_identity` | §20 |
| `esp_rtl_sdr_get_last_error` | §2 |
| `esp_rtl_sdr_get_logical_index` | §20 |
| `esp_rtl_sdr_get_metrics` | §11 |
| `esp_rtl_sdr_get_offset_tuning_state` | §20 |
| `esp_rtl_sdr_get_profile` | §20 |
| `esp_rtl_sdr_get_rate_passport` | §15 |
| `esp_rtl_sdr_get_rtl_agc` | §20 |
| `esp_rtl_sdr_get_sample_rate` | §13 |
| `esp_rtl_sdr_get_state` | §11 |
| `esp_rtl_sdr_get_stream_stats` | §20 |
| `esp_rtl_sdr_get_supported_rates` | §9 |
| `esp_rtl_sdr_get_tuner_bandwidth_state` | §20 |
| `esp_rtl_sdr_get_tuner_bandwidths` | §20 |
| `esp_rtl_sdr_get_tuner_gain` | §20 |
| `esp_rtl_sdr_get_tuner_gain_mode` | §20 |
| `esp_rtl_sdr_get_tuner_gains` | §20 |
| `esp_rtl_sdr_get_version` | §8 |
| `esp_rtl_sdr_get_version_string` | §8 |
| `esp_rtl_sdr_health_from_window` | §15 |
| `esp_rtl_sdr_health_info_t` | §7 |
| `esp_rtl_sdr_health_t` | §7 |
| `esp_rtl_sdr_hub_port_power_cycle` | §20 |
| `esp_rtl_sdr_hub_stats_default` | §20 |
| `esp_rtl_sdr_hub_stats_t` | §20 |
| `esp_rtl_sdr_identity_default` | §20 |
| `esp_rtl_sdr_identity_t` | §20 |
| `esp_rtl_sdr_install` | §1 |
| `esp_rtl_sdr_iq_block_t` | §7 |
| `esp_rtl_sdr_is_rate_supported` | §9 |
| `esp_rtl_sdr_metrics_delta` | §15 |
| `esp_rtl_sdr_metrics_t` | §7 |
| `esp_rtl_sdr_metrics_window_t` | §15 |
| `esp_rtl_sdr_need_t` | §7 |
| `esp_rtl_sdr_normalize_frequency` | §9 |
| `esp_rtl_sdr_offset_tuning_state_t` | §20 |
| `esp_rtl_sdr_passport_entry_t` | §7 |
| `esp_rtl_sdr_passport_opts_default` | §15 |
| `esp_rtl_sdr_passport_opts_t` | §7 |
| `esp_rtl_sdr_preset_frequency_hz` | §9 |
| `esp_rtl_sdr_preset_t` | §7 |
| `esp_rtl_sdr_probe_rates` | §15 |
| `esp_rtl_sdr_profile_t` | §20 |
| `esp_rtl_sdr_profile_to_name` | §20 |
| `esp_rtl_sdr_quantize_sample_rate` | §9 |
| `esp_rtl_sdr_rate_passport_t` | §7 |
| `esp_rtl_sdr_read` | §1 |
| `esp_rtl_sdr_refresh_device_list` | §14 |
| `esp_rtl_sdr_release_iq_block` | §12 |
| `esp_rtl_sdr_reset` | §12 |
| `esp_rtl_sdr_retune_hz` | §12 |
| `esp_rtl_sdr_select_device` | §14 |
| `esp_rtl_sdr_select_device_serial` | §14 |
| `esp_rtl_sdr_set_bias_tee` | §16 |
| `esp_rtl_sdr_set_center_freq` | §12 |
| `esp_rtl_sdr_set_freq_correction` | §14 |
| `esp_rtl_sdr_set_hf_direct_min_hz` | §20 |
| `esp_rtl_sdr_set_offset_tuning` | §20 |
| `esp_rtl_sdr_set_rtl_agc` | §20 |
| `esp_rtl_sdr_set_sample_rate` | §12 |
| `esp_rtl_sdr_set_tuner_bandwidth` | §20 |
| `esp_rtl_sdr_set_tuner_gain` | §16 |
| `esp_rtl_sdr_set_tuner_gain_mode` | §16 |
| `esp_rtl_sdr_start` | §1 |
| `esp_rtl_sdr_start_hz` | §1 |
| `esp_rtl_sdr_state_t` | §7 |
| `esp_rtl_sdr_state_to_name` | §7 |
| `esp_rtl_sdr_stop` | §12 |
| `esp_rtl_sdr_stream_config_default` | §9 |
| `esp_rtl_sdr_stream_config_t` | §7 |
| `esp_rtl_sdr_stream_config_validate` | §9 |
| `esp_rtl_sdr_stream_stats_default` | §20 |
| `esp_rtl_sdr_stream_stats_t` | §20 |
| `esp_rtl_sdr_tuner_frequency_hz` | §20 |
| `esp_rtl_sdr_uninstall` | §1 |
| `esp_rtl_sdr_usb_device_count` | §20 |
| `esp_rtl_sdr_usb_fault_guard_reset` | §20 |
| `esp_rtl_sdr_usb_safe_mode_active` | §20 |
| `esp_rtl_sdr_acquisition_config_default` | §21 |
| `esp_rtl_sdr_acquisition_config_t` | §21 |
| `esp_rtl_sdr_configure_acquisition` | §21 |
| `esp_rtl_sdr_iq_block_ex_t` | §21 |
| `esp_rtl_sdr_iq_quality_t` | §21 |
| `esp_rtl_sdr_read_ex` | §21 |
| `esp_rtl_sdr_sample_meta_default` | §21 |
| `esp_rtl_sdr_sample_meta_t` | §21 |

---

## 20. Profile, routing, identity and recovery

The declarations and field layouts in [the public header](../include/esp_rtl_sdr.h)
are authoritative. Unless noted otherwise, fallible pointer-output handle queries
require a live handle and non-NULL output pointers, and can return `ESP_ERR_INVALID_ARG`,
`ESP_RTL_SDR_ERR_STALE_HANDLE` or `ESP_RTL_SDR_ERR_TIMEOUT`. Call them from an
application task, serialize lifecycle ownership, and never overlap uninstall.
These errors do not make calling through freed pointers safe.
Value-returning profile/capability queries instead use UNKNOWN/zero sentinels.

### Profile and routing

| API / type | Inputs, output and limits |
|---|---|
| `esp_rtl_sdr_profile_t` | UNKNOWN, BLOG_V4, BLOG_V3, NOOELEC_SMART_V5, BLOG_V4L, E4000. A profile identifies implemented behavior, not a hardware qualification level. |
| `esp_rtl_sdr_get_profile(handle)` | Returns the active profile, or UNKNOWN for an invalid/unavailable handle. |
| `esp_rtl_sdr_profile_to_name(profile)` | Static diagnostic name for the enum; no allocation or hardware access. |
| `esp_rtl_sdr_get_device_capabilities(handle)` | Active-device bitmask; zero for unknown/unavailable device. Use this for device controls, rather than the binary-wide mask. Route/rate restrictions still apply. |
| `esp_rtl_sdr_frequency_uses_hf_upconverter(rf_hz)` | Pure legacy V4 arithmetic helper: tests RF below 28.8 MHz. Does not inspect a handle or prove another profile uses an upconverter. |
| `esp_rtl_sdr_tuner_frequency_hz(rf_hz)` | Pure legacy V4 RF-to-tuner mapping; adds 28.8 MHz below the boundary. Do not use it to infer the active route of another receiver. |
| `esp_rtl_sdr_delivery_mode_t` | BOTH feeds callback and read channels; CALLBACK omits the pull ring; READ omits IQ callbacks but retains the worker for controls/recovery. BOTH consumers observe separate deliveries of the same source data. |
| `esp_rtl_sdr_delivery_mode_uses_read(mode)` | Pure predicate for READ/BOTH; complements `esp_rtl_sdr_delivery_mode_uses_callback_iq(mode)`. |

Check controls after the receiver has been discovered/opened:

```c
uint32_t caps = esp_rtl_sdr_get_device_capabilities(sdr);
if (caps & ESP_RTL_SDR_CAP_GAIN) {
    /* Offer nominal gain controls; handle setter errors and async completion. */
}
```

### Board controls and their state

| API | Contract |
|---|---|
| `esp_rtl_sdr_get_tuner_gain_mode(handle, out_mode)` | Last requested AUTO/MANUAL mode, not a hardware readback. |
| `esp_rtl_sdr_get_tuner_gain(handle, out_gain_tenth_db)` | Last requested/accepted nominal ladder step; zero before a value has been set. Not calibrated RF gain. |
| `esp_rtl_sdr_get_tuner_gains(handle, out_gains_tenth_db, max_count, out_count)` | Query profile ladder; `max_count=0` is a size query. Allocate an int array for values in tenths of dB. Check the result/count before using it. |
| `esp_rtl_sdr_set_rtl_agc(handle, enable)` / `esp_rtl_sdr_get_rtl_agc(handle, out_enable)` | RTL2832 digital AGC, distinct from tuner AUTO. Live setter queues work; getter reports requested shadow. Current device masks include all five profiles. |
| `esp_rtl_sdr_get_bias_tee(handle, out_enable)` | Requested bias shadow on capable boards; not a voltage/current measurement. Generic V3 identity does not prove a physical bias circuit. |
| `esp_rtl_sdr_get_tuner_bandwidths(handle, out_hz, max_count, out_count)` | Size query with `max_count=0`. Supported nominal choices depend on profile/RF route at 2.4 MS/s; zero means automatic. V3/Nooelec direct-Q routes and E4000 return unsupported. |
| `esp_rtl_sdr_set_tuner_bandwidth(handle, hz)` | Requires streaming/claimed interface and a supported choice at 2.4 MS/s. Queues hardware filter/PLL/IF writes; `ESP_OK` means accepted. Unsupported choice/route/rate returns UNSUPPORTED; no stream returns NOT_STREAMING. |
| `esp_rtl_sdr_get_tuner_bandwidth_state(handle, requested_hz, applied_hz)` | Both outputs required. Distinguishes request from last successfully programmed nominal width; UNSUPPORTED if capability/applied state unavailable. Rollback preserves prior applied width; failed rollback enters FAULT. These values do not measure analog passbands. |
| `esp_rtl_sdr_set_hf_direct_min_hz(handle, min_hz)` / `esp_rtl_sdr_get_hf_direct_min_hz(handle, out_min_hz)` | V4/V4L tuner-input route override below 28.8 MHz. Nonzero minimum clamps to at least 24 MHz; zero restores the upconverter. Applied on next start/retune and cleared on attachment. Nonzero on other profiles returns UNSUPPORTED. This is not ADC I/Q direct sampling. |
| `esp_rtl_sdr_set_offset_tuning(handle, enabled)` | Experimental E4000 only; configure in IDLE, otherwise BUSY (unsupported profiles return UNSUPPORTED). Default off; attachment clears request. Applies on start/retune. |
| `esp_rtl_sdr_get_offset_tuning_state(handle, out)` | Requires an `esp_rtl_sdr_offset_tuning_state_t` output. Reports request, applied RF-minus-LO offset, physical programmed LO (including PPM) and requested IQ center. Applied fields are zero unless streaming. |

E4000 offset tuning moves the LO below RF and compensates inside the RTL2832U;
do **not** apply another software mixer. Near the lower supported RF limit the
offset is suspended (`applied_offset_hz=0`) to keep the LO in range. The state
type's `requested` flag can therefore be true with no applied offset.

Live gain, mode, bias, digital AGC and bandwidth requests are coalesced and
processed by the delivery task after draining bulk transfers. `ESP_OK` does
not promise that every intermediate request was programmed. Gain/mode/bias/AGC
getters are shadows; check state/last error for failures. There is no general
operation-ID/completion API. A retune has its separate `EVT_RETUNED` event.

### Receiver identity and statistics

| API | Inputs and result |
|---|---|
| `esp_rtl_sdr_identity_default(out)` | Initializes `esp_rtl_sdr_identity_t` and its size; fields describe USB address/path, logical index, enum index, VID/PID, strings, speed, presence and profile. |
| `esp_rtl_sdr_get_identity(handle, out)` | Snapshot of opened device; absent means `present=false`, address zero. Output unchanged on validation/lock failure. USB addresses/paths can change on re-enumeration; identical product strings are not unique identities. |
| `esp_rtl_sdr_get_candidate_identity(handle, index, out)` | Candidate snapshot without opening/selecting it. Index must be in the last candidate list; invalid index returns BAD_DEVICE. |
| `esp_rtl_sdr_get_logical_index(handle, out_index)` | Handle log slot, not USB address or persistent device ID; invalid handle reports STALE_HANDLE without changing the output. Initialize it yourself if a sentinel is needed. |
| `esp_rtl_sdr_format_usb_path(parent_addr, hub_port, dst, dst_sz)` | Bounded formatter for `root` or `P<parent>.p<port>`. Caller owns destination. |
| `esp_rtl_sdr_capture_meta_default(out)` | Initializes `esp_rtl_sdr_capture_meta_t`; fields cover identity, sequence/time, RF/tuner/rate, gain/PPM, bias/state and legacy loss counters. Reserved RMS/peak fields stay zero. |
| `esp_rtl_sdr_get_capture_meta(handle, out)` | Live handle snapshot, **not** metadata for queued bytes; sample count is zero. Use §21 when associating frequency/epoch with samples. |
| `esp_rtl_sdr_fill_capture_meta(out, ...)` | Pure helper: caller supplies all identity/time/configuration/counter fields in header declaration order; fills the capture struct without USB or allocation. It cannot verify those inputs or measure RF. |
| `esp_rtl_sdr_stream_stats_default(out)` / `esp_rtl_sdr_get_stream_stats(handle, out)` | Initialize/fill `esp_rtl_sdr_stream_stats_t`: received/consumed bytes, pairs, transfer/error counts, separately unit-labelled drop counts, queue/ring high-water marks, uptime and observed delivery rate. `usb_timeouts` remains zero with the present IDF host API. `dropped_buffers` is deprecated mixed-unit accounting; prefer slot-starve blocks and ring-overrun bytes/blocks. |
| `esp_rtl_sdr_hub_stats_default(out)` / `esp_rtl_sdr_get_hub_stats(handle, out)` | Initialize/fill `esp_rtl_sdr_hub_stats_t`: shared host state/refcount, claims, device events/conflicts and compiled hub support. NULL handle is allowed: reports the process-wide session. |

These snapshots are diagnostics, not synchronized RF timestamps or proof of
phase coherence. `effective_sample_rate` is observed delivery throughput, not
physical sample-clock calibration. Struct layouts and units are listed in the
[public header](../include/esp_rtl_sdr.h); initialize output structs with their
matching default helpers rather than inventing size values.

### USB discovery and recovery

| API | Inputs, errors and recovery boundary |
|---|---|
| `esp_rtl_sdr_usb_device_count(out_count)` | No handle. Raw USB host device-list count, including hubs/non-RTL devices; unlike accepted-candidate `get_device_count`. Requires a valid output and propagates host errors. Does not prove enumeration completed or RF availability. |
| `esp_rtl_sdr_usb_safe_mode_active()` | Whether this boot refused USB-host installation due to the enumeration panic guard. Retained guard state survives resets/panics, cleared by power loss. |
| `esp_rtl_sdr_usb_fault_guard_reset()` | Clears retained panic count/latch so a later install can try again; does not change an existing handle. Safe no-op if not latched. Does not fix the underlying crash. |
| `esp_rtl_sdr_hub_port_power_cycle(handle, off_ms)` | Disruptive recovery for downstream ports of external hubs, not one selected receiver. Requires a live USB client (otherwise NOT_READY); zero selects 300 ms, 100–1000 ms is suggested. NOT_FOUND means no hub could be opened. ESP_OK means a hub was processed, not proof power physically dropped or devices recovered. Hub switching hardware may ignore requests. |

Stop affected receivers and coordinate all shared-host users before a hub
power cycle. Never run it in an IQ callback. Expose recovery as an explicit
application action, then observe rediscovery; do not loop power cycles on an
empty candidate count. A stop/uninstall timeout does not prove USB ownership
ended. Keep a failed-uninstall pointer for cleanup retries, and clear it only
on successful destruction (§10).

## 21. Acquisition metadata

Include [`esp_rtl_sdr_acquisition.h`](../include/esp_rtl_sdr_acquisition.h)
in addition to the base header. The optional API associates copied/borrowed
sample spans with their original USB observation rather than live handle state.
Detailed behavior and evidence: [acquisition API design](acquisition-reliability/API_DESIGN.md)
and [performance limits](acquisition-reliability/PERFORMANCE_REPORT.md).

| API | Parameters, ownership and results |
|---|---|
| `esp_rtl_sdr_acquisition_config_default(out)` | Zero/default an `esp_rtl_sdr_acquisition_config_t`, including exact `struct_size`; disabled and quality off by default. Caller owns storage. |
| `esp_rtl_sdr_sample_meta_default(out)` | Initialize an `esp_rtl_sdr_sample_meta_t`, including exact `struct_size`, before `read_ex`. Caller owns storage. |
| `esp_rtl_sdr_configure_acquisition(handle, config)` | Configure in IDLE before first start; one owner task, not callbacks. Config pointer required; size must match exactly, quality stride 0–256, clock error within ±1000000 ppb. INVALID_ARG for invalid config; STALE_HANDLE for invalid handle; INVALID_STATE once started/configuration locked or storage already allocated. No allocation here: storage is reserved at start after USB buffers. |
| `esp_rtl_sdr_read_ex(handle, out_buf, max_bytes, timeout_ms, out_bytes, metadata)` | Requires enabled acquisition in READ/BOTH mode, non-NULL outputs, exact metadata size, capacity ≥2. Odd capacity rounds down to CU8 pairs. Returns one span at most, possibly shorter than requested; never crosses descriptors/epochs. Zero timeout polls. On error byte count is zero (when pointer valid), metadata unchanged. INVALID_ARG for invalid outputs/size/capacity or timeout above 30000 ms; UNSUPPORTED if acquisition/read delivery unavailable; TIMEOUT if no data/reader lock unavailable; STALE_HANDLE for invalid handle. Buffered spans can drain in IDLE after stop; empty idle returns NOT_STREAMING. |

Use one consumer style per channel; mixing legacy `read` and `read_ex` consumes
the same pull stream. `read_ex` serializes concurrent readers but does not make
concurrent destruction safe. With CALLBACK/BOTH, enabled acquisition adds
`EVT_IQ_BLOCK_EX` alongside the legacy event: consume one family to avoid double
processing. `esp_rtl_sdr_iq_block_ex_t` contains borrowed `data`, byte count and
metadata. Neither payload nor sample pointer may survive callback return.
Acquire mode is currently ignored; no retained-buffer API is enabled.

### Configuration and metadata types

| Type / fields | Meaning |
|---|---|
| `esp_rtl_sdr_acquisition_config_t` | `enabled` opts in. `quality_stride_pairs=0` disables quality, 1 scans every pair, 2–256 sparsely sample. `clock_estimate_valid`, signed `sample_clock_error_ppb` and `clock_uncertainty_ppb` are supplied by the app, never inferred from USB timing. |
| `esp_rtl_sdr_sample_meta_t`: identity/time | `receiver_id` is a boot-local handle instance; `device_id`/`usb_addr` identify the local receiver/bus. `epoch` distinguishes starts/drained interruptions. `host_timestamp_us` is client-task completion observation, **not RF sample time**. |
| Sample positions/loss | `sample_index`/`sample_count` describe returned observed CU8 pairs. `block_sample_index`/`block_sample_count` describe the original USB block. `known_lost_samples` counts observed omissions per delivery channel; `unknown_gap_count` counts interruption markers, not missing samples. Unknown physical losses cannot be reconstructed from these indexes. |
| Configuration/rates | `center_frequency_hz` is zero when a failed control leaves it uncertain. Requested rate, integer programmed rate, stored resampler ratio and ratio-derived `programmed_rate_millisps` are distinct. `frequency_correction_ppm` is RF-only. `estimated_rate_millisps` is zero if unknown; uncertainty zero means unquantified, not perfect accuracy. |
| `esp_rtl_sdr_iq_quality_t` | Original-block CU8 code statistics: inspected pairs, rail components (0/255), means ×1000, power ×4, RMS ×1000, peak component ×2 and stride. Partial reads keep original block quality/timestamp. Values are not calibrated RF power; rails alone do not identify an overloaded RF stage. |

`flags` is a bitmask using the `ESP_RTL_SDR_ACQ_` prefix:

| Flags | Meaning |
|---|---|
| `START`, `RETUNE`, `EPOCH_START` | Stream-start/retune reason and first delivery in a new observation epoch; continuity across epochs is not assumed. |
| `CONTROL`, `CONTROL_FAILED` | Drained control interruption, or failed control with configuration uncertainty. Check the labelled center frequency and driver error/state. |
| `KNOWN_LOSS`, `UNKNOWN_LOSS` | Observed pairs omitted on this channel, versus an interruption whose physical sample loss cannot be counted. Use the corresponding counters, not the bit count. |
| `USB_ERROR`, `SHORT_TRANSFER`, `DISCONNECTED` | USB error, shorter-than-requested observation, or disconnect cause. A short transfer alone is not an exact count of physically lost samples. |
| `CLOCK_ESTIMATE` | The app supplied a sample-clock estimate; not a measured RF timestamp or hardware correction. |
| `QUEUE_STARVE`, `PULL_OVERRUN`, `META_OVERRUN` | Loss due to unavailable delivery slots/queue, pull-ring space, or metadata-descriptor storage. |

Inspect flags and epoch changes before feeding a continuity-sensitive decoder.
Metadata does not guarantee uninterrupted capture, cross-receiver synchronization
or coherence. Odd-length USB observations make I/Q alignment uncertain and fault
acquisition until restart; a 64-bit timeline overflow also fails closed.

### Focused acquisition example

In the owner task, after install but before first start (install with READ or BOTH):

```c
#include "esp_rtl_sdr_acquisition.h"

esp_rtl_sdr_acquisition_config_t acq;
esp_rtl_sdr_acquisition_config_default(&acq);
acq.enabled = true;              /* quality stays off; no invented clock estimate */
esp_err_t err = esp_rtl_sdr_configure_acquisition(sdr, &acq);
/* Continue to start only if err == ESP_OK; preserve the handle on failure. */
```

After successful start, read into application-owned storage:

```c
uint8_t samples[4096];
esp_rtl_sdr_sample_meta_t meta;
esp_rtl_sdr_sample_meta_default(&meta);
size_t bytes = 0;
esp_err_t err = esp_rtl_sdr_read_ex(sdr, samples, sizeof(samples), 100, &bytes, &meta);
if (err == ESP_OK) {
    /* Process exactly bytes; meta labels this span, not the current handle state.
     * Reset continuity-sensitive DSP when epoch changes or loss flags appear. */
} else if (err == ESP_RTL_SDR_ERR_TIMEOUT) {
    /* No span available this time. Yield/retry according to the app's policy. */
} else {
    /* Notify the owner; do not use meta as a successful result. */
}
```

### Cleanup retry example

Keep the handle in owner-managed storage across calls, including failed installs.
Call this only from that owner, with no other task using the handle:

```c
static esp_err_t cleanup_radio(esp_rtl_sdr_handle_t *owned)
{
    if (owned == NULL) return ESP_ERR_INVALID_ARG;
    esp_err_t err = esp_rtl_sdr_uninstall(*owned);
    if (err == ESP_OK) *owned = NULL;
    return err;
}
/* On failure retain *owned and schedule a bounded retry/user recovery action.
 * Do not start another receiver session through this cleanup-only pointer. */
```

## Related documents

| Doc | Role |
|---|---|
| [`API.md`](API.md) | Design contract, threading invariants, versioning policy |
| [`LIBRTLSDR_PARITY_2026-11-09.md`](LIBRTLSDR_PARITY_2026-11-09.md) | Pinned source-level Osmocom comparison; not a hardware benchmark |
| [`../include/esp_rtl_sdr.h`](../include/esp_rtl_sdr.h) | Source of truth for signatures |
| [`../PROJECT_TRUTH.md`](../PROJECT_TRUTH.md) | Hardware / maturity claims |
| [`RATES.md`](RATES.md) | Rate windows deep dive |
| [`../architecture.md`](../architecture.md) | Layering & USB path |
| [`../examples/p4_serial_smoke/`](../examples/p4_serial_smoke/) | Runnable smoke |

---

*If this reference disagrees with the header, the header wins for signatures; if it oversells hardware, PROJECT_TRUTH wins — open a `truth:` issue.*

## Pull-ring allocation policy

Startup allocates the required IDF USB transfer pool before CPU I/Q queues and
the optional pull ring. Automatic pull capacity remains four times the URB
payload, bounded to 192000…524288 bytes (default 6×16384 → 393216 bytes).
PSRAM is preferred. Internal fallback budgets the largest available internal
block with a 32 KiB reserve, then retries smaller even sizes down to 64 KiB.
The reserve covers the 6 KiB delivery stack plus synchronization/application
headroom; it is a heuristic, not a throughput or concurrent-allocation guarantee.
The driver logs shrinkage and approximate buffering duration. Explicit sizes
are tried unchanged and return `ESP_ERR_NO_MEM` on failure. CALLBACK does not
allocate a pull ring; READ and BOTH do. USB DMA buffers still use IDF's allocator
and capability/cache rules; this change does not move DMA memory into PSRAM.
