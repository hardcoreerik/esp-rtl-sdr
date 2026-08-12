# Roadmap — esp_rtl_sdr

Close the gap with desktop librtlsdr-class drivers **without** porting them.
Evidence labels match `Project_truth.md`.

---

## North star

A stand-alone ESP-IDF component that:

1. Streams reliable CU8 IQ from RTL2832U-class dongles on ESP32-P4 HS USB.
2. Exposes a clear, discoverable API (freq, rate, gain, ppm, bias as capabilities).
3. Supports **multiple measured profiles** (Blog V4 first, then common R820T2-class).
4. Earns trust via docs, soak logs, and fail-closed behavior — not silent over-claim.

---

## Phase 0 — Stand-alone repo + rename (**complete**)

| Item | Status |
|---|---|
| Repo at `F:\Ai\ESP_RTL_SDR\` | **Done** |
| GitHub `hardcoreerik/esp-rtl-sdr` | **Done** |
| Rename API to `esp_rtl_sdr` | **Done** |
| Blog V4 profile tables preserved | **Done** |
| Docs suite | **Done** |
| Example smoke project | **Done** |

**Exit:** Met.

---

## Phase 1 — API shape parity (same Blog V4 RF) (**complete in 0.5.0**)

- [x] `set_center_freq` / `get_center_freq` (idle preferred LO; streaming → retune)
- [x] `set_sample_rate` / `get_sample_rate` (allowlist; streaming → BUSY)
- [x] Blocking `read` from pull ring (sync-read equivalent)
- [x] `start_hz(frequency_hz, sample_rate_sps)` convenience
- [x] Capability matrix updated
- [x] Clearer device error alias `ERR_UNSUPPORTED_DEVICE` (Phase 2 polish)
- [ ] Hardware re-run of *this* tree on Tab5/Waveshare — still open

**Exit:** API shape met; hardware re-soak tracked in Project_truth.

---

## Phase 2 — Rates, ppm, multi-device (**complete in 0.6.0**)

- [x] Expand allowlist with documented evidence (`docs/RATES.md`)
- [x] `set_freq_correction` / `get_freq_correction` (software LO offset)
- [x] Enumerate / open by index or serial (`refresh_device_list`, `select_*`)
- [ ] Per-rate P4 continuous soak log for non-provenance rates (→ Phase 5)
- [ ] Hardware re-soak of this tree for ppm + multi-device paths

**Exit:** Documented rate matrix; ppm usable for known crystal offset; multi-device API live.

---

## Phase 3 — Gain and bias-T (measured)

- [ ] Independent USB capture of gain-mode / gain-step traffic on physical Blog V4
- [ ] `set_tuner_gain_mode`, `set_tuner_gain`, `get_tuner_gains`
- [ ] Enable `CAP_GAIN` only when implemented
- [ ] Bias-T control when measured; enable `CAP_BIAS_TEE`
- [ ] Never paste librtlsdr gain tables

**Exit:** Gain and bias APIs work on Blog V4 with recorded procedure.

---

## Phase 4 — Second profile (broader RTL2832U)

- [ ] Capture init/tune for a common **R820T2** dongle (clean-room)
- [ ] `profile_r820t2` (name TBD) beside `transfers_blog_v4`
- [ ] VID/PID allowlist with fail-closed unknown
- [ ] Probe strategy documented in `docs/PROFILES.md`
- [ ] Direct sampling / HF only if separate evidence exists

**Exit:** At least two hardware-verified profiles; marketing still honest.

---

## Phase 5 — Reliability (peer “robust” bar)

- [ ] Unplug/replug recover to READY without reboot
- [ ] Five-minute soak at 960k and 2.048M with drop/SPS report checked in or linked
- [ ] Optional soak logs for formula rates (250k–3200k subset)
- [ ] Optional IQ acquire mode (`CAP_IQ_ACQUIRE`) if apps need zero-copy hold
- [ ] CI: build component + example on IDF 5.3+ (when available)

**Exit:** Reliability story competitive with vertical ESP RTL apps that used librtlsdr ports.

---

## Out of roadmap (unless evidence forces)

| Item | Why |
|---|---|
| Full SoapyRTLSDR / GQRX control surface | App territory |
| Wi-Fi rtl_tcp inside this component | Separate example/app |
| Classic ESP32 (no HS host) | Out of scope |
| Silent “try V4 tables on everything” | Violates fail-closed + clean-room honesty |

---

## Tracking

| Doc | Role |
|---|---|
| `Project_truth.md` | What is true **now** |
| `Roadmap.md` | How we get to parity |
| `docs/CAPABILITY_MATRIX.md` | Row-by-row desktop comparison |
| `docs/RATES.md` | Sample-rate allowlist + evidence |
| `CHANGELOG.md` | Released deltas |
