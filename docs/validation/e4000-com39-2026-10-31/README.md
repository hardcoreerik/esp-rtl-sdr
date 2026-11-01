# E4000 initial COM39 hardware evidence — October 31, 2026

This records initial acceptance on one Nooelec SMArt XTR v5, serial 82115932,
with an ESP32-P4 Tab5 on COM39. E4000 remains experimental.

The user confirmed FM audio reception and RDS decoding after replacing the
tinySA/attenuator/cable fixture with an approximately one-foot monopole.
The antenna's intended band is unspecified. A hash-verified `FT8 SNAP` PNG
shows decoded station name **KZEL**, 96.1 MHz, RUNNING and STEREO. Serial
status independently shows profile 5, high-speed USB, STREAMING, stereo lock,
RDS carrier lock, no reported clipping and zero drops/overruns at observation.
The exact driver frequency was 96,118,000 Hz; the screen rounds its display.

## Candidate and provenance

Runtime driver: `b7cb81a491ce69acb33a66234e1bdc3a85dc36c4`.
Application image SHA-256:
`0e7be6bcac84a5de8d69d72aed4e1a6873afc4fa68f535198a93714bf5d71d2e`.
Only the application at 0x10000 was flashed and esptool verified its hash.
COM17 was not flashed. C6 firmware and settings partitions were not flashed.
This test uses the existing C6; embedded update-payload coverage is not an acceptance claim.
The isolated application test branch has not been updated to current OrcSDR
main and is not a release artifact.

The build used a local component staged under the required `esp_rtl_sdr`
directory name. All 26 source/config files were checked against the driver
checkout. `build-provenance.json` records their hashes. After building, the
application manifest was changed to request the identical commit from the
local Git repository so the existing IQ tool could record an exact requested
revision. That manifest is not itself running-firmware attestation; the
separate source/image/flash evidence supplies that boundary.

## Bugs found before this observation

1. The descriptor matcher recognized SMArTee XTR v5 but not the captured
   **SMArt XTR v5**. The captured identity now selects the experimental profile.
2. The E4000 I2C bridge address was unshifted 0064. First-party PC ID transfers
   establish 00c8; probe, initialization and tuning now use that address.
3. New-tuner initialization skipped the final USB FIFO release. Tuner setup
   succeeded, but bulk startup stalled with metadata disabled. The shared
   capture-derived FIFO reset/release now completes initialization.

Host checks failed before the respective fixes and passed afterward. At this
candidate, profiles passed 2,037 assertions and the actual-function runtime
trace passed 8,462 checks. These mocks do not prove ESP timing or RF reception.

## IQ observations and remaining tests

Three 4,800,000-byte captures at 2.4 MS/s passed device/local SHA-256 comparison.
The source-off, nominal -50 dBm source-on and nominal -20 dBm source-on tests
did not establish a clear source-correlated tone. Do not report them as
controlled RF acceptance, sensitivity, gain or passband measurements. RF
fixture continuity and source state during those observations need an
independent check before repeating them. The user subsequently removed the
fixture and confirmed broadcast reception with the monopole.

The initial same-frequency hot-tune IQ request was rejected by the host tool's
readiness guard; subsequent captures used explicit stop/start. Preserve that
host-tool failure separately from receiver faults. Large IQ files and raw
logs remain in the application build's `e4000-b7cb81a` directory; hashes and
selected status are recorded here. Never publish pairing keys or NVS backups.

Repeat observation: authenticate with the existing serial helper; query
`RTL_STATUS`, `RTL_DRIVER STATUS` and `RTL_SIGNAL` without tuning. Capture the
current display with:

```powershell
python tools/tab5_ft8_shot.py --port COM39 --key F:/Ai/OrcSDR/.orclink/ui-doc.key fresh-output-name
```

Do not pass `--open`, `--tab` or `--sd` for an unchanged-screen observation.
The helper verifies PNG length and SHA-256. A screenshot proves displayed
state; user audio acceptance and driver counters remain separate evidence.
Continue with sustained observation, hotplug/start-stop/retune tests and
existing-dongle regression checks. Gain/AGC/bandwidth/HF/bias controls are not
exposed for E4000 and are not promoted by this result.

A 60.71-second authenticated observation collected 60 samples at the same
frequency. Byte counts advanced from 2,472,542,208 to 2,759,131,136 with
unchanged zero drop/overrun counters. The compact soak summary includes the
first/last state and hashes of the retained full observation.
