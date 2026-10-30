# COM39 no-PSRAM and attached-dongle uninstall check

2026-10-30 Pacific. One bounded run of the existing standalone p4_serial_smoke
image, source matching merged driver f14c8565e274bc7a5a68523fd1f0e9b7879a3d85,
ESP-IDF 5.5.4, Tab5 COM39 and attached Nooelec SMArt v5. Configuration:
CONFIG_SPIRAM unset, Tab5 power profile, 6 x 16 KiB URBs, extended acquisition
metadata disabled. Physical PSRAM exists on the Tab5 but this image does not
register/use it; this tests the no-PSRAM allocation path, not another board.

The first-party source findings originated with Przemek Godek
(https://github.com/pgodek), whose hardware was Waveshare/V4/IDF 5.5.6. This
run neither reproduces his old abort nor substitutes for that exact setup.

Results:
- SMOKE OVERALL PASS passed=46 failed=0 skipped=0 hardware=RUN.
- The pull ring shrank from a 393216-byte auto target to 253952 bytes. USB
  allocations, reads and the later control matrix completed.
- The separate eight-second quiet window delivered 959488 samples/second,
  15351808 bytes, zero overruns, drops or short transfers; advice=ok.
- Stop PASS and uninstall PASS, with the dongle left physically attached.
- No panic/abort was observed in the captured run.

Later cumulative drop/starvation diagnostics follow control steps which leave
IQ undrained. They are not the quiet-window result and are not evidence of a
lossless whole run. Full serial evidence and hashes accompany this report.

Only the application at 0x10000 was temporarily replaced. The existing
partition table, bootloader, NVS and C6 were not replaced or erased. The
previously tested OrcSDR image (SHA-256
0a5c3e886af28840e13ee67cfe58e172a606969a36003748e2d9a408ec9fa3bd)
was restored in a finally block and esptool verified its flash hash. COM17
and COM8 were not flashed by this check; the tinySA remained off.

This adds bounded physical evidence for the two original bug classes. It does
not close incomplete-enumeration, late-attachment, repeated reentry, no-dongle,
other-dongle/board or extended-acquisition no-PSRAM qualification. Existing
fail-closed teardown and hardware-acceptance caveats remain.
