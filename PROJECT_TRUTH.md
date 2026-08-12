# PROJECT_TRUTH — esp_rtl_sdr

**Authoritative current-state document.** When other docs disagree, this file
wins for *what is true right now*.

Same discipline as [TheOrc PROJECT_TRUTH](https://github.com/hardcoreerik/TheOrc):
claims need evidence labels; oversell is a bug; retract rather than spin.

Snapshot date: **2026-08-12**  
Version: **0.7.3** (async retune from callback — not production-ready)  
Local repo: `F:\Ai\ESP_RTL_SDR\`  
Remote: **https://github.com/hardcoreerik/esp-rtl-sdr**  
Open-source honesty: [docs/AI_DEVELOPMENT_DISCLOSURE.md](docs/AI_DEVELOPMENT_DISCLOSURE.md) ·
[docs/DOCUMENTATION_STANDARD.md](docs/DOCUMENTATION_STANDARD.md) ·
[SECURITY.md](SECURITY.md) · [CONTRIBUTING.md](CONTRIBUTING.md)

---

## Development honesty (read this first)

| Fact | Truth |
|---|---|
| Human maintainer | Erik / hardcoreerik — product direction, lab, release authority |
| How code is written | **AI-assisted** (Claude, Codex, Grok Build); human owns outcomes |
| Professional audit team | **No** — single maintainer + multi-AI review, not a firm |
| librtlsdr port? | **No** — clean-room measured Blog V4 path; different API |
| Production maturity | **0.x early** — API can still grow; fail-closed is a goal, not a warranty |
| Formal security audit | **None** |
| Stand-alone re-soak from this tree on P4 | **Still open** (Tab5/Waveshare = Provenance under OrcSDR) |

If marketing copy contradicts this table, open a `truth:` issue.

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
| In-stream `retune_hz` | **Implemented** | Drain bulk before EP0; **async from callback** (0.7.3) |
| Metrics | **Implemented** | `get_metrics` |
| Continuous sample rates (hardware windows) | **Implemented** | 225–300k ∪ 900k–3.2M + quantize → exact |
| Recommended rate list | **Implemented** | `get_supported_rates` |
| Rate passport (`probe_rates`) | **Implemented** | On-device soak; needs P4+dongle run |
| Host unit tests (policy) | **Implemented** | `tests/host` — no IDF; CI on push |
| CI truth/version hygiene | **Implemented** | `.github/workflows/ci.yml` |
| ESP-IDF P4 compile CI | **Implemented** | `examples/p4_serial_smoke` idf.py build esp32p4 |
| Full USB/RF CI | **No** | Needs P4 + dongle (lab only) |
| `apply_need()` intent presets | **Implemented** | FM/ADSB/WX/HF/MAX_STABLE/LISTEN |
| `get_health()` / `EVT_HEALTH` | **Implemented** | Poll + delivery emit (change/periodic) |
| Gain / bias public API | **Implemented (stubs)** | Returns UNSUPPORTED; CAP bits **off** |
| set/get center freq, rate, ppm | **Implemented** | |
| Sync `read()` | **Implemented** | |
| Multi-device select | **Implemented** | |
| Blog V4 filter `0bda:2838` | **Implemented** | |
| Dual-core USB/delivery | **Implemented** | |
| Tab5 / Waveshare Blog V4 RF | **Provenance** | OrcSDR |
| Re-verify from *this* tree on hardware | **Planned** | |
| Gain / bias-T hardware | **Planned** | Phase 3 — needs capture (GAIN_BIAS_CAPTURE.md) |
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

See **`docs/TESTING.md`** and honest hobbyist guide **`docs/LAB_HOBBYIST.md`**.

| Asset | Use |
|---|---|
| Blog V4 on P4 hosts | Primary IQ path |
| 2× Heltec V4 | Prior LoRa decode work; controlled digital RF |
| Baofeng UV-5R | FM / carrier stimulus (legal TX only; power risk) |
| Flipper Zero | Test tones / interferer / protocol toys |
| **TinySA Ultra** | Spectrum + weak generator; **relative** RF only |
| PC USBPcap/Wireshark | Phase 3 USB capture (hobbyist) |

**Lab posture:** hobbyist desk, not a cal chamber. Enough for USB + multimeter +
relative RF if claims stay labeled.

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

---

## How to use this document

1. Update the snapshot date when truth changes.
2. Move Planned → Implemented only when source lands.
3. Move Implemented → Hardware-verified only when *this* tree is exercised on
   named hardware with a log reference.
4. `Roadmap.md` tracks direction; this file tracks what is true **now**.
5. Follow [docs/DOCUMENTATION_STANDARD.md](docs/DOCUMENTATION_STANDARD.md).
6. If a release oversold something, **retract in CHANGELOG** — TheOrc-style.

## Related truth docs

| Doc | Role |
|---|---|
| [docs/AI_DEVELOPMENT_DISCLOSURE.md](docs/AI_DEVELOPMENT_DISCLOSURE.md) | Who writes code; trust rituals |
| [docs/CLEAN_ROOM.md](docs/CLEAN_ROOM.md) | No librtlsdr source paste |
| [docs/SILICON.md](docs/SILICON.md) | DS / cousins; what we refuse |
| [docs/VISION.md](docs/VISION.md) | Direction (not all Implemented) |
| [docs/TESTING.md](docs/TESTING.md) | Lab gear disclosure |
| [SECURITY.md](SECURITY.md) | Vulnerability reporting |
| [CONTRIBUTING.md](CONTRIBUTING.md) | PR rules |
