# RTL-SDR Blog V3 family / V3c — R820T2/R860 receiver profile

## 1. Overview and maturity

**Implemented driver profile; V3c hardware-tested and RF-validated.** Internal ID `blog_v3_r820t2` (`ESP_RTL_SDR_PROFILE_BLOG_V3`). The specific tested radio is **Blog V3c** with R860 per packaging. The same source profile can identify other Blog V3 and ambiguous R820-family receivers, but **V3c acceptance is not evidence that every older V3 revision has the same board front end or bias circuit**. OrcSDR classifies V3c as fully supported, with earlier V3 revisions requiring individual verification. Current standalone driver version: **0.9.6**.

## 2. USB descriptors and tuner probing

| Item | Selection rule |
|---|---|
| USB VID:PID | Shared `0bda:2838`; not sufficient |
| Named descriptors | `RTLSDRBlog` plus product exactly `Blog V3` or `RTL-SDR Blog V3` |
| Ambiguous fallback | Completed R820-type register-0 probe `0x96` or bit-reversed `0x69`; failed/STALL probe cannot match |
| Real V3c example | `Realtek / RTL2838UHIDIR`, serial `00000001`, identified by completed probe, not V3 strings |
| Tuner I2C | RTL2832U bridge value `0x0034`, distinct from V4's `0x0074` |
| Nonmatches | Unrecognized tuner ID, E4000 `0x40` probe, V4L descriptor and unrelated USB IDs must not be cast as V3 |

The [registry](../private/rtl_profile.hpp) and [host identity tests](../tests/host/test_profiles.cpp) establish priority: descriptor selection wins; otherwise only a **successful** chip-ID probe permits the R820 profile. A generic probe identifies a tuner family, **not a guarantee of V3c hardware layout**.

## 3. Initialization, filters and Q direct sampling

V3c is initialized with a captured RTL2832U/R820-family path and a **measured 28.8 MHz** reference. Its matched tuner-PLL and RTL demodulator IF on cold startup is **3.570 MHz**, with demod registers `0x19/0x1a/0x1b = 38/11/12` and tuner filter `0x0a/0x0b = d5/6b`. A previous V3c tuning-offset investigation resolved a false 32 MHz crystal hypothesis by accounting for the fractional PLL bytes; see [first-party control notes](captures/NOTES.md). V4's 1.814972 MHz IF is **not** the V3c default.

Below **24 MHz**, V3/V3c use the RTL2832U **Q-branch direct-sampling** route: park/clean up the tuner, enable Q, program a 22-bit signed NCO from RF and reference clock, then restore the tuner/repeater/IF before returning to normal tuning. This is *not* a V4 on-board upconverter. The direct-path register sequence is recorded in [LF/HF control captures](captures/v3c_lf_hf_direct_sampling_2026-09-14.txt) and implemented in [driver tune routines](../src/esp_rtl_sdr.cpp).

For RF filter selection, V3c and Nooelec v5 shared the measured [R820 band table](../private/r820t2_band.hpp) over the tested PC sweep; this does not transfer board-level behavior or all-band sensitivity from one to the other.

## 4. Frequency and rate policy

| Item | Value and evidence boundary |
|---|---|
| Driver-programmed RF span | Shared **24 kHz–1.766 GHz** arithmetic range; not a product-wide calibrated tuning guarantee |
| Route boundary | Below 24 MHz: direct Q; at/above 24 MHz: tuner path; the boundary does not guarantee PLL lock or signal quality |
| Verified FM | **96.1 and 99.1 MHz** on physical V3c; 99.1 RDS/stereo reception and cold/hot returns recorded September 2026 |
| HF signal | **7.074 MHz FT8** accepted in OrcSDR on 2026-10-09; other LF/AM limits unqualified |
| Tuning quantization | 1-Hz request normalization with actual PLL/NCO/crystal accuracy subject to measurement |

The rate policy is shared RTL2832U demodulator programming: **225,001–300,000 S/s** or **900,001–3,200,000 S/s**, with resampler quantization; 300,001–900,000 S/s and 900,000 S/s are rejected. These are programmable windows, **not a per-device sustained-throughput guarantee**. Query `esp_rtl_sdr_get_supported_rates()` for recommendations, `esp_rtl_sdr_quantize_sample_rate()` for the applied rate and `esp_rtl_sdr_probe_rates()` for a host-specific rate passport. See [rate policy](RATES.md) and [public API](API_REFERENCE.md).

The [2026-10-01 reference matrix](captures/vendor_dll_rate_matrix_2026-10-01.md) found that a fresh V3c start retains `d5/6b` and 3.570 MHz IF across the sampled rates, unlike V4/V4L's cold per-rate IF filter behavior; do not copy those per-rate settings between boards.

## 5. Gain, tuner AUTO, digital AGC, bandwidth and bias

**Current source capabilities (not a statement of calibration):** `CAP_GAIN`, `CAP_GAIN_AUTO`, `CAP_RTL_AGC`, `CAP_BIAS_TEE`, `CAP_DIRECT_SAMPLING` and `CAP_TUNER_BANDWIDTH`. Earlier prose describing all but manual gain as absent is **stale relative to the present registry**; see [`rtl_profile_device_capabilities`](../private/rtl_profile.hpp). No `CAP_HF_UPCONVERTER` and no E4000 `CAP_OFFSET_TUNING`.

- Manual R820-family ladder: **29 nominal requests (0.0–49.6 dB)** in first-party V3c PC campaign; **not calibrated** effective gain. Tuner AUTO and RTL digital AGC are distinct mode paths; a captured API/control sequence is not a universal gain/sensitivity measurement.
- **Bias-tee caveat:** the V3 profile mask exposes software bias control and PC testing recorded a no-load V3c output check. The driver itself warns that an ambiguously identified R820 stick may not have safe physical bias hardware; verify exact board and load before enabling. `CAP_BIAS_TEE` here must not be advertised as proven across older revisions.
- At **2.4 MS/s**, explicit tuner bandwidths **200/300/500k, 1.0/1.8/2.4 MHz** and AUTO are implemented. AUTO restores **d5/6b + 3.570 MHz + 38/11/12**. Explicit plans use matching variable PLL and demod IF (**2.125, 2.025, 1.700, 1.750, 1.815 MHz**); these are register plans, not analog passband measurements.
- In direct-Q mode the tuner is bypassed; tuner gain/mode/bandwidth writes are **unsupported** for that path even though capabilities exist for the receiver in normal mode.

The [2026-09-28 on-Tab5 test](captures/v3c_live_bandwidth_2026-09-28.md) exercised AUTO→six plans→AUTO at 96.1 MHz across seven runs. Carrier errors were within about +2 kHz in those runs; one final AUTO level differed up to 3.6 dB, with no reproduced root cause. Do not erase that outlier.

## 6. Streaming and lifecycle

The common ESP-IDF High-Speed USB Host client manages enumeration, interface claim, tuner setup, RTL2832U rate programming, bulk-IN multi-URB streaming, and interleaved unsigned 8-bit I/Q (CU8). The public API supports callback, blocking-read and BOTH delivery; `start`, in-stream `retune_hz`, `stop`, detach and reattachment use the shared lifecycle. Retunes from event callbacks are queued; application-task retunes attempt the change immediately and can block during transfer draining. Metrics and health counters are available, but an API acknowledgment is not equivalent to measured RF quality. A hardware test for one profile, host or firmware image **must not** be copied to another. [API contract](API.md) · [USB recovery test plan](acquisition-reliability/TEST_PLAN.md) · [test harness](../examples/p4_serial_smoke/README.md).

Real V3c identification, cold boot, battery boot, V4↔V3c hot swaps, stream without crash and matched-IF retunes were recorded September 11–12. [2026-10-01 hotplug gate](captures/tab5_rate_matrix_2026-10-01/README.md) separately records six detach/reprobe/restart rounds on the specified release candidate with app-side fix. This does not prove every earlier Blog V3 board or an unbounded soak.

## 7. Dated evidence matrix

| Test | Named hardware/date | Bounded outcome | Evidence |
|---|---|---|---|
| Ambiguous descriptor/probe | Physical Blog V3c, 2026-09-11 | Repeated successful R820 chip-ID `0x96/0x69` detection | [capture notes](captures/NOTES.md) |
| Correct IF, station tuning | Tab5 Blog V3c, 2026-09-11/12 | 96.1/99.1 FM, matched station and 99.1 RDS; cold/hot return | [capture notes](captures/NOTES.md), [truth](../PROJECT_TRUTH.md) |
| LF/HF direct-Q controls | V3c PC, 2026-09-14 | NCO/control writes and PC IQ at eight LF/HF points; **not** P4 reception for all points | [Q-route capture](captures/v3c_lf_hf_direct_sampling_2026-09-14.txt) |
| Gain/AGC/bias wire controls | V3c PC, 2026-09-25 | 29 nominal gain commands, mode traces and no-load bias check; analog calibration pending | [PC capture](captures/V4L_PC_ROUTE_2026-09-25.md) |
| Tuner bandwidth | Tab5 V3c, 2026-09-28 | Live AUTO/6 modes/AUTO, seven runs, one anomalous level | [report](captures/v3c_live_bandwidth_2026-09-28.md) |
| Band-select/rate matrix | V3c PC/Tab5, 2026-10-01 | 47-frequency PC sweep; bounded Tab5 RF/stream tests | [sweep](captures/nooelec_v5_band_sweep_2026-10-01.md), [Tab5 report](captures/tab5_rate_matrix_2026-10-01/README.md) |
| Disconnect/replug | Tab5 V3c, 2026-10-01 | Six cycles in candidate/harness conditions | [hotplug](captures/tab5_rate_matrix_2026-10-01/README.md) |
| 7.074 MHz FT8 | OrcSDR V3c, 2026-10-09 | Accepted reception on named band | [OrcSDR status](https://github.com/hardcoreerik/OrcSDR/blob/main/PROJECT_STATUS.md) |
| Other V3 revisions / broad soak | Not comprehensively reported | **Unverified** | [current truth](../PROJECT_TRUTH.md) |

## 8. OrcSDR integration

OrcSDR labels the tested **V3c** **fully supported / RF-verified**, while **earlier V3 hardware revisions remain individually verify / experimental** in its [2026-11-04 status](https://github.com/hardcoreerik/OrcSDR/blob/main/PROJECT_STATUS.md). It consumes the standalone driver's profile and capabilities. The 40 m FT8 result uses Q sampling; 99.1 MHz FM/RDS uses the native tuner. A dashboard open does not prove RF reception, and some physical V3 versions may lack the V3c bias arrangement. OrcSDR's documented pin (driver v0.9.5) does not prove its installed app was built from today's v0.9.6 driver.

## 9. Known limitations

Older V3 PCB/revision coverage is not equivalent to V3c; ambiguous chip ID identifies tuner family, not all board options. Manual gain remains nominal, physical bias-current/load safety is unqualified across the family, LF edges and tuner bandwidth analog response are not comprehensively measured. Extended standalone endurance and other-host tests remain distinct.

## 10. Engineering references

[Registry and probing](../private/rtl_profile.hpp) · [V3 USB controls](../private/transfers_blog_v3.hpp) · [R820 gain tables](../private/gain_r820t2.hpp) · [band select](../private/r820t2_band.hpp) · [bandwidth plans](../private/measured_tuner_bandwidth.hpp) · [stream/tune state machine](../src/esp_rtl_sdr.cpp) · [profile tests](../tests/host/test_profiles.cpp) · [V3c captures](captures/NOTES.md) · [Tab5 rate/hotplug](captures/tab5_rate_matrix_2026-10-01/README.md) · [v0.9.6 release](releases/v0.9.6.md).
