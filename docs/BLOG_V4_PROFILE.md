# RTL-SDR Blog V4 / R828D — receiver profile

## 1. Overview and implementation status

**Implemented; hardware-tested and RF-validated on named ESP32-P4 configurations.** Internal profile: `blog_v4_r828d` (`ESP_RTL_SDR_PROFILE_BLOG_V4`). This is the original first-party capture-derived path, not a generic R820T2 profile. Blog V4 FM/RDS, HF FT8, tuning and streaming have been reported in OrcSDR and Tab5 exercises. The profile is not a calibrated RF performance specification and not a warranty for every V4 board/antenna/host.

*Evidence vocabulary:* [status terminology](STATUS_TERMINOLOGY.md), [current project truth](../PROJECT_TRUTH.md). Source implementation is the authority; dated reports describe their tested commits rather than automatic acceptance of later releases. Current driver version at this alignment: **0.9.6**.

## 2. USB identity and probing

| Item | Driver behavior |
|---|---|
| USB VID:PID | `0bda:2838` (shared RTL2832U identifier, **insufficient alone**) |
| Accepted EEPROM strings | Exact manufacturer `RTLSDRBlog`, product `Blog V4` |
| Profile selector | `rtl_profile_from_descriptors()`, before any R820-family ambiguous-device fallback |
| Tuner | R828D, captured RTL2832U I2C bridge value `0x0074` |
| Negative cases | `Blog V4L`, generic `RTL2838UHIDIR`, Nooelec and unmatched VID/PID never become V4 through the shared ID |

[Profile registry](../private/rtl_profile.hpp) and [identity tests](../tests/host/test_profiles.cpp) describe the exact acceptance policy. The V4L descriptor is checked **before** V4; an I2C resemblance must not override it. No serial-number whitelist is applied.

## 3. Initialization and RF signal path

The RTL2832U reset/demodulator and R828D programming use this project's independently captured [USB control tables](../private/transfers_blog_v4.hpp). V4's reference is 28.8 MHz; its normal R828D low-IF path is based on the captured approximately **1,814,972 Hz** PLL offset. Rate-specific tuner filter and matched demod IF programming are documented in the [2026-10-01 vendor-DLL matrix](captures/vendor_dll_rate_matrix_2026-10-01.md), distinct from other R820-family boards.

For **RF below 28.8 MHz** the on-board V4 HF converter adds **28.8 MHz** to the requested RF for the tuner, with V4-specific Cable-2/GPIO5 and triplexer/front-end composition. This is *native board upconversion*, **not RTL2832U Q-branch direct sampling**. The internal HF routing and band switching live in [measured V4 front-end](../private/measured_gain_bias_v4.hpp) and [driver](../src/esp_rtl_sdr.cpp). An optional tuner-direct route in part of the HF range is application-controlled; it is not a promise of usable reception at the driver arithmetic lower edge.

The 2026-10-01 [four-board sweep](captures/nooelec_v5_band_sweep_2026-10-01.md) established that R828D tracking-filter register behavior **differs** from the R820T2 table. Do not map the V3/Nooelec table blindly onto V4. The 2026-10-01 Tab5 [A/B report](captures/tab5_rate_matrix_2026-10-01/README.md) records a bounded improvement around 230 MHz on one setup, not universal gain.

## 4. Frequencies, rates, tuning and HF

| Dimension | Current contract / evidence |
|---|---|
| Software RF acceptance | Driver arithmetic and V4 profile policy: **24 kHz–1.766 GHz**; not a verified RF-performance range |
| Native versus HF-converted route | Requested RF below 28.8 MHz normally maps to RF + 28.8 MHz with V4 front-end selection |
| Frequency resolution | One-hertz request normalization; PLL quantization, crystal accuracy and image rejection remain measurement questions |
| RF checkpoints | FM **96.1/99.1 MHz** and **7.074 MHz FT8** reception recorded in integration evidence; 96.1 MHz bandwidth cycling on Tab5 |
| Sample rates | Shared continuous RTL2832U legal windows below; different rates affect filter/IF plans |

The rate policy is shared RTL2832U demodulator programming: **225,001–300,000 S/s** or **900,001–3,200,000 S/s**, with resampler quantization; 300,001–900,000 S/s and 900,000 S/s are rejected. These are programmable windows, **not a per-device sustained-throughput guarantee**. Query `esp_rtl_sdr_get_supported_rates()` for recommendations, `esp_rtl_sdr_quantize_sample_rate()` for the applied rate and `esp_rtl_sdr_probe_rates()` for a host-specific rate passport. See [rate policy](RATES.md) and [public API](API_REFERENCE.md).

**Band boundaries:** 28.8 MHz marks the standard HF converter transition; 250 MHz separates a measured V4 front-end band class. These are routing/programming boundaries, **not tested filter passband edges**. V4 physical LF below 500 kHz and all-band edge characterization remain open.

## 5. Gain, AGC, bandwidth and bias tee

- **Nominal manual gain:** CAP_GAIN with capture-derived R828D gain programming ([gain/bias captures](GAIN_BIAS_CAPTURE.md)); request units are tenths of dB, not a calibrated analog gain measurement.
- **Tuner AUTO and RTL digital AGC:** distinct capabilities, `CAP_GAIN_AUTO` and `CAP_RTL_AGC`. `set_tuner_gain_mode(AUTO)` is not `set_rtl_agc(true)`; the latter modifies RTL2832U digital AGC.
- **Live tuner bandwidth:** `CAP_TUNER_BANDWIDTH` with AUTO, 200 kHz, 300 kHz, 500 kHz, 1.0 MHz, 1.8 MHz, 2.4 MHz at **2.4 MS/s**, subject to active-tuner path and deferred application. At or below 28.8 MHz without the direct tuner route, only AUTO, 200 kHz, 500 kHz and 2.4 MHz are accepted; 300 kHz, 1.0 MHz and 1.8 MHz return `ESP_RTL_SDR_ERR_UNSUPPORTED`. These are control plans, not measured analog -3 dB passbands.
- **Software-switchable bias tee:** `CAP_BIAS_TEE`; V4-only GPIO/front-end controls compose bias state with band/gain. No-load electrical tests do **not** certify current capacity under an attached load.
- **Not exposed:** `CAP_DIRECT_SAMPLING` and `CAP_OFFSET_TUNING` on the V4 profile. The HF converter flag is the appropriate capability.

A [2026-09-28 Tab5 bandwidth test](captures/v3c_live_bandwidth_2026-09-28.md) after the demod read-after-write fix kept the carrier within ±1.4 kHz at 96.1 MHz across the tested transitions, and final AUTO within 0.4 dB of baseline in that setup. That is bounded evidence, not blanket filter calibration.

## 6. I/Q streaming and lifecycle

The common ESP-IDF High-Speed USB Host client manages enumeration, interface claim, tuner setup, RTL2832U rate programming, bulk-IN multi-URB streaming, and interleaved unsigned 8-bit I/Q (CU8). The public API supports callback, blocking-read and BOTH delivery; `start`, in-stream `retune_hz`, `stop`, detach and reattachment use the shared lifecycle. Retunes from event callbacks are queued; application-task retunes attempt the change immediately and can block during transfer draining. Metrics and health counters are available, but an API acknowledgment is not equivalent to measured RF quality. A hardware test for one profile, host or firmware image **must not** be copied to another. [API contract](API.md) · [USB recovery test plan](acquisition-reliability/TEST_PLAN.md) · [test harness](../examples/p4_serial_smoke/README.md).

V4 participated in the [2026-10-01 Tab5 rate/hotplug campaigns](captures/tab5_rate_matrix_2026-10-01/README.md), including a five-cycle unplug/replug gate on a specified release candidate with an OrcSDR speaker fix. Separate [four-dongle application provenance](Test_reports/FOUR_DONGLE_2560K_EXTERNAL_POWER.md) concerns its specific application setup; it must not be read as a standalone, all-V4 long-soak guarantee. The 0.9.6 interrupted-start fix has host and bounded Tab5 evidence, not per-V4 requalification of every case.

## 7. Dated validation matrix

| Test | Hardware/date | Observed result | Primary evidence |
|---|---|---|---|
| USB identity/probe | PC V4, 2026-10-01 | `RTLSDRBlog / Blog V4`, R828D-specific path | [band sweep](captures/nooelec_v5_band_sweep_2026-10-01.md) |
| Tuner/IF/rate controls | PC V4, 2026-10-01 | Two 20-rate reference runs; band/filter registers | [rate matrix](captures/vendor_dll_rate_matrix_2026-10-01.md) |
| FM and live bandwidth | Tab5 V4, 2026-09-28 | 96.1 MHz, AUTO/200k…2.4M/AUTO, centered after fix | [hardware report](captures/v3c_live_bandwidth_2026-09-28.md) |
| HF reception | OrcSDR V4, 2026-10-09 | 7.074 MHz FT8 accepted by maintainer | [OrcSDR status](https://github.com/hardcoreerik/OrcSDR/blob/main/PROJECT_STATUS.md) |
| Sample-rate/retune/band | Tab5 V4, 2026-10-01 | B/C image bounded IQ/metrics and RF comparisons | [Tab5 campaign](captures/tab5_rate_matrix_2026-10-01/README.md) |
| Disconnect/reattach | Tab5 V4, 2026-10-01 | Five-cycle gate on candidate with companion app fix; earlier failures retained | [hotplug report](captures/tab5_rate_matrix_2026-10-01/README.md) |
| Sustained, calibrated full-band RF | Not recorded as complete | **Pending**, not inferred from above | [truth](../PROJECT_TRUTH.md), [soak plan](SOAK.md) |

## 8. OrcSDR integration and release scope

OrcSDR classifies Blog V4 as **fully supported / RF-verified** at its 2026-11-04 status snapshot, using this driver as a dependency rather than an architectural requirement. Reception, station metadata and FT8 are application-level observations. The V4 HF converter supports below-28.8 MHz dashboards *when that routing is available and the signal is received*; not every dashboard is individually validated. OrcSDR's documented driver pin at that snapshot was **v0.9.5**, while this driver repository's latest published release is **v0.9.6**: do not represent the latter as automatically installed in OrcSDR. See [OrcSDR project status](https://github.com/hardcoreerik/OrcSDR/blob/main/PROJECT_STATUS.md).

## 9. Known limitations and open gates

Analog gain/sensitivity, exact V4 passbands, loaded bias-current capacity, HF LF-edge behavior and full-span/long-duration re-soak remain incomplete. 2026-10-01 site-specific results do not imply performance improvement at every antenna or location. Hardware/USB acceptance applies only to recorded fixture and image. Unknown devices and newer board revisions require independent identity/routing evidence.

## 10. Engineering references

[Identity/caps](../private/rtl_profile.hpp) · [R828D capture table](../private/transfers_blog_v4.hpp) · [V4 front end](../private/measured_gain_bias_v4.hpp) · [bandwidth table](../private/measured_tuner_bandwidth.hpp) · [main driver](../src/esp_rtl_sdr.cpp) · [profile tests](../tests/host/test_profiles.cpp) · [policy tests](../tests/host/test_policy.cpp) · [control capture notes](captures/NOTES.md) · [release notes](releases/v0.9.6.md) · [central profile matrix](PROFILES.md).
