# Changelog

## Unreleased

## 0.7.8 (2026-08-26)

### Added — measured Tuner AGC AUTO + RTL digital AGC

- **`CAP_GAIN_AUTO`**: `set_tuner_gain_mode(AUTO)` writes measured R828D IR trio
  `05=E8 07=78 0C=6B` (lab 2026-08-26). MANUAL restores the last ladder step.
- **`CAP_RTL_AGC`**: additive `set/get_rtl_agc` — demod `0x19` ON=`0x25` OFF=`0x05`.
  Not tuner AUTO. Apps that never call it are unchanged.
- Same async bulk-pause sideband queue as mid-stream gain/bias (0.7.6).
- Fail-closed: unclaimed → `ERR_NOT_CLAIMED`; callback → `ERR_REENTRANT`;
  same mode twice is a no-op. `set_tuner_gain` still forces MANUAL.
- After AUTO, skip triplexer filter rewrite (would clobber `0x0c=0x6B`).
- **IF / SDR# Bandwidth:** capture was USB-silent after open. `CAP_IF_FILTER` not added.

### Evidence

- `agc_tuner_on_off.pcapng` SHA-256 `E131C5C6…3E8E` (4 ON/OFF clusters)
- `agc_rtl_on_off.pcapng` SHA-256 `1E5B0061…D482` (4 demod 0x19 writes)
- `if_filters_steps.pcapng` SHA-256 `D9D32608…A9FB` (software-only Bandwidth)
- Procedure: `docs/AGC_IF_CAPTURE.md`

## 0.7.7 (2026-08-13)

### Added — Blog V4 HF upconverter CAP

- **`CAP_HF_UPCONVERTER`**: full advertised span **500 kHz … 1766 MHz**
- User RF **&lt; 28.8 MHz** programs R828D at **RF + 28.8 MHz** (public V4 SA612 path)
- Triplexer band FE after every tune/retune: HF / VHF / UHF (UHF keeps prior measured block)
- Helpers: `esp_rtl_sdr_frequency_uses_hf_upconverter()`, `esp_rtl_sdr_tuner_frequency_hz()`
- `NEED_HF` default LO = WWV **10.000 MHz** (was LO-only placeholder)
- Post-gain band filter refresh (does not clobber reg05 gain ladder)

### Evidence

- Offset + band edges: **public** RTL-SDR Blog V4 product page / datasheet (not GPL source)
- IR EP0 envelope: measured Blog V4 profile (`0x0074`/`0x0610`); HF reg05 family from init captures

## 0.7.6 (2026-08-13)

### Fixed

- Mid-stream gain/bias: **async queue** on delivery task (one bulk-pause window) so
  app/HTTP loop no longer blocks 1–3 s during EP0.
- Bias then gain in the **same** paused window when both pending; 40 ms settle after SYS bias.
- Gain IR writes: up to 3 full retries; control transfers: 3 attempts on STALL.
- Still pauses bulk before EP0 (same class of fix as retune).

## 0.7.5 (2026-08-13)

### Added — Phase 3 measured gain / bias (Blog V4)

- Clean-room tables from lab USBPcap (`private/measured_gain_bias_v4.hpp`)
  - Bias ON/OFF SYS sequence (`0x3004/3003/3001/3000` @ `0x0210`)
  - Manual gain ladder 0.0…49.6 dB via IR `0x0074`/`0x0610` pairs `{0x05,val}`, `{0x07,val}`, `{0x0c,0x68}`
- `set_tuner_gain` / `get_tuner_gains` / `set_bias_tee` apply measured EP0 when interface claimed
- **CAP_GAIN** and **CAP_BIAS_TEE** enabled (`MEASURED_2026_08_12`)
- AUTO gain mode still `ERR_UNSUPPORTED` (AGC path not in this capture set)
- Evidence report: `docs/PHASE3_CAPTURE_REPORT.md`

### Notes

- Multimeter SMA DC not recorded — bias electrical claim remains capture-level
- P4 re-soak of new CAP paths still open (lab)

## 0.7.4 (2026-08-13)

### Added

- **Delivery modes** (`config.delivery_mode`): `BOTH` (default) | `CALLBACK` | `READ`
  - `CALLBACK`: `EVT_IQ_BLOCK` only — no large pull-ring allocation
  - `READ`: blocking `read()` only — no `EVT_IQ_BLOCK`
  - `BOTH`: previous behavior
- **Lazy pull ring:** buffer allocated on first IQ push or first `read()` when mode uses read
- Optional `config.pull_ring_bytes` (0 = auto; even, 1 KiB…1 MiB)
- `CAP_DELIVERY_MODE` + pure helpers `delivery_mode_uses_callback_iq` / `_uses_read`
- `read()` returns `ERR_UNSUPPORTED` in CALLBACK-only mode

### Fixed (CodeRabbit on PR #6)

- Serialize `ensure_pull_ring` on the handle lock; fail-closed teardown of partial rings
  (no double-alloc race between delivery task and `read()`)

### Docs / process (from 0.7.3 review gap work)

- Full API reference, Kconfig/troubleshooting/examples, soak template, SCOPE, licensing
- Legacy `struct_size` fallback for delivery fields documented; versioning table through 0.7.4

## 0.7.3 (2026-08-12)

### Changed

- **True async retune from event callback:** `retune_hz` / streaming `set_center_freq`
  queue the LO and return `ESP_OK`; delivery task drains URBs, EP0 tunes, emits
  `EVT_RETUNED`. App-task calls still apply synchronously.
- Coalescing: newer pending LO while apply is in flight is not lost.

### Docs

- `docs/DEVELOPMENT_NARRATIVE_0_7.md` — verbose 0.7.x development commentary
  (architecture, CI meaning, hardening, async retune, lab honesty, open list)

### CI (carry-forward)

- ESP-IDF P4 compile gate for smoke example (5.3.2 + 5.4.1)

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
