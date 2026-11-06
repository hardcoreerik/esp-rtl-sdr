# Driver maturity and evidence terminology

**Reviewed:** 2026-11-04. Current source truth: [PROJECT_TRUTH.md](../PROJECT_TRUTH.md). Dated release and lab reports remain historical evidence.

## Implementation and validation are separate

| Term | Meaning |
|---|---|
| **Implemented** | The named device profile and its declared driver API/capabilities exist in source. Unknown dongles still fail closed. |
| **Hardware-tested** | Named physical board/receiver was used for a bounded enumeration, IQ, control or lifecycle check. |
| **RF-validated** | A named source/signal was actually received and recorded, with frequency, setup and test date. |
| **Calibration pending** | Sensitivity, band-edge behavior, gain accuracy, analog passbands or soak endurance have not been established across the full requested scope. This does *not* retract the implementation. |
| **Experimental / lab-only** | Research-only paths not offered as an accepted general driver capability. Do not use this term as an unqualified product label for a completed receiver profile. |
| **Unsupported** | A capability absent or deliberately rejected by that exact model; fail-closed behavior is intentional. |

## Applied receiver policy

- **RTL-SDR Blog V4, V4 Lite (V4L), V3c and Nooelec NESDR SMArt V5:** driver profiles are implemented and have ESP32-P4 testing; their individual capabilities, front ends and calibration limits differ.
- **E4000 / Nooelec XTR:** driver profile **implemented**, including identity/probe, native tuner/zero-IF programming, IQ and manual tuner gain/AUTO. Initial Tab5 FM/RDS reception has been recorded. Full-band, RF calibration, gain accuracy and long-soak testing are still in progress. See [E4000 profile](E4000_PROFILE.md).
- **Arbitrary generic RTL2832U dongles, FC0012/FC0013/FC2580 and ESP32-S2/S3 host targets:** not implied by the above-named profile implementations.

When an app such as OrcSDR adopts a new driver profile, **application integration and dashboard eligibility are independent work**. Driver frequency tuning support does not enable a dashboard whose band is unreachable or whose front-end assumptions are wrong.

## Documentation update rule

Use `Implemented / [specific dated acceptance]` instead of a blanket `Experimental` when code and measured tests justify it. Never promote a versioned release or historic test retroactively; identify exact commit, firmware image, hardware, frequency and missing gates in an adjacent evidence note. Link current docs to this policy rather than inventing new readiness labels.
