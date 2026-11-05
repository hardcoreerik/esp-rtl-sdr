# Driver documentation freshness audit — 2026-11-04

This audit accompanies the standalone documentation PR and the matching [OrcSDR receiver/feature documentation update](https://github.com/hardcoreerik/OrcSDR/pull/231).

## Scope

GitHub inventory found **99 Markdown files** on the driver `master` branch. We searched current and historical documentation for experimental, provisional, unsupported, not verified, not implemented and hardware-acceptance claims, then compared current-facing entry points against [PROJECT_TRUTH](../PROJECT_TRUTH.md), driver v0.9.4 source documentation, first-party capture and dated P4 records. Dated release notes and test reports are preserved as evidence; this documentation PR does not re-run hardware tests or retroactively validate old firmware images.

## Reconciled claims

| Previous stale wording | Current accurate description |
|---|---|
| Blog V3 is a provisional stream | Blog V3c has named Tab5 streaming/tuning/retune/RDS evidence. Older V3 hardware revisions must not inherit a blanket certification. |
| Nooelec NESDR SMArt V5 P4 acceptance entirely pending | Driver is implemented and standalone Tab5 streaming/control was exercised; OrcSDR also accepted 7.074 MHz FT8. Per-band/analog RF performance is not universally calibrated. |
| E4000 implementation experimental and never tested on ESP32-P4 | E4000/XTR is **implemented**; initial COM39 Tab5 2.4 MS/s FM audio/RDS was observed on a dated image. Additional frequency/soak/current-image acceptance remains open. |
| E4000 has no gain controls | 14 nominal manual tuner gain steps and tuner AUTO are implemented. Their broad analog accuracy and serial smoke remain incomplete. RTL AGC, tuner bandwidth, bias and HF capabilities are not exposed. |
| Only Blog V4 profile is present / support is only V4 | Blog V4, V4 Lite, V3c, Nooelec V5 and E4000/XTR have implemented named model profiles with varying acceptance boundaries. |
| Experimental E4000 file as the primary user-facing entry | New canonical [E4000 profile](E4000_PROFILE.md); legacy filename kept as link redirect. |
| Capability matrix described v0.9.3 | Matrix reflects published v0.9.4 profile inventory, including E4000 and model-specific gain capabilities. |
| HF upconverter and V3/Nooelec Q-route shown as merely future work | Implementation is present and observed at particular HF frequencies; LF edges and universal RF calibration remain separate tasks. |

## What remains unresolved

- Complete E4000 gain/AUTO RF acceptance, full-band and band-edge verification, hotplug and long-soak behavior.
- OrcSDR application dashboard and tuning-range behavior for E4000 (different repository; driver implementation alone does not solve it).
- No generic RTL2832U hardware compatibility claim, no ESP32-S2/S3 host support claim, and no blanket calibrated sensitivity or amplifier performance claim.
- Historical reports and release tags continue to describe what was known at that date.

Use [status terminology](STATUS_TERMINOLOGY.md) to distinguish implemented software from its test matrix; never equate missing analog calibration with a missing driver implementation.
