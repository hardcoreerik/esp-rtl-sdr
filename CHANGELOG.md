# Changelog

## Unreleased

### Added

- USB enumeration fault guard: `esp_rtl_sdr_install()` now tracks (via
  RTC-retained state) consecutive boots that panic while a device is
  attaching. After 3 consecutive enumeration-time panics it skips
  `usb_host_install()` entirely for that boot instead of retrying forever,
  returning the new `ESP_RTL_SDR_ERR_USB_SAFE_MODE`. Apps can query
  `esp_rtl_sdr_usb_safe_mode_active()` and clear the latch with
  `esp_rtl_sdr_usb_fault_guard_reset()`. This does not fix the underlying
  crash (see below) — it turns an infinite reboot loop into "USB disabled
  this session" on an incompatible stick.

### Root cause investigation — enumeration-time EP0 stall panic

- Root-caused a hard reboot loop (previously reported as "One Blog V3 tester
  reported a reboot loop on insertion with RC1" in the 0.8.0-rc2 entry below)
  against a physical RTL-SDR Blog "V3c" unit. Confirmed via live serial
  monitor + ESP-IDF source reading, not guesswork:
  - The unit's USB descriptors report **`RTL2838UHIDIR`** (the bare factory
    RTL2838 string) rather than the `RTLSDRBlog`/`Blog V3` EEPROM strings
    this driver's `rtl_profile_from_descriptors()` looks for.
  - The panic happens **before** `client_event_cb` ever receives
    `USB_HOST_CLIENT_EVENT_NEW_DEV` (no `usb new_device addr=...` log line
    precedes it) — i.e. entirely inside stock ESP-IDF 5.5.4's own
    enumeration (`components/usb/enum.c`), not in this component's probe
    code (`probe_blog_v3_tuner` / `identify_profile` never run).
  - Sequence, reproduced deterministically on every boot with the stick
    attached: `BOOT_STAGE rtl_enumerating` → `E (…) USBH: Dev 1 EP 0 STALL`
    → `assert failed: usbh_dev_close usbh.c:1058
    (dev_obj->dynamic.num_ctrl_xfers_inflight == 0)` → coredump → reboot.
  - The crash's own backtrace past frame 2 (`ep_wrapper_alloc` /
    `interface_claim` / `usb_host_interface_claim`) is not trustworthy:
    those functions (read from the installed ESP-IDF 5.5.4 source) never
    call `usbh_dev_close`, and register `T0..T6`/`A6..A7` in the dump
    decode as ASCII fragments of the assert string itself, not real
    pointers — the unwinder walked stale/corrupted stack past that point.
  - This is a stock ESP-IDF `usb_host`/`enum.c` robustness gap triggered by
    this stick's EP0 behavior during enumeration; it cannot be fixed from
    this component's application-level code, hence the fault guard above
    as the available mitigation.

### Fixed

- Queues hotplug addresses for ordered processing by the USB client task in both USB-host ownership modes.
- Clears passport state on detach so a replacement dongle cannot inherit rate evidence.
- Keeps the Blog V4 initialization trace unchanged while excluding known V4-only board
  control values (`0x3001`, `0x3003`, `0x3004`) from provisional R820T2/R860 profiles.
- Adds profile-aware initialization, hotplug, and disconnect diagnostics.

### Known hardware status

- Blog V4 remains the non-regression baseline; RC2 hardware acceptance is pending.
- One Blog V3 tester reported a reboot loop on insertion with RC1.
- One Nooelec V5 tester reported static/intermittent waterfall but no received stations.
- V3 and Nooelec reception remain provisional and unverified by the maintainer.

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

## 0.7.15 (2026-09-09)

### Fixed — Blog V4 HF hardware routing (hardware acceptance pending)

- Completes capture-derived Cable-2 (`R828D 0x06=0x38`) and RTL2832 GPIO5-low selection for HF; restores GPIO5-high and VHF/UHF input masks outside HF.
- Composes GPIO0 Bias-T state with GPIO5 routing and keeps `GPOE=0x39`, so Bias-T changes cannot undo the selected RF path.
- Composes manual/AUTO register-05 low bits with the HF/VHF/UHF input masks, preserving gain state across retunes and routing across gain changes.
- Uses the same complete route application for startup, hot retune, gain/AUTO, and Bias-T, and marks the applied route valid only after every required write succeeds.
- Selects the HF Cable-2 route at exactly 28.8 MHz while applying the 28.8 MHz LO offset only below that frequency.
- Host tests and ESP-IDF compilation do not constitute physical reception acceptance; GPIO, raw-IQ, AM, Shortwave, VHF, and UHF tests remain open.

## 0.7.14 (2026-09-07)

### Changed

- **ESP-IDF floor raised to 5.5.0** (CI builds `v5.5.4`, same floor as OrcSDR Tab5).

### Fixed

- **stop drains live URBs before free_bulk_pool (Tab5 HCD race):**
  `stop_stream_internal()` (used by `esp_rtl_sdr_stop`, e.g. POCSAG band-switch
  stop→start) previously halt/flush/clear'd, then took a fixed `bulk_num` ×
  timed semaphore, then unconditionally zeroed `live_urbs` and freed the bulk
  transfer pool. That could free a `usb_transfer_t` while IDF DWC HCD still had
  a bulk descriptor in flight → `_buffer_parse_bulk` assert
  (`desc_status != SUCCESS`) on Tab5. Stop now matches `bulk_pause_and_drain`:
  shared `drain_live_urbs()` polls `live_urbs`→0, halt/flush/clear only if still
  live, polls again, and **does not** call `free_bulk_pool` until `live_urbs==0`
  (on drain timeout: skip free, keep `pause_resubmit`, return
  `ESP_RTL_SDR_ERR_TIMEOUT`, state FAULT). `free_bulk_pool` also refuses while
  `live_urbs>0`. Uninstall may clear a stuck counter only after host teardown.
- **reset refuses while live URBs outstanding:** `esp_rtl_sdr_reset()` returns
  `ESP_RTL_SDR_ERR_BUSY` and keeps FAULT when `live_urbs>0` (after timed-out
  stop) so start cannot orphan the old transfer pool while callbacks may still
  fire; caller retries stop until drain, then reset.
- **uninstall does not free pool if host uninstall fails:** check
  `usb_host_uninstall()` return; only clear stuck `live_urbs` / `free_bulk_pool`
  after success; on failure leave pool, clear `destroying`, return ERR_USB.

## 0.7.13 (2026-09-05)

### Added

- **Public windowed metrics/health from metric snapshots:** apps take two
  esp_rtl_sdr_get_metrics() snapshots and call esp_rtl_sdr_metrics_delta() /
  esp_rtl_sdr_health_from_window() for scoped bytes, overruns, consumer drops,
  short transfers, effective SPS, efficiency, and USB health
  (OK / USB_STARVING / APP_TOO_SLOW) for that window only. Pure helpers
  (no new handle mutex state). get_health() remains lifetime/cumulative from
  stream start so early pull-ring overflow is not mistaken for soak drops.
  p4_serial_smoke soak scoring now wraps the public helpers.

## 0.7.12 (2026-08-27)

### Fixed

- **p4_serial_smoke SOAK row attributed pre-soak pull-ring overflow to the 8 s drain:**
  the harness waits 400 ms after `start()` before `first_read`, then starts
  `soak_drain`. At 960 kS/s CU8 those cumulative `consumer_drops` match an
  undrained ring, not the soak window. v0.7.11 Tab5 logs (507904 drops @
  `6x16384`, 593920 @ `3x32768`) were that artifact. Smoke now restarts the
  stream on the same USB host install (metrics and pull ring reset), drains
  immediately, and reports scoped/delta bytes, overruns, drops, and advice
  for the drain window. PASS requires efficiency >= 90%, delta overruns == 0,
  delta consumer drops == 0, scoped advice not `USB_STARVING` or
  `APP_TOO_SLOW`, continuing IQ, and overall hardware PASS. Driver streaming
  engine unchanged. Tag `v0.7.11` is immutable.
- **p4_serial_smoke A/B URB images shared one project-root `sdkconfig`:**
  IDF 5.5.4 `-B` does not move `sdkconfig`. Overlay `set-target` could make
  the later default image boot as `usb_soak_960k_3x32k`. Example now pins
  `SDKCONFIG` under the `-B` directory; docs use `build-6x16k` /
  `build-3x32k` with a pre-flash URB Kconfig grep. Smoke harness/docs only.

## 0.7.11 (2026-08-27)

### Fixed

- **p4_serial_smoke soak_drain stack overflow ([#11](https://github.com/hardcoreerik/esp-rtl-sdr/issues/11)):**
  drain_task allocated a 16 KiB local buffer on a 4096-byte FreeRTOS task
  stack (xTaskCreate sizes are bytes). Buffer is now a single static 16 KiB
  array. Smoke harness only; driver streaming engine unchanged. Tag `v0.7.10`
  is immutable. Re-soak both URB images against this tag.

## 0.7.10 (2026-08-27)

### Smoke / Tab5 USB

- `examples/p4_serial_smoke` quiet USB soak at 960 kS/s (BOTH + auto ring,
  helper `read()` drain, no mid-stream EP0). PASS if efficiency >= 90%.
  **One USB host install per boot.** A/B URB layouts are two images:
  default `6x16384` (`usb_soak_960k_6x16k`) and overlay `3x32768`
  (`usb_soak_960k_3x32k`, `sdkconfig.defaults.urb_3x32k`). Do not re-install
  the host in one run (IDF 5.5.4 `usb_host_uninstall` is not a reliable
  re-entry on Tab5).
- `sdkconfig.defaults` sets `CONFIG_ESP32P4_SELECTS_REV_LESS_V3=y` plus
  `CONFIG_ESP32P4_REV_MIN_0=y` (example-only). IDF 5.5.4 hides rev 0 behind
  that parent symbol; without it Tab5 P4 v1.3 gets a v3.1 bootloader and
  esptool refuses. P4 rev <3.0 vs >=3.0 images are mutually exclusive.
- Example `PROJECT_VER` is `0.7.10` so the image identity is not git-describe
  of tag v0.7.9.
- The 0.7.9 Tab5 79% USB_STARVING figure mixed L4 gain/AGC bulk-pauses with a
  sparse BOTH `read()`. Those `consumer_drops` are not the starve signal.
  Hardware re-soak of both URB images is Tab5-operator work against this tag.

## 0.7.9 (2026-08-26)

### Fixed

- **Auto pull-ring `ESP_ERR_NO_MEM` on no-PSRAM Tab5:** default `pull_ring_bytes=0`
  still prefers ~4× URB (min ~192 KiB). If PSRAM/internal cannot allocate that,
  auto size now shrinks to the largest even internal block down to 64 KiB.
  Explicit `pull_ring_bytes` stays fail-closed. Tab5 L4 run 1 in
  `docs/Test_reports/ESP_RTL_SDR_0_7_8_DROP_IN_TEST_REPORT_2026-08-26.md`.
- **False `ERR_REENTRANT`:** reentrancy is the **callback task**, not a handle-wide
  depth flag. App-task setters no longer fail while the delivery task is emitting
  `EVT_IQ_BLOCK`. (Landed on 0.7.8 `master` untagged; first immutable tag is 0.7.9.)

### Smoke

- `examples/p4_serial_smoke` keeps real default auto ring (no 64 KiB override),
  suppresses IQ event logging, waits 3 s for enumeration, and emits L4 AUTO/RTL
  AGC rows. `sdkconfig.defaults` selects pre-v3 P4 silicon for Tab5 rev v1.3.
  Hardware re-run on COM17 is still required.

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
