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
| `v3c_gain_table_28step_10s_2026-09-11.txt` | (decoded text, committed) | Full-ladder gain-step retest, ~10s hold per step; see "Gain-step experiment" below |
| V3c gain-table raw pcapng | `3d2eef6b23c3a8e6dc902f3f783ac27996c1075eb80b88c10a6884876e63a1c0` | Held locally by the tester (large; ~1.58 GB), not committed |

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
3. **Superseded hypothesis (recorded here to preserve the investigation):**
   the initial capture review suggested that the mechanism making reception
   work was a continuous
   active AGC feedback loop, not a one-time init table** — on *both*
   V4 and V3c: write several gain/filter tuner registers (`0x10, 0x1a,
   0x12, 0x16, 0x15`-family), then a 1-byte write selecting register 0,
   then a **multi-byte read** (5 bytes, then 3 bytes, alternating) from
   `wIndex=0x0600`, repeating every few milliseconds while streaming.
   `esp_rtl_sdr` currently does nothing resembling this for any profile —
   `apply_tuner_agc_auto_records()` is a one-shot register write, not a
   loop, and never reads back a multi-byte status at all. This is the
   leading candidate at that point for why V3c streamed but did not lock a station: the
   driver has no active gain-management loop, and R820T2/R860 appears to
   need one (or benefit from one) more than R828D's own freerunning
   internal AGC does. Later controlled captures disproved the continuous-loop
   interpretation, and the matched 3.570 MHz tuner/demodulator IF repair produced
   correct physical FM reception. Gain calibration remains a separate open item.

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
real LNA/mixer gain registers after all (matching V4's scheme).

**Attempted retest with ~10s hold per step (full 28-step ladder,
`v3c_gain_table_28step_10s_2026-09-11.txt`, ~324s total): inconclusive,
does not confirm the "ramp settles per step" model either.** Distinct
reg 0x05/0x07 value *changes* (not re-writes of the same value) occurred
roughly every 20-25 seconds throughout the entire session — i.e. slower
than, and not clearly aligned with, the tester's ~10s-per-step pacing.
Only 16 distinct register values were reached across all 28 steps. This
is consistent with neither "one settle-step per user gain change" nor
"independent fixed-period timer" cleanly. Combined with the earlier
confirmed zero-writes-when-fully-idle result, the honest state is: reg
0x05/0x07 change in response to *something* about ongoing UI/driver
activity, but the exact triggering condition and step-to-value mapping
is still not understood from write-side observation alone.

**Recommended next angle**: decode the multi-byte *read* responses
(5-byte and 3-byte, from `wIndex=0x0600`) instead of continuing to
infer meaning from the write-side byte pattern — those are more likely
to carry a directly interpretable value (e.g. an RSSI/signal-level
readback) than reverse-engineering what appears to be an internal
counter on the write side. Not done this session.

## Next step (not yet implemented)

Decode the full multi-byte read payloads (not just request shape) from
both captures to determine what the loop is actually reading (RSSI?
demod lock/overload flags?) and what decision it drives, then implement
an equivalent loop in `esp_rtl_sdr.cpp` sourced from this evidence —
not from librtlsdr source, per `CLEAN_ROOM.md`. The gain-step experiment
above suggests this decode is necessary before a gain table can be built
correctly; simply copying register addresses by inspection was not
sufficient.

## PLL/tuning capture — 2026-09-11 (crystal frequency discovery)

### Purpose

`esp_rtl_sdr`'s PLL programming (`encode_r820_pll()`, `kRtlFinalTuneTemplate`)
was written and measured entirely against a real Blog V4 (R828D tuner). It
has been reused unconditionally for every profile, including BlogV3
(R820T2/R860), with zero independent verification that a different tuner
chip needs the same reference-crystal constant. Real-hardware test tonight
("just static" on 96.1 FM at every gain setting after the gain-path fixes)
raised the question directly: is V3c's PLL landing on the right frequency
at all? Method: same behavioral-oracle approach as the gain-loop capture
above — official RTL-SDR Blog Windows driver + USBPcap against the real
V3c unit, not librtlsdr source.

### Setup

- Host: this PC, Windows, Wireshark/tshark 4.6.8 + USBPcap (`USBPcap4`
  carried this device's traffic in this session; USBPcap channel numbering
  is not stable across sessions — identify by traffic volume/vendor ID,
  don't assume the same interface number next time).
- Stimulus: official RTL-SDR Blog Windows driver v1.4.0 (`rtl_sdr.exe`),
  one-shot `-f <freq> -s 2048000 -n <N>` runs, V3c plugged directly into
  the PC (not the Tab5).
- Frequencies swept: 88.1, 90.1, 92.1, 94.1, 96.1, 98.1, 100.1, 102.1,
  104.1, 106.1, 107.9 MHz — the FM broadcast band in exact 2 MHz steps,
  chosen specifically so the step size matches likely PLL resolution and
  makes integer-vs-fractional divider behavior easy to separate.
- Note: `rtl_sdr.exe` briefly logs `[R82XX] PLL not locked!` on every run
  before settling and printing `Tuned to <freq> Hz` — this is the
  *official* driver's own transient warning on this specific unit, not
  something introduced by our driver.
- Each run's I2C writes to the tuner (I2C addr `0x34`, `wIndex=0x0610`)
  were extracted with `tshark -Y "usb.bmRequestType==0x40 &&
  usb.setup.wIndex==0x0610" -T fields -e usb.setup.wValue -e
  usb.data_fragment`. Each write's data payload is `[register, value]`.

### Findings

Each capture contains two "tune-shaped" register clusters (writes to
0x10, 0x14, 0x12, 0x16, 0x15 in that order): a **first, frequency-invariant
calibration cluster** (always `10=8c 14=84 12=06 16=1c 15=72` regardless of
target frequency — a fixed internal calibration reference, not the user's
requested frequency) and a **second, frequency-dependent final-tune
cluster**. Only the second cluster was used below.

Final-tune register values (reg 0x10 / 0x14 / 0x15 / 0x16), by frequency:

| Freq (MHz) | reg0x10 | reg0x14 | reg0x15 | reg0x16 |
|---|---|---|---|---|
| 88.1  | 0x84 | 0x49 | 0x82 | 0xed |
| 90.1  | 0x84 | 0xc9 | 0xf6 | 0x09 |
| 92.1  | 0x84 | 0x0a | 0x66 | 0x26 |
| 94.1  | 0x84 | 0x4a | 0xd8 | 0x42 |
| 96.1  | 0x84 | 0x8a | 0x4a | 0x5f |
| 98.1  | 0x84 | 0xca | 0xbc | 0x7b |
| 100.1 | 0x84 | 0x0b | 0x2e | 0x98 |
| 102.1 | 0x84 | 0x4b | 0x9e | 0xb4 |
| 104.1 | 0x84 | 0x8b | 0x10 | 0xd1 |
| 106.1 | 0x84 | 0xcb | 0x82 | 0xed |
| 107.9 | 0x64 | 0x44 | 0xc2 | 0xf6 |

**Crystal frequency, confirmed.** `esp_rtl_sdr.cpp`'s existing
`(si2c<<6)|ni2c` packing (already used for reg 0x14, called `r20`
internally) matches reg 0x14's real bit layout exactly: rotating each
byte to extract `ni2c`/`si2c` and reconstructing `packed = ni2c*4+si2c`
gives a value that increases by **exactly +1 per 2 MHz step** across
92.1-106.1 MHz (8 consecutive points, zero exceptions). Solving
`div/(2*xtal) = 1/2,000,000 Hz` with `div=32` (the mixer divider active
across this whole range, confirmed separately below) gives
`xtal = 32,000,000 Hz` exactly — not the 28.8 MHz value
(`kRtlXtalHz`/`kMeasuredV4XtalHz`) measured against V4 and reused
unconditionally until tonight. (88.1 and 90.1 deviate slightly from the
clean +1/2MHz progression — likely residual settling from being the
first two tunes right after direct-sampling-mode was disabled; the 8
consecutive clean points from 92.1-106.1 are the reliable evidence.)

**Register assignment was already correct — this was a live
misdiagnosis, corrected in the same session.** Earlier in this
investigation the register *location* was suspected wrong (reg 0x14
looked "misplaced" when compared against a miscomputed 28.8 MHz
prediction). Re-checking `kRtlFinalTuneTemplate`'s own patch indices
confirms index 13 already targets reg 0x14 for the packed N-divider byte,
exactly matching real hardware. The only actual bug was the crystal
constant.

**Mixer-divider boundary, confirmed independently.** Reg 0x10 holds
`active = (((mix_log-1)&7)<<5)|0x04` — this driver's own existing formula
for the mixer-divider-select byte. It is `0x84` (divider=32) for every
frequency 88.1-106.1 MHz, then switches to `0x64` (divider=16) at 107.9
MHz. Plugging divider=32/16 into the existing formula reproduces `0x84`/
`0x64` exactly. The `1.77e9-3.90e9` Hz VCO candidate window
(`encode_r820_pll()`'s `kMixCandidates` loop) was NOT independently
re-derived tonight — it happens to still select the observed real divider
correctly across this sweep, but the true valid VCO range for R820T2 vs.
R828D has not been confirmed, so this window may be coincidentally
correct rather than verified. Flagging as an open assumption.

**NOT resolved — fractional bytes (reg 0x15/0x16).** These do not
correlate cleanly with frequency even after correcting the crystal
constant: at an exact 2 MHz step size (which should shift the fractional
remainder by *zero* if reg 0x14's integer byte alone fully explains the
+1-per-step behavior), reg 0x15/0x16 vary substantially and
non-monotonically across the sweep (see table above). An interleaved
single-byte I2C read (`wIndex=0x0600`, response `0x07` observed once)
immediately follows each write cluster, consistent with a closed-loop VCO
calibration search (write guess → read lock/cal status → retry) rather
than a static fractional-divider value — which would also explain the
official driver's own `[R82XX] PLL not locked!` transient warning on
every run. This was not decoded further this session (would need
request/response frame pairing across many more retunes, ideally
including repeated captures at the *same* frequency to test whether
these bytes vary run-to-run even with no frequency change, which would
confirm the calibration-search theory conclusively).

### What was fixed vs. what remains provisional

Fixed in `codex/usb-enum-fault-guard` (commit `3669d88`): the crystal
constant for BlogV3 only, via new `rtl_profile_pll_xtal_hz()`. This
should bring V3c's PLL from "wrong crystal, wrong integer N, off by an
unknown and possibly large amount" to "right crystal, right integer N,
fractional bytes still using the V4-derived formula/IF-offset guess" —
i.e. within roughly one fractional-divider step (a few hundred kHz to
low-MHz, depending on the true IF offset) of the correct frequency,
not exact.

NOT fixed, still open:
- The true IF offset (`kRtlIfOffsetHz`) for V3c — still using V4's
  1,814,972 Hz value, unverified for this chip.
- The fractional bytes (r21/r22) — still computed via the existing
  formula, which this sweep shows does not match real hardware's
  actual reg 0x15/0x16 behavior.
- The VCO candidate window bounds — coincidentally still selecting the
  right divider in this sweep, not independently re-derived.

### Recommended next angle

Capture the same frequency 3-5 times in a row (not swept) and diff reg
0x15/0x16 across repeats. If they vary run-to-run at a *fixed* frequency,
that conclusively confirms the calibration-search theory over a static
formula, and the next step becomes decoding the read-back byte's bit
meaning (pair request/response frames properly, e.g. via
`usb.request_in`) rather than continuing to search for a closed-form
fractional-N formula that may not exist.

## PLL follow-up — 2026-09-11 (calibration-search theory disproven; real fix found)

The "recommended next angle" above was run the same night. Result:
**the calibration-search theory was wrong**, and the real bug (and fix)
is different from what the section above concluded.

### Repeat-frequency test

Captured 5 consecutive `rtl_sdr.exe -f 96100000` runs in a row (fresh
USBPcap capture each time). Extracted the final-tune register cluster
from each:

```
repeat 1-5 (all identical): 10=84 14=4a[14=4a in first sub-cluster]/8a 12=06 16=4a/5f 15=aa/4a
```

Byte-for-byte **identical across all 5 repeats**. Reg 0x15/0x16 are
fully deterministic — not a calibration-search artifact. The earlier
conclusion was wrong; it came from mis-combining the bytes, not from
genuine non-determinism.

### Corrected regression

Re-analyzed the 11-point FM-band sweep (from the section above) treating
reg 0x14's packed byte via the SAME `(si2c<<6)|ni2c` unpacking this
driver already uses (`packed = nint-13`), combined with reg 0x15/0x16 as
`nfra = (reg16<<8)|reg15` — exactly this driver's existing
`r22*256+r21` structure. This combined 24-bit value is **perfectly
linear against frequency** (constant ~72,818/2 MHz step, vs. wildly
inconsistent when reg 0x14 was checked in isolation without its
fractional carry — the earlier session's mistake).

Linear regression solves to:
- **xtal = 28.8 MHz** — confirms the *original*, V4-derived value was
  correct all along. The previous section's "32 MHz" conclusion was
  wrong, caused by ignoring the fractional carry from reg 0x15/0x16
  when checking reg 0x14 alone.
- **IF offset = 3,570,000 Hz** — confirmed independently at three
  widely-spaced frequencies (88.1, 96.1, 106.1 MHz), all solving to
  within a few Hz of exactly 3.57 MHz, the standard RTL2832U/R820T
  default IF. V4's board-specific 1,814,972 Hz (a different, measured
  value reflecting V4's triplexer/filter board) was never correct for
  plain R820T2/V3c hardware.

Plugging both corrected constants back into this driver's *existing*
`encode_r820_pll()` formula (register assignment, packing, and
divider-select logic all unchanged) reproduces **6 of 11 real captured
register bytes exactly**, and the remaining 5 to within **1 LSB
(~27 Hz)** — a rounding-mode nuance (round-to-nearest vs. some other
tie-breaking rule in the real chip's own firmware), not a real error.
27 Hz is far below anything that matters for FM demodulation.

### Conclusion

The only real, load-bearing bug was **`kRtlIfOffsetHz`**, not the
crystal, and not the register map. Fixed in commit `ff1f07c`:
`rtl_profile_pll_if_offset_hz()` returns 3.57 MHz for `BlogV3`
specifically; `rtl_profile_pll_xtal_hz()` now unconditionally returns
28.8 MHz for all profiles (the 32 MHz branch from commit `3669d88` was
reverted as incorrect). `NooelecSmartV5` is deliberately left on the
V4-derived IF offset default — it has never been hardware tested.

Not yet flashed/verified on the real Tab5 as of this note being
written — next step is exactly that: overlay this commit, rebuild,
flash, and listen on 96.1 FM with the V3c.

## Matched-IF correction — 2026-09-11 (PLL-only conclusion was incomplete)

### New physical symptom after the PLL fix

The PLL-only change in `ff1f07c` did make V3c FM reception possible, which
invalidated the earlier "no reception" status. It also exposed a repeatable
frequency displacement:

| OrcSDR displayed | Station actually received | Approximate receiving dial |
|---|---|---|
| 96.1 MHz | KEZL 96.1, confirmed by audio/RDS | ~94.33 MHz |
| 99.1 MHz | The Beat 99.1, confirmed by audio/RDS | ~97.33 MHz |

The UI and RDS were not contradicting each other. They described opposite sides
of an analog/digital IF mismatch.

### Root cause and exact relationship

`rtl_profile_pll_if_offset_hz(BlogV3)` correctly moved the tuner PLL to a
3,570,000 Hz IF, but the shared initialization table later left the RTL2832
demodulator at the Blog V4-derived approximately 1,814,972 Hz IF. Therefore:

```
3,570,000 Hz - 1,814,972 Hz = 1,755,028 Hz
```

That 1.755028 MHz mismatch matches the roughly 1.77 MHz station displacement;
the small remainder is within the way the user found the station and OrcSDR's
existing bounded FM auto-centering behavior. The PLL math was correct, but a
correct tuner PLL is not sufficient unless the RTL2832 demodulator is configured
for the same IF.

### Official-capture provenance

The official-driver capture already contained the required final RTL2832 state
in `v3c_official_driver_control_transfers_2026-09-11.txt`:

- frame 11816: write `0x1920`, value `0x38`;
- frame 11818: settle read;
- frame 11820: write `0x1A20`, value `0x11`;
- frame 11822: settle read;
- frame 11824: write `0x1B20`, value `0x12`;
- frame 11826: settle read.

The same exact six records already existed as `kRtlInitTransfers[98..103]`.
The repair replays them for `BlogV3` after sample-rate setup and before initial
tuning. This is a clean-room reuse of first-party observed traffic, not a value
copied from another RTL-SDR implementation.

### Scope deliberately left unchanged

- 28.8 MHz crystal evidence and the 3.570 MHz V3c PLL work are preserved.
- Hot retune does not repeat the demodulator writes; stream initialization owns
  the stable demod IF state.
- Blog V4 executes no added USB records and retains its existing matched
  1.814972 MHz path.
- Nooelec retains its prior behavior because its IF has not been measured.
- OrcSDR display frequency, station data, FM auto-centering, tuner registers,
  gain behavior, and public driver API are unchanged.

### Test baseline and post-fix validation

The PowerShell and shell runners previously built two host executables but only
ran `esp_rtl_sdr_host_tests`. That false-green baseline was:

- policy: 373 passed, 0 failed;
- profile, when invoked manually: 125 passed, 1 failed because its Blog V3
  `CAP_GAIN` expectation was stale relative to the source;
- truth hygiene: passed.

The runners and Windows CI now use CTest. Current local results are:

- policy: 373 passed, 0 failed;
- profiles: 163 passed, 0 failed, including exact `38/11/12` write/read sequence,
  Blog V3 matched IF, unchanged Blog V4 IF policy, and no Nooelec override;
- truth hygiene: passed for 0.8.0-rc2;
- ESP32-P4 driver smoke compile: passed with ESP-IDF 5.5.4.

### Hardware results from the exact candidate

Driver commit `ae100781af355dfa5caf182fcddcb479789491e5` was overlaid without
changing OrcSDR's manifest/lockfile, built against OrcSDR
`4a13305790f3e13abcd29d38f76199d3d8fa7249` using ESP-IDF 5.5.4, and flashed as
an application image with SHA-256
`8513b586a160d90f20f52537cdca29433dc90059ef47ffb100bbf891eb8c3794`.

The first exact-frequency V3c test passed:

```
RTL_FM_ENTER display=99100000
tune rf=99113000 Hz ... pll_if_hz=3570000
RTL_HOT_TUNE display=99100000 lo=99113000 ok
RTL_FREQ_STATUS band=FM frequency_hz=99100000 mode=WBFM
RDS_STATUS ... block_locked=1 ... ps="EUGENE  "
  rt="99.1 The Beat of Eugene " pi=355F ...
  driver_overruns=0 driver_drops=0
```

The +13 kHz internal LO nudge is normal FM auto-centering and is inside the
existing +/-40 kHz limit. It is not the former 1.755028 MHz IF mismatch. IQ byte
and block counters advanced between status polls, with zero overruns/drops.

The same V3c then received the expected 96.1 MHz station at a displayed
96.100 MHz with a 96.113 MHz internal LO. The user confirmed correct reception;
the sampled status had carrier but had not yet acquired RDS block lock. Streaming
continued with zero overruns/drops.

The V3c was replaced by a real Blog V4 without resetting the Tab5. The continuous
log recorded orderly V3c detach, V4 selection as `blog_v4_r828d`, the unchanged
V4 initialization, and `pll_if_hz=1814972`. Re-entering the FM dashboard started
the new stream at 96.1 MHz; RDS locked to `KZEL` with PI `5277`. At 99.1 MHz,
RDS identified `99.1 The Beat of Eugene`; existing auto-centering settled at
99.097 MHz (3 kHz from the request), inside the +/-40 kHz bound. Both V4 checks
reported zero driver overruns and zero consumer drops.

Replacing the V4 with the V3c completed the reverse hotplug check. The driver
selected `blog_v3_r820t2`, emitted the matched-IF milestone with both IFs at
3.570 MHz and six restore records, and restarted at the saved 99.097 MHz. RDS
locked to `99.1 The Beat of Eugene` with PI `355F`; counters advanced with zero
overruns/drops. The Tab5 then completed a battery-only cold boot with the V3c
attached and received the saved station correctly. That boot is user-observed,
not serial-captured, because USB-C/COM17 was disconnected for the test.

The continuous serial portion is preserved as
`v3c_matched_if_hardware_2026-09-11.log` (SHA-256
`9aa8a058115d9150907c243acf75e7de67d503dc91964a365f8521e474ea9bfb`). It covers
the exact V3c hot retunes, V3c-to-V4 swap, V4 96.1/99.1 regression, V4-to-V3c
swap, matched-IF reinitialization, and V3c 99.1 RDS recovery.

This hardware-accepts the targeted V3c matched-IF repair and the V4 tuning
non-regression on the exact candidate. A V3c 96.1 RDS lock was not captured;
gain calibration remains separate. The live V3c-to-V4 swap also showed an
OrcSDR app behavior outside this driver fix: the newly attached V4 was ready but
remained idle until the active FM dashboard was left and re-entered.
