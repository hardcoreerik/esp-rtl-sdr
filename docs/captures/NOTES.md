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

## Next step (not yet implemented)

Decode the full multi-byte read payloads (not just request shape) from
both captures to determine what the loop is actually reading (RSSI?
demod lock/overload flags?) and what decision it drives, then implement
an equivalent loop in `esp_rtl_sdr.cpp` sourced from this evidence —
not from librtlsdr source, per `CLEAN_ROOM.md`.
