# p4_serial_smoke

ESP-IDF app that links **esp_rtl_sdr** and exercises the public 0.7.12 drop-in
contract on ESP32-P4 / Tab5.

The explicitly selected Tab5 profile uses M5Unified's power-expander helper to enable the USB-A rail
before installing the USB host, so cold boots do not depend on previously run
firmware or initialize unrelated display/audio hardware.

**One USB host install per boot.** IDF 5.5.4 `usb_host_uninstall` is not a
reliable re-entry on Tab5, so A/B URB layouts are **two firmware images**,
not two installs in one run.

## URB layouts (Kconfig)

| Image | Kconfig | `transfer_count x transfer_bytes` | Soak row |
|---|---|---|---|
| default | `CONFIG_ESP_RTL_SDR_SMOKE_URB_6X16K` | 6 x 16384 | `usb_soak_960k_6x16k` |
| overlay | `CONFIG_ESP_RTL_SDR_SMOKE_URB_3X32K` | 3 x 32768 | `usb_soak_960k_3x32k` |

Quiet soak: BOTH + auto ring, drain via `read()` on a helper task, no
gain/AGC/bias during the window, 960 kS/s, 8 s default. After `first_read`
the stream is stopped and started again on the **same USB host install** so
SOAK bytes/overruns/drops/advice are the drain window, not the 400 ms
undrained wait. PASS if scoped efficiency >= 90%, delta overruns == 0,
delta consumer drops == 0, scoped advice not `USB_STARVING` or
`APP_TOO_SLOW`, and IQ continues. Then the L4 gain / Tuner AUTO / RTL AGC /
`read()` matrix.

IQ `EVT_IQ_BLOCK` logging is suppressed so serial cannot stall delivery.

## Rows

```text
SMOKE <name> PASS|FAIL|SKIP
SOAK <label> eff=<pct> sps=<n> bytes=<n> over=<n> drops=<n> short=<n> urbs=<c>x<b> advice=<s> window_ms=<ms>
SMOKE OVERALL PASS|FAIL passed=<n> failed=<n> skipped=<n> hardware=RUN|SKIP
```

No dongle: RF tests use `hardware=SKIP`. On this maintenance branch final
owned-host uninstall returns an error and retains the host because enumeration
idle cannot be proven from an empty device list. The uninstall row and overall
result therefore FAIL; this is a cleanup limitation, not an RF failure.
Attached Blog V4: soak + L4 (`hardware=RUN`).

## Tab5 / P4 silicon (IDF 5.5.4)

`sdkconfig.defaults` contains:

```
CONFIG_ESP32P4_SELECTS_REV_LESS_V3=y
CONFIG_ESP32P4_REV_MIN_0=y
```

Tab5 is ESP32-P4 **rev v1.3**. Without `SELECTS_REV_LESS_V3`, IDF 5.5.4 silently
selects min_rev 3.1 and esptool refuses: bootloader requires [v3.1-v3.99], chip is v1.3.

These symbols are **this example only**. They do not ship in the library. P4 rev
<3.0 vs >=3.0 firmware is mutually exclusive; a v3.1+ module must not use this default.

`CMakeLists.txt` sets `PROJECT_VER` to `0.9.3` so the image identity matches the
driver, not git-describe of an older tag.

## Build (ESP32-P4, IDF 5.5.4)

IDF 5.5.4 keeps `sdkconfig` in the **project directory** unless `SDKCONFIG` is
set. `-B` alone is not isolation: overlay `set-target` can rewrite the file the
default image then builds. This example pins `SDKCONFIG` under the `-B` dir.
Still use two directories and two `sdkconfig` paths.

Default image (6 x 16 KiB):

```bash
cd examples/p4_serial_smoke
idf.py -B build-6x16k -D SDKCONFIG=build-6x16k/sdkconfig set-target esp32p4
idf.py -B build-6x16k -D SDKCONFIG=build-6x16k/sdkconfig build
grep CONFIG_ESP_RTL_SDR_SMOKE_URB build-6x16k/sdkconfig
# require: CONFIG_ESP_RTL_SDR_SMOKE_URB_6X16K=y
# require: # CONFIG_ESP_RTL_SDR_SMOKE_URB_3X32K is not set
# Tab5: idf.py -B build-6x16k -p COM17 flash monitor
```

3 x 32 KiB image (separate build dir **and** sdkconfig):

```bash
cd examples/p4_serial_smoke
idf.py -B build-3x32k -D SDKCONFIG=build-3x32k/sdkconfig \
  -D SDKCONFIG_DEFAULTS="sdkconfig.defaults;sdkconfig.defaults.urb_3x32k" set-target esp32p4
idf.py -B build-3x32k -D SDKCONFIG=build-3x32k/sdkconfig \
  -D SDKCONFIG_DEFAULTS="sdkconfig.defaults;sdkconfig.defaults.urb_3x32k" build
grep CONFIG_ESP_RTL_SDR_SMOKE_URB build-3x32k/sdkconfig
# require: CONFIG_ESP_RTL_SDR_SMOKE_URB_3X32K=y
# require: # CONFIG_ESP_RTL_SDR_SMOKE_URB_6X16K is not set
# Tab5: idf.py -B build-3x32k -p COM17 flash monitor
```

Do not flash until the matching `sdkconfig` grep is correct.

Confirm at boot: `esp_rtl_sdr 0.7.12 soak=usb_soak_960k_6x16k urbs=6x16384`
or `soak=usb_soak_960k_3x32k urbs=3x32768`. App version in
`build-6x16k/project_description.json` or
`build-3x32k/project_description.json` must be `0.7.12`.

The component is pulled via `components/esp_rtl_sdr/` (stable name wrapping the
repo root sources). No dongle is required for a successful **compile**.

Hardware soak evidence is produced on COM17 by the Tab5 operator (Codex), not
by this example's author flashing from a dirty tree.

## CI

GitHub Actions job `idf-p4-build` compiles the default image (no hardware).

## Explicit board profile

Select `ESP_RTL_SDR_SMOKE_BOARD_TAB5`, `..._GENERIC`, or `..._UNKNOWN` in menuconfig.
The existing `sdkconfig.defaults` explicitly selects Tab5 and pre-v3 silicon.
For another board, replace that defaults file with `sdkconfig.generic.defaults`
and verify the actual silicon revision before flashing. GENERIC means the board
or application already supplies USB host power; this app does not configure it.
UNKNOWN reports power as not verified. Neither profile probes Tab5 I2C hardware.

Tab5 runs the power test and still FAILs on a broken expander path. GENERIC prints
`SMOKE tab5_usb_power SKIP reason=NOT_APPLICABLE`; UNKNOWN uses
`reason=NOT_VERIFIED`. The skipped counter is separate from passed/failed.
`SMOKE OVERALL PASS` means no executed checks failed, not that skipped hardware
was verified. The Tab5 helper verifies the expander output latch, not VBUS under
load or the physical voltage.

## ESP-IDF 6.1 development comparison

This branch also builds with IDF 6.1 and `espressif/usb` **1.6.0**, selected only
on IDF >=6.0 through component manifests. The driver is explicitly compiled as
C++17. IDF 5.5.4 keeps its bundled USB component. Consumer pins are unchanged.
Build success is not hardware acceptance; host re-entry remains unsupported.

From the repository root, after exporting the chosen IDF environment:

```powershell
idf.py -C examples/p4_serial_smoke -B build/tab5-61 build
idf.py -C examples/p4_serial_smoke -B build/generic-61 -D SDKCONFIG_DEFAULTS=sdkconfig.generic.defaults build
```

Each build uses its own `sdkconfig` under `-B`; do not pass a relative SDKCONFIG
path intended to be relative to the repository root. IDF resolves that path
relative to the application directory. Inspect the resulting sdkconfig before
flashing. No board was flashed during this investigation.
