# Nooelec NESDR SMArt V5 — receiver profile

## 1. Overview and verified scope

**Implemented; standalone ESP32-P4 hardware-tested and OrcSDR fully supported / RF-verified** per the independent driver and OrcSDR current-status snapshots. Internal profile ID: `nooelec_smart_v5_r820t2` (`ESP_RTL_SDR_PROFILE_NOOELEC_SMART_V5`). This is the NESDR **SMArt V5** R820-family receiver, **not** the E4000-based SMArTee/SMArt **XTR V5**, although both can enumerate as `0bda:2838`.

The 2026-09-30 PC campaign proved control and IQ behavior, subsequent merged driver changes corrected IF and gain/restart behavior, the 2026-10-01 Tab5 campaign exercised rates, RF spot checks and hotplug, and the 2026-10-29 standalone smoke exercised its driver independently of OrcSDR. OrcSDR accepted **7.074 MHz FT8** reception on 2026-10-09. The label "fully supported" is a consumer integration/support classification, **not** full-band RF calibration. Driver release as of alignment: **v0.9.6**. [Current project truth](../PROJECT_TRUTH.md) and [terminology](STATUS_TERMINOLOGY.md) control evidence wording.

## 2. USB identity and tuner probe

| Item | Current rule / observation |
|---|---|
| USB | `0bda:2838` shared Realtek RTL2832U ID; this **does not** select a profile alone |
| EEPROM match | Exact manufacturer `Nooelec` and product **contains** `NESDR SMArt v5`; case/spelling are relevant to implementation |
| Observed serial | First-party unit **36309640**, useful capture provenance, **not** an allowlist |
| Tuner bridge | RTL2832U control value `0x0034`; R820-family |
| Silicon certainty | PC API reported **tuner type 5 / R820T**; Nooelec's product information names **R860**. This does not identify exact silicon revision from USB alone |
| Disallowed fallback | `SMArTee XTR v5` / `SMArt XTR v5` selects E4000; unmatched descriptors must not inherit SMArt V5 capabilities |

Source [profile selector](../private/rtl_profile.hpp) and [identity regression](../tests/host/test_profiles.cpp) are the authority. The [September 30 capture](captures/nooelec_v5_2026-09-30.md) records descriptor frame anchors and explicitly distinguishes parent captures, rejected attempts and raw lab artifacts not committed to Git.

## 3. RTL2832U, R820 tuning, RF paths and filter repair

Native tuner operation uses a captured **28.8 MHz** reference and **3.570 MHz** cold IF (PLL and RTL2832U demodulator must agree). The Nooelec-specific cold filter register pair is **`0x0a/0x0b = d3/6b`**, versus V3c's **d5/6b**; demod IF **`0x19/0x1a/0x1b = 38/11/12`**. The independently captured Nooelec profile remaps the relevant R820 initializations, restores IF after rate setup and when returning from direct mode, and keeps tuner-band RF selection separate from V4's GPIO/triplexer route.

**Why the fix mattered:** The previous v0.9.1 provisional code could program the tuner PLL using **3.570 MHz** while leaving the demodulator near **1.815 MHz**; the **approximately +1.755 MHz** resulting RF offset is consistent with the reported 97.3 dial / 99.1 station discrepancy. The implementation was aligned from first-party captures and merged in [PR #39](https://github.com/hardcoreerik/esp-rtl-sdr/pull/39), with later band/restart fixes. This is not an assertion that a historic before/after physical RF experiment proved every causal detail; see [merge notes](nooelec_v5_merge_notes.md).

For RF **below 24 MHz**, Nooelec uses RTL2832U **Q-branch direct sampling**, including tuner shutdown/transition and 22-bit Q NCO programming; the tuner is bypassed. For RF at/above 24 MHz it selects the normal R820 path, with full reinit/IF state on return. **It does not contain V4 HF upconverter, Cable-2, or V4 GPIO switching.** The reference PC 24 MHz boundary showed a PLL-lock warning; Nooelec's product rating begins native operation around 25 MHz, so 24 MHz is a software boundary, not RF qualification.

The [2026-10-01 47-frequency PC sweep](captures/nooelec_v5_band_sweep_2026-10-01.md) independently reproduced the captured [R820 tracking-filter band table](../private/r820t2_band.hpp) across **94/94** ascending/descending tunes for this board. It is PC register coverage; [Tab5 A/B experiments](captures/tab5_rate_matrix_2026-10-01/README.md) supplied separate bounded RF evidence around 453.925 MHz and other spots, not universal RF calibration.

## 4. RF frequencies and sample rates

| Parameter | Implemented behavior / caveat |
|---|---|
| Device-specific RF bounds | **100 kHz–1,750 MHz** enforced by `rtl_profile_supports_rf_hz()` (manufacturer model limits; *not* a measured edge-to-edge sensitivity range) |
| Direct Q | **100 kHz–below 24 MHz** according to software bounds/captured route; in this mode tuner gain and bandwidth are unavailable |
| Native tuner | **24 MHz–1,750 MHz** in driver; manufacturer rated from ~25 MHz and **24 MHz PLL lock was not established** |
| Accepted RF | **7.074 MHz FT8** via OrcSDR (2026-10-09); Tab5 FM/band-select spot checks in dated records |
| Other conditions | RF coupling/analog gain, all LF edges and 24–25 MHz transition require additional physical tests |

The rate policy is shared RTL2832U demodulator programming: **225,001–300,000 S/s** or **900,001–3,200,000 S/s**, with resampler quantization; 300,001–900,000 S/s and 900,000 S/s are rejected. These are programmable windows, **not a per-device sustained-throughput guarantee**. Query `esp_rtl_sdr_get_supported_rates()` for recommendations, `esp_rtl_sdr_quantize_sample_rate()` for the applied rate and `esp_rtl_sdr_probe_rates()` for a host-specific rate passport. See [rate policy](RATES.md) and [public API](API_REFERENCE.md).

[2026-10-01 vendor DLL rate captures](captures/vendor_dll_rate_matrix_2026-10-01.md) show two repeatable fresh opens across 20 rates and three frequencies; on **cold** Nooelec starts the 3.570 MHz `d3/6b` IF state across accepted rates. Those PC operations do not claim a complete Tab5 sustained-rate pass for every condition.

## 5. Gain, tuner AUTO, RTL AGC and bandwidth

- **Manual gain:** `CAP_GAIN`, 29 captured *nominal* manual requests from **0.0 to 49.6 dB**; captured R820 `0x05/0x07` pair mappings are implemented. These are command labels, not calibrated physical gain.
- **Tuner AUTO:** `CAP_GAIN_AUTO`; mode bits change while preserving requested gain nibbles. Reinitialization from Q/native mode and restart restores manual or AUTO intent. A prior 0.9.2 **manual-gain restart failure** (only 1 of 28 old fixed-gain captures worked) was reproduced on Tab5 and corrected on a later candidate (27 cells ran); see [Tab5 campaign](captures/tab5_rate_matrix_2026-10-01/README.md).
- **RTL digital AGC:** `CAP_RTL_AGC` is a separate RTL2832U control; the PC capture recorded page-0 register 19 **`05` off / `25` on**, with settle read.
- **Tuner bandwidth:** `CAP_TUNER_BANDWIDTH` supports AUTO, **200/300/500k, 1.0/1.8/2.4 MHz** at **2.4 MS/s** on the native route. Cold startup uses **d3/6b + 3.570 MHz** until a requested plan; explicit AUTO uses **c3/8f + 1.815 MHz**. Each plan pairs filter, PLL and demod IF with settle reads. A measured analog passband is *not* implied.
- **Unavailable:** `CAP_BIAS_TEE`, `CAP_HF_UPCONVERTER`, E4000 `CAP_OFFSET_TUNING`. Tuner gain/mode and bandwidth requests are intentionally unsupported while direct-Q bypasses the tuner. An **external Ham It Up** or similar converter is not automatically discovered or corrected by this driver.

Implementation is in [registry](../private/rtl_profile.hpp), [captured gain table](../private/gain_r820t2.hpp), [bandwidth plan](../private/measured_tuner_bandwidth.hpp) and [main source](../src/esp_rtl_sdr.cpp); capture evidence in the [September 30 report](captures/nooelec_v5_2026-09-30.md).

## 6. CU8 IQ streaming and lifecycle

The common ESP-IDF High-Speed USB Host client manages enumeration, interface claim, tuner setup, RTL2832U rate programming, bulk-IN multi-URB streaming, and interleaved unsigned 8-bit I/Q (CU8). The public API supports callback, blocking-read and BOTH delivery; `start`, in-stream `retune_hz`, `stop`, detach and reattachment use the shared lifecycle. Retunes from event callbacks are queued; application-task retunes attempt the change immediately and can block during transfer draining. Metrics and health counters are available, but an API acknowledgment is not equivalent to measured RF quality. A hardware test for one profile, host or firmware image **must not** be copied to another. [API contract](API.md) · [USB recovery test plan](acquisition-reliability/TEST_PLAN.md) · [test harness](../examples/p4_serial_smoke/README.md).

The V5 has **independent** standalone Tab5 hardware verification: the [2026-10-29 acquisition implementation report](acquisition-reliability/IMPLEMENTATION_REPORT.md) cites a baseline **45** and opted-in **48** checks; subsequent reauthorized Tab5 regression notes record IDF5.5.4 **46/46 baseline and 48/48 acquisition** and IDF6.1 **48/48 acquisition**. These are bounded smoke tests, not claims of gain calibration or an all-band soak. The [2026-10-01 hotplug gate](captures/tab5_rate_matrix_2026-10-01/README.md) separately reports six detach/probe/restart rounds in a specified candidate plus OrcSDR fix. The 0.9.6 release also contains generic interrupted-start recovery; its Tab5 swap observations are **not** a new Nooelec all-band RF qualification.

## 7. Dated validation and provenance

| Test | Named hardware / date | Bounded result | Evidence |
|---|---|---|---|
| Descriptor, Q/native controls, 29 gains, AGC, bandwidth | PC Nooelec SMArt V5 serial 36309640, 2026-09-30 | Captured matched tuner/demod registers and CU8; rejected early captures are segregated | [capture report](captures/nooelec_v5_2026-09-30.md) |
| Cold IF and +1.755 MHz discrepancy | PC first-party analysis, 2026-09-30; implementation 0.9.2 | Source mismatch corrected; historic user offset is consistent, not a hardware before/after proof | [merge/acceptance record](nooelec_v5_merge_notes.md), [PR #39](https://github.com/hardcoreerik/esp-rtl-sdr/pull/39) |
| RF band/filters | PC V5, 2026-10-01 | 94/94 row-register matches; not RF-performance proof | [band-sweep report](captures/nooelec_v5_band_sweep_2026-10-01.md) |
| Tuning/stream and manual restart | Tab5 V5, 2026-10-01 | Candidate restored stream/gain; old 0.9.2 failed most manual-gain cells | [Tab5 A/B report](captures/tab5_rate_matrix_2026-10-01/README.md) |
| Disconnect/replug | Tab5 V5, 2026-10-01 | Six cycles; no start failures/reboots/leaked URBs in specified setup | [hotplug report](captures/tab5_rate_matrix_2026-10-01/README.md) |
| 7.074 MHz FT8 | OrcSDR V5, 2026-10-09 | Owner accepted real HF reception via Q | [OrcSDR project status](https://github.com/hardcoreerik/OrcSDR/blob/main/PROJECT_STATUS.md) |
| Standalone ESP32-P4 smoke | Tab5 V5, 2026-10-29 and follow-up | Bounded baseline and acquisition suites pass, IDF 5.5.4/6.1 | [implementation report](acquisition-reliability/IMPLEMENTATION_REPORT.md) |
| Extended full-band soak/calibration | Not established | **Open** | [test plan](acquisition-reliability/TEST_PLAN.md) |

## 8. OrcSDR integration: fully supported ≠ calibrated

The authoritative [2026-11-04 OrcSDR project status](https://github.com/hardcoreerik/OrcSDR/blob/main/PROJECT_STATUS.md) names NESDR SMArt V5 **fully supported / RF-verified**. The driver provides profile selection, CU8 streaming, direct-Q HF, gain, tuner AUTO, RTL digital AGC, and native bandwidth; OrcSDR provides decoders, dashboards, audio and navigation. This distinction avoids describing the old **provisional** 0.9.1 branch note as today's product. The captured 7.074 MHz FT8 test is real reception, but other dashboard bands and RF sensitivity require their own evidence. OrcSDR's status snapshot pins driver **0.9.5**, while **0.9.6** is the independently released current driver; do not conflate an OrcSDR tagged build, nightly and driver HEAD.

## 9. Remaining gaps

A full calibrated gain ladder, effective analog bandwidths, LF edge/AM reception comparison, the **24 MHz warning and 24–25 MHz transition**, broad multi-antenna reception and extended per-device endurance remain unverified. A 2026-09-30 lab report/manifest discrepancy is explicitly documented in the first-party capture note and should be corrected in a future source-artifact export, **not hidden** in this profile. Nooelec SMArt V5 does **not** inherit XTR's E4000 or always-on-bias behavior.

## 10. Engineering references and implementation history

[Profile selector and capabilities](../private/rtl_profile.hpp) · [stream/init/tune routines](../src/esp_rtl_sdr.cpp) · [R820 gain mappings](../private/gain_r820t2.hpp) · [band-select table](../private/r820t2_band.hpp) · [tuner bandwidth plans](../private/measured_tuner_bandwidth.hpp) · [host profile tests](../tests/host/test_profiles.cpp) · [September 30 capture](captures/nooelec_v5_2026-09-30.md) · [Nooelec implementation/merge notes](nooelec_v5_merge_notes.md) · [PR #39](https://github.com/hardcoreerik/esp-rtl-sdr/pull/39) · [Nooelec gain-start fix PR #44](https://github.com/hardcoreerik/esp-rtl-sdr/pull/44) · [USB recovery PR #50](https://github.com/hardcoreerik/esp-rtl-sdr/pull/50) · [v0.9.6 release](releases/v0.9.6.md).
