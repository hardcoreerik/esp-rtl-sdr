# v0.7.12 handoff (driver -> Tab5 operator)

Immutable once tagged: `v0.7.12` on `https://github.com/hardcoreerik/esp-rtl-sdr`.

Supersedes the soak *evidence boundary* of `v0.7.11`. That tag is still valid
software (stack-safe `soak_drain`) but the SOAK row printed cumulative
`consumer_drops` from the 400 ms wait before `first_read`. Do not treat
v0.7.11 Tab5 drop counts (507904 @ `6x16384`, 593920 @ `3x32768`) as drain
evidence.

## What this release is

- Public version macros / `idf_component.yml` / `library.json`: **0.7.12**
- Smoke example: same quiet USB soak + L4 matrix as 0.7.11, plus a same-install
  stream restart so SOAK bytes/over/drops/advice are the 8 s drain window
- **Not** a claim that Tab5 USB efficiency is fixed. Hardware soak is your job.

## Do not use

- Tag `v0.7.10` for soak (issue #11)
- v0.7.11 SOAK `drops=` as proof the 8 s drain overflowed
- Dirty worktree builds

## Build both images (NEWCOREPC, IDF 5.5.4)

Checkout **tag `v0.7.12`** only.

IDF 5.5.4 `-B` does **not** isolate `sdkconfig`. Overlay `set-target` wrote
the project-root file, so a later default `idf.py build` booted as
`usb_soak_960k_3x32k`. Use a dedicated `-B` **and** `SDKCONFIG=` per image.
Do not flash until the URB grep matches.

```bat
cd examples\p4_serial_smoke

rem --- A: 6x16 KiB ---
idf.py -B build-6x16k -D SDKCONFIG=build-6x16k/sdkconfig set-target esp32p4
idf.py -B build-6x16k -D SDKCONFIG=build-6x16k/sdkconfig build
findstr CONFIG_ESP_RTL_SDR_SMOKE_URB build-6x16k\sdkconfig
rem require: CONFIG_ESP_RTL_SDR_SMOKE_URB_6X16K=y
rem require: # CONFIG_ESP_RTL_SDR_SMOKE_URB_3X32K is not set
rem confirm build-6x16k\project_description.json project_version == 0.7.12
rem confirm min_rev == 0
idf.py -B build-6x16k -p COM17 flash monitor

rem --- B: 3x32 KiB (separate build dir AND sdkconfig) ---
idf.py -B build-3x32k -D SDKCONFIG=build-3x32k/sdkconfig -D SDKCONFIG_DEFAULTS="sdkconfig.defaults;sdkconfig.defaults.urb_3x32k" set-target esp32p4
idf.py -B build-3x32k -D SDKCONFIG=build-3x32k/sdkconfig -D SDKCONFIG_DEFAULTS="sdkconfig.defaults;sdkconfig.defaults.urb_3x32k" build
findstr CONFIG_ESP_RTL_SDR_SMOKE_URB build-3x32k\sdkconfig
rem require: CONFIG_ESP_RTL_SDR_SMOKE_URB_3X32K=y
rem require: # CONFIG_ESP_RTL_SDR_SMOKE_URB_6X16K is not set
idf.py -B build-3x32k -p COM17 flash monitor
```

Boot must print `esp_rtl_sdr 0.7.12 soak=usb_soak_960k_6x16k urbs=6x16384`
or `... soak=usb_soak_960k_3x32k urbs=3x32768`.
It must also print `SMOKE tab5_usb_power PASS`; the example initializes
M5Unified and enables the Tab5 USB-A rail before driver installation.

## Pass criteria (each image)

- No stack-protection fault in `soak_drain`
- `SMOKE <soak-row> PASS`
- `SOAK ...` uses scoped/delta bytes, overruns, drops; `eff>=90`; `over=0`;
  `drops=0`; advice not `USB_STARVING` or `APP_TOO_SLOW`; IQ continuing
- `SMOKE OVERALL PASS ... hardware=RUN`
- Preserve serial log + bin SHA-256 under
  `\\HARDCOREPC\HARDCOREPC F-Drive\Ai\ESP_RTL_SDR\docs\Test_reports\`
- Restore OrcSDR on COM17 when done (Codex)

## Host tests (already green on author machine)

```bat
tests\scripts\run_host_tests.ps1
rem expect: RESULT passed=249 failed=0  HOST_TESTS_OK
```
