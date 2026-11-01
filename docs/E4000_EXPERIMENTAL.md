# E4000 experimental profile

## Status

**Experimental implementation — initial Tab5 FM/RDS reception verified;
broader hardware acceptance remains pending.**

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

## What is implemented

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

## Limits

- RF tuning range: **64 MHz–1.7 GHz**, from the E4000 datasheet.
- Tuner crystal: currently assumed to be **28.8 MHz** for this profile; confirm
  this on the target board before promoting the profile.
- Tuner gain, tuner AGC, RTL AGC, tuner bandwidth, direct sampling, HF
  upconversion and bias tee are **not exposed** by this profile.
- The E4000's nominal VCO range is 2.6–3.9 GHz. Some edge frequencies require
  a provisional divider fallback outside that nominal window; these are
  surfaced in the runtime log and require physical validation.
- No claim is made for sensitivity, absolute gain, frequency accuracy, spurious
  performance, or continuous-stream stability on the XTR v5.

## Validation required

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

## Reference

The tuner math and register meanings are based on the public
[Elonics E4000 datasheet](https://www.nooelec.com/store/downloads/dl/file/id/14/product/264/e4000_datasheet.pdf).
This implementation is independent of third-party tuner-driver source.

## RTL2832U zero-IF composition (experimental)

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

This is a documentation-derived experimental composition, not a measured XTR
sequence. First-party E4000 PC capture and physical I/Q reception remain required.
Host tests verify ordered writes and early failure at each transfer, not RF operation.
