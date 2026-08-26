# esp_rtl_sdr

**Make an RTL-SDR Blog V4 a first-class peripheral on ESP32-P4** — continuous I/Q over USB Host, with a real embedded driver API.

[![License: AGPL v3](https://img.shields.io/badge/License-AGPL_v3-blue.svg)](LICENSE)
![Status](https://img.shields.io/badge/version-0.7.8-green)
[![GitHub](https://img.shields.io/badge/github-esp--rtl--sdr-black)](https://github.com/hardcoreerik/esp-rtl-sdr)
![Target](https://img.shields.io/badge/ESP32--P4-HS_USB-green)

**Not a librtlsdr port.** Clean-room Blog V4 USB profile · stand-alone ESP-IDF component · fail-closed lifecycle  

**Status authority:** [`PROJECT_TRUTH.md`](PROJECT_TRUTH.md) wins if anything here disagrees. This is **0.x** — early, public, honest.

---

## Why this project exists

Most microcontroller “RTL-SDR” work falls into one of two traps:

| Typical approach | What goes wrong on an MCU |
|---|---|
| **Port / embed librtlsdr** | Desktop library assumptions (threads, heap, USB stack). Hard to own as a small component; GPL gravity; opaque lifecycle. |
| **One-off sketch** | Works once in a demo; no clear start/stop; no metrics when USB starves; no way to know what the *board* can sustain. |

**esp_rtl_sdr** is built for a different goal:

> An ESP32 should **own, characterize, monitor, and adapt** an RTL dongle as a **native embedded peripheral** — not pretend it’s a PC running GQRX.

That means:

- A **stable C API** you can put next to FreeRTOS tasks and UI code  
- **Fail-closed** behavior (no “half-open USB” after a failed start)  
- **Capability flags** so apps don’t assume gain/bias/HF without CAP bits  

- **Health + metrics** (is USB starving? is the app too slow? RF clipping?)  
- **Rate passport** — probe which sample rates this **host + stick** actually sustain  
- **Intent presets** (`NEED_FM`, `NEED_ADSB`, …) so apps speak missions, not only registers  
- **Profiles** (Blog V4 first) so more dongles can be added without rewriting the core  

Board stuff (display, Ethernet, VBUS, audio) stays in **your** app. This component is the radio USB path only.

Longer vision: [`docs/VISION.md`](docs/VISION.md).

---

## Why this tries to be *better* on microcontrollers

Desktop drivers optimize for “open stick, set knobs, dump I/Q to the PC.”  
On a P4, the **host USB and RAM path is the hard part**. We lean into that:

| Desktop / sketch style | esp_rtl_sdr |
|---|---|
| Assume the USB bus always keeps up | Measure **effective SPS**, overruns, drops |
| Fixed rate list or “set and hope” | Windows + quantize + optional **on-device passport** |
| Call set_freq from anywhere | **Hot retune** drains bulk before EP0; **async from callbacks** |
| Opaque internals | Explicit **state machine**, error codes, reentrancy rules |
| “Supports every RTL” marketing | **Fail closed** on unknown sticks; one measured profile first |
| App figures out “is RF dead?” | **Health** narrative (USB / app / RF clip / weak) |

We are **not** chasing full librtlsdr feature parity (tuner IF filter still open). We are chasing a driver that is **safe to live inside a real FreeRTOS product**.

---

## What you need

| Item | Notes |
|---|---|
| **MCU** | **ESP32-P4** with High-Speed USB Host (e.g. M5Stack Tab5, Waveshare P4 kit) |
| **Dongle** | **RTL-SDR Blog V4** — USB **`0bda:2838`**, mfg/product strings `RTLSDRBlog` / `Blog V4` |
| **Tooling** | ESP-IDF **≥ 5.3** with `esp32p4` support |
| **Antenna** | For RF; compile/smoke works without RF |

**Not claimed yet:** ESP32-S2/S3 Full-Speed hosts, random eBay RTL sticks, production warranty.  
**Why that is intentional:** [`docs/SCOPE.md`](docs/SCOPE.md).

---

## Quick start — try the smoke app

Fastest path to “does it build and talk USB?”

```bash
git clone https://github.com/hardcoreerik/esp-rtl-sdr.git
cd esp-rtl-sdr/examples/p4_serial_smoke

idf.py set-target esp32p4
idf.py build
idf.py -p PORT flash monitor
```

Plug the Blog V4 into the P4 **USB Host** port (not the flash/UART port).

- **No dongle:** helpers + install should run; `start` → `NO_DEVICE` is OK.  
- **With dongle:** stream, optional `read()`, health logs, passport if the example runs it.

More: [`examples/p4_serial_smoke/README.md`](examples/p4_serial_smoke/README.md).

---

## How to use in your own project

### 1. Add the component

**Option A — path next to your project**

```cmake
# your project CMakeLists.txt (before project())
set(EXTRA_COMPONENT_DIRS "/path/to/esp-rtl-sdr")
```

Component name is the folder name of that path. Prefer the smoke example’s pattern (stable name `esp_rtl_sdr` via a thin wrapper under `examples/p4_serial_smoke/components/`).

**Option B — git submodule**

```bash
git submodule add https://github.com/hardcoreerik/esp-rtl-sdr.git components/esp_rtl_sdr
```

Then `REQUIRES esp_rtl_sdr` from your app component (folder name must match).

### 2. Minimal stream (async IQ blocks)

```c
#include "esp_rtl_sdr.h"

static void on_sdr(esp_rtl_sdr_event_t ev, const void *payload, void *ctx)
{
    if (ev == ESP_RTL_SDR_EVT_IQ_BLOCK) {
        const esp_rtl_sdr_iq_block_t *iq = payload;
        /* iq->data: CU8 interleaved I,Q — valid only until callback returns */
        (void)iq;
    }
    if (ev == ESP_RTL_SDR_EVT_RETUNED) {
        /* LO applied (including after async retune from a callback) */
    }
    /* Do not call start/stop/uninstall from here on the same handle */
}

void app_start_sdr(void)
{
    esp_rtl_sdr_config_t cfg;
    esp_rtl_sdr_config_default(&cfg);
    cfg.event_cb = on_sdr;

    esp_rtl_sdr_handle_t sdr = NULL;
    ESP_ERROR_CHECK(esp_rtl_sdr_install(&cfg, &sdr));

    /* Mission preset: preferred rate/LO (does not start streaming) */
    ESP_ERROR_CHECK(esp_rtl_sdr_apply_need(sdr, ESP_RTL_SDR_NEED_FM));

    /* Or explicit: */
    /* esp_rtl_sdr_set_center_freq(sdr, 100100000); */
    /* esp_rtl_sdr_set_sample_rate(sdr, ESP_RTL_SDR_RATE_960K); */

    ESP_ERROR_CHECK(esp_rtl_sdr_start_hz(sdr, 0, 0)); /* 0 = use preferred */

    /* Later: */
    /* esp_rtl_sdr_retune_hz(sdr, 162400000); */
    /* esp_rtl_sdr_stop(sdr, 0); */
    /* esp_rtl_sdr_uninstall(sdr); */
}
```

### 3. Sync pull (blocking `read`)

Works with or without an event callback:

```c
uint8_t buf[16384];
size_t n = 0;
esp_err_t err = esp_rtl_sdr_read(sdr, buf, sizeof(buf), 1000, &n);
if (err == ESP_OK && n > 0) {
    /* process n bytes of CU8 IQ */
}
```

### 4. Discover features before you assume them

```c
uint32_t caps = esp_rtl_sdr_get_capabilities();
if (caps & ESP_RTL_SDR_CAP_STREAM) { /* start/stop OK to attempt */ }
if (caps & ESP_RTL_SDR_CAP_GAIN) {
    /* 0.7.5+: manual gain after start */
}
if (caps & ESP_RTL_SDR_CAP_GAIN_AUTO) {
    /* 0.7.8+: set_tuner_gain_mode(AUTO) — measured Tuner AGC */
}
```

**Full API reference (params, returns, examples):** [`docs/API_REFERENCE.md`](docs/API_REFERENCE.md)  
Design contract: [`docs/API.md`](docs/API.md) · header: [`include/esp_rtl_sdr.h`](include/esp_rtl_sdr.h).

---

## API feature map (what you can call today)

### Lifecycle

| API | Purpose |
|---|---|
| `esp_rtl_sdr_config_default` / `config_validate` | Safe defaults + checks |
| `esp_rtl_sdr_install` / `uninstall` | Create / destroy handle + USB client |
| `esp_rtl_sdr_start` / `start_hz` / `stop` | Stream on/off |
| `esp_rtl_sdr_reset` | Clear FAULT → IDLE when not streaming |
| `esp_rtl_sdr_get_state` / `state_to_name` | `IDLE` · `STARTING` · `STREAMING` · … |

### Tuning & rate

| API | Purpose |
|---|---|
| `set/get_center_freq` · `retune_hz` | LO (async if called from event callback) |
| `set/get_sample_rate` · `quantize_sample_rate` | Allowlisted windows → **exact** programmed SPS |
| `is_rate_supported` · `get_supported_rates` | Policy / UI lists |
| `set/get_freq_correction` | Software ppm LO offset (±200) |
| `apply_need` | `NEED_FM` · `NEED_ADSB` · `NEED_WX` · `NEED_HF` · `NEED_MAX_STABLE` · `NEED_LISTEN` |
| HF LO map | RF &lt; 28.8 MHz → tuner RF+28.8 MHz (`CAP_HF_UPCONVERTER`, **0.7.7+**) |

### Data path

| API / event | Purpose |
|---|---|
| `EVT_IQ_BLOCK` | Async CU8 I/Q (borrowed pointer) |
| `esp_rtl_sdr_read` | Blocking sync pull ring |
| Format | **Unsigned interleaved I/Q** `(I0,Q0,I1,Q1,…)` |

### Observability (MCU-native extras)

| API / event | Purpose |
|---|---|
| `get_metrics` | Bytes, blocks, overruns, consumer drops, effective SPS, sample min/max |
| `get_health` · `EVT_HEALTH` | USB starving / app slow / RF clip / weak + short advice |
| `probe_rates` · `get_rate_passport` | On-host rate stability matrix |

### Multi-device

| API | Purpose |
|---|---|
| `refresh_device_list` · `get_device_count` · `get_device_at` | Candidates |
| `select_device` · `select_device_serial` | Choose Blog V4 by index/serial |

### Gain, bias & HF (0.7.5–0.7.8 Blog V4)

| API | Today |
|---|---|
| `set/get_tuner_gain` · `get_tuner_gains` | Manual ladder 0.0…49.6 dB (CAP_GAIN); need claimed stream |
| `set_tuner_gain_mode(MANUAL\|AUTO)` | MANUAL ladder; AUTO measured R828D AGC (`CAP_GAIN_AUTO`, 0.7.8+) |
| `set/get_rtl_agc` | RTL2832 digital AGC (`CAP_RTL_AGC`); not tuner AUTO |
| `set/get_bias_tee` | Measured SYS EP0 (CAP_BIAS_TEE); need claimed stream |
| HF (0.7.7) | **500 kHz…1766 MHz**; RF&lt;28.8 MHz uses +28.8 MHz upconverter (`CAP_HF_UPCONVERTER`) |

Evidence: [`docs/PHASE3_CAPTURE_REPORT.md`](docs/PHASE3_CAPTURE_REPORT.md), [`docs/AGC_IF_CAPTURE.md`](docs/AGC_IF_CAPTURE.md). P4 RF soak of HF FE / AUTO still open.

### Typical rates (macros)

| Macro | Use |
|---|---|
| `ESP_RTL_SDR_RATE_960K` | FM-class continuous (provenance path) |
| `ESP_RTL_SDR_RATE_2048K` | ADS-B-class (provenance path) |
| Others in allowlist | See [`docs/RATES.md`](docs/RATES.md); low band starts at **225001 Hz** |

---

## Lifecycle (mental model)

```text
install → IDLE
            │ start()
            ▼
         STARTING ──fail──► IDLE / FAULT
            │ ok
            ▼
         STREAMING ←── retune_hz / set_center_freq (queued if from callback)
            │ stop()
            ▼
          IDLE
            │ uninstall()
            ▼
        destroyed
```

**Rules of thumb**

1. One owner task for `uninstall`.  
2. Event callbacks: OK to call `retune_hz` / `get_*` / `read` carefully; **don’t** `start`/`stop`/`uninstall` on the same handle.  
3. Check `get_capabilities()` before assuming advanced features.  
4. Prefer `config_default` + set fields; `struct_size` supports append-only growth.

---

## Honest status (short)

| Works | Not yet |
|---|---|
| Blog V4 stream + retune + metrics on P4 | Tuner IF / SDR# Bandwidth EP0 (USB-silent) |
| Continuous rates + passport API | “Any RTL2832U” |
| Manual gain + bias CAP (0.7.5+) | Formal multi-hour soak artifact from *this* repo only |
| HF upconverter CAP (0.7.7) | IF / channel filter EP0 |
| CI: host tests + **P4 compile** of smoke app | librtlsdr drop-in ABI |
| Clean-room tables | |

Details: [`PROJECT_TRUTH.md`](PROJECT_TRUTH.md) · story: [`docs/DEVELOPMENT_NARRATIVE_0_7.md`](docs/DEVELOPMENT_NARRATIVE_0_7.md).

---

## Project credits

Created and maintained by [Erik / hardcoreerik](https://github.com/hardcoreerik), with AI-assisted development (Claude Sonnet, OpenAI Codex, Grok Build) — same honesty model as [TheOrc](https://github.com/hardcoreerik/TheOrc).

See [`docs/AI_DEVELOPMENT_DISCLOSURE.md`](docs/AI_DEVELOPMENT_DISCLOSURE.md).

---

## Documentation

| Doc | Purpose |
|---|---|
| [PROJECT_TRUTH.md](PROJECT_TRUTH.md) | What is true **now** |
| [docs/API_REFERENCE.md](docs/API_REFERENCE.md) | Full API reference (params / returns / examples) |
| [docs/API.md](docs/API.md) | Design contract (invariants) |
| [docs/EXAMPLES.md](docs/EXAMPLES.md) | Usage recipes |
| [docs/TROUBLESHOOTING.md](docs/TROUBLESHOOTING.md) | Common failures |
| [docs/KCONFIG.md](docs/KCONFIG.md) | menuconfig |
| [docs/SCOPE.md](docs/SCOPE.md) | Why P4 + Blog V4 only (for now) |
| [docs/SOAK.md](docs/SOAK.md) | Hardware soak procedure |
| [docs/VISION.md](docs/VISION.md) | Product direction |
| [architecture.md](architecture.md) | Layering & threading |
| [docs/RATES.md](docs/RATES.md) | Rate windows + passport |
| [docs/TESTING_GUIDE.md](docs/TESTING_GUIDE.md) | Automated tests + CI |
| [docs/LAB_HOBBYIST.md](docs/LAB_HOBBYIST.md) | Desk lab + TinySA |
| [docs/DRIVER_GAPS_VS_DESKTOP.md](docs/DRIVER_GAPS_VS_DESKTOP.md) | Gaps vs SDR# / librtlsdr-class tools |
| [docs/README.md](docs/README.md) | Full index |
| [CHANGELOG.md](CHANGELOG.md) | Releases |
| [CONTRIBUTING.md](CONTRIBUTING.md) | PRs / clean-room |
| [LICENSING.md](LICENSING.md) | AGPL + commercial process |

---

## Ecosystem

- **[OrcSDR](https://github.com/hardcoreerik/OrcSDR)** — ESP32-P4 SDR application (optional consumer)  
- **[TheOrc](https://github.com/hardcoreerik/TheOrc)** — local-first multi-agent tools (truth culture)  
- **This repo** — driver only; builds without OrcSDR  

---

## License

**AGPL-3.0-only** by default. Commercial licensing available — [LICENSING.md](LICENSING.md).

---

**Try it. Break it. Open a `truth:` issue if we oversold something.**  
https://github.com/hardcoreerik/esp-rtl-sdr
