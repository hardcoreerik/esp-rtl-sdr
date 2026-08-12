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
- [ ] True async retune from callback
- [ ] Delivery CALLBACK/READ/BOTH + lazy pull ring
- [ ] Lab soak evidence from this tree

See `docs/HARDENING_0_7_2.md`.

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
- [ ] Independent USB capture of gain / bias on Blog V4
- [ ] Wire EP0 from capture into profile; `set_*` applies hardware
- [ ] Enable `CAP_GAIN` / `CAP_BIAS_TEE` only when implemented
- [ ] Never paste librtlsdr gain tables
- [ ] Lab: Baofeng / Flipper for clip vs weak health correlation

**Exit:** Gain and bias work on Blog V4 with recorded procedure.

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
