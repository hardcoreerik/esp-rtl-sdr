# esp_rtl_sdr public API — design contract

**Header:** [`include/esp_rtl_sdr.h`](../include/esp_rtl_sdr.h)  
**Version:** 0.9.6
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
| Concurrent receivers | Shared USB session + exclusive USB-address claims; `BIND_ANY` or index/serial |
| No callback re-entry | `ERR_REENTRANT` only if **this task** is inside the event callback. App tasks may set gain/mode while delivery emits. |
| Events outside lock | Callbacks run only after mutex release |
| Stable ABI growth | `struct_size` on config structs; new fields only at end |
| Clear failures | Component error codes + `err_to_name` + `get_last_error` |
| Feature discovery | Binary `get_capabilities`; attached-device `get_device_capabilities`; rate windows and recommended rates |
| No silent over-claim | Capability bits are enabled only for implemented paths |
| Idempotent teardown | `stop` when idle; `uninstall(NULL)`; skip events during destroy |

---

## Lifecycle

```text
install → IDLE
start   → STREAMING   (or error, stay IDLE / FAULT — never half-open)
retune  → STREAMING   (queued; never EP0 while bulk outstanding)
stop    → IDLE        (idempotent; emits EVT_STOPPED once when leaving stream)
uninstall → destroyed on ESP_OK; failure retains cleanup-only handle for retry
reset   → IDLE from FAULT if not streaming (clears metrics)
```

### Idempotence and safety

| Call | Behavior |
|---|---|
| `stop` when IDLE | `ESP_OK` |
| `stop(NULL)` | `ESP_OK` |
| `uninstall(NULL)` | `ESP_OK` |
| `uninstall` after success | Pointer is invalid; never reuse it |
| `start` while STREAMING | `ERR_BUSY` |
| `start` / `stop` / `reset` / `probe_rates` from event callback | `ERR_REENTRANT` |
| `retune_hz` / `set_center_freq` from event callback | **Allowed** — queues async apply (0.7.3+); later `EVT_RETUNED` |
| stale handle | Invalid after successful uninstall; do not call through a freed pointer. Validation is not a lifetime guarantee. |
| failed `start` | Inspect state/error; IDLE or FAULT depending on cleanup. Retain resources until stop/uninstall succeeds. |

---

## Threading

1. Use one owner task for lifecycle operations. Query/control paths use per-handle synchronization; this is not a guarantee that arbitrary operations can overlap safely.
2. Event callbacks must not call `install` / `uninstall` / `start` / `stop` / `reset` / `probe_rates` on the same handle (`ERR_REENTRANT`).
3. Callbacks **may** call `get_state`, `get_metrics`, `get_device_info`, `get_last_error`, `release_iq_block`, and **`retune_hz` / `set_center_freq`** (async queue).
4. IQ payload pointers are **borrowed** until the callback returns. Acquire mode is not implemented: its flag is ignored, CAP_IQ_ACQUIRE is off, and `release_iq_block` is a no-op.
5. Never invoke API from a USB completion ISR.
6. Do not call other APIs concurrent with `uninstall` on the same handle (single owner task).
7. Use one reader for legacy `read()`. Acquisition `read_ex()` serializes readers, but does not make concurrent uninstall safe.

---

## Validation rules

### Config (`config_validate`)

- Use `config_default()` for current size; validation accepts the legacy minimum through current `sizeof`, defaulting omitted trailing fields
- `transfer_bytes` in [512, 262144] and **multiple of 512**
- `transfer_count` in [2, 8]
- `control_timeout_ms` in (0, 30000]
- `usb_task_core_id` in {0, 1, 0xFF}

### Stream (`stream_config_validate`)

- Use `stream_config_default()`; validation accepts the legacy minimum through current `sizeof`
- `sample_rate_sps` 0 (fill preferred) or any in-window rate (see `docs/RATES.md`)
- `CUSTOM_HZ`: exact-Hz frequency in the 24 kHz…1766 MHz arithmetic range;
  the attached profile can reject a narrower unsupported range
- Named presets ignore `frequency_hz` (driver LO constants)
- `max_bytes` even (IQ pairs) when non-zero
- `timeout_ms` ≤ 30000

### Helpers

| Function | Purpose |
|---|---|
| `config_default` / `stream_config_default` | Zero + safe defaults + `struct_size` |
| `is_rate_supported` | Validate continuous hardware rate windows |
| `get_supported_rates` | Copy or size-query recommended rates; not an exhaustive list |
| `normalize_frequency` | Clamp/quantize policy |
| `preset_frequency_hz` | Named preset LO (not CUSTOM) |
| `state_to_name` / `err_to_name` | Logging without sprintf tables in apps |
| `release_iq_block` | No-op in borrow mode (safe unconditional call) |

---

## Errors

Prefer component codes over generic `INVALID_STATE` when the app can branch:

| Code | Meaning |
|---|---|
| `NO_DEVICE` | No accepted receiver available |
| `NOT_V4` / `UNSUPPORTED_DEVICE` | USB device present but identity mismatch |
| `BAD_DEVICE` | Device index / serial selection invalid |
| `BUSY` | Already streaming / stop in progress / concurrent uninstall |
| `NOT_STREAMING` | retune without stream |
| `BAD_RATE` / `BAD_FREQ` | Policy reject |
| `USB` / `TIMEOUT` / `FAULT` | Hardware path |
| `UNSUPPORTED` | Unavailable feature, profile, RF route or operating configuration |
| `STALE_HANDLE` | Handle validation failed; not permission to reuse a freed pointer |
| `REENTRANT` | Lifecycle API called from event callback |
| `NOT_CLAIMED` | Device present but not claimed (future) |
| `NOT_READY` | Transient not-ready (USB client not ready) |

---

## Capabilities

`get_capabilities()` describes implemented paths in the binary, not the attached
receiver. After discovery/open, use `get_device_capabilities(handle)` for controls.
Unknown/detached devices report no device capabilities. A bit does not establish
physical calibration or acceptance at every frequency/rate; consult
[`PROJECT_TRUTH.md`](../PROJECT_TRUTH.md) and the dated evidence.

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
| `HF_UPCONVERTER` | V4/V4L model-specific route; not ADC direct sampling |
| `BIAS_TEE` | Profile-specific; a generic V3 identity does not prove a bias circuit |
| `IQ_ACQUIRE` | Off |
| `DIRECT_SAMPLING` | V3/V3c and Nooelec V5 automatic Q branch below 24 MHz |
| `TUNER_BANDWIDTH` | V4/V4L/V3/Nooelec V5, at 2.4 MS/s on supported tuner routes |
| `OFFSET_TUNING` | Experimental E4000 only; configure in IDLE |

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
| `set/get_freq_correction` | ±200 ppm RF tuning correction; not sample-clock calibration |
| Gain / AUTO / RTL AGC / bias **set** | Claimed stream; **async** while streaming (bulk pause + EP0 on delivery task). `ESP_OK` = accepted |
| Gain / AUTO / RTL AGC / bias **get** | Last **requested** shadow — **not** EP0/I2C readback |
| Multi-device APIs | refresh / count / at / select |
| `apply_need` | Mission presets → preferred LO/rate |
| `get_health` | USB/RF categories + advice |
| `probe_rates` / `get_rate_passport` | On-device rate matrix |

Apps must:

```c
if (esp_rtl_sdr_get_device_capabilities(sdr) & ESP_RTL_SDR_CAP_STREAM) {
    /* an attached profile implements streaming; start can still fail */
} else {
    /* no accepted receiver is open, or this profile lacks the feature */
}
```

---

## Recommended app pattern

The [minimal receive example](API_REFERENCE.md#1-ten-minute-path) keeps the
handle in owner-task storage, reads CU8 pairs, and preserves the pointer after
failed cleanup. Use the [cleanup retry example](API_REFERENCE.md#cleanup-retry-example)
for error paths, including a failed install that returns a non-NULL handle.

Do not abort on recoverable USB errors or unconditionally clear the handle after
uninstall. Suspend readers/callback-dependent work before destroying the handle.

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

## Incomplete teardown

`uninstall()` destroys the handle only on `ESP_OK`. A drain, worker join,
device close, client deregistration or final host cleanup failure retains the
handle and resources. Keep the pointer and retry `uninstall()` from its owner;
other handle APIs are disabled during incomplete teardown. Do not clear the
pointer on failure. If `install()` fails and cannot safely unwind, it returns a
non-NULL cleanup-only handle through `out_handle`; the original install error is
returned and a cleanup error is logged.

An empty enumerated-device list cannot distinguish no dongle from incomplete
USB enumeration through the current public IDF API. This branch conservatively
returns `ESP_ERR_INVALID_STATE` on final owned-host teardown in that case and
keeps the host resident. A reboot may be required. This is an explicit limitation,
including the no-dongle case, rather than proof of clean teardown. See the
[2026-10-29 investigation](reviews/EXTERNAL_DRIVER_FINDINGS_2026-10-29.md).

## Additional integration reference

The [complete reference](API_REFERENCE.md#20-profile-routing-identity-and-recovery)
covers device capabilities, identity, recovery and board controls. Its
[acquisition section](API_REFERENCE.md#21-acquisition-metadata)
covers both public headers and byte-associated metadata. The
[dated Osmocom comparison](LIBRTLSDR_PARITY_2026-11-09.md) distinguishes implemented
differences from physical acceptance and future proposals.
