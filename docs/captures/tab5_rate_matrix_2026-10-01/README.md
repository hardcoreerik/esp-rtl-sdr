# Tab5 sample-rate / band A/B runs, Blog V4, 2026-10-01

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

## Not shown

V4L, V3c and Nooelec on the Tab5; the per-rate filter and IF change (column C); a second site or antenna; any claim relative to the vendor DLL.
