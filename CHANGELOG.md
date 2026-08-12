# Changelog

## Unreleased

### CI

- **ESP-IDF P4 compile gate:** build `examples/p4_serial_smoke` for `esp32p4` on IDF v5.3.2 and v5.4.1 (compile only)
- Smoke example uses stable component name `esp_rtl_sdr` via `components/esp_rtl_sdr/` wrapper (fixes `rtl_sdr_v4_esp` REQUIRES drift)

## 0.7.2 (2026-08-12)

### Hardening (review-driven)

- **STARTING** state serializes concurrent `start()`
- Deterministic **task join** on uninstall (notify, not fixed 50 ms delay)
- User callbacks use **atomic** `in_callback_depth`; `select_device*` emits **after** unlock
- **Transactional** IQ ring allocation / destroy on failure
- **struct_size** accepts min..sizeof (append-only ABI)
- **retune** from callback → `ERR_REENTRANT` (strict; true async later)
- **Kconfig** transfer size/count wired into `config_default` when built under IDF
- Pull ring uses **block memcpy** instead of per-byte loop
- `idf_component.yml` **targets: esp32p4**
- Docs: `docs/HARDENING_0_7_2.md`

### Fixed (carry-forward)

- Low-band min **225001 Hz** (not 225000) for ratio/desktop parity

### Version

- **0.7.2** (do not retag 0.7.1)

## Unreleased

### Tests / CI

- Expanded host policy suite (window edges, quantize idempotence, config matrix, names, CAP off)
- CI: Ubuntu + Windows matrix, `-Werror` on Linux, `ctest`, concurrency cancel
- `tests/scripts/check_truth_hygiene.sh` (version + required docs + CAP_GAIN/BIAS guard)

### Docs

- `docs/LAB_HOBBYIST.md` — honest hobbyist lab capabilities + TinySA Ultra how-to
- TESTING / GAIN_BIAS / PROJECT_TRUTH cross-links for desk-lab posture

## 0.7.1 (2026-08-12)

### Added

- Live **`EVT_HEALTH`** from delivery task (on overall change + every 48 IQ blocks)
- Phase 3 **API surface** (fail-closed): `set/get_tuner_gain_mode`, `set/get_tuner_gain`,
  `get_tuner_gains`, `set/get_bias_tee` — return `ERR_UNSUPPORTED`; **CAP_GAIN / CAP_BIAS_TEE remain off**
- `docs/GAIN_BIAS_CAPTURE.md` — clean-room capture procedure for lab (Baofeng/Flipper stimulus later)

### Notes

- Gain/bias preferences may be stored for apps; **no hardware effect** until measured EP0 lands.
- Version **0.7.1**.

## Unreleased

### Added (automated testing / TheOrc-aligned)

- Host unit suite: `tests/host` + `src/esp_rtl_sdr_policy.cpp` (no IDF/USB)
- Scripts: `tests/scripts/run_host_tests.ps1` / `.sh`
- CI: `.github/workflows/ci.yml` (policy tests + version/truth hygiene)
- `docs/TESTING_GUIDE.md` — layers, how to run, what is/isn't claimed

### Added (open-source honesty / TheOrc-aligned)

- `docs/AI_DEVELOPMENT_DISCLOSURE.md` — human-directed, AI-assisted; trust rituals
- `docs/DOCUMENTATION_STANDARD.md` — accuracy-first docs (no planned-as-done)
- `SECURITY.md` — vulnerability reporting
- `CONTRIBUTING.md` — clean-room + truth PR checklist
- `docs/README.md` — docs index
- `PROJECT_TRUTH.md` rename (was `Project_truth.md`) + development honesty table
- README credits + “where we are” honesty block

## 0.7.0 (2026-08-12)

### Added

- **Continuous sample rates** within hardware windows  
  (225–300 kHz ∪ 900 kHz–3.2 MHz) with `quantize_sample_rate` → exact SPS.
- **Intent:** `esp_rtl_sdr_apply_need()` — FM / ADS-B / WX / HF / MAX_STABLE / LISTEN.
- **Health:** `esp_rtl_sdr_get_health()` — USB/RF categories + advice string.
- **Passport:** `probe_rates` / `get_rate_passport` / opts default; progress events.
- Caps: `CONTINUOUS_RATE`, `NEED`, `HEALTH`, `PASSPORT`.
- Events: `EVT_HEALTH`, `EVT_PASSPORT_PROGRESS`, `EVT_PASSPORT_DONE`.
- Docs: `docs/VISION.md`, `docs/SILICON.md`, `docs/TESTING.md` (Heltec V4×2,
  Baofeng UV-5R, Flipper Zero lab notes); RATES rewritten for continuous policy.

### Changed

- `is_rate_supported` = in-window + quantizable (not fixed allowlist only).
- `get_supported_rates` returns **recommended** named rates (includes 2.56M).
- `set_sample_rate` / `start` store **exact** quantized SPS.
- Version → **0.7.0**.

### Notes

- `NEED_HF` stores preferred LO only; upconverter CAP still open.
- Passport requires attached Blog V4; NO_DEVICE without dongle.
- Gain / bias / adaptive URB remain later phases.

## 0.6.0 (2026-08-12)

### Added

- Phase 2: expanded recommended rates, ppm correction, multi-device select.
- `docs/RATES.md`, capability/docs updates.

## 0.5.0 (2026-08-11)

### Added

- Stand-alone **esp_rtl_sdr** rename, Phase 1 desktop-shaped API, smoke example,
  Project_truth / architecture / Roadmap, GitHub hardcoreerik/esp-rtl-sdr.
