# E4000 Center-Artifact Correction Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox syntax for tracking.

**Goal:** Keep E4000 receiver DC/LO artifacts away from the selected channel
without blanking real carriers, changing existing dongle behavior, or applying
frequency correction twice.

**Architecture:** The driver owns tuner/RTL2832U hardware controls and reports
effective IQ frequency semantics. OrcSDR enables supported correction through
reported capabilities, not USB product-name checks. Prefer the existing
RTL2832U digital IF path if first-party PC offset captures establish correct
complex IQ behavior. If software mixing is necessary instead, expose the raw
LO offset explicitly and apply the mixer once before consumer DSP; do not mix
again in the driver. The reference capture decides this before implementation.

**Tech Stack:** C++17 ESP-IDF driver, existing OrcSDR native DSP/serial controls,
black-box RTL-SDR PC API, USBPcap/TShark, CU8 capture and host regression tests.

**Spec:** [center-feature evidence and correction boundaries](../../validation/e4000-com39-2026-10-31/CENTER_PEAK.md), plus the user's approved capability-based driver/application split.

## Current evidence and boundaries

- Driver working branch: codex/lab-regression-20261029, base origin/master.
- OrcSDR lab checkout is an older test base with local driver pin/lock edits.
  Preserve it and recovery artifacts. Production integration must be based on
  a scouted current origin/main checkout under F:/AI/OrcSDR-TEMP.
- Current display already subtracts each FFT window's I/Q means. The saved
  source-off IQ still has a near-DC feature about 18 dB above the surrounding
  floor after equivalent preprocessing; see CENTER_PEAK.md.
- No center-bin replacement, hidden notch, raw capture alteration or global
  processing change for legacy tuners is authorized as a substitute.
- I/Q image calibration is a separate measurement problem. Do not enable an
  unvalidated adaptive correction merely because the tuner is E4000.
- The USB HAL panic remains a separate release-blocking gate.

## Task 1 — Establish first-party offset behavior

- [ ] Capture and validate the PC reference before implementation.

Run the prepared local run_offset_reference.py only with XTR serial 82115932
on the PC. Capture offset disabled/enabled/disabled at 960k, 2.4M and 3.2M
samples/s, requested frequency 96.1 MHz, API return/readback, exact control
traffic and hash-indexed IQ. Preserve physical antenna/converter context.
Verify capture target bus/address, pair control transfers, and derive the
hardware LO delta, digital IF sign/encoding and transition sequencing.
Use a calibrated-source or verified broadcast comparison before asserting
that the logical channel is preserved. API return codes alone are insufficient.

## Task 2 — Driver implementation and regression

- [ ] Write failing offset-policy/trace tests, implement the correction, run the focused checks, and commit.

Use private/e4000.hpp for pure offset/rate/frequency policy and
src/esp_rtl_sdr.cpp for the E4000 control transaction. Reuse existing IF/NCO
encoding and transaction/error handling where the capture supports it.
Add an opt-in public offset-tuning setter and state getter in
include/esp_rtl_sdr.h, plus a device capability. Final state fields and hardware
policy must match Task 1 evidence. Default remains disabled for existing
clients; unsupported profiles fail without changing state.

Write failing pure-policy and actual-function wire tests before production
changes. Cover disabled compatibility, enabled exact sequences, all tested
rates, RF edges, correction sign, repeat calls, start/retune/rate-change,
control-transfer failures, and capability restrictions. Preserve acquisition
epochs, effective center-frequency meaning and explicit offset provenance.
Record a MINOR-version requirement for the new public API; do not tag/merge.

## Task 3 — OrcSDR selection and observability

- [ ] Integrate capability selection and serial/metadata reporting; verify correction is applied exactly once.

Use current origin/main in an isolated worktree. On receiver-ready, configure
correction from capability and report requested/applied state through the
named serial driver interface. Reapply appropriately after reconnect/rate
changes. Keep UI frequency and spectrum labels on the real requested channel.
IQ metadata must distinguish physical tuner LO, effective IQ center, applied
hardware correction and any software correction. Do not infer running-source
identity from a manifest pin alone. Keep spectrum layout/redraw paths intact.

## Task 4 — Physical comparison and documentation

- [ ] Validate controlled or verified-broadcast reception, PNGs, lifecycle and legacy regression gates; commit evidence.

Verify build/source/image hashes, flash COM39 only, then acquire before/after
known-tone or verified-broadcast IQ and FT8 SNAP PNGs at matched rate/gain and
RF setup. Check center artifact relocation/reduction, true carrier amplitude,
labels, FM/RDS and AM demodulation, stop/start, hotplug and rate changes.
Check existing Nooelec/V4 paths separately; never flash COM17. Quantify CPU,
memory and losses if software DSP is introduced. Commit reusable procedures,
results and hashes; retain failures and unclosed hardware gates explicitly.

## Execution ledger (2026-10-31)

- Task 1: PC captures complete, 939 IQ-run controls plus 2,981 valid-rate
  controls, zero unpaired or failed controls; all API returns successful.
  Nine IQ hashes validated. The failed 400 kS/s attempt is retained. Register
  arithmetic supports hardware DDC; calibrated RF preservation is still Task 4.
- Ruling: use hardware DDC with opt-in IDLE configuration, not a new software
  mixer. This avoids duplicate correction and extra per-sample CPU/memory.
  Cost if wrong: the RF comparison must reject the candidate before release.
- Ruling: suspend offset near the RF lower edge and expose applied zero. This
  preserves the existing RF range; center-artifact reduction is unavailable
  there until a separately validated edge strategy exists.
- Ruling: use a 3.3 MHz selection target for mixer code 0xc based on the
  first-party 1.92 MS/s observation. The datasheet labels it 3.0 MHz. This is
  not an analog calibration; cost if wrong is reduced passband/blocker margin,
  which remains a hardware measurement gate.
- Task 2: RED observed for missing planner, then GREEN; actual wire trace RED
  observed for missing LO offset, then GREEN. Expanded 16-rate tests caught
  the mixer-selection difference; capability tests caught inherited V4 support
  and now prove isolation. Current profiles: 2,155 passed/0 failed. Actual
  runtime trace: 9,012 passed/0 failed, including every failing transfer of
  enabled tuning, zero-IF restoration and existing Nooelec controls. Mocked-lock public API contract checks pass. Firmware
  compilation and physical lifecycle checks remain unclosed.
- Task 3: clean OrcSDR branch codex/e4000-offset-20261031 created at current
  origin/main 0b60a166 under F:/AI/OrcSDR-TEMP. A small source candidate adds
  capability-based setup, stopped-radio serial toggle and requested/applied
  reporting. No screen layout change or new software mixing. Not built/flashed.
- Task 4: not started; XTR remains on PC with 20 m antenna and converter in
  passthrough. COM17/COM8 unchanged. USB HAL crash gate remains separate.

- Final review: fixed partial live-control failure with a fail-closed E4000
  guard. RED observed for missing FAULT, then GREEN for every injected transfer
  failure: no streaming/resubmission, FAULT set, previous applied cache retained.
  A further RED/GREEN check preserves the original RF ppm correction after DDC.
- Final: Ruling: retain EC/1F as the disabled/restored baseline, rather than the
  PC's rate-specific OFF filters. This is the existing ESP driver baseline,
  deliberately preserved for offset-OFF compatibility. Replacing it would add
  an unqualified bandwidth change. Cost: OFF mode does not match PC filter
  selection; the comparison must record that difference.
