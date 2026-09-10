# Device profiles — esp_rtl_sdr

## Concept

A **profile** is a measured package of identity rules + USB control sequences +
tuner policy for one dongle class. The core host client stays shared.

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
| Status | **Provisional / not maintainer-verified** |
| USB | Shared `0bda:2838` + exact `Nooelec` + product contains `NESDR SMArt v5` |
| Tuner | R820T2/R860 @ I2C `0x34` (mapped from Blog V4 IR records) |
| HF | **Rejected** below 24 MHz; no V4 HF routing |
| Caps | STREAM/RETUNE/etc. without HF_UPCONVERTER / GAIN / BIAS_TEE |
| Evidence | Contributor/OrcSDR tester reports; clean-room remap only |

### `blog_v3` (RTL-SDR Blog V3 / R820T2) — IDENTITY ONLY

| Field | Value |
|---|---|
| Status | **Experimental / unverified** — identity probe only |
| USB | Exact V3 descriptors, or completed R820T2 chip-id `0x96`/`0x69` on ambiguous `0bda:2838` |
| Tuner | Probe only; **no** init table |
| Caps | **CAP_STREAM false**; `start` -> `ERR_UNSUPPORTED` |
| Evidence | No physical V3 capture in this tree |

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
