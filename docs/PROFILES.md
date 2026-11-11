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

## Quick reference — implemented receiver profiles

All **five** names below are implemented profiles in the [current source registry](../private/rtl_profile.hpp). Physical verification is **model- and revision-specific**; “implemented” is not “calibrated on every board”.

| Device / tuner | Driver ID | Document | Dated evidence at a glance |
|---|---|---|---|
| RTL-SDR Blog V4 / R828D | `blog_v4_r828d` | **[V4 engineering profile](BLOG_V4_PROFILE.md)** | Tab5 FM/bandwidth, V4 HF FT8, PC captures |
| RTL-SDR Blog V4 Lite / R828S | `blog_v4l_r828s` | **[V4L engineering profile](BLOG_V4L_PROFILE.md)** | Tab5 IQ/bandwidth/hotplug, HF FT8, PC AM |
| Blog V3 family / R820T2 or R860 | `blog_v3_r820t2` | **[V3/V3c engineering profile](BLOG_V3_PROFILE.md)** | **V3c** FM/RDS, Q-HF, Tab5 hotplug; earlier V3 revisions individually verify |
| Nooelec NESDR SMArt V5 / R820 family | `nooelec_smart_v5_r820t2` | **[SMArt V5 engineering profile](NOOELEC_V5_PROFILE.md)** | Standalone Tab5 smoke, band/stream/hotplug, 7.074 MHz FT8 |
| Nooelec XTR / Elonics E4000 | `e4000_experimental` (historical name) | **[E4000/XTR engineering profile](E4000_PROFILE.md)** | Tab5 FM/RDS, multi-rate IQ, offset and bounded lifecycle tests; RF/gain/long-soak gates remain |

Per-model capabilities come from `esp_rtl_sdr_get_device_capabilities()`, **not** the library-wide capability union; the source registry is the authority. See [status terminology](STATUS_TERMINOLOGY.md), [v0.9.6 release](releases/v0.9.6.md), [full API](API_REFERENCE.md).

## Active profiles

### `e4000_experimental` (internal ID; Nooelec XTR / E4000) — IMPLEMENTED; EXTENDED ACCEPTANCE OPEN

| Field | Value |
|---|---|
| Status | **Implemented; ESP32-P4 hardware-tested**, including FM stereo/RDS, IQ at multiple rates, offset-tuning and bounded lifecycle cycles on named candidates. Broad RF/gain calibration, long soak and a complete current-release matrix remain open. Internal profile name is historical, not a marketing status label. |
| USB | Exact `0bda:2838` + `Nooelec` / product contains `SMArTee XTR v5`; other shared-ID descriptors require E4000 register-0x02 ID `0x40`. Other VID/PID pairs are not accepted yet |
| Tuner | E4000 at seven-bit I2C address `0x64` (USB bridge encoded `wValue=0x00c8`) |
| Clock / RF | Assumes a 28.8 MHz tuner reference; published E4000 range **64 MHz–1.7 GHz** |
| Path | Shared RTL2832U demodulator and IQ streaming; E4000-specific zero-IF tuning and RF filter selection |
| Caps | STREAM/RETUNE, manual tuner gain (`CAP_GAIN`, 14 nominal steps including -1 dB) and tuner AUTO (`CAP_GAIN_AUTO`) are implemented, along with RTL digital AGC (`CAP_RTL_AGC`). Tuner bandwidth, direct sampling, HF upconverter and **software-switchable** bias tee are unavailable; the reported XTR hardware bias tee is not a controllable driver capability. |
| Evidence | Host profile/PLL/actual-tune checks; first-party PC captures; Tab5 2.4 MS/s FM/KZEL RDS and 60.71-second stream; separately recorded 960 kS/s and 3.2 MS/s IQ, six COM39 offset IQ captures and bounded reconnect/hotplug cycles; targeted gain-history comparison. Candidate/date and external SDK patch boundaries apply. See [E4000 profile](E4000_PROFILE.md), [completion audit](validation/E4000_REMAINING_GATES.md) and [gain controls](E4000_GAIN_CONTROLS.md). |

The profile deliberately bypasses the R828D/R820-family initialization, tune tables, and the two R828D gain writes embedded in the shared sample-rate slice. It uses a fixed, conservative IF-filter configuration and a captured 14-step manual gain ladder plus tuner AUTO. The E4000 tuner clock is an assumption until confirmed on hardware; divider fallbacks outside the datasheet's nominal VCO range are logged.

### `blog_v4` (RTL-SDR Blog V4 / R828D) — PRIMARY

| Field | Value |
|---|---|
| Status | **Implemented** (tables in-tree); HF route composition from 0.7.15 / PR #18 |
| USB | VID `0x0BDA` PID `0x2838` + exact `RTLSDRBlog` / `Blog V4` |
| Tuner | R828D @ I2C `0x74` |
| HF | RF<28.8 MHz -> LO+28.8e6; RF<=28.8 MHz -> Cable-2 + GPIO5-low |
| Caps | STREAM, HF_UPCONVERTER, GAIN, GAIN_AUTO, RTL_AGC, TUNER_BANDWIDTH and switchable BIAS_TEE; **no DIRECT_SAMPLING or OFFSET_TUNING** |
| Hardware | Blog V4 has Tab5 FM/RDS and 7.074 MHz FT8 acceptance, plus observed band-select/retune cycles; analog gain calibration, GPIO electrical measurements and all-band soak are separate evidence tasks. |
| Tuner bandwidth | Live bandwidth AUTO, 200k, 300k, 500k, 1.0M, 1.8M, 2.4M, AUTO **hardware-tested** on the M5 Tab5 at 96.1 MHz (2026-09-28, with the read-after-write fix: one cold boot and one unplug/replug): the carrier stays within +/-1.4 kHz of the request at every stage and final AUTO matches the cold-boot passband within 0.4 dB. **Not measured before the fix**; the V4L, which shares these plan words, shifted -312..+386 kHz without it. Plan values are vendor-driver control choices derived from captures, not measured analog passbands. See `docs/captures/v3c_live_bandwidth_2026-09-28.md`. |

### `blog_v4l` (RTL-SDR Blog V4 Lite / R828S) — IMPLEMENTED; TAB5 TESTED

V4L has its own accepted device profile with a 28.8 MHz HF upconverter route, native tuner bandwidth/gain controls and optional 24–28.8 MHz direct tuner route. Tab5 streaming, frequency changes, bandwidth cycling, hotplug and 7.074 MHz FT8 reception have been exercised; the radio's analog filters, exact sensitivity and long-soak behavior are not universally characterized. See [OrcSDR receiver evidence](https://github.com/hardcoreerik/OrcSDR/blob/main/PROJECT_STATUS.md).

### `nooelec_smart_v5` (NESDR SMArt V5 / R820-family) — IMPLEMENTED; TAB5 TESTED

| Field | Value |
|---|---|
| Status | **Implemented** from first-party PC captures; standalone P4 streaming/control passed on 2026-10-29 and integrated OrcSDR 7.074 MHz FT8 reception was accepted 2026-10-09. Per-band/calibration and long-soak claims remain separate. |
| USB | Shared `0bda:2838` + exact `Nooelec` + product contains `NESDR SMArt v5` |
| Tuner | R820-family @ I2C `0x34`; the PC oracle reports tuner type 5 / R820T, which does not establish exact silicon. Nooelec's public datasheet names R860. |
| RF / HF | Model bounds **100 kHz–1750 MHz**. Automatic Q-branch direct sampling below **24 MHz**, native tuner above; no V4 upconverter, triplexer, or board GPIO. The 24 MHz route cutoff matches the PC capture, whose boundary tune warned that the PLL was not locked; the manufacturer rates native operation from 25 MHz. No RF acceptance claim at 24 MHz. |
| Caps | STREAM/RETUNE/etc. plus DIRECT_SAMPLING, GAIN, GAIN_AUTO, RTL_AGC, TUNER_BANDWIDTH. **No BIAS_TEE or HF_UPCONVERTER.** Gain/mode and tuner bandwidth requests are unsupported while Q bypasses the tuner. |
| Cold IF | Captured tuner filter `0x0a/0x0b = d3/6b`; PLL IF **3.570 MHz**, demod IF bytes **38 11 12**. Reinitialize and restore these after sample-rate setup and when returning from Q to the tuner. |
| Bandwidth | At **2.4 MS/s**, AUTO, 200k, 300k, 500k, 1.0M, 1.8M and 2.4M are captured native-route control choices. Every plan uses `0x0a=c3` with paired PLL/demod IF and settle reads. Explicit AUTO uses **c3/8f and 1.815 MHz**; it does not inherit the older V3c AUTO boot-state policy. Ordinary startup/retune stays at cold 3.570 MHz until a bandwidth request is applied. Analog passbands are unmeasured. |
| Gain / AGC | All **29 nominal 0.0–49.6 dB** manual register pairs match the existing R820-family ladder. Nooelec tuner AUTO preserves the current gain nibbles and changes mode bits, including after reinit. RTL AGC writes page-0 register 19 (`05` off / `25` on) and performs the captured settle read. |
| Evidence | [Nooelec capture record](captures/nooelec_v5_2026-09-30.md) contains identity, hashes, procedures and frame anchors; [merge and acceptance notes](nooelec_v5_merge_notes.md) separate source, build and hardware gates. David Coulson's earlier independent testing flagged an IF mismatch ([PR #26](https://github.com/hardcoreerik/esp-rtl-sdr/pull/26)); that historical report is retained as attribution, while this implementation uses the new first-party captures. |

### `blog_v3` (RTL-SDR Blog V3 / V3c / R820T2 / R860) — IDENTIFICATION, STREAMING, AND MATCHED-IF TUNING VERIFIED

| Field | Value |
|---|---|
| Status | Identification, streaming, and 3.570 MHz matched-IF tuning **hardware-verified** (2026-09-11/12, real V3c unit, R860 tuner per packaging). Physical checks covered 96.1 MHz, 99.1 MHz with matching RDS, cold start, hot retune, V3c/V4 hotplug in both directions, USB-powered boot, battery-powered boot, and the Blog V4 regression. Gain accuracy remains provisional. |
| USB | Exact V3 descriptors, or completed R820T2 chip-id `0x96`/`0x69` on ambiguous `0bda:2838` (the tested V3c unit reports the bare factory `RTL2838UHIDIR` descriptor, not `RTLSDRBlog`/`Blog V3` — identified via the ambiguous-descriptor chip-id probe, not string match) |
| Tuner | R820T2/R860 @ I2C `0x34` (same USB IR template addressing as Nooelec; independently measured board differences remain profile-specific) |
| HF/LF | Below 24 MHz uses first-party-capture-derived RTL2832 Q-branch direct sampling; **no** V4 HF upconverter / Cable-2 / GPIO5 |
| Caps | Current registry exposes STREAM/RETUNE, nominal GAIN, GAIN_AUTO, RTL_AGC, BIAS_TEE, DIRECT_SAMPLING and TUNER_BANDWIDTH; **no HF_UPCONVERTER or OFFSET_TUNING**. The tested V3c has captured PC gain/AGC and no-load bias observations, but ambiguous R820-family identity does **not** validate a bias circuit on every earlier V3 revision. Tuner gain and bandwidth setters are unsupported while Q direct sampling bypasses the tuner. |
| IF evidence | Official-driver capture measured the V3c PLL IF at 3.570 MHz and ended RTL2832 setup with `0x19/0x1A/0x1B = 0x38/0x11/0x12`, including a settle read after each write. The driver restores that sequence after sample-rate setup and before the first tune. When tuner bandwidth returns to AUTO it restores the whole V3c boot tuning state: demod IF `38 11 12`, PLL IF 3.570 MHz and tuner filter registers `0x0a/0x0b = d5/6b` (read back from the chip after a cold boot). The explicit bandwidth plans use their own captured IF (2.125, 2.025, 1.700, 1.750 or 1.815 MHz) and are written from the same plan; the V3c therefore runs a variable IF by design. V4 stays on its existing matched 1.814972 MHz path; Nooelec separately matches its PLL offset to the 3.57 MHz IF at cold start; standalone Tab5 streaming/control is now tested, while a directly paired physical before/after IF-offset experiment and broad calibration remain open. |
| Evidence | Probe recovered from `agent/blog-v3-profile` / `d870740`. Identification/streaming verified via repeated cold-boot and hot-swap testing (V4 ↔ V3-family, both directions) with zero crashes. The matched-IF fix passes both host suites, truth hygiene, ESP-IDF 5.5.4 ESP32-P4 compile, and physical V3c/V4 tuning acceptance. Manual-gain calibration remains open. |
| Tuner bandwidth | Live bandwidth AUTO, 200k, 300k, 500k, 1.0M, 1.8M, 2.4M, AUTO **hardware-tested** on the M5 Tab5 at 96.1 MHz (2026-09-28): the carrier stays within +2.0 kHz of the request at every stage across seven runs (six cold boots and one cold boot followed by an unplug/replug), final AUTO matches the cold-boot passband within 0.8 dB in all but one run (that run: 3.6 dB, repeated cleanly), no USB overruns or IQ drops. Plan values are vendor-driver control choices derived from captures, not measured analog passbands. See `docs/captures/v3c_live_bandwidth_2026-09-28.md`. |

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
