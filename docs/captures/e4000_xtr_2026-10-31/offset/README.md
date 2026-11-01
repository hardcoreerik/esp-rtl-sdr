# XTR offset-tuning reference and candidate

Setup: XTR serial **82115932** on the Windows PC, outside 20 m telescopic
antenna, Ham It Up Plus v2 in **passthrough** with a separate power supply.
No tinySA injection was used. The reference DLL/hash is recorded in the parent
capture report. Raw captures remain under the local lab capture root.

The first run records OFF/ON/OFF at 960 kS/s, 2.4 MS/s and 3.2 MS/s, with
524,288-byte IQ blocks and API state. All nine IQ hashes match their event
records; 939 paired USB controls have successful status. A second controls-only
run covers 16 valid rates from 240 kS/s to 3.2 MS/s: 2,981 paired controls,
zero unpaired submits. An earlier attempt stopped when the reference correctly
rejected 400 kS/s; that failure and its raw capture are retained.

Observed policy: tuner LO is requested RF minus 0.85 times sample rate;
the RTL2832U signed 22-bit NCO applies the matching negative correction.
The register-derived effective center remains 96.1 MHz within quantization.
The tuner IF filters widen with offset. See `results.json` for the exact
register observations, byte lengths and SHA-256 inventory, and `analysis.md`
for the IQ comparison and its limits.

The candidate encodes filter registers independently from the
[E4000 datasheet](https://www.nooelec.com/store/downloads/dl/file/id/14/product/264/e4000_datasheet.pdf),
tables 22–24, then checks selection against all 16 reference rates. One
selection detail differs from the printed table: mixer code 0xc is labeled
3.0 MHz there, but the PC chooses it at a 3.264 MHz target (1.92 MS/s).
A 3.3 MHz selection target reproduces this observation. It is a software
selection rule inferred from controls, **not a measured analog corner**.

## Repeat

Only the enrolled XTR may be connected to the PC. Confirm USBPcap1 and the
device bus/address before capture. Run in an elevated terminal if USBPcap
requires it. Choose a new output directory; scripts refuse to overwrite one.

```powershell
$env:ESP_RTL_SDR_OFFSET_OUTPUT='C:/Tools/esp-rtl-sdr-lab/captures/NEW-offset-iq'
python run_offset_reference.py
$env:ESP_RTL_SDR_OFFSET_OUTPUT='C:/Tools/esp-rtl-sdr-lab/captures/NEW-offset-rates'
python run_offset_valid_rates.py
```

Use the retained `decode_ep0.py PCAP EVENTS BUS ADDRESS OUT` lab decoder to
pair controls. Capture contains unrelated USB traffic; keep raw files local.
Record physical setup, DLL hash, return codes, offset readback, center getter,
IQ hashes, control status and failures. Antenna observations do not establish
calibrated sensitivity, filter response or preservation of a particular station.

## Driver contract and open gates

New API requires a MINOR release. Offset tuning is opt-in, E4000-only, configured
while IDLE, and cleared on attachment. The next start and later retunes apply
hardware correction; no client software mixer is needed. The next start's
actual rate drives the offset, including an explicit stream-rate override.
Near 64 MHz, correction is suspended rather than driving the LO below range;
requested remains true but the applied offset is zero. Disabled clients and
other tuner paths retain their controls.

Host checks cover captured plans, capabilities, physical LO/NCO/filter writes,
failed-transfer early exit and restoration. They do not prove physical RF
reception or ESP-IDF public API concurrency. OrcSDR integration, firmware build,
matched before/after RF measurements, FM/RDS/AM, lifecycle tests and legacy
dongle hardware regressions remain gates. The separate USB HAL panic also
remains unresolved; this correction is not its fix.
