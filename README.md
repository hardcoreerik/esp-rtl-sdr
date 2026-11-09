# esp-rtl-sdr

A standalone RTL-SDR USB Host driver for **ESP32-P4**, written for ESP-IDF.

`esp-rtl-sdr` is a clean-room implementation, not a port or wrapper of `librtlsdr`. It handles supported RTL2832U-based USB receivers and provides raw I/Q samples and hardware controls to ESP32-P4 applications.

**Version:** 0.9.6 (pre-1.0) · **License:** AGPL-3.0-only

## Why this project exists

`librtlsdr` is designed primarily for computers. ESP32-P4 devices have different USB, memory, and processing constraints.

This project provides a native driver for those constraints, without requiring a desktop SDR library or a particular application. It works with compatible ESP32-P4 boards that have a properly configured High-Speed USB Host connection.

### Benefits on ESP32-P4

| Area | `esp-rtl-sdr` approach |
|---|---|
| Integration | Uses ESP-IDF USB Host and FreeRTOS directly |
| Streaming | Continuous I/Q delivery through callbacks or blocking reads |
| Monitoring | Built-in sample-rate, USB, overrun, and drop metrics |
| Sample rates | Can test which rates a particular host and receiver can sustain |
| Recovery | Explicit start/stop handling and USB disconnect recovery |
| Receiver support | Individual tuner profiles and per-device capability reporting |

`librtlsdr` supports more receiver types and is well established on desktop systems. This driver is focused on ESP32-P4; the table describes design differences, **not a claim of higher speed or better RF performance**.

## Features

- Continuous unsigned 8-bit I/Q streaming (interleaved I, Q)
- Frequency and sample-rate control
- Gain, AGC, bandwidth, bias tee, and HF controls where supported by the receiver
- Device discovery and multiple receiver sessions
- USB recovery, stream health, and performance metrics
- Rate testing and intent presets
- C API for ESP-IDF applications

The driver supplies I/Q samples and hardware control. Decoders, audio, displays, and networking belong to the application using it.

## Supported receivers

| Receiver | Status |
|---|---|
| RTL-SDR Blog V4 | Implemented; ESP32-P4 tested |
| RTL-SDR Blog V4 Lite | Implemented; ESP32-P4 tested |
| RTL-SDR Blog V3c | Implemented; ESP32-P4 tested |
| Nooelec NESDR SMArt V5 | Implemented; ESP32-P4 tested |
| Nooelec XTR / E4000 | Implemented; initial ESP32-P4 reception recorded |

Not all features are available on every receiver. Other RTL2832U dongles are not automatically supported.

See [receiver profiles](docs/PROFILES.md) and [current test status](PROJECT_TRUTH.md).

## Requirements

- ESP32-P4 with High-Speed USB Host
- ESP-IDF 5.5 or newer
- A supported RTL-SDR USB receiver
- Adequate USB power and an antenna for RF reception

The M5Stack Tab5 and Waveshare ESP32-P4 boards have been used for testing. **The driver is not tied to either board.** Each application must configure USB Host power and any board-specific hardware.

## Quick start

Clone the repository and build the included test application:

```bash
git clone https://github.com/hardcoreerik/esp-rtl-sdr.git
cd esp-rtl-sdr/examples/p4_serial_smoke
idf.py set-target esp32p4
idf.py build
```

The example's default configuration is intended for a tested Tab5 setup. **Before flashing another ESP32-P4 board**, select the appropriate board profile, USB power configuration, and silicon revision settings. See the [smoke test instructions](examples/p4_serial_smoke/README.md).

## Use in your own project

### 1. Add the driver

From your ESP-IDF project's root directory:

```bash
git submodule add https://github.com/hardcoreerik/esp-rtl-sdr.git components/esp_rtl_sdr
```

This keeps the ESP-IDF component name as `esp_rtl_sdr`. For repeatable builds, pin the submodule to a tested commit or release tag.

### 2. Declare the dependency

In your application's `main/CMakeLists.txt` (adjust the source filenames for your project):

```cmake
idf_component_register(
    SRCS "main.c"
    INCLUDE_DIRS "."
    REQUIRES esp_rtl_sdr
)
```

### 3. Read I/Q samples

This minimal example starts streaming at 100.1 MHz and reads one block. The receiver must already be detected and ready; a real application should wait for device attachment and handle failures.

```c
#include "esp_rtl_sdr.h"
#include "esp_log.h"

static const char *TAG = "rtl_example";
static uint8_t iq[4096];

void app_main(void)
{
    esp_rtl_sdr_config_t cfg;
    esp_rtl_sdr_config_default(&cfg);
    cfg.delivery_mode = ESP_RTL_SDR_DELIVERY_READ;

    esp_rtl_sdr_handle_t sdr = NULL;
    esp_err_t err = esp_rtl_sdr_install(&cfg, &sdr);
    if (err == ESP_OK) {
        err = esp_rtl_sdr_start_hz(sdr, 100100000, ESP_RTL_SDR_RATE_960K);
        if (err == ESP_OK) {
            size_t received = 0;
            err = esp_rtl_sdr_read(sdr, iq, sizeof(iq), 1000, &received);
            if (err == ESP_OK) {
                ESP_LOGI(TAG, "Received %u I/Q bytes", (unsigned)received);
                /* Process iq[0..received-1] here. */
            }
            (void)esp_rtl_sdr_stop(sdr, 0);
        }
    }
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Driver error: %s", esp_rtl_sdr_err_to_name(err));
    }
    if (sdr != NULL) {
        (void)esp_rtl_sdr_uninstall(sdr);
    }
}
```

USB Host power and board setup are not shown. For complete examples, see [usage examples](docs/EXAMPLES.md) and [p4_serial_smoke](examples/p4_serial_smoke/).

## API feature map

The public API covers device detection and selection, streaming, frequency and sample-rate control, receiver capabilities, gain and other tuner controls, health metrics, and USB lifecycle management.

**[Full API reference](docs/API_REFERENCE.md)** — function signatures, parameters, return values, and examples.

- [Public API header](include/esp_rtl_sdr.h) — current declarations
- [API design contract](docs/API.md) — threading and lifecycle rules
- [Usage examples](docs/EXAMPLES.md) — integration recipes

The header is the source of truth for available functions while the API reference is being brought up to date with version 0.9.6.

## Current limitations

- Pre-1.0: APIs may still change.
- Not every RTL2832U receiver or tuner is supported.
- RF calibration and extended hardware testing remain incomplete for some profiles.
- ESP32-S2/S3 Full-Speed USB Host support is not currently claimed.
- Not ABI-compatible with `librtlsdr`.

See [PROJECT_TRUTH.md](PROJECT_TRUTH.md) for the current implementation and verification status.

## Documentation

- [API reference](docs/API_REFERENCE.md)
- [Receiver profiles](docs/PROFILES.md)
- [Architecture](architecture.md)
- [Testing guide](docs/TESTING_GUIDE.md)
- [Troubleshooting](docs/TROUBLESHOOTING.md)
- [Changelog](CHANGELOG.md)
- [Project roadmap](Roadmap.md)
- [Full documentation index](docs/README.md)
- [AI development disclosure](docs/AI_DEVELOPMENT_DISCLOSURE.md)

## License

AGPL-3.0-only. See [LICENSING.md](LICENSING.md) for commercial licensing information.

## Support

Developed and maintained by **[Erik Elfstrom](https://github.com/hardcoreerik)**. Thanks to everyone who has tested hardware, reported problems, or contributed improvements.

If this driver is useful to you, consider supporting its continued development.

[Website](https://theorc.dev) · [GitHub Sponsors](https://github.com/sponsors/hardcoreerik) · [Ko-fi](https://ko-fi.com/hardcoreerik) · [PayPal](https://paypal.me/hardcoreerik)
