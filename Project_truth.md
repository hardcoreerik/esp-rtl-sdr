# Project truth — esp_rtl_sdr

**Authoritative current-state document.** When other docs disagree, this file
wins for *what is true right now*.

Snapshot date: **2026-08-12**  
Version: **0.7.0** (continuous rates + need / health / passport)  
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
SDR dongles. It delivers continuous **CU8 IQ** with a fail-closed C API, and is
evolving into a **dongle nervous system** (intent, health, on-host passport) —
not a librtlsdr port.

Product vision: **`docs/VISION.md`**. Silicon / DS map: **`docs/SILICON.md`**.

---

## Current capability truth

| Area | State | Boundary |
|---|---|---|
| Lifecycle install/start/stop/uninstall | **Implemented** | |
| Continuous bulk IQ (multi-URB) | **Implemented** | Blog V4 profile |
| In-stream `retune_hz` | **Implemented** | Drain bulk before EP0 |
| Metrics | **Implemented** | `get_metrics` |
| Continuous sample rates (hardware windows) | **Implemented** | 225–300k ∪ 900k–3.2M + quantize → exact |
| Recommended rate list | **Implemented** | `get_supported_rates` |
| Rate passport (`probe_rates`) | **Implemented** | On-device soak; needs P4+dongle run |
| `apply_need()` intent presets | **Implemented** | FM/ADSB/WX/HF/MAX_STABLE/LISTEN |
| `get_health()` | **Implemented** | USB/RF narrative + advice |
| set/get center freq, rate, ppm | **Implemented** | |
| Sync `read()` | **Implemented** | |
| Multi-device select | **Implemented** | |
| Blog V4 filter `0bda:2838` | **Implemented** | |
| Dual-core USB/delivery | **Implemented** | |
| Tab5 / Waveshare Blog V4 RF | **Provenance** | OrcSDR |
| Re-verify from *this* tree on hardware | **Planned** | |
| Gain / bias-T | **Planned** | Phase 3 — measured EP0 |
| HF upconverter path CAP | **Planned** | NEED_HF stores LO only today |
| R828D stage gain / input / notches | **Planned** | |
| Adaptive USB URB | **Planned** | |
| Beacon ppm learn | **Planned** | |
| librtlsdr ABI | **Deferred** | Not a goal |

---

## Versioning truth

| Version | Meaning |
|---|---|
| OrcSDR **0.4.1** | Historical `rtl_sdr_v4_esp` |
| **0.5.0** | Rename + Phase 1 freq/rate/read |
| **0.6.0** | Phase 2: expanded rates list + ppm + multi-device |
| **0.7.0** | Continuous rates + need + health + passport + docs (vision/silicon/lab) |

---

## Test lab (available gear)

See **`docs/TESTING.md`**.

| Asset | Use |
|---|---|
| Blog V4 on P4 hosts | Primary IQ path |
| 2× Heltec V4 | Prior LoRa decode work; controlled digital RF |
| Baofeng UV-5R | FM / carrier stimulus (legal TX only) |
| Flipper Zero | Test tones / interferer / protocol toys |

---

## Clean-room truth

- Blog V4 EP0 tables: independent measurement, not librtlsdr.  
- Rules: `docs/CLEAN_ROOM.md`.  
- DS / R820T2 PDFs: insight only; new features need capture or soak.

---

## Relationship to OrcSDR

Stand-alone driver. **Do not modify OrcSDR / OrcSDR_Waveshare** from this workspace
unless a task explicitly says otherwise.

---

## Hardware matrix (hosts)

| Host | USB | Dongle | State |
|---|---|---|---|
| ESP32-P4 Tab5 | HS | Blog V4 | **Provenance** |
| ESP32-P4 Waveshare | HS | Blog V4 | **Provenance** |
| ESP32-S3/S2 | FS | — | **Not claimed** |
