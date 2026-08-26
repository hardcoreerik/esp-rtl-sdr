# p4_serial_smoke

ESP-IDF app that links **esp_rtl_sdr** and exercises the public 0.7.9 drop-in
contract on ESP32-P4 / Tab5.

Default install only: `delivery_mode` BOTH, `pull_ring_bytes` 0 (auto). The
driver must shrink the auto ring if PSRAM is missing; this app must **not**
hardcode 64 KiB.

IQ `EVT_IQ_BLOCK` logging is suppressed so serial cannot stall delivery.

## Rows

```text
SMOKE <name> PASS|FAIL
SMOKE OVERALL PASS|FAIL passed=<n> failed=<n> hardware=RUN|SKIP
```

No dongle: helpers + install/uninstall still pass (`hardware=SKIP`).
Attached Blog V4: L4 gain / tuner AUTO / RTL AGC / read matrix (`hardware=RUN`).

## Build (ESP32-P4)

```bash
export IDF_PATH=...   # ESP-IDF ≥ 5.3 with esp32p4 support
cd examples/p4_serial_smoke
idf.py set-target esp32p4
idf.py build
# optional: idf.py -p COMx flash monitor
```

`sdkconfig.defaults` selects pre-v3 P4 silicon so a Tab5 rev v1.3 can flash.

The component is pulled via `components/esp_rtl_sdr/` (stable name wrapping the
repo root sources). No dongle is required for a successful **compile**.

## CI

GitHub Actions job `idf-p4-build` only compiles (no hardware). Lab soak remains
manual — see `docs/LAB_HOBBYIST.md` and `docs/TESTING_GUIDE.md`.

L5 unchanged-consumer (OrcSDR) and L6 soak remain open.
