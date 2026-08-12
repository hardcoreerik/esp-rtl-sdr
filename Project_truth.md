# Project truth — esp_rtl_sdr

**Authoritative current-state document.** When other docs disagree, this file
wins for *what is true right now*.

Snapshot date: **2026-08-12**  
Version: **0.6.0** (Phase 2: rates + ppm + multi-device)  
Local repo: `F:\Ai\ESP_RTL_SDR\`  
Remote: **https://github.com/hardcoreerik/esp-rtl-sdr**

---

## Evidence labels

| Label | Meaning |
|---|---|
| **Hardware-verified** | Observed on named physical hardware and recorded |
| **Build-verified** | Compiles / present as artifacts without hardware claim |
| **Implemented** | Present in source; acceptance may still be open |
| **Planned** | Accepted roadmap work, not implemented |
| **Deferred** | Intentionally out of current milestone |
| **Provenance** | Measured under another project; tables copied here; re-soak open |
| **Formula** | RTL ratio math programmed; continuous P4 soak not claimed |

---

## What this project is

**esp_rtl_sdr** is a **stand-alone ESP-IDF USB Host driver** for **RTL2832U-class**
software-defined radio dongles. It delivers continuous **unsigned interleaved
I/Q (CU8)** over a fail-closed C API designed for FreeRTOS / ESP32-P4.

It is **not**:

- A port of **librtlsdr** / rtl-sdr-blog
- The OrcSDR application (demod, UI, web)
- A claim of Full-Speed ESP32-S2/S3 support until measured
- A claim that every RTL2832U dongle works until a **profile** is measured

---

## Current capability truth

| Area | State | Boundary |
|---|---|---|
| Public C API lifecycle (install/start/stop/uninstall) | **Implemented** | Header + source in this repo |
| Continuous bulk IQ stream (multi-URB) | **Implemented** | Blog V4 profile tables |
| In-stream `retune_hz` | **Implemented** | Drain bulk before EP0 |
| Metrics (bytes, SPS, min/max/mean, drops) | **Implemented** | `get_metrics` |
| Sample rates | **Implemented** | Allowlist: 250k, 256k, 960k, 1024k, 1800k, 2048k, 2400k, 3200k — see `docs/RATES.md` |
| P4 continuous rate evidence | **Provenance** | 960k + 2048k under OrcSDR; others **Formula** |
| Frequency | **Implemented** | CUSTOM_HZ + presets; `set/get_center_freq`; retune when streaming |
| Sample rate set/get (allowlist) | **Implemented** | `set/get_sample_rate`; mid-stream rate change = BUSY |
| Sync `read()` (blocking IQ pull) | **Implemented** | Pull ring; `CAP_SYNC_READ` |
| `start_hz(freq, rate)` convenience | **Implemented** | Phase 1 |
| ppm correction (software LO offset) | **Implemented** | `set/get_freq_correction`; ±200 ppm; applied at tune |
| Multi-device enumerate / select | **Implemented** | index + serial; max 8 candidates |
| Blog V4 identity filter `0bda:2838` | **Implemented** | Product/mfg string checks in profile |
| Dual-core USB / delivery split | **Implemented** | Core0 USB, Core1 delivery path |
| Fail-closed / reentrancy rules | **Implemented** | Documented in `docs/API.md` |
| Tab5 Blog V4 RF (960k / 2.048M ADS-B) | **Provenance** | Measured under OrcSDR; tables originated there |
| Waveshare P4 Blog V4 (same driver, unmodified) | **Provenance** | Second-board proof under OrcSDR Waveshare shell |
| Re-verify from *this* stand-alone tree on hardware | **Planned** | Flash example or consumer app from `F:\Ai\ESP_RTL_SDR` |
| Gain get/set / AGC modes | **Planned** | Roadmap Phase 3 — needs independent USB evidence |
| Bias-T | **Planned** | CAP reserved until measured |
| Direct sampling / HF | **Deferred** | CAP reserved; not claimed |
| Arbitrary sample rates | **Deferred** | Only allowlist |
| R820T2 / other tuner profiles | **Planned** | Architecture ready; tables not present |
| Hot-plug recovery without reboot | **Planned** | Events exist; full recovery open |
| Five-minute formal soak artifact in-repo | **Planned** | Phase 5 |
| librtlsdr ABI compatibility | **Deferred** | Not a goal; API is ESP-native |

---

## Versioning truth

| Version | Meaning |
|---|---|
| OrcSDR component **0.4.1** | Historical name `rtl_sdr_v4_esp`; Blog V4 streaming + retune |
| This repo **0.5.0** | Rename to `esp_rtl_sdr` + Phase 1 set/get freq/rate + sync `read` + `start_hz` |
| This repo **0.6.0** | Phase 2: expanded rates + ppm + multi-device select |

Breaking rename (0.5.0): consumers must switch includes and symbols. No shim in
this repo (OrcSDR may add a shim later; **not done here**).

---

## Clean-room truth

- Transfer sequences for the Blog V4 profile are from **independent USB
  observation**, not from librtlsdr source.
- Rules: `docs/CLEAN_ROOM.md`.
- New tuners, gain, bias-T, HF require **new evidence** before claims.

---

## Relationship to OrcSDR

| | |
|---|---|
| This repo | Stand-alone driver product |
| OrcSDR | Optional application consumer (Tab5 UI, Waveshare web shell, demod) |
| Coupling | **None required.** This tree must build and document without OrcSDR |

**Project rule for this workspace:** do not modify OrcSDR / OrcSDR_Waveshare
when working here unless a future task explicitly says otherwise.

---

## Hardware matrix (truth)

| Host | USB | Dongle profile | State |
|---|---|---|---|
| ESP32-P4 M5Stack Tab5 | HS host | Blog V4 R828D | **Provenance** (OrcSDR) |
| ESP32-P4 Waveshare Module-DEV-KIT | HS host | Blog V4 R828D | **Provenance** (OrcSDR second board) |
| ESP32-S3 / S2 | FS OTG | — | **Not claimed** |
| Classic ESP32 | no native HS host | — | **Out of scope** |

---

## How to use this document

1. Update the snapshot date when truth changes.
2. Move rows from Planned → Implemented only when source lands.
3. Move Implemented → Hardware-verified only when *this* repo’s build is
   exercised on named hardware with a log reference.
4. `Roadmap.md` tracks how we close gaps; this file tracks what is true **now**.
