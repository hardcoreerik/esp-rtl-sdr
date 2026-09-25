# Multi-device performance

Separate **theoretical**, **measured**, and **not yet measured**.

No hardware soak from `esp-rtl-sdr-signal-anomaly` has been recorded yet.
Do not treat the theoretical rows as lab results.

---

## Payload math (theoretical)

CU8 IQ = 2 bytes per complex sample.

| Receivers | Rate / device | Payload | Payload bits |
|---|---|---|---|
| 1 | 2.4 MS/s | 4.8 MB/s | ~38.4 Mbit/s |
| 2 | 2.4 MS/s | 9.6 MB/s | ~76.8 Mbit/s |
| 3 | 2.4 MS/s | 14.4 MB/s | ~115.2 Mbit/s |

USB HS is 480 Mbit/s on the wire. Protocol overhead, hub split, HCD, memcpy,
and delivery tasks all sit between that number and sustained IQ.

Single-receiver 2.4 MS/s has been in the measured Blog V4 envelope on P4
**provenance** hosts (Tab5 / Waveshare) in earlier trees. Re-soak of *this*
branch is open.

---

## Memory (theoretical, default 6 × 16 KiB URBs)

| Item | 1 RX | 2 RX | 3 RX |
|---|---|---|---|
| USB URB payload | 96 KiB | 192 KiB | 288 KiB |
| IQ ring (6 slots × URB) | 96 KiB | 192 KiB | 288 KiB |
| Tasks (cli+iq per handle, 1 host daemon) | 1 + 2 | 1 + 4 | 1 + 6 |

Plus control transfers, BSS, and optional pull rings (CALLBACK mode in the
harness avoids the large pull ring).

P4 module: 768 KiB HP SRAM + 32 MB PSRAM. USB buffers are DMA allocations;
IQ rings prefer PSRAM.

---

## CPU (theoretical)

USB client + memcpy of 4.8 MB/s per stream on core 0; delivery on core 1.
Three streams ≈ 14.4 MB/s memcpy. Expected to be feasible; **not measured**.

---

## Measured

| Config | Sample rate | Aggregate | Duration | Bytes | Errors | Drops | CPU | RAM | PSRAM | Notes |
|---|---|---|---|---|---|---|---|---|---|---|
| 1 RX | — | — | — | — | — | — | — | — | — | **not yet measured** |
| 2 RX | — | — | — | — | — | — | — | — | — | **not yet measured** |
| 3 RX | — | — | — | — | — | — | — | — | — | **not yet measured** |

Fill from `examples/multi_rtlsdr_test` `rtl stats` / `rtl watch` after Gates
1–5. Attach serial logs under `docs/Test_reports/` when they exist.

Host unit tests (no USB) prove claim isolation, identity, metadata, and
sequence numbers only. They are **not** throughput measurements.

---

## How to measure

Harness: `examples/multi_rtlsdr_test`

```
rtl stream all start
rtl watch 5000
```

Record for each receiver:

- `effective_sample_rate`
- `bytes_received`
- `usb_transfer_errors`
- `buffer_overruns`
- `dropped_buffers`
- `short_transfers`
- `queue_high_water`
- `stream_uptime_ms`

Plus `hub` counters and `idf.py size` / `heap_caps_get_free_size` if taken.

If 3 × 2.4 MS/s fails, step 2.048 MS/s then 1.8 MS/s then 960 kS/s and write
the first stable row. That is a valid scientific result.
