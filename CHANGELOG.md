# Changelog

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
