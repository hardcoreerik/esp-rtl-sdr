# Hot retune latency: measurement and safe optimization gate

Branch: `feature/fast-retune-aircraft-scan`. **Profiling/acceptance work only. No fast-retune optimization has been enabled.**

## Measured problem and origin

The current `src/esp_rtl_sdr.cpp` has an instrumented hot retune:
- `bulk_pause_and_drain()`: pauses bulk-IN resubmission and waits for live URBs to finish
- `run_profile_tune()`: enables the RTL2832U I2C repeater and sends tuner profile records, with cached identical register writes skipped
- `run_band_frontend()`: controls Blog V4/V4L route/filter
- `run_acquisition_fifo_reset()`: prevents samples at a previous frequency from being mislabeled
- `bulk_resume()`: resumes USB IQ acquisition and emits a new acquisition epoch

The source explicitly records about **188–190 ms total**, including about **171 ms in profile tuning** on one measured platform. A series of individual control records took approximately 7.8 ms each. These are dated code comments/measurements, not cross-hardware acceptance data. The user supplied a Perplexity research discussion as supporting context; primary verification is in this repo's source and future on-device logs.

A tuner synthesizer lock specification (sometimes quoted as ~5 ms) must not be confused with end-to-end USB/firmware retune or time to first verified post-retune signal.

## Investigation order

1. Reproduce the baseline on *unchanged* firmware with a Blog V4 (also V4L/V3c/Nooelec as available), noting model, board, exact firmware SHA, sample rate, URB layout and firmware logging level.
2. Capture at least 30 same-band hops (136.975 <-> 136.800 MHz), and at least 30 cross-band hops (136.975 <-> 1090 MHz) at a fixed sample rate. Also measure required stop/start on sample-rate changes separately.
3. Capture a source of known-good signal at each frequency, with before/after CRC-valid decoder evidence and acquisition epoch tagging. Missing or stale sample validity fails the gate, even if the retune API returned OK.
4. Analyze measured `retune phases us:` and `tune record us:` events; inspect both the count of records executed and any per-record ~7.8ms floor. Determine whether host scheduling, USB control completion, I2C forwarding, explicit waits or register writes dominate.
5. Try one **isolated** optimization at a time behind an opt-in driver flag. Preserve fail-closed behavior, V4/V4L route switching, PLL checks, known-good tuner registers and acquisition epochs. Never skip safety writes merely to win a timing number.
6. Repeat same captures with unchanged RF sources and compare sensitivity, decoded message totals, USB discontinuities, stale-epoch rejections, fault recovery and observed timing percentiles. No universal speed claims from one device.
7. Test live processing load and UI responsiveness on the actual ESP32-P4, including return-to-primary-channel behavior.

Do **not** assume EP0 concurrency with outstanding ESP32-P4 bulk URBs is safe merely because another desktop/libusb stack supports it. The correct opportunity must be proven in the actual USB host implementation.

## How to run/test

From a local checkout, run the host-only parser tests:

```bash
python3 -m unittest discover -s tests -p test_retune_latency_report.py -v 2>&1 | tee "$PWD/fast-retune-unit-tests.log"
```

Capture serial output containing the driver's `retune phases us` debug lines (and optionally `tune record us` debug lines). From the directory where you want all artifacts saved:

```bash
python3 /path/to/esp-rtl-sdr/tools/retune_latency_report.py --input /path/to/capture.log --prefix v4-aircraft-hops \
  2>&1 | tee "$PWD/fast-retune-analysis-run.log"
```

The parser writes `v4-aircraft-hops.csv`, `v4-aircraft-hops.json`, and `v4-aircraft-hops.log` to the invocation directory. It exits nonzero if no phase records were found and never treats missing data as zero latency.

## Design consequences for OrcSDR Aircraft Target Lock

The OrcSDR CPDLC/Target Lock branch owns the UI and shared aircraft identity/correlation logic. This driver branch must not modify OrcSDR or start a second RTL tuner owner.

A single receiver will always pause its original signal when retuned; faster hopping only improves scan coverage. **Simultaneous VHF CPDLC/ACARS and 1090 MHz ADS-B requires independent RF receivers**, or an explicitly labeled external feed. Design for a multi-device allocation policy, tune/settle/dwell budget and honest partial-reception indicators. Scanning all aviation voice/data frequencies is not a sound guarantee of full aircraft coverage.

## Source pointers

- `src/esp_rtl_sdr.cpp`: `run_tune`, `run_profile_tune`, `bulk_pause_and_drain`, `apply_pending_retune`
- `docs/API_REFERENCE.md`: `esp_rtl_sdr_retune_hz` asynchronous semantics
- `docs/MULTI_DEVICE_ARCHITECTURE.md`: independently owned handles and USB host
- `docs/PROJECT_TRUTH.md`: current support and validation boundaries
- ICAO ARINC/AT1 CPDLC identification: https://www.icao.int/sites/default/files/sp-files/SAM/Documents/2010/SURAUTOSEM/17%20Arinc_CPDLC%20ADSC.pdf

No physical device was reflashed or benchmarked by this document. Qualification remains OPEN until attached run logs and actual RF checks exist.
