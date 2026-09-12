# Capture evidence — 2026-09-11 official-driver AGC loop discovery

## Purpose

Root-cause why the `blog_v3_r820t2` profile streams without crashing but
never locks a clean FM station, after ruling out capability-gating and
frontend/GPIO routing as the blocker (see CHANGELOG.md "Ruled out by
direct test"). Method: use the official RTL-SDR Blog Windows driver +
SDR++ as a behavioral oracle against the same physical hardware, captured
via Wireshark/USBPcap, per `CLEAN_ROOM.md`'s "Independent USB observation"
policy — not by reading librtlsdr source.

## Setup

- Host: this PC (NEWCOREPC), Windows, Wireshark 4.6.8 + USBPcap.
- Stimulus: SDR++ v1.3.0, RTL-SDR source module (bundled librtlsdr-based
  driver, via Zadig WinUSB), WFM mode, 96.1 MHz (KEZL — team's standard FM
  reference station), stereo + RDS decode on.
- Devices: real RTL-SDR Blog V4 (`RTLSDRBlog Blog V4`, serial `00000001`)
  and the V3c test unit (bare `RTL2838UHIDIR` descriptor, R860 tuner per
  packaging, serial `00000001`), one at a time, same PC port, same SDR++
  settings.
- Both confirmed receiving 96.1 KEZL cleanly (audio + RDS lock on V4;
  audio confirmed clean on V3c — "great" per live tester feedback) during
  capture.

## Files

| File | SHA-256 | Notes |
|---|---|---|
| `v4_official_driver_control_transfers_2026-09-11.txt` | (decoded text, committed) | Filtered control-transfer fields from the V4 capture; raw pcapng not committed (large) |
| `v3c_official_driver_control_transfers_2026-09-11.txt` | (decoded text, committed) | Same, for the V3c unit |
| V4 raw pcapng | `852d3e94f163755f97a8d960467e13c671ff6423ab3e242a7ebb94a062a942da` | Held locally by the tester (large; ~670 MB), not committed |
| V3c raw pcapng | `24225496695f426caf20539dbc8fbd38c2835a4e1a1b5e89325d47191e7ba774` | Held locally by the tester (large; ~224 MB), not committed |

Both raw pcapng captures were stopped by force-terminating tshark rather
than a clean capture stop, so each file's tail packet is truncated
(`tshark` reports "cut short in the middle of a packet"); everything
before the truncation point decoded cleanly and is unaffected.

## Findings

1. **RTL2832U I2C repeater bracketing, confirmed on real hardware**: both
   units show `wValue=0x0120, wIndex=0x0011, data=0x18` (repeater ON)
   immediately before a burst of tuner-address writes, and
   `wValue=0x0120, wIndex=0x0011, data=0x10` (repeater OFF) immediately
   after — exactly matching the publicly-known RTL2832U demod-page-1
   register-0x01 repeater enable/disable convention. Each write is
   followed by a settle read at `wIndex=0x000a`. This matches this
   project's own `kRtlInitTransfers[0..86)` bring-up prefix in shape
   (open→enable-before-tuner-traffic), independently corroborating the
   `run_demod_bringup()` fix already landed.
2. **Real, working V3c tuner init burst** (`wValue=0x0034` — R820T2/R860's
   real I2C address): registers `0x06,0x05,0x07,0x08,0x09,0x0a,0x0c,0x0f,
   0x11,0x17,0x19` written with real, verified values (see the V3c
   decoded file, frames 10294–10320) as part of a **known-good, confirmed
   reception** session — first-party evidence this project has never had
   for this tuner family.
3. **The actual mechanism that makes reception work is a continuous
   active AGC feedback loop, not a one-time init table** — on *both*
   V4 and V3c: write several gain/filter tuner registers (`0x10, 0x1a,
   0x12, 0x16, 0x15`-family), then a 1-byte write selecting register 0,
   then a **multi-byte read** (5 bytes, then 3 bytes, alternating) from
   `wIndex=0x0600`, repeating every few milliseconds while streaming.
   `esp_rtl_sdr` currently does nothing resembling this for any profile —
   `apply_tuner_agc_auto_records()` is a one-shot register write, not a
   loop, and never reads back a multi-byte status at all. This is the
   leading candidate for why V3c streams but never locks a station: the
   driver has no active gain-management loop, and R820T2/R860 appears to
   need one (or benefit from one) more than R828D's own freerunning
   internal AGC does.

## Gain-step experiment (SDR#, same V3c unit, two sessions)

Attempted to build a real R820T2/R860 gain-step table (same method used
historically for `kMeasuredV4GainSteps`): stepped AirSpy SDR# Studio's
manual RF Gain slider through the standard 28-point R82xx ladder (0.0,
0.9, 1.4, 2.7, 3.7, 7.7, 8.7, 12.5, 14.4, 15.7, 16.6, 19.7, 20.7, 22.9,
25.4, 28.0, 29.7, 32.8, 33.8, 36.4, 37.2, 38.6, 40.2, 42.1, 43.4, 43.9,
44.5, 48.0, 49.6 dB — RTL AGC and Tuner AGC both off), capturing
throughout, twice: once with the V3c behind a powered USB hub, once
plugged directly into the PC (to rule out supply-noise as a factor).

**Result: inconclusive for building a gain table, but a real negative
finding.** Tuner registers `0x05` and `0x07` (LNA/mixer gain in the
public R82xx scheme, and the pair `kMeasuredV4GainSteps` uses for V4)
are written repeatedly throughout both sessions, but their values
increment **monotonically and in lockstep** (`05:0x90→0x91→0x92→...`,
`07:0x60→0x61→0x62→...`) on a **fixed ~3-5 second timer**, identically
in both the hub and direct-connection sessions, and independently
confirmed by re-running with longer pauses between slider moves (same
drift, same cadence). This rules out: (a) USB hub power-quality as a
factor (identical behavior direct-connected), and (b) any simple
correlation between these two registers' values and the manual gain
slider position.

**Follow-up steady-state test resolved this.** Set gain once to 22.9 dB
(a value already reached partway through the earlier step sweep) and
left it completely untouched for ~40 s while capturing: **zero**
`wValue=0x0034` control-transfer writes of any kind occurred during that
entire window — not just reg 0x05/0x07, no tuner traffic at all. This
proves the earlier monotonic drift is **not** an independent background
timer, an AGC loop, or PPM/housekeeping tick — it only occurs while the
gain slider is actively being changed. The most likely explanation:
SDR#'s own gain-control code **ramps/smooths** the transition to a new
gain value over a few seconds (incrementing reg 0x05/0x07 by small steps
every ~3-5 s) rather than jumping directly, purely to avoid an audible
"pop" — a UI/driver-side transition behavior, not continuous tuner-side
gain management.

**Practical implication**: audible reception on this unit really did
require manually raising gain in SDR#/SDR++ to work at all (~32.8 dB,
AGC off) — that observation stands, and reg 0x05/0x07 likely *are* the
real LNA/mixer gain registers after all (matching V4's scheme). Building
a correct R820T2/R860 gain table is now a well-understood, mechanical
task: hold at each of the 28 standard dB steps long enough for the ramp
to fully settle (confirmed above: no more writes = settled), *then*
record the resting reg 0x05/0x07 values — not the values seen while the
ramp is still in flight, which is what the original rapid step-through
captured. Not done this session; a good, concrete next-session task.

## Next step (not yet implemented)

Decode the full multi-byte read payloads (not just request shape) from
both captures to determine what the loop is actually reading (RSSI?
demod lock/overload flags?) and what decision it drives, then implement
an equivalent loop in `esp_rtl_sdr.cpp` sourced from this evidence —
not from librtlsdr source, per `CLEAN_ROOM.md`. The gain-step experiment
above suggests this decode is necessary before a gain table can be built
correctly; simply copying register addresses by inspection was not
sufficient.
