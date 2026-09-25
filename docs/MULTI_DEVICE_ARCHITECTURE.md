# Multi-device driver architecture

**Status:** Implemented in source on `esp-rtl-sdr-signal-anomaly`.  
**Hardware concurrent streams:** not yet measured from this tree.  
**RF coherence:** none. Captures are concurrent and host-timestamped only.

This document is the driver foundation for a future multi-receiver anomaly
platform. It does **not** describe ML, novelty scoring, or classification.

---

## What the old driver could not do

The public API was already handle-based (`esp_rtl_sdr_handle_t`) with
per-handle tuner, URB pool, ring, and metrics. “Multi-device” meant:

```text
one handle
  → enumerate N candidates
  → select ONE
  → stream ONE
```

That is **not** concurrent operation. Barriers:

| Barrier | Effect |
|---|---|
| Each `install()` called `usb_host_install()` unless the app set `host_library_already_installed` | Second handle failed or tore down the first’s host |
| Host daemon task was owned by the handle that installed USB | Uninstalling handle 0 killed USB under handle 1 |
| Auto-open always preferred candidate index 0 | Two handles both claimed the same dongle |
| No exclusive USB-address table | Two clients could open the same RTL2832U |
| Logs had no `[RTLn]` identity | Streaming errors were ambiguous |
| `device_info_t` lacked USB path / parent / port | Identical VID/PID/serial sticks could not be told apart by topology |
| IQ metadata lacked `device_id` | Later comparison of receivers had no stamp |

Tuner/gain/frequency state was already per-handle. The missing piece was
**shared USB session + exclusive claim + identity**.

---

## New device-context model

```text
                    esp_rtl_sdr_handle_t  (= one receiver)
                    ┌─────────────────────────────────────┐
                    │ logical_index  [RTL0]               │
                    │ USB client + device handle          │
                    │ addr / parent / hub_port / serial   │
                    │ RTL2832 + tuner profile state       │
                    │ freq, rate, gain, ppm, bias         │
                    │ URB pool, IQ ring, pull ring        │
                    │ delivery task, client task          │
                    │ metrics + stream_stats              │
                    │ callbacks + user ctx                │
                    └─────────────────────────────────────┘
```

Compatibility: one `install()` + `start()` still streams one dongle.

Concurrent use:

```c
esp_rtl_sdr_handle_t rtl0, rtl1, rtl2;
esp_rtl_sdr_config_t cfg;
esp_rtl_sdr_config_default(&cfg);
cfg.bind_device_index = ESP_RTL_SDR_BIND_ANY;
esp_rtl_sdr_install(&cfg, &rtl0);
esp_rtl_sdr_install(&cfg, &rtl1);
esp_rtl_sdr_install(&cfg, &rtl2);
```

Each handle claims the first USB address not already owned.

---

## USB host topology (ESP32-P4)

ESP32-P4 has one **High-Speed** OTG host used by this driver. Multiple
class clients are supported. Hub support requires
`CONFIG_USB_HOST_HUBS_SUPPORTED=y`.

```text
        ESP32-P4 USB HS host
                |
         usb_host_install  (once, refcounted session)
                |
         host daemon task  rtl_usb_lib
                |
        +-------+--------+
        |                |
   client RTL0      client RTL1  ...
   rtl_cli0         rtl_cli1
        |                |
     dongle A         dongle B
```

ESP-IDF facts that constrain us:

- Transfer timeouts are not implemented in the host stack (`usb_timeouts` stays 0).
- External hub driver has **no Transaction Translator**: FS/LS devices behind
  a HS hub will not work. RTL2832U is High-Speed, so this is acceptable.
- Hub error/overcurrent handling is incomplete in IDF.
- P4 has 16 host channels. Each RTL2832U needs EP0 + bulk IN 0x81, plus the
  hub itself. Three HS bulk devices are within the channel budget on paper.

---

## Waveshare hub model

See [`WAVESHARE_P4_MULTI_RTL_PROTOTYPE.md`](WAVESHARE_P4_MULTI_RTL_PROTOTYPE.md).

```text
P4 HS ── mux (FSUSB42) ─┬─ HOST jumper  → USB-A port 1 (direct)
                        └─ DEVICE jumper → CH334F ─┬─ port 2
                                                   ├─ port 3
                                                   └─ port 4
```

Port 1 **or** ports 2–4, not both. Three dongles = DEVICE jumper + CH334.

---

## Tasks

| Task | Core (default) | Count | Role |
|---|---|---|---|
| `rtl_usb_lib` | 0 | 1 (session) | `usb_host_lib_handle_events` |
| `rtl_cliN` | 0 | 1 per handle | client events, probe, EP0 wait pump |
| `rtl_iqN` | 1 | 1 per streaming handle | IQ callback + sideband EP0 |

No extra task per URB. Continuous streaming does not allocate.

---

## Queues and transfer ownership

Per handle (unchanged layout, now instanced N times):

```text
bulk URB pool  (usb_host_transfer_alloc, DMA-capable)
      │ complete → memcpy
      ▼
 IQ ring slots (PSRAM preferred, else internal)
      │
      ├─ EVT_IQ_BLOCK  (borrowed until callback returns)
      └─ optional pull ring for read()
```

Default: 6 × 16 KiB URBs. Three receivers ≈ 288 KiB USB buffers plus rings.

Ownership:

- USB transfers belong to the handle that allocated them.
- `context` on every URB is that handle.
- Disconnect of RTL1 stops only RTL1’s URBs.

---

## Memory

| Buffer | Location | Why |
|---|---|---|
| USB transfer data | IDF `usb_host_transfer_alloc` (DMA) | Must be DMA-capable |
| IQ ring | PSRAM then internal | Copied out of the URB in `bulk_cb` |
| Pull ring | Internal, shrink-to-fit | `read()` path only |
| Control transfer | DMA | EP0 |

Streaming data **may** live in PSRAM after the USB copy. The URB itself must
not.

---

## Locks

| Lock | Scope |
|---|---|
| Handle mutex | Public API on that handle |
| `ctrl_mutex` | EP0 on that handle |
| Session mutex | Claim table, refcount, hub counters |

No global tuner lock. I2C/EP0 is per dongle (each RTL2832U has its own I2C
bridge). The USB stack serializes EP0 per device; two handles may submit
control to two devices concurrently.

Do not take the session lock while holding a handle lock across USB I/O.

---

## Device identity

VID/PID/product are shared across Blog V4 sticks. Identity is:

```text
logical_index + usb_addr + parent_addr + hub_port + serial + profile
```

`esp_rtl_sdr_get_identity()` / `get_candidate_identity()` expose this.
Logical roles (REFERENCE / PROBE_A / PROBE_B) are **not** hardcoded; apps
map them using identity.

---

## Enumeration and hotplug

1. Session `usb_host_install` once.
2. Each handle registers its own USB client.
3. `NEW_DEV` is delivered to **every** client. Only an unclaimed address is
   opened.
4. `DEV_GONE` is delivered only to clients that opened that device.
5. One dongle failing goes FAULT/IDLE on **that** handle. Others keep
   streaming.

Not a full production hotplug recovery: IDF hub overcurrent is unimplemented.
Do not assume a hub unplug is cleanly recoverable without re-install.

---

## Statistics

- Per handle: `get_metrics()` (existing) + `get_stream_stats()` (extended).
- Session: `get_hub_stats()`.

Use these to decide whether 2 or 3 × 2.4 MS/s is healthy. Do not guess.

---

## Timestamps

`host_timestamp_us` is taken in `bulk_cb` at USB transfer completion with
`esp_timer_get_time()` (P4 monotonic µs).

That is the lowest-jitter software point this stack has: it runs in the
USB client event context, before queueing and before the delivery task.

Limitations:

- Not the RTL2832 sample clock.
- Not SOF-derived.
- Jitter includes HCD complete → callback scheduling.
- Independent dongle oscillators remain independent.

Do **not** label buffers “synchronized IQ”.

---

## Future anomaly-platform requirements

The driver must allow, later, without a rewrite:

```text
rtl0 hold configuration
rtl1 frequency offset +100 kHz
rtl2 gain −8 dB
capture N samples from all active receivers
```

That is why configuration is per-handle, claims are exclusive, and every
buffer can carry `device_id` + host timestamp + freq/gain/rate.

The anomaly layer stays **above** this API. No novelty scores here.
