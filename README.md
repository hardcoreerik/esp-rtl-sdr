# esp_rtl_sdr

**Clean-room ESP-IDF USB Host driver for RTL2832U-class SDR dongles**

Blog V4 (R828D) profile first · ESP32-P4 High-Speed · not a librtlsdr port

[![License: AGPL v3](https://img.shields.io/badge/License-AGPL_v3-blue.svg)](LICENSE)
![Status](https://img.shields.io/badge/version-0.6.0-green)
[![GitHub](https://img.shields.io/badge/github-esp--rtl--sdr-black)](https://github.com/hardcoreerik/esp-rtl-sdr)
![Target](https://img.shields.io/badge/ESP32--P4-HS_USB-green)

---

## Why this exists

Most ESP32 “RTL-SDR” work either:

- embeds **librtlsdr** (GPL, hard to treat as a clean reusable component), or  
- is a one-off sketch with no lifecycle API.

**esp_rtl_sdr** is a **stand-alone component** with:

- install → start → IQ events → retune → stop → uninstall  
- fail-closed state machine and reentrancy rules  
- first-class metrics (effective SPS, drops, sample stats)  
- **profile-based** path to more dongles without rewriting the core  
- clean-room Blog V4 USB tables (measured; not copied from librtlsdr)

Provenance: continuous IQ on **ESP32-P4** with official **RTL-SDR Blog V4** was
measured on M5Stack Tab5 and a Waveshare P4 kit under the OrcSDR program; this
repo is the **driver-only** home for that work under the name **esp_rtl_sdr**.

---

## Quick start (ESP-IDF)

```cmake
# your project CMakeLists.txt
set(EXTRA_COMPONENT_DIRS "F:/Ai/ESP_RTL_SDR")
```

```c
#include "esp_rtl_sdr.h"

esp_rtl_sdr_config_t cfg;
esp_rtl_sdr_config_default(&cfg);
cfg.event_cb = my_event_cb;

esp_rtl_sdr_handle_t sdr = NULL;
ESP_ERROR_CHECK(esp_rtl_sdr_install(&cfg, &sdr));

esp_rtl_sdr_stream_config_t st;
esp_rtl_sdr_stream_config_default(&st);
st.preset = ESP_RTL_SDR_PRESET_CUSTOM_HZ;
st.frequency_hz = 100000000;
st.sample_rate_sps = ESP_RTL_SDR_RATE_960K;
esp_err_t err = esp_rtl_sdr_start(sdr, &st);
```

Smoke example: [`examples/p4_serial_smoke`](examples/p4_serial_smoke).

---

## What works today (honest)

| Feature | Status |
|---|---|
| Blog V4 `0bda:2838` stream | Implemented (profile tables) |
| Rates 250k…3200k allowlist | Implemented — see [`docs/RATES.md`](docs/RATES.md) |
| Hot retune / set_center_freq | Implemented |
| set/get sample rate | Implemented (mid-stream rate = stop/start) |
| Sync `read()` IQ pull | Implemented |
| ppm correction | Implemented (software LO offset) |
| Multi-device select | Implemented (index / serial) |
| Metrics | Implemented |
| Gain / bias-T | Roadmap Phase 3 |
| Any random RTL2832U | **Not claimed** — profiles only |

```c
esp_rtl_sdr_set_sample_rate(sdr, ESP_RTL_SDR_RATE_960K);
esp_rtl_sdr_set_center_freq(sdr, 100100000);
esp_rtl_sdr_set_freq_correction(sdr, -12);  /* ppm */
esp_rtl_sdr_start_hz(sdr, 0, 0);  /* uses preferred rate/freq */
uint8_t buf[16384];
size_t n = 0;
esp_rtl_sdr_read(sdr, buf, sizeof(buf), 1000, &n);
```

Authoritative status: **[`Project_truth.md`](Project_truth.md)**.  
Architecture: **[`architecture.md`](architecture.md)**.  
Plan: **[`Roadmap.md`](Roadmap.md)**.  
vs desktop drivers: **[`docs/CAPABILITY_MATRIX.md`](docs/CAPABILITY_MATRIX.md)**.  
Rates evidence: **[`docs/RATES.md`](docs/RATES.md)**.

---

## Documentation map

| Doc | Purpose |
|---|---|
| [Project_truth.md](Project_truth.md) | What is true **now** (evidence boundaries) |
| [architecture.md](architecture.md) | Design, profiles, threading |
| [Roadmap.md](Roadmap.md) | How we close librtlsdr-class gaps |
| [docs/API.md](docs/API.md) | Full public API contract |
| [docs/CAPABILITY_MATRIX.md](docs/CAPABILITY_MATRIX.md) | Side-by-side desktop comparison |
| [docs/RATES.md](docs/RATES.md) | Sample-rate allowlist + evidence |
| [docs/CLEAN_ROOM.md](docs/CLEAN_ROOM.md) | Clean-room rules |
| [docs/PROFILES.md](docs/PROFILES.md) | Dongle profiles (Blog V4 first) |
| [docs/PORTING.md](docs/PORTING.md) | Board BSP vs driver |
| [CHANGELOG.md](CHANGELOG.md) | Release notes |
| [LICENSING.md](LICENSING.md) | AGPL + commercial option |

---

## IQ format

Default stream: **unsigned 8-bit interleaved I/Q**  
`(I0, Q0, I1, Q1, …)` — same family as common desktop rtl-sdr CU8 dumps.

---

## License

**AGPL-3.0-only** by default. Commercial licensing available — see [LICENSING.md](LICENSING.md).

---

## Related

- [OrcSDR](https://github.com/hardcoreerik/OrcSDR) — full ESP32-P4 radio appliances that *consume* this class of driver  
- This repo is independent; you do not need OrcSDR to use esp_rtl_sdr
