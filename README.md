# esp_rtl_sdr

**Clean-room ESP-IDF USB Host driver for RTL2832U-class SDR dongles**

Blog V4 (R828D) profile first · ESP32-P4 High-Speed · **not a librtlsdr port**

[![License: AGPL v3](https://img.shields.io/badge/License-AGPL_v3-blue.svg)](LICENSE)
![Status](https://img.shields.io/badge/version-0.7.1-green)
[![GitHub](https://img.shields.io/badge/github-esp--rtl--sdr-black)](https://github.com/hardcoreerik/esp-rtl-sdr)
![Target](https://img.shields.io/badge/ESP32--P4-HS_USB-green)

**Authoritative status:** [`PROJECT_TRUTH.md`](PROJECT_TRUTH.md) — if this README and PROJECT_TRUTH disagree, **PROJECT_TRUTH wins**.

---

## Project credits

**esp_rtl_sdr** is created and maintained by [Erik / hardcoreerik](https://github.com/hardcoreerik), with **AI-assisted** development and review — the same open honesty model as [TheOrc](https://github.com/hardcoreerik/TheOrc).

| Contributor | Role |
|---|---|
| [Erik / hardcoreerik](https://github.com/hardcoreerik) | Creator, maintainer, product direction, hardware lab, release authority |
| Claude Sonnet | Architecture planning, implementation support, review |
| OpenAI Codex | Implementation support, adversarial review |
| Grok Build | Implementation, docs, PROJECT_TRUTH audits |

See **[`docs/AI_DEVELOPMENT_DISCLOSURE.md`](docs/AI_DEVELOPMENT_DISCLOSURE.md)** for what “AI-assisted” means: what is verified, what is not, and how to report a doc/code mismatch.

---

## Where the project is right now (honest)

This is a **0.x stand-alone driver**, extracted from work that began under **OrcSDR**, then renamed and expanded here. It is **early**, **public**, and held to truth labels — not marketing.

| Topic | Truth today |
|---|---|
| What ships | Fail-closed C API, Blog V4 profile stream, retune, metrics, sync `read`, ppm, multi-device, continuous rates (hardware windows), `apply_need`, `get_health`, rate `probe_rates` passport |
| What does **not** ship | librtlsdr ABI, gain/bias CAP, full HF upconverter CAP, “any eBay RTL stick”, formal security audit |
| Hardware | Tab5 / Waveshare P4 Blog V4 = **Provenance** (OrcSDR). Re-soak from *this* tree’s example = **still open** |
| Clean-room | Blog V4 EP0 tables measured; **not** copied from librtlsdr / rtl-sdr-blog source |
| License | **AGPL-3.0-only** default + commercial option ([LICENSING.md](LICENSING.md)) |

Oversell is a bug. Prefer a `truth:` GitHub issue over silent README inflation.

---

## Why this exists

Most ESP32 “RTL-SDR” work either embeds **librtlsdr** (hard to treat as a clean reusable component) or is a one-off sketch with no lifecycle API.

**esp_rtl_sdr** is a **stand-alone component** with:

- install → start → IQ events → retune → stop → uninstall  
- fail-closed state machine and reentrancy rules  
- first-class metrics (effective SPS, drops, sample stats)  
- intent / health / on-host rate passport (ESP-native; not a desktop clone)  
- **profile-based** expansion without becoming a silent port of desktop drivers  

Direction (not all implemented): [`docs/VISION.md`](docs/VISION.md).

---

## Quick start (ESP-IDF)

```cmake
# your project CMakeLists.txt — use your path or a submodule
set(EXTRA_COMPONENT_DIRS "/path/to/esp-rtl-sdr")
```

```c
#include "esp_rtl_sdr.h"

esp_rtl_sdr_config_t cfg;
esp_rtl_sdr_config_default(&cfg);
cfg.event_cb = my_event_cb;

esp_rtl_sdr_handle_t sdr = NULL;
ESP_ERROR_CHECK(esp_rtl_sdr_install(&cfg, &sdr));

esp_rtl_sdr_apply_need(sdr, ESP_RTL_SDR_NEED_FM);
esp_rtl_sdr_start_hz(sdr, 0, 0);

uint8_t buf[16384];
size_t n = 0;
esp_rtl_sdr_read(sdr, buf, sizeof(buf), 1000, &n);

esp_rtl_sdr_health_info_t h;
esp_rtl_sdr_get_health(sdr, &h);
```

Smoke example: [`examples/p4_serial_smoke`](examples/p4_serial_smoke).

---

## Open source & truth practice (TheOrc-aligned)

| Practice | Where |
|---|---|
| Evidence labels on claims | [PROJECT_TRUTH.md](PROJECT_TRUTH.md) |
| AI-assisted development disclosed | [docs/AI_DEVELOPMENT_DISCLOSURE.md](docs/AI_DEVELOPMENT_DISCLOSURE.md) |
| Docs must not mark planned as done | [docs/DOCUMENTATION_STANDARD.md](docs/DOCUMENTATION_STANDARD.md) |
| **Automated host tests + CI** | [docs/TESTING_GUIDE.md](docs/TESTING_GUIDE.md) · `.github/workflows/ci.yml` |
| Clean-room rules | [docs/CLEAN_ROOM.md](docs/CLEAN_ROOM.md) |
| Security contact (no public 0-days) | [SECURITY.md](SECURITY.md) |
| How to contribute | [CONTRIBUTING.md](CONTRIBUTING.md) |
| Silicon / DS honesty | [docs/SILICON.md](docs/SILICON.md) |
| Lab gear (Heltec ×2, Baofeng UV-5R, Flipper Zero) | [docs/TESTING.md](docs/TESTING.md) |

```powershell
# Run automated policy tests (no hardware, no ESP-IDF)
powershell -ExecutionPolicy Bypass -File tests\scripts\run_host_tests.ps1
```

Retract oversell in [CHANGELOG.md](CHANGELOG.md) rather than rewriting history without a note.

---

## Documentation map

| Doc | Purpose |
|---|---|
| [PROJECT_TRUTH.md](PROJECT_TRUTH.md) | What is true **now** |
| [docs/README.md](docs/README.md) | Full docs index |
| [architecture.md](architecture.md) | Design, profiles, threading |
| [Roadmap.md](Roadmap.md) | Phases |
| [docs/VISION.md](docs/VISION.md) | Nervous-system direction |
| [docs/API.md](docs/API.md) | Public API contract |
| [docs/RATES.md](docs/RATES.md) | Continuous rates + passport |
| [docs/CAPABILITY_MATRIX.md](docs/CAPABILITY_MATRIX.md) | vs desktop drivers |
| [CHANGELOG.md](CHANGELOG.md) | Releases |
| [LICENSING.md](LICENSING.md) | AGPL + commercial |

---

## IQ format

Default stream: **unsigned 8-bit interleaved I/Q** `(I0, Q0, I1, Q1, …)`.

---

## Ecosystem

- **[TheOrc](https://github.com/hardcoreerik/TheOrc)** — local-first multi-agent orchestration (truth-audit culture we copy)  
- **[OrcSDR](https://github.com/hardcoreerik/OrcSDR)** — ESP32-P4 SDR application (optional consumer; provenance host)  
- **[esp-rtl-sdr](https://github.com/hardcoreerik/esp-rtl-sdr)** — **this** stand-alone driver  

Coupling is optional. This tree must document and build without OrcSDR.

---

## License

**AGPL-3.0-only** by default. Commercial licensing available — see [LICENSING.md](LICENSING.md).
