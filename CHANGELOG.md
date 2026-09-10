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
  Contributor-tested; maintainer soak pending; community hardware soak welcome.
- **Provisional Blog V3 stream**: exact V3 descriptors or completed R820T2
  chip-id (`0x96`/`0x69`); `CAP_STREAM` true; shares evidence-backed R820T2
  I2C `0x34` IR remap with Nooelec (same USB template remapping only);
  RF < 24 MHz rejected; no V4 HF Cable-2/GPIO5; gain/bias CAP off.
  Experimental/community soak — **not** Hardware-verified / maintainer-unverified.
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
