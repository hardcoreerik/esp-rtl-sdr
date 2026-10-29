# COM39 Tab5 regression checks — 2026-10-29

No regression detected in the bounded driver checks below. The user explicitly
reauthorized COM39 flashing after the earlier hold. Tested functional source:
`003925e86e3abedd7d7c83b7875e3e84fa1ef9c8`; changes during this run are documentation only.

COM39 was freshly identified as ESP32-P4 rev1.3. Nooelec SMArt v5 enumerated at
the root high-speed port with serial 36309640. The saved 6-MiB factory application
and 4-KiB partition-table recovery files remain present. Only the application at
0x10000 was flashed; no bootloader, partition table or credential partition was
extracted or deliberately replaced during this pass. COM8 was not accessed.

All configurations used the Tab5 power helper, six 16-KiB URBs, BOTH delivery,
automatic pull ring, 960 kS/s quiet soak, 1-kHz ticks, 8-KiB main stack and no PSRAM.
Acquisition enabled sparse quality. Each image booted after flashing; these
were not physical power-cycle tests or repeated host installs in one boot.

| Configuration | Passed / failed / skipped | Quiet-window bytes | Samples/s | Overruns / drops |
|---|---|---|---|---|
| 5.5.4 default | 46/0/0 | 15,351,808 | 959,488 | 0 / 0 |
| 5.5.4 acquisition | 48/0/0 | 15,351,808 | 959,488 | 0 / 0 |
| 6.1 acquisition / USB 1.6.0 | 48/0/0 | 15,351,808 | 959,488 | 0 / 0 |

Each quiet window was 8,000 ms. The original maintenance baseline had 45 checks
and exactly the same bytes/rate/overrun/drop results. The latest default has 46
because the drain-worker join assertion was added; acquisition has 48. The earlier
144-byte metadata prototype also passed 48, but the current 152-byte format is now
physically checked on Tab5 under both SDKs. Named captured baseline rows remain
present in the new logs. No default-path metadata allocation was observed.

The matrix includes startup, reads, direct-Q/native RF route transitions,
manual gain, tuner AUTO/idempotence, independent RTL AGC, metrics, worker join,
stop and uninstall. Acquisition adds configuration and byte-associated metadata
invariants for reads and extended callbacks. Returned success does not measure
actual RF gain/passbands or prove reception on the requested bands.

No-PSRAM memory: default pull ring253,952bytes, acquisition245,760bytes plus
11,424bytes internal metadata runtime. Eight-second performance was unchanged.
Later control steps intentionally leave data undrained and accumulate software
drops, as in the baseline. Whole-run health warnings after those steps are not
the quiet-window result; no whole-run lossless claim is made. Partial-read span
counts vary with scheduling and are not sample-rate measurements.

## Commands

Use a fresh PowerShell for each SDK and export its matching installed environment:
```
$env:IDF_TOOLS_PATH='C:\Espressif'
. C:\Espressif\frameworks\esp-idf-v5.5.4\export.ps1
idf.py -C examples/p4_serial_smoke -B build/tab5-acquisition55 build

. C:\Espressif\v6.1\esp-idf\export.ps1
idf.py -C examples/p4_serial_smoke -B build/changed61 build
```
The saved effective sdkconfig files record the exact options; acquisition was
explicitly enabled/disabled before building each image. IDF 5.5.4's pre-existing
unrelated local SDK modifications were retained; SDK6.1 used managed USB 1.6.0.

For each saved image, using its concrete stem from the artifact records:
```
C:\Espressif\python_env\idf6.1_py3.14_env\Scripts\python.exe -m esptool --port COM39 --baud 921600 write-flash 0x10000 build/phase2-evidence/<stem>.bin
C:\Espressif\python_env\idf6.1_py3.14_env\Scripts\python.exe build/phase2-evidence/capture_com39_regression.py --prefix <stem>
```
The actual command spelling uses separate `--baud 921600` and `write-flash 0x10000`
arguments. The local capture helper sets DTR/RTS false before opening 115200 baud,
flushes logs as they arrive, stops at the overall marker and checks PASS/zero
failures/hardware RUN. All three captures exited 0. Build and flash logs use the
same `com39-regression*` prefixes in the evidence directory.

## Limits and retained device state

COM39 currently retains the IDF 6.1 acquisition smoke image. It is serial-only,
with no normal display UI. Original recovery files are retained. No runtime
source change was needed to obtain these results. No master merge or release.

Long-duration Tab5 soak, physical power-cycle/replug, PSRAM, RF decoding,
externally calibrated timing and OrcSDR UI/audio paths remain untested here.
These results do not close the documented late-attachment/host-reentry gates.
The earlier independent-review usage-limit blocker is unchanged.

## Artifact records

### 5.5.4 default

- Log: `build/phase2-evidence/com39-regression55-default-serial.log`
- Capture UTC: 2026-10-29T22:57:06.744289+00:00 through 2026-10-29T22:57:24.312150+00:00
- Log SHA-256: `22b9629b173f525d18f37102f3b6726e71a2141ac0abceee0afecde42256712b`
- Image SHA-256: `0301c2c774adb7b65e36905c28ed093f58b513c44eb557ea04e71c2b92952e5f`
- Saved effective configuration: `build/phase2-evidence/com39-regression55-default.sdkconfig`

### 5.5.4 acquisition

- Log: `build/phase2-evidence/com39-regression55-acquisition-serial.log`
- Capture UTC: 2026-10-29T22:54:40.307583+00:00 through 2026-10-29T22:54:57.848159+00:00
- Log SHA-256: `4f8428661af3c10218c4205df761cbdf8b2364ec21a14aa239310dc6c4b03a11`
- Image SHA-256: `fb0f1e1aaf5321a0b10d6a89e753019672df0a3ef7c4c9a353158d20791777d9`
- Saved effective configuration: `build/phase2-evidence/com39-regression55-acquisition.sdkconfig`
- Metadata summary: `read_blocks=964 read_errors=0 known_lost=0 callbacks=1379 callback_errors=0 metadata_bytes=152`

### 6.1 acquisition / USB 1.6.0

- Log: `build/phase2-evidence/com39-regression61-acquisition-serial.log`
- Capture UTC: 2026-10-29T23:00:35.855296+00:00 through 2026-10-29T23:00:53.424750+00:00
- Log SHA-256: `eda62595e9d7d53500c5cb81694ed076799125943cec4a1db505280c58b13a30`
- Image SHA-256: `4ea51d065f1641ecc7004fefcb2035fe1433d9d74b0e37e54938e1567dce7639`
- Saved effective configuration: `build/phase2-evidence/com39-regression61-acquisition.sdkconfig`
- Metadata summary: `read_blocks=958 read_errors=0 known_lost=0 callbacks=1379 callback_errors=0 metadata_bytes=152`
