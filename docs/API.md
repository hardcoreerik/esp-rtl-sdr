# esp_rtl_sdr public API — design contract

**Header:** [`include/esp_rtl_sdr.h`](../include/esp_rtl_sdr.h)  
**Version:** 0.7.8  
**Full parameter / return / example reference:** [`API_REFERENCE.md`](API_REFERENCE.md)

This document is the **design contract** (invariants, threading, ABI growth).  
For every function’s parameters, return semantics, and copy-paste examples, use
**[`API_REFERENCE.md`](API_REFERENCE.md)** — that is the detailed reference.

---

## Design goals

| Goal | Mechanism |
|---|---|
| Works every time | Strict validation; no half-open USB; fail closed to IDLE/FAULT |
| Safe under concurrency | Per-handle mutex; RAII locks; short timeouts on queries |
| No callback re-entry | `in_callback_depth` → `ERR_REENTRANT` on lifecycle APIs |
| Events outside lock | Callbacks run only after mutex release |
| Stable ABI growth | `struct_size` on config structs; new fields only at end |
| Clear failures | Component error codes + `err_to_name` + `get_last_error` |
| Feature discovery | `get_capabilities`, rate allowlist, `get_supported_rates` |
| No silent over-claim | Capability bits are enabled only for implemented paths |
| Idempotent teardown | `stop` when idle; `uninstall(NULL)`; skip events during destroy |

---

## Lifecycle

```text
install → IDLE
start   → STREAMING   (or error, stay IDLE / FAULT — never half-open)
retune  → STREAMING   (queued; never EP0 while bulk outstanding)
stop    → IDLE        (idempotent; emits EVT_STOPPED once when leaving stream)
uninstall → destroyed (NULL-safe; always frees; second use → STALE_HANDLE)
reset   → IDLE from FAULT if not streaming (clears metrics)
```

### Idempotence and safety

| Call | Behavior |
|---|---|
| `stop` when IDLE | `ESP_OK` |
| `stop(NULL)` | `ESP_OK` |
| `uninstall(NULL)` | `ESP_OK` |
| `uninstall` twice | second → `STALE_HANDLE` |
| `start` while STREAMING | `ERR_BUSY` |
| `start` / `stop` / `reset` / `probe_rates` from event callback | `ERR_REENTRANT` |
| `retune_hz` / `set_center_freq` from event callback | **Allowed** — queues async apply (0.7.3+); later `EVT_RETUNED` |
| `get_*` with NULL/stale handle | safe; no crash |
| failed `start` | handle remains **IDLE**; no interface claim |

---

## Threading

1. Public API is serialized per handle (mutex).
2. Event callbacks must not call `install` / `uninstall` / `start` / `stop` / `reset` / `probe_rates` on the same handle (`ERR_REENTRANT`).
3. Callbacks **may** call `get_state`, `get_metrics`, `get_device_info`, `get_last_error`, `release_iq_block`, and **`retune_hz` / `set_center_freq`** (async queue).
4. IQ payload pointers are **borrowed** until the callback returns (unless acquire mode + `release_iq_block` when CAP_IQ_ACQUIRE is set).
5. Never invoke API from a USB completion ISR.
6. Do not call other APIs concurrent with `uninstall` on the same handle (single owner task).

---

## Validation rules

### Config (`config_validate`)

- `struct_size == sizeof(config)`
- `transfer_bytes` in [512, 262144] and **multiple of 512**
- `transfer_count` in [2, 8]
- `control_timeout_ms` in (0, 30000]
- `usb_task_core_id` in {0, 1, 0xFF}

### Stream (`stream_config_validate`)

- `struct_size` match
- `sample_rate_sps` 0 (fill preferred) or any in-window rate (see `docs/RATES.md`)
- `CUSTOM_HZ`: frequency in [24 MHz, 1766 MHz], quantized to 1 kHz
- Named presets ignore `frequency_hz` (driver LO constants)
- `max_bytes` even (IQ pairs) when non-zero
- `timeout_ms` ≤ 30000

### Helpers

| Function | Purpose |
|---|---|
| `config_default` / `stream_config_default` | Zero + safe defaults + `struct_size` |
| `is_rate_supported` | Single-rate allowlist check |
| `get_supported_rates` | Copy or size-query the allowlist |
| `normalize_frequency` | Clamp/quantize policy |
| `preset_frequency_hz` | Named preset LO (not CUSTOM) |
| `state_to_name` / `err_to_name` | Logging without sprintf tables in apps |
| `release_iq_block` | No-op in borrow mode (safe unconditional call) |

---

## Errors

Prefer component codes over generic `INVALID_STATE` when the app can branch:

| Code | Meaning |
|---|---|
| `NO_DEVICE` | No Blog V4 attached |
| `NOT_V4` / `UNSUPPORTED_DEVICE` | USB device present but identity mismatch |
| `BAD_DEVICE` | Device index / serial selection invalid |
| `BUSY` | Already streaming / stop in progress / concurrent uninstall |
| `NOT_STREAMING` | retune without stream |
| `BAD_RATE` / `BAD_FREQ` | Policy reject |
| `USB` / `TIMEOUT` / `FAULT` | Hardware path |
| `UNSUPPORTED` | Feature not built yet |
| `STALE_HANDLE` | Use after uninstall / bad pointer |
| `REENTRANT` | Lifecycle API called from event callback |
| `NOT_CLAIMED` | Device present but not claimed (future) |
| `NOT_READY` | Transient not-ready (USB client not ready) |

---

## Capabilities (0.7.8 binary)

| Flag | Status |
|---|---|
| `STREAM` | On |
| `RETUNE` | On; bulk drains before EP0 apply |
| `HOTPLUG` | On; recovery soak pending |
| `METRICS` | On |
| `CUSTOM_HZ` | On |
| `FREQ_CORRECTION` | On |
| `MULTI_DEVICE` | On |
| `SYNC_READ` | On |
| `CONTINUOUS_RATE` | On; windows + quantize |
| `NEED` | On; `apply_need` |
| `HEALTH` | On; `get_health` |
| `PASSPORT` | On; `probe_rates` |
| `DELIVERY_MODE` | On; `config.delivery_mode` BOTH/CALLBACK/READ |
| `GAIN` | On (measured Blog V4 manual ladder) |
| `GAIN_AUTO` | On (measured Tuner AGC AUTO, 0.7.8) |
| `RTL_AGC` | On (measured demod 0x19, 0.7.8; additive) |
| `HF_UPCONVERTER` | On (0.7.7) |
| `BIAS_TEE` | On (measured SYS sequence; no multimeter DC yet) |
| `IQ_ACQUIRE` | Off |
| `DIRECT_SAMPLING` | Reserved off |

---

## Phase 1 / 2 / 2.1 ergonomics

| Function | Behavior |
|---|---|
| `set/get_center_freq` | Preferred LO when idle; retune when streaming (async if called from callback) |
| `retune_hz` | Hot LO change; **async from callback** → later `EVT_RETUNED` |
| `set/get_sample_rate` | Quantize to exact; **BUSY** if streaming |
| `quantize_sample_rate` | Pure helper; no handle |
| `read` | Blocking CU8 IQ |
| `start_hz` | Convenience start (0 = preferred) |
| `set/get_freq_correction` | ±200 ppm software LO |
| Gain / AUTO / RTL AGC / bias **set** | Claimed stream; **async** while streaming (bulk pause + EP0 on delivery task). `ESP_OK` = accepted |
| Gain / AUTO / RTL AGC / bias **get** | Last **requested** shadow — **not** EP0/I2C readback |
| Multi-device APIs | refresh / count / at / select |
| `apply_need` | Mission presets → preferred LO/rate |
| `get_health` | USB/RF categories + advice |
| `probe_rates` / `get_rate_passport` | On-device rate matrix |

Apps must:

```c
if (esp_rtl_sdr_get_capabilities() & ESP_RTL_SDR_CAP_STREAM) {
    /* start guaranteed to attempt USB */
} else {
    /* feature is unavailable in this build */
}
```

---

## Recommended app pattern

```c
esp_rtl_sdr_config_t cfg;
esp_rtl_sdr_config_default(&cfg);
cfg.event_cb = on_evt;
ESP_ERROR_CHECK(esp_rtl_sdr_config_validate(&cfg));

esp_rtl_sdr_handle_t sdr = NULL;
esp_err_t err = esp_rtl_sdr_install(&cfg, &sdr);
if (err != ESP_OK) { /* sdr is NULL */ }

esp_rtl_sdr_stream_config_t st;
esp_rtl_sdr_stream_config_default(&st);
st.preset = ESP_RTL_SDR_PRESET_CUSTOM_HZ;
st.frequency_hz = 100100000;
err = esp_rtl_sdr_start(sdr, &st);
/* handle hardware and validation errors; check CAP_STREAM first */

/* teardown always */
(void)esp_rtl_sdr_stop(sdr, 0); /* 0 = default timeout */
(void)esp_rtl_sdr_uninstall(sdr);
sdr = NULL;
```

### Event callback rules (copy-paste)

```c
static void on_evt(esp_rtl_sdr_event_t ev, const void *payload, void *ctx)
{
    switch (ev) {
    case ESP_RTL_SDR_EVT_IQ_BLOCK: {
        const esp_rtl_sdr_iq_block_t *iq = payload;
        /* copy or process quickly; pointer invalid after return */
        (void)esp_rtl_sdr_release_iq_block((esp_rtl_sdr_handle_t)ctx, iq);
        break;
    }
    case ESP_RTL_SDR_EVT_ERROR: {
        const esp_rtl_sdr_error_info_t *e = payload;
        ESP_LOGE("app", "%s", e ? e->message : "?");
        break;
    }
    default:
        break;
    }
    /* NEVER call start/stop/uninstall here on this handle */
}
```

---

## Streaming invariants

Preserve these implemented invariants:

- [x] No EP0 while bulk is outstanding
- [x] Start failure has a cleanup path
- [x] Stop releases the interface best effort
- [x] Metrics use bounded synchronization
- [x] `EVT_IQ_BLOCK` does not hold the API mutex across the app callback
- [x] Retune drains bulk before the USB control sequence
- [x] `CAP_STREAM | CAP_RETUNE | CAP_FREQ_CORRECTION | CAP_MULTI_DEVICE | CAP_SYNC_READ` match working code paths
- [x] Reentrancy guard rejects lifecycle calls from callbacks

Hardware soak and recovery acceptance remain tracked in `PROJECT_TRUTH.md`.

---

## Versioning policy

- **Patch:** bugfix, no API change  
- **Minor:** new fields at end of structs (requires `struct_size`), new functions  
- **Major:** break ABI or semantics  

Bump `ESP_RTL_SDR_VERSION_*`, `ESP_RTL_SDR_VERSION_STRING` (via macros), and `idf_component.yml` together.

Packed runtime version: `(major << 16) | (minor << 8) | patch` from `get_version()`.
