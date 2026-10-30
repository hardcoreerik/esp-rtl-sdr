# COM39 conducted tinySA regression findings

Session began 2026-10-29 Pacific and continued into 2026-10-30. This is bounded
hardware evidence, not a calibrated receiver specification or an old/new A/B.

## Setup and provenance

- Tab5 COM39, Nooelec NESDR SMArt v5; unchanged application test base with
  driver `f14c8565e274bc7a5a68523fd1f0e9b7879a3d85` from upstream master.
- Application SHA-256: `0a5c3e886af28840e13ee67cfe58e172a606969a36003748e2d9a408ec9fa3bd`.
  The preceding deployment checked the device MAC, managed source and flash hash.
- tinySA Ultra COM7, serial 400, hardware V0.4.5.1.1, firmware
  `tinySA4_v1.4-151-g31a89ce`.
- RF chain: tinySA **RF** -> 30 dB attenuator -> SMA cable -> Nooelec. No antenna.
  Source settings are nominal; external attenuation/cable loss were not calibrated.
- Current OrcSDR uses IDF 5.5.4, callback acquisition; extended driver metadata
  is not enabled in this application. COM17 and COM8 were not modified by this RF run.

## Results

Four automated telemetry sequences completed **94 observations**, plus **10
raw IQ captures** with device/local SHA-256 matching. All sequence cleanup paths
completed. Every reported drop/overrun count was zero; every measured clipping
value was zero. This does not prove every possible interruption was observed.

| Check | Evidence | Conclusion |
|---|---|---|
| Gain controls | All 29 native gain requests; selected return steps | Accepted with stable streaming. Measured level span 21.6 dB; return spreads 0.0-0.4 dB. Not absolute gain calibration. |
| Source level | Nominal -60 to -30 dBm at gain 40.2 dB | Weak inputs approach the broad-power floor; stronger source settings produce clear response. |
| Maximum gain/input ramp | Gain 49.6 dB; source -40 to -19 dBm through 30 dB | No clipping. Last 5 dB step changed readout 4.9 dB; final 1 dB changed 0.9 dB. Positive clipping detection remains untested. |
| Tuning at 100 MHz | Source 100.1 MHz; raw IQ 2.4 MS/s | Tone +99,975.6 Hz; absent with source off. |
| Tuning at 433 MHz | Source 433.1 MHz; raw IQ 2.4 MS/s | Tone +99,902.3 Hz. |
| LoRa acquisition at 906.875 MHz | Source 906.975 MHz; raw IQ 960 kS/s | Tone +99,668.0 Hz; absent with source off. Unmodulated tone is not a LoRa packet test. |
| Bandwidth controls | AUTO, 200k, 300k, 500k, 1M, 1.8M, 2.4M requests | All accepted with applied shadows and continuing IQ. Analog widths are not established by these shadows. |
| RTL AGC | On/off at fixed tone and manual tuner gain | Readout changed from -26.1 to -11.6 dBFS, with no clipping/loss observed. |
| Return to operation | Fresh final serial query | Streaming 906875000 Hz, AUTO, manual shadow 0, RTL AGC off, bandwidth request 0, LoRa gain guard on. tinySA acknowledged output off. |

These raw-tone positions provide first-party reception evidence at these three
frequencies. They do not calibrate either device's clock. Above 800 MHz the
original tinySA Ultra has additional output limitations; see its
[specification](https://tinysa.org/wiki/pmwiki.php?n=TinySA4.Specification).

## Important finding: requested bandwidth is not measured passband

The 200 kHz request did **not** act as a centered 200 kHz RF passband here.
At a +300 kHz tone, raw FFT peak power was -18.16 dBFS under the 200 kHz request
and -17.82 dBFS under AUTO: only 0.34 dB difference. The separate stronger-tone
sweep showed an asymmetric response: negative offsets were attenuated much
earlier than positive offsets. See measurements.csv and frequency-response.png.

This does not establish a newly introduced regression. The existing table is
capture-derived vendor control choices; its source already distinguishes those
choices from measured analog passbands. No equivalent conducted PC sweep or
old-driver A/B was performed here. Next: compare this same rig against the PC
reference with identical bandwidth choices before changing tuner registers.
Review `bw_applied_hz` and UI wording so they are not interpreted as a calibrated
physical width.

## Important finding: broadband strength is not channel strength

Off-channel rejection was inconclusive in the broad level readout. Raw IQ
resolved the intended +100 kHz tone decisively. In the 100 MHz source-off capture,
about 90% of spectral power was at absolute offsets 800-1080 kHz, far outside
that target channel. The source-off narrow peak tracked approximately 100.8 MHz
RF as the receiver changed (offsets +800k, +600k, -600k); its physical origin is
not established. The narrow peak alone does not dominate total power.

A useful follow-up is channel-specific power/noise diagnostics alongside the
existing full-stream dBFS. Do not reinterpret full-stream dBFS as sensitivity,
noise figure or calibrated in-channel SNR. No DSP or tuner change was made on
the basis of these observations.

## OrcSDR host-tool improvements

The IQ runner contained two hard-coded historical driver commit IDs. It now
records the exact requested driver manifest pin, explicitly distinguishes it
from running-firmware attestation, and labels the capture workspace commit.
Bandwidth requested/applied values are included. The runner now accepts LoRa
IQ capture; two 960 kS/s captures exercised this path successfully.

The fixture's generic bandwidth-capture suitability label initially said
+100 kHz even for the +20/+300 kHz tests. Precise source commands and suite
manifests were retained. results.json uses the precise values and records the
label correction; original artifacts were not rewritten.

## Reproduction and limits

Local raw data, source command/reply traces and executable fixture procedures
remain under `F:/AI/OrcSDR-TEMP/driver-acquisition-ab-20261029/build/driver-ab/`:
`tinysa-master-matrix-v2`, `tinysa-bandwidth-matrix`, `tinysa-clipping-ramp`,
`tinysa-frequency-response`, `tinysa-iq-captures`, `tinysa-spur-retunes`, and
`tinysa-bandwidth-iq-v2`. Large CU8/serial logs are not committed. The initial
PowerShell helper alias failure and invalid same-frequency hot-tune attempt
are preserved separately; neither produced an RF pass claim.

`results.json` contains capture hashes and sequence provenance;
`measurements.csv` contains the 94 observations. Reproduce spectra with:

```text
python analyze_tones.py <local capture suite directory> <output directory>
```

The FFT uses 65536 points and up to 32 Hann-windowed frames. Its peak/median-bin
contrast is not integrated-channel SNR. The analysis passed a synthetic known
frequency/zero-clipping/frame-count self-check.

Host checks run: `tests/test-iq-provenance.ps1` (5 cases),
`tests/test-driver-regression-runner.ps1`,
`tests/test-driver-app-regression-runner.ps1`, and
`run-tab5-ui-regression.ps1 -SelfCheck -IqBand LORA`: all passed.

Still open: actual clipped-input detection, absolute calibration, full filter
characterization, USB physical teardown/no-PSRAM acceptance, other dongles,
antenna acceptance and decoder traffic. The app's provisional Nooelec flag is
also still present, separately from the implemented driver profile status.
