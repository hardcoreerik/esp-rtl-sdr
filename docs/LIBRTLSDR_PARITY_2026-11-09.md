# esp-rtl-sdr / Osmocom librtlsdr — source comparison

Date: 2026-11-09. Audience: firmware integrators choosing APIs and acceptance
tests. This is a source-level comparison, not a hardware benchmark or a claim
of superior RF performance, lossless acquisition, calibration or reliability.

## Revisions and scope

- esp-rtl-sdr 0.9.6: [`dabdda1aff6aa196f3c1523d791adf80d2c24ccc`](https://github.com/hardcoreerik/esp-rtl-sdr/tree/dabdda1aff6aa196f3c1523d791adf80d2c24ccc).
- Osmocom rtl-sdr: [`797f8143266d983c56d8f35d2d442527529dd8a5`](https://github.com/osmocom/rtl-sdr/tree/797f8143266d983c56d8f35d2d442527529dd8a5).
- Compared public headers with control/streaming implementations, receiver
  masks/routing and relevant tuner bandwidth implementations. No tests,
  builds, flashes or physical measurements were performed for this comparison.
- Osmocom means this upstream revision, not vendor forks or SDR applications.
  Public source was inspected to establish observable feature differences;
  no implementation code or register tables were imported.

Primary sources: our [header](https://github.com/hardcoreerik/esp-rtl-sdr/blob/dabdda1aff6aa196f3c1523d791adf80d2c24ccc/include/esp_rtl_sdr.h),
[acquisition header](https://github.com/hardcoreerik/esp-rtl-sdr/blob/dabdda1aff6aa196f3c1523d791adf80d2c24ccc/include/esp_rtl_sdr_acquisition.h),
[implementation](https://github.com/hardcoreerik/esp-rtl-sdr/blob/dabdda1aff6aa196f3c1523d791adf80d2c24ccc/src/esp_rtl_sdr.cpp)
and [profile rules](https://github.com/hardcoreerik/esp-rtl-sdr/blob/dabdda1aff6aa196f3c1523d791adf80d2c24ccc/private/rtl_profile.hpp);
Osmocom's [public header](https://github.com/osmocom/rtl-sdr/blob/797f8143266d983c56d8f35d2d442527529dd8a5/include/rtl-sdr.h),
[library](https://github.com/osmocom/rtl-sdr/blob/797f8143266d983c56d8f35d2d442527529dd8a5/src/librtlsdr.c),
[R82xx tuner](https://github.com/osmocom/rtl-sdr/blob/797f8143266d983c56d8f35d2d442527529dd8a5/src/tuner_r82xx.c)
and [E4000 tuner](https://github.com/osmocom/rtl-sdr/blob/797f8143266d983c56d8f35d2d442527529dd8a5/src/tuner_e4k.c).

## Receive controls and hardware access

“Present” means an implemented path, not drop-in API/ABI compatibility or equal
physical behavior. Applications still need per-device capability checks and
must handle supported-rate/RF-route limits.

| Area | esp-rtl-sdr compared with Osmocom |
|---|---|
| Center tuning | Present; profile limits and queued/drained live retune. Osmocom performs tuning in the setter call. |
| Sample rates | Same nominal windows: 225001–300000 and 900001–3200000 samples/s; realized-ratio arithmetic implemented. Not evidence that every rate is physically lossless. |
| Rate changes | Partial: our setter requires stop/start; Osmocom directly reprograms the rate without our streaming-state gate. Neither statement promises uninterrupted samples. |
| Gain and tuner AUTO | Present with profile ladders; ours uses nominal labels and asynchronous live writes. Equal labels are not calibrated equal RF gain. |
| RTL digital AGC | Present, distinct from tuner AUTO. Current source masks include E4000; older evidence/status descriptions must retain their dates. |
| Hardware tuner bandwidth | Partial: ours exposes captured choices at 2.4 MS/s for V4/V4L/V3/Nooelec V5 tuner routes. Osmocom R82xx programming covers a broader set of rate/width choices. Neither implements arbitrary application channel filtering here. |
| E4000 bandwidth | No public selectable-width control in ours; Osmocom programs mixer/RC/channel filters. Our offset implementation does program its own filter plan, so “no hardware filters” is incorrect. |
| E4000 IF gain | No individual-stage API in ours; Osmocom exposes stage gain. Ours provides a combined nominal ladder. |
| Offset tuning | Experimental E4000 in ours, IDLE configuration, with requested/applied state and low-RF-limit suspension. Osmocom exposes zero-IF offset tuning, but rejects R820T/R828D and direct-sampling mode. |
| Direct sampling | Partial: ours automatically selects Q for V3/Nooelec below 24 MHz; no public force-off/I/Q selector. Osmocom exposes off/I/Q selection. Physical usefulness depends on board wiring. |
| HF upconverter | Implemented V4/V4L board routing in ours; current upstream also recognizes V4/V4L. Our tuner-route override is separate from ADC direct sampling. |
| RF PPM | Present with different semantics: ours corrects RF tuning. Osmocom also writes RTL sample-frequency correction registers and updates corrected tuner-clock calculations. |
| Sample-clock estimate | Our optional app-supplied metadata estimate describes uncertainty; it does not program clock correction. |
| Crystal frequencies | No public configurable RTL/tuner crystal API in ours; Osmocom has setters/getters. Ours uses profile clock assumptions. |
| Hardware test counter | Missing in ours; Osmocom exposes the RTL2832 generated-counter mode. |
| EEPROM | No public read/write API in ours; Osmocom provides both. Reading USB descriptors is not raw EEPROM access. |
| Bias tee | Present on capable profiles in ours; no arbitrary bias GPIO API. Osmocom supports GPIO 0 and a caller-selected GPIO. Neither API proves a circuit is installed or measures loaded current. |
| Receiver coverage | Ours implements V4, V4L, V3, Nooelec V5 and E4000/XTR profiles. Osmocom additionally has FC0012/FC0013/FC2580 tuner paths and broader USB-ID recognition. Tuner-family support does not guarantee all board variants. |

API presence alone is insufficient: Osmocom FC0012/FC0013 bandwidth handlers
are no-ops; FC2580 bandwidth handling is not arbitrary-Hz selection. Avoid a
feature score based only on function names or profile counts.

## Streaming and integration contracts

| Area | Difference |
|---|---|
| Data format | Both deliver interleaved unsigned 8-bit I/Q (CU8). |
| Callback delivery | Ours uses a delivery task; Osmocom calls the application from libusb completion processing before resubmission. Neither enables retaining callback storage indefinitely. |
| Blocking reads | Ours consumes a software pull ring; Osmocom synchronous reads perform a USB bulk transfer. These are functional alternatives, not identical timing/backpressure contracts. |
| Backpressure | Our pull ring refuses incoming excess bytes, keeps I/Q pairs and counts loss; callback starvation also has counters. Osmocom's completion path and buffer ownership differ. No matched throughput comparison was made. |
| Stop/reset | Ours has explicit states, draining and retained-resource cleanup retries. Its reset is not Osmocom's hardware endpoint-buffer-reset API. |
| Multi-device | Both support multiple handles. Ours adds shared-host lifetime, exclusive USB-address claims, candidate/serial selection and a configured eight-device limit. |
| Completion/getters | Our live gain/mode/AGC/bias/bandwidth writes may be queued/coalesced. Requested shadows are not applied/readback proof. Osmocom generally performs writes in the call, but its getters also often return stored values. |
| Diagnostics | Ours adds lifecycle events, capabilities, health/windows, rate probing, identity/hub/stream stats and optional sample-loss/epoch metadata beyond Osmocom's public API. More diagnostics do not establish better reliability. |
| RF decoding | Stereo, RDS, demodulation, FFTs and application channel filtering belong above both driver APIs. Their absence is not driver-parity debt. |

## Evidence and separate follow-up observations

Keep implementation, programmed state, readback and physical acceptance separate.
The [current truth record](../PROJECT_TRUTH.md) tracks acceptance. The dated
[one-hour four-device application report](Test_reports/FOUR_DONGLE_2560K_EXTERNAL_POWER.md)
uses an older pinned driver and network observation; it is not current-release
standalone, watchdog, RF calibration or replug acceptance. This comparison adds
no hardware evidence and does not replace any open acceptance gate.

One source behavior merits a separate decision: `set_freq_correction` stores
the new PPM preference before requesting a live retune. If that retune fails,
the getter can show the new preference while hardware application is uncertain.
This is verified ordering, not a confirmed defect; it may be intended requested
state. Reproduction and any behavior change belong in a separate driver change.

There is also a header-comment mismatch: `get_logical_index` describes 0xFF on a
stale handle, but the implementation returns STALE_HANDLE without writing the
output. The refreshed reference describes the implementation. Resolve that
header-contract discrepancy separately; callers should initialize any sentinel
and inspect the error rather than depend on an output after failure.

Missing controls above are observations, not an approved implementation roadmap.
Future implementation must preserve first-party derivation/provenance and
profile-specific acceptance. Do not copy upstream source or tables into this
clean-room driver to close a parity row.
