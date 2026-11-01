# Nooelec SMArt XTR v5: first-party PC evidence, October 31, 2026

This is a black-box PC reference campaign, not ESP32-P4 E4000 acceptance.
No third-party source, disassembly, or register tables were used to derive
these measurements. The reference DLL was exercised through its public API;
USB controls and IQ were captured from the user's physical device.

## Identity and tools

| Item | Observed value |
|---|---|
| Manufacturer / product | Nooelec / **SMArt XTR v5** |
| Serial / USB ID | `82115932` / `0BDA:2838` |
| Reference tuner report | Elonics E4000, type 1 |
| Windows binding | WinUSB, libwdi provider, problem code zero |
| Reference library | RTL-SDR Blog 1.4.0 package, x64 `rtlsdr.dll` |
| Library SHA-256 | `0fa7f8bf4b7e07918a0a9e98228007cccac3109dbaac7497e116e416135d58a5` |
| Library version resource | `v0.6.0-131-gaed0`, osmocom rtl-sdr |
| TShark | Wireshark 4.6.8, hash in manifest |
| Capture interface / target | USBPcap1 / bus 1, address 10 for this connection |
| Source | tinySA Ultra, USB serial 400, COM7 |
| Source firmware | `tinySA4_v1.4-151-g31a89ce`, hardware V0.4.5.1.1 |

The package is a PC reference, not a confirmed Nooelec proprietary driver.
No Windows driver installation or EEPROM write was performed.
The actual product string differs from the driver's explicit experimental
`SMArTee XTR v5` match. This is a detection hypothesis, not a completed fix.
COM39 with driver commit `4a9874b2425cc985a3edcb4944e6659dff2b54c7`
saw the device, stalled during control probes, and rejected its profile.

## Retained evidence

Raw evidence: `C:\Tools\esp-rtl-sdr-lab\captures\e4000-xtr-20261031`.
Large PCAP and CU8 files stay in the lab. This directory contains the compact
results, RF CSV, raw-artifact hash manifest, and repeatable stimulus scripts.
The retained originals of executed scripts are also hashed. Committed scripts
add only an optional output-root environment override.

| Accepted trace | Paired controls | Unpaired submits |
|---|---:|---:|
| `controls-verified/controls.pcapng` | 1,378 | 0 |
| `sustained-and-boundaries/controls.pcapng` | 3,519 | 0 |
| `tinysa-paused-matrix/controls.pcapng` | 813 | 0 |

Pairing does not mean every control request succeeded or that every sample
arrived. Inspect returned status and stage events for individual outcomes.

## PC control and delivery results

The reference exposes 14 nominal gain settings, in tenths of dB:
`-10,15,40,65,90,115,140,165,190,215,240,290,340,420`.
Gain, tuner gain mode, RTL AGC and bandwidth API calls returned zero.
Direct I/Q set/readback also succeeded and was restored off. This does not
prove external RF routing, useful HF reception, or analog filter width.

Each delivery run used a warm-up read and at least five seconds of synchronous
reads. No short reads or reported read errors occurred:

| Requested samples/s | Host-delivered samples/s |
|---:|---:|
| 250,000 | 249,997 |
| 960,000 | 957,980 |
| 1,024,000 | 1,023,219 |
| 2,048,000 | 2,045,927 |
| 2,400,000 | 2,388,388 |
| 2,560,000 | 2,556,992 |
| 2,880,000 | 2,876,662 |
| 3,200,000 | 3,193,537 |

These are host throughput measurements including loop overhead, not sample
clock calibration or losslessness proof. Five reopen/tune/read/close cycles
each returned 262,144 bytes successfully; this was not physical hotplug.

The user clarified that a telescopic 20 m antenna was attached during PC
delivery testing. Its exact attachment time is unknown. All those IQ captures
are qualified as live-antenna observations; raw no-source labels are superseded.
The 250 kS/s block had substantial clipping, which cannot be attributed to a
driver regression or quiet-input behavior from this setup.

Tune requests at 1.11 through 1.20 GHz, including 1.135 GHz, returned -1 with
PLL-unlock diagnostics. Tested points 1.08, 1.09, 1.10, 1.25, 1.30, 1.50 and
1.70 GHz succeeded. This brackets sampled failures; exact gap boundaries and
RF reception there are not established. Retuning to 100 MHz recovered.

## Controlled RF results

The antenna was replaced by: tinySA **RF -> 30 dB attenuator -> SMA -> XTR**.
The PC API explicitly disabled bias tee, selected manual tuner gain and
disabled RTL AGC. Source levels and attenuation are nominal, not independently
calibrated. The receiver was tuned to 100 MHz at 2.4 MS/s.

The accepted run contains **40 hash-indexed IQ blocks**, 524,288 bytes each:
source off/on, 14 gains, three levels, 20 bandwidth/offset combinations, and
final source-off. No rail-valued samples occurred in these blocks.

- A nominal 100.1 MHz source appeared at +99,975.586 Hz in IQ. Source-on was
  -22.70 dBFS versus -77.02 dBFS at the searched offset with source off,
  a 54.3 dB difference. Final source-off was -77.03 dBFS. The roughly -24 Hz
  difference combines source and receiver clocks and FFT resolution.
- The gain sweep changed measured tone level from -43.36 to -6.74 dBFS,
  about 36.6 dB. Nominal 29 and 34 dB settings both measured about -13.19
  dBFS. Repeat this plateau before interpreting its cause; the gain labels
  are API settings, not calibrated analog gain measurements.
- Source levels -50, -40, -30 dBm measured -32.81, -22.86, -12.95 dBFS:
  changes of approximately 9.95 and 9.90 dB. This supports relative response
  under this setup, not absolute input power calibration.
- Bandwidth requests 0, 200 kHz, 500 kHz, 1 MHz and 2.4 MHz were tested at
  +20, +300, -300 and +900 kHz. The 200 kHz request retained the +300 kHz
  tone at -23.76 dBFS versus -23.49 dBFS in AUTO. A successful 200 kHz API
  request is therefore not evidence of a measured 200 kHz passband.

FFT values use a 65,536-point Hann window, coherent amplitude normalization,
and a peak search within 2.5 kHz of the requested offset. They are peak-bin
amplitudes, with window scalloping and source/clock effects; they are not
integrated channel power. See CSV and CU8 for independent reanalysis.

## Rejected attempts and instrument recovery

Keep all failed attempts in the manifest:

- `controls`: unsupported TShark extcap capture-filter option; API events
  exist but its USB trace is not accepted.
- `tinysa`: serial write timeout during initial setup, output-on not sent.
- `tinysa-verified`: commands completed but no verified injected tone;
  `freq` did not configure the intended CW sweep. RF results are rejected.
- `tinysa-tone`: source-on prerequisite failed and USB capture had zero
  packets while another capture was active. RF/USB results are rejected.
- `tinysa-accepted`: despite its attempt name, repeated CW setup stalled;
  **not accepted**. Output-off was unconfirmed until instrument power cycle.
- `tinysa-resume-probe`: one valid off/on pair established a tone after
  `resume`; this is a setup diagnostic, separate from the full matrix.

The final matrix pauses before each CW frequency change, waits for `ch> `,
then resumes. It requires a source-on rise above 15 dB before continuing.
It confirmed output-off at the end and closed the receiver. Power cycles
were user actions, not automated recovery. The exact tinySA stall cause is
unresolved; successful final sequencing does not prove a firmware fix.
Official command semantics: <https://tinysa.org/wiki/pmwiki.php?n=Main.USBInterface>.

## Repeat procedure

Use a trusted Windows lab PC with the above reference DLL, Python with
`pyserial` and `numpy`, TShark and USBPcap installed. Only the enrolled XTR
serial may be connected. Recheck its USB bus/address and USBPcap interface
after reconnecting; the scripts currently select USBPcap1. They require the
exact serial and one visible SDR, never write EEPROM, and retain partial runs.
Close competing captures and SDR applications. This is a manual hardware lab
procedure, not a GitHub-hosted CI test or an unattended physical setup.

Select a fresh output directory; scripts refuse to overwrite their run folder:

```powershell
$env:ESP_RTL_SDR_PC_OUTPUT_ROOT = 'C:/Tools/esp-rtl-sdr-lab/captures/e4000-repeat-01'
New-Item -ItemType Directory -Path $env:ESP_RTL_SDR_PC_OUTPUT_ROOT
python pc_controls.py
python pc_delivery.py
# Then connect tinySA RF -> 30 dB attenuator -> XTR, remove antenna,
# connect tinySA USB on COM7, and start with source output OFF.
python pc_tinysa.py
python verify_run.py $env:ESP_RTL_SDR_PC_OUTPUT_ROOT
```

Before running, adjust the fixed DLL/port/capture-interface paths if inventory
differs, and hash the edited script. For USB decoding, use the retained
`v3c-live-bw-20260928/tools/decode_ep0.py` with PCAP, events JSON, bus, address
and output path. It preserves raw E4000 transfers even though its old tuner
labels target other tuners. The decoder is included in the manifest.
The offline verifier checks saved phase completeness, IQ hashes/lengths,
the tone prerequisite, clipping, final output-off acknowledgement and nonempty
vendor control captures. It does not attest physical wiring, decode transfer
pairing, or prove stream losslessness. Recompute hashes of PCAP/CU8/script files
and compare IQ hashes with events;
do not reuse this campaign's expected hashes for a newly acquired capture.

Acceptance gates: exact identity and tuner report; expected API return/readback;
no short reads/read errors; successful reopen cycles; source-on rise >15 dB;
zero clipped samples for relative RF comparisons; final source-off; successful
capture and complete control pairing. A failed RF gate must not be reported
as gain/filter success. Phase-specific failures remain separate from recovery.

Remaining: ESP enumeration/init and RF acceptance, physical hotplug, longer
soak/deterministic loss tests, repeated gain plateau, direct-sampling RF route,
and calibrated sensitivity/absolute gain/frequency measurements. Nothing in
this report promotes E4000 from experimental or establishes release readiness.
