# Test lab & RF fixtures

Authoritative list of **bench gear available to this project** for driver and
app validation. Update when gear changes.

## Hosts (SDR under test)

| Host | Role |
|---|---|
| ESP32-P4 M5Stack Tab5 | Provenance continuous IQ (OrcSDR) |
| ESP32-P4 Waveshare Module-DEV-KIT | Second-board Blog V4 (OrcSDR shell) |
| Stand-alone `examples/p4_serial_smoke` from this repo | Driver-only re-soak (open) |

## RTL-SDR under test

| Device | Notes |
|---|---|
| RTL-SDR Blog V4 (`0bda:2838`) | Primary profile; R828D |

## Companion / stimulus devices

These are **not** part of the USB driver tree; they generate or observe RF so
we can validate LO, bandwidth, demod, and regressions without guessing.

| Device | Role in testing |
|---|---|
| **2× Heltec V4** (LoRa) | Known-good digital RF sources/sinks; prior project work already used them for **LoRa decode** experiments. Useful for controlled on-air packets near 433/868/915 (region-dependent) when validating SNR/gain and spectrum occupancy near the stick. |
| **Baofeng UV-5R** | Cheap dual-band HT: FM voice and simple keyed carriers on 2 m / 70 cm for **FM path** checks (audio demod in apps), rough LO sanity, and “is the front-end alive?” smoke. Observe local regulations; keep TX power/legal limits. |
| **Flipper Zero** | Sub-GHz / general RF Swiss army: generate test tones, replay captures, or act as a **known interferer** when testing desense, notches, and health/clip heuristics. Also useful for NFC/IR later if apps care — out of scope for pure IQ driver. |

## Suggested exercises (driver-relevant)

| Goal | Method |
|---|---|
| Rate passport | `probe_rates` on P4 + Blog V4; log best_stable |
| Continuous rate | `set_sample_rate(1536000)` etc.; confirm exact + stream |
| NEED_ADSB | `apply_need(ADSB)` + start; optional live aircraft |
| NEED_FM / LISTEN | Baofeng or broadcast FM; app demod |
| NEED_WX | NOAA WX if in range |
| Health RF_WEAK / CLIP | Flipper/Baofeng near-field vs distant; watch `get_health` |
| LoRa adjacent | Heltec TX while stick on nearby freq — desense / passport stability |

## Honesty

- Companion radios do **not** replace USB capture for gain/bias EP0 evidence.
- TX into the Blog V4 without attenuation can damage the front-end — use
  distance, attenuators, or low power.
- Legal: only transmit where licensed / permitted.

## Related

- `Project_truth.md` — host matrix
- `docs/VISION.md` — passport / health product goals
- `docs/RATES.md` — rate windows
