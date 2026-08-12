# Changelog

## 0.5.0-dev (2026-08-11)

### Added

- Stand-alone repository layout for **esp_rtl_sdr** (renamed from the
  historical OrcSDR component name `rtl_sdr_v4_esp`).
- Public API header `esp_rtl_sdr.h` / symbols `esp_rtl_sdr_*` (semantic
  version **0.5.0** for the rename; behavior of the Blog V4 profile matches
  the measured 0.4.1 transfer tables).
- Blog V4 profile tables in `private/transfers_blog_v4.hpp`.
- Documentation suite: `Project_truth.md`, `architecture.md`, `Roadmap.md`,
  `docs/API.md`, `docs/CAPABILITY_MATRIX.md`, `docs/CLEAN_ROOM.md`,
  `docs/PROFILES.md`, `docs/PORTING.md`.
- Example `examples/p4_serial_smoke`.

### Notes

- Not a librtlsdr port. Multi-tuner / gain / bias-T / ppm are roadmap items.
- Hardware re-verification from *this* tree is tracked in `Project_truth.md`
  (provenance of tables: Tab5 + Waveshare P4 measurements under OrcSDR).
