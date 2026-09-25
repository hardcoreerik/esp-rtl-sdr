# multi_rtlsdr_test

Hardware validation harness for **concurrent** RTL-SDR receivers on ESP32-P4.

This is a driver test, not an anomaly detector. Captures are **concurrent and
host-timestamped**, not phase-coherent.

## Hardware

Primary board: **Waveshare ESP32-P4-Module-DEV-KIT**.

USB topology (from Waveshare FAQ + schematic):

```
                    jumper
                 HOST | DEVICE
                      |
  P4 USB HS  ---- FSUSB42 mux ----+
                                  |
                     HOST: USB-A port 1 (direct)
                     DEVICE: CH334F hub → USB-A ports 2, 3, 4
```

Port 1 and ports 2–4 are **mutually exclusive**. Two or three dongles require:

1. Jumper on **DEVICE**
2. Dongles in USB-A ports **2, 3, 4**
3. `CONFIG_USB_HOST_HUBS_SUPPORTED=y` (this example’s default)

Keep **bias tees OFF**. See `docs/WAVESHARE_P4_MULTI_RTL_PROTOTYPE.md`.

## Build (ESP-IDF 5.5.4, ESP32-P4)

```bash
cd examples/multi_rtlsdr_test
idf.py -B build -D SDKCONFIG=build/sdkconfig set-target esp32p4
idf.py -B build -D SDKCONFIG=build/sdkconfig build
idf.py -B build -D SDKCONFIG=build/sdkconfig -p COMx flash monitor
```

Tab5 (rev v1.3) overlay:

```bash
idf.py -B build-tab5 -D SDKCONFIG=build-tab5/sdkconfig \
  -D SDKCONFIG_DEFAULTS="sdkconfig.defaults;sdkconfig.defaults.tab5" \
  set-target esp32p4
```

## Gates

| Gate | Action |
|---|---|
| 1 | One dongle, `rtl list`, `rtl stream 0 start`, `rtl stats` |
| 2 | Two dongles enumerate; `rtl info 0` / `rtl info 1` distinct USB addr |
| 3 | `rtl stream all start` at 2.4 MS/s, watch ≥10 min |
| 4 | Three dongles enumerate |
| 5 | Three concurrent streams; record drops/errors |
| 6 | `rtl freq 1 101100000` while 0 and 2 keep streaming |

Do not publish estimates as measurements. Fill `docs/MULTI_DEVICE_PERFORMANCE.md`.
