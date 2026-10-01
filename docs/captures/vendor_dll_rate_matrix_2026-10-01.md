# Vendor DLL sample-rate matrix and band edges, 2026-10-01

Black-box PC captures of the RTL-SDR Blog 1.4.0 `rtlsdr.dll` (public API), USBPcap + TShark, one dongle at a time on
the PC with the ADS-B dipole. Registers are extracted from the control transfers. No foreign driver source was read.
PC evidence only: nothing here is an ESP32-P4 / OrcSDR result, and none of it shows that a setting improves reception.

Companion note: `nooelec_v5_band_sweep_2026-10-01.md` (band table). Raw pcapng files stay in the lab at
`C:\Tools\esp-rtl-sdr-lab\captures\` (hashes in `rate_matrix_2026-10-01/pcap_sha256.tsv`); the extracted tables are here.

## Status

| Test | Nooelec v5 | Blog V3c | Blog V4 | Blog V4L |
|---|---|---|---|---|
| Band sweep (47 freq, asc+desc) | done | done | done | done |
| 10 kHz band-edge scans | - | - | done (14 `1b` edges, 4 `17` edges, 3 rates) | 2 edges |
| Rate matrix (20 rates, 3 freq) | earlier sweep covers 250k-3.2M, not the edge cases | pending | **done, 2 identical runs** | **done, 2 identical runs** |

## Rate matrix, Blog V4 and V4L (vendor DLL, 96.1 MHz stage)

Each rate is a fresh `rtlsdr_open`, `set_sample_rate`, then tunes at 96.1 / 433.92 / 1090 MHz and a 262,144-sample read
timed against the wall clock. Run 1 and run 2 of each dongle (`blog_v4_run*.csv`, `blog_v4l_run*.csv`) match on all 60 rows,
every field. The raw USB writes for 960k, 2.048M and 3.2M were dumped (`verify_rate.py`) and match the table.
V4L and V4 differ only in tuner `0a` (low bit: V4L `c4`/`d4`, V4 `c5`/`d5`) and register `17`; ratio, IF, `0b` and the
PLL registers are identical.

| Rate (S/s) | Resampler ratio | IF (Hz) | Demod IF word | Tuner `0b` | V4 `0a` | V4L `0a` |
|---|---|---|---|---|---|---|
| 250000 | 0ccccccc | 2124996 | 3b471d | e6 | c5 | c4 |
| 256000 | 0c200000 | 2124996 | 3b471d | e6 | c5 | c4 |
| 400000 | rejected | - | - | - | d5 | d4 |
| 600000 | rejected | - | - | - | d5 | d4 |
| 900000 | rejected | - | - | - | d5 | d4 |
| 960000 | 07800000 | 1699997 | 3c38e4 | eb | c5 | c4 |
| 1024000 | 07080000 | 1699997 | 3c38e4 | eb | c5 | c4 |
| 1400000 | 05249248 | 1575000 | 3c8000 | ec | c5 | c4 |
| 1800000 | 04000000 | 1749998 | 3c1c72 | ac | c5 | c4 |
| 1920000 | 03c00000 | 1674996 | 3c471d | ae | c5 | c4 |
| 2000000 | 03999998 | 1624995 | 3c638f | af | c5 | c4 |
| 2048000 | 03840000 | 1624995 | 3c638f | af | c5 | c4 |
| 2400000 | 03000000 | 1814996 | 3bf778 | 8f | c5 | c4 |
| 2560000 | 02d00000 | 3569994 | 381112 | 6b | d5 | d4 |
| 2800000 | 02924924 | 3569994 | 381112 | 6b | d5 | d4 |
| 2880000 | 02800000 | 3569994 | 381112 | 6b | d5 | d4 |
| 3200000 | 02400000 | 3569994 | 381112 | 6b | d5 | d4 |
| 3300000 | rejected | - | - | - | d5 | d4 |
| 3600000 | rejected | - | - | - | d5 | d4 |
| 4000000 | rejected | - | - | - | d5 | d4 |

- **Rejected by the DLL (`rc=-22`, "Invalid sample rate"):** 400000, 600000, 900000, 3300000, 3600000, 4000000, on both
  dongles. The hardware keeps its previous rate when a rate is rejected (IQ timing after a rejected call matches the previous
  open's rate).
- **Real stream rate:** accepted rates stream at the requested rate (960 kS/s measured 960.3 kS/s, 3.2 MS/s measured 3.21 MS/s).
- **250k / 256k:** the ratio written is the masked value (`0ccccccc`, `0c200000`), the values the driver's #43 produces.
  The old driver wrote `0c200094` for 256k.
- **Filter and IF follow the rate.** The same `0b` / IF pairs appear in the earlier Nooelec rate sweep (`0a` low bits differ
  by tuner: `c3` Nooelec, `c5` V4, `c4` V4L), so this is one DLL table across R82xx dongles; the V3c is still to confirm.
- **Write order inside `set_sample_rate`:** tuner `0a`, `0b`, then demod IF (`0x19..0x1b`), then the PLL is re-programmed for the
  new IF, then the resampler ratio (`0x9f..0xa2`). A driver adopting this must keep the order.
- **The band registers key on LO = RF + the IF in use** (V4 `1b` edge moves from 48.38 MHz at 2.048 MS/s to 48.20 MHz at
  2.4 MS/s and is out of range at 3.2 MS/s). The V4's register-17 bit 3 edges (85, 112, 172, 242 MHz) are RF-keyed and do not
  move with rate.

## Reproduce

Scripts are in `rate_matrix_2026-10-01/` and `band_sweep_2026-10-01/`. From PowerShell (needs USBPcap + TShark, WinUSB on the
dongle, exactly one RTL dongle on the PC):

```
# band sweep for one dongle: finds the USBPcap interface, captures, extracts
.\run_band_sweep.ps1 -Label v4
# 10 kHz edge scan
.\run_fine_scan.ps1 -Label v4 -Start 48200000 -Stop 48550000 -Step 10000 -N <usbpcap#> -Bus <bus> -Addr <addr> [-Rate 2400000]
# rate matrix (start a tshark capture on the dongle's USBPcap interface first)
python rate_matrix_stimulus.py <outdir>
python extract_rate_matrix.py <pcap> <outdir>\events.json <bus> <addr> <out.csv>
```

Gotchas: run tshark from PowerShell (Git Bash mangles `\.\USBPcap2`); `usb.setup.wIndex` prints as decimal in field output
(`0x0610` is `1552`, `0x0011` is `17`); the last tune's window must stop before the close-time writes.

## Not shown

Effect on reception for any setting; behaviour beyond what the DLL accepts (the chip may take other ratios, untested);
the V3c and Nooelec rate matrices.
