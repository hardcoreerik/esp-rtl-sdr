# Porting — host boards and consumers

## Driver vs board BSP

| In esp_rtl_sdr | In the application |
|---|---|
| USB host client for the dongle | VBUS / 5 V enable for host port |
| Profile init + bulk IQ | GPIO map for display, Ethernet PHY, codec |
| Metrics and events | Demod, UI, storage, network protocols |

**Do not** put Tab5 `M5.Power` or Waveshare pin maps inside this component.

## ESP32-P4 High-Speed

Primary measured host class. Blog V4 bulk MPS is 512 B (HS). Continuous
multi-hundred-kS/s to multi-MS/s IQ is practical only with HS host.

## Full-Speed hosts (S2/S3)

Not claimed until a measured profile + soak exists. Bulk MPS and CPU budget
differ; do not assume P4 tables “just work.”

## Integrating as a component

### ESP-IDF (this repo as component root)

```cmake
# In your project CMakeLists.txt before project()
set(EXTRA_COMPONENT_DIRS "F:/Ai/ESP_RTL_SDR")
```

Or clone beside your project and point `EXTRA_COMPONENT_DIRS` at it.

### Example

```text
F:\Ai\ESP_RTL_SDR\examples\p4_serial_smoke
```

Set `IDF_PATH`, then:

```bash
idf.py set-target esp32p4
idf.py build
```

### PlatformIO

```ini
lib_extra_dirs = F:/Ai/ESP_RTL_SDR
lib_deps = esp_rtl_sdr
```

(Adjust paths; `library.json` names the package `esp_rtl_sdr`.)

## Consumer apps (examples of boundary)

| App | Role |
|---|---|
| OrcSDR Tab5 | Full radio UI + speaker; uses driver for IQ |
| OrcSDR Waveshare shell | Web + LCD status; same driver, different BSP |
| Your firmware | Call `esp_rtl_sdr.h` only |

This repository does **not** require OrcSDR to build or document.
