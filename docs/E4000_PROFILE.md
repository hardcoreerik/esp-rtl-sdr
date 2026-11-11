# E4000 / Nooelec XTR — receiver profile

## 1. Overview and implementation status

**Implemented; ESP32-P4 hardware-tested on one Nooelec XTR v5.** Named Tab5/OrcSDR test candidates recorded FM stereo/RDS reception, hash-verified IQ streaming at multiple rates, physical offset OFF/ON/OFF comparisons, and bounded restart/hotplug cycles; those are not a full-band or current-release endurance guarantee. The source profile is `ESP_RTL_SDR_PROFILE_E4000`, with the historical internal name `e4000_experimental`. That string is retained for compatibility and is **not** today's overall implementation classification. The standalone driver's current release at this alignment is **v0.9.6**. RF calibration, all bands, extended lifecycle and endurance are independently open, not reasons to call the implementation missing. See [current truth](../PROJECT_TRUTH.md), [status terminology](STATUS_TERMINOLOGY.md) and the [October 31 Tab5 hardware report](validation/e4000-com39-2026-10-31/README.md).

The implementation arose in response to a real [OrcSDR user compatibility request #179](https://github.com/hardcoreerik/OrcSDR/issues/179): a user's **Nooelec SMArTee XTR v5** enumerated with the same USB identity as the supported NESDR SMArt V5, but its **Elonics E4000** could not answer the R820 probe and was correctly rejected. The solution was a **separate E4000 driver profile**, not renaming the dongle or treating the tuners as interchangeable. The issue is closed; dated community/operator claims still have different provenance from independent lab evidence.

## 2. USB identification and probing

| Field | Exact policy / observation |
|---|---|
| Shared USB | **VID:PID `0bda:2838`**; insufficient alone |
| Explicit descriptor matches | `Nooelec` plus product containing `SMArTee XTR v5` or exactly `SMArt XTR v5` |
| First-party unit | `Nooelec / SMArt XTR v5`, serial `82115932`, 2026-10-31 capture; other physical revisions remain individually unqualified |
| Fallback for ambiguous descriptor | Completed E4000 tuner register **0x02** read returning **0x40**; a failed or incomplete probe never matches |
| Bridge address | E4000 seven-bit **0x64**, encoded as RTL2832U USB control **`wValue=0x00c8`**; former `0x0064` value was incorrect |
| Explicit nonmatches | Distinct `NESDR SMArt v5` R820-family identity, other VID/PID pairs, unrelated tuners and an incomplete E4000 probe |

After reset, the probe tolerates the documented first E4000 dummy I2C NACK then retries selection and read. Details: [profile registry](../private/rtl_profile.hpp), [probe and tuner source](../src/esp_rtl_sdr.cpp), [host tests](../tests/host/test_profiles.cpp), [PC controls, frame 703/705](captures/e4000_xtr_2026-10-31/README.md). Successful USB enumeration **alone** is not E4000 tuner verification.

## 3. Tuner initialization, PLL and RF routing

E4000 has its own [`RtlTunerDriver` implementation](../src/esp_rtl_sdr.cpp) and [fractional-N PLL/band/filter planner](../private/e4000.hpp). It intentionally **does not replay R828D/R820 initialization or tuner-dependent shared sample-rate writes**. It uses a currently **assumed 28.8 MHz E4000 reference clock**; that specific board's independent measurement remains open. The driver sets the E4000's zero-IF path with demod `B1=1B`, enables both ADCs (`08=CD`) and programs the digital IF/NCO. These writes have settle reads and fail closed on transfer errors; their technical provenance is preserved in the appendix below.

The PLL planner selects VHF2 below 140 MHz, VHF3 below 350 MHz, UHF below 1,135 MHz and L band above; tracking filters and low-pass selection are tuner-specific. Register **0x78** preserves reserved bits (captured VHF `03`, L band `00`). Some frequencies require an output divider outside the **nominal 2.6–3.9 GHz VCO** window: the runtime warns, and those edge choices still need physical verification. An earlier new-tuner startup path skipped a **USB FIFO reset/release** record; host regressions and the subsequent bounded Tab5 run distinguish the corrected initializer from the failing one.

**Hardware offset tuning** (`CAP_OFFSET_TUNING`) is opt-in and programs tuner LO and RTL2832U digital center correction together, retaining the requested IQ center. It is **not** an external software mixer or an AM converter. The [offset trace](captures/e4000_xtr_2026-10-31/offset/README.md) is PC evidence; physical current-release offset quality needs further acceptance.

## 4. Frequency, sample rates and tuning limits

| Dimension | Current source / measurement |
|---|---|
| Native tuner RF | **64 MHz–1.700 GHz**, enforced in source from E4000 datasheet; not an all-band sensitivity certification |
| Below 64 MHz | **Rejected natively** by capability guard; no Q-direct sampling or onboard HF upconverter |
| Reference | **28.8 MHz assumed**, not independently verified for this board |
| Native zero-IF / offset | Zero-IF implemented; optional hardware offset when requested |
| Shared sample rates | **225,001–300,000 S/s** or **900,001–3,200,000 S/s**, quantized; not a per-device 3.2 MS/s soak guarantee |
| ESP32-P4 field checkpoints | **96.118 MHz** exact requested center (screen rounds to KZEL 96.1), Tab5 **2.4 MS/s** FM/RDS and IQ; separate candidate **960 kS/s and 3.2 MS/s** IQ captures also recorded on COM39 (October 31 audit) |
| Other bands | Broad RF edge/PLL lock, PPM, spurs and output level remain incompletely characterized |

Sample-rate policy is shared RTL2832U code; the E4000 tuner-dependent gain writes from the V4 captured table are **filtered out**. Use [rates](RATES.md) and [API reference](API_REFERENCE.md). No native tuner bandwidth choices are exposed. A bandwidth inferred from IQ rate is **not** a measured E4000 analog IF passband.

**External HF upconverter:** October 31 follow-up used a separately powered **Ham It Up Plus v2**, with approximately **125 MHz** conversion. OrcSDR tuned **126.600 MHz**, corresponding nominally to **1.600 MHz AM** after subtracting the converter LO; the user confirmed a local station. This proves **translated reception in that configuration**, not native E4000 HF ability, not automated driver converter detection, and not correct dashboard frequency labeling. [Preserved observation](validation/e4000-com39-2026-10-31/README.md).

## 5. Gain, AGC, bandwidth and antenna bias

| Control | Driver behavior |
|---|---|
| Manual tuner gain | **`CAP_GAIN`**, 14 captured nominal requests: **-1, 1.5, 4, 6.5, 9, 11.5, 14, 16.5, 19, 21.5, 24, 29, 34, 42 dB** |
| Tuner AUTO | **`CAP_GAIN_AUTO`**; E4000-specific mode fields, stored intent restored on start/retune |
| RTL2832U digital AGC | **`CAP_RTL_AGC` implemented** in **v0.9.5** ([PR #65](https://github.com/hardcoreerik/esp-rtl-sdr/pull/65)); independent from E4000 tuner AUTO |
| Offset | **`CAP_OFFSET_TUNING`**, opt-in hardware PLL/NCO route |
| Tuner bandwidth | **Not exposed** (`CAP_TUNER_BANDWIDTH` absent); fixed conservative IF configuration in source |
| Direct sampling and HF conversion | **Not exposed** (no `CAP_DIRECT_SAMPLING` or `CAP_HF_UPCONVERTER`) |
| Software bias-tee control | **Not exposed** (`CAP_BIAS_TEE` absent); the user's **SMArTee XTR** is reported with an **always-on hardware bias tee**, which must not be described as software-switchable or generalized to every XTR variant |

Manual steps are **nominal API gain labels**, not calibrated total analog gain. The PC matrix observed a plateau around nominal **29/34 dB**. Captured 2026-11-01 gain-baseline/startup-history work reduced one bounded fixed-gain before/after-restart difference from roughly **1.85 dB** to **0.0879 dB** on a particular conducted test, **one pair only**; broad AUTO and every-gain hardware acceptance remain open. Gain writes use masked read/modify/write/readback; failure during active streaming enters FAULT rather than delivering mislabeled IQ. See [complete gain evidence](E4000_GAIN_CONTROLS.md) and [capture trace](captures/e4000_xtr_2026-10-31/README.md).

## 6. CU8 IQ streaming and receiver lifecycle

The common ESP32-P4 High-Speed USB Host path supplies continuous interleaved unsigned 8-bit I/Q, multi-URB transfers, callback/read/BOTH delivery, retune, health/metrics, stop and detach/recovery state machine. Retunes from event callbacks are queued; application-task retunes attempt the change immediately and can block while transfers drain. The E4000 **does not** require OrcSDR to implement its tuner protocol. [Core source](../src/esp_rtl_sdr.cpp) · [API](API_REFERENCE.md).

The [2026-10-31 COM39 Tab5 run](validation/e4000-com39-2026-10-31/README.md) has three hashed **4,800,000-byte** IQ captures at 2.4 MS/s and a **60.71-second** observation advancing stream bytes with zero reported drops/overruns in that window. The cabled injected-tone tests in that initial run were **inconclusive** and must **not** be described as RF-tone calibration. Subsequently the antenna was swapped and a user confirmed FM audio, stereo and KZEL RDS.

The [October 31 E4000 completion audit](validation/E4000_REMAINING_GATES.md) separately records six hash-verified physical COM39 offset OFF/ON/OFF captures at 2.4 MS/s: a **20.14 dB** reduction in the measured center peak with a **-1.17 dB** tone-level difference, plus successful **960 kS/s and 3.2 MS/s** diagnostic IQ captures after a recorder-routing fix. One restart/reconnect and one uninterrupted hotplug cycle were recorded on a pre-recorder-fix candidate. These are meaningful ESP32-P4 tests, but not broad calibration, a long soak, or revalidation of every behavior on release 0.9.6.

Driver **0.9.6** contains interrupted-start recovery ([PR #69](https://github.com/hardcoreerik/esp-rtl-sdr/pull/69)) with focused host tests and a Tab5 Nooelec→XTR swap that returned to about 2.40 MS/s with zero reported drops/overruns. That hardware session involved an **OrcSDR-local ESP-IDF USB patch**; installing 0.9.6 alone does not supply it. Neither the patch-assisted swap nor the 60-second observation is an E4000 long-soak certificate. Full XTR repeated hotplug and multi-receiver acceptance remain distinct open gates.

## 7. Dated verification matrix

| Test | Hardware / date | Result and strict boundary | Evidence |
|---|---|---|---|
| E4000 USB/I2C identity | PC Nooelec SMArt XTR v5 serial 82115932, 2026-10-31 | `0bda:2838`, ID register `02=40`, bridge `0x00c8` | [PC captured controls](captures/e4000_xtr_2026-10-31/README.md) |
| Fractional-N/control RF matrix | PC XTR + tinySA attenuator, 2026-10-31 | Controlled source/tuner response and 14-step gain labels; PC reference, not ESP driver acceptance | [PC test report](captures/e4000_xtr_2026-10-31/README.md) |
| Startup and IQ delivery | ESP32-P4 Tab5 COM39 + XTR, 2026-10-31 | 2.4 MS/s, hashed IQ; first controlled injected-tone trial inconclusive | [COM39 report](validation/e4000-com39-2026-10-31/README.md) |
| Broadcast FM + RDS | Same physical Tab5/XTR and monopole, 2026-10-31 | User audio/stereo, KZEL 96.1 RDS observed; not all-band sensitivity | [COM39 acceptance](validation/e4000-com39-2026-10-31/README.md) |
| Short streaming interval | Tab5/XTR, 2026-10-31 | 60.71 s, advancing bytes, zero reported drop/overrun counters in observation | [soak summary](validation/e4000-com39-2026-10-31/README.md) |
| Offset/rate and bounded lifecycle | COM39/XTR, 2026-10-31 candidate audit | Six physical offset IQ captures at 2.4 MS/s, 20.14 dB center-peak reduction and -1.17 dB tone change; 960 kS/s / 3.2 MS/s IQ and one restart/reconnect and hotplug cycle each | [E4000 test-completion audit](validation/E4000_REMAINING_GATES.md) |
| Ham It Up AM reception | Tab5/XTR + separately powered upconverter, 2026-10-31 | 126.6 MHz tuned corresponds to nominal 1.6 MHz input, user heard station; manual translation | [AM observation](validation/e4000-com39-2026-10-31/README.md) |
| Gain baseline | COM39 XTR, 2026-11-01 | One fixed-14-dB restart comparison improved; other gain/AUTO gates pending | [gain report](E4000_GAIN_CONTROLS.md) |
| Stop/start, interrupted USB start | Host regressions and bounded Tab5 switch, 2026-11-09 | Driver 0.9.6 fix; patch-assisted lab observations, not complete per-XTR lifecycle | [0.9.6 notes](releases/v0.9.6.md) |
| Broad band edges, calibrated gain and long soak | Not recorded as completed | **Pending** | [remaining gates](validation/E4000_REMAINING_GATES.md) |

## 8. OrcSDR integration and user-request history

The [OrcSDR 2026-11-04 project status](https://github.com/hardcoreerik/OrcSDR/blob/main/PROJECT_STATUS.md) states **E4000 driver fully implemented / OrcSDR works within the tuner's range**. OrcSDR guards out-of-range tuning requests; an AM broadcast dashboard may open but **does not tune the XTR directly** below 64 MHz. A dashboard warning or idle waterfall is correct capability enforcement, not a failure of RF identification. A Ham It Up converter can make an input audible after separate hardware translation, but there is **no automated converter configuration or source-frequency remapping** for E4000 in the driver or the cited OrcSDR snapshot.

[OrcSDR issue #179](https://github.com/hardcoreerik/OrcSDR/issues/179) records the original independent user who identified the incompatibility. That report is valuable community/problem provenance; the separate [October 31 first-party PC](captures/e4000_xtr_2026-10-31/README.md) and [Tab5 lab](validation/e4000-com39-2026-10-31/README.md) tests supply narrower engineering evidence. OrcSDR status documents a v0.9.5 dependency pin at its snapshot; current standalone driver v0.9.6 does **not** imply every official OrcSDR image or nightly contains it.

## 9. Limitations and remaining acceptance

The XTR's reference clock remains assumed; full 64–1,700 MHz band/edge and PLL/filters, absolute RF gain/sensitivity, uncalibrated 29/34 dB plateau, broad manual/AUTO/RTL-AGC effects, long-duration soak, disconnect/reconnect under varied USB power, multi-unit receiver permutations, and board revisions need physical acceptance. An earlier unresolved USB-host crash and app-local SDK workaround are separately documented in [COM39 evidence](validation/e4000-com39-2026-10-31/README.md) and [0.9.6 release](releases/v0.9.6.md). No automatic external HF converter or software-switchable bias feature should be inferred.

## 10. Engineering sources, tests and dated issues

[Profile registry](../private/rtl_profile.hpp) · [E4000 PLL/gain math](../private/e4000.hpp) · [main tuner/zero-IF/stream source](../src/esp_rtl_sdr.cpp) · [profile host tests](../tests/host/test_profiles.cpp) · [first-party E4000 PC captures](captures/e4000_xtr_2026-10-31/README.md) · [offset controls](captures/e4000_xtr_2026-10-31/offset/README.md) · [gain investigation](E4000_GAIN_CONTROLS.md) · [COM39 field result](validation/e4000-com39-2026-10-31/README.md) · [remaining acceptance](validation/E4000_REMAINING_GATES.md) · [0.9.6 release](releases/v0.9.6.md) · [OrcSDR issue #179](https://github.com/hardcoreerik/OrcSDR/issues/179) · [E4000 RTL-AGC PR #65](https://github.com/hardcoreerik/esp-rtl-sdr/pull/65) · [startup-recovery PR #69](https://github.com/hardcoreerik/esp-rtl-sdr/pull/69).

---

## Historical engineering narrative (preserved and annotated)

The following content is the prior E4000 profile engineering narrative, retained for its dated implementation story and trace provenance. Phrases such as “experimental”, “acceptance pending” and “not exposed” in older dated work **describe that candidate's state, not today's status**. The present-day feature matrix and v0.9.6 distinctions above supersede stale claims. The original E4000 gain work also has its own dated [investigation](E4000_GAIN_CONTROLS.md).

### Status

**Implemented driver profile — initial Tab5 FM/RDS reception verified; broader RF-band, gain-accuracy, and lifecycle/soak acceptance remains open.**

The October 31 [COM39 report](validation/e4000-com39-2026-10-31/README.md)
records exact source/image provenance, high-speed enumeration, 2.4 MS/s
delivery, a one-minute observation, user-confirmed FM audio and decoded KZEL
RDS with an approximately one-foot monopole. This does not establish
calibrated sensitivity, all-band support or the inconclusive cabled-tone test.

This profile currently targets the Nooelec SMArTee XTR v5 descriptor, the
first-party captured Nooelec SMArt XTR v5 descriptor (serial 82115932), and
ambiguous devices using the shared `0bda:2838` identity only when an E4000 ID
probe succeeds. Other USB IDs are not accepted yet. It is not a claim that
every E4000-branded receiver has identical RF routing or clocking.

### What is implemented

- Exact Nooelec / SMArTee XTR v5 descriptor recognition.
- Exact Nooelec / SMArt XTR v5 recognition from the October 31 PC campaign;
  see [the capture report](captures/e4000_xtr_2026-10-31/README.md).
- E4000 ID probing on the existing shared `0bda:2838` USB identity when the
  product descriptor is ambiguous.
- A separate E4000 register-control path, without R820T2/R828D init or tune
  tables.
- Datasheet-based fractional-N PLL calculation, frequency-band selection,
  RF tracking-filter selection, zero-IF setup and a conservative fixed IF
  filter configuration.
- Reuse of the shared RTL2832U USB, sample-rate and IQ streaming path.
- Captured 14-step manual tuner gain and tuner AUTO controls, using the existing
  public gain APIs. See [gain controls](E4000_GAIN_CONTROLS.md) for evidence and
  the still-open physical acceptance gate.

### Limits

- RF tuning range: **64 MHz–1.7 GHz**, from the E4000 datasheet.
- Tuner crystal: currently assumed to be **28.8 MHz** for this profile; confirm
  this on the target board before promoting the profile.
- Tuner bandwidth, direct sampling, HF upconversion and software-switchable bias tee were not exposed by this profile; RTL digital AGC was added in v0.9.5.
- The E4000's nominal VCO range is 2.6–3.9 GHz. Some edge frequencies require
  a provisional divider fallback outside that nominal window; these are
  surfaced in the runtime log and require physical validation.
- No claim is made for sensitivity, absolute gain, frequency accuracy, spurious
  performance, or continuous-stream stability on the XTR v5.

### Validation required

October 31 first-party PC controls establish RTL2832U I2C control address
`0x00c8` for E4000 (seven-bit `0x64` shifted left). In the verified controls
trace, frame 703 selects ID register 02 and frame 705 returns 40 with status
zero. Probe, registry and initialization/tuning use the same profile address.
The previous unshifted `0x0064` value was incorrect. Host profile and actual
initializer/tune trace regressions cover the corrected address; ESP hardware
acceptance remains a separate gate.

PC controls frames 1785 and 2801 also establish tuner bias register 78 values
03 (VHF) and 00 (L-band). Datasheet section 1.11/table 4 assigns only bits
[1:0]; upper bits are reserved. The planner previously produced 0f/0c and now
preserves those reserved bits as zero. Profile and actual-tune trace tests
cover this correction. It is not yet included in the b7cb81a FM/RDS hardware
result and must not inherit that candidate's acceptance automatically.

The October 31 P4 run accepted the corrected identity/address and initialized
the tuner, but exposed a skipped USB FIFO release in the new-tuner startup
path. PC delivery trace frames 1887/1889 establish the shared 2148 reset/release
pair. The new-tuner init path now completes that pair without requiring
acquisition metadata. Actual-function tests reject a missing release and
inject failure into both transfers. RF streaming acceptance is still pending.

1. Confirm `rtl_test -t` reports E4000 on the XTR v5 and save the full output.
2. Confirm the USB descriptors with `rtl_eeprom`; do not write to the EEPROM.
3. On ESP32-P4, capture cold boot, first tune, retunes across VHF/UHF/L-band,
   stream start/stop, unplug/replug and a sustained IQ soak.
4. Verify PLL lock and received signals at several frequencies against a
   known-good desktop receiver.
5. Verify the other supported dongles still pass the existing smoke, tuning,
   gain/bias, hotplug and streaming regression checks.
6. Only then promote E4000 RF acceptance from pending to hardware-verified.

### Reference

The tuner math and register meanings are based on the public
[Elonics E4000 datasheet](https://www.nooelec.com/store/downloads/dl/file/id/14/product/264/e4000_datasheet.pdf).
This implementation is independent of third-party tuner-driver source.

### RTL2832U zero-IF composition (implemented; validation scoped)

The E4000-only setup enables zero-IF with page-1 B1=1B, enables both ADCs
with page-0 08=CD, then clears the digital IF offset. Each write has a settle
read, and a failed transfer aborts the sequence. Existing tuner setup is unchanged.

Derivation: first-party Nooelec controls capture frames 215, 247 and 251 show
B1=1B followed by the low-IF B1=1A / 08=4D state. The proposed dual-ADC value
preserves the captured 08 lower bits and enables bit 7 alongside bit 6.
The Realtek-authored RTL2832U rev. 1.4, sections 9.1-9.3, corroborates these
ADC/baseband bits. [Document mirror](https://homepages.uni-regensburg.de/~erc24492/SDR/Data_rtl2832u.pdf).
That document is marked confidential and copyright Realtek; it is not bundled
or reproduced here. No foreign driver source was imported.

The original composition was documentation-derived. The 2026-10-31 first-party
XTR reference now independently records B1=1B; physical FM/RDS and translated
AM reception are documented in `validation/e4000-com39-2026-10-31/`.
Those observations do not validate every ADC setting or every RF band.

Opt-in hardware offset tuning and its first-party PC evidence are described in
[the offset report](captures/e4000_xtr_2026-10-31/offset/README.md). This preserves
the effective requested IQ center while moving tuner LO/DC away from it; no
application offset mixer should be added. The offset correction and gain controls are implemented; the dated candidate still needs
current-release firmware and physical comparison. Complete E4000 model qualification remains open.

