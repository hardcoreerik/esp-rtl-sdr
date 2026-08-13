# Runtime constants (non-Kconfig)

Review feedback asked about “magic numbers.” This page names them, why they exist,
and how apps can influence them.

**Source:** `src/esp_rtl_sdr.cpp` (and public defaults in `include/esp_rtl_sdr.h`).

---

## IQ pull ring

| Constant | Value | Meaning |
|---|---|---|
| `kRingDepth` | **6** | Number of IQ slots in the internal free/filled queues for `read()` / delivery |

**Why 6:** Matches multi-URB pipeline depth class of default `transfer_count` (6).
Deep enough to absorb brief consumer jitter without unbounded RAM.

**App control:** Not Kconfig yet. Tune **URB** geometry via config /
[`KCONFIG.md`](KCONFIG.md) first; watch `consumer_drops`.

---

## Core pinning

| Constant | Value | Role |
|---|---|---|
| `kUsbCore` | **0** | Default core for USB host owner task |
| `kDeliveryCore` | **1** | IQ event delivery task |

**Why:** ESP32-P4 dual-core — keep USB completion path off the app UI core when possible.

**App control:**

```c
cfg.usb_task_core_id = 0;    /* pin USB task to core 0 */
cfg.usb_task_core_id = 0xFF; /* no affinity — driver falls back to kUsbCore for create */
cfg.usb_task_priority = 0;   /* 0 = driver default priority */
```

Delivery core is fixed at `kDeliveryCore` today (**Implemented** constant).
Making delivery core configurable is **Planned** if products need it.

---

## Health event cadence

| Constant | Value | Meaning |
|---|---|---|
| `kHealthPeriodBlocks` | **48** | While streaming, emit `EVT_HEALTH` at least every N IQ blocks (also on overall change) |

**Why 48:** ~order-of-magnitude seconds at typical block sizes / 960k without flooding logs.

**App control:** Always free to **poll** `get_health()` at your own rate.

---

## Public header defaults (not magic)

| Macro | Default | Doc |
|---|---|---|
| `ESP_RTL_SDR_DEFAULT_XFER_BYTES` | 16384 | Also Kconfig |
| `ESP_RTL_SDR_DEFAULT_XFER_COUNT` | 6 | Also Kconfig |
| `ESP_RTL_SDR_DEFAULT_STOP_TIMEOUT_MS` | 3000 | `stop(…, 0)` |
| `ESP_RTL_SDR_PASSPORT_DEFAULT_DWELL_MS` | 1500 | Passport |
| Rate / freq windows | see header | [`RATES.md`](RATES.md) |

---

## USB bulk endpoint

| Macro | Value |
|---|---|
| `ESP_RTL_SDR_BULK_EP_IN` | `0x81` | RTL2832U HS bulk IN (profile) |

Not tunable — profile identity.

---

## Change policy

Changing `kRingDepth` / cores / health period is a **behavior change**:

1. Note in CHANGELOG  
2. Re-run host tests if pure policy affected  
3. Prefer a lab passport soak after URB or ring changes  
4. Do not enable CAP bits as a side effect  
