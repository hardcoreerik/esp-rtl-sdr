# v0.7.11 handoff (driver -> Tab5 operator)

Immutable once tagged: `v0.7.11` on `https://github.com/hardcoreerik/esp-rtl-sdr`.

Supersedes the hardware soak of `v0.7.10`. That tag is still valid software
(host tests 225/225, both images built) but `p4_serial_smoke` `soak_drain`
overflowed a 4 KiB task stack with a 16 KiB local buffer (issue #11). Do not
re-flash `v0.7.10` for soak evidence.

## What this release is

- Public version macros / `idf_component.yml` / `library.json`: **0.7.11**
- Smoke example: same quiet USB soak + L4 matrix as 0.7.10, plus `s_drain_buf`
  moved off the `soak_drain` stack
- **Not** a claim that Tab5 USB efficiency is fixed. Hardware soak is your job.

## Do not use

- Tag `v0.7.10` for soak (issue #11)
- Dirty worktree builds
- Log `p4_serial_smoke_COM17_0_7_10_usb_soak_2026-08-26.txt` (INVALID_STATE)

## Build both images (NEWCOREPC, IDF 5.5.4)

Checkout **tag `v0.7.11`** only.

```bat
cd examples\p4_serial_smoke

rem --- A: 6x16 KiB ---
idf.py set-target esp32p4
idf.py build
rem confirm project_description.json project_version == 0.7.11
rem confirm min_rev == 0
idf.py -p COM17 flash monitor

rem --- B: 3x32 KiB (separate build dir) ---
idf.py -B build-3x32k -D SDKCONFIG_DEFAULTS="sdkconfig.defaults;sdkconfig.defaults.urb_3x32k" set-target esp32p4
idf.py -B build-3x32k build
idf.py -B build-3x32k -p COM17 flash monitor
```

Boot must print `esp_rtl_sdr 0.7.11 soak=usb_soak_960k_6x16k urbs=6x16384`
or `... soak=usb_soak_960k_3x32k urbs=3x32768`.

## Pass criteria (each image)

- No stack-protection fault in `soak_drain`
- `SMOKE <soak-row> PASS`
- `SOAK ... eff>=90` and advice not USB starving
- `SMOKE OVERALL PASS ... hardware=RUN`
- Preserve serial log + bin SHA-256 under
  `\\HARDCOREPC\HARDCOREPC F-Drive\Ai\ESP_RTL_SDR\docs\Test_reports\`
- Restore OrcSDR on COM17 when done (Codex)

## Host tests (already green on author machine)

```bat
tests\scripts\run_host_tests.ps1
rem expect: RESULT passed=225 failed=0  HOST_TESTS_OK
```