# v0.7.10 handoff (driver -> Tab5 operator)

Immutable once tagged: `v0.7.10` on `https://github.com/hardcoreerik/esp-rtl-sdr`.

## What this release is

- Public version macros / `idf_component.yml` / `library.json`: **0.7.10**
- Smoke example: quiet USB soak + L4 matrix; Tab5 P4 rev defaults
- **Not** a claim that Tab5 USB efficiency is fixed. Hardware soak is your job.

## Do not use

- Dirty worktree builds (`v0.7.9-dirty`, untagged `grok/v0.7.10-usb-starve` WIP)
- Log `p4_serial_smoke_COM17_0_7_10_usb_soak_2026-08-26.txt` (FAILED soaks:
  `ESP_ERR_INVALID_STATE` from multi-install). Discard as evidence.

## Build both images (NEWCOREPC, IDF 5.5.4)

Checkout **tag `v0.7.10`** only.

```bat
cd examples\p4_serial_smoke

rem --- A: 6x16 KiB ---
idf.py set-target esp32p4
idf.py build
rem confirm project_description.json project_version == 0.7.10
rem confirm min_rev == 0
idf.py -p COM17 flash monitor

rem --- B: 3x32 KiB (separate build dir) ---
idf.py -B build-3x32k -D SDKCONFIG_DEFAULTS="sdkconfig.defaults;sdkconfig.defaults.urb_3x32k" set-target esp32p4
idf.py -B build-3x32k build
idf.py -B build-3x32k -p COM17 flash monitor
```

Boot must print `esp_rtl_sdr 0.7.10 soak=usb_soak_960k_6x16k urbs=6x16384`
or `... soak=usb_soak_960k_3x32k urbs=3x32768`.

## Pass criteria (each image)

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