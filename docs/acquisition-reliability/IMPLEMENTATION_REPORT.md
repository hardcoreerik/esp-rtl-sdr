# Implementation status — 2026-10-29

Feature branch: `feature/decoder-grade-acquisition-20261029`, based on maintenance
commit `b283029dd0257ec207d5e94db3730077c40a23de`. No master merge or release.

Implemented: optional extended C API, completion-time immutable metadata,
per-receiver epochs and observed sample indices, per-channel known delivery
loss, unknown interruption markers, bounded pull descriptors, partial reads,
optional integer quality and explicit sample-clock uncertainty. READ skips
unused callback buffers. Failed recovery drain retains outstanding resources.
Opted-in control boundaries also replay the existing captured FIFO-reset pair;
conducted RF settling/timing is not established by that digital fence.

Nooelec SMArt v5 is **Implemented**, not experimental or provisional. This is
supported by first-party control captures, host regressions and standalone Tab5
streaming/control checks. RF calibration and per-band qualification remain
separate, as for other implemented profiles. The Waveshare dual-device run
used V4 and V3/R820T2, not Nooelec.

Current verification and pending qualification are listed in [test plan](TEST_PLAN.md).
Final refinement/soak and the installed SDK build matrix passed; functional
implementation is committed and pushed. Independent review remains open. The requested independent reviewer
could not run because its account usage limit was reached; no clean independent
review is claimed.

COM8 currently runs a serial-only test program, so the Hosyond display is blank.
Its original factory application and partition table remain backed up locally.
COM39 remains under the user's no-flash restriction. Neither board's original
application has been restored. Recovery backups and raw hardware logs are
local ignored artifacts, not committed release deliverables.

Unresolved maintenance gates remain: conservative cleanup when enumeration-idle
cannot be proved, late attachment between ALL_FREE and uninstall, and reliable
host reinstall after those failures. Cleanup errors must not be treated as a
successful release of ownership.

## Previous maintenance state

The preceding maintenance commit retained shared USB ownership, joined workers
before freeing, allocated DMA before optional rings and propagated cleanup
failures. It was pushed on `fix/external-driver-findings-20261029`; master was
not changed. The baseline hardware test passed before adding acquisition.
See `docs/reviews/EXTERNAL_DRIVER_FINDINGS_2026-10-29.md` for exact findings.

## What external applications gain

`configure_acquisition` enables integrity metadata before capture;
`read_ex` associates copied bytes with an immutable original configuration;
`EVT_IQ_BLOCK_EX` gives callback users the same association. Decoder authors
can detect interruptions and reset continuity without guessing from a later
live metrics snapshot. Per-handle epochs and independent delivery cursors keep
one receiver's restart or one consumer's drops from relabeling another stream.

Quality statistics expose CU8 rail hits, I/Q balance, power and peaks without
a decoder dependency. They are code-domain diagnostics, not calibrated RF power.
Clock fields distinguish nominal resampler arithmetic from external calibration.
The application may provide an estimate and uncertainty; the driver does not
pretend USB completion timing measures the RF sample clock.

## Compatibility

Existing API signatures and IQ payload ABI are preserved. Acquisition is opt-in;
disabled handles add 40 bytes. Opted-in legacy reads consume the metadata sidecar
and may return a single descriptor span. Callback/BOTH deliver the extra event
only when enabled. READ omits unused callback slot buffers. All physical tests
used no PSRAM; PSRAM hardware qualification remains open. Future high-speed
targets need independent hardware evidence despite successful compilation.

## Source map

- `include/esp_rtl_sdr_acquisition.h`: public C configuration, metadata, flags,
  extended borrowed payload and read/configure functions.
- `private/rtl_acquisition.hpp`: Timeline, Cursor, Sidecar, quality and clock math.
- `src/esp_rtl_sdr.cpp`: bulk_cb snapshots, pull_ring_push, acquisition_read,
  acquisition_epoch, run_acquisition_fifo_reset, safe recovery and allocation.
- `src/esp_rtl_sdr_policy.cpp`: public default initializers.
- `examples/acquisition_probe`: neutral C multi-receiver hardware harness.
- `examples/p4_serial_smoke/main`: optional extended callback/read assertions.
- `tests/host` and lifecycle generator: model, extracted runtime, C ABI,
  allocator, recovery, FIFO and benchmark coverage.
- Current README/truth/scope/capability/profile comments: Nooelec Implemented.
- `idf_component.yml`: S31 target declaration; no version increment.

## Reproduction and artifacts

From an exported IDF 5.5.4 PowerShell:
```
idf.py -C examples/acquisition_probe -B build/waveshare-multi55 build
```
The ignored sdkconfig selects pre-v3 P4 silicon, two receivers, built-in hub,
1-kHz ticks, 8-KiB main stack and a 900,000-ms final duration.
Only the application at 0x10000 was written on COM8:
```
C:\Espressif\python_env\idf6.1_py3.14_env\Scripts\python.exe -m esptool --port COM8 --baud 921600 write-flash 0x10000 build/waveshare-multi55/esp_rtl_sdr_acquisition_probe.bin
```
Serial capture used `build/phase2-evidence/capture_com8.py --prefix
waveshare-final-long --duration 960`. That local helper fixes COM8, disables
DTR/RTS before opening, and records UTC capture bounds separately.

IDF 6.1 compile commands:
```
idf.py -C examples/p4_serial_smoke -B build/changed61 build
idf.py --preview -C examples/acquisition_probe -B build/probe-s31 -D IDF_TARGET=esp32s31 build
```

All raw logs live in ignored `build/phase2-evidence/`: baseline
`tab5-baseline55-serial.log`, initial opt-in `tab5-acquisition55-serial.log`,
V4 `waveshare-acquisition55-serial.log`, initial shared `waveshare-multi55-serial.log`,
final short `waveshare-final-short-serial.log`, final long
`waveshare-final-long-serial.log`; build logs `final-p4-61-build.log`,
`final-s31-61-build.log`, `final-short-build.log`, `long55-build.log`.
Host result logs: `pre-review-windows-test.log`, `pre-review-linux.log`.
Recovery images: `com39-original-factory-app.bin`,
`com39-original-partitions.bin`, `com8-original-factory-app.bin`,
`com8-original-partitions.bin`. These files are not portable release packages.

## Git and future release scope

Functional commit: `f92aa10` (this record will be followed by an evidence/docs
commit). Base master: `73ea1fbd3fbd404842bdc07ccc284dee276b9fc3`;
maintenance base: `b283029dd0257ec207d5e94db3730077c40a23de`.
Functional commit was pushed and verified on the feature branch. The following
documentation commit records final evidence; its exact hash is reported in the
delivery message (a commit cannot embed its own hash).

An appropriate future release would contain maintenance safety fixes, opt-in
metadata, documented uncertainty/quality, READ memory savings and the Nooelec
Implemented status. S31 should remain build-verified pending hardware, and
teardown/reentry limitations must remain explicit. No version or tag was changed.

Final compile verification: IDF 5.5.4 P4 acquisition probe, IDF 6.1 P4 smoke,
and IDF 6.1 S31 acquisition probe all completed successfully. S31 was not flashed.
Long-run COM8 image SHA-256:
`52e14fc0f0872495279f125f3028d3a01093c8a4bf4ba5f0d969bc7d4e47c53e`.
Tested sdkconfig SHA-256:
`cab54966260cb7a7afd336eb3711cddcbea7bc0bf607030d6a3200f50e2d0d6f`.
The application was built from the functional tree committed as f92aa10 before
that commit was created; subsequent changes are comments/docs and a matching
reproduction profile, not runtime changes. Its project version remains 0.9.3.
Target ELF confirms handle=2552 and AcquisitionRuntime=11424 bytes.

A fresh equivalent Waveshare build can use the tracked
`examples/acquisition_probe/sdkconfig.waveshare_multi` profile. All nine profile
values were compared against the tested effective sdkconfig and matched.

## Final COM8 result

Final run: 900,003 ms, OVERALL PASS failures=0, with successful final cleanup.
V4 receiver0 delivered 1,324,711,936 bytes in 80,854 spans before its scheduled
release; epochs 1/2/3 covered initial capture, retune and rate-changing restart.
It reported zero known lost pairs and two unknown continuity markers at the
control boundaries. V3/R820T2-profile receiver1 remained in epoch1 at 2.4 MS/s:
4,320,247,806 delivered bytes, 263,687 spans, zero metadata invariant errors,
zero USB errors, one known lost pair (2 ring-overrun bytes). Its observed receive
counter reached 4,320,247,808 bytes, beyond 4 GiB; no metadata-pressure bytes
were dropped. A final status snapshot is not a promise all pending bytes were
drained through teardown. This is a bounded acquisition test, not RF decoding
or a universal lossless/reentry guarantee.

Capture UTC: 2026-10-29T22:36:46.130222Z through 22:51:52.644936Z.
Serial log SHA-256:
`39cb09d08909e5db0a5e72dfc0b22a8f3ee7c93a397f3c78ca1075cfa6ac1d1d`.
The test ran on COM8 only; COM39 was not flashed during this final work.
All implementation work selected for this session and available automated/
hardware checks are delivered. Independent review and the explicitly listed
physical/calibration qualification gates remain outstanding.