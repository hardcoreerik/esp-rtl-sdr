# Device profiles — esp_rtl_sdr

## Concept

A **profile** is a measured package of identity rules + USB control sequences +
tuner policy for one dongle class. The core host client stays shared.

**esp_rtl_sdr is becoming a general RTL2832U-class SDR driver.** These dongles
share the RTL2832U USB chip; profiles capture board/tuner differences (Blog V4
R828D + HF routing vs R820T2/R860 without V4 HF). Capability-driven —
**no user picker**.

**Identity is not tuner family is not board front-end.** Sharing an R820T2/R860
I2C address does not imply the same init, GPIO, or HF path.

Plug-and-play: the driver selects the profile; apps read
`esp_rtl_sdr_get_profile()` and `esp_rtl_sdr_get_device_capabilities()`.

Default until identified (and after detach): **`Unknown`** — never Blog V4.

## Active profiles

### `blog_v4` (RTL-SDR Blog V4 / R828D) — PRIMARY

| Field | Value |
|---|---|
| Status | **Implemented** (tables in-tree); HF route composition from 0.7.15 / PR #18 |
| USB | VID `0x0BDA` PID `0x2838` + exact `RTLSDRBlog` / `Blog V4` |
| Tuner | R828D @ I2C `0x74` |
| HF | RF<28.8 MHz -> LO+28.8e6; RF<=28.8 MHz -> Cable-2 + GPIO5-low |
| Caps | Full library set including STREAM, HF_UPCONVERTER, GAIN, BIAS_TEE |
| Hardware | Maintainer can soak Blog V4; GPIO/RF acceptance still open |

### `nooelec_smart_v5` (NESDR SMArt v5 / R820T2-R860) — PROVISIONAL

| Field | Value |
|---|---|
| Status | **Provisional** — contributor-tested; maintainer soak pending |
| USB | Shared `0bda:2838` + exact `Nooelec` + product contains `NESDR SMArt v5` |
| Tuner | R820T2/R860 @ I2C `0x34` (mapped from Blog V4 IR records) |
| HF | **Rejected** below 24 MHz; no V4 HF routing |
| Caps | STREAM/RETUNE/etc. without HF_UPCONVERTER / GAIN / BIAS_TEE |
| Evidence | Contributor/OrcSDR tester reports; clean-room remap only; community soak welcome |

### `blog_v3` (RTL-SDR Blog V3 / V3c / R820T2 / R860) — IDENTIFICATION+STREAM VERIFIED, TUNING PROVISIONAL

| Field | Value |
|---|---|
| Status | Identification and streaming **hardware-verified** (2026-09-11, real V3c unit, R860 tuner per packaging) after the demod-bring-up fix (see CHANGELOG). Actual RF tuning/gain accuracy **still provisional / not Hardware-verified** — community soak wanted |
| USB | Exact V3 descriptors, or completed R820T2 chip-id `0x96`/`0x69` on ambiguous `0bda:2838` (the tested V3c unit reports the bare factory `RTL2838UHIDIR` descriptor, not `RTLSDRBlog`/`Blog V3` — identified via the ambiguous-descriptor chip-id probe, not string match) |
| Tuner | R820T2/R860 @ I2C `0x34` (same USB IR template remap as Nooelec provisional; R860 is pin/register-compatible with R820T2, same profile covers both — no separate profile needed) |
| HF | **Rejected** below 24 MHz; **no** V4 HF upconverter / Cable-2 / GPIO5 |
| Caps | STREAM/RETUNE/etc. without HF_UPCONVERTER / GAIN / BIAS_TEE |
| Evidence | Probe recovered from `agent/blog-v3-profile` / `d870740`. Identification/streaming verified via repeated cold-boot and hot-swap testing (V4 ↔ V3-family, both directions) with zero crashes after the demod-bring-up fix. Tune/gain register math still **not** Hardware-verified — no unique V3/R860 tables measured yet, shared R820T2/Nooelec remap only; static-with-no-station-lock observed on real hardware. Next: PLL register readback capture vs. Rafael Micro's public R820T2 datasheet formula |

## Fail closed

- Unknown / non-matching `0bda:2838` -> not accepted (not Blog V4).
- Do not claim interface half-way on reject.
- Hotplug detach clears profile, caps, and V4 front-end shadows.

## Profile checklist (new dongle class)

1. Record USB descriptor strings and VID/PID.
2. Capture full init + one tune + one rate change + cleanup.
3. Note expected STALLs (if any) with indices.
4. Implement profile module; do **not** reuse another board's front-end blindly.
5. Soak on ESP32-P4 HS host.
6. Document here + `PROJECT_TRUTH.md` with honest evidence labels.
