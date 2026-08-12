# Changelog

## 0.6.0 (2026-08-12)

### Added

- **Phase 2 — rates, ppm, multi-device**
  - Expanded sample-rate allowlist: 250k, 256k, 960k, 1024k, 1800k, 2048k, 2400k, 3200k
  - `set_freq_correction` / `get_freq_correction` (software LO offset, ±200 ppm)
  - Multi-device: `refresh_device_list`, `get_device_count`, `get_device_at`,
    `select_device`, `select_device_serial`
  - Capability bits: `CAP_FREQ_CORRECTION`, `CAP_MULTI_DEVICE`, `CAP_SYNC_READ`
  - Error: `ESP_RTL_SDR_ERR_BAD_DEVICE` (+ preferred alias `ERR_UNSUPPORTED_DEVICE` for NOT_V4)
- Docs: `docs/RATES.md` (allowlist + evidence labels)

### Changed

- Tune path applies ppm correction at LO programming time; user-facing
  `get_center_freq` remains the requested frequency
- Device open path scans candidates and honors preferred index/serial
- Version macros / component metadata → **0.6.0**

### Notes

- Additional rates beyond 960k/2048k are **formula-programmed**; P4 continuous
  soak is only provenance-claimed for 960k and 2048k (see `docs/RATES.md`)
- Gain / bias-T / R820T2 profiles remain later phases

## 0.5.0 (2026-08-11)

### Added

- Stand-alone repository **esp_rtl_sdr** (renamed from OrcSDR `rtl_sdr_v4_esp`).
- Public API `esp_rtl_sdr.h` / `esp_rtl_sdr_*` (Blog V4 profile = measured 0.4.1 tables).
- Phase 1 desktop-shaped API:
  - `set_center_freq` / `get_center_freq`
  - `set_sample_rate` / `get_sample_rate` (allowlist; mid-stream rate change returns BUSY)
  - `read` — blocking CU8 IQ pull (sync-read equivalent)
  - `start_hz` — convenience start from frequency + rate
- Pull ring fed by delivery task (works with or without event callback).
- Docs: `Project_truth.md`, `architecture.md`, `Roadmap.md`, full `docs/` suite.
- Example `examples/p4_serial_smoke`.
- GitHub: https://github.com/hardcoreerik/esp-rtl-sdr

### Notes

- Not a librtlsdr port. Gain / ppm / bias-T / extra rates / multi-tuner = later phases.
- Hardware re-soak from this tree still open in `Project_truth.md`.
