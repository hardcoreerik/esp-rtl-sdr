# RTL-SDR Blog V4 Lite / R828S — receiver profile

## 1. Overview and implementation status

**Implemented; Tab5 hardware-tested, with bounded RF reception evidence.** Internal profile: `blog_v4l_r828s` (`ESP_RTL_SDR_PROFILE_BLOG_V4L`). V4 Lite has an R828S front end; it is **not** the V4's R828D nor the V3's Q-direct sampling board. OrcSDR classifies V4L as **fully supported** based on dated integration results. Current driver release at this review: **0.9.6**. See [current truth](../PROJECT_TRUTH.md) and [evidence vocabulary](STATUS_TERMINOLOGY.md).

## 2. USB identity and probing

| Item | Exact implementation |
|---|---|
| USB | Shared `0bda:2838`; VID/PID alone is never sufficient |
| EEPROM match | Exact `RTLSDRBlog` / `Blog V4L`, checked **before** `Blog V4` |
| Tuner | R828S; RTL2832U I2C control value `0x0034` (R820-like address is **not** a V3 identity) |
| Selector | `rtl_profile_from_descriptors()` in [registry](../private/rtl_profile.hpp) |
| Rejection | Generic `RTL2838UHIDIR`, `Blog V4`, Nooelec and other IDs must not silently become V4L |

Earlier V4L units were incorrectly classified as V3 because an R828S may answer an R820-like probe. Descriptor precedence prevents an invalid V3 direct-Q route. This distinction is tested in [host profile tests](../tests/host/test_profiles.cpp).

## 3. Initialization and RF architecture

V4L uses its measured R828S-reachable tuner path at bridge `0x34`, including the cold R820-family reinit slice where evidenced, but **never inherits the V4 GPIO/triplexer controls or the V3 direct-Q sequence**. Its matched low-IF path (approximately **1.815 MHz** in the 2.4 MS/s reference) is separately measured. First-party [2026-09-25 PC captures](captures/V4L_PC_ROUTE_2026-09-25.md) show native FM to AM/HF and back with corresponding tuner PLL, GPIO/input and demod IF state.

Below **28.8 MHz**, the V4L board's HF upconverter maps requested RF to **RF + 28.8 MHz**. An optional application-selected direct tuner route in the **24–28.8 MHz** portion bypasses the upconverter; neither choice should be confused with V3 Q-direct sampling. [V4L front-end plan](../private/measured_v4l_frontend.hpp) and [driver RF-route selector](../src/esp_rtl_sdr.cpp) are authoritative. The [2026-10-01 four-board sweep](captures/nooelec_v5_band_sweep_2026-10-01.md) indicates V4L tracking-filter switching differs from Nooelec/V3 near boundaries; additional fine scans remain open.

## 4. Frequencies, rates and tuning

| Property | Status |
|---|---|
| Software RF span | Shared driver **24 kHz–1.766 GHz** policy; not a physically certified V4L RF performance span |
| HF path | Below 28.8 MHz normally uses +28.8 MHz on-board converter; optional direct tuner route is narrower |
| Native tuner/low IF | At/above the converter transition uses R828S controls and matched PLL/demod IF |
| Recorded RF | PC AM 1.280 MHz voice and carrier check; Tab5/OrcSDR 7.074 MHz FT8 and FM96.1 MHz stages |
| Band-specific uncertainty | LF performance, native direct-route lower edge, sensitivity and actual analog passbands need more RF work |

The rate policy is shared RTL2832U demodulator programming: **225,001–300,000 S/s** or **900,001–3,200,000 S/s**, with resampler quantization; 300,001–900,000 S/s and 900,000 S/s are rejected. These are programmable windows, **not a per-device sustained-throughput guarantee**. Query `esp_rtl_sdr_get_supported_rates()` for recommendations, `esp_rtl_sdr_quantize_sample_rate()` for the applied rate and `esp_rtl_sdr_probe_rates()` for a host-specific rate passport. See [rate policy](RATES.md) and [public API](API_REFERENCE.md).

At 2.4 MS/s, captured filter/IF selection is coupled to the requested bandwidth. Never equate an IQ sample rate with the analog tuner bandwidth; [vendor DLL rate matrix](captures/vendor_dll_rate_matrix_2026-10-01.md) shows the programming differences.

## 5. Gain, AGC, bandwidth and antenna power

- **Manual gain:** `CAP_GAIN`, 29 nominal 0.0–49.6 dB requests were exercised in first-party **PC** FM and AM sweeps; these are register/API levels, not a calibrated measurement of RF gain.
- **Tuner AUTO:** `CAP_GAIN_AUTO` is distinct from RTL2832U digital `CAP_RTL_AGC`; both are represented by the V4L capability mask, with separate APIs.
- **Native tuner bandwidth:** `CAP_TUNER_BANDWIDTH`, AUTO and six explicit plans (200k/300k/500k/1.0M/1.8M/2.4M) at **2.4 MS/s**, with matched demod IF and tuner programming. At or below 28.8 MHz on the HF upconverter route (not direct tuner), only AUTO, 200 kHz, 500 kHz and 2.4 MHz are accepted; 300 kHz, 1.0 MHz and 1.8 MHz return `ESP_RTL_SDR_ERR_UNSUPPORTED`.
- **Bias tee:** V4L advertises `CAP_BIAS_TEE`. First-party PC captures include software OFF/ON and no-load voltage checks. No loaded output-current acceptance was recorded.
- **Unavailable:** direct sampling Q and E4000 hardware offset tuning. It does advertise `CAP_HF_UPCONVERTER`.

A live [Tab5 bandwidth regression](captures/v3c_live_bandwidth_2026-09-28.md) fixed an important IF sequencing issue: V4L **before** the fix shifted from -312 to +386 kHz in some stages; **after** the fix, the tested cold/replug sequence remained approximately within 2.5 kHz of requested carrier and AUTO returned near baseline. Historic failures remain historic; this result is not passband calibration.

## 6. IQ streaming, start/stop and hotplug

The common ESP-IDF High-Speed USB Host client manages enumeration, interface claim, tuner setup, RTL2832U rate programming, bulk-IN multi-URB streaming, and interleaved unsigned 8-bit I/Q (CU8). The public API supports callback, blocking-read and BOTH delivery; `start`, in-stream `retune_hz`, `stop`, detach and reattachment use the shared lifecycle. Retunes from event callbacks are queued; application-task retunes attempt the change immediately and can block during transfer draining. Metrics and health counters are available, but an API acknowledgment is not equivalent to measured RF quality. A hardware test for one profile, host or firmware image **must not** be copied to another. [API contract](API.md) · [USB recovery test plan](acquisition-reliability/TEST_PLAN.md) · [test harness](../examples/p4_serial_smoke/README.md).

V4L has named Tab5 IQ, rate, bandwidth and replug results. [2026-10-01 hotplug records](captures/tab5_rate_matrix_2026-10-01/README.md) report **six disconnects, six probes, six restarts**, with zero start failures/reboots/leaked URBs in the stated candidate/software setup; a separate console-text interleaving timeout must not be mistaken for USB failure. The shared 0.9.6 interrupted-start recovery changes are not a retrospective V4L full release soak.

## 7. Dated validation matrix

| Test | Hardware/date | Result | Evidence |
|---|---|---|---|
| Identity and separate tuner path | V4L PC, 2026-09-25 | Exact `Blog V4L`, R828S/0x34 route | [PC report](captures/V4L_PC_ROUTE_2026-09-25.md) |
| HF programming and AM reception | V4L PC, 2026-09-25 | +28.8 MHz programming, later AM1280 voice confirmed; initial IQ run invalid | [PC RF tests](captures/V4L_PC_ROUTE_2026-09-25.md) |
| Gain/bias controls | V4L PC, 2026-09-25 | 29 nominal gain requests; unloaded bias voltage only | [PC report](captures/V4L_PC_ROUTE_2026-09-25.md) |
| Bandwidth and IF | Tab5 V4L, 2026-09-28 | Live AUTO/6-band/AUTO cycle centered after fix | [live bandwidth report](captures/v3c_live_bandwidth_2026-09-28.md) |
| Rates and RF band test | Tab5 V4L, 2026-10-01 | B/Candidate IQ and RF comparison; neutral result at test site | [Tab5 A/B campaign](captures/tab5_rate_matrix_2026-10-01/README.md) |
| Hotplug/restart | Tab5 V4L, 2026-10-01 | Six disconnect/reprobe/restart rounds in bounded run | [hotplug report](captures/tab5_rate_matrix_2026-10-01/README.md) |
| 7.074 MHz FT8 | OrcSDR V4L, 2026-10-09 | Operator-accepted decoding | [OrcSDR status](https://github.com/hardcoreerik/OrcSDR/blob/main/PROJECT_STATUS.md) |
| All-band RF sensitivity / extended soak | Not established | **Pending** | [testing guide](TESTING_GUIDE.md) |

## 8. OrcSDR integration

At the [2026-11-04 OrcSDR status snapshot](https://github.com/hardcoreerik/OrcSDR/blob/main/PROJECT_STATUS.md), V4L is **fully supported / hardware-verified** with 40 m FT8 reception. OrcSDR consumes per-device driver capabilities to choose legal RF routes and dashboard behavior. Real FM/AM reception is evidenced at specific setups; not every decoded dashboard has been retested on V4L. OrcSDR's recorded dependency pin is v0.9.5, distinct from the driver's current v0.9.6 release; nightlies/source branches are not automatically tagged-release evidence.

## 9. Remaining limitations

No universal analog passband/sensitivity or gain accuracy measurement; no loaded bias capacity rating; lower-end optional direct-HF performance is not established; V4L tracking-filter boundaries differ from R820T2 and require targeted physical A/B. Generic USB identity or R820 chip-ID alone is not sufficient to claim a V4L board. Hardware results are not an unbounded endurance certification.

## 10. Engineering references

[Profile registry](../private/rtl_profile.hpp) · [R828S board route](../private/measured_v4l_frontend.hpp) · [IF/bandwidth plans](../private/measured_tuner_bandwidth.hpp) · [tuner controls](../src/esp_rtl_sdr.cpp) · [profile tests](../tests/host/test_profiles.cpp) · [2026-09-25 PC campaign](captures/V4L_PC_ROUTE_2026-09-25.md) · [2026-10-01 sweep](captures/nooelec_v5_band_sweep_2026-10-01.md) · [Tab5 rate/hotplug](captures/tab5_rate_matrix_2026-10-01/README.md) · [v0.9.6 notes](releases/v0.9.6.md).
