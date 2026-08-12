# Vision — Dongle Nervous System

**esp_rtl_sdr is not a librtlsdr port.** Desktop drivers are a PC IQ faucet:
open → set a few knobs → async callback. They do not know whether *this host*
can sustain a rate, do not self-tune USB pipes, and do not speak in missions.

This driver’s unfair advantage is being born on the **host that is the
bottleneck** (ESP32-P4 HS USB + dual core + metrics).

## Thesis

> librtlsdr is a remote control for a TV demod used as an ADC.  
> **esp_rtl_sdr should be a nervous system:** intent in, health + passport out,
> silicon blocks as voluntary muscles — measured, fail-closed, MCU-native.

## Mental model

```text
App speaks NEED / INTENT / HEALTH
        │
        ▼
Driver owns PASSPORT (learned on this board)
  · rates that work *here*
  · gain ceiling before clip (when measured)
  · USB URB sweet spot (planned)
  · ppm from beacon (planned)
        │
        ▼
Profile owns Blog V4 measured EP0 (clean-room)
```

## Layers beyond sample rate

Sample rate is one resampler field. The broader map (see `docs/SILICON.md`):

| Layer | Examples |
|---|---|
| Intent | `apply_need(FM|ADSB|WX|HF|MAX_STABLE|LISTEN)` |
| Passport | `probe_rates` → best stable SPS on **this** P4 + stick |
| Health | USB starving / app slow / RF clip / RF weak + advice string |
| RF chain | gain stages, bias-T, R828D inputs, IF BW (measured later) |
| Host USB | multi-URB self-tune (planned) |
| World cal | beacon ppm (ADS-B / NOAA / FM pilot) (planned) |

## What “more than librtlsdr” means here

| Desktop gap | Our answer |
|---|---|
| No host sustainability knowledge | Rate passport on-device |
| Fixed URB policy | Adaptive URB (planned) |
| Register-centric API | `need()` missions |
| Metrics optional | First-class health narrative |
| V4 special-cases in forks | Profile + honest CAP bits |
| GPL port gravity | Clean-room EP0 + public DS as *reference*, never source paste |

## Build order (beyond rates)

| Step | Status |
|---|---|
| A — `apply_need()` intent presets | **Implemented** (0.7) |
| B — Passport soak skeleton | **Implemented** (0.7) |
| C — Health events / `get_health` | **Implemented** (0.7 get; EVT optional from app poll) |
| D — Measured gain / bias (Phase 3) | Planned |
| E — R828D stage gain + IF BW + input | Planned |
| F — Adaptive USB URB + auto rate downshift | Planned |
| G — Beacon ppm learn | Planned |

## Continuous rates (subordinate to passport)

Any rate in hardware windows is accepted after quantize; recommended list remains
for discovery. See `docs/RATES.md`. Passport decides what is *stable here*.

## Clean-room

Authority order: (1) our measured Blog V4 captures, (2) RTL2832U DS blocks,
(3) R820T2 register PDF, (4) Linux `rtl28xxu` map, (5) Blog V4 product DS,
(6) librtlsdr **behavior only** — never copied source.
