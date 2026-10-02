# Tab5 sample-rate / band A/B runs, Blog V4, V4L, V3c and Nooelec v5, 2026-10-01

Tab5 (ESP32-P4) with a Blog V4 and the ADS-B dipole in the east window, the same antenna as the PC captures. One-second pre-DSP IQ per
cell, scored by `rtl-gate/band_snr.py`. All runs use the same OrcSDR build apart from the driver pin (OrcSDR `main` at #149 plus one
line so the IQ capture records at an RF Lab rate; branch `claude/tab5-rate-matrix`). Tool: `run-tab5-rate-matrix.ps1`.

| Column | Driver | Pin |
|---|---|---|
| A0 | v0.9.1 | `105caa5` |
| A | v0.9.2 as published (master) | `139d70a` |
| B | release candidate `rc/v4-band-test`: master + #44, #43, #50, #49 + the V4/V4L band change | `b91c4f0` |

Gain: the runs named `*_agc.json` left the tuner AGC as the app set it; its state drifted between identical runs (FM clipping 9% to 48%), so
they are only good for stream health and pass/fail. All other runs use one fixed test gain (19.7 dB), restored to AUTO afterwards.

## Results

- **Stream health (all builds, all rates 960k to 3.2M):** effective rate within 0.2% of requested, no overruns, no drops. DSP load stayed at 17% at
  3.2 MS/s with an empty queue, so display and DSP load did not affect the stream.
- **250 kS/s:** A0 and A fail to start on the Tab5 (`RTL_START ESP_RTL_SDR_ERR_BAD_RATE rate=250000`, stream stays IDLE). B starts and streams at
  0.249 MS/s. This is the hardware confirmation of the low-band rate fix (#43).
- **Fixed-gain A vs B, 960k to 3.2M at 96.1 / 433.92 / 1090 MHz (`compare_A_vs_B_g197.txt`):** no cells lost, noise floor -0.16 dB on average, no
  clipping change.
- **433.92 and 453.925 MHz, 4 interleaved runs (B, A, B, A; 3 captures each; `*_rep-*.json`):** no reliable difference. An 11 dB peak-over-noise
  reading at 433.92 MHz in the first A/B pair was one captured burst and did not repeat (B 1.9 to 3.1 dB, A 1.8 to 7.2 dB).
- **VHF, 4 interleaved runs (A, B, A, B; 3 captures each; `*_vhf-*.json`):** at **230 MHz** B reads 11.5 to 14.5 dB peak over noise and A reads 7.8 to
  8.3 dB in both pairs, with 4 to 6 strong bins on B against none on A. The noise floor rose 1 to 2 dB, so the net signal-to-noise gain is about 3 dB.
  At 118.9, 162.55 and 178 MHz A and B overlap or differ in opposite directions between runs: no reliable difference.

Reading: the V4 band change helps where the old route's `1b` was most mismatched (it wrote `34`, the 96 MHz setting, at 230 MHz and now writes
`13`), and does nothing measurable elsewhere. One site, one antenna, one dongle.

## Blog V4L (same method; `blog_v4l-*.json`)

- **250 kS/s:** fails on A (`NO_IQ`, stream never starts), works on B. Same result as the V4 (#43).
- **Rate sweep (250k, 960k, 2.048M, 2.4M, 3.2M at 96.1 / 433.92 MHz, plus 1090 MHz at 2.048M):** A 9 of 11 cells (the two missing are 250 kS/s), B 11 of 11. Otherwise
  similar numbers on both.
- **VHF, interleaved A/B/A/B, 3 captures each:** at **230 MHz** peak over noise is 16.4 to 17.1 dB on B and 15.7 to 16.2 dB on A (under 1 dB), while the
  noise floor is about 3.5 dB higher on B (-72 vs -76 dBFS): the whole level at 230 MHz rises and the signal-to-noise ratio is about the same.
  At 118.9, 162.55 and 178 MHz no reliable difference.

Reading: on the V4L the band change is neutral for signal-to-noise on this site and antenna (no regression, no measured benefit); on the V4 it gave about
+3 dB at 230 MHz. The two boards do not respond the same way to the same `1b` change.

## Blog V3c (`blog_v3c-*.json`, descriptor `Realtek RTL2838UHIDIR`, profile `blog_v3_r820t2`)

Rates sweep (250k, 960k, 2.048M, 3.2M at 96.1 / 433.92 MHz, plus 1090 MHz at 2.048M) and an interleaved replication (A, B, A, B; 3 captures each) at
960 kS/s across 118.9, 230, 433.92, 453.925, 915 and 1090 MHz, fixed 19.7 dB test gain.

- **453.925 MHz: the R820T2 band select (#49) changes everything.** A reads 0.9 to 3.1 dB peak over noise, 0 strong bins and a noise floor pinned at the
  -77 dBFS floor (the front end is effectively deaf, the issue #25 blind spot). B reads 40 to 45 dB, 159 to 246 strong bins and a -71 dBFS floor. Same in both
  interleaved pairs.
- **433.92, 915 and 1090 MHz:** the A floor is -77 dBFS everywhere; B lifts it to -74 to -76 dBFS and signals begin to appear (peak over noise up to 3 to 13 dB;
  no known transmitters at 915 / 1090 on this site).
- **230 MHz:** the level rises 4 to 6 dB on B (floor -69 vs -73/-75 dBFS) and peak over noise stays about the same (18 to 20 dB vs 17 to 23 dB): more
  gain, not better signal-to-noise. 118.9 MHz: no reliable difference.
- **250 kS/s:** fails on A, works on B (#43), as on the V4 and V4L. On A the failed start also left the V3c idle through the next request (960 kS/s);
  it recovered only at 2.048 MS/s. The 960 kS/s cells on A are therefore `NO_IQ` for that reason, not a rate fault. A failed start leaving the app idle until the
  next successful one is an OrcSDR-side robustness note.

## Nooelec NESDR SMArt v5 (`nooelec_*.json`, profile `nooelec_smart_v5_r820t2`)

- **The published 0.9.2 cannot hold a manual gain on the Nooelec.** In the fixed-gain sweeps A's stream went idle as soon as the test gain was applied (1 of 28 captures
  worked) and stayed idle until the Tab5 was reset; B ran all 27 fixed-gain cells. This is the "restart after manual gain" fault fixed by #44, reproduced on the
  Tab5. A therefore has no fixed-gain data for the Nooelec (`nooelec_nooelec-rates-A.json` is mostly `NO_IQ`); the A/B comparison below uses the tuner AGC on both.
- **AGC vs AGC, 960 kS/s, 3 captures each (`nooelec_nooelec-rep-A-agc.json` vs `nooelec_nooelec-rep-B-agc.json`):** at **453.925 MHz** peak over noise goes from
  16 to 18 dB to 46.4 to 46.8 dB (+29 dB), strong bins 14 to 16 to 49 to 66, noise floor -80 to -69 dBFS. At 915 MHz 4.3 to 9 to 17 dB, at 1090 MHz 2.0 to 7.5 to 7.9 dB, at
  433.92 MHz about 1 to 4 to 8 dB. At 230 MHz the peak over noise is the same (23 vs 24 dB) with the floor 5.5 dB higher on B (more level, not more signal-to-noise);
  118.9 MHz no real change. No clipping on either.
- B at a fixed 19.7 dB (`nooelec_nooelec-rep-B1.json`) agrees: 42 to 43 dB at 453.925 MHz.
- This matches the earlier hardware check on the same dongle in PR #49 (453.9 MHz 21 to 43 dB, 17 to 72 strong signals).

## Summary across the four boards (A = published 0.9.2, B = release candidate)

| Board | 250 kS/s | Band change effect | Other |
|---|---|---|---|
| V4 | fixed (A fails) | +3 to 6 dB peak over noise at 230 MHz; nothing elsewhere | |
| V4L | fixed (A fails) | level +3.5 dB at 230 MHz, signal-to-noise unchanged | |
| V3c | fixed (A fails; leaves the app idle for one more request) | 453.925 MHz 1 dB to 40 to 45 dB; 433 / 915 / 1090 MHz come alive | |
| Nooelec | not measured | 453.925 MHz 17 dB to 46 dB; 915 / 1090 / 433 MHz up 5 to 15 dB | A cannot hold a manual gain (#44) |

## Unplug / replug on the Tab5 (the #50 gate), Blog V4 (`hotplug/`)

- **Release candidate B (V4 plugged in, 5 unplug/replug cycles asked): crashed on the second unplug.** `assert failed: tlsf_free ... block already marked as free`, then two
  reboots (reset reason `wdt`). The backtrace decodes (with an ELF rebuilt from the same commit; 76 of 3.8 million bytes differ from the flashed image, all header/checksum)
  to `rtl_dsp_task -> queue_audio_samples -> flush_audio_play_batch -> restart_rtl_speaker_i2s -> m5::Speaker_Class::begin -> _setup_i2s -> i2s_del_channel`: OrcSDR's speaker code,
  not the driver. On disconnect the main task idled the speaker with an unguarded `M5.Speaker.end()` while the DSP task restarted it, and both deleted the same I2S channel.
  The driver side of that run is clean: bulk resubmit errors, `usb disconnected`, re-probe, and a root-port power cycle after 10 s with nothing attached, as designed.
- **B plus the OrcSDR speaker fix (all speaker end/begin under one mutex; no restart without a receiver): passed.** 6 disconnects, 6 probes, 6 restarts, 0 start failures,
  0 reboots, 0 leaked URBs. One clean run is not proof for a race; the fix addresses the decoded cause. OrcSDR PR: `claude/fix-speaker-disconnect-race`.
- Still to do: the same cycles on the V4L and V3c (and the Nooelec again) with the fixed app.

## Not shown

the Nooelec 250 kS/s case (A cannot hold the test gain) and a Nooelec rate sweep on A; the per-rate filter and IF change (column C); a second site or antenna; any claim relative to the vendor DLL.
