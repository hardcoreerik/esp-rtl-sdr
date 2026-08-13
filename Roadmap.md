# Roadmap — esp_rtl_sdr

Evidence labels match `PROJECT_TRUTH.md`. Vision: `docs/VISION.md`.

---

## North star

A stand-alone ESP-IDF component that:

1. Streams reliable CU8 IQ from RTL2832U-class dongles on ESP32-P4 HS USB.
2. Exposes discoverable API (freq, rate, gain, ppm, bias as capabilities).
3. Acts as a **dongle nervous system**: intent, health, on-host passport.
4. Supports multiple measured profiles (Blog V4 first).
5. Earns trust via docs, soak logs, fail-closed behavior.

---

## Phase 0 — Stand-alone repo (**complete**)

Done: repo, GitHub, rename, Blog V4 tables, docs suite, smoke example.

---

## Phase 1 — API shape parity (**complete in 0.5.0**)

Done: set/get center freq & rate, `read`, `start_hz`, caps matrix.

---

## Phase 2 — Rates, ppm, multi-device (**complete in 0.6.0**)

Done: expanded recommended rates, ppm, multi-device select, RATES.md.

Open: formal P4 soak logs for non-provenance rates.

---

## Phase 2.1 — Continuous rates + nervous system spine (**complete in 0.7.0**)

- [x] Any rate within hardware windows + quantize + exact SPS
- [x] `apply_need()` intent presets
- [x] `get_health()` USB/RF narrative
- [x] `probe_rates()` passport skeleton
- [x] Docs: VISION, SILICON, TESTING (Heltec / Baofeng / Flipper)
- [ ] Hardware run of passport on P4 + Blog V4 (lab)
- [x] Emit `EVT_HEALTH` from delivery (change + periodic)

---

## Phase 2.3 — Runtime hardening 0.7.2 (**complete in tree**)

Review-driven (pause Phase 3 hardware for one release):

- [x] STARTING state / concurrent start serialization
- [x] Deterministic worker join on uninstall
- [x] Callback emit without API-lock reentrancy trap on select_device
- [x] ensure_ring transactional rollback
- [x] struct_size min..sizeof compatibility
- [x] Strict retune reentrancy (ERR_REENTRANT from callback)
- [x] Kconfig defaults wired
- [x] Pull-ring memcpy optimization
- [x] Component `targets: esp32p4`
- [x] ESP-IDF P4 compile CI (`idf-p4-build` on 5.3.2 + 5.4.1)
- [x] True async retune from callback (queue + delivery apply + EVT_RETUNED)
- [x] Delivery CALLBACK/READ/BOTH + lazy pull ring (**0.7.4**)
- [ ] Lab soak evidence from this tree (procedure ready: `docs/SOAK.md`)

See `docs/HARDENING_0_7_2.md`.

---

## Phase 2.4 — Review gap closure (docs / process) (**complete in tree**)

External review @ v0.7.3. Map: `docs/REVIEW_GAPS_2026-08.md`.

- [x] Full `docs/API_REFERENCE.md` (params, returns, examples)
- [x] Kconfig user doc, troubleshooting, examples recipes
- [x] Intentional scope doc (P4 + Blog V4 first)
- [x] Soak procedure + `docs/lab/SOAK_LOG_TEMPLATE.md`
- [x] Runtime constants documented (ring/cores/health period)
- [x] Commercial licensing process clarified
- [x] GitHub Release + tracking issues
- [ ] Phase 3 gain/bias CAP (lab capture — not closable by docs)
- [ ] Filled soak log from this tree (lab run)

---

## Phase 3 — Gain / bias (next hardware)

- [x] USB capture per `docs/GAIN_BIAS_CAPTURE.md` (lab 2026-08-12)
- [x] Enable `CAP_GAIN` / `CAP_BIAS_TEE` only after measured sequences (0.7.5)
- [ ] Update PROJECT_TRUTH to Hardware-verified after P4 re-soak

---

## Phase 2.2 — Automated testing spine (TheOrc-aligned) (**complete**)

- [x] Extract pure policy to `esp_rtl_sdr_policy.cpp` (host-linkable)
- [x] Host unit suite `tests/host` (rates, quantize, config, version, caps)
- [x] Scripts `tests/scripts/run_host_tests.ps1` / `.sh`
- [x] GitHub Actions CI (host tests + truth/version hygiene)
- [x] `docs/TESTING_GUIDE.md`
- [ ] Optional: IDF target unit tests on hardware runner
- [ ] Optional: passport log artifact upload from lab script

---

## Phase 3 — Gain and bias-T (measured)

- [x] Public fail-closed API surface + capture procedure (`docs/GAIN_BIAS_CAPTURE.md`)
- [x] Independent USB capture of gain / bias on Blog V4 (PC USBPcap)
- [x] Wire EP0 from capture into profile; `set_*` applies hardware (0.7.5)
- [x] Enable `CAP_GAIN` / `CAP_BIAS_TEE` only when measured tables wired
- [x] Never paste librtlsdr gain tables (clean-room `measured_gain_bias_v4.hpp`)
- [ ] P4 re-soak: `set_tuner_gain` / `set_bias_tee` on ESP host path
- [ ] Optional multimeter SMA DC for bias-T
- [ ] AUTO AGC capture (still unsupported)
- [ ] Lab: Baofeng / Flipper for clip vs weak health correlation

**Exit:** Gain and bias work on Blog V4 with recorded procedure (+ P4 soak).

---

## Phase 4 — Second profile + V4 RF path depth

- [ ] R820T2 profile (clean-room) if needed for non-V4 sticks
- [ ] Blog V4: HF upconverter CAP, R828D input AUTO, notches
- [ ] `NEED_HF` becomes fully functional (not LO-only)
- [ ] Direct sampling only if separate evidence (V4 uses upconverter)

---

## Phase 5 — Reliability + host superpowers

- [ ] Unplug/replug recover without reboot
- [ ] Five-minute soak artifacts (960k / 2.048M)
- [ ] Adaptive URB size/count from passport
- [ ] Auto rate downshift when health = USB_STARVING
- [ ] Beacon ppm learn (ADS-B / NOAA)
- [ ] CI build on IDF 5.3+ when available

---

## Out of roadmap

| Item | Why |
|---|---|
| Full Soapy / GQRX surface | App territory |
| rtl_tcp inside component | Separate example |
| Classic ESP32 (no HS) | Out of scope |
| Silent V4 tables on unknown sticks | Fail-closed |

---

## Tracking

| Doc | Role |
|---|---|
| `PROJECT_TRUTH.md` | What is true **now** |
| `Roadmap.md` | How we get there |
| `docs/VISION.md` | Nervous-system product model |
| `docs/SILICON.md` | DS / cousins / authority |
| `docs/TESTING.md` | Lab gear |
| `docs/RATES.md` | Rate windows + passport |
| `docs/CAPABILITY_MATRIX.md` | vs desktop |
| `CHANGELOG.md` | Releases |
