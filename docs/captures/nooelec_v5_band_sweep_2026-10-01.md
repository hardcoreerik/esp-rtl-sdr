# R820T2 band-select sweep, four dongles, 2026-10-01

Black-box PC capture: the RTL-SDR Blog 1.4.0 `rtlsdr.dll` (public API) tuned each dongle through 47 RF
frequencies, ascending then descending, with USBPcap + TShark recording the control transfers. The tuner
registers `0x17`, `0x1a`, `0x1b` written for each tune are extracted from the capture. No foreign driver
source was read or used. This is PC evidence only: no ESP32-P4 or OrcSDR hardware result.

Frequencies: both sides of every observed boundary (LO = RF + 3.57 MHz, +/-0.3 MHz) of the band table in `private/r820t2_band.hpp`,
a point inside the lowest row, 433.92 and 915 MHz, and 1.0/1.5/1.7 GHz. Antenna: the ADS-B dipole. Tool:
`band_sweep_2026-10-01/run_band_sweep.ps1` (stimulus, capture, extraction).

| Dongle | Descriptor | Tuner API type | Capture SHA-256 |
|---|---|---|---|
| Nooelec NESDR SMArt v5 | `Nooelec` / `NESDR SMArt v5`, serial 36309640 | 5 | `f53582d54ee9bfdcda7ce065e12ee3b7cc837db82c049a99aea683350f52a142` |
| Blog V3c test unit | `Realtek` / `RTL2838UHIDIR`, serial 00000001 | 5 | `69400e4df44bfc85958abb9244c1024619f07c6e89e2dd686ef33298aca11240` |
| Blog V4 | `RTLSDRBlog` / `Blog V4`, serial 00000001 | 6 | `f21fbdb7820398fcc33da832169e8bd08530fef34000e34fe85ea5e82a5d5a05` |
| Blog V4L | `RTLSDRBlog` / `Blog V4L`, serial 00000001 | 5 | `de2407f4524086654b57b249bb06edb3972858986610dac9c93941e490a1ea6e` |

The pcapng files stay in the lab (`C:\Tools\esp-rtl-sdr-lab\captures\`); the per-dongle extracted registers
are the CSV files beside this note, and `band_sweep_2026-10-01/summary_table.md` lines them up (`17/1a/1b`).

## Results

- **Nooelec v5 and Blog V3c: every row of the band table in `private/r820t2_band.hpp` reproduced (the table is built from these captures).** 94 of 94 tunes (47 frequencies,
  both directions) leave `17` bit 3, `1a & 0xc3` and `1b` exactly as the table predicts, for both boards.
  This is the evidence for the R820T2 band select in this repository. (One V3c descending point showed
  `1a=22` where the Nooelec showed `2a`: the differing bit is outside the `0xc3` band mask.)
- **Blog V4 (tuner type 6) steps `1b` through about 12 values** from 50 to 650 MHz with its own `17` pattern,
  so it does not follow the R820T2 table (28 of 94 match by coincidence). It is correctly excluded from the
  band patch.
- **Blog V4L steps `1b` through the same sequence** (`df be 8b 7b 69 58 44 34 24 14 13 11 00`). Its row
  boundaries sit above the Nooelec's: every point 0.3 MHz above a table boundary still showed the previous
  row's `1b`, so its band switching is keyed on a different IF or on RF. A finer boundary scan would pin
  this down; it is not done here.
- Ascending and descending passes agree exactly on all four dongles (no hysteresis).

## What this does not show

Register values only. Whether a closer match of `1b` (tracking filter) improves reception on V4 or V4L, whose
driver routes write only two or three `1b` states, is untested and needs an on-hardware A/B.
