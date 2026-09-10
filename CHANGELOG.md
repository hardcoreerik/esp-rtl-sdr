# Changelog

## Unreleased

## 0.8.0-rc1 (2026-09-10) — EXPERIMENTAL multi-dongle

### Added

- **Unified hardware profiles** (`Unknown`, `BlogV4`, `BlogV3`, `NooelecSmartV5`):
  driver-owned plug-and-play identity; apps consume
  `esp_rtl_sdr_get_profile()` / `esp_rtl_sdr_get_device_capabilities()`.
- **Provisional Nooelec NESDR SMArt v5** path: exact `Nooelec` + product contains
  `NESDR SMArt v5`; R820T2/R860 tuner I2C `0x34` remapping; RF < 24 MHz rejected;
  no Blog V4 HF Cable-2/GPIO5 routing; gain/bias CAP bits off (fail-closed).
- **Blog V3 identity/probe only**: exact V3 descriptors or completed R820T2
  chip-id (`0x96`/`0x69`); `CAP_STREAM` false; `start` -> `ERR_UNSUPPORTED`.
- Behavioral host profile tests (detection, unknown reject, tuner isolation,
  frequency policy, capability/transition matrix). Replaces PR #19 source-text
  scaffold checks.

### Changed

- Bare / unknown `0bda:2838` is **never** treated as Blog V4.
- Hotplug detach clears profile, device caps, and V4 front-end shadows (no
  cross-profile leakage).
- Experimental prerelease version **0.8.0-rc1** (does not silently replace
  stable 0.7.x).

### Preserved

- Blog V4 HF/VHF/UHF route composition from PR #18 / 0.7.15 remains the primary
  regression gate (Cable-2/GPIO5/Bias-T/gain). Physical V4 soak still open.

## 0.7.15 (2026-09-09)

### Fixed — Blog V4 HF hardware routing (hardware acceptance pending)

- Completes capture-derived Cable-2 (`R828D 0x06=0x38`) and RTL2832 GPIO5-low selection for HF; restores GPIO5-high and VHF/UHF input masks outside HF.
- Composes GPIO0 Bias-T state with GPIO5 routing and keeps `GPOE=0x39`, so Bias-T changes cannot undo the selected RF path.
- Composes manual/AUTO register-05 low bits with the HF/VHF/UHF input masks, preserving gain state across retunes and routing across gain changes.
- Uses the same complete route application for startup, hot retune, gain/AUTO, and Bias-T, and marks the applied route valid only after every required write succeeds.
- Selects the HF Cable-2 route at exactly 28.8 MHz while applying the 28.8 MHz LO offset only below that frequency.
- Host tests and ESP-IDF compilation do not constitute physical reception acceptance; GPIO, raw-IQ, AM, Shortwave, VHF, and UHF tests remain open.
