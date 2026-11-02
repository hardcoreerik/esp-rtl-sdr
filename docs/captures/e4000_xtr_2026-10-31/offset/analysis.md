# E4000 offset tuning capture analysis

Date: 2026-10-31. Scope: read-only analysis of `events.json`, `control-transfers.txt`, and the nine `.cu8` captures under this directory. No hardware access, vendor source, disassembly, or repository edits were used.

## Capture integrity and setup

- Device: Nooelec SMArt XTR v5, serial `82115932`; events recorded open/close and every requested sample-rate, center-frequency, and offset change with `rc=0` and matching offset readback.
- Setup per capture note and task: outside 20 m antenna through Ham It Up passthrough; no calibrated tone.
- USB log header: 939 paired control transfers, 0 unpaired submits; all 939 statuses are `0x00000000`.
- Three runs are OFF → ON → OFF at 96.1 MHz for 960 kS/s, 2.4 MS/s, and 3.2 MS/s. Each IQ file is 524,288 bytes (262,144 complex CU8 samples). All nine SHA-256 values match `events.json`.
- Inputs: `events.json` SHA-256 `21e4873695b87ea88a325c88fe59bb495a6c778696a7c82753a0f24e1348e942`; `control-transfers.txt` SHA-256 `723e9eef2e6c3acab9067d41c8832fd132588fd8498b04135f5f190adb5dba3b`.

## Register-derived LO and effective IQ center

The PLL decode uses the first-party clean-room E4000 fractional-N equation and register mapping. The capture shows `r0d=0x0d`; under that mapping, bit `0x08` selects three-phase operation and low divider code `5` selects `/32` from `{4,8,12,16,24,32,40,48}`. `r09` is integer `Z`; `r0b:r0a` is fractional `X`. The current clean-room planner may choose `/40` at 96.1 MHz as another nominal-range plan; the reference capture's raw `r0d` selects `/32`.

Equations:

- `LO = (Z + X/65536) * 28,800,000 / 32`, with `X = (r0b << 8) | r0a`.
- IF NCO word `r19:r1a:r1b` is signed 22-bit; `IF = signed_word * 28,800,000 / 2^22`.
- Effective center is `LO - IF`; equivalently, at the requested RF center the residual is `96,100,000 - LO + IF`.

| Rate | State | PLL r09/r0a/r0b/r0d | LO from PLL (Hz) | NCO word | Signed NCO (Hz) | Effective center (Hz) |
|---:|---|---|---:|---|---:|---:|
| 960 kS/s | OFF | `6a/1c/c7/0d` | 96,099,993.896 | `000000` | 0 | 96,099,993.896 |
| 960 kS/s | ON | `69/01/df/0d` | 95,283,998.108 | `3e2fca` | −815,995.789 | 96,099,993.896 |
| 2.4 MS/s | OFF | `6a/1c/c7/0d` | 96,099,993.896 | `000000` | 0 | 96,099,993.896 |
| 2.4 MS/s | ON | `68/d8/82/0d` | 94,059,997.559 | `3b7778` | −2,039,996.338 | 96,099,993.896 |
| 3.2 MS/s | OFF | `6a/1c/c7/0d` | 96,099,993.896 | `000000` | 0 | 96,099,993.896 |
| 3.2 MS/s | ON | `67/6c/c1/0d` | 93,379,998.779 | `39f4a0` | −2,719,995.117 | 96,099,993.896 |

The ON LO is below the request by about `0.85 × sample rate`; the signed IF word is negative with matching magnitude. Their composition returns the register-derived effective center to 96,099,993.896 Hz at every rate, a +6.104 Hz quantization residual from the 96.1 MHz request. This verifies the programmed sign and arithmetic across all three rates. It does not establish an independently measured RF tone or calibration.

## Final tuner filter-register states

Final values per OFF → ON → OFF state (registers written through the E4000 I2C path):

| Rate | `r11` | `r12` |
|---:|---|---|
| 960 kS/s | `ff → fd → ff` | `1f → 1f → 1f` |
| 2.4 MS/s | `ec → 9a → ec` | `1a → 07 → 1a` |
| 3.2 MS/s | `cb → 89 → cb` | `0e → 00 → 0e` |

At 960 kS/s, `r12=0x1f` came from open/setup and was not rewritten during the offset toggles. At 2.4 and 3.2 MS/s, both registers changed for ON and returned to the rate-specific OFF values. This shows register restoration, not the RF response of each filter setting.

## IQ observations

Method: decoded interleaved unsigned CU8 as centered complex samples (`I,Q − 127.5`); averaged four non-overlapping 65,536-sample Hann periodograms per file. Resolution is 14.65, 36.62, and 48.83 Hz/bin at 960 kS/s, 2.4 MS/s, and 3.2 MS/s respectively. Peak excess is peak power within ±2 kHz divided by median per-bin power from 2–20 kHz; it is local contrast, not calibrated power.

| Rate | Sample/state | Strongest bin within ±2 kHz | Peak excess over local floor | Mean complex magnitude (CU8 counts) | ±2 kHz average excess |
|---:|---|---:|---:|---:|---:|
| 960 kS/s | `iq-960000-0-6.cu8` OFF | +1.582 kHz | 7.9 dB | 1.05 | 3.0 dB |
| 960 kS/s | `iq-960000-1-9.cu8` ON | −1.274 kHz | 10.4 dB | 0.51 | 5.1 dB |
| 960 kS/s | `iq-960000-0-12.cu8` OFF | +1.831 kHz | 15.2 dB | 0.85 | 9.4 dB |
| 2.4 MS/s | `iq-2400000-0-17.cu8` OFF | +0.256 kHz | 9.0 dB | 1.55 | 3.8 dB |
| 2.4 MS/s | `iq-2400000-1-20.cu8` ON | −0.256 kHz | 8.4 dB | 0.68 | 3.8 dB |
| 2.4 MS/s | `iq-2400000-0-23.cu8` OFF | −0.989 kHz | 7.8 dB | 1.46 | 3.5 dB |
| 3.2 MS/s | `iq-3200000-0-28.cu8` OFF | 0 Hz | 20.4 dB | 0.64 | 10.1 dB |
| 3.2 MS/s | `iq-3200000-1-31.cu8` ON | −1.221 kHz | 8.1 dB | 0.17 | 4.4 dB |
| 3.2 MS/s | `iq-3200000-0-34.cu8` OFF | 0 Hz | 24.6 dB | 0.86 | 11.9 dB |

At 3.2 MS/s, the strongest near-DC bin is exactly DC in both OFF captures and has much higher contrast against the surrounding floor than in the ON capture. ON's strongest bin in ±2 kHz is at −1.221 kHz and has lower local contrast. This pattern is consistent with reducing a center/DC feature, while short, non-coherent samples and a changing noise floor prevent attributing it solely to the offset setting. At 960 kS/s and 2.4 MS/s, peak contrast does not decrease consistently during ON.

Some off-center spectrum remains visible with offset ON. At 2.4 MS/s, a strong feature near −800.35 kHz appears in first OFF and ON samples at the same FFT bin, with peak levels 20.1 and 13.6 dB/bin; restored OFF's largest feature in that band is −797.57 kHz at 5.0 dB/bin. At 3.2 MS/s, ON has a −800.15 kHz peak (2.9 dB/bin), while both OFF samples have weaker peaks around −800 kHz (−17.9 and −21.3 dB/bin). The captures show off-center spectral content during ON but do not establish preservation of the same broadcast signal: samples were recorded at different times and are not coherent or calibrated. Do not identify these features as measured RF tones.

## Evidence boundary

- Register evidence and first-party PLL arithmetic show the intended LO/IF composition, sign, and filter-register restoration at all three rates.
- IQ captures show a prominent 3.2 MS/s OFF center bin relative to its local floor; the ON near-DC peak has lower local contrast. Lower-rate results are mixed.
- Off-center energy is present during ON, but preservation of a specific live broadcast is not proven.
- Raw settings are directly visible in `control-transfers.txt` sections `offset_960000_*`, `offset_2400000_*`, and `offset_3200000_*`; sample identities, declared settings, and digests are in `events.json`.
