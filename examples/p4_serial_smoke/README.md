# p4_serial_smoke

Minimal ESP-IDF app that links **esp_rtl_sdr** and exercises the public API
(helpers, install/start/read, need/health/passport when a Blog V4 is present).

## Build (ESP32-P4)

```bash
export IDF_PATH=...   # ESP-IDF ≥ 5.3 with esp32p4 support
cd examples/p4_serial_smoke
idf.py set-target esp32p4
idf.py build
# optional: idf.py -p COMx flash monitor
```

The component is pulled via `components/esp_rtl_sdr/` (stable name wrapping the
repo root sources). No dongle is required for a successful **compile**.

## CI

GitHub Actions job `idf-p4-build` only compiles (no hardware). Lab soak remains
manual — see `docs/LAB_HOBBYIST.md` and `docs/TESTING_GUIDE.md`.
